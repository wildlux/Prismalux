#include "agenti_page.h"
#include "agenti_page_p.h"
#include "../prismalux_paths.h"
namespace P = PrismaluxPaths;
#include <QImage>
#include <QBuffer>
#include <QFile>
#include <QFileInfo>
#include <QTemporaryFile>
#include <QTextStream>
#include <QProcess>
#include <QDir>
#include <QTimer>
#include <QStandardPaths>
#include <functional>
#include "../widgets/stt_whisper.h"

/* ══════════════════════════════════════════════════════════════
   loadDroppedFile — dispatcher per tutti i tipi di file
   ══════════════════════════════════════════════════════════════ */
void AgentiPage::loadDroppedFile(const QString& filePath)
{
    const QString ext = QFileInfo(filePath).suffix().toLower();

    /* ── Immagini → vision ── */
    static const QStringList imgExts = {"png","jpg","jpeg","gif","webp","bmp","tiff","tif"};
    if (imgExts.contains(ext)) {
        QImage img(filePath);
        if (img.isNull()) { m_log->append("\xe2\x9d\x8c  Immagine non leggibile."); return; }
        QFileInfo fi(filePath);
        if (fi.size() > 1024*1024)
            img = img.scaled(1024,1024,Qt::KeepAspectRatio,Qt::SmoothTransformation);
        const QString mime = (ext=="png") ? "image/png"
                           : (ext=="gif") ? "image/gif"
                           : (ext=="webp") ? "image/webp" : "image/jpeg";
        m_imgMime = mime;
        QByteArray arr; QBuffer buf(&arr); buf.open(QIODevice::WriteOnly);
        img.save(&buf, ext=="png" ? "PNG" : "JPEG");
        m_imgBase64 = arr.toBase64();
        m_log->append(QString("\xf0\x9f\x96\xbc  Immagine allegata: <b>%1</b> (%2\xc3\x97%3 px)")
                      .arg(fi.fileName()).arg(img.width()).arg(img.height()));
        return;
    }

    /* ── Audio → trascrivi voce o note musicali ── */
    static const QStringList audExts = {"mp3","wav","flac","ogg","opus","aac","m4a","wma","aiff","aif"};
    if (audExts.contains(ext)) {
        _loadAudioAsText(filePath);
        return;
    }

    /* ── PDF: pdftotext (Linux) + fallback pypdf Python (cross-platform) ── */
    if (ext == "pdf") {
        m_log->append("\xf0\x9f\x93\x84  Estrazione PDF in corso...");

        /* Helper: applica il testo estratto al contesto */
        auto applyPdfText = [this, filePath](const QString& text) {
            m_docContext = _sanitize_prompt(text);
            const QString fname = QFileInfo(filePath).fileName();
            m_input->setPlaceholderText(
                QString("Allegato: %1 — fai una domanda...").arg(fname));
            m_log->append(QString("\xf0\x9f\x93\x84  PDF allegato: <b>%1</b> (%2 caratteri)")
                          .arg(fname).arg(text.length()));
        };

        /* 1. pdftotext (poppler-utils, disponibile su Linux/Mac) */
        const QString pdftt = QStandardPaths::findExecutable("pdftotext");
        if (!pdftt.isEmpty()) {
            auto* proc = new QProcess(this);
            proc->start(pdftt, {filePath, "-"});
            connect(proc, QOverload<int,QProcess::ExitStatus>::of(&QProcess::finished),
                    this, [this, proc, filePath, applyPdfText](int code, QProcess::ExitStatus){
                const QString text = QString::fromUtf8(proc->readAllStandardOutput()).trimmed();
                proc->deleteLater();
                if (code == 0 && !text.isEmpty()) { applyPdfText(text); return; }
                /* pdftotext fallito → prova Python */
                _extractPdfPython(filePath, applyPdfText);
            });
            return;
        }

        /* 2. pypdf via Python (cross-platform, funziona su Windows con venv) */
        _extractPdfPython(filePath, applyPdfText);
        return;
    }

    /* ── Fogli di calcolo Excel / ODS ── */
    static const QStringList xlsExts = {"xls","xlsx","ods","ots","fods","xlsm","xlsb"};
    if (xlsExts.contains(ext)) {
        m_log->append("\xf0\x9f\x93\x8a  Conversione foglio di calcolo...");

        auto applyXls = [this, filePath](const QString& text) {
            m_docContext = _sanitize_prompt(text);
            const QString fname = QFileInfo(filePath).fileName();
            m_input->setPlaceholderText(
                QString("Allegato: %1 — fai una domanda...").arg(fname));
            m_log->append(
                QString("\xf0\x9f\x93\x8a  Foglio allegato: <b>%1</b> (%2 righe CSV)")
                .arg(fname).arg(text.count('\n') + 1));
        };

        /* Su Windows: Python + openpyxl/xlrd (sempre disponibile via venv) */
        /* Su Linux/Mac: prova ssconvert o LibreOffice, poi Python come fallback */
#ifndef Q_OS_WIN
        const QString outCsv = QDir::tempPath() + "/prisma_sheet_out.csv";
        QFile::remove(outCsv);

        const QString ssconv = QStandardPaths::findExecutable("ssconvert");
        const QString lo     = QStandardPaths::findExecutable("libreoffice");
        const QString tmpDir = QDir::tempPath();

        if (!ssconv.isEmpty() || !lo.isEmpty()) {
            QStringList args;
            QString tool;
            if (!ssconv.isEmpty()) {
                tool = ssconv;
                args = { "--export-type=Gnumeric_stf:stf_csv", filePath, outCsv };
            } else {
                tool = lo;
                args = { "--headless", "--convert-to", "csv", "--outdir", tmpDir, filePath };
            }
            auto* proc = new QProcess(this);
            proc->start(tool, args);
            connect(proc, QOverload<int,QProcess::ExitStatus>::of(&QProcess::finished),
                    this, [this, proc, filePath, outCsv, lo, tmpDir, applyXls]
                    (int code, QProcess::ExitStatus){
                proc->deleteLater();
                /* LibreOffice mette il csv in <tmp>/<basename>.csv */
                QString csvPath = outCsv;
                if (!QFileInfo::exists(csvPath) && !lo.isEmpty())
                    csvPath = tmpDir + "/" + QFileInfo(filePath).completeBaseName() + ".csv";
                QFile f(csvPath);
                if (code == 0 && f.open(QIODevice::ReadOnly | QIODevice::Text)) {
                    applyXls(QTextStream(&f).readAll());
                } else {
                    /* ssconvert/LO falliti → Python */
                    _extractXlsPython(filePath, applyXls);
                }
            });
            return;
        }
#endif
        /* Windows (o Linux senza ssconvert/LO): Python openpyxl/xlrd */
        _extractXlsPython(filePath, applyXls);
        return;
    }

    /* ── Testo generico (txt, md, csv, json, py, cpp, …) ── */
    QFile f(filePath);
    if (!f.open(QIODevice::ReadOnly | QIODevice::Text)) {
        m_log->append(QString("\xe2\x9d\x8c  Impossibile aprire: %1").arg(filePath));
        return;
    }
    QTextStream ts(&f);
    const QString text = ts.readAll();
    m_docContext = _sanitize_prompt(text);
    const QString fname = QFileInfo(filePath).fileName();
    m_input->setPlaceholderText(
        QString("Allegato: %1 — fai una domanda...").arg(fname));
    m_log->append(QString("\xf0\x9f\x93\x8e  Documento allegato: <b>%1</b> (%2 caratteri)")
                  .arg(fname).arg(text.length()));
}

