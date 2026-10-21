#include "decision_onnx.h"
#include "prismalux_paths.h"

#include <QByteArray>
#include <QFile>
#include <QFileInfo>
#include <QHash>
#include <QRegularExpression>
#include <QSet>
#include <QStringList>
#include <QtGlobal>
#include <algorithm>
#include <array>
#include <cmath>
#include <string>
#include <vector>

#ifdef HAVE_ONNX
#  if __has_include(<onnxruntime/core/session/onnxruntime_cxx_api.h>)
#    include <onnxruntime/core/session/onnxruntime_cxx_api.h>
#  elif __has_include(<onnxruntime/onnxruntime_cxx_api.h>)
#    include <onnxruntime/onnxruntime_cxx_api.h>
#  else
#    include <onnxruntime_cxx_api.h>
#  endif
#endif

namespace P = PrismaluxPaths;

/* ══════════════════════════════════════════════════════════════
   Feature hashing — DEVE combaciare con gen_decision_onnx.py

   Normalizzazione: minuscolo, ogni carattere non alfanumerico
   diventa spazio, spazi collassati.
   Token: parola intera, 4-grammi di caratteri, bigrammi di parole.
   Hash: FNV-1a 32 bit su UTF-8. Il classificatore usa mod 512; il
   verificatore usa mod 448 + offset 64 (bucket 0..63 riservati ai
   segnali strutturati).
   ══════════════════════════════════════════════════════════════ */
