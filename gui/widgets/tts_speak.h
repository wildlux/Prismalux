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
#include <QDir>
#include <QFile>
#include <QFileInfo>
#include <QObject>
#include <QProcess>
#include <QProcessEnvironment>
#include <QStandardPaths>
#include <QString>

namespace TtsSpeak {

/* ── Cartella piper in ~/.prismalux/piper/ ── */
inline QString piperDir() {
    return QDir::homePath() + "/.prismalux/piper";
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
    /* QStandardPaths::TempLocation è sempre assoluto (fix Windows AppData/Temp bug) */
    QString tmp = QStandardPaths::writableLocation(QStandardPaths::TempLocation);
    if (tmp.isEmpty() || QDir(tmp).isRelative()) tmp = QDir::homePath();
    return tmp + "/prismalux_tts_input.txt";
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

   Sicurezza: il path viene passato via $env:PRISMA_TTS_PATH invece
   di interpolarlo nella stringa PS — immune ad apostrofi nel path
   (es. C:\Users\O'Brien\...) e ad altri caratteri speciali PS.
   ──────────────────────────────────────────────────────────── */
inline QStringList sapiArgs(const QString& tempPath) {
    /* Script PS: legge il path da variabile d'ambiente — zero interpolazione
       ReadAllText con UTF-8 esplicito per accenti italiani (è, à, ù…). */
    const QString script =
        "Add-Type -AssemblyName System.Speech;"
        "$s=New-Object System.Speech.Synthesis.SpeechSynthesizer;"
        "$s.Speak([System.IO.File]::ReadAllText("
            "$env:PRISMA_TTS_PATH,[System.Text.Encoding]::UTF8))";

    /* Il path viaggia come variabile d'ambiente: nessun rischio di injection
       indipendentemente dai caratteri presenti nel path stesso. */
    Q_UNUSED(tempPath);  /* usato via setProcessEnvironment */
    return { "-NoProfile", "-WindowStyle", "Hidden", "-Command", script };
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
    /* Windows: voci interne SAPI via PowerShell — nessun piper richiesto.
       Il path del file TTS viaggia tramite variabile d'ambiente (non
       interpolato nella stringa PS) per evitare injection da apostrofi. */
    const QString tmpPath = ttsTempFile();
    if (!writeTtsTemp(t)) return;

    auto* proc = new QProcess();
    QProcessEnvironment env = QProcessEnvironment::systemEnvironment();
    env.insert("PRISMA_TTS_PATH", QDir::toNativeSeparators(tmpPath));
    proc->setProcessEnvironment(env);
    /* Auto-cleanup al termine (fire-and-forget) */
    QObject::connect(proc,
        QOverload<int,QProcess::ExitStatus>::of(&QProcess::finished),
        proc, [proc](){ proc->deleteLater(); });
    proc->start("powershell", sapiArgs(tmpPath));
#else
    const QString bin  = piperBin();
    const QString onnx = activeOnnx();

    /* ── 1. Piper (neurale, qualità alta) ──────────────────────────
       Pipeline: piper --output_raw  →  aplay (stdin raw audio)
       Implementata con QProcess::setStandardOutputProcess() — zero
       shell intermedia, immune a injection da caratteri speciali nei
       path di bin/onnx/tempfile.
       ──────────────────────────────────────────────────────────── */
    if (!bin.isEmpty() && QFileInfo::exists(onnx)) {
        if (!writeTtsTemp(t)) return;

        auto* piperProc = new QProcess();
        auto* aplayProc = new QProcess();

        /* Pipe: stdout di piper → stdin di aplay */
        piperProc->setStandardOutputProcess(aplayProc);

        /* piper legge il testo da file (stdin redirect, senza shell) */
        piperProc->setStandardInputFile(ttsTempFile());

        /* Cleanup quando aplay finisce (fine naturale della riproduzione) */
        QObject::connect(aplayProc,
            QOverload<int,QProcess::ExitStatus>::of(&QProcess::finished),
            aplayProc, [piperProc, aplayProc](){
                piperProc->terminate();
                piperProc->waitForFinished(300);
                piperProc->deleteLater();
                aplayProc->deleteLater();
            });
        /* Se piper finisce prima (errore), termina aplay */
        QObject::connect(piperProc,
            QOverload<int,QProcess::ExitStatus>::of(&QProcess::finished),
            piperProc, [aplayProc](int code, QProcess::ExitStatus){
                if (code != 0) aplayProc->terminate();
            });

        /* Avvia prima aplay (legge), poi piper (scrive) */
        aplayProc->start("aplay", {"-r", "22050", "-f", "S16_LE", "-t", "raw", "-q", "-"});
        piperProc->start(bin, {"--model", onnx, "--output_raw"});
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

/* ── Pronuncia una parola inglese isolata (tabella IPA) ─────────
   NOTA: espeak-ng 1.52 accetta il tag SSML <phoneme alphabet="ipa"
   ph="..."> senza errori ma lo IGNORA silenziosamente — pronuncia solo
   il testo racchiuso ("x") come lettera, cioè sempre "eks/ex" qualunque
   fosse il fonema richiesto (bug osservato empiricamente, non teorico).
   Non esiste un modo affidabile per sintetizzare un fonema IPA isolato
   con questa versione di espeak-ng; si pronuncia invece la parola
   d'esempio reale (già mostrata sotto al simbolo in tabella), sintesi
   testuale normale — robusta perché è la funzione più collaudata del
   motore, non una feature SSML di nicchia.
   Voce inglese fissa: la tabella IPA è quella dell'inglese britannico.
   ──────────────────────────────────────────────────────────── */
inline void speakEnglishWord(const QString& word) {
    if (word.trimmed().isEmpty()) return;
#ifdef Q_OS_WIN
    Q_UNUSED(word);
#else
    QProcess::startDetached("espeak-ng", {"-v", "en-gb", word});
#endif
}

/* ── Pronuncia il fonema isolato, poi la parola d'esempio (tabella IPA) ──
   A differenza del tag SSML <phoneme> (ignorato, vedi sopra), la sintassi
   di espeak-ng "[[mnemonic]]" (modalità fonemi nativa, non SSML) produce
   davvero audio distinto per fonema diverso — verificato empiricamente:
   generati i 44 WAV per tutti i mnemonic di questa tabella, dimensioni e
   durate tutte diverse tra loro (non un output fisso ripetuto come nel
   bug SSML). I mnemonic NON sono l'alfabeto IPA stesso ma il set ASCII
   interno di espeak-ng, derivato confrontando `espeak-ng --ipa "parola"`
   con `espeak-ng -x "parola"` su decine di parole inglesi reali per ogni
   fonema (non da una tabella di conversione IPA→espeak trovata altrove,
   MAI verificata prima empiricamente — vedi tentativo fallito sopra).
   Sequenza sincrona: aspetta la fine del fonema isolato prima di avviare
   la parola, altrimenti i due audio si sovrappongono. ── */
inline void speakIpaSequence(const QString& mnemonic, const QString& word) {
#ifdef Q_OS_WIN
    Q_UNUSED(mnemonic);
    speakEnglishWord(word);
#else
    if (mnemonic.trimmed().isEmpty()) { speakEnglishWord(word); return; }

    auto* proc = new QProcess();
    QObject::connect(proc,
        QOverload<int,QProcess::ExitStatus>::of(&QProcess::finished),
        proc, [proc, word](int, QProcess::ExitStatus) {
            proc->deleteLater();
            speakEnglishWord(word);
        });
    proc->start("espeak-ng", {"-v", "en-gb", QString("[[%1]]").arg(mnemonic)});
#endif
}

} // namespace TtsSpeak