/* ══════════════════════════════════════════════════════════════
   _sanitizePyCode — corregge bug tipici nel codice Python generato
   dall'AI prima di eseguirlo.

   Pattern corretti:
   - `if name == "main":` / `if name == '__main__':` → __name__ guard corretta
   - `print x` (Python 2 syntax) → `print(x)`
   - import psutil / numpy / pandas / matplotlib / scipy →
     inietta auto-install silenzioso in testa (fallback graceful su Windows)
   ══════════════════════════════════════════════════════════════ */
QString AgentiPage::_sanitizePyCode(const QString& code)
{
    QString out = code;

    /* Pattern 1: __name__ guard scritto male dall'AI */
    static const QRegularExpression reNameMain(
        R"(if\s+_?name_?\s*==\s*['"]__?main__?['"]\s*:)",
        QRegularExpression::CaseInsensitiveOption);
    out.replace(reNameMain, "if __name__ == \"__main__\":");

    /* Pattern 2: `if __name__ == "main":` (senza doppio underscore) */
    static const QRegularExpression reNameMain2(
        R"(if\s+__name__\s*==\s*['"]main['"]\s*:)");
    out.replace(reNameMain2, "if __name__ == \"__main__\":");

    /* Nota: l'auto-install pip è stato rimosso deliberatamente.
       Iniettare un preamble di `pip install` prima che l'utente veda il
       codice nel dialog C1 bypassa la guardia C2 (conferma esplicita per
       ogni pacchetto): l'utente clicca "Esegui" credendo di approvare il
       codice AI, ma pip è già nel preamble invisibile.
       Il percorso corretto è: il codice fallisce con ModuleNotFoundError →
       C2 mostra il dialog con il nome del pacchetto → l'utente decide. */
    return out;
}

/* ══════════════════════════════════════════════════════════════
   _extractPdfPython — estrae testo da PDF via pypdf (Python).
   Fallback cross-platform quando pdftotext non è disponibile
   (tipicamente su Windows). Usa il venv creato da Avvia.bat.
   ══════════════════════════════════════════════════════════════ */
void AgentiPage::_extractPdfPython(
    const QString& filePath,
    std::function<void(const QString&)> onText)
{
    const QString python = PrismaluxPaths::findPython();

    /* QTemporaryFile: nome casuale → immune a TOCTOU targeting di nomi fissi.
       autoRemove=false: il file viene rimosso manualmente nel lambda finished
       così rimane disponibile per tutta la durata del QProcess. */
    auto* tmpScript = new QTemporaryFile(
        PrismaluxPaths::safeTempPath() + "/prisma_pdf_XXXXXX.py", this);
    tmpScript->setAutoRemove(false);
    if (!tmpScript->open()) {
        m_log->append("\xe2\x9d\x8c  Impossibile creare script Python temporaneo.");
        return;
    }
    QTextStream ts(tmpScript);
    ts << "import sys\n"
       << "try:\n"
       << "    from pypdf import PdfReader\n"
       << "except ImportError:\n"
       << "    from PyPDF2 import PdfReader\n"
       << "r = PdfReader(sys.argv[1])\n"
       << "print(''.join(p.extract_text() or '' for p in r.pages))\n";
    tmpScript->close();
    const QString script = tmpScript->fileName();

    auto* proc = new QProcess(this);
    proc->start(python, {script, filePath});
    connect(proc, QOverload<int,QProcess::ExitStatus>::of(&QProcess::finished),
            this, [this, proc, tmpScript, onText](int code, QProcess::ExitStatus) {
        const QString text = QString::fromUtf8(proc->readAllStandardOutput()).trimmed();
        proc->deleteLater();
        tmpScript->remove();   /* rimuove il file dal disco */
        tmpScript->deleteLater();
        if (code == 0 && !text.isEmpty()) {
            onText(text);
        } else {
            m_log->append(
                "\xe2\x9a\xa0  Estrazione PDF fallita.<br>"
                "Installa <code>pypdf</code>: gi\xc3\xa0 incluso in "
                "<code>requirements_python.txt</code>.<br>"
                "Su Windows lancia <code>Avvia_Prismalux.bat</code> "
                "per installarlo automaticamente.");
        }
    });
}

/* ══════════════════════════════════════════════════════════════
   _extractXlsPython — converte foglio Excel/ODS in CSV via Python.
   Usa openpyxl (xlsx/xlsm) con fallback xlrd (xls).
   Cross-platform: funziona su Windows con il venv del progetto.
   ══════════════════════════════════════════════════════════════ */
void AgentiPage::_extractXlsPython(
    const QString& filePath,
    std::function<void(const QString&)> onText)
{
    const QString python = PrismaluxPaths::findPython();

    auto* tmpScript = new QTemporaryFile(
        PrismaluxPaths::safeTempPath() + "/prisma_xls_XXXXXX.py", this);
    tmpScript->setAutoRemove(false);
    if (!tmpScript->open()) {
        m_log->append("\xe2\x9d\x8c  Impossibile creare script Python temporaneo.");
        return;
    }
    {
        QTextStream ts(tmpScript);
        ts << "import sys, csv, io\n"
           << "path = sys.argv[1]\n"
           << "try:\n"
           << "    import openpyxl\n"
           << "    wb = openpyxl.load_workbook(path, read_only=True, data_only=True)\n"
           << "    ws = wb.active\n"
           << "    out = io.StringIO()\n"
           << "    w = csv.writer(out)\n"
           << "    for row in ws.iter_rows(values_only=True):\n"
           << "        w.writerow(['' if c is None else str(c) for c in row])\n"
           << "    print(out.getvalue(), end='')\n"
           << "except ImportError:\n"
           << "    import xlrd\n"
           << "    wb = xlrd.open_workbook(path)\n"
           << "    ws = wb.sheet_by_index(0)\n"
           << "    out = io.StringIO()\n"
           << "    w = csv.writer(out)\n"
           << "    for i in range(ws.nrows):\n"
           << "        w.writerow([str(ws.cell_value(i,j)) for j in range(ws.ncols)])\n"
           << "    print(out.getvalue(), end='')\n";
    }
    tmpScript->close();
    const QString script = tmpScript->fileName();

    auto* proc = new QProcess(this);
    proc->start(python, {script, filePath});
    connect(proc, QOverload<int,QProcess::ExitStatus>::of(&QProcess::finished),
            this, [this, proc, filePath, tmpScript, onText](int code, QProcess::ExitStatus) {
        const QString text = QString::fromUtf8(proc->readAllStandardOutput()).trimmed();
        proc->deleteLater();
        tmpScript->remove();
        tmpScript->deleteLater();
        if (code == 0 && !text.isEmpty()) {
            onText(text);
        } else {
            m_log->append(
                "\xe2\x9a\xa0  Conversione foglio fallita.<br>"
                "Installa <code>openpyxl</code>: gi\xc3\xa0 in "
                "<code>requirements_python.txt</code>.<br>"
                "Su Windows lancia <code>Avvia_Prismalux.bat</code>.");
        }
    });
}

/* ══════════════════════════════════════════════════════════════
   _loadAudioAsText — trascrivi parlato o rileva note musicali
   ══════════════════════════════════════════════════════════════ */
void AgentiPage::_loadAudioAsText(const QString& filePath)
{
    /* Nomi note MIDI in italiano (0=Do, 1=Do#, 2=Re, ..., 11=Si) */
    static const char* kNoteIt[12] = {
        "Do","Do#","Re","Re#","Mi","Fa","Fa#","Sol","Sol#","La","La#","Si"
    };
    auto midiToNote = [](int midi) -> QString {
        int oct  = midi / 12 - 1;
        int name = midi % 12;
        static const char* n[12] = {
            "Do","Do#","Re","Re#","Mi","Fa","Fa#","Sol","Sol#","La","La#","Si" };
        return QString("%1%2").arg(n[name]).arg(oct);
    };
    (void)kNoteIt;

    const QString ext    = QFileInfo(filePath).suffix().toLower();
    const QString wavTmp = "/tmp/prisma_audio_in.wav";
    const QString fname  = QFileInfo(filePath).fileName();

    m_log->append(QString("\xf0\x9f\x8e\xb5  Audio ricevuto: <b>%1</b> — conversione...").arg(fname));

    /* ── Step 1: converti in WAV 16kHz mono via ffmpeg (se non già WAV) ── */
    auto runTranscription = [this, filePath, fname, midiToNote](const QString& wav) {
        if (!SttWhisper::isAvailable()) {
            m_log->append(
                "<p style='color:#e2e8f0;'>"
                "\xe2\x9a\xa0  <b>whisper-cli o modello non trovati.</b> "
                "<a href=\"settings:trascrivi\" style=\"color:#93c5fd;\">"
                "Clicca qui</a> per aprire Impostazioni &rarr; Trascrivi."
                "</p>");
            return;
        }

        m_log->append("\xe2\x8c\x9b  Trascrizione in corso...");

        m_sttProc = SttWhisper::transcribe(wav, "it", this,
            [this, filePath, fname, wav, midiToNote](const QString& text, bool ok) {
                m_sttProc = nullptr;

                /* Controlla se whisper ha rilevato solo musica/rumore */
                const bool noSpeech =
                    !ok || text.trimmed().isEmpty()
                    || text.contains("[MUSIC]",  Qt::CaseInsensitive)
                    || text.contains("[Musica]", Qt::CaseInsensitive)
                    || text.contains("[Noise]",  Qt::CaseInsensitive)
                    || text.contains("[Rumore]", Qt::CaseInsensitive)
                    || text.contains("[Applausi]",Qt::CaseInsensitive);

                if (!noSpeech) {
                    /* ── Parlato trovato ── */
                    m_docContext = _sanitize_prompt(text);
                    m_input->setPlaceholderText(
                        QString("Allegato audio: %1 — fai una domanda...").arg(fname));
                    m_log->append(
                        QString("\xf0\x9f\x8e\xa4  Trascrizione: <b>%1</b><br>"
                                "<i>%2</i>")
                        .arg(fname)
                        .arg(text.left(200).toHtmlEscaped()
                             + (text.length() > 200 ? "..." : "")));
                    return;
                }

                /* ── Nessun parlato → analisi musicale con aubionotes ── */
                m_log->append(
                    "\xf0\x9f\x8e\xb5  Nessun parlato rilevato — analisi note musicali...");

                const QString aubio = QStandardPaths::findExecutable("aubionotes");
                if (aubio.isEmpty()) {
                    m_log->append(
                        "\xe2\x9a\xa0  <b>aubionotes</b> non trovato.<br>"
                        "Installa: <code>sudo apt install aubio-tools</code>");
                    return;
                }

                auto* notesProc = new QProcess(this);
                notesProc->start(aubio, { "-i", filePath });
                connect(notesProc,
                    QOverload<int,QProcess::ExitStatus>::of(&QProcess::finished),
                    this, [this, notesProc, fname, midiToNote](int, QProcess::ExitStatus) {
                        const QString raw =
                            QString::fromLocal8Bit(notesProc->readAllStandardOutput())
                            + QString::fromLocal8Bit(notesProc->readAllStandardError());
                        notesProc->deleteLater();

                        /* Parsing: ogni riga = "onset  duration  midi_pitch" */
                        QStringList lines = raw.split('\n', Qt::SkipEmptyParts);
                        QStringList notes;
                        for (const QString& line : lines) {
                            const QStringList tok = line.trimmed().split(
                                QRegularExpression(R"(\s+)"));
                            if (tok.size() < 3) continue;
                            bool ok1, ok2, ok3;
                            const double onset = tok[0].toDouble(&ok1);
                            const double dur   = tok[1].toDouble(&ok2);
                            const int    midi  = qRound(tok[2].toDouble(&ok3));
                            if (!ok1||!ok2||!ok3||midi<0||midi>127) continue;
                            notes << QString("%1 (%.2fs-%.2fs, %2s)")
                                        .arg(midiToNote(midi))
                                        .arg(onset)
                                        .arg(onset+dur)
                                        .arg(dur, 0,'f',2);
                        }

                        if (notes.isEmpty()) {
                            m_log->append(
                                "\xf0\x9f\x8e\xb5  Nessuna nota rilevata "
                                "(file troppo corto o silenzioso?).");
                            return;
                        }

                        /* Costruisci il testo musicale */
                        const QString score =
                            QString("\xf0\x9f\x8e\xb5 Analisi musicale: %1\n"
                                    "Note rilevate (%2):\n%3")
                            .arg(fname)
                            .arg(notes.size())
                            .arg(notes.join(", "));

                        m_docContext = score;
                        m_input->setPlaceholderText(
                            QString("Audio musicale: %1 — chiedi le note...").arg(fname));
                        m_log->append(
                            QString("\xf0\x9f\x8e\xb5  <b>%1 note rilevate</b> in %2:<br>"
                                    "<code>%3</code>")
                            .arg(notes.size())
                            .arg(fname)
                            .arg(notes.mid(0, 8).join(", ")
                                 + (notes.size()>8
                                    ? QString("... +%1").arg(notes.size()-8) : "")));
                    });
            });
    };

    /* Se è già WAV va bene direttamente, altrimenti converti con ffmpeg */
    if (ext == "wav") {
        runTranscription(filePath);
    } else {
        const QString ffmpeg = QStandardPaths::findExecutable("ffmpeg");
        if (ffmpeg.isEmpty()) {
            m_log->append(
                "\xe2\x9a\xa0  <b>ffmpeg</b> non trovato.<br>"
                "Installa: <code>sudo apt install ffmpeg</code>");
            return;
        }
        QFile::remove(wavTmp);
        auto* convProc = new QProcess(this);
        convProc->start(ffmpeg, {
            "-y", "-i", filePath,
            "-ar", "16000", "-ac", "1", "-f", "wav", wavTmp
        });
        connect(convProc,
            QOverload<int,QProcess::ExitStatus>::of(&QProcess::finished),
            this, [this, convProc, wavTmp, runTranscription](int code, QProcess::ExitStatus){
                convProc->deleteLater();
                if (code != 0 || !QFileInfo::exists(wavTmp)) {
                    m_log->append("\xe2\x9d\x8c  Conversione audio fallita (ffmpeg errore).");
                    return;
                }
                runTranscription(wavTmp);
            });
    }
}

