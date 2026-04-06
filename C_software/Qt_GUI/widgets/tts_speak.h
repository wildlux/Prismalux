#pragma once
/* ══════════════════════════════════════════════════════════════
   TtsSpeak — helper C++ per sintesi vocale Piper / espeak-ng
   ══════════════════════════════════════════════════════════════
   Gerarchia:
     1. Piper TTS (neurale, naturale)  → binario ~/.prismalux/piper/piper
        voce attiva: ~/.prismalux/piper/active_voice
        file ONNX:   ~/.prismalux/piper/voices/<id>.onnx
     2. espeak-ng  -v it+f3   (voce femminile formant, fallback)
     3. spd-say    --lang it  (speech-dispatcher, fallback finale)

   Uso:
     #include "widgets/tts_speak.h"
     TtsSpeak::speak("Ciao mondo", parentQObject);
   ══════════════════════════════════════════════════════════════ */
#include <QCoreApplication>
#include <QDir>
#include <QFile>
#include <QFileInfo>
#include <QObject>
#include <QProcess>
#include <QStandardPaths>
#include <QString>

namespace TtsSpeak {

/* ── Cartella piper accanto all'eseguibile ── */
inline QString piperDir() {
    return QCoreApplication::applicationDirPath() + "/piper";
}

/* ── Percorso binario piper ── */
inline QString piperBin() {
#ifdef Q_OS_WIN
    const QString name = "piper.exe";
#else
    const QString name = "piper";
#endif
    const QString local = piperDir() + "/" + name;
    if (QFileInfo::exists(local)) return local;
    return QStandardPaths::findExecutable("piper");
}

/* ── Voce ONNX attiva ── */
inline QString activeOnnx() {
    QString id = "it_IT-paola-medium";   /* default: voce femminile */
    QFile f(piperDir() + "/active_voice");
    if (f.open(QIODevice::ReadOnly))
        id = QString::fromUtf8(f.readAll()).trimmed();
    return piperDir() + "/voices/" + id + ".onnx";
}

/* ── Percorso file temporaneo per il testo TTS ── */
inline QString ttsTempFile() {
    return QDir::tempPath() + "/prismalux_tts_input.txt";
}

/* ── Scrive il testo nel file temporaneo (evita shell-quoting) ── */
inline bool writeTtsTemp(const QString& text) {
    QFile f(ttsTempFile());
    if (!f.open(QIODevice::WriteOnly | QIODevice::Truncate)) return false;
    f.write(text.toUtf8());
    return true;
}

/* ── Comando PowerShell SAPI (Windows) ─────────────────────────
   Legge il testo dal file temp ed usa le voci native Windows (SAPI).
   Nessuna dipendenza esterna, funziona su qualsiasi Windows 10/11.
   ──────────────────────────────────────────────────────────── */
inline QString sapiCommand() {
    /* ReadAllText con UTF-8 esplicito: necessario per accenti italiani (è, à, ù…)
       senza questo PowerShell legge il file con l'encoding ANSI di sistema (cp1252)
       e i caratteri accentati vengono letti in modo errato da SAPI. */
    return QString(
        "Add-Type -AssemblyName System.Speech;"
        "$s=New-Object System.Speech.Synthesis.SpeechSynthesizer;"
        "$s.Speak([System.IO.File]::ReadAllText('%1',"
        "[System.Text.Encoding]::UTF8))"
    ).arg(QDir::toNativeSeparators(ttsTempFile()));
}

/* ── Funzione principale ──────────────────────────────────────
   text — testo da leggere (fino a 3000 caratteri)
   Windows: voci native SAPI via PowerShell (fire-and-forget).
   Linux:   Piper → espeak-ng → spd-say (catena fallback).
   ──────────────────────────────────────────────────────────── */
inline void speak(const QString& text) {
    if (text.trimmed().isEmpty()) return;

    const QString t = text.left(3000);

#ifdef Q_OS_WIN
    /* Windows: voci interne SAPI — nessun piper richiesto */
    if (!writeTtsTemp(t)) return;
    QProcess::startDetached("powershell",
        {"-NoProfile", "-WindowStyle", "Hidden", "-Command", sapiCommand()});
#else
    const QString bin  = piperBin();
    const QString onnx = activeOnnx();

    /* ── 1. Piper (neurale, qualità alta) ── */
    if (!bin.isEmpty() && QFileInfo::exists(onnx)) {
        if (!writeTtsTemp(t)) return;
        const QString cmd =
            QString("\"%1\" --model \"%2\" --output_raw < \"%3\""
                    " | aplay -r 22050 -f S16_LE -t raw -q -")
            .arg(bin, onnx, ttsTempFile());
        QProcess::startDetached("bash", {"-c", cmd});
        return;
    }

    /* ── 2. espeak-ng: prova voce femminile, poi standard ── */
    if (QProcess::startDetached("espeak-ng", {"-v", "it+f3", "--punct=none", t}))
        return;
    if (QProcess::startDetached("espeak-ng", {"-l", "it", "--punct=none", t}))
        return;

    /* ── 3. spd-say (fallback finale) ── */
    QProcess::startDetached("spd-say", {"--lang", "it", "--", t});
#endif
}

} // namespace TtsSpeak
