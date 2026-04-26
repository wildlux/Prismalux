#include "agenti_page.h"
#include "agenti_page_p.h"
#include "../prismalux_paths.h"
namespace P = PrismaluxPaths;
#include <QTime>
#include <QElapsedTimer>
#include <QKeyEvent>
#include <QVBoxLayout>
#include <QHBoxLayout>
#include <QGridLayout>
#include <QDialog>
#include <QDialogButtonBox>
#include <QComboBox>
#include <QLabel>
#include <QFrame>
#include <QUrl>
#include <QNetworkRequest>
#include <QProcess>
#include <QFile>
#include <QDateTime>
#include <QDir>
#include <QScrollArea>
#include <QScrollBar>
#include <QMessageBox>
#include <QFileDialog>
#include <QTextStream>
#include <QPrinter>
#include <QPrintDialog>
#include <QPageSize>
#include <QImage>
#include <QBuffer>
#include <QClipboard>
#include <QGuiApplication>
#include <QMenu>
#include <QTimer>
#include <QSettings>
#include <QToolTip>
#include <QDragEnterEvent>
#include <QDropEvent>
#include <QMimeData>
#include <QTextDocument>
#include <QTextCursor>
#include "../widgets/stt_whisper.h"
#include "../widgets/chart_widget.h"
#include "../widgets/formula_parser.h"
#include "agents_config_dialog.h"
#include <QStandardPaths>
#ifndef Q_OS_WIN
#  include <csignal>
#  include <sys/types.h>
#endif

