#include "agenti_page.h"
#include "agents_config_dialog.h"
#include "../prismalux_paths.h"
#include "../widgets/tts_speak.h"
#include "../widgets/stt_whisper.h"
#include "../widgets/chart_widget.h"
#include "../widgets/formula_parser.h"
#include "../theme_manager.h"
#include <QApplication>
namespace P = PrismaluxPaths;

/* ══════════════════════════════════════════════════════════════
   BubClr — palette bubble che segue il tema attivo
   Aggiungere qui nuovi temi se necessario; di default: dark.
   ══════════════════════════════════════════════════════════════ */
namespace {
struct BubClr {
    /* User bubble */
    const char* uBg;  const char* uBdr; const char* uHdr;
    const char* uTxt; const char* uBtn; const char* uBtnB;
    /* Agent bubble */
    const char* aBg;  const char* aBdr; const char* aHr;
    const char* aHdr; const char* aTxt;
    const char* aBtn; const char* aBtnB; const char* aBtnC;
    /* Local / math bubble */
    const char* lBg;  const char* lBdr; const char* lHr;
    const char* lHdr; const char* lTxt; const char* lRes;
    const char* lBtn; const char* lBtnB; const char* lBtnC;
    /* Tool strip */
    const char* tBgOk; const char* tBdOk;
    const char* tBgEr; const char* tBdEr;
    const char* tTxt;  const char* tStOk; const char* tStEr;
    /* Controller bubble */
    const char* cBgOk; const char* cBdOk; const char* cHdOk;
    const char* cBgWn; const char* cBdWn; const char* cHdWn;
    const char* cBgEr; const char* cBdEr; const char* cHdEr;
};

/* ── Preset scuro (default — tutti i dark_*) ── */
static const BubClr kDark = {
    "#162544","#1d4ed8","#93c5fd","#e2e8f0","#1e3a5f","#1d4ed8",
    "#0e1624","#1e2d47","#2d3a54","#a78bfa","#e2e8f0","#1a2641","#4c3a7a","#c4b5fd",
    "#0e1a12","#166534","#166534","#4ade80","#e2e8f0","#86efac","#0e2318","#166534","#86efac",
    "#0b1a10","#166534","#1a0a0a","#7f1d1d","#8b949e","#4ade80","#f87171",
    "#0b1a10","#166534","#4ade80","#1a1400","#854d0e","#facc15","#1a0a0a","#7f1d1d","#f87171"
};

/* ── Preset chiaro (light_*, pink) ── */
static const BubClr kLight = {
    "#dbeafe","#3b82f6","#1d4ed8","#1e293b","#bfdbfe","#93c5fd",
    "#f1f5f9","#94a3b8","#e2e8f0","#7c3aed","#1e293b","#ede9fe","#a78bfa","#7c3aed",
    "#dcfce7","#4ade80","#86efac","#15803d","#1e293b","#15803d","#bbf7d0","#4ade80","#15803d",
    "#f0fdf4","#4ade80","#fef2f2","#f87171","#374151","#15803d","#dc2626",
    "#f0fdf4","#4ade80","#15803d","#fffbeb","#fcd34d","#92400e","#fef2f2","#fca5a5","#991b1b"
};

static const BubClr& bc() {
    const QColor bg = qApp->palette().color(QPalette::Window);
    const int lum = (bg.red()*299 + bg.green()*587 + bg.blue()*114) / 1000;
    return lum > 128 ? kLight : kDark;
}
} // namespace
#include <QTime>
#include <QElapsedTimer>
#include <QKeyEvent>
#include <cmath>
#include <cstdlib>
#include <QRegularExpression>
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
#include <QDir>
#include <QScrollArea>
#include <QScrollBar>
#include <QMessageBox>
#include <algorithm>
#include <QFileDialog>
#include <QTextStream>
#include <QImage>
#include <QBuffer>
#include <QSet>
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

/* Forward declaration — _sanitize_prompt è definita nella sezione helper
   ma usata anche in setupUI() (allegato documento). */
static QString _sanitize_prompt(const QString& raw);

/* ══════════════════════════════════════════════════════════════
   Costruttore
   ══════════════════════════════════════════════════════════════ */
AgentiPage::AgentiPage(AiClient* ai, QWidget* parent)
    : QWidget(parent), m_ai(ai)
{
    m_namAuto = new QNetworkAccessManager(this);

    /* Dialog configurazione agenti — non-modal, creato una volta */
    m_cfgDlg = new AgentsConfigDialog(this);

    setupUI();

    connect(m_ai, &AiClient::token,       this, &AgentiPage::onToken);
    connect(m_ai, &AiClient::finished,    this, &AgentiPage::onFinished);
    connect(m_ai, &AiClient::error,       this, &AgentiPage::onError);
    connect(m_ai, &AiClient::modelsReady, this, &AgentiPage::onModelsReady);

    m_ai->fetchModels();
}

/* ══════════════════════════════════════════════════════════════
   setupUI — layout stile ChatGPT
   ┌─ toolbar ──────────────────────────────────────────────────┐
   │ 🤖 Agenti AI  [Modalità ▼] [Agenti: ⬆] [🤖 Auto] [⚙️ Cfg] │
   ├─ log ──────────────────────────────────────────────────────┤
   │  output agenti (flex, grande)                              │
   ├─ progress bar ──────────────────────────────────────────────│
   ├─ input ─────────────────────────────────────────────────── │
   │  [task...] [⚡ Singolo] [▶ Avvia] [■ Stop] [🫀 RAM]        │
   └────────────────────────────────────────────────────────────┘
   ══════════════════════════════════════════════════════════════ */
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
        if (m_ttsProc) { m_ttsProc->kill(); m_ttsProc->waitForFinished(300); }
#ifndef Q_OS_WIN
        QProcess::startDetached("pkill", {"aplay"});
        QProcess::startDetached("pkill", {"piper"});
#endif
        m_btnTtsStop->setVisible(false);
    });

    toolLay->addStretch(1);

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

    /* Quando l'utente sceglie un modello diverso, lo applica subito all'AI client */
    connect(m_cmbLLM, QOverload<int>::of(&QComboBox::currentIndexChanged),
            this, [this](int idx){
        if (idx < 0 || !m_cmbLLM) return;
        const QString mdl = m_cmbLLM->currentText();
        if (mdl.isEmpty() || mdl == "(caricamento...)") return;
        m_ai->setBackend(m_ai->backend(), m_ai->host(), m_ai->port(), mdl);
    });

    /* Modalità */
    auto* modeLbl = new QLabel("Modalit\xc3\xa0:", toolbar);
    modeLbl->setObjectName("cardDesc");
    m_cmbMode = new QComboBox(toolbar);
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
    toolLay->addWidget(modeLbl);
    toolLay->addWidget(m_cmbMode);

    /* Pulsante ↩ Torna indietro (undo cancellazione bolle) */
    auto* btnUndo = new QPushButton("\xe2\x86\xa9  Torna indietro", toolbar);
    btnUndo->setObjectName("actionBtn");
    btnUndo->setToolTip("Annulla l'ultima eliminazione di un messaggio (Ctrl+Z)");
    toolLay->addWidget(btnUndo);
    connect(btnUndo, &QPushButton::clicked, this, [this]{
        if (m_undoHtmlStack.isEmpty()) {
            QToolTip::showText(QCursor::pos(),
                "\xe2\x84\xb9\xef\xb8\x8f  Nessuna operazione da annullare.", nullptr, {}, 2000);
            return;
        }
        m_log->setHtml(m_undoHtmlStack.pop());
        m_log->moveCursor(QTextCursor::End);
    });

    /* Pulsante auto-assegna */
    m_btnAuto = new QPushButton("\xf0\x9f\xaa\x84 Auto-assegna ruoli", toolbar);
    m_btnAuto->setObjectName("actionBtn");
    m_btnAuto->setToolTip("Il modello pi\xc3\xb9 grande assegna ruoli e modelli automaticamente");
    toolLay->addWidget(m_btnAuto);

    /* Pulsante config agenti — apre finestra separata */
    m_btnCfg = new QPushButton("\xe2\x9a\x99\xef\xb8\x8f  Configura Agenti", toolbar);
    m_btnCfg->setObjectName("actionBtn");
    m_btnCfg->setToolTip("Apri la finestra di configurazione agenti\n(ruolo, modello, contesto RAG per ciascuno)");
    toolLay->addWidget(m_btnCfg);

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
                "L'operazione pu\xc3\xb2 essere annullata con il pulsante \xe2\x86\xa9 nella toolbar.");
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
        if (c2 > 0) {
            text = QString::fromUtf8(QByteArray::fromBase64(
                s.mid(c2 + 1).toLatin1(), QByteArray::Base64UrlEncoding));
        } else {
            if (!m_bubbleTexts.contains(idx)) return;
            text = m_bubbleTexts.value(idx);
        }

        if (s.startsWith("edit:")) {
            /* Copia il testo dell'agente nel campo input per modifica e reinvio */
            m_input->setPlainText(text.trimmed());
            m_input->setFocus();
            m_input->moveCursor(QTextCursor::End);
            QToolTip::showText(QCursor::pos(),
                "\xe2\x9c\x8f\xef\xb8\x8f  Testo copiato nell'input \xe2\x80\x94 modifica e premi Avvia",
                nullptr, {}, 3000);
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
    inputGrid->setColumnStretch(0, 1);  /* testo si espande */
    inputGrid->setColumnStretch(1, 0);  /* col 1: Avvia / Stop */
    inputGrid->setColumnStretch(2, 0);  /* col 2: Singolo / Voce */
    inputGrid->setColumnStretch(3, 0);  /* col 3: Simboli / Traduci */
    inputGrid->setColumnStretch(4, 0);  /* col 4: Documento / Immagine */

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

    /* Colonna 1 */
    m_btnRun = new QPushButton("\xe2\x96\xb6  Avvia", inputArea);
    m_btnRun->setObjectName("actionBtn");
    m_btnRun->setToolTip("Avvia la modalit\xc3\xa0 selezionata (Enter)");
    tagExec(m_btnRun, "\xe2\x96\xb6", "Avvia");

    m_btnStop = new QPushButton("\xe2\x8f\xb9 Stop", inputArea);
    m_btnStop->setObjectName("actionBtn");
    m_btnStop->setProperty("danger", true);
    m_btnStop->setEnabled(false);
    m_btnStop->setToolTip("Interrompi l'elaborazione corrente");
    tagExec(m_btnStop, "\xe2\x8f\xb9", "Stop");

    /* Colonna 2 */
    auto* btnQuick = new QPushButton("\xe2\x9a\xa1 Singolo", inputArea);
    btnQuick->setObjectName("actionBtn");
    btnQuick->setToolTip("Risposta immediata — 1 solo agente");
    tagExec(btnQuick, "\xe2\x9a\xa1", "Singolo");

    m_btnVoice = new QPushButton("\xf0\x9f\x8e\xa4 Voce", inputArea);
    m_btnVoice->setObjectName("actionBtn");
    m_btnVoice->setToolTip("Parla — trascrivi la voce nel campo di testo (whisper.cpp)");
    tagExec(m_btnVoice, "\xf0\x9f\x8e\xa4", "Voce");

    /* Colonna 3 */
    auto* btnSymbols = new QPushButton("\xce\xa9  Simboli", inputArea);
    btnSymbols->setObjectName("actionBtn");
    btnSymbols->setToolTip("Inserisci caratteri speciali: matematica, greco, lingue");

    m_btnTranslate = new QPushButton("\xf0\x9f\x8c\x90  Traduci", inputArea);
    m_btnTranslate->setObjectName("actionBtn");
    m_btnTranslate->setToolTip("Traduci il testo selezionando lingue e modello AI");
    tagExec(m_btnTranslate, "\xf0\x9f\x8c\x90", "Traduci");

    inputGrid->addWidget(m_btnRun,       0, 1);
    inputGrid->addWidget(btnQuick,       0, 2);
    inputGrid->addWidget(btnSymbols,     0, 3);
    inputGrid->addWidget(m_btnStop,      1, 1);
    inputGrid->addWidget(m_btnVoice,     1, 2);
    inputGrid->addWidget(m_btnTranslate, 1, 3);

    /* Colonna 4: Documento / Immagine (spostati dalla toolbar) */
    m_btnDoc = new QPushButton("\xf0\x9f\x93\x8e  Documenti", inputArea);
    m_btnDoc->setObjectName("actionBtn");
    m_btnDoc->setToolTip("Allega documento (.txt, .md, .csv, .json, .py, .cpp, .h, .pdf...)");
    tagExec(m_btnDoc, "\xf0\x9f\x93\x8e", "Documenti");
    m_btnImg = new QPushButton("\xf0\x9f\x96\xbc  Immagini", inputArea);
    m_btnImg->setObjectName("actionBtn");
    m_btnImg->setToolTip("Allega immagine per vision models (.png, .jpg, .jpeg, .gif, .webp)");
    tagExec(m_btnImg, "\xf0\x9f\x96\xbc", "Immagini");
    inputGrid->addWidget(m_btnDoc, 0, 4);
    inputGrid->addWidget(m_btnImg, 1, 4);

    lay->addWidget(inputArea);

    /* ── Footer suggerimenti ── */
    {
        auto* hints = new QLabel(
            "\xe2\x8c\xa8  "
            "<b>Shift+Invio</b> = a capo"
            " &nbsp;&nbsp;\xc2\xb7&nbsp;&nbsp; "
            "\xf0\x9f\x93\x88  Grafico cartesiano: es. "
            "<i>Grafico di sin(x) per x da -3 a 3</i>"
            " &nbsp;&nbsp;\xc2\xb7&nbsp;&nbsp; "
            "\xf0\x9f\x96\xb1  Click destro sul grafico per salvare l'immagine",
            this);
        hints->setObjectName("footerHints");
        hints->setAlignment(Qt::AlignLeft | Qt::AlignVCenter);
        hints->setWordWrap(false);
        hints->setTextFormat(Qt::RichText);
        lay->addWidget(hints);
    }

    /* ── Connessioni ── */

    /* Avvio pipeline */
    connect(m_btnRun, &QPushButton::clicked, this, [this]{
        int mode = m_cmbMode->currentIndex();
        if      (mode == 10) runConsiglioScientifico();
        else if (mode == 0 || (mode >= 3 && mode < 10)) runPipeline();
        else if (mode == 1) runByzantine();
        else if (mode == 2) runMathTheory();
    });
    /* Invio = Avvia  |  Shift+Invio = a capo nel testo */
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
    connect(m_btnStop, &QPushButton::clicked, m_ai, &AiClient::abort);

    /* Singolo (1 agente) */
    connect(btnQuick, &QPushButton::clicked, this, [this]{
        m_cfgDlg->numAgentsSpinBox()->setValue(1);
        m_maxShots = 1;
        m_btnRun->click();
    });

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

        m_btnRun->setEnabled(false);
        m_btnTranslate->setEnabled(false);
        m_btnStop->setEnabled(true);
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
        m_opMode       = OpMode::Idle;
        m_currentAgent = 0;
        m_agentOutputs.clear();
        m_byzStep      = 0;
        m_btnRun->setEnabled(true);
        m_btnStop->setEnabled(false);
        emit pipelineStatus(0, "\xe2\x9c\x8b  Interrotto");
        for (int i = 0; i < MAX_AGENTS; i++)
            m_cfgDlg->enabledChk(i)->setStyleSheet("");
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

/* ══════════════════════════════════════════════════════════════
   _sttStartRecording — avvia registrazione + trascrizione
   ══════════════════════════════════════════════════════════════ */
void AgentiPage::_sttStartRecording()
{
    const QString wavPath = QDir::tempPath() + "/prisma_stt_in.wav";
    QFile::remove(wavPath);

    m_sttState = SttState::Recording;
    m_btnVoice->setText("\xf0\x9f\x94\xb4 Registrando... (click per fermare)");
    m_btnVoice->setProperty("danger","true");
    P::repolish(m_btnVoice);

    m_recProc = new QProcess(this);
#ifdef Q_OS_WIN
    /* Windows: sox rec (https://sox.sourceforge.net) */
    const QString recBin = QStandardPaths::findExecutable("rec");
    const QString soxBin = QStandardPaths::findExecutable("sox");
    if (!recBin.isEmpty()) {
        m_recProc->start("rec",
            {"-r","16000","-c","1","-b","16", wavPath, "trim","0","6"});
    } else if (!soxBin.isEmpty()) {
        m_recProc->start("sox",
            {"-t","waveaudio","default","-r","16000","-c","1","-b","16",
             wavPath, "trim","0","6"});
    } else {
        m_sttState = SttState::Idle;
        m_btnVoice->setText("\xf0\x9f\x8e\xa4 Trascrivi voce");
        m_btnVoice->setProperty("danger","false");
        P::repolish(m_btnVoice);
        m_log->append(
            "\xe2\x9a\xa0  <b>Registrazione audio non disponibile.</b><br>"
            "Su Windows installa <code>sox</code> (sox.sourceforge.net) "
            "per abilitare la trascrizione vocale.");
        m_recProc->deleteLater(); m_recProc = nullptr;
        return;
    }
#else
    m_recProc->start("arecord",
        {"-d", "6", "-r", "16000", "-c", "1", "-f", "S16_LE", "-q", wavPath});
#endif

    /* Countdown nel testo del pulsante */
    auto* tick = new QTimer(this);
    tick->setProperty("secs", 6);
    connect(tick, &QTimer::timeout, this, [this, tick]{
        if (m_sttState != SttState::Recording) { tick->deleteLater(); return; }
        int s = tick->property("secs").toInt() - 1;
        tick->setProperty("secs", s);
        if (s > 0)
            m_btnVoice->setText(
                QString("\xf0\x9f\x94\xb4 Registrando... %1s (click per fermare)").arg(s));
    });
    tick->start(1000);

    /* Dopo 6.5s ferma la registrazione e avvia la trascrizione */
    QTimer::singleShot(6500, this, [this, wavPath, tick]{
        tick->stop(); tick->deleteLater();
        if (m_sttState != SttState::Recording) return;  // utente ha fermato prima
        if (m_recProc) { m_recProc->terminate(); m_recProc->waitForFinished(1000);
                         m_recProc->deleteLater(); m_recProc = nullptr; }

        if (!QFileInfo::exists(wavPath)) {
            m_sttState = SttState::Idle;
            m_btnVoice->setText("\xf0\x9f\x8e\xa4 Trascrivi voce");
            m_btnVoice->setProperty("danger","false");
            P::repolish(m_btnVoice);
            m_log->append("\xe2\x9a\xa0  Registrazione fallita (arecord non disponibile?)");
            return;
        }

        m_sttState = SttState::Transcribing;
        m_btnVoice->setText("\xe2\x8c\x9b Trascrivendo...");
        m_btnVoice->setProperty("danger","false");
        P::repolish(m_btnVoice);
        m_btnVoice->setEnabled(false);

        m_sttProc = SttWhisper::transcribe(wavPath, "it", this,
            [this](const QString& text, bool ok) {
                m_sttState = SttState::Idle;
                m_sttProc  = nullptr;
                m_btnVoice->setText("\xf0\x9f\x8e\xa4 Trascrivi voce");
                m_btnVoice->setEnabled(true);

                if (ok && !text.isEmpty()) {
                    const QString cur = m_input->toPlainText();
                    m_input->setPlainText(cur.isEmpty() ? text : cur + " " + text);
                    m_input->setFocus();
                } else {
                    m_log->append(
                        "\xe2\x9a\xa0  Trascrizione fallita o audio vuoto.<br>"
                        + QString(text).replace("\n","<br>"));
                }
            });
    });
}

/* ══════════════════════════════════════════════════════════════
   _ttsPlay — avvia TTS tracciato con feedback visivo di caricamento.
   Mostra "🔊 Avvio lettura..." nella toolbar durante il caricamento
   dei moduli vocali (Piper TTS → espeak-ng → spd-say).
   ══════════════════════════════════════════════════════════════ */
void AgentiPage::_ttsPlay(const QString& tts)
{
    if (tts.trimmed().isEmpty()) return;

    /* Ferma TTS precedente se attivo */
    if (m_ttsProc) {
        m_ttsProc->kill();
        m_ttsProc->waitForFinished(300);
        if (m_ttsProc) { m_ttsProc->deleteLater(); m_ttsProc = nullptr; }
    }
#ifndef Q_OS_WIN
    QProcess::startDetached("pkill", {"aplay"});
    QProcess::startDetached("pkill", {"espeak-ng"});
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
        if (m_ttsProc)    { m_ttsProc->deleteLater(); m_ttsProc = nullptr; }
    });