namespace {

constexpr int kDim = DecisionOnnx::kFeatureDim;

QString normalizeText(const QString& text)
{
    QString out;
    out.reserve(text.size());
    bool prevSpace = true;
    for (const QChar& c : text) {
        if (c.isLetterOrNumber()) {
            out.append(c.toLower());
            prevSpace = false;
        } else if (!prevSpace) {
            out.append(QLatin1Char(' '));
            prevSpace = true;
        }
    }
    while (out.endsWith(QLatin1Char(' ')))
        out.chop(1);
    return out;
}

quint32 fnv1a(const QByteArray& data)
{
    quint32 h = 2166136261u;
    for (unsigned char c : data) {
        h ^= quint32(c);
        h *= 16777619u;
    }
    return h;
}

void addFeature(QVector<float>& vec, const QString& key, float weight = 1.0f)
{
    const quint32 h = fnv1a(key.toUtf8());
    vec[int(h % quint32(kDim))] += weight;
}

void addText(QVector<float>& vec, const QString& text, const QString& prefix = {})
{
    const QStringList words = normalizeText(text).split(QLatin1Char(' '),
                                                       Qt::SkipEmptyParts);
    for (int i = 0; i < words.size(); ++i) {
        const QString& w = words.at(i);
        addFeature(vec, prefix + QStringLiteral("w:") + w);
        for (int j = 0; j + 4 <= w.size(); ++j)
            addFeature(vec, prefix + QStringLiteral("c:") + w.mid(j, 4), 0.5f);
        if (i + 1 < words.size())
            addFeature(vec, prefix + QStringLiteral("b:") + w
                            + QLatin1Char('_') + words.at(i + 1), 0.5f);
    }
}

void l2Normalize(QVector<float>& vec)
{
    double sum = 0.0;
    for (float v : vec) sum += double(v) * double(v);
    const double norm = std::sqrt(sum);
    if (norm > 1e-9)
        for (float& v : vec) v = float(double(v) / norm);
}

QSet<QString> contentWords(const QString& text)
{
    QSet<QString> set;
    for (const QString& w : normalizeText(text).split(QLatin1Char(' '),
                                                      Qt::SkipEmptyParts))
        if (w.size() > 2)
            set.insert(w);
    return set;
}

const QStringList& refusalCues()
{
    static const QStringList cues = {
        QStringLiteral("non lo so"),
        QStringLiteral("non posso"),
        QStringLiteral("impossibile"),
        QStringLiteral("mi dispiace"),
        QStringLiteral("non ne ho idea"),
        QStringLiteral("i don't know"),
        QStringLiteral("cannot"),
        QStringLiteral("unable"),
        QStringLiteral("not sure"),
        QStringLiteral("no idea"),
        QStringLiteral("non ho informazioni"),
        QStringLiteral("non ho abbastanza"),
    };
    return cues;
}

/* ── Verificatore: segnali strutturati ───────────────────────────────
   I primi 64 bucket sono riservati ai segnali (indice fisso); il testo
   usa i bucket 64..511. La stessa mappatura è in gen_decision_onnx.py.
   ─────────────────────────────────────────────────────────────────── */
constexpr int kVerifierTextOffset = 64;
constexpr int kVerifierTextDim = kDim - kVerifierTextOffset;

int verifierSignalIndex(const QString& key)
{
    static const QHash<QString, int> map = {
        { QStringLiteral("x_ov_0"), 0 }, { QStringLiteral("x_ov_1"), 1 },
        { QStringLiteral("x_ov_2"), 2 }, { QStringLiteral("x_ov_3"), 3 },
        { QStringLiteral("x_ov_4"), 4 }, { QStringLiteral("x_ov_5"), 5 },
        { QStringLiteral("x_len_0"), 6 }, { QStringLiteral("x_len_1"), 7 },
        { QStringLiteral("x_len_2"), 8 }, { QStringLiteral("x_len_3"), 9 },
        { QStringLiteral("x_ratio_0"), 10 }, { QStringLiteral("x_ratio_1"), 11 },
        { QStringLiteral("x_ratio_2"), 12 }, { QStringLiteral("x_ratio_3"), 13 },
        { QStringLiteral("x_refusal"), 14 }, { QStringLiteral("x_empty"), 15 },
        { QStringLiteral("x_q_num"), 16 }, { QStringLiteral("x_q_who"), 17 },
        { QStringLiteral("x_q_where"), 18 }, { QStringLiteral("x_q_when"), 19 },
        { QStringLiteral("x_q_how"), 20 }, { QStringLiteral("x_q_why"), 21 },
        { QStringLiteral("x_q_open"), 22 }, { QStringLiteral("x_q_fact"), 23 },
        { QStringLiteral("x_a_num"), 24 }, { QStringLiteral("x_a_proper"), 25 },
        { QStringLiteral("x_a_code"), 26 }, { QStringLiteral("x_a_proc"), 27 },
        { QStringLiteral("x_a_causal"), 28 }, { QStringLiteral("x_a_vague"), 29 },
        { QStringLiteral("x_a_short"), 30 }, { QStringLiteral("x_a_veryshort"), 31 },
        { QStringLiteral("x_num_num"), 32 }, { QStringLiteral("x_num_nonum"), 33 },
        { QStringLiteral("x_who_proper"), 34 }, { QStringLiteral("x_who_noproper"), 35 },
        { QStringLiteral("x_where_proper"), 36 }, { QStringLiteral("x_where_noproper"), 37 },
        { QStringLiteral("x_fact_proper"), 38 }, { QStringLiteral("x_fact_num"), 39 },
        { QStringLiteral("x_fact_noproper"), 40 },
        { QStringLiteral("x_when_num"), 41 }, { QStringLiteral("x_when_nonum"), 42 },
        { QStringLiteral("x_how_proc"), 43 }, { QStringLiteral("x_how_noproc"), 44 },
        { QStringLiteral("x_why_causal"), 45 }, { QStringLiteral("x_why_nocausal"), 46 },
        { QStringLiteral("x_open_long"), 47 }, { QStringLiteral("x_open_short"), 48 },
        { QStringLiteral("x_short_proper"), 49 }, { QStringLiteral("x_short_num"), 50 },
        { QStringLiteral("x_vague_neg"), 51 },
        { QStringLiteral("x_noov_short_specific"), 52 },
        { QStringLiteral("x_noov_short_generic"), 53 },
        { QStringLiteral("x_overlap_vague"), 54 },
        { QStringLiteral("x_explain_long"), 55 }, { QStringLiteral("x_explain_short"), 56 },
        { QStringLiteral("x_a_digit"), 57 }, { QStringLiteral("x_num_digit"), 58 },
        { QStringLiteral("x_num_nodigit"), 59 },
        { QStringLiteral("x_short_proper_noov"), 60 },
        { QStringLiteral("x_short_num_noov"), 61 },
        { QStringLiteral("x_q_explain"), 62 }, { QStringLiteral("x_q_capital"), 63 },
    };
    return map.value(key, -1);
}

void addVerifierSignal(QVector<float>& vec, const QString& key, float weight = 1.0f)
{
    const int idx = verifierSignalIndex(key);
    Q_ASSERT(idx >= 0 && idx < kVerifierTextOffset);
    if (idx >= 0 && idx < kVerifierTextOffset)
        vec[idx] += weight;
}

void addVerifierText(QVector<float>& vec, const QString& text,
                     const QString& prefix = {})
{
    const QStringList words = normalizeText(text).split(QLatin1Char(' '),
                                                       Qt::SkipEmptyParts);
    for (int i = 0; i < words.size(); ++i) {
        const QString& w = words.at(i);
        const QString kw = prefix + QStringLiteral("w:") + w;
        vec[kVerifierTextOffset
            + int(fnv1a(kw.toUtf8()) % quint32(kVerifierTextDim))] += 1.0f;
        for (int j = 0; j + 4 <= w.size(); ++j) {
            const QString kc = prefix + QStringLiteral("c:") + w.mid(j, 4);
            vec[kVerifierTextOffset
                + int(fnv1a(kc.toUtf8()) % quint32(kVerifierTextDim))] += 0.5f;
        }
        if (i + 1 < words.size()) {
            const QString kb = prefix + QStringLiteral("b:") + w
                + QLatin1Char('_') + words.at(i + 1);
            vec[kVerifierTextOffset
                + int(fnv1a(kb.toUtf8()) % quint32(kVerifierTextDim))] += 0.5f;
        }
    }
}

const QSet<QString>& commonWords()
{
    static const QSet<QString> set = [] {
        QSet<QString> out;
        const QString all = QStringLiteral(
            "il la lo un uno una i gli le the a an this that these those questo "
            "questa questi queste it he she they we you i me him her us them my your "
            "his its our their oggi ieri domani today tomorrow yesterday basta sure ok "
            "okay yes no si sì certo vero falso perché perche because grazie salve ciao "
            "ecco tutto tutti tutta tutte nulla niente nessuno sempre mai molto poco "
            "bene male meglio peggio prima dopo poi anche solo come quando dove quindi "
            "allora sono è era ha hanno può deve vuole fa fanno essere avere fare dire "
            "andare venire stare is are was were be been have has had do does did can "
            "could will would should may might must there here now then very really just "
            "only also and or but if of to in on at for with from by as about into over "
            "after before between under above again further once all any both each few "
            "more most other some such nor not too don esatto giusto corretto "
            "perfetto assolutamente certamente ovviamente naturalmente infatti "
            "appunto esattamente forse probabilmente chiaramente semplicemente "
            "volentieri prego scusa scusi buongiorno buonasera buonanotte "
            "arrivederci right correct exactly absolutely certainly obviously "
            "indeed true false maybe perhaps probably clearly simply please sorry "
            "hello hi goodbye welcome");
        for (const QString& w : all.split(QLatin1Char(' '), Qt::SkipEmptyParts))
            out.insert(w);
        return out;
    }();
    return set;
}

const QSet<QString>& numberWords()
{
    static const QSet<QString> set = {
        QStringLiteral("zero"), QStringLiteral("uno"), QStringLiteral("due"),
        QStringLiteral("tre"), QStringLiteral("quattro"), QStringLiteral("cinque"),
        QStringLiteral("sei"), QStringLiteral("sette"), QStringLiteral("otto"),
        QStringLiteral("nove"), QStringLiteral("dieci"), QStringLiteral("cento"),
        QStringLiteral("mille"), QStringLiteral("one"), QStringLiteral("two"),
        QStringLiteral("three"), QStringLiteral("four"), QStringLiteral("five"),
        QStringLiteral("six"), QStringLiteral("seven"), QStringLiteral("eight"),
        QStringLiteral("nine"), QStringLiteral("ten"), QStringLiteral("hundred"),
        QStringLiteral("thousand"),
    };
    return set;
}

const QStringList& vagueCues()
{
    static const QStringList cues = {
        QStringLiteral("importante"), QStringLiteral("interessante"),
        QStringLiteral("utile"), QStringLiteral("argomento"),
        QStringLiteral("aspetto"), QStringLiteral("numero casuale"),
        QStringLiteral("casuale"), QStringLiteral("famoso"),
        QStringLiteral("famosa"), QStringLiteral("famous"),
        QStringLiteral("thing"), QStringLiteral("stuff"),
        QStringLiteral("sure"), QStringLiteral("basta"),
        QStringLiteral("tema"), QStringLiteral("discusso"),
        QStringLiteral("strumento"), QStringLiteral("processo molto"),
        QStringLiteral("senza alcuna spiegazione"), QStringLiteral("random number"),
        QStringLiteral("linguaggio di programmazione"),
        QStringLiteral("programming language"), QStringLiteral("weather"),
    };
    return cues;
}

const QStringList& procCues()
{
    static const QStringList cues = {
        QStringLiteral("usa "), QStringLiteral("usare"), QStringLiteral("puoi "),
        QStringLiteral("devi "), QStringLiteral("sort"), QStringLiteral("sorted"),
        QStringLiteral("funzione"), QStringLiteral("comando"), QStringLiteral("passo"),
        QStringLiteral("poi "), QStringLiteral("prima "), QStringLiteral("installa"),
        QStringLiteral("esegui"), QStringLiteral("chiama"), QStringLiteral("aggiungi"),
        QStringLiteral("crea "), QStringLiteral("definisci"), QStringLiteral("return "),
        QStringLiteral("def "), QStringLiteral("riposo"), QStringLiteral("liquidi"),
        QStringLiteral("bevi"), QStringLiteral("mangia"), QStringLiteral("dormi"),
        QStringLiteral("evita"), QStringLiteral("prendi"), QStringLiteral("assumi"),
        QStringLiteral("trattamento"), QStringLiteral("terapia"), QStringLiteral("medico"),
        QStringLiteral("farmaco"), QStringLiteral("analgesico"),
        QStringLiteral("decongestionante"), QStringLiteral("checkout"),
        QStringLiteral("switch -c"), QStringLiteral("reverse"), QStringLiteral("slicing"),
    };
    return cues;
}

const QStringList& causalCues()
{
    static const QStringList cues = {
        QStringLiteral("perche"), QStringLiteral("perché"), QStringLiteral("because"),
        QStringLiteral("dovuto"), QStringLiteral("a causa"), QStringLiteral("causa"),
        QStringLiteral("scattering"), QStringLiteral("gravita"), QStringLiteral("gravità"),
        QStringLiteral("pressione"), QStringLiteral("temperatura"),
        QStringLiteral("breaks down"), QStringLiteral("reveals"), QStringLiteral("produce"),
        QStringLiteral("caused"), QStringLiteral("grazie a"), QStringLiteral("dipende da"),
    };
    return cues;
}

bool containsAny(const QString& low, const QStringList& cues)
{
    for (const QString& cue : cues)
        if (low.contains(cue))
            return true;
    return false;
}

bool hasDigit(const QString& text)
{
    for (const QChar& c : text)
        if (c.isDigit())
            return true;
    return false;
}

bool hasNumber(const QString& text)
{
    if (hasDigit(text))
        return true;
    const QStringList parts = normalizeText(text).split(QLatin1Char(' '),
                                                        Qt::SkipEmptyParts);
    for (const QString& w : parts)
        if (numberWords().contains(w))
            return true;
    return false;
}

bool hasAnyWord(const QString& normalized, const QStringList& words)
{
    const QStringList parts = normalized.split(QLatin1Char(' '),
                                                Qt::SkipEmptyParts);
    for (const QString& w : words)
        if (parts.contains(w))
            return true;
    return false;
}

QString questionType(const QString& question)
{
    const QString n = normalizeText(question);
    if (n.contains(QStringLiteral("how many"))
        || n.contains(QStringLiteral("how much"))
        || hasAnyWord(n, { QStringLiteral("quanto"), QStringLiteral("quanti"),
                           QStringLiteral("quante"), QStringLiteral("percent"),
                           QStringLiteral("probabilita"), QStringLiteral("probabilità"),
                           QStringLiteral("probability"),
                           QStringLiteral("calcola"), QStringLiteral("calculate"),
                           QStringLiteral("risolvi"), QStringLiteral("solve") }))
        return QStringLiteral("num");
    if (hasAnyWord(n, { QStringLiteral("chi"), QStringLiteral("who") }))
        return QStringLiteral("who");
    if (hasAnyWord(n, { QStringLiteral("dove"), QStringLiteral("where") }))
        return QStringLiteral("where");
    if (n.contains(QStringLiteral("in che anno"))
        || hasAnyWord(n, { QStringLiteral("quando"), QStringLiteral("when") }))
        return QStringLiteral("when");
    if (hasAnyWord(n, { QStringLiteral("come"), QStringLiteral("how") }))
        return QStringLiteral("how");
    if (hasAnyWord(n, { QStringLiteral("perche"), QStringLiteral("perché"),
                        QStringLiteral("why") }))
        return QStringLiteral("why");
    if (hasAnyWord(n, { QStringLiteral("spiega"), QStringLiteral("spiegami"),
                        QStringLiteral("explain"), QStringLiteral("describe"),
                        QStringLiteral("descrivi"), QStringLiteral("riassumi"),
                        QStringLiteral("summarize") }))
        return QStringLiteral("explain");
    if (n.contains(QStringLiteral("che ne pensi"))
        || n.contains(QStringLiteral("tell me"))
        || n.contains(QStringLiteral("help me"))
        || n.contains(QStringLiteral("puoi tradurre"))
        || hasAnyWord(n, { QStringLiteral("consigliami"), QStringLiteral("recommend"),
                           QStringLiteral("raccontami"), QStringLiteral("scrivimi"),
                           QStringLiteral("write"), QStringLiteral("translate"),
                           QStringLiteral("parlami"), QStringLiteral("dimmi"),
                           QStringLiteral("inventa") }))
        return QStringLiteral("open");
    if (hasAnyWord(n, { QStringLiteral("capitale"), QStringLiteral("capital") }))
        return QStringLiteral("capital");
    return QStringLiteral("fact");
}

QStringList properTokens(const QString& text)
{
    /* Equivalente a [A-ZÀ-Ý][\wÀ-ÿ'-]+ in gen_decision_onnx.py: il token
       inizia su una lettera maiuscola e prosegue con lettere/cifre/'/-.
       Un apostrofo o un trattino iniziale non fa partire il token. */
    QStringList out;
    QString token;
    bool inToken = false;
    auto flush = [&] {
        if (inToken && token.size() > 1
            && !commonWords().contains(token.toLower()))
            out.append(token);
        token.clear();
        inToken = false;
    };
    for (const QChar& c : text) {
        const bool tokenChar = c.isLetterOrNumber()
            || c == QLatin1Char('\'') || c == QLatin1Char('-');
        if (tokenChar) {
            if (inToken)
                token.append(c);
            else if (c.isLetter() && c.isUpper()) {
                token.append(c);
                inToken = true;
            }
        } else {
            flush();
        }
    }
    flush();
    return out;
}

bool hasCode(const QString& answer)
{
    static const QRegularExpression re(
        QStringLiteral("(def |return |[{};]|\\(\\)|=>|==|import |printf|console\\.)"));
    return answer.contains(re);
}

struct PairSignals {
    QSet<QString> qwords;
    QSet<QString> awords;
    double overlap = 0.0;
    int alen = 0;
    QString qtype;
    bool answerEmpty = false;
    bool num = false;
    bool digit = false;
    bool proper = false;
    bool code = false;
    bool proc = false;
    bool causal = false;
    bool vague = false;
    bool refusal = false;
    bool empty = false;
    bool shortAns = false;
    bool veryShort = false;
    QStringList newProper;
};

PairSignals analyzePair(const QString& question, const QString& answer)
{
    PairSignals s;
    s.qwords = contentWords(question);
    s.awords = contentWords(answer);
    s.overlap = s.qwords.isEmpty()
        ? 0.0 : double((s.qwords & s.awords).size()) / double(s.qwords.size());
    s.alen = s.awords.size();
    s.qtype = questionType(question);
    s.answerEmpty = answer.trimmed().isEmpty();
    s.num = hasNumber(answer);
    s.digit = hasDigit(answer);
    const QStringList props = properTokens(answer);
    s.proper = !props.isEmpty();
    /* Un nome proprio già presente nella domanda è il tema, non una
       nuova entità che risponde (es. "Git è uno strumento utile"). */
    for (const QString& p : props)
        if (!s.qwords.contains(p.toLower()))
            s.newProper.append(p);
    const QString low = answer.toLower();
    s.code = hasCode(answer);
    s.proc = containsAny(low, procCues());
    s.causal = containsAny(low, causalCues());
    s.vague = containsAny(low, vagueCues());
    s.refusal = containsAny(low, refusalCues());
    s.empty = s.alen < 3;
    s.shortAns = s.alen <= 4;
    s.veryShort = s.alen <= 2;
    return s;
}

QVector<float> buildPairFeatures(const QString& question, const QString& answer,
                                 const PairSignals& s)
{
    QVector<float> vec(kDim, 0.0f);
    addVerifierText(vec, question);
    addVerifierText(vec, answer);
    addVerifierText(vec, answer, QStringLiteral("a:"));

    const int ovBin = std::min(5, int(s.overlap * 5.0 + 0.5));
    addVerifierSignal(vec, QStringLiteral("x_ov_%1").arg(ovBin), 2.0f);

    const int lenBin = s.alen < 5 ? 0 : s.alen < 20 ? 1 : s.alen < 60 ? 2 : 3;
    addVerifierSignal(vec, QStringLiteral("x_len_%1").arg(lenBin), 2.0f);

    const double ratio = s.qwords.isEmpty()
        ? 0.0 : double(s.alen) / double(s.qwords.size());
    const int ratioBin = ratio < 0.3 ? 0 : ratio < 1.0 ? 1 : ratio < 3.0 ? 2 : 3;
    addVerifierSignal(vec, QStringLiteral("x_ratio_%1").arg(ratioBin), 2.0f);

    if (s.refusal) addVerifierSignal(vec, QStringLiteral("x_refusal"), 3.0f);
    if (s.empty)   addVerifierSignal(vec, QStringLiteral("x_empty"), 3.0f);

    addVerifierSignal(vec, QStringLiteral("x_q_") + s.qtype, 1.5f);
    if (s.num)    addVerifierSignal(vec, QStringLiteral("x_a_num"), 2.0f);
    if (s.digit)  addVerifierSignal(vec, QStringLiteral("x_a_digit"), 2.0f);
    if (s.proper) addVerifierSignal(vec, QStringLiteral("x_a_proper"), 2.0f);
    if (s.code)   addVerifierSignal(vec, QStringLiteral("x_a_code"), 2.0f);
    if (s.proc)   addVerifierSignal(vec, QStringLiteral("x_a_proc"), 1.5f);
    if (s.causal) addVerifierSignal(vec, QStringLiteral("x_a_causal"), 1.5f);
    if (s.vague)  addVerifierSignal(vec, QStringLiteral("x_a_vague"), 2.0f);
    if (s.shortAns) addVerifierSignal(vec, QStringLiteral("x_a_short"), 1.5f);
    if (s.veryShort) addVerifierSignal(vec, QStringLiteral("x_a_veryshort"), 1.0f);

    if (s.qtype == QStringLiteral("num")) {
        addVerifierSignal(vec, s.num ? QStringLiteral("x_num_num")
                                     : QStringLiteral("x_num_nonum"), 3.0f);
        addVerifierSignal(vec, s.digit ? QStringLiteral("x_num_digit")
                                       : QStringLiteral("x_num_nodigit"), 3.0f);
    }
    if (s.qtype == QStringLiteral("who") || s.qtype == QStringLiteral("where")
        || s.qtype == QStringLiteral("fact")) {
        if (s.proper)
            addVerifierSignal(vec, QStringLiteral("x_") + s.qtype
                                       + QStringLiteral("_proper"), 3.0f);
        else if (s.qtype == QStringLiteral("fact") && s.num)
            addVerifierSignal(vec, QStringLiteral("x_fact_num"), 3.0f);
        else
            addVerifierSignal(vec, QStringLiteral("x_") + s.qtype
                                       + QStringLiteral("_noproper"), 2.0f);
    }
    if (s.qtype == QStringLiteral("when"))
        addVerifierSignal(vec, s.num ? QStringLiteral("x_when_num")
                                     : QStringLiteral("x_when_nonum"), 3.0f);
    if (s.qtype == QStringLiteral("how"))
        addVerifierSignal(vec, (s.proc || s.code) ? QStringLiteral("x_how_proc")
                                                  : QStringLiteral("x_how_noproc"), 3.0f);
    if (s.qtype == QStringLiteral("why"))
        addVerifierSignal(vec, s.causal ? QStringLiteral("x_why_causal")
                                        : QStringLiteral("x_why_nocausal"), 3.0f);
    if (s.qtype == QStringLiteral("explain"))
        addVerifierSignal(vec, s.alen >= 5 ? QStringLiteral("x_explain_long")
                                           : QStringLiteral("x_explain_short"), 3.0f);
    if (s.qtype == QStringLiteral("open"))
        addVerifierSignal(vec, s.alen >= 4 ? QStringLiteral("x_open_long")
                                           : QStringLiteral("x_open_short"), 2.0f);

    if (s.shortAns && s.proper)
        addVerifierSignal(vec, QStringLiteral("x_short_proper"), 2.0f);
    if (s.shortAns && s.num)
        addVerifierSignal(vec, QStringLiteral("x_short_num"), 2.0f);
    if (s.vague && !(s.num || s.proper || s.code))
        addVerifierSignal(vec, QStringLiteral("x_vague_neg"), 2.5f);
    if (s.overlap == 0.0 && s.shortAns && (s.num || s.proper))
        addVerifierSignal(vec, QStringLiteral("x_noov_short_specific"), 4.0f);
    if (s.overlap == 0.0 && s.shortAns && s.proper)
        addVerifierSignal(vec, QStringLiteral("x_short_proper_noov"), 3.0f);
    if (s.overlap == 0.0 && s.shortAns && s.num)
        addVerifierSignal(vec, QStringLiteral("x_short_num_noov"), 3.0f);
    if (s.overlap == 0.0 && s.shortAns && !(s.num || s.proper))
        addVerifierSignal(vec, QStringLiteral("x_noov_short_generic"), 2.0f);
    if (s.overlap > 0.25 && s.vague && !(s.num || s.proper || s.code))
        addVerifierSignal(vec, QStringLiteral("x_overlap_vague"), 2.0f);

    l2Normalize(vec);
    return vec;
}

DecisionOnnx::Result applyRelevanceGuard(const PairSignals& s,
                                         DecisionOnnx::Result r)
{
    int label = r.label;
    QString reason;

    auto decide = [&](int newLabel, const QString& why) {
        label = newLabel;
        reason = why;
    };

    if (s.answerEmpty || s.refusal) {
        decide(0, QStringLiteral("rifiuto o risposta vuota"));
    } else if (s.qtype == QStringLiteral("num")) {
        if (s.digit)
            decide(1, QStringLiteral("domanda numerica con cifre"));
        else if (s.num && s.alen <= 2)
            decide(1, QStringLiteral("domanda numerica con numero"));
        else
            decide(0, QStringLiteral("domanda numerica senza cifre"));
    } else if (s.qtype == QStringLiteral("capital")) {
        if (!s.newProper.isEmpty())
            decide(1, QStringLiteral("capitale indicata"));
        else
            decide(0, QStringLiteral("nessuna capitale nella risposta"));
    } else if (s.veryShort) {
        if (s.digit || !s.newProper.isEmpty())
            decide(1, QStringLiteral("risposta breve specifica"));
        else
            decide(0, QStringLiteral("risposta breve generica"));
    } else if (s.vague && !(s.digit || !s.newProper.isEmpty()
                            || s.code || s.proc)) {
        decide(0, QStringLiteral("risposta generica"));
    } else if (s.qtype == QStringLiteral("how")
               || s.qtype == QStringLiteral("why")
               || s.qtype == QStringLiteral("explain")) {
        if (s.code || s.proc || s.causal)
            decide(1, QStringLiteral("risposta procedurale o causale"));
        else if (s.alen >= 6 && !s.vague)
            decide(1, QStringLiteral("spiegazione articolata"));
        else
            decide(0, QStringLiteral("manca procedura/spiegazione"));
    } else if (s.qtype == QStringLiteral("open")) {
        if (s.alen >= 3 && !s.vague)
            decide(1, QStringLiteral("risposta aperta articolata"));
        else
            decide(0, QStringLiteral("risposta aperta troppo generica"));
    }

    if (!reason.isEmpty()) {
        if (label != r.label)
            r.confidence = 1.0;
        r.label = label;
        r.reason = reason;
    }
    return r;
}

#ifdef HAVE_ONNX
Ort::Env& ortEnv()
{
    static Ort::Env env{ORT_LOGGING_LEVEL_WARNING, "prismalux_decision_onnx"};
    return env;
}

struct SessionSlot {
    std::unique_ptr<Ort::Session> session;
    std::string inputName;
    std::string outputName;
};

bool loadSession(const QString& path, SessionSlot& slot, QString* error)
{
    try {
        Ort::SessionOptions opts;
        opts.SetIntraOpNumThreads(1);
        opts.SetGraphOptimizationLevel(GraphOptimizationLevel::ORT_ENABLE_ALL);
        slot.session = std::make_unique<Ort::Session>(
            ortEnv(),
#ifdef Q_OS_WIN
            path.toStdWString().c_str(),
#else
            path.toStdString().c_str(),
#endif
            opts);

        Ort::AllocatorWithDefaultOptions alloc;
        if (slot.session->GetInputCount() < 1 || slot.session->GetOutputCount() < 1) {
            *error = QStringLiteral("modello ONNX senza input/output: %1").arg(path);
            slot.session.reset();
            return false;
        }
        slot.inputName  = slot.session->GetInputNameAllocated(0, alloc).get();
        slot.outputName = slot.session->GetOutputNameAllocated(0, alloc).get();

        /* Se la forma statica è nota, il modello deve accettare 512
           feature: un modello di un'altra famiglia produrrebbe solo
           risultati casuali. */
        const auto shape = slot.session
            ->GetInputTypeInfo(0)
            .GetTensorTypeAndShapeInfo()
            .GetShape();
        if (!shape.empty() && shape.back() > 0 && shape.back() != kDim) {
            *error = QStringLiteral("modello ONNX incompatibile: attese %1 feature, "
                                    "trovate %2 (%3)")
                         .arg(kDim).arg(shape.back()).arg(path);
            slot.session.reset();
            return false;
        }
        return true;
    } catch (const Ort::Exception& e) {
        *error = QStringLiteral("%1: %2").arg(path, QString::fromUtf8(e.what()));
        slot.session.reset();
        return false;
    }
}

std::vector<float> runSession(const SessionSlot& slot, const QVector<float>& feature)
{
    if (!slot.session || feature.size() != kDim)
        return {};

    auto memory = Ort::MemoryInfo::CreateCpu(OrtArenaAllocator, OrtMemTypeDefault);
    std::array<int64_t, 2> shape{1, kDim};
    std::vector<float> inputData(feature.begin(), feature.end());
    Ort::Value input = Ort::Value::CreateTensor<float>(
        memory, inputData.data(), inputData.size(), shape.data(), shape.size());

    const char* inputNames[]  = {slot.inputName.c_str()};
    const char* outputNames[] = {slot.outputName.c_str()};
    try {
        auto outputs = slot.session->Run(Ort::RunOptions{nullptr},
                                         inputNames, &input, 1,
                                         outputNames, 1);
        if (outputs.empty()) return {};
        const auto info = outputs.at(0).GetTensorTypeAndShapeInfo();
        const auto outShape = info.GetShape();
        int count = 1;
        for (int64_t d : outShape) {
            if (d <= 0) return {};
            count *= int(d);
        }
        const float* data = outputs.at(0).GetTensorData<float>();
        return std::vector<float>(data, data + count);
    } catch (const Ort::Exception&) {
        return {};
    }
}
#endif // HAVE_ONNX

DecisionOnnx::Result resultFromLogits(const std::vector<float>& logits,
                                      int expectedLabels)
{
    DecisionOnnx::Result r;
    if (int(logits.size()) < expectedLabels || expectedLabels <= 0) {
        r.error = QStringLiteral("output del modello ONNX non valido");
        return r;
    }

    const float maxLogit = *std::max_element(logits.begin(),
                                             logits.begin() + expectedLabels);
    double sum = 0.0;
    QVector<double> probs(expectedLabels, 0.0);
    for (int i = 0; i < expectedLabels; ++i) {
        probs[i] = std::exp(double(logits[i]) - double(maxLogit));
        sum += probs[i];
    }
    if (sum <= 0.0) {
        r.error = QStringLiteral("softmax degenere");
        return r;
    }
    int best = 0;
    for (int i = 0; i < expectedLabels; ++i) {
        probs[i] /= sum;
        if (probs[i] > probs[best]) best = i;
    }

    r.ok = true;
    r.label = best;
    r.confidence = probs[best];
    return r;
}

} // namespace