void AgentiPage::setupUI() {
    auto* lay = new QVBoxLayout(this);
    lay->setContentsMargins(16, 12, 16, 12);
    lay->setSpacing(8);

    /* ── Toolbar ── */
    auto* toolbar = new QWidget(this);
    auto* toolLay = new QHBoxLayout(toolbar);
    toolLay->setContentsMargins(0, 0, 0, 0);
    toolLay->setSpacing(8);

    m_waitLbl = new QLabel(this);
    m_waitLbl->setStyleSheet("color: #E5C400; padding: 2px 0; font-style: italic;");
    m_waitLbl->setVisible(false);
    toolLay->addWidget(m_waitLbl, 1);

    m_btnTtsStop = new QPushButton("\xe2\x8f\xb9 Ferma lettura", toolbar);
    m_btnTtsStop->setObjectName("actionBtn");
    m_btnTtsStop->setToolTip("Interrompi la lettura vocale");
    m_btnTtsStop->setVisible(false);
    toolLay->addWidget(m_btnTtsStop);
    connect(m_btnTtsStop, &QPushButton::clicked, this, [this]{
        /* Ferma piper prima di aplay (evita dati residui in pipe) */
        if (m_piperProc) {
            m_piperProc->kill();
            m_piperProc->waitForFinished(300);
            m_piperProc->deleteLater();
            m_piperProc = nullptr;
        }
        if (m_ttsProc) { m_ttsProc->kill(); m_ttsProc->waitForFinished(300); }
#ifndef Q_OS_WIN
        QProcess::startDetached("pkill", {"-9", "aplay"});
        QProcess::startDetached("pkill", {"-9", "piper"});
#endif
        m_ttsPaused = false;
        if (m_btnTtsPause) { m_btnTtsPause->setText("\xe2\x8f\xb8  Pausa"); m_btnTtsPause->setVisible(false); }
        m_btnTtsStop->setVisible(false);
    });

    m_btnTtsPause = new QPushButton("\xe2\x8f\xb8  Pausa", toolbar);
    m_btnTtsPause->setObjectName("actionBtn");
    m_btnTtsPause->setToolTip("Metti in pausa / riprendi la lettura vocale");
    m_btnTtsPause->setVisible(false);
    toolLay->addWidget(m_btnTtsPause);
    connect(m_btnTtsPause, &QPushButton::clicked, this, [this]{
#ifndef Q_OS_WIN
        const auto sendSig = [](QProcess* p, int sig){
            if (p && p->state() == QProcess::Running && p->processId() > 0)
                ::kill(static_cast<pid_t>(p->processId()), sig);
        };
        if (!m_ttsPaused) {
            sendSig(m_ttsProc,   SIGSTOP);
            sendSig(m_piperProc, SIGSTOP);
            m_ttsPaused = true;
            m_btnTtsPause->setText("\xe2\x96\xb6  Riprendi");
        } else {
            sendSig(m_ttsProc,   SIGCONT);
            sendSig(m_piperProc, SIGCONT);
            m_ttsPaused = false;
            m_btnTtsPause->setText("\xe2\x8f\xb8  Pausa");
        }
#else
        /* Windows: pausa non supportata, simula stop */
        if (m_ttsProc)   { m_ttsProc->kill(); m_ttsProc->waitForFinished(300); }
        if (m_piperProc) { m_piperProc->kill(); m_piperProc->waitForFinished(300); }
        if (m_btnTtsPause) m_btnTtsPause->setVisible(false);
        if (m_btnTtsStop)  m_btnTtsStop->setVisible(false);
#endif
    });

    /* ── Esporta conversazione ── */
    auto* btnExport = new QPushButton("\xf0\x9f\x92\xbe", toolbar);
    btnExport->setObjectName("actionBtn");
    btnExport->setToolTip("Esporta conversazione (.md / .html / .txt)");
    btnExport->setFixedWidth(32);
    toolLay->addWidget(btnExport);
    connect(btnExport, &QPushButton::clicked, this, [this](){
        if (!m_log || m_log->toPlainText().trimmed().isEmpty()) return;
        const QString ts   = QDateTime::currentDateTime().toString("yyyyMMdd_HHmmss");
        const QString name = QString("prismalux_chat_%1.md").arg(ts);
        const QString path = QFileDialog::getSaveFileName(
            this, "Esporta conversazione", QDir::homePath() + "/" + name,
            "Markdown (*.md);;HTML (*.html);;Testo (*.txt)");
        if (path.isEmpty()) return;

        QFile f(path);
        if (!f.open(QIODevice::WriteOnly | QIODevice::Truncate)) return;
        QTextStream out(&f);

        if (path.endsWith(".html", Qt::CaseInsensitive)) {
            out << m_log->toHtml();
        } else if (path.endsWith(".txt", Qt::CaseInsensitive)) {
            out << m_log->toPlainText();
        } else {
            QTextDocument doc;
            doc.setHtml(m_log->toHtml());
            out << "# Prismalux — Conversazione\n";
            out << "_Esportata il " << QDateTime::currentDateTime().toString("dd/MM/yyyy HH:mm") << "_\n\n";
            out << "---\n\n";
            out << doc.toMarkdown();
        }
    });

    /* ── Esporta come PDF ── */
    auto* btnExportPdf = new QPushButton("\xf0\x9f\x93\x84", toolbar);
    btnExportPdf->setObjectName("actionBtn");
    btnExportPdf->setToolTip("Esporta conversazione (.pdf)");
    btnExportPdf->setFixedWidth(32);
    toolLay->addWidget(btnExportPdf);
    connect(btnExportPdf, &QPushButton::clicked, this, [this](){
        if (!m_log || m_log->toPlainText().trimmed().isEmpty()) return;
        const QString ts   = QDateTime::currentDateTime().toString("yyyyMMdd_HHmmss");
        const QString name = QString("prismalux_chat_%1.pdf").arg(ts);
        const QString path = QFileDialog::getSaveFileName(
            this, "Esporta come PDF", QDir::homePath() + "/" + name,
            "PDF (*.pdf)");
        if (path.isEmpty()) return;
        QPrinter printer(QPrinter::HighResolution);
        printer.setOutputFormat(QPrinter::PdfFormat);
        printer.setOutputFileName(path);
        printer.setPageSize(QPageSize::A4);
        m_log->document()->print(&printer);
    });

    toolLay->addStretch(1);

    /* ══ Toggle Mono-Agente / Multi-Agente ══
       Default: Mono-Agente (semplice chat LLM, nessuna pipeline).
       Toggle ON → Multi-Agente: abilita pipeline, Motore Byzantino, configurazione agenti. */
    static const char* kStyleMono =
        "QPushButton{"
          "background:#1e2d45;border:2px solid #334155;color:#64748b;"
          "border-radius:14px;padding:4px 16px;font-weight:bold;font-size:12px;}"
        "QPushButton:hover{background:#243650;color:#94a3b8;}";
    static const char* kStyleMulti =
        "QPushButton{"
          "background:#00a37f20;border:2px solid #00a37f;color:#00a37f;"
          "border-radius:14px;padding:4px 16px;font-weight:bold;font-size:12px;}"
        "QPushButton:hover{background:#00a37f35;}";

    m_btnModeToggle = new QPushButton("\xf0\x9f\xa4\x96  Mono-Agente", toolbar);
    m_btnModeToggle->setCheckable(true);
    m_btnModeToggle->setChecked(false);
    m_btnModeToggle->setStyleSheet(kStyleMono);
    m_btnModeToggle->setToolTip(
        "Mono-Agente \xe2\x80\x94 risposta diretta dal modello selezionato (default)\n"
        "Multi-Agente \xe2\x80\x94 abilita pipeline agenti, Motore Byzantino e Consiglio Scientifico");
    toolLay->addWidget(m_btnModeToggle);

    /* ── Selettore LLM singolo ── */
    auto* llmLbl = new QLabel("LLM:", toolbar);
    llmLbl->setObjectName("cardDesc");
    m_cmbLLM = new QComboBox(toolbar);
    m_cmbLLM->setObjectName("settingsCombo");
    m_cmbLLM->setMinimumWidth(160);
    m_cmbLLM->setToolTip(
        "Seleziona il modello AI da usare.\n"
        "Per assegnare modelli diversi a ogni agente usa \xe2\x9a\x99\xef\xb8\x8f Configura Agenti.");
    m_cmbLLM->addItem("(caricamento...)");
    toolLay->addWidget(llmLbl);
    toolLay->addWidget(m_cmbLLM);

    /* Quando l'utente sceglie un modello diverso, lo applica all'AI client
       e lo salva come preferenza privata di questa scheda. */
    connect(m_cmbLLM, QOverload<int>::of(&QComboBox::currentIndexChanged),
            this, [this](int idx){
        if (idx < 0 || !m_cmbLLM) return;
        const QString mdl = m_cmbLLM->currentText();
        if (mdl.isEmpty() || mdl == "(caricamento...)") return;
        m_pageModel = mdl;   /* preferenza privata — non sovrascritta da modelChanged */
        m_ai->setBackend(m_ai->backend(), m_ai->host(), m_ai->port(), mdl);
    });

    /* ── Container Multi-Agente (nascosto di default in modalità Mono-Agente) ── */
    m_multiAgentBar = new QWidget(toolbar);
    auto* maLay = new QHBoxLayout(m_multiAgentBar);
    maLay->setContentsMargins(0, 0, 0, 0);
    maLay->setSpacing(8);

    /* Separatore visivo */
    auto* maSep = new QFrame(m_multiAgentBar);
    maSep->setFrameShape(QFrame::VLine);
    maSep->setFrameShadow(QFrame::Sunken);
    maSep->setStyleSheet("color:#334155;");
    maLay->addWidget(maSep);

    /* Modalità pipeline */
    auto* modeLbl = new QLabel("Modalit\xc3\xa0:", m_multiAgentBar);
    modeLbl->setObjectName("cardDesc");
    m_cmbMode = new QComboBox(m_multiAgentBar);
    m_cmbMode->addItem("\xf0\x9f\x94\x84  Pipeline Agenti");           // 0
    m_cmbMode->addItem("\xf0\x9f\x94\xae  Motore Byzantino");          // 1
    m_cmbMode->addItem("\xf0\x9f\xa7\xae  Matematico Teorico");        // 2
    m_cmbMode->addItem("\xf0\x9f\x93\x90  Matematica");                // 3
    m_cmbMode->addItem("\xf0\x9f\x92\xbb  Informatica");               // 4
    m_cmbMode->addItem("\xf0\x9f\x94\x90  Sicurezza");                 // 5
    m_cmbMode->addItem("\xe2\x9a\x9b\xef\xb8\x8f  Fisica");            // 6
    m_cmbMode->addItem("\xf0\x9f\xa7\xaa  Chimica");                   // 7
    m_cmbMode->addItem("\xf0\x9f\x8c\x90  Lingue & Traduzione");       // 8
    m_cmbMode->addItem("\xf0\x9f\x8c\x8d  Generico");                  // 9
    m_cmbMode->addItem("\xf0\x9f\x8f\x9b  Consiglio Scientifico");    // 10
    maLay->addWidget(modeLbl);
    maLay->addWidget(m_cmbMode);

    /* Pulsante auto-assegna */
    m_btnAuto = new QPushButton("\xf0\x9f\xaa\x84 Auto-assegna ruoli", m_multiAgentBar);
    m_btnAuto->setObjectName("actionBtn");
    m_btnAuto->setToolTip("Il modello pi\xc3\xb9 grande assegna ruoli e modelli automaticamente");
    maLay->addWidget(m_btnAuto);

    /* Pulsante config agenti — apre finestra separata */
    m_btnCfg = new QPushButton("\xe2\x9a\x99\xef\xb8\x8f  Configura Agenti", m_multiAgentBar);
    m_btnCfg->setObjectName("actionBtn");
    m_btnCfg->setToolTip("Apri la finestra di configurazione agenti\n(ruolo, modello, contesto RAG per ciascuno)");
    maLay->addWidget(m_btnCfg);

    m_multiAgentBar->setVisible(false);  // default: Mono-Agente
    toolLay->addWidget(m_multiAgentBar);

    /* ── Collegamento toggle Mono / Multi-Agente ── */
    connect(m_btnModeToggle, &QPushButton::toggled, this, [this](bool multiOn) {
        m_multiAgentBar->setVisible(multiOn);
        /* In multi-agente forza sempre la modalità Avvia (pipeline completa) */
        if (multiOn && !m_modePipeline) {
            m_modePipeline = true;
            m_btnRun->setText("\xe2\x96\xb6  Avvia");
            m_btnRun->setToolTip("Avvia la pipeline multi-agente completa\n"
                                 "Stop da fermo \xe2\x86\x92 torna a Singolo");
        }
        m_btnModeToggle->setText(multiOn
            ? "\xf0\x9f\x91\xa5  Multi-Agente"
            : "\xf0\x9f\xa4\x96  Mono-Agente");
        m_btnModeToggle->setStyleSheet(multiOn ? kStyleMulti : kStyleMono);
    });

    /* ── Controller LLM spostato dentro "Configura Agenti" (dialog AgentsConfigDialog) ──
       Accessibile via m_cfgDlg->controllerEnabled() — rimosso dalla toolbar per pulizia. */

    lay->addWidget(toolbar);

    /* Stato auto-assegnazione / preset */
    m_autoLbl = new QLabel("", this);
    m_autoLbl->setObjectName("cardDesc");
    m_autoLbl->setVisible(false);
    lay->addWidget(m_autoLbl);

    /* ── Output agenti (area grande) ── */
    m_log = new QTextBrowser(this);
    m_log->setObjectName("chatLog");
    m_log->setReadOnly(true);
    m_log->setOpenLinks(false);           /* gestiamo i click sui link manualmente */
    m_log->setOpenExternalLinks(false);
    /* Colore testo di default per append() con HTML misto (senza color: esplicito) */
    m_log->document()->setDefaultStyleSheet("body { color:#e2e8f0; }");
    m_log->setPlaceholderText(
        "L'output degli agenti appare qui...\n\n"
        "\xf0\x9f\x8d\xba Invocazione riuscita. Gli dei ascoltano.");
    lay->addWidget(m_log, 1);

    /* ── Smart auto-scroll: l'utente può scorrere su durante lo streaming ── */
    connect(m_log->verticalScrollBar(), &QScrollBar::valueChanged,
            this, [this](int value) {
        if (m_suppressScrollSig) return;
        m_userScrolled = (value < m_log->verticalScrollBar()->maximum());
    });

    /* ── Pannello grafico: appare automaticamente quando l'AI restituisce una formula ── */
    m_chartPanel = new QFrame(this);
    m_chartPanel->setObjectName("cardFrame");
    m_chartPanel->setVisible(false);
    m_chartPanel->setFixedHeight(260);
    {
        auto* cpLay = new QVBoxLayout(m_chartPanel);
        cpLay->setContentsMargins(8, 6, 8, 6);
        cpLay->setSpacing(4);
        auto* cpHeader = new QWidget(m_chartPanel);
        auto* cpHL = new QHBoxLayout(cpHeader);
        cpHL->setContentsMargins(0, 0, 0, 0);
        auto* cpLbl = new QLabel(
            "\xf0\x9f\x93\x8a  <b>Grafico Cartesiano</b>"
            " &nbsp;<span style='color:#7b7f9e;font-size:11px;'>"
            "\xf0\x9f\x96\xb1 Click destro per salvare il grafico"
            "</span>", m_chartPanel);
        cpLbl->setObjectName("cardTitle");
        cpLbl->setTextFormat(Qt::RichText);
        cpHL->addWidget(cpLbl, 1);
        m_btnChartOpen = new QPushButton("\xf0\x9f\x93\x88  Apri nel Grafico", m_chartPanel);
        m_btnChartOpen->setObjectName("actionBtn");
        m_btnChartOpen->setToolTip("Apri nella sezione Grafico per zoom, export e personalizzazione");
        connect(m_btnChartOpen, &QPushButton::clicked, this, [this]{
            emit requestShowInGrafico(m_lastChartExpr, m_lastChartXMin, m_lastChartXMax, m_lastChartPts);
        });
        cpHL->addWidget(m_btnChartOpen);

        auto* cpClose = new QPushButton("\xc3\x97", m_chartPanel);
        cpClose->setObjectName("actionBtn");
        cpClose->setFixedSize(22, 22);
        cpClose->setToolTip("Chiudi grafico");
        connect(cpClose, &QPushButton::clicked, m_chartPanel, &QWidget::hide);
        cpHL->addWidget(cpClose);
        cpLay->addWidget(cpHeader);
        /* Il ChartWidget viene aggiunto dinamicamente da tryShowChart() */
    }
    lay->addWidget(m_chartPanel);

    /* ── Click su link copia/TTS dentro le bolle HTML ── */
    connect(m_log, &QTextBrowser::anchorClicked,
            this, [this](const QUrl& url){
        const QString s = url.toString();
        /* formato: "copy:IDX" | "tts:IDX" | "chart:show" | "settings:<tab>" */
        if (s.startsWith("settings:")) {
            emit requestOpenSettings(s.mid(9));
            return;
        }
        if (s == "chart:show") {
            if (m_chartPanel) m_chartPanel->setVisible(true);
            return;
        }
        /* ── Elimina messaggio con conferma ── */
        if (s.startsWith("del:")) {
            QMessageBox ask(this);
            ask.setWindowTitle("\xf0\x9f\x97\x91  Elimina messaggio");
            ask.setText("<b>Eliminare questo messaggio dalla chat?</b>");
            ask.setInformativeText(
                "Questa operazione \xc3\xa8 irreversibile.");
            QPushButton* btnDel = ask.addButton("Elimina", QMessageBox::DestructiveRole);
            ask.addButton("Annulla", QMessageBox::RejectRole);
            ask.setDefaultButton(btnDel);
            ask.exec();
            if (ask.clickedButton() != btnDel) return;

            /* Salva snapshot per undo */
            m_undoHtmlStack.push(m_log->toHtml());

            /* Rimuovi il blocco della bolla dall'HTML usando il del:ID univoco */
            const QString c1s = s.mid(4, s.indexOf(':', 4) - 4); /* estrai ID */
            QString html = m_log->toHtml();
            /* Cerca e rimuove la <table> che contiene href='del:ID: */
            QRegularExpression re(
                "<table[^>]*>(?:(?!</table>).)*?" +
                QRegularExpression::escape("del:" + c1s + ":") +
                ".*?</table>(?:\\s*<p[^>]*>\\s*</p>)?",
                QRegularExpression::DotMatchesEverythingOption);
            html.remove(re);
            m_log->setHtml(html);
            m_log->moveCursor(QTextCursor::End);
            return;
        }

        if (!s.startsWith("copy:") && !s.startsWith("tts:") && !s.startsWith("edit:")) return;
        /* Formato nuovo: "copy:N:BASE64URL" — il testo è embedded nell'href.
           Formato vecchio: "copy:N"          — fallback su m_bubbleTexts. */
        const int c1 = s.indexOf(':');              // colon dopo "copy"/"tts"/"edit"
        const int c2 = s.indexOf(':', c1 + 1);      // secondo colon (nuovo formato), -1 se vecchio
        bool ok = false;
        const int idx = s.mid(c1 + 1, c2 < 0 ? -1 : c2 - c1 - 1).toInt(&ok);
        if (!ok) return;
        QString text;
        const QString origB64 = (c2 > 0) ? s.mid(c2 + 1) : QString();
        if (c2 > 0) {
            text = QString::fromUtf8(QByteArray::fromBase64(
                origB64.toLatin1(), QByteArray::Base64UrlEncoding));
        } else {
            if (!m_bubbleTexts.contains(idx)) return;
            text = m_bubbleTexts.value(idx);
        }

        if (s.startsWith("edit:")) {
            /* Apre un dialog di modifica: l'utente può editare il testo liberamente
               e scegliere se rimpiazzare la bolla nel log o inviare come nuovo task */
            auto* dlg  = new QDialog(this);
            dlg->setWindowTitle("\xe2\x9c\x8f\xef\xb8\x8f  Modifica testo");
            dlg->setMinimumSize(520, 360);
            auto* dlgLay = new QVBoxLayout(dlg);
            dlgLay->setSpacing(10);

            auto* hint = new QLabel(
                "<small>\xe2\x84\xb9  Modifica il testo. <b>Invia come task</b> lo carica nel campo "
                "input pronto per essere rielaborato dall'AI. "
                "<b>Aggiorna bolla</b> sostituisce il testo nel log.</small>");
            hint->setWordWrap(true);
            dlgLay->addWidget(hint);

            auto* editor = new QTextEdit(dlg);
            editor->setPlainText(text.trimmed());
            editor->setFocus();
            dlgLay->addWidget(editor, 1);

            auto* btnBox = new QDialogButtonBox(dlg);
            auto* btnTask   = btnBox->addButton("Invia come task \xe2\x96\xb6",
                                                QDialogButtonBox::AcceptRole);
            auto* btnUpdate = btnBox->addButton("Aggiorna bolla \xf0\x9f\x94\x84",
                                                QDialogButtonBox::ApplyRole);
            auto* btnCancel = btnBox->addButton(QDialogButtonBox::Cancel);
            btnCancel->setText("Annulla");
            dlgLay->addWidget(btnBox);

            connect(btnTask, &QPushButton::clicked, dlg, [=]{
                m_input->setPlainText(editor->toPlainText().trimmed());
                m_input->setFocus();
                m_input->moveCursor(QTextCursor::End);
                dlg->accept();
                /* Avvia il pipeline dopo la chiusura del dialog */
                QTimer::singleShot(0, this, [this]{ m_btnRun->click(); });
            });
            connect(btnUpdate, &QPushButton::clicked, dlg, [=]{
                /* Rimpiazza il testo della bolla nell'HTML del log */
                const QString newText = editor->toPlainText().trimmed();
                if (!newText.isEmpty() && !origB64.isEmpty()) {
                    /* Salva snapshot undo prima di modificare */
                    m_undoHtmlStack.push(m_log->toHtml());
                    /* Sostituisce il base64 originale nell'href con il testo aggiornato */
                    const QString newB64 = newText.left(4096).toUtf8().toBase64(
                        QByteArray::Base64UrlEncoding | QByteArray::OmitTrailingEquals);
                    QString html = m_log->toHtml();
                    /* Aggiorna tutti i link della bolla che contengono il b64 originale */
                    html.replace(origB64, newB64);
                    m_log->setHtml(html);
                    m_log->moveCursor(QTextCursor::End);
                }
                dlg->accept();
            });
            connect(btnBox, &QDialogButtonBox::rejected, dlg, &QDialog::reject);

            dlg->exec();
            dlg->deleteLater();
            return;
        }

        if (s.startsWith("copy:")) {
            /* Rimuovi tag HTML prima di copiare */
            QString plain = text;
            plain.remove(QRegularExpression("<[^>]*>"));
            plain = plain.trimmed();
            QGuiApplication::clipboard()->setText(plain.isEmpty() ? text : plain);
            /* Notifica visiva */
            QToolTip::showText(QCursor::pos(),
                "\xe2\x9c\x85  Il testo \xc3\xa8 stato copiato in memoria",
                nullptr, {}, 2000);
        } else {
            /* TTS — rimuovi tag HTML, limita a 400 parole */
            QString plain = text;
            plain.remove(QRegularExpression("<[^>]*>"));
            plain = plain.trimmed();
            if (plain.isEmpty()) plain = text;
            QStringList words = plain.split(' ', Qt::SkipEmptyParts);
            if (words.size() > 400) words = words.mid(words.size() - 400);
            _ttsPlay(words.join(" "));
        }
    });

    /* ── Copia / Leggi ora sono dentro ogni bolla — nessuna barra globale ── */

    /* Context menu sul log: copia / leggi selezione o tutto */
    m_log->setContextMenuPolicy(Qt::CustomContextMenu);
    connect(m_log, &QTextEdit::customContextMenuRequested,
            this, [this](const QPoint& pos){
        const QString sel  = m_log->textCursor().selectedText();
        const bool hasSel  = !sel.isEmpty();
        const QString label = hasSel ? "selezione" : "tutto";

        QMenu menu(m_log);
        QAction* actCopy = menu.addAction(
            "\xf0\x9f\x97\x82  Copia " + label);
        QAction* actRead = menu.addAction(
            "\xf0\x9f\x8e\x99  Leggi " + label);

        QAction* chosen = menu.exec(m_log->mapToGlobal(pos));
        const QString txt = hasSel ? sel : m_log->toPlainText();

        if (chosen == actCopy) {
            QGuiApplication::clipboard()->setText(txt);
        } else if (chosen == actRead) {
            QStringList words = txt.split(' ', Qt::SkipEmptyParts);
            if (words.size() > 400) words = words.mid(words.size() - 400);
            _ttsPlay(words.join(" "));
        }
    });

    /* ── Area input: testo + 3 colonne pulsanti ── */
    auto* inputArea = new QWidget(this);
    auto* inputGrid = new QGridLayout(inputArea);
    inputGrid->setContentsMargins(0, 0, 0, 0);
    inputGrid->setSpacing(6);
    inputGrid->setColumnStretch(0, 1);  /* col 0: testo (si espande) */
    inputGrid->setColumnStretch(1, 0);  /* col 1: Avvia (r0) · Voce (r1) */
    inputGrid->setColumnStretch(2, 0);  /* col 2: Simboli (r0) · Traduci (r1) */
    inputGrid->setColumnStretch(3, 0);  /* col 3: Documenti (r0) · Immagini (r1) */

    /* Colonna 0: campo testo (rowspan 2) — QTextEdit per altezza variabile */
    m_input = new QTextEdit(inputArea);
    m_input->setObjectName("chatInput");
    m_input->setPlaceholderText("Scrivi la tua domanda...");
    m_input->setAcceptRichText(false);
    m_input->setVerticalScrollBarPolicy(Qt::ScrollBarAsNeeded);
    m_input->setSizePolicy(QSizePolicy::Expanding, QSizePolicy::Expanding);
    inputGrid->addWidget(m_input, 0, 0, 2, 1);

    /* ── helper locale per taggare pulsanti di esecuzione ── */
    auto tagExec = [](QPushButton* btn, const char* icon, const char* text){
        btn->setProperty("execFull", btn->text());
        btn->setProperty("execIcon", QString::fromUtf8(icon));
        btn->setProperty("execText", QString::fromUtf8(text));
    };

    /* Colonna 1 — pulsante azione unificato (Singolo / Avvia) */
    m_btnRun = new QPushButton("\xf0\x9f\x92\xac CHAT con RAG", inputArea);  /* default: CHAT con RAG */
    m_btnRun->setObjectName("actionBtn");
    m_btnRun->setToolTip("Risposta immediata con contesto RAG \xe2\x80\x94 1 solo agente (Invio)\n"
                         "Stop da fermo \xe2\x86\x92 cambia modalit\xc3\xa0 (CHAT con RAG \xe2\x86\x94 Avvia)");
    tagExec(m_btnRun, "\xf0\x9f\x92\xac", "CHAT con RAG");


    /* Colonna 2 */
    m_btnVoice = new QPushButton("\xf0\x9f\x8e\xa4 Voce", inputArea);
    m_btnVoice->setObjectName("actionBtn");
    m_btnVoice->setToolTip("Parla — trascrivi la voce nel campo di testo (whisper.cpp)");
    tagExec(m_btnVoice, "\xf0\x9f\x8e\xa4", "Voce");

    auto* btnSymbols = new QPushButton("\xce\xa9  Simboli", inputArea);
    btnSymbols->setObjectName("actionBtn");
    btnSymbols->setToolTip("Inserisci caratteri speciali: matematica, greco, lingue");

    /* Colonna 3 */
    m_btnTranslate = new QPushButton("\xf0\x9f\x8c\x90  Traduci", inputArea);
    m_btnTranslate->setObjectName("actionBtn");
    m_btnTranslate->setToolTip("Traduci il testo selezionando lingue e modello AI");
    tagExec(m_btnTranslate, "\xf0\x9f\x8c\x90", "Traduci");

    /* 2 righe × 3 colonne di pulsanti:
     *  r0: Avvia    · Simboli  · Documenti
     *  r1: Voce     · Traduci  · Immagini   */
    inputGrid->addWidget(m_btnRun,       0, 1);
    inputGrid->addWidget(m_btnVoice,     1, 1);
    inputGrid->addWidget(btnSymbols,     0, 2);
    inputGrid->addWidget(m_btnTranslate, 1, 2);

    m_btnDoc = new QPushButton("\xf0\x9f\x93\x8e  Documenti", inputArea);
    m_btnDoc->setObjectName("actionBtn");
    m_btnDoc->setToolTip("Allega documento (.txt, .md, .csv, .json, .py, .cpp, .h, .pdf...)");
    tagExec(m_btnDoc, "\xf0\x9f\x93\x8e", "Documenti");
    m_btnImg = new QPushButton("\xf0\x9f\x96\xbc  Immagini", inputArea);
    m_btnImg->setObjectName("actionBtn");
    m_btnImg->setToolTip("Allega immagine per vision models (.png, .jpg, .jpeg, .gif, .webp)");
    tagExec(m_btnImg, "\xf0\x9f\x96\xbc", "Immagini");
    inputGrid->addWidget(m_btnDoc, 0, 3);
    inputGrid->addWidget(m_btnImg, 1, 3);

    lay->addWidget(inputArea);

    /* ── Footer suggerimenti ── */
    {
        auto* hints = new QLabel(
            "\xe2\x8c\xa8  "
            "<b>Invio</b> = modalit\xc3\xa0 corrente"
            " &nbsp;&nbsp;\xc2\xb7&nbsp;&nbsp; "
            "<b>Stop da fermo</b> = cambia CHAT con RAG \xe2\x86\x94 Avvia"
            " &nbsp;&nbsp;\xc2\xb7&nbsp;&nbsp; "
            "<b>Shift+Invio</b> = a capo"
            " &nbsp;&nbsp;\xc2\xb7&nbsp;&nbsp; "
            "\xf0\x9f\x93\x88  Grafico cartesiano: es. "
            "<i>Grafico di sin(x) per x da -3 a 3</i>",
            this);
        hints->setObjectName("footerHints");
        hints->setAlignment(Qt::AlignLeft | Qt::AlignVCenter);
        hints->setWordWrap(false);
        hints->setTextFormat(Qt::RichText);
        lay->addWidget(hints);
    }

    /* ── Connessioni ── */

    /* Pulsante unico: se busy → abort; altrimenti esegue in base a modalità */
    connect(m_btnRun, &QPushButton::clicked, this, [this]{
        if (m_ai->busy()) { m_ai->abort(); return; }
        if (!m_modePipeline) {
            /* Modalità Singolo: forza 1 agente */
            m_cfgDlg->numAgentsSpinBox()->setValue(1);
            m_maxShots = 1;
        }
        const int mode = m_cmbMode->currentIndex();
        if      (mode == 10) runConsiglioScientifico();
        else if (mode == 2)  runMathTheory();
        else                 runPipeline();
    });

    /* Invio = modalità corrente  |  Shift+Invio = a capo nel testo */
    {
        struct EnterFilter : public QObject {
            QPushButton* btn;
            EnterFilter(QPushButton* b, QObject* p) : QObject(p), btn(b) {}
            bool eventFilter(QObject*, QEvent* ev) override {
                if (ev->type() == QEvent::KeyPress) {
                    auto* ke = static_cast<QKeyEvent*>(ev);
                    const bool enter = ke->key() == Qt::Key_Return
                                    || ke->key() == Qt::Key_Enter;
                    if (enter && !(ke->modifiers() & Qt::ShiftModifier)) {
                        btn->click();
                        return true;
                    }
                }
                return false;
            }
        };
        m_input->installEventFilter(new EnterFilter(m_btnRun, m_input));
    }

    /* ── Pannello inline caratteri speciali (toggle) ── */
    {
        static const struct { const char* cat; const char* chars; int btnW; } kGroups[] = {
            /* ── Funzioni testo ── */
            { "Funzioni matematiche",
              "sin( cos( tan( cot( sec( csc( "
              "arcsin( arccos( arctan( arccot( "
              "sinh( cosh( tanh( coth( "
              "log( ln( log\xe2\x82\x82( log\xe2\x82\x81\xe2\x82\x80( "   /* log₂( log₁₀( */
              "lim exp( "
              "\xe2\x88\x9a( \xe2\x88\x9b( \xe2\x88\x9c( "               /* √( ∛( ∜( */
              "max( min( abs( floor( ceil( gcd( lcm( mod",
              46 },
            /* ── LaTeX ── */
            { "LaTeX — Funzioni",
              "\\sin \\cos \\tan \\cot \\sec \\csc "
              "\\arcsin \\arccos \\arctan "
              "\\sinh \\cosh \\tanh "
              "\\log \\ln \\lg \\exp \\lim \\max \\min \\sup \\inf "
              "\\gcd \\lcm \\mod \\deg",
              50 },
            { "LaTeX — Operatori",
              "\\int \\iint \\iiint \\oint "
              "\\sum \\prod \\coprod "
              "\\frac{}{} \\sqrt{} \\sqrt[]{} "
              "\\partial \\nabla \\Delta "
              "\\pm \\mp \\times \\div \\cdot "
              "\\leq \\geq \\neq \\approx \\equiv \\sim "
              "\\in \\notin \\subset \\supset \\subseteq \\supseteq "
              "\\forall \\exists \\infty \\emptyset",
              54 },
            { "LaTeX — Lettere greche",
              "\\alpha \\beta \\gamma \\delta \\epsilon \\varepsilon "
              "\\zeta \\eta \\theta \\vartheta \\iota \\kappa "
              "\\lambda \\mu \\nu \\xi \\pi \\varpi \\rho \\varrho "
              "\\sigma \\varsigma \\tau \\upsilon \\phi \\varphi \\chi \\psi \\omega "
              "\\Gamma \\Delta \\Theta \\Lambda \\Xi \\Pi \\Sigma \\Upsilon \\Phi \\Psi \\Omega",
              50 },
            { "Matematica",
              "\xe2\x88\x91 \xe2\x88\xab \xe2\x88\x8f \xe2\x88\x9a \xe2\x88\x9e "
              "\xe2\x88\x82 \xcf\x80 \xc2\xb1 \xc3\x97 \xc3\xb7 "
              "\xe2\x89\xa0 \xe2\x89\xa4 \xe2\x89\xa5 \xe2\x89\x88 \xe2\x89\xa1 "
              "\xe2\x88\x88 \xe2\x88\x89 \xe2\x8a\x82 \xe2\x8a\x83 \xe2\x88\x80 "
              "\xe2\x88\x83 \xe2\x88\x87 \xe2\x84\x9d \xe2\x84\xa4 \xe2\x84\x95 \xe2\x84\x82",
              32 },
            { "Greco",
              "\xce\xb1 \xce\xb2 \xce\xb3 \xce\xb4 \xce\xb5 \xce\xb6 \xce\xb7 \xce\xb8 "
              "\xce\xbb \xce\xbc \xce\xbd \xce\xbe \xcf\x81 \xcf\x83 \xcf\x84 "
              "\xcf\x86 \xcf\x87 \xcf\x88 \xcf\x89 "
              "\xce\x94 \xce\x9b \xce\xa3 \xce\xa8 \xce\xa9 \xce\x93 \xce\xa0 \xce\xa6 \xce\x98",
              32 },
            { "Potenze / Indici",
              "\xc2\xb2 \xc2\xb3 \xc2\xb9 \xe2\x81\xb0 \xe2\x81\xb4 \xe2\x81\xb5 \xe2\x81\xb6 \xe2\x81\xb7 \xe2\x81\xb8 \xe2\x81\xb9 "
              "\xe2\x82\x80 \xe2\x82\x81 \xe2\x82\x82 \xe2\x82\x83 \xe2\x82\x84 \xe2\x82\x85 \xe2\x82\x86 \xe2\x82\x87 \xe2\x82\x88 \xe2\x82\x89 "
              "\xc2\xbd \xe2\x85\x93 \xe2\x85\x94 \xc2\xbc \xc2\xbe",
              32 },
            { "Lingue / Accenti",
              "\xc3\xa9 \xc3\xa8 \xc3\xaa \xc3\xab "
              "\xc3\xa0 \xc3\xa2 \xc3\xa4 "
              "\xc3\xb9 \xc3\xbb \xc3\xbc "
              "\xc3\xb4 \xc3\xb6 \xc3\xb1 \xc3\xa7 \xc3\x9f "
              "\xc3\xa6 \xc3\xb8 \xc3\xa5 "
              "\xc3\xac \xc3\xae \xc3\xaf \xc3\xb3 \xc3\xb2",
              32 },
            /* ── Frecce ── */
            { "Frecce",
              /* → ← ↑ ↓ ↔ ↕ */
              "\xe2\x86\x92 \xe2\x86\x90 \xe2\x86\x91 \xe2\x86\x93 \xe2\x86\x94 \xe2\x86\x95 "
              /* ⇒ ⇐ ⇑ ⇓ ⇔ */
              "\xe2\x87\x92 \xe2\x87\x90 \xe2\x87\x91 \xe2\x87\x93 \xe2\x87\x94 "
              /* ↗ ↘ ↙ ↖ ↺ ↻ */
              "\xe2\x86\x97 \xe2\x86\x98 \xe2\x86\x99 \xe2\x86\x96 \xe2\x86\xba \xe2\x86\xbb "
              /* ➜ ➝ ➞ ➡ ⟵ ⟶ ⟷ ⟹ */
              "\xe2\x9e\x9c \xe2\x9e\x9d \xe2\x9e\x9e \xe2\x9e\xa1 "
              "\xe2\x9f\xb5 \xe2\x9f\xb6 \xe2\x9f\xb7 \xe2\x9f\xb9",
              32 },
            /* ── Valute ── */
            { "Valute",
              /* € £ $ ¥ ¢ ₿ ₽ ₩ ₪ ₫ ₴ ₦ ₱ ₭ ₮ ₺ ₼ ₾ */
              "\xe2\x82\xac \xc2\xa3 $ \xc2\xa5 \xc2\xa2 "
              "\xe2\x82\xbf \xe2\x82\xbd \xe2\x82\xa9 \xe2\x82\xaa \xe2\x82\xab "
              "\xe2\x82\xb4 \xe2\x82\xa6 \xe2\x82\xb1 \xe2\x82\xad \xe2\x82\xae "
              "\xe2\x82\xba \xe2\x82\xbc \xe2\x82\xbe",
              32 },
            /* ── Tipografia ── */
            { "Tipografia",
              /* © ® ™ ° § ¶ † ‡ ※ ‰ … — – · • ‣ ″ ′ */
              "\xc2\xa9 \xc2\xae \xe2\x84\xa2 \xc2\xb0 \xc2\xa7 \xc2\xb6 "
              "\xe2\x80\xa0 \xe2\x80\xa1 \xe2\x80\xbb \xe2\x80\xb0 "
              "\xe2\x80\xa6 \xe2\x80\x94 \xe2\x80\x93 \xc2\xb7 \xe2\x80\xa2 \xe2\x80\xa3 "
              "\xe2\x80\xb3 \xe2\x80\xb2 "
              /* « » „ " " ‹ › */
              "\xc2\xab \xc2\xbb \xe2\x80\x9e \xe2\x80\x9c \xe2\x80\x9d \xe2\x80\xb9 \xe2\x80\xba",
              32 },
            /* ── Geometria / Forme ── */
            { "Forme / Simboli",
              /* ● ○ ■ □ ▲ △ ▼ ▽ ◆ ◇ ★ ☆ ♦ ♠ ♣ ♥ ♡ */
              "\xe2\x97\x8f \xe2\x97\x8b \xe2\x96\xa0 \xe2\x96\xa1 "
              "\xe2\x96\xb2 \xe2\x96\xb3 \xe2\x96\xbc \xe2\x96\xbd "
              "\xe2\x97\x86 \xe2\x97\x87 \xe2\x98\x85 \xe2\x98\x86 "
              "\xe2\x99\xa6 \xe2\x99\xa0 \xe2\x99\xa3 \xe2\x99\xa5 \xe2\x99\xa1 "
              /* ✓ ✗ ✔ ✘ ☑ ☐ ☒ */
              "\xe2\x9c\x93 \xe2\x9c\x97 \xe2\x9c\x94 \xe2\x9c\x98 "
              "\xe2\x98\x91 \xe2\x98\x90 \xe2\x98\x92",
              32 },
        };

        /* Contenuto pannello — layout a 2 colonne: categoria | pulsanti */
        m_symbolsPanel = new QFrame;
        m_symbolsPanel->setObjectName("actionCard");
        m_symbolsPanel->setFrameShape(QFrame::StyledPanel);
        auto* panGrid = new QGridLayout(m_symbolsPanel);
        panGrid->setContentsMargins(6, 4, 6, 4);
        panGrid->setHorizontalSpacing(8);
        panGrid->setVerticalSpacing(3);
        panGrid->setColumnMinimumWidth(0, 118);
        panGrid->setColumnStretch(1, 1);

        constexpr int BTN_H = 22;
        /* Larghezza viewport stimata conservativamente:
           finestra - sidebar(210) - margini - etichetta categoria(118+8+12) */
        constexpr int kViewportW = 760;

        int gridRow = 0;
        for (auto& g : kGroups) {
            /* ── Colonna sinistra: etichetta categoria ── */
            auto* catLbl = new QLabel(QString::fromUtf8(g.cat), m_symbolsPanel);
            catLbl->setStyleSheet(
                "font-size:10px; color:#99aacc; padding:2px 6px;"
                "background:#1e1e2a; border-radius:3px; border:1px solid #2a2a44;");
            catLbl->setAlignment(Qt::AlignRight | Qt::AlignVCenter);
            catLbl->setWordWrap(true);
            catLbl->setFixedWidth(118);
            panGrid->addWidget(catLbl, gridRow, 0, Qt::AlignTop | Qt::AlignRight);

            /* ── Colonna destra: pulsanti simbolo (QGridLayout per allineamento colonne) ── */
            auto* btnArea = new QWidget(m_symbolsPanel);
            auto* btnGrid = new QGridLayout(btnArea);
            btnGrid->setContentsMargins(0, 0, 0, 0);
            btnGrid->setSpacing(1);

            QStringList tokens = QString::fromUtf8(g.chars).split(' ', Qt::SkipEmptyParts);

            /* Larghezza adattiva: misura il token più lungo con font metrics + padding */
            QFontMetrics fm(m_symbolsPanel->font());
            int fixedW = g.btnW;
            for (const QString& ch : tokens)
                fixedW = std::max(fixedW, fm.horizontalAdvance(ch) + 14);

            /* Numero colonne adattivo: quanti pulsanti entrano nella viewport senza scroll */
            const int perRow = std::max(4, kViewportW / (fixedW + 1));

            int bCol = 0, bRow = 0;
            for (const QString& ch : tokens) {
                if (ch.isEmpty()) continue;
                if (bCol >= perRow) { bCol = 0; ++bRow; }
                auto* b = new QPushButton(ch, btnArea);
                b->setObjectName("symbolBtn");
                b->setFixedSize(fixedW, BTN_H);
                connect(b, &QPushButton::clicked, this, [this, ch]{
                    m_input->insertPlainText(ch);
                });
                btnGrid->addWidget(b, bRow, bCol);
                ++bCol;
            }

            panGrid->addWidget(btnArea, gridRow, 1);
            ++gridRow;
        }

        /* Scroll verticale: max 180px visibili, poi scrollabile */
        auto* outerScroll = new QScrollArea(this);
        outerScroll->setWidget(m_symbolsPanel);
        outerScroll->setWidgetResizable(true);
        outerScroll->setMaximumHeight(260);
        outerScroll->setFrameShape(QFrame::NoFrame);
        outerScroll->setHorizontalScrollBarPolicy(Qt::ScrollBarAlwaysOff);
        outerScroll->setVerticalScrollBarPolicy(Qt::ScrollBarAsNeeded);
        outerScroll->setVisible(false);
        lay->addWidget(outerScroll);

        connect(btnSymbols, &QPushButton::clicked, this, [outerScroll]{
            outerScroll->setVisible(!outerScroll->isVisible());
        });
    }

    /* ── Dialog traduzione ── */
    connect(m_btnTranslate, &QPushButton::clicked, this, [this]{
        static const QStringList kLangs = {
            "Auto-rileva",
            "Italiano", "Inglese", "Francese", "Spagnolo", "Portoghese",
            "Tedesco", "Olandese", "Svedese", "Norvegese", "Danese",
            "Russo", "Ucraino", "Polacco", "Ceco", "Slovacco",
            "Cinese (Mandarino)", "Giapponese", "Coreano",
            "Arabo", "Persiano (Farsi)", "Turco", "Ebraico",
            "Hindi", "Bengalese", "Urdu",
            "Swahili", "Hausa", "Somalo",
            "Greco", "Rumeno", "Ungherese", "Finlandese",
            "Catalano", "Galiziano", "Basco",
            "Indonesiano", "Malese", "Tagalog (Filippino)",
            "Thai", "Vietnamita"
        };
        if (m_ai->busy()) {
            m_log->append("\xe2\x9a\xa0  Un'altra operazione \xc3\xa8 in corso.");
            return;
        }
        QString inputText = m_input->toPlainText().trimmed();
        if (inputText.isEmpty()) {
            m_log->append("\xe2\x9a\xa0  Inserisci il testo da tradurre nel campo input.");
            return;
        }

        auto* dlg = new QDialog(this);
        dlg->setWindowTitle("\xf0\x9f\x8c\x90  Traduci testo");
        dlg->setFixedWidth(420);
        auto* dlgLay = new QVBoxLayout(dlg);
        dlgLay->setSpacing(10);

        /* ── Lingua sorgente ── */
        auto* srcRow = new QHBoxLayout;
        srcRow->addWidget(new QLabel("Da:", dlg));
        auto* cmbSrc = new QComboBox(dlg);
        cmbSrc->addItems(kLangs);
        /* Ripristina ultima selezione */
        int si = cmbSrc->findText(m_translateSrcLang);
        if (si >= 0) cmbSrc->setCurrentIndex(si);
        srcRow->addWidget(cmbSrc, 1);
        dlgLay->addLayout(srcRow);

        /* ── Lingua target ── */
        auto* dstRow = new QHBoxLayout;
        dstRow->addWidget(new QLabel("A:", dlg));
        auto* cmbDst = new QComboBox(dlg);
        for (const QString& l : kLangs)
            if (l != "Auto-rileva") cmbDst->addItem(l);
        cmbDst->setCurrentText(m_translateDstLang.isEmpty() ? "Italiano" : m_translateDstLang);
        dstRow->addWidget(cmbDst, 1);
        dlgLay->addLayout(dstRow);

        /* ── Modello ── */
        auto* mdlRow = new QHBoxLayout;
        mdlRow->addWidget(new QLabel("Modello:", dlg));
        auto* cmbMdl = new QComboBox(dlg);
        for (auto& mi : m_modelInfos) cmbMdl->addItem(mi.name);
        if (cmbMdl->count() == 0) cmbMdl->addItem(m_ai->model());
        else {
            int idx = cmbMdl->findText(m_ai->model());
            if (idx >= 0) cmbMdl->setCurrentIndex(idx);
        }
        mdlRow->addWidget(cmbMdl, 1);
        dlgLay->addLayout(mdlRow);

        /* ── Testo in anteprima ── */
        auto* previewLbl = new QLabel(
            QString("\xf0\x9f\x93\x9d  Testo: <i>%1</i>")
            .arg(inputText.length() > 80
                 ? inputText.left(80) + "\xe2\x80\xa6"
                 : inputText), dlg);
        previewLbl->setWordWrap(true);
        previewLbl->setStyleSheet("color:#9ca3af; font-size:11px;");
        dlgLay->addWidget(previewLbl);

        /* ── Pulsanti ── */
        auto* bb = new QDialogButtonBox(
            QDialogButtonBox::Ok | QDialogButtonBox::Cancel, dlg);
        bb->button(QDialogButtonBox::Ok)->setText("\xf0\x9f\x8c\x90  Traduci");
        dlgLay->addWidget(bb);
        connect(bb, &QDialogButtonBox::accepted, dlg, &QDialog::accept);
        connect(bb, &QDialogButtonBox::rejected, dlg, &QDialog::reject);

        if (dlg->exec() != QDialog::Accepted) { dlg->deleteLater(); return; }

        /* ── Avvia traduzione ── */
        m_translateSrcLang = cmbSrc->currentText();
        m_translateDstLang = cmbDst->currentText();
        QString selModel   = cmbMdl->currentText();
        dlg->deleteLater();

        /* Salva modello corrente e passa al modello di traduzione */
        m_preTranslateModel = m_ai->model();
        m_ai->setBackend(m_ai->backend(), m_ai->host(), m_ai->port(), selModel);

        /* Prompt fedele */
        QString prompt;
        if (m_translateSrcLang == "Auto-rileva")
            prompt = QString("Traducimi il seguente testo nella lingua %1. "
                             "Mantieni il significato originale nel modo pi\xc3\xb9 fedele possibile. "
                             "Rispondi SOLO con la traduzione, senza commenti aggiuntivi.\n\n"
                             "Testo:\n%2").arg(m_translateDstLang).arg(inputText);
        else
            prompt = QString("Traducimi il seguente testo da %1 a %2. "
                             "Mantieni il significato originale nel modo pi\xc3\xb9 fedele possibile. "
                             "Rispondi SOLO con la traduzione, senza commenti aggiuntivi.\n\n"
                             "Testo:\n%3")
                     .arg(m_translateSrcLang).arg(m_translateDstLang).arg(inputText);

        const QString sys =
            "Sei un traduttore professionale. "
            "Rispondi SEMPRE e SOLO con la traduzione richiesta, senza spiegazioni.";

        m_log->clear();
        m_log->append(QString("\xf0\x9f\x8c\x90  Traduzione  <b>%1</b> \xe2\x86\x92 <b>%2</b>"
                              "  [modello: %3]\n")
                      .arg(m_translateSrcLang, m_translateDstLang, selModel));
        m_log->append(QString(43, QChar(0x2500)));

        m_taskOriginal  = inputText;
        m_translateBuf.clear();
        m_pendingMode = OpMode::Idle;  /* traduzione pura: nessuna pipeline dopo */
        m_opMode      = OpMode::Translating;

        _setRunBusy(true);
        m_btnTranslate->setEnabled(false);
        m_waitLbl->setVisible(true);

        m_ai->chat(sys, prompt);
    });

    /* ── Lettore Documenti (PDF, Excel, ODS, testo) ── */
    connect(m_btnDoc, &QPushButton::clicked, this, [this]{
        static const QString filter =
            "Tutti i documenti supportati "
            "(*.txt *.md *.csv *.json *.py *.cpp *.c *.h *.html *.xml *.rst *.log "
            "*.pdf *.xls *.xlsx *.ods *.ots *.fods);;"
            "Testo (*.txt *.md *.csv *.json *.py *.cpp *.c *.h *.html *.xml *.rst *.log);;"
            "PDF (*.pdf);;"
            "Excel / Foglio di calcolo (*.xls *.xlsx *.ods *.ots *.fods);;"
            "Tutti (*)";
        const QString fp = QFileDialog::getOpenFileName(
            this, "Allega documento", QString(), filter);
        if (!fp.isEmpty()) loadDroppedFile(fp);
    });

    /* ── Analizzatore Immagini ── */
    connect(m_btnImg, &QPushButton::clicked, this, [this]{
        const QString fp = QFileDialog::getOpenFileName(
            this, "Allega immagine", QString(),
            "Immagini (*.png *.jpg *.jpeg *.gif *.webp);;Tutti (*)");
        if (!fp.isEmpty()) loadDroppedFile(fp);
    });

    /* ── Drag & Drop file su m_input ─────────────────────────────
       Accetta qualsiasi file trascinato sulla casella di testo.
       Dispatching per tipo: PDF → pdftotext, Excel/ODS → ssconvert,
       audio → whisper (voce) / aubionotes (musica), immagini → vision.
       ──────────────────────────────────────────────────────────── */
    m_input->setAcceptDrops(true);
    {
        struct DropFilter : public QObject {
            AgentiPage* page;
            DropFilter(AgentiPage* p, QObject* par) : QObject(par), page(p) {}
            bool eventFilter(QObject* obj, QEvent* ev) override {
                if (ev->type() == QEvent::DragEnter) {
                    auto* de = static_cast<QDragEnterEvent*>(ev);
                    if (de->mimeData()->hasUrls()) {
                        de->acceptProposedAction();
                        /* Evidenzia l'area con un bordo tratteggiato verde */
                        static_cast<QWidget*>(obj)->setStyleSheet(
                            "QTextEdit { border:2px dashed #4ade80; background:#0e2318; }");
                        return true;
                    }
                }
                if (ev->type() == QEvent::DragLeave) {
                    static_cast<QWidget*>(obj)->setStyleSheet("");
                    return true;
                }
                if (ev->type() == QEvent::Drop) {
                    auto* de = static_cast<QDropEvent*>(ev);
                    static_cast<QWidget*>(obj)->setStyleSheet("");
                    if (de->mimeData()->hasUrls()) {
                        de->acceptProposedAction();
                        for (const QUrl& url : de->mimeData()->urls()) {
                            const QString path = url.toLocalFile();
                            if (!path.isEmpty()) { page->loadDroppedFile(path); break; }
                        }
                        return true;
                    }
                }
                return QObject::eventFilter(obj, ev);
            }
        };
        m_input->installEventFilter(new DropFilter(this, this));
    }

    /* Auto-assegna */
    connect(m_btnAuto, &QPushButton::clicked, this, &AgentiPage::autoAssignRoles);

    /* Config dialog */
    connect(m_btnCfg, &QPushButton::clicked, this, [this]{
        m_cfgDlg->show();
        m_cfgDlg->raise();
        m_cfgDlg->activateWindow();
    });

    /* Numero agenti (dal dialog) → aggiorna m_maxShots */
    connect(m_cfgDlg->numAgentsSpinBox(), QOverload<int>::of(&QSpinBox::valueChanged),
            this, [this](int v){ m_maxShots = v; });

    /* Preset categoria → applica ruoli nel dialog e mostra suggerimento */
    static const char* presetLabels[7] = {
        "\xf0\x9f\x93\x90  Preset Matematica applicato \xe2\x80\x94 puoi modificare i ruoli.",
        "\xf0\x9f\x92\xbb  Preset Informatica applicato \xe2\x80\x94 puoi modificare i ruoli.",
        "\xf0\x9f\x94\x90  Preset Sicurezza applicato \xe2\x80\x94 puoi modificare i ruoli.",
        "\xe2\x9a\x9b\xef\xb8\x8f  Preset Fisica applicato \xe2\x80\x94 puoi modificare i ruoli.",
        "\xf0\x9f\xa7\xaa  Preset Chimica applicato \xe2\x80\x94 puoi modificare i ruoli.",
        "\xf0\x9f\x8c\x90  Preset Lingue applicato \xe2\x80\x94 puoi modificare i ruoli.",
        "\xf0\x9f\x8c\x8d  Preset Generico applicato \xe2\x80\x94 puoi modificare i ruoli.",
    };
    connect(m_cmbMode, QOverload<int>::of(&QComboBox::currentIndexChanged),
            this, [this](int idx){
        if (idx < 3) { m_autoLbl->setVisible(false); return; }
        m_cfgDlg->applyPreset(idx);
        static const char* lbls[] = {
            "\xf0\x9f\x93\x90  Preset Matematica applicato.",
            "\xf0\x9f\x92\xbb  Preset Informatica applicato.",
            "\xf0\x9f\x94\x90  Preset Sicurezza applicato.",
            "\xe2\x9a\x9b\xef\xb8\x8f  Preset Fisica applicato.",
            "\xf0\x9f\xa7\xaa  Preset Chimica applicato.",
            "\xf0\x9f\x8c\x90  Preset Lingue applicato.",
            "\xf0\x9f\x8c\x8d  Preset Generico applicato.",
        };
        m_autoLbl->setText(QString::fromUtf8(lbls[idx - 3])
                           + "  \xe2\x80\x94  Puoi modificarli in \xe2\x9a\x99\xef\xb8\x8f Configura Agenti.");
        m_autoLbl->setVisible(true);
    });

    /* Modelli matematici: pre-seleziona reasoning model */
    connect(m_cmbMode, QOverload<int>::of(&QComboBox::currentIndexChanged),
            this, [this](int idx) {
        if (idx != 2) return;
        static const QStringList mathKw  = {"r1","math","reason","think","qwq","deepseek-r"};
        static const QStringList coderKw = {"coder","code","starcoder","codellama","qwen2.5-coder"};
        static const QStringList largeKw = {"qwen3","llama3","gemma","mistral","phi"};

        auto bestMatch = [&](int i) -> int {
            QComboBox* cb = m_cfgDlg->modelCombo(i);
            for (int p = 0; p < cb->count(); p++) {
                QString n = cb->itemText(p).toLower();
                for (auto& kw : mathKw) if (n.contains(kw)) return p;
            }
            for (int p = 0; p < cb->count(); p++) {
                QString n = cb->itemText(p).toLower();
                for (auto& kw : coderKw) if (n.contains(kw)) return p;
            }
            for (int p = 0; p < cb->count(); p++) {
                QString n = cb->itemText(p).toLower();
                for (auto& kw : largeKw) if (n.contains(kw)) return p;
            }
            return -1;
        };
        int assigned = 0;
        for (int i = 0; i < MAX_AGENTS; i++) {
            if (!m_cfgDlg->enabledChk(i)->isChecked()) continue;
            int best = bestMatch(i);
            if (best >= 0) { m_cfgDlg->modelCombo(i)->setCurrentIndex(best); assigned++; }
        }
        m_autoLbl->setText(assigned > 0
            ? "\xf0\x9f\xa7\xae  Modelli reasoning/coder pre-selezionati per Matematico Teorico."
            : "\xf0\x9f\x92\xa1  Consigliato: modelli reasoning (deepseek-r1, qwen3, qwq).");
        m_autoLbl->setVisible(true);
    });

    /* AI interrotta: reset UI */
    connect(m_ai, &AiClient::aborted, this, [this]{
        m_waitLbl->setVisible(false);

        /* Rimuove il contenuto parziale dello streaming (testo grezzo non ancora
           convertito in bolla) che va da m_agentBlockStart alla fine del documento.
           Senza questo cleanup, il testo grezzo rimane visibile e la formattazione
           appare "sballata" quando si torna sulla scheda dopo un cambio tab. */
        if (m_agentBlockStart > 0) {
            QTextCursor sel(m_log->document());
            sel.setPosition(m_agentBlockStart);
            sel.movePosition(QTextCursor::End, QTextCursor::KeepAnchor);
            if (!sel.selectedText().trimmed().isEmpty())
                sel.removeSelectedText();
            m_agentBlockStart = 0;
        }

        m_opMode       = OpMode::Idle;
        m_currentAgent = 0;
        m_agentOutputs.clear();
        m_byzStep      = 0;
        _setRunBusy(false);
        emit pipelineStatus(0, "\xe2\x9c\x8b  Interrotto");
        for (int i = 0; i < MAX_AGENTS; i++)
            m_cfgDlg->enabledChk(i)->setStyleSheet("");
        m_log->moveCursor(QTextCursor::End);
        m_log->append("\n\xe2\x9c\x8b  Pipeline interrotta.");
    });

    /* ── STT Trascrivi Voce — registra + whisper.cpp (zero Python) ──
       Il pulsante funge anche da Stop: clic durante registrazione = cancella.
       Stati: Idle → Recording → Transcribing → Idle
       Se il modello manca viene scaricato automaticamente prima di avviare. ── */
    connect(m_btnVoice, &QPushButton::clicked, this, [this]{
        /* ── Stop durante registrazione ── */
        if (m_sttState == SttState::Recording) {
            if (m_recProc) { m_recProc->kill(); m_recProc->waitForFinished(300); }
            m_sttState = SttState::Idle;
            m_btnVoice->setText("\xf0\x9f\x8e\xa4 Trascrivi voce");
            m_btnVoice->setProperty("danger","false");
            P::repolish(m_btnVoice);
            m_btnVoice->setEnabled(true);
            return;
        }
        /* ── Ignora clic mentre trascrive o scarica ── */
        if (m_sttState == SttState::Transcribing ||
            m_sttState == SttState::Downloading) return;

        /* ── Controlla binario ── */
        if (SttWhisper::whisperBin().isEmpty()) {
            m_log->append(
                "<p style='color:#e2e8f0;'>"
                "\xe2\x9a\xa0  <b>whisper-cli non trovato.</b> "
                "<a href=\"settings:trascrivi\" style=\"color:#93c5fd;\">"
                "Clicca qui</a> per aprire le Impostazioni &rarr; Trascrivi"
                " e installare il riconoscimento vocale."
                "</p>");
            return;
        }

        /* ── Modello assente: avvia download automatico ── */
        if (SttWhisper::whisperModel().isEmpty()) {
            downloadWhisperModel();
            return;
        }

        _sttStartRecording();
    });
}

void AgentiPage::_setRunBusy(bool busy)
{
    if (busy) {
        m_btnRun->setText("\xe2\x8f\xb9 Stop");
        m_btnRun->setProperty("danger", true);
    } else {
        m_btnRun->setText(m_modePipeline
            ? "\xe2\x96\xb6  Avvia"
            : "\xf0\x9f\x92\xac CHAT con RAG");
        m_btnRun->setProperty("danger", false);
    }
    m_btnRun->setEnabled(true);
    P::repolish(m_btnRun);
}

