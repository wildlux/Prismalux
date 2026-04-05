#pragma once
/* ══════════════════════════════════════════════════════════════
   SttWhisper — Speech-to-Text nativo via whisper.cpp (zero Python)
   ══════════════════════════════════════════════════════════════
   I binari e i modelli vivono DENTRO il progetto:
     C_software/whisper.cpp/build/bin/whisper-cli   (compilato da WhisperAutoSetup)
     C_software/whisper/models/ggml-*.bin            (scaricato da WhisperAutoSetup)

   Uso:
     if (!SttWhisper::isAvailable()) { ... mostra istruzioni ... }
     else SttWhisper::transcribe(wavPath, "it", parentObj, callback);
   ══════════════════════════════════════════════════════════════ */
#include <QDir>
#include <QFileInfo>
#include <QObject>
#include <QProcess>
#include <QRegularExpression>
#include <QSettings>
#include <QString>
#include <QStringList>
#include <functional>
#include "../prismalux_paths.h"

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
    const QString pref = QSettings("Prismalux","GUI").value("stt/model_path").toString();
    if (!pref.isEmpty() && QFileInfo::exists(pref)) return pref;

    /* 2. Auto-detect nella cartella modelli del progetto
       Ordine: small > base > tiny > medium > large (velocità vs precisione) */
    const QStringList names = {
        "ggml-small.bin",    "ggml-small.en.bin",
        "ggml-base.bin",     "ggml-base.en.bin",
        "ggml-tiny.bin",     "ggml-tiny.en.bin",
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
    return QSettings("Prismalux","GUI").value("stt/model_path").toString();
}

/* ── Salva il modello preferito ── */
inline void savePreferredModel(const QString& path)
{
    QSettings("Prismalux","GUI").setValue("stt/model_path", path);
}

/* ── true se tutto il necessario è presente ── */
inline bool isAvailable()
{
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
   ──────────────────────────────────────────────────────────── */
inline QProcess* transcribe(
    const QString& wavPath,
    const QString& lang,
    QObject*       parent,
    std::function<void(const QString& text, bool ok)> onDone)
{
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
            proc->deleteLater();
            if (onDone) onDone(out, code == 0 && !out.isEmpty());
        });

    proc->start(bin, {
        "-m", model,
        "-f", wavPath,
        "-l", lang,
        "-nt",   /* --no-timestamps: output testo puro */
        "-np",   /* --no-prints: nessuna barra di avanzamento */
    });
    return proc;
}

} // namespace SttWhisper