/* ══════════════════════════════════════════════════════════════
   PIMPL
   ══════════════════════════════════════════════════════════════ */
struct DecisionOnnxPrivate {
    bool ready = false;
    QString error;

#ifdef HAVE_ONNX
    SessionSlot classifier;
    SessionSlot verifier;
#endif
};

DecisionOnnx::DecisionOnnx()
    : d(std::make_unique<DecisionOnnxPrivate>())
{}

DecisionOnnx::~DecisionOnnx() = default;

const QStringList& DecisionOnnx::classifierLabels()
{
    static const QStringList labels = {"A", "B", "C", "D"};
    return labels;
}

const QStringList& DecisionOnnx::verifierLabels()
{
    static const QStringList labels = {"N", "Y"};
    return labels;
}

QString DecisionOnnx::defaultDir()
{
    const QString env = qEnvironmentVariable("PRISMALUX_DECISION_ONNX_DIR");
    if (!env.trimmed().isEmpty())
        return env.trimmed();
    return P::onnxDir() + QStringLiteral("/models");
}

QString DecisionOnnx::classifierPath()
{
    return defaultDir() + QStringLiteral("/decision_classifier.onnx");
}

QString DecisionOnnx::verifierPath()
{
    return defaultDir() + QStringLiteral("/decision_verifier.onnx");
}

