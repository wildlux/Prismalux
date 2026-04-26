#include "agenti_page.h"
#include "agenti_page_p.h"
#include "../prismalux_paths.h"
namespace P = PrismaluxPaths;
#include <QTimer>
#include <QFileInfo>
#include <QDir>
#include <QProcess>
#include <QProcessEnvironment>
#include "../widgets/tts_speak.h"
#include <csignal>

/* ══════════════════════════════════════════════════════════════
   _ttsPlay — avvia TTS tracciato con feedback visivo di caricamento.
   Mostra "🔊 Avvio lettura..." nella toolbar durante il caricamento
   dei moduli vocali (Piper TTS → espeak-ng → spd-say).
   ══════════════════════════════════════════════════════════════ */
void AgentiPage::_ttsPlay(const QString& tts)
{
    if (tts.trimmed().isEmpty()) return;

    /* Ferma TTS precedente se attivo — prima piper, poi aplay */
    if (m_piperProc) {
        m_piperProc->kill();
        m_piperProc->waitForFinished(300);
        m_piperProc->deleteLater();
        m_piperProc = nullptr;
    }
    if (m_ttsProc) {
        m_ttsProc->kill();
        m_ttsProc->waitForFinished(300);
        if (m_ttsProc) { m_ttsProc->deleteLater(); m_ttsProc = nullptr; }
    }
#ifndef Q_OS_WIN
    QProcess::startDetached("pkill", {"-9", "aplay"});
    QProcess::startDetached("pkill", {"-9", "espeak-ng"});
    QProcess::startDetached("pkill", {"-9", "piper"});
#endif

    /* Avviso caricamento moduli vocali */
    if (m_waitLbl) {
        m_waitLbl->setText("\xf0\x9f\x94\x8a  Avvio lettura...");
        m_waitLbl->setVisible(true);
    }

    m_ttsProc = new QProcess(this);
    connect(m_ttsProc, QOverload<int,QProcess::ExitStatus>::of(&QProcess::finished),
            this, [this](int, QProcess::ExitStatus){
        if (m_waitLbl)    m_waitLbl->setVisible(false);
        if (m_btnTtsStop) m_btnTtsStop->setVisible(false);
        m_ttsPaused = false;
        if (m_btnTtsPause) { m_btnTtsPause->setText("\xe2\x8f\xb8  Pausa"); m_btnTtsPause->setVisible(false); }
        if (m_piperProc)  {
            m_piperProc->terminate();
            m_piperProc->waitForFinished(300);
            m_piperProc->deleteLater();
            m_piperProc = nullptr;
        }
        if (m_ttsProc)    { m_ttsProc->deleteLater(); m_ttsProc = nullptr; }
    });
    /* Fallisce silenziosamente (binario non trovato, permessi, ecc.) */
    connect(m_ttsProc, &QProcess::errorOccurred,
            this, [this](QProcess::ProcessError err){
        if (m_waitLbl)    m_waitLbl->setVisible(false);
        if (m_btnTtsStop) m_btnTtsStop->setVisible(false);
        m_ttsPaused = false;
        if (m_btnTtsPause) { m_btnTtsPause->setText("\xe2\x8f\xb8  Pausa"); m_btnTtsPause->setVisible(false); }
        const QString errStr = (err == QProcess::FailedToStart)
            ? "binario non trovato o permessi mancanti"
            : (err == QProcess::Crashed ? "processo terminato inaspettatamente" : "errore sconosciuto");
        m_log->append(
            "\xe2\x9a\xa0  <b>TTS non disponibile</b> (" + errStr + "). "
            "Installa <code>espeak-ng</code> oppure configura Piper TTS "
            "nelle Impostazioni per la lettura vocale.");
        if (m_ttsProc) { m_ttsProc->deleteLater(); m_ttsProc = nullptr; }
    });

#ifdef Q_OS_WIN
    /* Windows: voci native SAPI via PowerShell.
       Il path del file TTS viaggia come variabile d'ambiente (PRISMA_TTS_PATH)
       invece di essere interpolato nella stringa PS — immune ad apostrofi nel
       path (es. C:\Users\O'Brien\...) e a injection da caratteri speciali. */
    {
        const QString tmpPath = TtsSpeak::ttsTempFile();
        if (TtsSpeak::writeTtsTemp(tts)) {
            QProcessEnvironment env = QProcessEnvironment::systemEnvironment();
            env.insert("PRISMA_TTS_PATH", QDir::toNativeSeparators(tmpPath));
            m_ttsProc->setProcessEnvironment(env);
            m_ttsProc->start(QString("powershell"), TtsSpeak::sapiArgs(tmpPath));
        } else {
            if (m_waitLbl) m_waitLbl->setVisible(false);
            m_ttsProc->deleteLater(); m_ttsProc = nullptr;
            return;
        }
    }
#else
    /* Linux: Piper → espeak-ng → spd-say */
    const QString bin  = TtsSpeak::piperBin();
    const QString onnx = TtsSpeak::activeOnnx();
    if (!bin.isEmpty() && QFileInfo::exists(onnx) && TtsSpeak::writeTtsTemp(tts)) {
        /* Two-process pipe senza shell intermediaria:
           piper --output_raw → pipe stdout/stdin → aplay
           Immune a path injection: bin e onnx sono args, non interpolati in shell. */
        m_piperProc = new QProcess(this);
        m_piperProc->setStandardOutputProcess(m_ttsProc);  // piper stdout → aplay stdin
        m_piperProc->setStandardInputFile(TtsSpeak::ttsTempFile());
        m_ttsProc->start(QString("aplay"), {"-r","22050","-f","S16_LE","-t","raw","-q","-"});
        m_piperProc->start(bin, {"--model", onnx, "--output_raw"});
    } else if (!QProcess::startDetached("espeak-ng",{"-v","it+f3","--",tts})) {
        /* espeak-ng non trovato: prova spd-say tracciato */
        m_ttsProc->start("spd-say", {"--lang","it","--",tts});
    } else {
        /* espeak-ng avviato in detached — nascondi loading dopo 1.5s */
        QTimer::singleShot(1500, this, [this]{
            if (m_waitLbl) m_waitLbl->setVisible(false);
        });
        m_ttsProc->deleteLater(); m_ttsProc = nullptr;
        return;
    }
#endif
    if (m_ttsProc && m_btnTtsStop)
        m_btnTtsStop->setVisible(true);
    if (m_ttsProc && m_btnTtsPause) { m_ttsPaused = false; m_btnTtsPause->setVisible(true); }
    /* Nasconde "Avvio lettura..." non appena il processo parte */
    if (m_ttsProc) {
        QTimer::singleShot(400, this, [this]{
            if (m_waitLbl) m_waitLbl->setVisible(false);
        });
    }
}
