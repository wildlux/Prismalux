#pragma once
/* ══════════════════════════════════════════════════════════════
   SttWhisper — Speech-to-Text nativo, due backend disponibili:

   1. faster-whisper (priorità)  pip install faster-whisper
      · Usa CTranslate2 — 2-4× più veloce di whisper.cpp su CPU
      · VAD integrata (vad_filter=True) — salta il silenzio
      · Rilevamento: CLI `faster-whisper` in PATH/MCPs/venv/bin/
        oppure script Python Tools/scripts/fast_whisper_transcribe.py
      · Disabilitabile via QSettings: stt/fast_whisper_enabled = false

   2. whisper-cli (fallback)  compilato da WhisperAutoSetup
      C_software/whisper.cpp/build/bin/whisper-cli  +  modello GGML

   Uso:
     if (!SttWhisper::isAvailable())  { ... mostra istruzioni ... }
     else SttWhisper::transcribe(wavPath, "it", parentObj, callback);
   ══════════════════════════════════════════════════════════════ */
#include <QDir>
#include <QFileInfo>
#include <QObject>
#include <QProcess>
#include <QRegularExpression>
#include <QString>
#include <QStringList>
#include <QThread>
#include <functional>
#include "../prismalux_paths.h"
#include "../app_config.h"

namespace SttWhisper {

namespace P = PrismaluxPaths;

/* ── Cerca il binario whisper-cli (delegato a PrismaluxPaths) ── */
inline QString whisperBin()   { return P::whisperBin(); }

/* ── Cartella modelli dentro il progetto ── */
inline QString whisperModelsDir() { return P::whisperModelsDir(); }

/* ── Cerca il modello GGML dentro il progetto ── */
inline QString whisperModel()
{
    /* 1. Preferenza utente salvata in QSettings */
    const QString pref = AppConfig::s().value(P::SK::kSttModelPath).toString();
    if (!pref.isEmpty() && QFileInfo::exists(pref)) return pref;

    /* 2. Auto-detect nella cartella modelli del progetto
       Ordine: tiny > small > base > medium > large (priorità velocità) */
    const QStringList names = {
        "ggml-tiny.bin",     "ggml-tiny.en.bin",
        "ggml-small.bin",    "ggml-small.en.bin",
        "ggml-base.bin",     "ggml-base.en.bin",
        "ggml-medium.bin",
        "ggml-large-v3.bin", "ggml-large-v3-turbo.bin",
    };
    const QString dir = P::whisperModelsDir();
    for (const QString& n : names) {
        const QString p = QDir(dir).filePath(n);
        if (QFileInfo::exists(p)) return p;
    }
    return {};
}

/* ── Legge il modello preferito da QSettings ── */
inline QString preferredModel()
{
    return AppConfig::s().value(P::SK::kSttModelPath).toString();
}

/* ── Salva il modello preferito ── */
inline void savePreferredModel(const QString& path)
{
    AppConfig::s().setValue(P::SK::kSttModelPath, path);
}

/* ── Nome modello faster-whisper (HuggingFace, da QSettings o default) ── */
inline QString fastWhisperModelName()
{
    const QString pref = AppConfig::s().value(P::SK::kSttFastWhisperModel).toString();
    return pref.isEmpty() ? QStringLiteral("large-v3-turbo") : pref;
}

/* ── true se faster-whisper (CLI o script Python) è disponibile ── */
inline bool isFastWhisperAvailable()
{
    if (!P::fastWhisperBin().isEmpty()) return true;
    return QFileInfo::exists(P::fastWhisperScript()) && !P::findPython().isEmpty();
}

/* ── true se diarizzazione speaker è abilitata nelle impostazioni ── */
inline bool isDiarizeEnabled()
{
    return AppConfig::s().value(P::SK::kSttDiarizeEnabled, false).toBool();
}

/* ── Numero speaker fisso per diarizzazione (0 = auto-detect) ── */
inline int diarizeNSpeakers()
{
    return AppConfig::s().value(P::SK::kSttDiarizeNSpeakers, 0).toInt();
}

/* ── true se faster-whisper è abilitato nelle impostazioni ── */
inline bool isFastWhisperEnabled()
{
    return AppConfig::s().value(P::SK::kSttFastWhisperEnabled, true).toBool();
}

/* ── true se almeno un backend è utilizzabile ── */
inline bool isAvailable()
{
    if (isFastWhisperEnabled() && isFastWhisperAvailable()) return true;
    return !whisperBin().isEmpty() && !whisperModel().isEmpty();
}

/* ── Messaggio di aiuto quando manca qualcosa ── */
inline QString setupMessage()
{
    const bool hasBin   = !whisperBin().isEmpty();
    const bool hasModel = !whisperModel().isEmpty();

    if (!hasBin)
        return
            "\xe2\x9a\xa0  Binario whisper-cli non trovato.\n"
            "Whisper si compila automaticamente al primo avvio di Prismalux.\n"
            "Se il setup non è partito, apri Impostazioni \xe2\x86\x92 Voce & Audio.";

    if (!hasModel)
        return
            "\xe2\x9a\xa0  Modello whisper non trovato.\n"
            "Il modello viene scaricato automaticamente al primo avvio.\n"
            "Se non è ancora disponibile, apri Impostazioni \xe2\x86\x92 Voce & Audio.";

    return {};
}

/* ── Avvia la trascrizione (asincrona) ──────────────────────────
   wavPath  — file WAV 16kHz mono 16-bit
   lang     — codice lingua ISO 639-1 (es. "it", "en")
   parent   — QObject padre del QProcess creato
   onDone   — callback(text, ok): chiamata al termine

   Priorità backend:
     1. faster-whisper CLI o script Python (se abilitato e disponibile)
     2. whisper-cli con modello GGML (fallback automatico se 1 fallisce)
   ──────────────────────────────────────────────────────────── */
inline QProcess* transcribe(
    const QString& wavPath,
    const QString& lang,
    QObject*       parent,
    std::function<void(const QString& text, bool ok)> onDone)
{
    /* ── Percorso 1: faster-whisper ─────────────────────────── */
    if (isFastWhisperEnabled() && isFastWhisperAvailable()) {
        const QString fwBin    = P::fastWhisperBin();
        const QString fwScript = P::fastWhisperScript();
        const QString py       = P::findPython();
        const QString fwModel  = fastWhisperModelName();

        auto* proc = new QProcess(parent);
        const bool usingCli = !fwBin.isEmpty();

        QObject::connect(
            proc,
            QOverload<int, QProcess::ExitStatus>::of(&QProcess::finished),
            parent,
            [proc, onDone, wavPath, lang, parent, usingCli](int code, QProcess::ExitStatus) {
                QString out = QString::fromUtf8(proc->readAllStandardOutput()).trimmed();
                proc->deleteLater();

                if (code == 0 && !out.isEmpty()) {
                    /* CLI faster-whisper stampa "[0.00s -> 3.40s]  testo" — strip timestamp */
                    if (usingCli) {
                        static const QRegularExpression reFw(
                            R"(\[\d+\.\d+s\s*->\s*\d+\.\d+s\]\s*)");
                        out.remove(reFw);
                        out = out.trimmed();
                    }
                    static const QRegularExpression reMeta(
                        R"(^\s*[\[\(][^\]\)]{0,30}[\]\)]\s*$)");
                    if (!out.isEmpty() && !reMeta.match(out).hasMatch()) {
                        if (onDone) onDone(out, true);
                        return;
                    }
                }

                /* Fallback automatico a whisper-cli se faster-whisper ha fallito
                   (modulo non installato, modello non scaricato, errore generico) */
                const QString bin   = whisperBin();
                const QString model = whisperModel();
                if (bin.isEmpty() || model.isEmpty()) {
                    if (onDone) onDone(setupMessage(), false);
                    return;
                }
                auto* proc2 = new QProcess(parent);
                QObject::connect(
                    proc2,
                    QOverload<int, QProcess::ExitStatus>::of(&QProcess::finished),
                    parent,
                    [proc2, onDone](int c2, QProcess::ExitStatus) {
                        QString o = QString::fromUtf8(proc2->readAllStandardOutput()).trimmed();
                        static const QRegularExpression reTs(
                            R"(\[\d{2}:\d{2}:\d{2}\.\d{3}\s*-->\s*\d{2}:\d{2}:\d{2}\.\d{3}\]\s*)");
                        o.remove(reTs);
                        o = o.trimmed();
                        static const QRegularExpression reMeta2(
                            R"(^\s*[\[\(][^\]\)]{0,30}[\]\)]\s*$)");
                        proc2->deleteLater();
                        if (onDone) onDone(o, c2 == 0 && !o.isEmpty() && !reMeta2.match(o).hasMatch());
                    });
                const int nT = qBound(2, QThread::idealThreadCount() / 2, 8);
                proc2->start(bin, { "-m", model, "-f", wavPath, "-l", lang,
                                    "-nt", "-np", "--beam-size", "1",
                                    "-t",  QString::number(nT) });
            });

        if (usingCli) {
            proc->start(fwBin, { wavPath, "--model", fwModel, "--language", lang });
        } else {
            proc->start(py, { fwScript, wavPath, lang, fwModel });
        }
        return proc;
    }

    /* ── Percorso 2: whisper-cli (originale) ────────────────── */
    const QString bin   = whisperBin();
    const QString model = whisperModel();
    if (bin.isEmpty() || model.isEmpty()) {
        if (onDone) onDone(setupMessage(), false);
        return nullptr;
    }

    auto* proc = new QProcess(parent);
    QObject::connect(
        proc,
        QOverload<int, QProcess::ExitStatus>::of(&QProcess::finished),
        parent,
        [proc, onDone](int code, QProcess::ExitStatus) {
            QString out = QString::fromUtf8(proc->readAllStandardOutput()).trimmed();
            /* whisper-cli stampa "[HH:MM:SS.mmm --> HH:MM:SS.mmm]  testo"
               Rimuove i timestamp se presenti */
            static const QRegularExpression reTs(
                R"(\[\d{2}:\d{2}:\d{2}\.\d{3}\s*-->\s*\d{2}:\d{2}:\d{2}\.\d{3}\]\s*)");
            out.remove(reTs);
            out = out.trimmed();
            /* Filtra output composto solo da etichette meta whisper:
               [Musica], [Silenzio], [Applausi], (musica), ecc. */
            static const QRegularExpression reMeta(
                R"(^\s*[\[\(][^\]\)]{0,30}[\]\)]\s*$)");
            const bool isMeta = reMeta.match(out).hasMatch();
            proc->deleteLater();
            if (onDone) onDone(out, code == 0 && !out.isEmpty() && !isMeta);
        });

    /* Numero di thread: metà dei core logici, min 2, max 8 */
    const int nThreads = qBound(2, QThread::idealThreadCount() / 2, 8);

    proc->start(bin, {
        "-m", model,
        "-f", wavPath,
        "-l", lang,
        "-nt",                                      /* --no-timestamps: output testo puro */
        "-np",                                      /* --no-prints: nessuna barra avanzamento */
        "--beam-size", "1",                         /* greedy decoding: ~3x più veloce */
        "-t",          QString::number(nThreads),   /* thread CPU */
    });
    return proc;
}

/* ── Diarizzazione speaker opzionale (post-processing) ──────────
   Identifica chi parla in wavPath aggiungendo speaker tag al testo.
   Richiede Tools/scripts/speaker_diarize.py e almeno un backend:
     pip install simple-diarizer        (offline, nessun token)
     pip install pyannote.audio         (migliore qualità, token HF)

   transcript — path del file di testo già trascritto da Whisper
                (se vuoto, la diarizzazione non ha la colonna text)
   nSpeakers  — 0 = auto-detect, >0 = numero fisso di speaker
   hfToken    — token HuggingFace per pyannote.audio (può essere vuoto)
   onDone(json, ok) — json è il testo grezzo dell'output del processo

   Uso tipico (dopo transcribe()):
     SttWhisper::transcribe(wav, "it", this, [this, wav](const QString& text, bool ok) {
         if (ok) {
             SttWhisper::diarize(wav, transcriptPath, 0, {}, this,
                 [this](const QString& json, bool d_ok) { applyDiarization(json); });
         }
     });
   ──────────────────────────────────────────────────────────── */
inline QProcess* diarize(
    const QString& wavPath,
    const QString& transcriptPath,
    int            nSpeakers,
    const QString& hfToken,
    QObject*       parent,
    std::function<void(const QString& json, bool ok)> onDone)
{
    const QString py     = P::findPython();
    const QString script = P::root() + "/Tools/scripts/speaker_diarize.py";

    if (py.isEmpty() || !QFileInfo::exists(script)) {
        if (onDone) onDone(
            R"({"error":"speaker_diarize.py non trovato o Python non disponibile","install":"pip install simple-diarizer"})",
            false);
        return nullptr;
    }

    QStringList args = { script, wavPath };
    if (nSpeakers > 0)
        args << "--speakers" << QString::number(nSpeakers);
    if (!transcriptPath.isEmpty() && QFileInfo::exists(transcriptPath))
        args << "--transcript" << transcriptPath;
    if (!hfToken.isEmpty())
        args << "--hf-token" << hfToken;

    auto* proc = new QProcess(parent);
    QObject::connect(
        proc,
        QOverload<int, QProcess::ExitStatus>::of(&QProcess::finished),
        parent,
        [proc, onDone](int code, QProcess::ExitStatus) {
            const QString out = QString::fromUtf8(proc->readAllStandardOutput()).trimmed();
            const QString err = QString::fromUtf8(proc->readAllStandardError()).trimmed();
            proc->deleteLater();
            if (code == 0 && !out.isEmpty()) {
                if (onDone) onDone(out, true);
            } else {
                const QString msg = !err.isEmpty() ? err : out;
                if (onDone) onDone(msg.isEmpty()
                    ? R"({"error":"speaker_diarize.py ha restituito output vuoto"})"
                    : msg, false);
            }
        });

    proc->start(py, args);
    return proc;
}

/* ── Formatta il JSON di diarizzazione in testo leggibile ────────
   Converte l'output JSON di diarize() in testo con speaker tag:
     [SPEAKER_00] Buongiorno a tutti...
     [SPEAKER_01] Grazie per l'introduzione...
   ──────────────────────────────────────────────────────────── */
inline QString formatDiarization(const QString& diarJson)
{
    if (diarJson.isEmpty() || diarJson.startsWith(R"({"error")"))
        return diarJson;

    QStringList lines;
    /* Parser JSON minimale — evita dipendenza da QJsonDocument nel namespace */
    int segStart = diarJson.indexOf(QStringLiteral("\"segments\""));
    if (segStart < 0) return diarJson;

    /* Estrae ogni oggetto segmento come blocco flat (nessuna graffa annidata),
       poi cerca "speaker"/"text" al suo interno indipendentemente dall'ordine
       dei campi — speaker_diarize.py scrive "text" DOPO "end", quindi un unico
       regex con [^}]* greedy tra "start" e "text" non lo cattura mai. */
    static const QRegularExpression reBlock(R"(\{[^{}]*\})");
    static const QRegularExpression reSpeaker(R"RE("speaker"\s*:\s*"([^"]+)")RE");
    static const QRegularExpression reText(R"RE("text"\s*:\s*"([^"]*)")RE");

    QRegularExpressionMatchIterator it = reBlock.globalMatch(diarJson, segStart);
    while (it.hasNext()) {
        const QString block = it.next().captured(0);
        const auto spkMatch = reSpeaker.match(block);
        if (!spkMatch.hasMatch()) continue;
        const QString speaker = spkMatch.captured(1);
        const auto txtMatch = reText.match(block);
        const QString text = txtMatch.hasMatch() ? txtMatch.captured(1).trimmed() : QString();
        if (!text.isEmpty())
            lines << QStringLiteral("[%1] %2").arg(speaker, text);
        else
            lines << QStringLiteral("[%1]").arg(speaker);
    }
    return lines.isEmpty() ? diarJson : lines.join(QLatin1Char('\n'));
}

} // namespace SttWhisper