bool DecisionOnnx::modelsPresent()
{
    return QFile::exists(classifierPath()) && QFile::exists(verifierPath());
}

bool DecisionOnnx::runtimeAvailable()
{
#ifdef HAVE_ONNX
    return true;
#else
    return false;
#endif
}

bool DecisionOnnx::load()
{
    if (d->ready) return true;
    d->error.clear();

#ifndef HAVE_ONNX
    d->error = QStringLiteral("ONNX Runtime non disponibile in questa build "
                              "(ricompila con libonnxruntime-dev)");
    return false;
#else
    const QString cPath = classifierPath();
    const QString vPath = verifierPath();

    QString missing;
    if (!QFile::exists(cPath)) missing += cPath + QLatin1Char('\n');
    if (!QFile::exists(vPath)) missing += vPath + QLatin1Char('\n');
    if (!missing.isEmpty()) {
        d->error = QStringLiteral("modelli ONNX mancanti:\n%1"
                                  "Rigenerali con gui/tools/gen_decision_onnx.py")
                       .arg(missing);
        return false;
    }

    QString err;
    if (!loadSession(cPath, d->classifier, &err)) {
        d->error = err;
        return false;
    }
    if (!loadSession(vPath, d->verifier, &err)) {
        d->classifier.session.reset();
        d->error = err;
        return false;
    }

    d->ready = true;
    return true;
#endif
}