#ifdef Q_OS_WIN
    /* Windows: voci native SAPI via PowerShell */
    if (TtsSpeak::writeTtsTemp(tts)) {
        m_ttsProc->start("powershell",
            {"-NoProfile", "-WindowStyle", "Hidden",
             "-Command", TtsSpeak::sapiCommand()});
    } else {
        if (m_waitLbl) m_waitLbl->setVisible(false);
        m_ttsProc->deleteLater(); m_ttsProc = nullptr;
        return;
    }
#else
    /* Linux: Piper → espeak-ng → spd-say */
    const QString bin  = TtsSpeak::piperBin();
    const QString onnx = TtsSpeak::activeOnnx();
    if (!bin.isEmpty() && QFileInfo::exists(onnx) && TtsSpeak::writeTtsTemp(tts)) {
        const QString cmd =
            QString("\"%1\" --model \"%2\" --output_raw < \"%3\""
                    " | aplay -r 22050 -f S16_LE -t raw -q -")
            .arg(bin, onnx, TtsSpeak::ttsTempFile());
        m_ttsProc->start("bash", {"-c", cmd});
    } else if (!QProcess::startDetached("espeak-ng",{"-v","it+f3","--punct=none",tts})) {
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
    /* Nasconde "Avvio lettura..." non appena il processo parte */
    if (m_ttsProc) {
        QTimer::singleShot(400, this, [this]{
            if (m_waitLbl) m_waitLbl->setVisible(false);
        });
    }
}

/* ══════════════════════════════════════════════════════════════
   downloadWhisperModel — scarica ggml-small.bin in ~/.prismalux/whisper/models/
   ══════════════════════════════════════════════════════════════ */
