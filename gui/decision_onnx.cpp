#include "decision_onnx.h"
#include "prismalux_paths.h"

#include <QByteArray>
#include <QFile>
#include <QFileInfo>
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
   Hash: FNV-1a 32 bit su UTF-8, modulo 512.
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
    };
    return cues;
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
    const auto logits = runSession(d->verifier, featuresPair(question, answer));
    return resultFromLogits(logits, kVerifierLabels);
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
    QVector<float> vec(kDim, 0.0f);
    addText(vec, question);
    addText(vec, answer);
    addText(vec, answer, QStringLiteral("a:"));

    const QSet<QString> qw = contentWords(question);
    const QSet<QString> aw = contentWords(answer);
    const int shared = (qw & aw).size();
    const double overlap = qw.isEmpty()
        ? 0.0 : double(shared) / double(qw.size());
    addFeature(vec, QStringLiteral("x_ov_%1")
                    .arg(std::min(5, int(std::lround(overlap * 5.0)))), 2.0f);

    const int aLen = aw.size();
    const int lenBin = aLen < 5 ? 0 : aLen < 20 ? 1 : aLen < 60 ? 2 : 3;
    addFeature(vec, QStringLiteral("x_len_%1").arg(lenBin), 2.0f);

    const double ratio = qw.isEmpty() ? 0.0 : double(aLen) / double(qw.size());
    const int ratioBin = ratio < 0.3 ? 0 : ratio < 1.0 ? 1 : ratio < 3.0 ? 2 : 3;
    addFeature(vec, QStringLiteral("x_ratio_%1").arg(ratioBin), 2.0f);

    const QString low = answer.toLower();
    bool refusal = false;
    for (const QString& cue : refusalCues())
        if (low.contains(cue)) { refusal = true; break; }
    if (refusal) addFeature(vec, QStringLiteral("x_refusal"), 3.0f);
    if (aLen < 3)  addFeature(vec, QStringLiteral("x_empty"), 3.0f);

    l2Normalize(vec);
    return vec;
}