bool DecisionOnnx::isReady() const { return d->ready; }
QString DecisionOnnx::lastError() const { return d->error; }

DecisionOnnx::Result DecisionOnnx::classify(const QString& text) const
{
    if (!d->ready) {
        Result r;
        r.error = d->error.isEmpty()
            ? QStringLiteral("backend ONNX non pronto") : d->error;
        return r;
    }
#ifndef HAVE_ONNX
    Result r;
    r.error = QStringLiteral("ONNX Runtime non disponibile");
    return r;
#else
    const auto logits = runSession(d->classifier, featuresText(text));
    return resultFromLogits(logits, kClassifierLabels);
#endif
}

DecisionOnnx::Result DecisionOnnx::verify(const QString& question,
                                          const QString& answer) const
{
    if (!d->ready) {
        Result r;
        r.error = d->error.isEmpty()
            ? QStringLiteral("backend ONNX non pronto") : d->error;
        return r;
    }
#ifndef HAVE_ONNX
    Result r;
    r.error = QStringLiteral("ONNX Runtime non disponibile");
    return r;
#else
    const PairSignals pairSig = analyzePair(question, answer);
    const auto logits = runSession(d->verifier,
                                   buildPairFeatures(question, answer, pairSig));
    Result r = resultFromLogits(logits, kVerifierLabels);
    if (r.ok)
        r = applyRelevanceGuard(pairSig, r);
    return r;
#endif
}

/* ══════════════════════════════════════════════════════════════
   Feature extraction — pubblica per i test di parità
   ══════════════════════════════════════════════════════════════ */
QVector<float> DecisionOnnx::featuresText(const QString& text)
{
    QVector<float> vec(kDim, 0.0f);
    addText(vec, text);
    l2Normalize(vec);
    return vec;
}

QVector<float> DecisionOnnx::featuresPair(const QString& question,
                                          const QString& answer)
{
    return buildPairFeatures(question, answer, analyzePair(question, answer));
}