void AgentiPage::downloadWhisperModel()
{
    const QString destDir  = QDir::homePath() + "/.prismalux/whisper/models";
    const QString destFile = destDir + "/ggml-small.bin";
    const QString url      =
        "https://huggingface.co/ggerganov/whisper.cpp/resolve/main/ggml-small.bin";

    QDir().mkpath(destDir);

    m_sttState = SttState::Downloading;
    m_btnVoice->setEnabled(false);
    m_btnVoice->setText("\xf0\x9f\x93\xa5 Download modello...");

    m_log->append(
        "\xf0\x9f\x93\xa5  <b>Download modello whisper (ggml-small, ~141 MB)...</b><br>"
        "Destinazione: <code>" + destFile + "</code>");

    /* Usa wget se disponibile, altrimenti curl */
    const bool hasWget = !QStandardPaths::findExecutable("wget").isEmpty();
    auto* dlProc = new QProcess(this);
    dlProc->setProcessChannelMode(QProcess::MergedChannels);

    if (hasWget) {
        dlProc->start("wget", {
            "--progress=dot:mega",
            "-O", destFile,
            url
        });
    } else {
        dlProc->start("curl", {
            "-L", "--progress-bar",
            "-o", destFile,
            url
        });
    }

    /* Mostra progresso leggendo stdout/stderr del downloader */
    connect(dlProc, &QProcess::readyReadStandardOutput, this, [this, dlProc]{
        const QString chunk = QString::fromLocal8Bit(dlProc->readAllStandardOutput());
        /* Mostra solo le righe con percentuale o MB per non intasare il log */
        for (const auto& line : chunk.split('\n')) {
            const QString t = line.trimmed();
            if (t.contains('%') || t.contains("MB") || t.contains("KB"))
                m_log->append("  " + t.toHtmlEscaped());
        }
    });

    connect(dlProc,
        QOverload<int,QProcess::ExitStatus>::of(&QProcess::finished),
        this, [this, dlProc, destFile](int code, QProcess::ExitStatus){
            dlProc->deleteLater();
            m_btnVoice->setEnabled(true);

            if (code == 0 && QFileInfo::exists(destFile) &&
                QFileInfo(destFile).size() > 10'000'000LL) {
                m_log->append(
                    "\xe2\x9c\x85  <b>Modello scaricato.</b> Avvio registrazione...");
                m_sttState = SttState::Idle;
                _sttStartRecording();
            } else {
                m_sttState = SttState::Idle;
                m_btnVoice->setText("\xf0\x9f\x8e\xa4 Trascrivi voce");
                /* File parziale → rimuovi per non confondere isAvailable() */
                if (QFileInfo::exists(destFile) &&
                    QFileInfo(destFile).size() < 10'000'000LL)
                    QFile::remove(destFile);
                m_log->append(
                    "\xe2\x9d\x8c  Download fallito (controlla la connessione).<br>"
                    "Puoi scaricarlo manualmente:<br>"
                    "<code>mkdir -p ~/.prismalux/whisper/models &amp;&amp; "
                    "wget -O ~/.prismalux/whisper/models/ggml-small.bin "
                    "https://huggingface.co/ggerganov/whisper.cpp/resolve/main/ggml-small.bin</code>");
            }
        });
}

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

    /* Auto-install moduli esterni comuni che l'AI genera ma che possono
       mancare nell'ambiente Python dell'utente (tipico su Windows senza venv).
       Il blocco viene iniettato solo se almeno uno dei moduli è importato. */
    static const QStringList kOptionalPkgs = {
        "psutil", "numpy", "pandas", "matplotlib", "scipy",
        "sklearn", "PIL", "requests", "sympy"
    };
    /* Mappa import-name → package-name (se diversi) */
    static const QMap<QString,QString> kPkgMap = {
        {"PIL", "Pillow"}, {"sklearn", "scikit-learn"}
    };

    QStringList toInstall;
    for (const QString& mod : kOptionalPkgs) {
        /* Cerca "import <mod>" oppure "from <mod>" nel codice */
        QRegularExpression reImp(
            QString(R"((?:^|\n)\s*(?:import\s+%1|from\s+%1\s))").arg(
                QRegularExpression::escape(mod)));
        if (reImp.match(out).hasMatch())
            toInstall.append(kPkgMap.value(mod, mod));
    }

    if (!toInstall.isEmpty()) {
        QString pkgList;
        for (const QString& pkg : toInstall)
            pkgList += QString("    \"%1\",\n").arg(pkg);

        QString preamble =
            "import subprocess, sys\n"
            "def _prisma_ensure(*pkgs):\n"
            "    import importlib\n"
            "    for p in pkgs:\n"
            "        name = p.replace('-','_').split('[')[0]\n"
            "        try: importlib.import_module(name)\n"
            "        except ImportError:\n"
            "            subprocess.check_call(\n"
            "                [sys.executable, '-m', 'pip', 'install', p, '-q'],\n"
            "                stdout=subprocess.DEVNULL, stderr=subprocess.DEVNULL)\n"
            "_prisma_ensure(\n"
            + pkgList +
            ")\n\n";

        out = preamble + out;
    }

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
    const QString script = QDir::tempPath() + "/prisma_pdf_extract.py";

    {
        QFile sf(script);
        if (!sf.open(QIODevice::WriteOnly | QIODevice::Text)) {
            m_log->append("\xe2\x9d\x8c  Impossibile creare script Python temporaneo.");
            return;
        }
        QTextStream ts(&sf);
        ts << "import sys\n"
           << "try:\n"
           << "    from pypdf import PdfReader\n"
           << "except ImportError:\n"
           << "    from PyPDF2 import PdfReader\n"
           << "r = PdfReader(sys.argv[1])\n"
           << "print(''.join(p.extract_text() or '' for p in r.pages))\n";
    }

    auto* proc = new QProcess(this);
    proc->start(python, {script, filePath});
    connect(proc, QOverload<int,QProcess::ExitStatus>::of(&QProcess::finished),
            this, [this, proc, script, onText](int code, QProcess::ExitStatus) {
        const QString text = QString::fromUtf8(proc->readAllStandardOutput()).trimmed();
        proc->deleteLater();
        QFile::remove(script);
        if (code == 0 && !text.isEmpty()) {
            onText(text);
        } else {
            m_log->append(
                "\xe2\x9a\xa0  Estrazione PDF fallita.<br>"
                "Installa <code>pypdf</code>: già incluso in "
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
    const QString script = QDir::tempPath() + "/prisma_xls_convert.py";

    {
        QFile sf(script);
        if (!sf.open(QIODevice::WriteOnly | QIODevice::Text)) {
            m_log->append("\xe2\x9d\x8c  Impossibile creare script Python temporaneo.");
            return;
        }
        QTextStream ts(&sf);
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

    auto* proc = new QProcess(this);
    proc->start(python, {script, filePath});
    connect(proc, QOverload<int,QProcess::ExitStatus>::of(&QProcess::finished),
            this, [this, proc, filePath, script, onText](int code, QProcess::ExitStatus) {
        const QString text = QString::fromUtf8(proc->readAllStandardOutput()).trimmed();
        proc->deleteLater();
        QFile::remove(script);
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

/* ══════════════════════════════════════════════════════════════
   onModelsReady — aggiorna combo modelli nel dialog
   ══════════════════════════════════════════════════════════════ */
void AgentiPage::onModelsReady(const QStringList& list) {
    /* Con llama.cpp (server) i modelli da configurare sono i file GGUF locali,
       non il singolo modello correntemente caricato nel server.
       L'utente sceglie quale GGUF assegnare ad ogni agente;
       la pipeline userà quello già caricato (o in futuro riavvierà il server). */
    /* ── Helper lambda: popola m_cmbLLM con la lista data ── */
    auto populateLLMCombo = [this](const QStringList& models) {
        if (!m_cmbLLM) return;
        /* Blocca il segnale per evitare cambio modello mentre si ricarica */
        m_cmbLLM->blockSignals(true);
        const QString prev = m_cmbLLM->currentText();
        m_cmbLLM->clear();
        for (const QString& m : models)
            m_cmbLLM->addItem(m);
        /* Ripristina selezione precedente; se non trovata usa il primo */
        const int idx = m_cmbLLM->findText(prev);
        m_cmbLLM->setCurrentIndex(idx >= 0 ? idx : 0);
        m_cmbLLM->blockSignals(false);
    };

    if (m_ai->backend() == AiClient::LlamaServer) {
        QStringList ggufNames;
        for (const QString& path : P::scanGgufFiles())
            ggufNames << QFileInfo(path).fileName();
        const QStringList effective = ggufNames.isEmpty() ? list : ggufNames;
        m_cfgDlg->setModels(effective);
        populateLLMCombo(effective);
        /* Con llama.cpp un solo modello può girare alla volta — auto-assign non ha senso */
        m_btnAuto->setEnabled(false);
        m_btnAuto->setToolTip(
            "Auto-assegna non disponibile con llama.cpp:\n"
            "un solo modello GGUF \xc3\xa8 caricato alla volta.\n"
            "Seleziona manualmente il modello per ogni agente.");
        return;
    }

    /* ── Nessun modello: Ollama non e' in esecuzione ── */
    if (list.isEmpty()) {
        /* Mostra placeholder nel combo invece di lasciarlo vuoto */
        if (m_cmbLLM) {
            m_cmbLLM->blockSignals(true);
            m_cmbLLM->clear();
            m_cmbLLM->addItem("\xe2\x9d\x8c  Ollama non attivo — avvia 'ollama serve'");
            m_cmbLLM->setEnabled(false);
            m_cmbLLM->blockSignals(false);
        }
        m_cfgDlg->setModels(QStringList{"\xe2\x9d\x8c  Ollama non attivo"});
        m_btnAuto->setEnabled(false);
        m_btnAuto->setToolTip(
            "Ollama non raggiungibile.\n"
            "Avvia Ollama con: ollama serve\n"
            "Poi riapri Prismalux o cambia backend.");
        return;
    }

    /* Se in precedenza era disabilitato per lista vuota, riabilitalo */
    if (m_cmbLLM) m_cmbLLM->setEnabled(true);

    m_cfgDlg->setModels(list);
    populateLLMCombo(list);

    /* ── Auto-assegna: sempre abilitato.
       Se VRAM benchmark mancante, cliccarlo apre direttamente le impostazioni Hardware. ── */
    const bool canAutoAssign = list.size() > 1;
    const bool hasVramProfile = QFileInfo::exists(P::vramProfilePath());
    m_btnAuto->setEnabled(true);
    if (!hasVramProfile) {
        m_btnAuto->setToolTip(
            "\xf0\x9f\x94\xac  Benchmark VRAM non ancora eseguito\n"
            "Clicca per aprire Hardware \xe2\x86\x92 VRAM Bench e avviare il benchmark.\n"
            "Le ottimizzazioni pre-registrate vengono usate nell'auto-assegnazione.");
    } else {
        m_btnAuto->setToolTip(canAutoAssign
            ? "Auto-assegna ruoli e modelli ottimali tramite orchestratore AI"
            : "Auto-assegna non disponibile: un solo modello presente.\n"
              "llama-server carica un modello alla volta — usa Ollama per la selezione automatica.");
    }
}

/* ══════════════════════════════════════════════════════════════
   autoAssignRoles
   ══════════════════════════════════════════════════════════════ */
void AgentiPage::autoAssignRoles() {
    QString task = m_input->toPlainText().trimmed();
    if (task.isEmpty()) {
        m_log->append("\xe2\x9a\xa0  Inserisci prima un task per l'auto-assegnazione."); return;
    }

    /* ── Benchmark VRAM mancante: apri direttamente le Impostazioni Hardware ── */
    if (!QFileInfo::exists(P::vramProfilePath())) {
        m_log->append(
            "\xf0\x9f\x94\xac  <b>Benchmark VRAM non trovato.</b><br>"
            "Apertura Impostazioni \xe2\x86\x92 Hardware \xe2\x86\x92 VRAM Bench&hellip;<br>"
            "Avvia il benchmark, poi torna qui e riprova.");
        emit requestOpenSettings("Hardware");
        return;
    }

    m_autoLbl->setText("\xf0\x9f\x94\x84  Recupero modelli disponibili...");
    m_autoLbl->setVisible(true);
    m_btnAuto->setEnabled(false);

    const bool isOllama = (m_ai->backend() == AiClient::Ollama);
    QString url = QString("http://%1:%2%3")
                  .arg(m_ai->host()).arg(m_ai->port())
                  .arg(isOllama ? "/api/tags" : "/v1/models");
    auto* reply = m_namAuto->get(QNetworkRequest(QUrl(url)));

    connect(reply, &QNetworkReply::finished, this, [this, reply, task, isOllama]{
        reply->deleteLater();
        m_modelInfos.clear();

        if (reply->error() == QNetworkReply::NoError) {
            QJsonDocument doc = QJsonDocument::fromJson(reply->readAll());
            if (isOllama) {
                for (auto v : doc.object()["models"].toArray()) {
                    QJsonObject obj = v.toObject();
                    m_modelInfos.append({ obj["name"].toString(),
                                          obj["size"].toVariant().toLongLong() });
                }
            } else {
                for (auto v : doc.object()["data"].toArray()) {
                    m_modelInfos.append({ v.toObject()["id"].toString(), 0 });
                }
            }
        }

        if (m_modelInfos.isEmpty()) {
            m_autoLbl->setText("\xe2\x9d\x8c  Nessun modello trovato sul server corrente.");
            m_btnAuto->setEnabled(true);
            return;
        }

        std::sort(m_modelInfos.begin(), m_modelInfos.end(),
                  [](const ModelInfo& a, const ModelInfo& b){ return a.size > b.size; });

        const QString biggest = m_modelInfos.first().name;
        const qint64  bigSize = m_modelInfos.first().size;

        QStringList modelNames;
        for (const auto& mi : m_modelInfos)
            modelNames << mi.name;

        auto roleList = AgentsConfigDialog::roles();
        QStringList roleNames;
        for (const auto& r : roleList)
            roleNames << r.name;

        m_autoLbl->setText(QString(
            "\xf0\x9f\xa4\x96  Orchestratore: \xef\xbc\x88%1, %2 GB\xef\xbc\x89 — assegnazione ruoli...")
            .arg(biggest)
            .arg(bigSize / 1'000'000'000.0, 0, 'f', 1));

        m_ai->setBackend(m_ai->backend(), m_ai->host(), m_ai->port(), biggest);

        m_opMode = OpMode::AutoAssign;
        m_autoBuffer.clear();

        QString sys =
            "Sei un orchestratore AI. Analizza il task e assegna i ruoli pi\xc3\xb9 efficaci "
            "agli agenti della pipeline. Rispondi SOLO con JSON valido, senza altro testo.";

        QString prompt = QString(
            "Task dell'utente: \"%1\"\n\n"
            "Ruoli disponibili: %2\n\n"
            "Modelli disponibili: %3\n\n"
            "Assegna da 2 a 6 agenti. Per ogni agente scegli ruolo e modello ottimali.\n"
            "Rispondi ESATTAMENTE con questo JSON (nessun testo prima o dopo):\n"
            "[\n"
            "  {\"agente\": 1, \"ruolo\": \"NomeRuolo\", \"modello\": \"nomemodello\"},\n"
            "  {\"agente\": 2, \"ruolo\": \"NomeRuolo\", \"modello\": \"nomemodello\"}\n"
            "]")
            .arg(task)
            .arg(roleNames.join(", "))
            .arg(modelNames.join(", "));

        m_ai->chat(sys, prompt);
    });
}

/* ══════════════════════════════════════════════════════════════
   parseAutoAssign
   ══════════════════════════════════════════════════════════════ */
void AgentiPage::parseAutoAssign(const QString& raw) {
    QString cleaned = raw;

    /* Rimuove <think>...</think> */
    {
        QRegularExpression re("<think>[\\s\\S]*?</think>",
                              QRegularExpression::CaseInsensitiveOption);
        cleaned.remove(re);
    }
    /* Estrae blocco ```json ... ``` */
    {
        QRegularExpression re("```(?:json)?\\s*([\\s\\S]*?)```");
        auto m = re.match(cleaned);
        if (m.hasMatch()) cleaned = m.captured(1).trimmed();
    }

    int start = cleaned.indexOf('[');
    int end   = cleaned.lastIndexOf(']');
    if (start < 0 || end < start) {
        m_autoLbl->setText("\xe2\x9d\x8c  Formato JSON non valido nella risposta dell'orchestratore.");
        m_btnAuto->setEnabled(true);
        return;
    }

    QJsonDocument doc = QJsonDocument::fromJson(cleaned.mid(start, end - start + 1).toUtf8());
    if (!doc.isArray()) {
        m_autoLbl->setText("\xe2\x9d\x8c  JSON non analizzabile.");
        m_btnAuto->setEnabled(true);
        return;
    }

    auto roleList = AgentsConfigDialog::roles();
    QJsonArray arr = doc.array();
    int assigned = 0;

    for (int i = 0; i < MAX_AGENTS; i++)
        m_cfgDlg->enabledChk(i)->setChecked(false);

    for (int i = 0; i < arr.size() && i < MAX_AGENTS; i++) {
        QJsonObject obj = arr[i].toObject();
        QString roleName  = obj["ruolo"].toString();
        QString modelName = obj["modello"].toString();

        int roleIdx = -1;
        for (int r = 0; r < roleList.size(); r++) {
            if (roleList[r].name.compare(roleName, Qt::CaseInsensitive) == 0) {
                roleIdx = r; break;
            }
        }
        if (roleIdx < 0) {
            for (int r = 0; r < roleList.size(); r++) {
                if (roleList[r].name.contains(roleName, Qt::CaseInsensitive) ||
                    roleName.contains(roleList[r].name, Qt::CaseInsensitive)) {
                    roleIdx = r; break;
                }
            }
        }
        if (roleIdx < 0) roleIdx = i % roleList.size();

        m_cfgDlg->enabledChk(i)->setChecked(true);
        m_cfgDlg->roleCombo(i)->setCurrentIndex(roleIdx);

        int mIdx = m_cfgDlg->modelCombo(i)->findText(modelName, Qt::MatchContains);
        if (mIdx >= 0) m_cfgDlg->modelCombo(i)->setCurrentIndex(mIdx);

        assigned++;
    }

    if (m_cfgDlg->modelCombo(0)->count() > 0)
        m_ai->setBackend(m_ai->backend(), m_ai->host(), m_ai->port(),
                         m_cfgDlg->modelCombo(0)->currentText());

    m_autoLbl->setText(QString("\xe2\x9c\x85  Assegnati %1 agenti — puoi modificarli in \xe2\x9a\x99\xef\xb8\x8f Configura Agenti.").arg(assigned));
    m_btnAuto->setEnabled(true);
}

/* ══════════════════════════════════════════════════════════════
   GUARDIA MATEMATICA LOCALE
   (invariata — parser recursive descent)
   ══════════════════════════════════════════════════════════════ */

static const char* _gp_ptr;
static bool        _gp_err;

static void   _gp_ws()   { while(*_gp_ptr==' '||*_gp_ptr=='\t') _gp_ptr++; }
static double _gp_expr();

static double _gp_prim() {
    _gp_ws();
    if (*_gp_ptr=='(') {
        _gp_ptr++; double v=_gp_expr(); _gp_ws();
        if (*_gp_ptr==')') _gp_ptr++;
        return v;
    }
    if (isalpha((unsigned char)*_gp_ptr)) {
        const char* s=_gp_ptr;
        while (isalpha((unsigned char)*_gp_ptr)||isdigit((unsigned char)*_gp_ptr)) _gp_ptr++;
        int len=(int)(_gp_ptr-s); char nm[32]={};
        for(int i=0;i<len&&i<31;i++) nm[i]=tolower((unsigned char)s[i]);
        _gp_ws();
        if (!strcmp(nm,"pi"))  return M_PI;
        if (!strcmp(nm,"phi")) return 1.6180339887498948;
        if (!strcmp(nm,"e") && *_gp_ptr!='(') return M_E;
        if (*_gp_ptr=='(') {
            _gp_ptr++;
            double a=_gp_expr(); _gp_ws();
            double b=a;
            if (*_gp_ptr==',') { _gp_ptr++; b=_gp_expr(); _gp_ws(); }
            if (*_gp_ptr==')') _gp_ptr++;
            if (!strcmp(nm,"sqrt")||!strcmp(nm,"radice")) return a>=0?std::sqrt(a):NAN;
            if (!strcmp(nm,"cbrt"))  return std::cbrt(a);
            if (!strcmp(nm,"abs"))   return std::fabs(a);
            if (!strcmp(nm,"floor")) return std::floor(a);
            if (!strcmp(nm,"ceil"))  return std::ceil(a);
            if (!strcmp(nm,"round")) return std::round(a);
            if (!strcmp(nm,"trunc")) return std::trunc(a);
            if (!strcmp(nm,"sign"))  return (double)((a>0)-(a<0));
            if (!strcmp(nm,"exp"))   return std::exp(a);
            if (!strcmp(nm,"sin"))   return std::sin(a);
            if (!strcmp(nm,"cos"))   return std::cos(a);
            if (!strcmp(nm,"tan"))   return std::tan(a);
            if (!strcmp(nm,"asin")||!strcmp(nm,"arcsin")) return std::asin(a);
            if (!strcmp(nm,"acos")||!strcmp(nm,"arccos")) return std::acos(a);
            if (!strcmp(nm,"atan")||!strcmp(nm,"arctan")) return std::atan(a);
            if (!strcmp(nm,"atan2")) return std::atan2(a,b);
            if (!strcmp(nm,"log")||!strcmp(nm,"ln"))   return std::log(a);
            if (!strcmp(nm,"log2"))  return std::log2(a);
            if (!strcmp(nm,"log10")||!strcmp(nm,"lg")) return std::log10(a);
            if (!strcmp(nm,"logb"))  return b!=1?std::log(a)/std::log(b):NAN;
            if (!strcmp(nm,"min"))   return std::min(a,b);
            if (!strcmp(nm,"max"))   return std::max(a,b);
            if (!strcmp(nm,"pow"))   return std::pow(a,b);
            if (!strcmp(nm,"gcd")||!strcmp(nm,"mcd")) {
                long long ia=(long long)std::fabs(a),ib=(long long)std::fabs(b);
                while(ib){long long t=ib;ib=ia%ib;ia=t;} return (double)ia;
            }
            if (!strcmp(nm,"lcm")||!strcmp(nm,"mcm")) {
                long long ia=(long long)std::fabs(a),ib=(long long)std::fabs(b),g=ia,tb=ib;
                while(tb){long long t=tb;tb=g%tb;g=t;} return ia?(double)(ia/g*ib):0;
            }
            return a;
        }
        _gp_err=true; return 0.0;
    }
    char *e; double v=strtod(_gp_ptr,&e);
    if (e!=_gp_ptr){_gp_ptr=e;return v;}
    return 0.0;
}
static double _gp_pow() {
    _gp_ws(); int neg=(*_gp_ptr=='-'); if(neg||*_gp_ptr=='+') _gp_ptr++;
    double v=_gp_prim(); _gp_ws();
    if (*_gp_ptr=='^'){_gp_ptr++;v=std::pow(v,_gp_pow());}
    /* '!' = fattoriale SOLO se non e' '!=' (operatore disuguaglianza) */
    if (*_gp_ptr=='!' && *(_gp_ptr+1)!='='){_gp_ptr++;long long n=(long long)std::fabs(v),f=1;
        for(long long i=2;i<=n&&i<=20;i++)f*=i;v=(double)f;}
    return neg?-v:v;
}
static double _gp_term() {
    double v=_gp_pow();
    while(true){_gp_ws();char op=*_gp_ptr;
        if(op=='*'||op=='/'||op=='%'||op==':'){_gp_ptr++;double r=_gp_pow();
            if(op=='*')v*=r;
            else if(op=='/'||op==':')v=(r?v/r:NAN);
            else v=(r?std::fmod(v,r):NAN);}
        else break;}
    return v;
}
static double _gp_expr() {
    double v=_gp_term();
    while(true){_gp_ws();
        if(*_gp_ptr=='+'){_gp_ptr++;v+=_gp_term();}
        else if(*_gp_ptr=='-'){_gp_ptr++;v-=_gp_term();}
        else break;}
    return v;
}
static QString _gp_fmt(double v) {
    if (std::isnan(v))  return "errore (dominio non valido)";
    if (std::isinf(v))  return v>0?"infinito":"-infinito";
    if (v==(long long)v && std::fabs(v)<1e15) return QString::number((long long)v);
    return QString::number(v,'g',10);
}
static bool _gp_try(const QByteArray& ba, double& out) {
    _gp_err=false; _gp_ptr=ba.constData();
    out=_gp_expr(); _gp_ws();
    return !_gp_err && !*_gp_ptr;
}
/* ── Anti-prompt-injection ────────────────────────────────────────────────
   Rimuove o neutralizza sequenze che potrebbero indurre il modello a ignorare
   le istruzioni di sistema (role hijack) o a leakare dati interni.
   Applicata su: task utente, contesto RAG, testo documento allegato.

   Pattern neutralizzati:
     - Format token ChatML / Llama2 / Mistral (im_start, INST, SYS, ecc.)
     - Separatori di sezione "###" (usati da alcuni modelli per separare ruoli)
     - Role-override via doppio newline: "\n\nSystem:", "\n\nAssistant:", ecc.
     - Commenti HTML nascosti "<!-- "
     - Frasi di jailbreak comuni (case-insensitive)

   La funzione è conservativa: sostituisce i pattern con spazi per non
   alterare l'allineamento del testo; non rimuove caratteri legittimi.    */
static QString _sanitize_prompt(const QString& raw)
{
    QString s = raw;

    /* 1. Format token — rimpiazza con spazio */
    static const char* const kTokens[] = {
        "<|im_start|>", "<|im_end|>", "<|endoftext|>", "<|eot_id|>",
        "[INST]", "[/INST]", "<<SYS>>", "<</SYS>>",
        "<s>", "</s>",
        "###",
        "<!-- ",
        nullptr
    };
    for (int i = 0; kTokens[i]; ++i) {
        s.replace(QString::fromUtf8(kTokens[i]),
                  QString(strlen(kTokens[i]), u' '));
    }

    /* 2. Role-override via "\n\n<Parola>:" — rimuove solo il doppio newline */
    static const char* const kRoles[] = {
        "\n\nSystem:", "\n\nAssistant:", "\n\nUser:",
        "\n\nHuman:", "\n\nAI:", "\n\nIgnore",
        nullptr
    };
    for (int i = 0; kRoles[i]; ++i) {
        /* Sostituisce "\n\n" con "  " lasciando il testo */
        QString pat = QString::fromUtf8(kRoles[i]);
        int idx = 0;
        while ((idx = s.indexOf(pat, idx)) != -1) {
            s[idx]   = u' ';
            s[idx+1] = u' ';
            idx += 2;
        }
    }

    /* 3. Frasi di jailbreak comuni (substringa, case-insensitive) */
    static const char* const kPhrases[] = {
        "ignore previous instructions",
        "ignore all previous",
        "disregard your instructions",
        "forget your system prompt",
        nullptr
    };
    QString lower = s.toLower();
    for (int i = 0; kPhrases[i]; ++i) {
        QString pat = QString::fromUtf8(kPhrases[i]);
        int idx = 0;
        while ((idx = lower.indexOf(pat, idx)) != -1) {
            /* Sostituisce con trattini per segnalare il filtraggio */
            for (int k = 0; k < pat.size() && idx+k < s.size(); ++k)
                s[idx+k] = u'-';
            lower = s.toLower();
            idx += pat.size();
        }
    }

    return s;
}

/* Rileva inglese contando articoli e parole funzionali comuni.
   Soglia: almeno 2 hit su testo di qualsiasi lunghezza → probabile inglese. */
static bool _is_likely_english(const QString& text)
{
    static const QStringList kWords = {
        " the ", " a ", " an ", " is ", " are ", " was ", " were ",
        " it ", " its ", " this ", " that ", " these ", " those ",
        " in ", " on ", " at ", " of ", " to ", " for ", " with ",
        " from ", " by ", " as ", " and ", " or ", " not ", " but ",
        " have ", " has ", " had ", " do ", " does ", " did ",
        " will ", " would ", " can ", " could ", " should ", " may ",
        " you ", " he ", " she ", " we ", " they ", " me ", " him ",
        " your ", " his ", " her ", " our ", " their ", " my ",
        " write ", " create ", " make ", " build ", " generate ",
        " explain ", " describe ", " list ", " show ", " find ",
        " how ", " what ", " why ", " when ", " where ", " who ",
        " please ", " help ", " using ", " function ", " class ",
        " code ", " program ", " script ", " file ", " data ",
        "the "  /* anche a inizio frase — rimosso "a " (falsi positivi con "da ") */
    };
    QString low = " " + text.toLower() + " ";
    int hits = 0;
    for (const QString& w : kWords) {
        if (low.contains(w)) {
            hits++;
            if (hits >= 2) return true;
        }
    }
    return false;
}

/* Aggiunge al system prompt la nota "calcoli già fatti" se il task contiene [=N] */
static QString _math_sys(const QString& task, const QString& base) {
    if (task.contains("[="))
        return base + " I valori nella forma 'espressione[=N]' nel task sono stati "
               "pre-calcolati da un motore matematico C collaudato: quei risultati sono "
               "corretti e definitivi, non ricalcolarli.";
    return base;
}

/* ── _buildSys ────────────────────────────────────────────────────────────────
 * Compone il system prompt finale applicando in ordine:
 *   1. _math_sys()   — inietta nota pre-calcoli se il task contiene [=N]
 *   2. adattamento per famiglia di modello:
 *      • LlamaLocal       → prompt minimo (≤ 2 frasi) per risparmiare contesto
 *      • modelli reasoning → aggiunge istruzione di risposta diretta
 *        (qwen3, qwq, deepseek-r1, marco-o1: hanno already internal <think>)
 *      • modelli piccoli ≤4B → tronca a max 220 caratteri per non saturare
 *        il context window limitato
 * ──────────────────────────────────────────────────────────────────────────── */
/* ── _buildSys (5 argomenti) ──────────────────────────────────────────────────
 * full       — prompt completo per modelli 7B+
 * small      — prompt corto per modelli ≤4B  (vuoto → fallback a full)
 * Flusso:
 *   1. Seleziona full o small in base a taglia modello / backend
 *   2. Inietta nota pre-calcoli _math_sys se task contiene [=N]
 *   3. LlamaLocal → tronca a max 2 frasi (contesto limitato)
 *   4. Reasoning (qwen3/qwq/deepseek-r1) → aggiunge istruzione risposta diretta
 * ──────────────────────────────────────────────────────────────────────────── */
static QString _buildSys(const QString& task,
                          const QString& full, const QString& small,
                          const QString& modelName, AiClient::Backend backend)
{
    const QString ml = modelName.toLower();

    /* Rileva modelli piccoli (≤4.5B) dal nome */
    static QRegularExpression sizeRe(R"((\d+(?:\.\d+)?)\s*b\b)");
    auto sm = sizeRe.match(ml);
    const bool isSmall = sm.hasMatch() && sm.captured(1).toDouble() <= 4.5;

    /* Seleziona il prompt base: small se disponibile e il modello lo richiede */
    const bool useSmall = (backend == AiClient::LlamaLocal || isSmall) && !small.isEmpty();
    QString sys = _math_sys(task, useSmall ? small : full);

    /* ── LlamaLocal: massimo 2 frasi (priorità assoluta) ── */
    if (backend == AiClient::LlamaLocal) {
        int p1 = sys.indexOf(". ");
        if (p1 > 0) {
            int p2 = sys.indexOf(". ", p1 + 2);
            sys = sys.left(p2 > 0 ? p2 + 1 : p1 + 1);
        }
        return sys;
    }

    /* ── Reasoning models: rispondi direttamente (dopo la selezione prompt) ── */
    const bool isReasoning =
        ml.contains("qwen3")       || ml.contains("qwq")   ||
        ml.contains("deepseek-r1") || ml.contains("r1-")   ||
        ml.contains("-r1:")        || ml.contains("marco-o1");
    if (isReasoning)
        sys += " Nella risposta finale, vai direttamente al punto senza "
               "riformulare il processo di ragionamento.";

    return sys;
}

/* ── _buildSys (4 argomenti) — backward compat per prompt senza variante small ── */
static QString _buildSys(const QString& task, const QString& full,
                          const QString& modelName, AiClient::Backend backend)
{
    return _buildSys(task, full, QString(), modelName, backend);
}

static bool _gp_is_prime(long long n) {
    if(n<2)return false; if(n<4)return true;
    if(n%2==0||n%3==0)return false;
    for(long long i=5;i*i<=n;i+=6) if(n%i==0||n%(i+2)==0)return false;
    return true;
}

/* ══════════════════════════════════════════════════════════════
   _gp_preprocess — normalizza la stringa prima di passarla al parser.
   Converte operatori alternativi in quelli riconosciuti dal parser.
   ══════════════════════════════════════════════════════════════ */
static QString _gp_preprocess(QString s) {
    s.replace("**", "^");          /* Python-style power: 3**4 → 3^4  */
    s.replace("\xc3\x97", "*");    /* × (U+00D7) → *                  */
    s.replace("\xc3\xb7", "/");    /* ÷ (U+00F7) → /                  */
    s.replace(",", ".");           /* virgola decimale → punto         */
    return s;
}

/* ══════════════════════════════════════════════════════════════
   injectMathResults — pre-elaborazione task ibridi (math + AI)
   Trova espressioni numeriche nel testo (es. "12345*6789") e le
   sostituisce con "12345*6789[=83810205]" prima di passare il
   task all'AI, così l'agente riceve già il valore calcolato.
   Restituisce il task modificato (invariato se nessuna expr trovata).
   ══════════════════════════════════════════════════════════════ */
static QString _inject_math(const QString& task)
{
    /* Pre-pass: sqrt(X) / radq(X) → sqrt(X)[=result] */
    static QRegularExpression reSqrtInline(
        R"((?:sqrt|radq)\s*\(\s*([0-9]+(?:[.,][0-9]+)?(?:[eE][+\-]?[0-9]+)?)\s*\))");
    QString result = task;
    int off = 0;
    auto itSq = reSqrtInline.globalMatch(task);
    while (itSq.hasNext()) {
        auto m = itSq.next();
        double n = m.captured(1).replace(',','.').toDouble();
        double v = std::sqrt(n);
        QString rep = QString("sqrt(%1)[=%2]").arg(_gp_fmt(n)).arg(_gp_fmt(v));
        result.replace(off + m.capturedStart(0), m.capturedLength(0), rep);
        off += rep.length() - m.capturedLength(0);
    }

    /* Regex: sequenza di cifre/operatori/parentesi non banale (almeno un operatore) */
    static QRegularExpression reExpr(
        R"([\(\-]?[0-9]+(?:[.,][0-9]+)?(?:\s*[\+\-\*\/\^%]\s*[\(\-]?[0-9]+(?:[.,][0-9]+)?)+\)?)");
    int offset = 0;
    auto it = reExpr.globalMatch(result);
    while (it.hasNext()) {
        auto m = it.next();
        QString expr = m.captured(0).simplified();
        /* Normalizza: virgola → punto, spazi attorno op rimossi */
        QString normalized = _gp_preprocess(expr);
        normalized.remove(' ');
        QByteArray ba = normalized.toLatin1();
        double v;
        if (_gp_try(ba, v) && !std::isnan(v) && !std::isinf(v)) {
            QString replacement = QString("%1[=%2]").arg(expr).arg(_gp_fmt(v));
            result.replace(offset + m.capturedStart(0), m.capturedLength(0), replacement);
            offset += replacement.length() - m.capturedLength(0);
        }
    }
    return result;
}

QString AgentiPage::guardiaMath(const QString& input)
{
    /* Blocchi di codice: non intercettare mai come espressioni matematiche.
       Segnali inequivocabili di codice sorgente → passa direttamente all'AI. */
    if (input.contains('\n') && input.length() > 80) return {};
    static const QStringList codeKw = {
        "def ", "class ", "import ", "return ", "print(", "for ", "while ",
        "if (", "if(", "!= ", "==", "->", "=>", "#include", "public ", "void ",
        "np.", "pd.", "plt.", "os.", "sys.", "self."
    };
    for (const QString& kw : codeKw)
        if (input.contains(kw)) return {};

    QString low = input.toLower().trimmed();
    while (!low.isEmpty() && (low.back()=='?' || low.back()==' ')) low.chop(1);
    if (low.isEmpty()) return {};

    static const QStringList prefissi = {
        "quanto fa ", "quanto vale ", "quanto e' ", "quanto e ",
        "calcola ", "risultato di ", "dimmi ", "quanto risulta ",
        "quant'e' ", "quante fa ", "fammi ", "dimmi il risultato di ",
        "quanto fa' "
    };
    for (const QString& pref : prefissi) {
        if (low.startsWith(pref)) {
            QString expr = low.mid(pref.length()).trimmed();
            QByteArray ba = expr.toLatin1(); double v;
            if (_gp_try(ba,v))
                return QString("%1 = %2").arg(expr).arg(_gp_fmt(v));
            break;
        }
    }
    /* Cerca il prefisso anche in mezzo alla frase (es: "ciao quanto fa 5+5") */
    for (const QString& pref : prefissi) {
        int idx = low.indexOf(pref);
        if (idx > 0) {
            QString expr = low.mid(idx + pref.length()).trimmed();
            QByteArray ba = expr.toLatin1(); double v;
            if (_gp_try(ba,v))
                return QString("%1 = %2").arg(expr).arg(_gp_fmt(v));
        }
    }

    {
        /* sqrt/radq come input diretto: "sqrt(9)", "sqrt 9", "radq(9)", "radq 9" */
        static QRegularExpression reSqrt(
            R"(^(?:sqrt|radq)\s*\(?\s*([0-9]+(?:[.,][0-9]+)?(?:[eE][+\-]?[0-9]+)?)\s*\)?$)");
        auto ms=reSqrt.match(low);
        if(ms.hasMatch()){
            double n=ms.captured(1).replace(',','.').toDouble();
            return QString("sqrt(%1) = %2").arg(_gp_fmt(n)).arg(_gp_fmt(std::sqrt(n)));
        }
    }
    {
        static QRegularExpression re1("radice\\s+quadrata\\s+(?:di\\s+)?([0-9.e+\\-]+)");
        static QRegularExpression re2("radice\\s+cubica\\s+(?:di\\s+)?([0-9.e+\\-]+)");
        static QRegularExpression re3("radice\\s+(?:di\\s+)?([0-9.e+\\-]+)");
        auto m1=re1.match(low); if(m1.hasMatch()){double n=m1.captured(1).toDouble();return QString("sqrt(%1) = %2").arg(_gp_fmt(n)).arg(_gp_fmt(std::sqrt(n)));}
        auto m2=re2.match(low); if(m2.hasMatch()){double n=m2.captured(1).toDouble();return QString("cbrt(%1) = %2").arg(_gp_fmt(n)).arg(_gp_fmt(std::cbrt(n)));}
        auto m3=re3.match(low); if(m3.hasMatch()){double n=m3.captured(1).toDouble();return QString("sqrt(%1) = %2").arg(_gp_fmt(n)).arg(_gp_fmt(std::sqrt(n)));}
    }
    {
        static QRegularExpression re("log(?:aritmo)?\\s+(?:in\\s+)?base\\s+([0-9.]+)\\s+(?:di\\s+)?([0-9.e+\\-]+)");
        auto m=re.match(low);
        if(m.hasMatch()){
            double base=m.captured(1).toDouble(), n=m.captured(2).toDouble();
            return QString("log base %1 di %2 = %3").arg(_gp_fmt(base)).arg(_gp_fmt(n)).arg(_gp_fmt(std::log(n)/std::log(base)));
        }
    }
    {
        static QRegularExpression re("sconto\\s+([0-9.]+)%\\s+su\\s+([0-9.]+)");
        auto m=re.match(low);
        if(m.hasMatch()){
            double pct=m.captured(1).toDouble(), base=m.captured(2).toDouble();
            double risp=base*pct/100.0, fin=base-risp;
            return QString("Sconto %1% su %2\nRisparmio: %3   Prezzo finale: %4")
                   .arg(pct).arg(_gp_fmt(base)).arg(_gp_fmt(risp)).arg(_gp_fmt(fin));
        }
    }
    {
        static QRegularExpression re("([0-9.]+)%\\s+di\\s+([0-9.]+)");
        auto m=re.match(low);
        if(m.hasMatch()){
            double pct=m.captured(1).toDouble(), base=m.captured(2).toDouble();
            return QString("%1% di %2 = %3").arg(pct).arg(_gp_fmt(base)).arg(_gp_fmt(base*pct/100.0));
        }
    }
    {
        static QRegularExpression re("(?:numeri\\s+)?primi\\s+(?:da|tra|fra)\\s+([0-9]+)\\s+(?:a|e|fino\\s+a)\\s+([0-9]+)");
        auto m=re.match(low);
        if(m.hasMatch()){
            long long a=m.captured(1).toLongLong(), b=m.captured(2).toLongLong();
            if(b-a<=10000){
                QStringList lst;
                for(long long i=a;i<=b;i++) if(_gp_is_prime(i)) lst<<QString::number(i);
                if(lst.isEmpty()) return QString("Nessun numero primo tra %1 e %2.").arg(a).arg(b);
                return QString("Primi tra %1 e %2 (%3 totali):\n%4").arg(a).arg(b).arg(lst.size()).arg(lst.join(", "));
            }
        }
    }
    {
        /* "i primi N numeri primi" / "dimmi i primi N primi" / "elenca i primi N numeri primi" */
        static QRegularExpression re("(?:(?:dimmi|elenca|lista|calcola|trova|mostrami)\\s+)?(?:i\\s+)?primi\\s+([0-9]+)\\s+(?:numeri\\s+)?prim(?:i|o)");
        auto m = re.match(low);
        if (m.hasMatch()) {
            long long n = m.captured(1).toLongLong();
            if (n > 0 && n <= 1000) {
                QStringList lst;
                long long num = 2;
                while ((long long)lst.size() < n) {
                    if (_gp_is_prime(num)) lst << QString::number(num);
                    num++;
                }
                return QString("Primi %1 numeri primi:\n%2").arg(n).arg(lst.join(", "));
            }
        }
    }
    {
        static QRegularExpression re("([0-9]+)\\s+(?:e'|è|e)\\s+primo");
        static QRegularExpression re2("(?:e'|è)\\s+primo\\s+([0-9]+)");
        static QRegularExpression re3("primo\\s+([0-9]+)");
        auto mm=re.match(low); if(!mm.hasMatch()) mm=re2.match(low); if(!mm.hasMatch()) mm=re3.match(low);
        if(mm.hasMatch()){
            long long n=mm.captured(1).toLongLong();
            return QString("%1 %2 un numero primo.").arg(n).arg(_gp_is_prime(n)?"è":"non è");
        }
    }
    {
        static QRegularExpression re("somm(?:a|atoria)\\s+(?:da|da\\s+1\\s+a)?\\s*([0-9]+)\\s+(?:a|fino\\s+a)\\s+([0-9]+)");
        auto m=re.match(low);
        if(m.hasMatch()){
            long long a=m.captured(1).toLongLong(), b=m.captured(2).toLongLong();
            return QString("Somma da %1 a %2 = %3").arg(a).arg(b).arg((b-a+1)*(a+b)/2);
        }
        static QRegularExpression re2("somm(?:a|atoria)\\s+(?:da\\s+1\\s+a|dei\\s+primi)\\s+([0-9]+)");
        auto m2=re2.match(low);
        if(m2.hasMatch()){
            long long n=m2.captured(1).toLongLong();
            return QString("Somma 1..%1 = %2").arg(n).arg(n*(n+1)/2);
        }
    }
    /* ── Logaritmo naturale / ln ── */
    {
        static QRegularExpression re(
            "(?:log(?:aritmo)?\\s+naturale|ln)\\s+(?:di\\s+)?([0-9.e+\\-]+)");
        auto m = re.match(low);
        if (m.hasMatch()) {
            double n = m.captured(1).toDouble();
            if (n > 0)
                return QString("ln(%1) = %2").arg(_gp_fmt(n)).arg(_gp_fmt(std::log(n)));
        }
    }
    /* ── Logaritmo (base 10 implicita in italiano) ── */
    {
        static QRegularExpression re(
            "log(?:aritmo)?\\s+(?:di\\s+)?([0-9.e+\\-]+)(?!\\s+base|\\s+in\\s+base)");
        auto m = re.match(low);
        if (m.hasMatch() && !low.contains("base") && !low.contains("naturale")) {
            double n = m.captured(1).toDouble();
            if (n > 0)
                return QString("log10(%1) = %2").arg(_gp_fmt(n)).arg(_gp_fmt(std::log10(n)));
        }
    }
    /* ── Funzioni trigonometriche in linguaggio naturale ── */
    {
        struct TrigEntry { const char* pattern; const char* name; double(*fn)(double); };
        static const std::initializer_list<TrigEntry> trigs = {
            {"(?:seno|sin)\\s+(?:di\\s+)?([0-9.e+\\-]+)",          "sin",    std::sin },
            {"(?:coseno|cos)\\s+(?:di\\s+)?([0-9.e+\\-]+)",        "cos",    std::cos },
            {"(?:tangente|tan)\\s+(?:di\\s+)?([0-9.e+\\-]+)",      "tan",    std::tan },
            {"(?:arcoseno|arcsin|asin)\\s+(?:di\\s+)?([0-9.e+\\-]+)",  "asin", std::asin},
            {"(?:arcocoseno|arccos|acos)\\s+(?:di\\s+)?([0-9.e+\\-]+)","acos", std::acos},
            {"(?:arcotangente|arctan|atan)\\s+(?:di\\s+)?([0-9.e+\\-]+)","atan",std::atan},
        };
        bool gradi = low.contains("grad");  /* "in gradi" → converti deg→rad */
        for (const auto& t : trigs) {
            QRegularExpression re(t.pattern);
            auto m = re.match(low);
            if (m.hasMatch()) {
                double n = m.captured(1).toDouble();
                double arg = gradi ? (n * M_PI / 180.0) : n;
                double res = t.fn(arg);
                QString unit = gradi ? "°" : " rad";
                return QString("%1(%2%3) = %4").arg(t.name).arg(_gp_fmt(n)).arg(unit).arg(_gp_fmt(res));
            }
        }
    }
    /* ── Elevamento a potenza in linguaggio naturale ── */
    {
        static QRegularExpression re(
            "([0-9.]+)\\s+(?:elevato\\s+(?:alla\\s+)?|alla\\s+)?(?:potenza\\s+)?(?:di\\s+)?([0-9.]+)");
        static QRegularExpression reElevato("([0-9.]+)\\s+elevato\\s+a\\s+([0-9.]+)");
        auto m = reElevato.match(low);
        if (m.hasMatch()) {
            double base = m.captured(1).toDouble(), exp = m.captured(2).toDouble();
            return QString("%1^%2 = %3").arg(_gp_fmt(base)).arg(_gp_fmt(exp)).arg(_gp_fmt(std::pow(base,exp)));
        }
    }
    /* ── Quadrato / cubo ── */
    {
        static QRegularExpression reQ("(?:quadrato|quadra)\\s+(?:di\\s+)?([0-9.]+)");
        static QRegularExpression reC("cubo\\s+(?:di\\s+)?([0-9.]+)");
        auto mq = reQ.match(low);
        if (mq.hasMatch()) {
            double n = mq.captured(1).toDouble();
            return QString("%1² = %2").arg(_gp_fmt(n)).arg(_gp_fmt(n*n));
        }
        auto mc = reC.match(low);
        if (mc.hasMatch()) {
            double n = mc.captured(1).toDouble();
            return QString("%1³ = %2").arg(_gp_fmt(n)).arg(_gp_fmt(n*n*n));
        }
    }
    /* ── Fattoriale ── */
    {
        /* Lookahead negativo (?!=): esclude '!=' (operatore disuguaglianza Python/C) */
        static QRegularExpression re("(?:fattoriale\\s+(?:di\\s+)?)?([0-9]+)\\s*!(?!=)");
        static QRegularExpression re2("fattoriale\\s+(?:di\\s+)?([0-9]+)");
        auto m = re.match(low);
        if (!m.hasMatch()) m = re2.match(low);
        if (m.hasMatch()) {
            int n = m.captured(1).toInt();
            if (n >= 0 && n <= 20) {
                long long f = 1;
                for (int i = 2; i <= n; i++) f *= i;
                return QString("%1! = %2").arg(n).arg(f);
            }
        }
    }
    /* ── Valore assoluto ── */
    {
        static QRegularExpression re("(?:valore\\s+assoluto|modulo)\\s+(?:di\\s+)?([\\-]?[0-9.]+)");
        auto m = re.match(low);
        if (m.hasMatch()) {
            double n = m.captured(1).toDouble();
            return QString("|%1| = %2").arg(_gp_fmt(n)).arg(_gp_fmt(std::fabs(n)));
        }
    }
    /* ── Espressione pura (con pre-processing ** → ^, × → *, ecc.) ── */
    {
        QByteArray ba = _gp_preprocess(low).toLatin1();
        double v;
        if (_gp_try(ba, v))
            return QString("%1 = %2").arg(input.trimmed()).arg(_gp_fmt(v));
    }
    return {};
}

/* ══════════════════════════════════════════════════════════════
   checkRam — controllo RAM centralizzato pre-pipeline
   Ritorna true se si può procedere, false se bloccato/annullato.
   ══════════════════════════════════════════════════════════════ */
bool AgentiPage::checkRam()
{
    double ramPct = 0.0;
    QFile minfo("/proc/meminfo");
    if (minfo.open(QIODevice::ReadOnly)) {
        long long total = 0, avail = 0;
        static const QRegularExpression reWs("\\s+");
        while (!minfo.atEnd()) {
            QString line = minfo.readLine().trimmed();
            const auto parts = line.split(reWs);
            if (parts.size() >= 2) {
                if (line.startsWith("MemTotal:"))    total = parts[1].toLongLong();
                if (line.startsWith("MemAvailable:")) avail = parts[1].toLongLong();
            }
        }
        minfo.close();
        if (total > 0) ramPct = 100.0 * (total - avail) / total;
    }
    if (ramPct >= 92.0) {
        m_log->append(QString("\xe2\x9d\x8c  <b>RAM critica (%1% usata)</b> — operazione bloccata.")
                      .arg(ramPct, 0, 'f', 0));
        m_log->append("\xf0\x9f\x92\xa1  Chiudi altre applicazioni o scarica il modello prima di continuare.");
        emit pipelineStatus(-1, "");
        return false;
    }
    if (ramPct >= 75.0) {
        auto btn = QMessageBox::warning(this, "RAM elevata",
            QString("RAM al %1% — la pipeline potrebbe crashare a met\xc3\xa0.\n\nContinuare comunque?")
                .arg(ramPct, 0, 'f', 0),
            QMessageBox::Yes | QMessageBox::No, QMessageBox::No);
        if (btn != QMessageBox::Yes) {
            emit pipelineStatus(-1, "");
            return false;
        }
    }
    return true;
}

/* ══════════════════════════════════════════════════════════════
   checkModelSize — avvisa se il modello pesa > 70% RAM libera
   ══════════════════════════════════════════════════════════════ */
bool AgentiPage::checkModelSize(const QString& model)
{
    /* Trova la dimensione del modello in m_modelInfos */
    qint64 sizeBytes = 0;
    for (const auto& mi : m_modelInfos) {
        if (mi.name == model) { sizeBytes = mi.size; break; }
    }
    if (sizeBytes <= 0) return true;  /* dimensione sconosciuta: procedi */

    /* Leggi RAM libera da /proc/meminfo (MemAvailable, in kB) */
    long long availKb = 0;
    QFile minfo("/proc/meminfo");
    if (minfo.open(QIODevice::ReadOnly)) {
        while (!minfo.atEnd()) {
            QString line = minfo.readLine().trimmed();
            if (line.startsWith("MemAvailable:")) {
                static const QRegularExpression reWs("\\s+");
                const auto parts = line.split(reWs);
                if (parts.size() >= 2) availKb = parts[1].toLongLong();
                break;
            }
        }
        minfo.close();
    }
    if (availKb <= 0) return true;  /* non su Linux: procedi */

    const double sizeGb  = sizeBytes / 1e9;
    const double availGb = availKb   / 1e6;

    if (sizeBytes > availKb * 1024LL * 0.7) {
        auto btn = QMessageBox::warning(this,
            "Modello grande — conferma",
            QString("Il modello <b>%1</b> pesa circa <b>%2 GB</b>.<br>"
                    "RAM libera disponibile: <b>%3 GB</b>.<br><br>"
                    "Il caricamento potrebbe causare rallentamenti o crash.<br>"
                    "Continuare comunque?")
                .arg(model.toHtmlEscaped())
                .arg(sizeGb,  0, 'f', 1)
                .arg(availGb, 0, 'f', 1),
            QMessageBox::Yes | QMessageBox::No, QMessageBox::No);
        return btn == QMessageBox::Yes;
    }
    return true;
}

/* ══════════════════════════════════════════════════════════════
   Pipeline sequenziale
   ══════════════════════════════════════════════════════════════ */
void AgentiPage::runPipeline() {
    m_userScrolled = false;  /* nuovo task: torna in auto-scroll */
    QString task = _sanitize_prompt(m_input->toPlainText().trimmed());
    if (task.isEmpty()) { m_log->append("\xe2\x9a\xa0  Inserisci un task."); return; }

    {
        QElapsedTimer tmr; tmr.start();
        QString ris = guardiaMath(task);
        double ms = tmr.nsecsElapsed() / 1e6;
        if (!ris.isEmpty()) {
            m_log->clear();
            { int i = m_bubbleIdx++; m_bubbleTexts[i] = task;
              m_log->moveCursor(QTextCursor::End);
              m_log->insertHtml(buildUserBubble(task, i)); }
            m_log->append("");
            { int i = m_bubbleIdx++; m_bubbleTexts[i] = ris;
              m_log->moveCursor(QTextCursor::End);
              m_log->insertHtml(buildLocalBubble(ris, ms, i)); }
            emit chatCompleted(task.left(40), m_log->toHtml());
            return;
        }
    }

    /* ── Guardia Grafico: formula rilevata → grafico locale, zero token AI ── */
    {
        const QString expr = FormulaParser::tryExtract(task);
        if (!expr.isEmpty()) {
            FormulaParser fp(expr);
            if (fp.ok()) {
                double xMin = -10.0, xMax = 10.0;
                FormulaParser::tryExtractXRange(task, xMin, xMax);
                const auto pts = fp.sample(xMin, xMax, 400);
                if (!pts.isEmpty()) {
                    m_log->clear();
                    m_taskOriginal = task;
                    { int i = m_bubbleIdx++; m_bubbleTexts[i] = task;
                      m_log->insertHtml(buildUserBubble(task, i)); }
                    m_log->append("");
                    /* Bolla chart: solo pulsante "Mostra grafico" senza testo formula */
                    {
                        const auto& _c = bc();
                        const int br = QSettings("Prismalux","GUI").value("bubble_radius",10).toInt();
                        const QString brs = QString::number(br) + "px";
                        const QString chartHtml =
                            "<p style='margin:6px 0;'></p>"
                            "<table width='100%' cellpadding='0' cellspacing='0'>"
                            "<tr>"
                              "<td bgcolor='" + QString(_c.lBg) + "' style='"
                                  "border:1px solid " + _c.lBdr + ";"
                                  "border-radius:" + brs + ";"
                                  "padding:10px 14px;"
                                  "color:" + _c.lTxt + ";"
                              "'>"
                                "<p style='color:" + _c.lHdr + ";font-size:11px;font-weight:bold;"
                                    "margin:0 0 8px 0;'>"
                                  "\xe2\x9a\xa1  Risposta locale  \xc2\xb7\xc2\xb7  0 token  \xc2\xb7\xc2\xb7  &lt;1 ms"
                                "</p>"
                                "<hr style='border:none;border-top:1px solid " + _c.lHr + ";margin:5px 0 8px 0;'>"
                                "<p align='center' style='margin:4px 0 8px 0;'>"
                                  "<a href='chart:show'"
                                     " style='color:" + _c.lBtn + ";font-size:14px;font-weight:bold;"
                                             "text-decoration:none;background:" + _c.lRes + ";"
                                             "border:1px solid " + _c.lBdr + ";"
                                             "border-radius:6px;padding:6px 20px;'>"
                                    "\xf0\x9f\x93\x88  Mostra grafico"
                                  "</a>"
                                "</p>"
                              "</td>"
                              "<td width='30'>&nbsp;</td>"
                            "</tr>"
                            "</table>"
                            "<p style='margin:4px 0;'></p>";
                        m_log->insertHtml(chartHtml);
                    }
                    tryShowChart(task);
                    m_input->clear();
                    emit chatCompleted(task.left(40), m_log->toHtml());
                    return;
                }
            }
        }
    }

    int count = 0;
    for (int i = 0; i < MAX_AGENTS; i++)
        if (m_cfgDlg->enabledChk(i)->isChecked()) count++;
    if (count == 0) { m_log->append("\xe2\x9a\xa0  Abilita almeno un agente."); return; }

    /* Aggiunge il contesto documento se allegato */
    if (!m_docContext.isEmpty()) {
        task += "\n\n--- DOCUMENTO ALLEGATO ---\n" + m_docContext.left(8000);
    }
    m_taskOriginal = _inject_math(task);
    m_agentOutputs.clear();
    m_currentAgent = 0;
    m_maxShots     = m_cfgDlg->numAgents();
    m_opMode       = OpMode::Pipeline;

    for (int i = 0; i < MAX_AGENTS; i++)
        m_cfgDlg->enabledChk(i)->setStyleSheet("");
    emit pipelineStatus(0, "Avvio pipeline...");

    /* NON si cancella il log: le Q&A si accumulano nella stessa chat */
    /* Bolla utente — mostra il task come messaggio "Tu" */
    { int i = m_bubbleIdx++; m_bubbleTexts[i] = m_taskOriginal;
      m_log->insertHtml(buildUserBubble(m_taskOriginal, i)); }
    m_log->append("");

    /* ── Controllo RAM e dimensione modello pre-pipeline ── */
    if (!checkRam()) return;
    if (!checkModelSize(m_ai->model())) return;

    m_btnRun->setEnabled(false);
    m_btnStop->setEnabled(true);
    m_waitLbl->setVisible(true);

    advancePipeline();
}

void AgentiPage::advancePipeline() {
    while (m_currentAgent < MAX_AGENTS && !m_cfgDlg->enabledChk(m_currentAgent)->isChecked())
        m_currentAgent++;

    if (m_currentAgent >= MAX_AGENTS || m_currentAgent >= m_maxShots) {
        m_log->append("\n\xe2\x9c\x85  Pipeline completata. \xe2\x9c\xa8 Verit\xc3\xa0 rivelata.");
        emit pipelineStatus(100, "\xe2\x9c\x85  Completata!");
        m_btnRun->setEnabled(true);
        m_btnStop->setEnabled(false);
        m_opMode = OpMode::Idle;
        /* Rilevamento formula → mostra grafico se presente */
        if (!m_agentOutputs.isEmpty())
            tryShowChart(m_taskOriginal + "\n" + m_agentOutputs.last());
        /* Salva storico chat */
        emit chatCompleted(m_taskOriginal.left(40), m_log->toHtml());
        return;
    }

    int total = 0, done = 0;
    for (int i = 0; i < MAX_AGENTS; i++)
        if (m_cfgDlg->enabledChk(i)->isChecked()) total++;
    for (int i = 0; i < m_currentAgent; i++)
        if (m_cfgDlg->enabledChk(i)->isChecked()) done++;
    int pct = (total > 0) ? (done * 100 / total) : 0;

    auto roleList = AgentsConfigDialog::roles();
    int roleIdx = m_cfgDlg->roleCombo(m_currentAgent)->currentIndex();
    if (roleIdx < 0 || roleIdx >= roleList.size()) roleIdx = 0;
    QString roleName = roleList[roleIdx].icon + " " + roleList[roleIdx].name;

    m_tokenCount = 0;
    emit pipelineStatus(pct, QString("\xe2\x9a\x99\xef\xb8\x8f  Agente %1/%2 \xe2\x80\x94 %3...")
        .arg(done + 1).arg(total).arg(roleName));

    runAgent(m_currentAgent);
}

void AgentiPage::runAgent(int idx) {
    auto roleList = AgentsConfigDialog::roles();
    int  roleIdx  = m_cfgDlg->roleCombo(idx)->currentIndex();
    if (roleIdx < 0 || roleIdx >= roleList.size()) roleIdx = 0;
    const auto& role = roleList[roleIdx];

    QString selectedModel = m_cfgDlg->modelCombo(idx)->currentData().toString();
    if (selectedModel.isEmpty()) selectedModel = m_cfgDlg->modelCombo(idx)->currentText();
    if (!selectedModel.isEmpty() && selectedModel != "(nessun modello)")
        m_ai->setBackend(m_ai->backend(), m_ai->host(), m_ai->port(), selectedModel);

    QString ts = QTime::currentTime().toString("HH:mm:ss");

    /* Salva posizione PRIMA dell'header: l'intera bolla (header+contenuto) sarà sostituita su finish */
    {
        QTextCursor c(m_log->document());
        c.movePosition(QTextCursor::End);
        m_agentBlockStart = c.position();
    }

    /* Metadati per la bolla finale */
    m_currentAgentLabel = role.icon + QString("  Agente %1 \xe2\x80\x94 %2").arg(idx + 1).arg(role.name);
    m_currentAgentModel = selectedModel;
    m_currentAgentTime  = ts;

    /* Indicatore streaming temporaneo (sarà sostituito dalla bolla su onFinished) */
    m_log->append(QString("\n%1  [Agente %2 \xe2\x80\x94 %3]  \xf0\x9f\x94\x84 generando...\n")
                  .arg(role.icon).arg(idx + 1).arg(role.name));
    m_agentTextStart = m_log->document()->characterCount() - 1;

    QString userPrompt = m_taskOriginal;

    if (m_cfgDlg->ragWidget(idx) && m_cfgDlg->ragWidget(idx)->hasContext())
        userPrompt += _sanitize_prompt(m_cfgDlg->ragWidget(idx)->ragContext());
    if (!m_agentOutputs.isEmpty()) {
        userPrompt += "\n\n\xe2\x80\x94\xe2\x80\x94 Output agenti precedenti \xe2\x80\x94\xe2\x80\x94\n";
        for (int i = 0; i < m_agentOutputs.size(); i++) {
            int prevRole = m_cfgDlg->roleCombo(i)->currentIndex();
            QString prevName = (prevRole >= 0 && prevRole < roleList.size())
                               ? roleList[prevRole].name : "Agente";
            userPrompt += QString("\n[Agente %1 — %2]:\n%3\n")
                          .arg(i + 1).arg(prevName).arg(m_agentOutputs[i]);
        }
        userPrompt += "\n" + QString(16, QChar(0x2500)) + "\n";
        userPrompt += QString("Ora esegui il tuo ruolo di %1.").arg(role.name);
    }

    m_agentOutputs.append("");
    /* Usa chatWithImage per il primo agente se c'è un'immagine allegata */
    if (idx == m_currentAgent && !m_imgBase64.isEmpty()) {
        m_ai->chatWithImage(_buildSys(m_taskOriginal, role.sysPrompt, role.sysPromptSmall, m_ai->model(), m_ai->backend()), userPrompt,
                            m_imgBase64, m_imgMime);
    } else {
        m_ai->chat(_buildSys(m_taskOriginal, role.sysPrompt, role.sysPromptSmall, m_ai->model(), m_ai->backend()), userPrompt);
    }
}

/* ══════════════════════════════════════════════════════════════
   Motore Byzantino
   ══════════════════════════════════════════════════════════════ */
void AgentiPage::runByzantine() {
    m_userScrolled = false;  /* nuovo task: torna in auto-scroll */
    QString fact = _sanitize_prompt(m_input->toPlainText().trimmed());
    if (fact.isEmpty()) { m_log->append("\xe2\x9a\xa0  Inserisci una domanda."); return; }

    {
        QElapsedTimer tmr; tmr.start();
        QString ris = guardiaMath(fact);
        double ms = tmr.nsecsElapsed() / 1e6;
        if (!ris.isEmpty()) {
            m_log->clear();
            { int i = m_bubbleIdx++; m_bubbleTexts[i] = fact;
              m_log->moveCursor(QTextCursor::End);
              m_log->insertHtml(buildUserBubble(fact, i)); }
            m_log->append("");
            { int i = m_bubbleIdx++; m_bubbleTexts[i] = ris;
              m_log->moveCursor(QTextCursor::End);
              m_log->insertHtml(buildLocalBubble(ris, ms, i)); }
            emit chatCompleted(fact.left(40), m_log->toHtml());
            return;
        }
    }

    /* ── Controllo RAM e dimensione modello pre-Byzantine ── */
    if (!checkRam()) return;
    if (!checkModelSize(m_ai->model())) return;

    /* Aggiunge il contesto documento se allegato */
    if (!m_docContext.isEmpty()) {
        fact += "\n\n--- DOCUMENTO ALLEGATO ---\n" + m_docContext.left(8000);
    }
    m_byzStep = 0; m_byzA = m_byzC = "";
    m_taskOriginal = _inject_math(fact);
    m_opMode = OpMode::Byzantine;

    m_log->clear();
    m_log->append(QString(
        "\xf0\x9f\x94\xae  Motore Byzantino \xe2\x80\x94 verifica a 4 agenti\n"
        "\xe2\x9d\x93  Domanda: %1\n").arg(m_taskOriginal));
    m_log->append(QString(43, QChar(0x2500)));
    m_log->append("\xf0\x9f\x85\x90  [Agente A \xe2\x80\x94 Originale]\n");

    m_btnRun->setEnabled(false);
    m_btnStop->setEnabled(true);
    m_waitLbl->setVisible(true);

    m_ai->chat(
        _buildSys(m_taskOriginal, QString(
            "Sei l'Agente A del Motore Byzantino di Prismalux. "
            "Fornisci una risposta diretta e ben argomentata. "
            "Rispondi SEMPRE e SOLO in italiano."),
            m_ai->model(), m_ai->backend()),
        m_taskOriginal);
}

/* ══════════════════════════════════════════════════════════════
   Matematico Teorico
   ══════════════════════════════════════════════════════════════ */
void AgentiPage::runMathTheory() {
    m_userScrolled = false;  /* nuovo task: torna in auto-scroll */
    QString problem = _sanitize_prompt(m_input->toPlainText().trimmed());
    if (problem.isEmpty()) { m_log->append("\xe2\x9a\xa0  Inserisci un problema matematico."); return; }

    {
        QElapsedTimer tmr; tmr.start();
        QString ris = guardiaMath(problem);
        double ms = tmr.nsecsElapsed() / 1e6;
        if (!ris.isEmpty()) {
            m_log->clear();
            { int i = m_bubbleIdx++; m_bubbleTexts[i] = problem;
              m_log->moveCursor(QTextCursor::End);
              m_log->insertHtml(buildUserBubble(problem, i)); }
            m_log->append("");
            { int i = m_bubbleIdx++; m_bubbleTexts[i] = ris;
              m_log->moveCursor(QTextCursor::End);
              m_log->insertHtml(buildLocalBubble(ris, ms, i)); }
            emit chatCompleted(problem.left(40), m_log->toHtml());
            return;
        }
    }

    /* ── Controllo RAM e dimensione modello pre-MathTheory ── */
    if (!checkRam()) return;
    if (!checkModelSize(m_ai->model())) return;

    m_byzStep = 0; m_byzA = m_byzC = "";
    m_taskOriginal = _inject_math(problem);
    m_opMode = OpMode::MathTheory;

    m_log->clear();
    m_log->append(QString(
        "\xf0\x9f\xa7\xae  Matematico Teorico \xe2\x80\x94 esplorazione a 4 agenti\n"
        "\xe2\x9d\x93  Problema: %1\n").arg(m_taskOriginal));
    m_log->append(QString(43, QChar(0x2500)));
    m_log->append("\xf0\x9f\x85\xb0  [Agente 1 \xe2\x80\x94 Enunciatore]\n");

    m_btnRun->setEnabled(false);
    m_btnStop->setEnabled(true);
    m_waitLbl->setVisible(true);

    m_ai->chat(
        "Sei l'Enunciatore matematico. "
        "Riformula il problema in forma rigorosa: definizioni, dominio, ipotesi. "
        "Sii conciso (max 150 parole). Rispondi SOLO in italiano.", problem);
}

/* ══════════════════════════════════════════════════════════════
   buildUserBubble / buildAgentBubble
   Layout chat-style: utente a destra (blu), AI a sinistra (scuro).
   QTextEdit renderizza HTML4 + CSS2 parziale: usiamo <table> per il
   layout e style inline per colori/bordi/padding.
   ══════════════════════════════════════════════════════════════ */
QString AgentiPage::buildUserBubble(const QString& text, int bubbleIdx)
{
    /* Normalizza: minuscolo (meno memoria nel modello, aspetto più naturale) */
    QString safe = text.toLower();
    safe.replace("&","&amp;").replace("<","&lt;").replace(">","&gt;");
    /* Preserva a capo */
    safe.replace("\n","<br>");

    /* Barra azioni dentro la bolla — link semplici (QTextBrowser gestisce solo <a href> piani) */
    const auto& c = bc();
    QString actionBar;
    if (bubbleIdx >= 0) {
        const QString id  = QString::number(bubbleIdx);
        const QString b64 = text.left(4096).toUtf8().toBase64(
            QByteArray::Base64UrlEncoding | QByteArray::OmitTrailingEquals);
        const QString lnk = QString("color:%1;font-size:12px;text-decoration:none;"
            "border:1px solid %2;padding:2px 10px;background:%3;")
            .arg(c.uHdr, c.uBtnB, c.uBtn);
        actionBar =
            "<p align='right' style='margin:6px 0 0 0;'>"
              "<a href='copy:" + id + ":" + b64 + "' style='" + lnk + "'>"
                "\xf0\x9f\x97\x82</a>"
              " &nbsp; "
              "<a href='edit:" + id + ":" + b64 + "' style='" + lnk + "'>"
                "\xe2\x9c\x8f\xef\xb8\x8f</a>"
              " &nbsp; "
              "<a href='tts:" + id + ":" + b64 + "' style='" + lnk + "'>"
                "\xf0\x9f\x8e\x99</a>"
              " &nbsp; "
              "<a href='del:" + id + ":" + b64 + "' style='" + lnk + "'>"
                "\xf0\x9f\x97\x91</a>"
            "</p>";
    }

    const int br = QSettings("Prismalux","GUI").value("bubble_radius",10).toInt();
    const QString brs = QString::number(br) + "px";
    return
        "<table width='100%' cellpadding='0' cellspacing='0'>"
        "<tr>"
          "<td width='80'>&nbsp;</td>"
          "<td bgcolor='" + QString(c.uBg) + "' style='"
              "border:1px solid " + c.uBdr + ";"
              "border-radius:" + brs + ";"
              "padding:10px 14px;"
              "color:" + c.uTxt + ";"
          "'>"
            "<p style='color:" + c.uHdr + ";font-size:11px;font-weight:bold;"
                       "margin:0 0 5px 0;'>"
              "\xf0\x9f\x91\xa4  Tu</p>"
            "<p style='margin:0;line-height:1.6;color:" + c.uTxt + ";'>" + safe + "</p>"
            + actionBar +
          "</td>"
        "</tr>"
        "</table>"
        "<p style='margin:6px 0;'></p>";
}

QString AgentiPage::buildAgentBubble(const QString& label, const QString& model,
                                     const QString& time,  const QString& htmlContent,
                                     int bubbleIdx)
{
    /* Header bolla: "🛸  Agente 1 — Ricercatore  ·  🤖 deepseek-r1:7b  ·  01:55:47" */
    QString esc_model = model;
    esc_model.replace("&","&amp;").replace("<","&lt;").replace(">","&gt;");
    QString esc_label = label;
    esc_label.replace("&","&amp;").replace("<","&lt;").replace(">","&gt;");

    QString header = esc_label
        + " &nbsp;\xc2\xb7\xc2\xb7&nbsp; "
        "\xf0\x9f\xa4\x96 " + esc_model
        + " &nbsp;\xc2\xb7\xc2\xb7&nbsp; "
        "\xf0\x9f\x95\x90 " + time;

    /* Barra azioni dentro la bolla agente */
    const auto& c = bc();
    QString actionBar;
    if (bubbleIdx >= 0) {
        const QString id = QString::number(bubbleIdx);
        QTextDocument _tmp;
        _tmp.setHtml(htmlContent);
        const QString b64 = _tmp.toPlainText().left(4096).toUtf8().toBase64(
            QByteArray::Base64UrlEncoding | QByteArray::OmitTrailingEquals);
        QString metaLeft;
        if (!time.isEmpty() || !model.isEmpty()) {
            QString esc_t = time;
            QString esc_m = model;
            esc_t.replace("&","&amp;").replace("<","&lt;").replace(">","&gt;");
            esc_m.replace("&","&amp;").replace("<","&lt;").replace(">","&gt;");
            metaLeft =
                "<span style='color:#6b7280;font-size:11px;'>"
                + (time.isEmpty()  ? QString() : "\xf0\x9f\x95\x90 " + esc_t)
                + ((!time.isEmpty() && !model.isEmpty()) ? " &nbsp;\xc2\xb7&nbsp; " : QString())
                + (model.isEmpty() ? QString() : "\xf0\x9f\xa4\x96 " + esc_m)
                + "</span>";
        }
        const QString lnk = QString("color:%1;font-size:12px;text-decoration:none;"
            "border:1px solid %2;padding:2px 10px;background:%3;")
            .arg(c.aBtnC, c.aBtnB, c.aBtn);
        actionBar =
            "<table width='100%' cellpadding='0' cellspacing='0' style='margin:6px 0 0 0;'>"
            "<tr>"
              "<td>" + metaLeft + "</td>"
              "<td align='right' style='white-space:nowrap;'>"
                "<a href='copy:" + id + ":" + b64 + "' style='" + lnk + "'>"
                  "\xf0\x9f\x97\x82</a>"
                " &nbsp; "
                "<a href='edit:" + id + ":" + b64 + "' style='" + lnk + "' "
                   "title='Modifica e reinvia'>"
                  "\xe2\x9c\x8f\xef\xb8\x8f</a>"
                " &nbsp; "
                "<a href='tts:" + id + ":" + b64 + "' style='" + lnk + "'>"
                  "\xf0\x9f\x8e\x99</a>"
                " &nbsp; "
                "<a href='del:" + id + ":" + b64 + "' style='" + lnk + "' "
                   "title='Elimina questo messaggio'>"
                  "\xf0\x9f\x97\x91</a>"
              "</td>"
            "</tr>"
            "</table>";
    }

    const int br = QSettings("Prismalux","GUI").value("bubble_radius",10).toInt();
    const QString brs = QString::number(br) + "px";
    return
        "<p style='margin:6px 0;'></p>"
        "<table width='100%' cellpadding='0' cellspacing='0'>"
        "<tr>"
          "<td bgcolor='" + QString(c.aBg) + "' style='"
              "border:1px solid " + c.aBdr + ";"
              "border-radius:" + brs + ";"
              "padding:10px 14px;"
              "color:" + c.aTxt + ";"
          "'>"
            "<p style='color:" + c.aHdr + ";font-size:11px;font-weight:bold;"
                       "margin:0 0 6px 0;'>"
              + header +
            "</p>"
            "<hr style='border:none;border-top:1px solid " + c.aHr + ";margin:5px 0 8px 0;'>"
            + htmlContent
            + actionBar +
          "</td>"
          "<td width='30'>&nbsp;</td>"
        "</tr>"
        "</table>"
        "<p style='margin:4px 0;'></p>";
}

/* ══════════════════════════════════════════════════════════════
   buildLocalBubble — bolla per risposta locale (0 token, math)
   ══════════════════════════════════════════════════════════════ */
QString AgentiPage::buildLocalBubble(const QString& result, double ms, int bubbleIdx,
                                      const QString& extraLinks)
{
    QString safe = result;
    safe.replace("&","&amp;").replace("<","&lt;").replace(">","&gt;");
    safe.replace("\n","<br>");

    QString timing = ms < 1.0
        ? QString::number(ms, 'f', 3) + " ms"
        : QString::number(ms / 1000.0, 'f', 3) + " s";

    /* Barra azioni dentro la bolla locale */
    const auto& c = bc();
    QString actionBar;
    if (bubbleIdx >= 0) {
        const QString id  = QString::number(bubbleIdx);
        const QString b64 = result.left(4096).toUtf8().toBase64(
            QByteArray::Base64UrlEncoding | QByteArray::OmitTrailingEquals);
        const QString linkStyle = QString(
            "color:%1;font-size:12px;text-decoration:none;"
            "border:1px solid %2;padding:2px 10px;background:%3;")
            .arg(c.lBtnC, c.lBtnB, c.lBtn);
        actionBar =
            "<p align='right' style='margin:6px 0 0 0;'>";
        if (!extraLinks.isEmpty())
            actionBar += extraLinks + " &nbsp; ";
        actionBar +=
              "<a href='copy:" + id + ":" + b64 + "' style='" + linkStyle + "'>"
                "\xf0\x9f\x97\x82</a>"
              " &nbsp; "
              "<a href='tts:" + id + ":" + b64 + "' style='" + linkStyle + "'>"
                "\xf0\x9f\x8e\x99</a>"
            "</p>";
    }

    const int br = QSettings("Prismalux","GUI").value("bubble_radius",10).toInt();
    const QString brs = QString::number(br) + "px";
    return
        "<p style='margin:6px 0;'></p>"
        "<table width='100%' cellpadding='0' cellspacing='0'>"
        "<tr>"
          "<td bgcolor='" + QString(c.lBg) + "' style='"
              "border:1px solid " + c.lBdr + ";"
              "border-radius:" + brs + ";"
              "padding:10px 14px;"
              "color:" + c.lTxt + ";"
          "'>"
            "<p style='color:" + c.lHdr + ";font-size:11px;font-weight:bold;margin:0 0 5px 0;'>"
              "\xe2\x9a\xa1  Risposta locale  \xc2\xb7\xc2\xb7  0 token  \xc2\xb7\xc2\xb7  "
              + timing +
            "</p>"
            "<hr style='border:none;border-top:1px solid " + c.lHr + ";margin:5px 0 8px 0;'>"
            "<p style='font-size:15px;font-weight:bold;margin:0;color:" + c.lRes + ";'>"
              + safe +
            "</p>"
            + actionBar +
          "</td>"
          "<td width='30'>&nbsp;</td>"
        "</tr>"
        "</table>"
        "<p style='margin:4px 0;'></p>";
}

/* ══════════════════════════════════════════════════════════════
   buildToolStrip — striscia esecutore Python
   ══════════════════════════════════════════════════════════════ */
QString AgentiPage::buildToolStrip(const QString& code, const QString& output,
                                   int exitCode, double ms)
{
    QString esc_out = output;
    esc_out.replace("&","&amp;").replace("<","&lt;").replace(">","&gt;");
    if (esc_out.trimmed().isEmpty()) esc_out = "(nessun output)";
    if (esc_out.length() > 3000) esc_out = esc_out.left(3000) + "\n... [troncato]";
    esc_out.replace("\n","<br>");

    QString esc_code = code;
    esc_code.replace("&","&amp;").replace("<","&lt;").replace(">","&gt;");
    if (esc_code.length() > 500) esc_code = esc_code.left(500) + "\n... [troncato]";
    esc_code.replace("\n","<br>");

    const auto& c = bc();
    const QString statusIcon  = (exitCode == 0) ? "\xe2\x9c\x85" : "\xe2\x9d\x8c";
    const QString statusColor = (exitCode == 0) ? c.tStOk : c.tStEr;
    const QString borderColor = (exitCode == 0) ? c.tBdOk : c.tBdEr;
    const QString bgColor     = (exitCode == 0) ? c.tBgOk : c.tBgEr;

    return
        "<p style='margin:2px 0;'></p>"
        "<table width='100%' cellpadding='0' cellspacing='0'>"
        "<tr>"
          "<td width='20'>&nbsp;</td>"
          "<td bgcolor='" + bgColor + "' style='"
              "border:1px solid " + borderColor + ";"
              "border-radius:8px;"
              "padding:8px 12px;"
              "color:" + QString(c.tTxt) + ";"
          "'>"
            "<p style='font-size:11px;font-weight:bold;margin:0 0 4px 0;color:" + statusColor + ";'>"
              "\xe2\x9a\x99\xef\xb8\x8f  Esecutore Python  \xc2\xb7\xc2\xb7  "
              + statusIcon + " exit " + QString::number(exitCode) +
              "  \xc2\xb7\xc2\xb7  " + QString::number(ms, 'f', 0) + " ms"
            "</p>"
            "<hr style='border:none;border-top:1px solid " + borderColor + ";margin:4px 0;'>"
            "<p style='font-family:\"JetBrains Mono\",monospace;font-size:11px;"
               "color:#8b949e;margin:0 0 6px 0;'>"
              + esc_out +
            "</p>"
          "</td>"
          "<td width='20'>&nbsp;</td>"
        "</tr>"
        "</table>"
        "<p style='margin:2px 0;'></p>";
}

/* ══════════════════════════════════════════════════════════════
   buildControllerBubble — bolla del controller LLM
   Colore in base al verdetto: ✅ verde / ⚠️ giallo / ❌ rosso
   ══════════════════════════════════════════════════════════════ */
QString AgentiPage::buildControllerBubble(const QString& htmlContent)
{
    /* Determina tono in base al verdetto presente nell'HTML */
    const auto& c = bc();
    QString bg = c.cBgOk, border = c.cBdOk, hdrColor = c.cHdOk;
    if (htmlContent.contains("AVVISO") || htmlContent.contains("\xe2\x9a\xa0")) {
        bg = c.cBgWn; border = c.cBdWn; hdrColor = c.cHdWn;
    }
    if (htmlContent.contains("ERRORE") || htmlContent.contains("\xe2\x9d\x8c")) {
        bg = c.cBgEr; border = c.cBdEr; hdrColor = c.cHdEr;
    }

    return
        "<p style='margin:2px 0;'></p>"
        "<table width='100%' cellpadding='0' cellspacing='0'>"
        "<tr>"
          "<td width='40'>&nbsp;</td>"
          "<td bgcolor='" + bg + "' style='"
              "border:1px solid " + border + ";"
              "border-radius:10px;"
              "padding:10px 14px;"
              "color:#e2e8f0;"
          "'>"
            "<p style='color:" + hdrColor + ";font-size:11px;font-weight:bold;margin:0 0 5px 0;'>"
              "\xf0\x9f\x9b\xa1\xef\xb8\x8f  Controller AI  \xc2\xb7\xc2\xb7  Validatore pipeline"
            "</p>"
            "<hr style='border:none;border-top:1px solid " + border + ";margin:5px 0 8px 0;'>"
            + htmlContent +
          "</td>"
        "</tr>"
        "</table>"
        "<p style='margin:6px 0;'></p>";
}

/* ══════════════════════════════════════════════════════════════
   extractPythonCode — trova il primo blocco ```python...```
   ══════════════════════════════════════════════════════════════ */
QString AgentiPage::extractPythonCode(const QString& text)
{
    static QRegularExpression re(
        "```(?:python|py)?\\s*([\\s\\S]*?)```",
        QRegularExpression::CaseInsensitiveOption);
    auto m = re.match(text);
    return m.hasMatch() ? m.captured(1).trimmed() : QString();
}

/* ══════════════════════════════════════════════════════════════
   runPipelineController — Controller LLM dopo l'esecutore
   Valida task + output agente + output esecutore.
   ══════════════════════════════════════════════════════════════ */
void AgentiPage::runPipelineController()
{
    if (m_agentOutputs.isEmpty()) { advancePipeline(); return; }

    m_opMode = OpMode::PipelineControl;
    m_ctrlAccum.clear();

    /* Segna posizione DOPO la tool strip (già inserita) per la bolla controller */
    {
        QTextCursor c(m_log->document());
        c.movePosition(QTextCursor::End);
        m_ctrlBlockStart = c.position();
    }

    /* System prompt: validator conciso */
    const QString sysPrompt =
        "Sei il Controller della pipeline Prismalux. "
        "Valida l'output dell'agente e l'esito del codice eseguito. "
        "Rispondi SOLO con una di queste righe:\n"
        "\xe2\x9c\x85 APPROVATO \xe2\x80\x94 [motivazione breve]\n"
        "\xe2\x9a\xa0\xef\xb8\x8f AVVISO \xe2\x80\x94 [problema non critico]\n"
        "\xe2\x9d\x8c ERRORE \xe2\x80\x94 [problema e correzione]\n"
        "Max 2 righe. Rispondi SOLO in italiano.";

    QString userMsg = QString(
        "Task: %1\n\n"
        "Output agente:\n%2\n\n"
        "Output esecutore (exit %3):\n%4")
        .arg(m_taskOriginal)
        .arg(m_agentOutputs.last())
        .arg(m_executorOutput.isEmpty() ? "N/A" : m_executorOutput)
        .arg(m_executorOutput.isEmpty() ? "(nessun output)" : m_executorOutput);

    m_ai->chat(sysPrompt, userMsg);
}

/* ══════════════════════════════════════════════════════════════
   markdownToHtml — converte la risposta AI in HTML leggibile
   Gestisce: code fence, inline code, bold, italic, heading,
             liste puntate/numerate, righe separatore, paragrafi.
   ══════════════════════════════════════════════════════════════ */
QString AgentiPage::markdownToHtml(const QString& md)
{
    auto escHtml = [](const QString& s) {
        QString r = s;
        r.replace("&","&amp;").replace("<","&lt;").replace(">","&gt;");
        return r;
    };

    auto inlineFmt = [&escHtml](QString s) -> QString {
        s = escHtml(s);
        /* Code span `text` — prima degli altri per proteggere il contenuto */
        static QRegularExpression cRe("`([^`]+)`");
        s.replace(cRe,
            "<code style='background:rgba(99,110,123,0.18);padding:1px 5px;"
            "border-radius:3px;font-family:\"JetBrains Mono\",monospace;"
            "font-size:0.88em;'>\\1</code>");
        /* Bold **text** */
        static QRegularExpression b2Re(R"(\*\*(.+?)\*\*)");
        s.replace(b2Re, "<b>\\1</b>");
        /* Bold __text__ */
        static QRegularExpression buRe(R"(__(.+?)__)");
        s.replace(buRe, "<b>\\1</b>");
        /* Italic *text* */
        static QRegularExpression i1Re(R"(\*([^\*\n]+?)\*)");
        s.replace(i1Re, "<i>\\1</i>");
        return s;
    };

    QString html;
    const QStringList lines = md.split('\n');
    bool    inCodeBlock = false;
    bool    inOrderedList = false;
    QString codeBuf;

    auto closeList = [&](){
        if (inOrderedList) { html += "</ol>\n"; inOrderedList = false; }
    };

    for (const QString& rawLine : lines) {
        /* ── Code fence ── */
        if (rawLine.startsWith("```")) {
            if (!inCodeBlock) {
                closeList();
                inCodeBlock = true;
                codeBuf.clear();
            } else {
                inCodeBlock = false;
                QString esc = escHtml(codeBuf).trimmed();
                html += "<pre style='background:#0d1117;color:#e6edf3;"
                        "border:1px solid #30363d;border-radius:6px;"
                        "padding:10px 14px;margin:8px 0;overflow-x:auto;"
                        "font-family:\"JetBrains Mono\",\"Fira Code\",monospace;"
                        "font-size:12px;line-height:1.5;white-space:pre;'>"
                        + esc + "</pre>\n";
                codeBuf.clear();
            }
            continue;
        }
        if (inCodeBlock) { codeBuf += rawLine + '\n'; continue; }

        const QString line = rawLine;

        /* ── Headings ── */
        if (line.startsWith("### ")) {
            closeList();
            html += "<h4 style='color:#58a6ff;margin:10px 0 4px;font-size:13px;'>"
                    + inlineFmt(line.mid(4)) + "</h4>\n";
            continue;
        }
        if (line.startsWith("## ")) {
            closeList();
            html += "<h3 style='color:#79c0ff;margin:12px 0 6px;font-size:15px;'>"
                    + inlineFmt(line.mid(3)) + "</h3>\n";
            continue;
        }
        if (line.startsWith("# ")) {
            closeList();
            html += "<h2 style='color:#79c0ff;margin:14px 0 8px;font-size:17px;font-weight:700;'>"
                    + inlineFmt(line.mid(2)) + "</h2>\n";
            continue;
        }

        /* ── Bullet list (-, *, +) ── */
        static QRegularExpression bulletRe("^[\\-\\*\\+]\\s+");
        QRegularExpressionMatch bm = bulletRe.match(line);
        if (bm.hasMatch()) {
            if (!inOrderedList) { html += "<ul style='margin:4px 0;padding-left:22px;'>"; inOrderedList = true; }
            html += "<li style='margin:2px 0;'>" + inlineFmt(line.mid(bm.capturedLength())) + "</li>";
            continue;
        }

        /* ── Numbered list ── */
        static QRegularExpression numRe("^\\d+[\\.):]\\s+");
        QRegularExpressionMatch nm = numRe.match(line);
        if (nm.hasMatch()) {
            if (!inOrderedList) { html += "<ol style='margin:4px 0;padding-left:22px;'>"; inOrderedList = true; }
            html += "<li style='margin:2px 0;'>" + inlineFmt(line.mid(nm.capturedLength())) + "</li>";
            continue;
        }

        closeList();

        /* ── Riga vuota ── */
        if (line.trimmed().isEmpty()) {
            html += "<div style='height:6px;'></div>\n";
            continue;
        }

        /* ── Separatore --- ── */
        if (line.trimmed() == "---" || line.trimmed() == "***") {
            html += "<hr style='border:none;border-top:1px solid #30363d;margin:8px 0;'>\n";
            continue;
        }

        /* ── Paragrafo normale ── */
        html += "<p style='margin:1px 0;line-height:1.6;'>" + inlineFmt(line) + "</p>\n";
    }
    closeList();

    /* Code block non chiuso (fallback) */
    if (inCodeBlock && !codeBuf.isEmpty()) {
        html += "<pre style='background:#0d1117;color:#e6edf3;"
                "border:1px solid #30363d;border-radius:6px;"
                "padding:10px 14px;margin:8px 0;"
                "font-family:\"JetBrains Mono\",monospace;font-size:12px;'>"
                + escHtml(codeBuf).trimmed() + "</pre>\n";
    }

    return html;
}

/* ══════════════════════════════════════════════════════════════
   Slot segnali AI
   ══════════════════════════════════════════════════════════════ */
void AgentiPage::onToken(const QString& t) {
    if (m_opMode == OpMode::AutoAssign) {
        m_autoBuffer += t;
        return;
    }
    if (m_opMode == OpMode::ConsiglioScientifico) {
        /* I peer gestiscono i loro token internamente via lambda — ignora */
        return;
    }
    /* In modalità Idle non siamo attivi: i token appartengono ad un'altra pagina.
       Ignorarli previene output fantasma nel log degli agenti. */
    if (m_opMode == OpMode::Idle) return;

    m_waitLbl->setVisible(false);
    QTextCursor cursor(m_log->document());
    cursor.movePosition(QTextCursor::End);
    cursor.insertText(t);
    if (!m_userScrolled) {
        m_suppressScrollSig = true;
        m_log->ensureCursorVisible();
        m_suppressScrollSig = false;
    }

    if (m_opMode == OpMode::Pipeline && !m_agentOutputs.isEmpty()) {
        m_agentOutputs.last() += t;

        m_tokenCount++;
        int total = 0, done = 0;
        for (int i = 0; i < MAX_AGENTS; i++) if (m_cfgDlg->enabledChk(i)->isChecked()) total++;
        for (int i = 0; i < m_currentAgent; i++) if (m_cfgDlg->enabledChk(i)->isChecked()) done++;
        if (total > 0) {
            double share    = 100.0 / total;
            double startPct = done * share;
            double fill     = share * (1.0 - std::exp(-m_tokenCount / 150.0)) * 0.92;
            emit pipelineStatus(qBound(0, (int)(startPct + fill), 99), QString());
        }
    }
    else if (m_opMode == OpMode::Byzantine) {
        if      (m_byzStep == 0) m_byzA += t;
        else if (m_byzStep == 2) m_byzC += t;
    } else if (m_opMode == OpMode::MathTheory) {
        if      (m_byzStep == 0) m_byzA += t;
        else if (m_byzStep == 2) m_byzC += t;
    } else if (m_opMode == OpMode::PipelineControl) {
        m_ctrlAccum += t;
    } else if (m_opMode == OpMode::Translating) {
        m_translateBuf += t;
    }
}

void AgentiPage::onFinished(const QString& /*full*/) {
    m_waitLbl->setVisible(false);

    if (m_opMode == OpMode::Translating) {
        /* Traduzione completata — aggiorna il task e riparte con la modalità originale */
        QString translated = m_translateBuf.trimmed();
        if (translated.isEmpty()) translated = m_taskOriginal; /* fallback: testo originale */
        m_taskOriginal = _inject_math(translated);
        m_log->append(QString("\n\xf0\x9f\x8c\x90  Traduzione: <i>%1</i>\n").arg(m_taskOriginal));
        m_log->append(QString(43, QChar(0x2500)));
        OpMode next = m_pendingMode;
        m_pendingMode = OpMode::Idle;
        m_opMode = OpMode::Idle;
        /* Riavvia la modalità richiesta con il testo tradotto già in m_taskOriginal */
        if (next == OpMode::Pipeline) {
            int count = 0;
            for (int i = 0; i < MAX_AGENTS; i++)
                if (m_cfgDlg->enabledChk(i)->isChecked()) count++;
            m_agentOutputs.clear();
            m_currentAgent = 0;
            m_maxShots = m_cfgDlg->numAgents();
            m_opMode   = OpMode::Pipeline;
            for (int i = 0; i < MAX_AGENTS; i++) m_cfgDlg->enabledChk(i)->setStyleSheet("");
            emit pipelineStatus(0, "Avvio pipeline...");
            { int idx = m_bubbleIdx++; m_bubbleTexts[idx] = m_taskOriginal;
              m_log->insertHtml(buildUserBubble(m_taskOriginal, idx)); }
            m_log->append("");
            m_waitLbl->setVisible(true);
            advancePipeline();
        } else if (next == OpMode::Byzantine) {
            m_byzStep = 0; m_byzA = m_byzC = "";
            m_opMode = OpMode::Byzantine;
            m_log->append("\xf0\x9f\x94\xae  Motore Byzantino — verifica a 4 agenti\n");
            m_log->append("\xf0\x9f\x85\x90  [Agente A — Originale]\n");
            m_waitLbl->setVisible(true);
            m_ai->chat(_buildSys(m_taskOriginal, QString(
                "Sei l'Agente A del Motore Byzantino di Prismalux. "
                "Fornisci una risposta diretta e ben argomentata. "
                "Rispondi SEMPRE e SOLO in italiano."),
                m_ai->model(), m_ai->backend()), m_taskOriginal);
        } else if (next == OpMode::MathTheory) {
            m_byzStep = 0; m_byzA = m_byzC = "";
            m_opMode = OpMode::MathTheory;
            m_log->append("\xf0\x9f\xa7\xae  Matematico Teorico — esplorazione a 4 agenti\n");
            m_log->append("\xf0\x9f\x85\xb0  [Agente 1 — Enunciatore]\n");
            m_waitLbl->setVisible(true);
            m_ai->chat(_buildSys(m_taskOriginal, QString(
                "Sei l'Enunciatore matematico. Riformula il problema in forma rigorosa. "
                "Rispondi SEMPRE e SOLO in italiano."),
                m_ai->model(), m_ai->backend()), m_taskOriginal);
        } else {
            /* next == Idle: traduzione esplicita senza pipeline successiva */
            QString translated = m_translateBuf.trimmed();
            m_log->append("\n" + translated);
            /* Ripristina modello precedente se era stato cambiato */
            if (!m_preTranslateModel.isEmpty() && m_preTranslateModel != m_ai->model())
                m_ai->setBackend(m_ai->backend(), m_ai->host(), m_ai->port(), m_preTranslateModel);
            m_preTranslateModel.clear();
            m_btnRun->setEnabled(true);
            if (m_btnTranslate) m_btnTranslate->setEnabled(true);
            m_btnStop->setEnabled(false);
        }
        return;
    }

    if (m_opMode == OpMode::AutoAssign) {
        m_opMode = OpMode::Idle;
        parseAutoAssign(m_autoBuffer);
        return;
    }

    /* ── Controller LLM completato ── */
    if (m_opMode == OpMode::PipelineControl) {
        m_opMode = OpMode::Pipeline;   /* ripristina prima di advance */

        /* Sostituisce il testo grezzo del controller con la bolla colorata */
        QString ctrlHtml = markdownToHtml(m_ctrlAccum.trimmed());
        QTextCursor selCtrl(m_log->document());
        selCtrl.setPosition(m_ctrlBlockStart);
        selCtrl.movePosition(QTextCursor::End, QTextCursor::KeepAnchor);
        selCtrl.removeSelectedText();
        selCtrl.insertHtml(buildControllerBubble(ctrlHtml));

        advancePipeline();
        return;
    }

    if (m_opMode == OpMode::Pipeline) {
        if (m_currentAgent < MAX_AGENTS)
            m_cfgDlg->enabledChk(m_currentAgent)->setStyleSheet(
                "QCheckBox { color: #4caf50; font-weight: bold; }"
                "QCheckBox::indicator:checked { background-color: #4caf50; border: 2px solid #388e3c; border-radius: 3px; }");

        m_currentAgent++;

        int total = 0, done = 0;
        for (int i = 0; i < MAX_AGENTS; i++) if (m_cfgDlg->enabledChk(i)->isChecked()) total++;
        for (int i = 0; i < m_currentAgent; i++) if (m_cfgDlg->enabledChk(i)->isChecked()) done++;
        int pct = (total > 0) ? qMin(done * 100 / total, 99) : 0;
        emit pipelineStatus(pct, QString("\xe2\x9c\x85  Agente %1/%2 completato  (%3%)")
            .arg(done).arg(qMin(total, m_maxShots)).arg(pct));

        /* Sostituisce l'indicatore streaming + testo grezzo con la bolla AI completa */
        QString rawResp;
        if (m_currentAgent > 0 && !m_agentOutputs.isEmpty()) {
            rawResp = m_agentOutputs[m_currentAgent - 1];
            /* Rimuove blocchi <think>...</think> (reasoning models: qwen3, deepseek-r1, qwq...)
               Il contenuto di thinking non deve essere mostrato né passato agli agenti successivi. */
            {
                QRegularExpression reTh("<think>[\\s\\S]*?</think>",
                                        QRegularExpression::CaseInsensitiveOption);
                rawResp.remove(reTh);
                rawResp = rawResp.trimmed();
                m_agentOutputs[m_currentAgent - 1] = rawResp;  /* aggiorna il contesto */
            }
            QString htmlContent = rawResp.isEmpty()
                ? "<p style='color:#6b7280;font-style:italic;margin:0;'>Nessun output.</p>"
                : markdownToHtml(rawResp);

            QTextCursor sel(m_log->document());
            sel.setPosition(m_agentBlockStart);
            sel.movePosition(QTextCursor::End, QTextCursor::KeepAnchor);
            sel.removeSelectedText();
            { int idx = m_bubbleIdx++; m_bubbleTexts[idx] = rawResp;
              sel.insertHtml(buildAgentBubble(m_currentAgentLabel,
                                             m_currentAgentModel,
                                             m_currentAgentTime,
                                             htmlContent, idx)); }
        }

        /* ── Tool Executor: estrae ed esegue codice Python, poi avvia il Controller ── */
        QString pyCode = extractPythonCode(rawResp);
        if (!pyCode.isEmpty()) {
            /* Corregge i bug tipici nei codici generati dall'AI */
            pyCode = _sanitizePyCode(pyCode);

            /* Scrivi il codice in un file temporaneo */
            QString tmpPath = QDir::tempPath() + "/prismalux_exec.py";
            { QFile f(tmpPath); if (f.open(QIODevice::WriteOnly|QIODevice::Text)) f.write(pyCode.toUtf8()); }

            m_executorOutput.clear();
            if (m_execProc) { m_execProc->kill(); m_execProc->deleteLater(); m_execProc = nullptr; }
            m_execProc = new QProcess(this);
            m_execProc->setProcessChannelMode(QProcess::MergedChannels);

            auto* tmr = new QElapsedTimer;
            tmr->start();

            connect(m_execProc,
                    QOverload<int,QProcess::ExitStatus>::of(&QProcess::finished),
                    this, [this, tmpPath, tmr](int exitCode, QProcess::ExitStatus) {
                double ms = tmr->elapsed();
                delete tmr;
                QString out = QString::fromUtf8(m_execProc->readAll());
                m_execProc->deleteLater();
                m_execProc = nullptr;

                /* ── Auto-install modulo mancante ────────────────────────────
                   Se l'errore e' ModuleNotFoundError, installa il pacchetto
                   con pip e riavvia l'esecuzione automaticamente (una volta). */
                static QRegularExpression reModule(
                    "ModuleNotFoundError: No module named '([^']+)'");
                auto mMatch = reModule.match(out);
                if (exitCode != 0 && mMatch.hasMatch()) {
                    const QString pkg = mMatch.captured(1).split('.').first();
                    const QString python = PrismaluxPaths::findPython();

                    QTextCursor logC(m_log->document());
                    logC.movePosition(QTextCursor::End);
                    logC.insertHtml(QString(
                        "<div style='color:#facc15;font-style:italic;margin:4px 0'>"
                        "\xf0\x9f\x93\xa6  Installo '%1' via pip...</div>").arg(pkg));
                    if (!m_userScrolled) m_log->ensureCursorVisible();

                    /* pip install in foreground (breve — nessun blocco UI perche'
                       eseguito in thread QProcess non bloccante) */
                    auto* pip = new QProcess(this);
                    pip->setProcessChannelMode(QProcess::MergedChannels);
                    connect(pip, QOverload<int,QProcess::ExitStatus>::of(&QProcess::finished),
                            this, [this, pip, tmpPath, pkg](int rc, QProcess::ExitStatus) {
                        const QString pipOut = QString::fromUtf8(pip->readAll()).trimmed();
                        pip->deleteLater();

                        QTextCursor logC2(m_log->document());
                        logC2.movePosition(QTextCursor::End);
                        if (rc == 0) {
                            logC2.insertHtml(QString(
                                "<div style='color:#4ade80;margin:4px 0'>"
                                "\xe2\x9c\x85  '%1' installato. Riprovo l'esecuzione...</div>").arg(pkg));
                            if (!m_userScrolled) m_log->ensureCursorVisible();

                            /* Riavvia il codice con il modulo ora disponibile */
                            auto* retry = new QProcess(this);
                            retry->setProcessChannelMode(QProcess::MergedChannels);
                            auto* t2 = new QElapsedTimer; t2->start();
                            connect(retry, QOverload<int,QProcess::ExitStatus>::of(&QProcess::finished),
                                    this, [this, retry, tmpPath, t2](int rc2, QProcess::ExitStatus) {
                                double ms2 = t2->elapsed(); delete t2;
                                QString out2 = QString::fromUtf8(retry->readAll());
                                retry->deleteLater();
                                QFile::remove(tmpPath);
                                m_executorOutput = out2;
                                QTextCursor c2(m_log->document());
                                c2.movePosition(QTextCursor::End);
                                c2.insertHtml(buildToolStrip(QString(), out2, rc2, ms2));
                                if (!m_userScrolled) m_log->ensureCursorVisible();
                                if (m_cfgDlg->controllerEnabled())
                                    runPipelineController();
                                else
                                    advancePipeline();
                            });
                            retry->start(PrismaluxPaths::findPython(), {tmpPath});
                        } else {
                            logC2.insertHtml(QString(
                                "<div style='color:#f87171;margin:4px 0'>"
                                "\xe2\x9d\x8c  pip install '%1' fallito. Installa manualmente:<br>"
                                "<code>pip install %1</code></div>").arg(pkg));
                            QFile::remove(tmpPath);
                            m_executorOutput = pipOut;
                            if (m_cfgDlg->controllerEnabled())
                                runPipelineController();
                            else
                                advancePipeline();
                        }
                    });
                    pip->start(PrismaluxPaths::findPython(), {"-m", "pip", "install", pkg,
                        "--quiet", "--trusted-host", "pypi.org",
                        "--trusted-host", "files.pythonhosted.org"});
                    return;  /* non rimuovere tmpPath: lo usa il retry */
                }
                /* ── fine auto-install ─────────────────────────────────────── */

                QFile::remove(tmpPath);
                m_executorOutput = out;

                /* Inserisce la tool strip nel log */
                QTextCursor c(m_log->document());
                c.movePosition(QTextCursor::End);
                c.insertHtml(buildToolStrip(QString(), out, exitCode, ms));
                if (!m_userScrolled) {
                    m_suppressScrollSig = true;
                    m_log->ensureCursorVisible();
                    m_suppressScrollSig = false;
                }

                /* Avvia il controller LLM (solo se checkbox abilitato) */
                if (m_cfgDlg->controllerEnabled())
                    runPipelineController();
                else
                    advancePipeline();
            });

            connect(m_execProc, &QProcess::errorOccurred,
                    this, [this, tmr](QProcess::ProcessError err){
                if (err == QProcess::FailedToStart) {
                    delete tmr;
                    m_execProc->deleteLater();
                    m_execProc = nullptr;
                    advancePipeline();   /* python3 non disponibile: salta executor */
                }
            });
            m_execProc->start(PrismaluxPaths::findPython(), {tmpPath});
        } else {
            /* Nessun codice trovato: avanza direttamente */
            advancePipeline();
        }
        return;
    }

    /* Matematico Teorico — strip think tags prima di usare l'output come contesto */
    if (m_opMode == OpMode::MathTheory) {
        {
            static const QRegularExpression reTh("<think>[\\s\\S]*?</think>",
                                                  QRegularExpression::CaseInsensitiveOption);
            if (m_byzStep == 0) { m_byzA.remove(reTh); m_byzA = m_byzA.trimmed(); }
            if (m_byzStep == 2) { m_byzC.remove(reTh); m_byzC = m_byzC.trimmed(); }
        }
        m_byzStep++;
        static const char* mathLabels[] = {
            "\xf0\x9f\x94\xad  [Agente 2 \xe2\x80\x94 Esploratore]\n",
            "\xf0\x9f\x93\x90  [Agente 3 \xe2\x80\x94 Dimostratore]\n",
            "\xe2\x9c\xa8  [Agente 4 \xe2\x80\x94 Sintetizzatore]\n",
        };
        if (m_byzStep <= 3) {
            m_log->append(QString("\n") + QString(43, QChar(0x2500)));
            m_log->append(mathLabels[m_byzStep - 1]);
        }
        QString ctx;
        switch (m_byzStep) {
            case 1:
                ctx = QString("Problema originale: %1\n\nFormulazione rigorosa:\n%2").arg(m_taskOriginal, m_byzA);
                m_ai->chat("Sei l'Esploratore matematico. Individua teoremi o metodi applicabili. "
                           "Sii conciso (max 150 parole). Rispondi SOLO in italiano.", ctx);
                break;
            case 2:
                ctx = QString("Problema: %1\n\nFormulazione rigorosa:\n%2").arg(m_taskOriginal, m_byzA);
                m_ai->chat("Sei il Dimostratore matematico. Fornisci la soluzione passo per passo. "
                           "Sii conciso (max 200 parole). Rispondi SOLO in italiano.", ctx);
                break;
            case 3:
                ctx = QString("Problema: %1\n\nSoluzione:\n%2").arg(m_taskOriginal, m_byzC);
                m_ai->chat("Sei il Sintetizzatore matematico. Riassumi il risultato finale. "
                           "Sii conciso (max 150 parole). Rispondi SOLO in italiano.", ctx);
                break;
            default:
                m_log->append("\n\n\xe2\x9c\x85  Esplorazione matematica completata.");
                m_btnRun->setEnabled(true);
                m_btnStop->setEnabled(false);
                m_opMode = OpMode::Idle;
                tryShowChart(m_taskOriginal + "\n" + m_byzA + "\n" + m_byzC);
                emit chatCompleted(m_taskOriginal.left(40), m_log->toHtml());
                break;
        }
        return;
    }

    /* Byzantino — strip think tags prima di usare l'output come contesto */
    {
        static const QRegularExpression reTh("<think>[\\s\\S]*?</think>",
                                              QRegularExpression::CaseInsensitiveOption);
        if (m_byzStep == 0) { m_byzA.remove(reTh); m_byzA = m_byzA.trimmed(); }
        if (m_byzStep == 2) { m_byzC.remove(reTh); m_byzC = m_byzC.trimmed(); }
    }
    m_byzStep++;
    static const char* labels[] = {
        "\xf0\x9f\x85\xb1  [Agente B \xe2\x80\x94 Avvocato del Diavolo]\n",
        "\xf0\x9f\x85\x92  [Agente C \xe2\x80\x94 Gemello Indipendente]\n",
        "\xf0\x9f\x85\x93  [Agente D \xe2\x80\x94 Giudice]\n",
    };
    if (m_byzStep <= 3) {
        m_log->append(QString("\n") + QString(43, QChar(0x2500)));
        m_log->append(labels[m_byzStep - 1]);
    }
    QString userMsg;
    switch (m_byzStep) {
        case 1:
            userMsg = QString("Domanda: %1\n\nRisposta A:\n%2").arg(m_taskOriginal, m_byzA);
            m_ai->chat(_buildSys(m_taskOriginal, QString(
                       "Sei l'Agente B del Motore Byzantino, l'Avvocato del Diavolo. "
                       "Cerca ATTIVAMENTE errori e contraddizioni nella risposta A. "
                       "Rispondi SEMPRE e SOLO in italiano."),
                       m_ai->model(), m_ai->backend()), userMsg);
            break;
        case 2:
            m_ai->chat(_buildSys(m_taskOriginal, QString(
                       "Sei l'Agente C del Motore Byzantino, il Gemello Indipendente. "
                       "Rispondi alla domanda originale da un angolo diverso. "
                       "Rispondi SEMPRE e SOLO in italiano."),
                       m_ai->model(), m_ai->backend()), m_taskOriginal);
            break;
        case 3:
            userMsg = QString("Domanda: %1\n\nRisposta A:\n%2\n\nRisposta C:\n%3")
                      .arg(m_taskOriginal, m_byzA, m_byzC);
            m_ai->chat(_buildSys(m_taskOriginal, QString(
                       "Sei l'Agente D del Motore Byzantino, il Giudice. "
                       "Se A e C concordano e B non trova errori validi: conferma. "
                       "Altrimenti segnala l'incertezza. Produci il verdetto finale. "
                       "Rispondi SEMPRE e SOLO in italiano."),
                       m_ai->model(), m_ai->backend()), userMsg);
            break;
        default:
            m_log->append("\n\n\xe2\x9c\x85  Verifica Byzantina completata.");
            m_log->append("\xe2\x9c\xa8 Verit\xc3\xa0 rivelata. Bevi la conoscenza.");
            m_btnRun->setEnabled(true);
            m_btnStop->setEnabled(false);
            m_opMode = OpMode::Idle;
            tryShowChart(m_taskOriginal + "\n" + m_byzA + "\n" + m_byzC);
            emit chatCompleted(m_taskOriginal.left(40), m_log->toHtml());
            break;
    }
}

/* ══════════════════════════════════════════════════════════════
   Consiglio Scientifico — query parallela multi-modello
   ══════════════════════════════════════════════════════════════ */
double AgentiPage::jaccardSim(const QString& a, const QString& b)
{
    QSet<QString> setA = QSet<QString>(a.split(' ', Qt::SkipEmptyParts).begin(),
                                       a.split(' ', Qt::SkipEmptyParts).end());
    QSet<QString> setB = QSet<QString>(b.split(' ', Qt::SkipEmptyParts).begin(),
                                       b.split(' ', Qt::SkipEmptyParts).end());
    if (setA.isEmpty() && setB.isEmpty()) return 1.0;
    QSet<QString> inter = setA;
    inter.intersect(setB);
    QSet<QString> uni = setA;
    uni.unite(setB);
    return uni.isEmpty() ? 0.0 : (double)inter.size() / (double)uni.size();
}

void AgentiPage::runConsiglioScientifico()
{
    QString task = m_input->toPlainText().trimmed();
    if (task.isEmpty()) {
        m_log->append("\xe2\x9a\xa0  Inserisci una domanda per il Consiglio Scientifico.");
        return;
    }
    if (m_modelInfos.isEmpty()) {
        m_log->append("\xe2\x9a\xa0  Nessun modello disponibile. Controlla la connessione al backend.");
        return;
    }

    if (!m_docContext.isEmpty()) task += "\n\n--- DOCUMENTO ALLEGATO ---\n" + m_docContext.left(8000);
    m_taskOriginal = task;

    /* Selezione strategia tramite dialog */
    {
        auto* dlg = new QDialog(this);
        dlg->setWindowTitle("\xf0\x9f\x8f\x9b  Consiglio Scientifico — Strategia");
        dlg->setFixedWidth(380);
        auto* lay = new QVBoxLayout(dlg);
        lay->addWidget(new QLabel("Scegli la strategia di aggregazione:", dlg));
        auto* cmb = new QComboBox(dlg);
        cmb->addItem("0  Aggregazione Pesata  (primaria + alternative)");
        cmb->addItem("1  Analisi Comparativa  (consenso Jaccard)");
        cmb->addItem("2  Sintesi AI  (un LLM sintetizza tutte le risposte)");
        cmb->setCurrentIndex(m_consiglioStrategy);
        lay->addWidget(cmb);
        auto* bb = new QDialogButtonBox(QDialogButtonBox::Ok | QDialogButtonBox::Cancel, dlg);
        lay->addWidget(bb);
        connect(bb, &QDialogButtonBox::accepted, dlg, &QDialog::accept);
        connect(bb, &QDialogButtonBox::rejected, dlg, &QDialog::reject);
        if (dlg->exec() != QDialog::Accepted) { dlg->deleteLater(); return; }
        m_consiglioStrategy = cmb->currentIndex();
        dlg->deleteLater();
    }

    /* Seleziona fino a 5 modelli */
    QVector<ModelInfo> selected;
    for (auto& mi : m_modelInfos) {
        selected.append(mi);
        if (selected.size() >= 5) break;
    }
    if (selected.isEmpty()) {
        m_log->append("\xe2\x9d\x8c  Nessun modello selezionabile.");
        return;
    }

    /* Libera eventuali peer precedenti */
    for (auto& p : m_peers) if (p.client) p.client->deleteLater();
    m_peers.clear();
    m_peersDone = 0;

    m_log->clear();
    m_log->append(QString("\xf0\x9f\x8f\x9b  <b>Consiglio Scientifico</b> — %1 modelli in parallelo\n")
                  .arg(selected.size()));
    m_log->append(QString("\xe2\x9d\x93  Domanda: <i>%1</i>\n").arg(task));
    m_log->append(QString(43, QChar(0x2500)));

    m_opMode = OpMode::ConsiglioScientifico;
    m_btnRun->setEnabled(false);
    m_btnStop->setEnabled(true);
    m_waitLbl->setText(QString("\xf0\x9f\x94\x84  %1 modelli in elaborazione...").arg(selected.size()));
    m_waitLbl->setVisible(true);

    const QString sys =
        "Sei un esperto scientifico del Consiglio Prismalux. "
        "Rispondi in modo preciso, strutturato e approfondito. "
        "Rispondi SEMPRE e SOLO in italiano.";

    for (const auto& mi : selected) {
        ConsiglioPeer peer;
        peer.client = new AiClient(this);
        peer.client->setBackend(m_ai->backend(), m_ai->host(), m_ai->port(), mi.name);
        peer.model  = mi.name;

        /* Peso basato sul nome del modello */
        QString nl = mi.name.toLower();
        if (nl.contains("math") || nl.contains("qwen"))        peer.weight = 1.4;
        else if (nl.contains("coder") || nl.contains("code"))  peer.weight = 1.3;
        else if (nl.contains("r1") || nl.contains("reason") || nl.contains("think")) peer.weight = 1.2;
        else                                                    peer.weight = 1.0;

        peer.done   = false;
        peer.accum  = "";

        int peerIdx = m_peers.size();
        m_log->append(QString("\xf0\x9f\xa4\x96  <b>%1</b>  [peso: %2]  \xf0\x9f\x94\x84 generando...")
                      .arg(mi.name).arg(peer.weight, 0, 'f', 1));

        /* Connetti token e finished */
        connect(peer.client, &AiClient::token, this, [this, peerIdx](const QString& t){
            if (peerIdx < m_peers.size()) m_peers[peerIdx].accum += t;
        });
        connect(peer.client, &AiClient::finished, this, [this, peerIdx](const QString&){
            if (peerIdx >= m_peers.size()) return;
            m_peers[peerIdx].done = true;
            m_peersDone++;
            m_log->append(QString("\xe2\x9c\x85  <b>%1</b> completato (%2/%3)")
                          .arg(m_peers[peerIdx].model)
                          .arg(m_peersDone)
                          .arg(m_peers.size()));
            if (m_peersDone >= m_peers.size()) aggregaConsiglio();
        });
        connect(peer.client, &AiClient::error, this, [this, peerIdx](const QString& err){
            if (peerIdx >= m_peers.size()) return;
            m_peers[peerIdx].done  = true;
            m_peers[peerIdx].accum = QString("[Errore: %1]").arg(err);
            m_peersDone++;
            if (m_peersDone >= m_peers.size()) aggregaConsiglio();
        });

        m_peers.append(peer);
    }

    /* Avvia tutte le chat in parallelo — adatta sys al modello specifico del peer */
    for (auto& p : m_peers)
        p.client->chat(_buildSys(task, sys, p.model, m_ai->backend()), task);
}

void AgentiPage::aggregaConsiglio()
{
    m_waitLbl->setVisible(false);
    m_btnRun->setEnabled(true);
    m_btnStop->setEnabled(false);
    m_opMode = OpMode::Idle;

    m_log->append("\n" + QString(43, QChar(0x2500)));
    m_log->append("\xf0\x9f\x8f\x9b  <b>Aggregazione Consiglio Scientifico</b>");

    if (m_consiglioStrategy == 0) {
        /* Strategia 0: Aggregazione Pesata */
        m_log->append("\n\xf0\x9f\x94\xac  <b>Strategia: Aggregazione Pesata</b>");
        /* Mostra la risposta con peso maggiore come primaria */
        int bestIdx = 0;
        for (int i = 1; i < m_peers.size(); i++)
            if (m_peers[i].weight > m_peers[bestIdx].weight) bestIdx = i;
        QString html = markdownToHtml(m_peers[bestIdx].accum.trimmed());
        { int idx = m_bubbleIdx++; m_bubbleTexts[idx] = m_peers[bestIdx].accum.trimmed();
          m_log->insertHtml(buildAgentBubble(
              "\xf0\x9f\x8f\x9b  Risposta Primaria",
              m_peers[bestIdx].model,
              QTime::currentTime().toString("HH:mm:ss"),
              html, idx)); }
        m_log->append("");
        /* Alternative */
        for (int i = 0; i < m_peers.size(); i++) {
            if (i == bestIdx) continue;
            m_log->append(QString("\n\xf0\x9f\x94\x97  <b>%1</b> (peso %2):")
                          .arg(m_peers[i].model).arg(m_peers[i].weight, 0, 'f', 1));
            m_log->append(m_peers[i].accum.trimmed().left(500) +
                          (m_peers[i].accum.length() > 500 ? "..." : ""));
        }
    } else if (m_consiglioStrategy == 1) {
        /* Strategia 1: Analisi Comparativa (Jaccard) */
        m_log->append("\n\xf0\x9f\x94\xac  <b>Strategia: Analisi Comparativa (consenso Jaccard)</b>");
        /* Calcola similarità pairwise, trova il peer con Jaccard medio più alto */
        int best = 0; double bestScore = -1.0;
        for (int i = 0; i < m_peers.size(); i++) {
            double sum = 0.0;
            for (int j = 0; j < m_peers.size(); j++) {
                if (i == j) continue;
                sum += jaccardSim(m_peers[i].accum, m_peers[j].accum);
            }
            double avg = m_peers.size() > 1 ? sum / (m_peers.size() - 1) : 0.0;
            if (avg > bestScore) { bestScore = avg; best = i; }
        }
        m_log->append(QString("\xf0\x9f\x93\x8a  Risposta con consenso massimo: <b>%1</b> (Jaccard medio: %2)")
                      .arg(m_peers[best].model).arg(bestScore, 0, 'f', 3));
        QString html = markdownToHtml(m_peers[best].accum.trimmed());
        { int idx = m_bubbleIdx++; m_bubbleTexts[idx] = m_peers[best].accum.trimmed();
          m_log->insertHtml(buildAgentBubble(
              "\xf0\x9f\x8f\x9b  Risposta Consenso",
              m_peers[best].model,
              QTime::currentTime().toString("HH:mm:ss"),
              html, idx)); }
        m_log->append("");
    } else {
        /* Strategia 2: Sintesi tramite LLM principale */
        m_log->append("\n\xf0\x9f\x94\xac  <b>Strategia: Sintesi AI</b> — invio all'AI per sintesi...");

        QString combined = QString("Domanda originale: %1\n\n").arg(m_taskOriginal);
        for (int i = 0; i < m_peers.size(); i++) {
            combined += QString("=== Risposta da %1 ===\n%2\n\n")
                        .arg(m_peers[i].model)
                        .arg(m_peers[i].accum.trimmed().left(2000));
        }

        const QString sysSintesi =
            "Sei un sintetizzatore scientifico del Consiglio Prismalux. "
            "Hai ricevuto le risposte di %1 modelli AI alla stessa domanda. "
            "Crea una sintesi coerente, evidenziando:\n"
            "1. I punti di accordo\n"
            "2. Le divergenze significative\n"
            "3. La risposta più completa e accurata\n"
            "Rispondi SEMPRE e SOLO in italiano.";

        m_opMode = OpMode::Pipeline;  /* usa il flusso normale per streaming */
        m_agentOutputs.clear();
        m_agentOutputs.append("");
        m_currentAgent = 0;
        m_currentAgentLabel = "\xf0\x9f\x8f\x9b  Sintetizzatore Consiglio";
        m_currentAgentModel = m_ai->model();
        m_currentAgentTime  = QTime::currentTime().toString("HH:mm:ss");
        {
            QTextCursor c(m_log->document());
            c.movePosition(QTextCursor::End);
            m_agentBlockStart = c.position();
        }
        m_log->append("\n\xf0\x9f\x8f\x9b  [Sintetizzatore] \xf0\x9f\x94\x84 generando...\n");
        m_agentTextStart = m_log->document()->characterCount() - 1;
        m_btnRun->setEnabled(false);
        m_btnStop->setEnabled(true);
        m_waitLbl->setVisible(true);
        m_ai->chat(_buildSys(m_taskOriginal, QString(sysSintesi).arg(m_peers.size()),
                             m_ai->model(), m_ai->backend()), combined);
    }

    /* Pulizia peer */
    for (auto& p : m_peers) if (p.client) { p.client->deleteLater(); p.client = nullptr; }
    m_peers.clear();
    m_peersDone = 0;

    emit chatCompleted(m_taskOriginal.left(40), m_log->toHtml());
}

/* ── _categorizeError ─────────────────────────────────────────────────────────
 * Classifica l'errore e restituisce un messaggio localizzato con suggerimento.
 * Categorie (in ordine di priorità):
 *   ram       → RAM critica (già nel messaggio, nessun hint aggiuntivo)
 *   abort     → operazione annullata (silenzioso)
 *   network   → Connection refused / ECONNREFUSED
 *   closed    → RemoteHostClosed (crash server o VRAM esaurita)
 *   timeout   → socket timeout / nessuna risposta
 *   json      → risposta malformata / JSON non valido
 *   other     → errore generico con hint backend
 * ──────────────────────────────────────────────────────────────────────────── */
static QString _categorizeError(const QString& msg, AiClient::Backend backend) {
    const QString ml = msg.toLower();

    /* RAM critica — il messaggio è già autoesplicativo */
    if (ml.contains("ram critica")) return msg;

    /* Operazione annullata dall'utente o dal sistema — nessun output */
    if (ml.contains("operation canceled") || ml.contains("operazione annullata") ||
        ml.contains("canceled") || ml.contains("aborted"))
        return QString();   /* stringa vuota = silenzioso */

    /* Connessione rifiutata: backend non avviato */
    if (ml.contains("connection refused") || ml.contains("econnrefused") ||
        ml.contains("refused") || ml.contains("could not connect")) {
        return (backend == AiClient::Ollama)
            ? "\xe2\x9d\x8c  Backend non raggiungibile. Avvia Ollama: <code>ollama serve</code>"
            : "\xe2\x9d\x8c  Backend non raggiungibile. Avvia llama-server dalla tab \xf0\x9f\x94\x8c Backend.";
    }

    /* Connessione chiusa improvvisamente: crash del server o VRAM esaurita */
    if (ml.contains("remotehostclosed") || ml.contains("remote host closed") ||
        ml.contains("connection reset") || ml.contains("broken pipe")) {
        return "\xe2\x9a\xa0  Connessione chiusa improvvisamente.\n"
               "\xf0\x9f\x92\xbe  Possibili cause: VRAM esaurita, crash del server, modello troppo grande.";
    }

    /* Timeout: modello lento o in caricamento */
    if (ml.contains("timed out") || ml.contains("timeout") || ml.contains("socket timeout")) {
        return "\xe2\x8f\xb1  Timeout: nessuna risposta dal backend.\n"
               "\xf0\x9f\xa4\x94  Il modello potrebbe essere in caricamento o troppo grande per la RAM disponibile.";
    }

    /* Risposta JSON malformata */
    if (ml.contains("json") || ml.contains("parse") || ml.contains("malform") ||
        ml.contains("invalid content")) {
        return "\xe2\x9a\xa0  Risposta malformata dal backend. Riprova.";
    }

    /* Errore generico con hint backend */
    QString out = "\xe2\x9d\x8c  " + msg;
    out += (backend == AiClient::Ollama)
        ? "\n\xf0\x9f\x8c\xab  La nebbia copre la verit\xc3\xa0. Controlla che Ollama sia in esecuzione: <code>ollama serve</code>"
        : "\n\xf0\x9f\x8c\xab  La nebbia copre la verit\xc3\xa0. Controlla che llama-server sia avviato.";
    return out;
}

void AgentiPage::onError(const QString& msg) {
    m_waitLbl->setVisible(false);
    if (m_opMode == OpMode::AutoAssign) {
        m_autoLbl->setText(QString("\xe2\x9d\x8c  Errore auto-assegnazione: %1").arg(msg));
        m_btnAuto->setEnabled(true);
        m_opMode = OpMode::Idle;
        return;
    }

    const QString categorized = _categorizeError(msg, m_ai->backend());
    if (!categorized.isEmpty())
        m_log->append("\n" + categorized);
    /* Se categorized è vuota (abort silenzioso) non mostriamo nulla */

    m_btnRun->setEnabled(true);
    m_btnStop->setEnabled(false);
    m_opMode = OpMode::Idle;
    m_waitLbl->setVisible(false);
}

/* ══════════════════════════════════════════════════════════════
   tryShowChart — rileva formula o coppie di punti nel testo AI
   e mostra il grafico nel pannello inferiore della pagina.
   Usa FormulaParser::tryExtract (pattern "y=..." o "f(x)=...")
   e FormulaParser::tryExtractPoints (coppie coordinate).
   ══════════════════════════════════════════════════════════════ */
void AgentiPage::tryShowChart(const QString& text) {
    if (!m_chartPanel) return;

    /* Rimuovi ChartWidget precedente */
    const auto oldCharts = m_chartPanel->findChildren<ChartWidget*>();
    for (auto* c : oldCharts) { c->setParent(nullptr); c->deleteLater(); }

    /* Intervallo x: cerca nel testo (es. "da x=-5 a x=5", "x ∈ [-10,10]") */
    double xMin = -10.0, xMax = 10.0;
    FormulaParser::tryExtractXRange(text, xMin, xMax);

    /* 1. Cerca una formula "y = expr", "f(x) = expr" o "grafico di expr" */
    const QString expr = FormulaParser::tryExtract(text);
    if (!expr.isEmpty()) {
        FormulaParser fp(expr);
        if (fp.ok()) {
            auto pts = fp.sample(xMin, xMax, 400);
            if (!pts.isEmpty()) {
                auto* chart = new ChartWidget(m_chartPanel);
                chart->setTitle(expr);
                chart->setAxisLabels("x", "y");
                ChartWidget::Series s;
                s.name = expr;
                s.pts  = pts;
                chart->addSeries(s);
                m_chartPanel->layout()->addWidget(chart);
                m_chartPanel->setVisible(true);
                return;
            }
        }
    }

    /* 2. Cerca coppie di coordinate "(x, y)" nel testo */
    const auto pts = FormulaParser::tryExtractPoints(text);
    if (pts.size() >= 2) {
        auto* chart = new ChartWidget(m_chartPanel);
        chart->setAxisLabels("x", "y");
        ChartWidget::Series s;
        s.name = "Punti";
        s.pts  = pts;
        s.mode = ChartWidget::Scatter;
        chart->addSeries(s);
        m_chartPanel->layout()->addWidget(chart);
        m_chartPanel->setVisible(true);
    }
}

