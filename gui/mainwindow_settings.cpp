/* ══════════════════════════════════════════════════════════════
   mainwindow_settings.cpp — MainWindow: dialog Impostazioni + Log + stile tab
   ============================================================================
   Costruzione lazy dei dialog Impostazioni/Log, applicazione live delle
   preferenze (etichette tab, pulsanti esecuzione, stile navigazione).
   Split da mainwindow.cpp (TODO D-8).
   ══════════════════════════════════════════════════════════════ */
#include "mainwindow.h"
#include "prismalux_paths.h"
#include "dpi_utils.h"
#include "pages/settings_main.h"
#include "pages/main_maintenance.h"
#include "pages/main_ai.h"
#include "pages/main_research.h"
#include "theme_manager.h"

#include <QSettings>
#include <QTimer>
#include <QDialog>
#include <QVBoxLayout>
#include <QHBoxLayout>
#include <QLabel>
#include <QTabWidget>
#include <QTextEdit>
#include <QTextCursor>
#include <QPushButton>
#include <QFont>
#include <QDateTime>
#include <QRegularExpression>
#include <QVariant>

namespace P = PrismaluxPaths;

/* ── Livello 2: applica stile nav e modalità exec btn da QSettings ── */
void MainWindow::applyContentSettings()
{
    QSettings s("Prismalux", "GUI");
    applyNavStyle(s.value(P::SK::kNavStyle, "tabs_top").toString());
    const QString execMode = s.value(P::SK::kNavExecBtnMode, "icon_text").toString();
    if (execMode != "icon_text") {
        m_pendingExecMode = execMode;
        QTimer::singleShot(0, this, &MainWindow::onApplyExecBtnMode);
    }
}

/* ══════════════════════════════════════════════════════════════
   ensureSettingsDialog — crea il dialog Impostazioni la prima volta (lazy).
   Sicuro da chiamare più volte (no-op se già creato).
   IMPORTANTE: nessun Qt::Window — evita il crash Windows nella
   gestione parent-child quando QDialog ha sia Qt::Window che un parent.
   ══════════════════════════════════════════════════════════════ */
void MainWindow::ensureSettingsDialog()
{
    if (m_impDlg) return;
    m_impDlg = new QDialog(this);
    m_impDlg->setWindowTitle("\xe2\x9a\x99\xef\xb8\x8f  Impostazioni \xe2\x80\x94 Prismalux");
    /* NO Qt::Window flag — QDialog default flags funzionano correttamente
       su tutte le piattaforme senza scatenare bug Windows API parent-child */
    m_impDlg->setAttribute(Qt::WA_DeleteOnClose, false);
    m_impDlg->resize(dpiScale(1050), dpiScale(680));
    m_impPage = new ImpostazioniPage(m_ai, m_hw, m_impDlg);
    m_impPage->setGraficoCanvas(m_grafCanvas);
    /* installCronPanel è chiamata solo da onCronPanelFirstOpen (primo clic su Cron) */
    auto* dl = new QVBoxLayout(m_impDlg);
    dl->setContentsMargins(0, 0, 0, 0);
    dl->addWidget(m_impPage);
    if (m_hw && m_hw->hwReady())
        m_impPage->onHWReady(m_hw->hwInfo());
    connect(m_impPage, &ImpostazioniPage::tabModeChanged,
            this,      &MainWindow::applyTabMode);
    connect(m_impPage, &ImpostazioniPage::navStyleChanged,
            this,      &MainWindow::applyNavStyle);
    connect(m_impPage, &ImpostazioniPage::execBtnModeChanged,
            this,      &MainWindow::applyExecBtnMode);
    if (auto* ap = findChild<AgentiPage*>()) {
        connect(m_impPage, &ImpostazioniPage::bubbleStyleChanged,
                ap, &AgentiPage::onBubbleStyleChanged);
        /* Ricolora le bolle esistenti ogni volta che l'utente cambia tema */
        connect(ThemeManager::instance(), &ThemeManager::changed,
                ap, &AgentiPage::recolorLog);
    }

    /* Feedback indicizzazione RAG nella status bar — visibile anche a dialog chiuso */
    connect(m_impPage, &ImpostazioniPage::indexingProgress,
            this, &MainWindow::onIndexingProgress);
    connect(m_impPage, &ImpostazioniPage::indexingFinished,
            this, &MainWindow::onIndexingFinished);

    /* Auto-trigger RagGraph dopo reindicizzazione RAG */
    if (m_ricercaPage)
        connect(m_impPage, &ImpostazioniPage::indexingFinished,
                m_ricercaPage, &RicercaPage::onAutoRagTrigger);

    /* ── Indicatore download LLM — visibile da qualsiasi tab ── */
    auto* man = m_impPage->manutenzione();
    if (man) {
        connect(man, &ManutenzioneePage::downloadStarted,
                this, [this](const QString& model) {
            if (m_dlStatusLbl) {
                m_dlStatusLbl->setText(
                    "\xe2\xac\x87 " + model + "  \xe2\x8f\xb3");
                m_dlStatusLbl->setVisible(true);
            }
        }, Qt::QueuedConnection);

        connect(man, &ManutenzioneePage::downloadProgress,
                this, [this](const QString& line) {
            if (!m_dlStatusLbl || !m_dlStatusLbl->isVisible()) return;
            /* Mostra solo testo utile: taglia ANSI e righe troppo lunghe */
            QString clean = line;
            clean.remove(QRegularExpression("\x1b\\[[0-9;]*[A-Za-z]"));
            clean.remove('\r');
            clean = clean.trimmed().left(60);
            if (!clean.isEmpty())
                m_dlStatusLbl->setText("\xe2\xac\x87 " + clean + "  \xe2\x8f\xb3");
        }, Qt::QueuedConnection);

        connect(man, &ManutenzioneePage::downloadFinished,
                this, [this](bool ok, const QString& model) {
            if (!m_dlStatusLbl) return;
            if (ok) {
                m_dlStatusLbl->setText("\xe2\x9c\x85 " + model + " scaricato");
                /* Nasconde la label dopo 5 secondi */
                QTimer::singleShot(5000, m_dlStatusLbl, [this]{
                    if (m_dlStatusLbl) m_dlStatusLbl->setVisible(false);
                });
            } else {
                m_dlStatusLbl->setText("\xe2\x9d\x8c Download " + model + " fallito");
                QTimer::singleShot(8000, m_dlStatusLbl, [this]{
                    if (m_dlStatusLbl) m_dlStatusLbl->setVisible(false);
                });
            }
        }, Qt::QueuedConnection);
    }
}

/* ══════════════════════════════════════════════════════════════
   openSettingsDialog — apre Impostazioni (invocabile da AiErrorWidget).
   ══════════════════════════════════════════════════════════════ */
void MainWindow::openSettingsDialog()
{
    ensureSettingsDialog();
    m_impDlg->show();
    m_impDlg->raise();
    m_impDlg->activateWindow();
}

/* ══════════════════════════════════════════════════════════════
   ensureLogDialog — dialog Messaggi/Log (creato lazy, non-modale).
   ══════════════════════════════════════════════════════════════ */
void MainWindow::ensureLogDialog()
{
    if (m_logDlg) return;

    m_logDlg = new QDialog(this);
    m_logDlg->setWindowTitle("\xf0\x9f\x93\x8b  Messaggi \xe2\x80\x94 Prismalux");
    m_logDlg->setAttribute(Qt::WA_DeleteOnClose, false);
    m_logDlg->resize(dpiScale(720), dpiScale(480));

    auto* lay = new QVBoxLayout(m_logDlg);
    lay->setContentsMargins(12, 12, 12, 12);
    lay->setSpacing(8);

    auto* header = new QLabel(
        "\xf0\x9f\x93\x8b  <b>Log eventi</b> \xe2\x80\x94 backend, AI, pipeline, errori");
    header->setTextFormat(Qt::RichText);
    header->setObjectName("sectionTitle");
    lay->addWidget(header);

    /* ── Tab Sistema / AI ── */
    m_logTabs = new QTabWidget(m_logDlg);
    m_logTabs->setObjectName("logTabWidget");

    auto makeView = [&](const QString& placeholder) -> QTextEdit* {
        auto* v = new QTextEdit(m_logDlg);
        v->setReadOnly(true);
        v->setObjectName("chatLog");
        v->setPlaceholderText(placeholder);
        v->setFont(QFont("Monospace", 9));
        return v;
    };

    m_logViewSis = makeView("Nessun evento di sistema. Backend, server, Qt, ONNX.");
    m_logViewAI  = makeView("Nessun evento AI. Pipeline, inferenza, RAG, embedding.");

    m_logTabs->addTab(m_logViewSis, "\xf0\x9f\x96\xa5  Sistema");   /* 🖥 */
    m_logTabs->addTab(m_logViewAI,  "\xf0\x9f\xa4\x96  AI");         /* 🤖 */
    lay->addWidget(m_logTabs, 1);

    /* Pulsanti */
    auto* btnRow = new QWidget(m_logDlg);
    auto* btnLay = new QHBoxLayout(btnRow);
    btnLay->setContentsMargins(0, 0, 0, 0);
    btnLay->setSpacing(8);

    auto* clearBtn = new QPushButton("\xf0\x9f\x97\x91  Pulisci log", btnRow);
    clearBtn->setObjectName("actionBtn");
    clearBtn->setFixedHeight(dpiScale(32));
    connect(clearBtn, &QPushButton::clicked, this, &MainWindow::onClearLogClicked);

    auto* closeBtn = new QPushButton("Chiudi", btnRow);
    closeBtn->setObjectName("actionBtn");
    closeBtn->setFixedHeight(dpiScale(32));
    connect(closeBtn, &QPushButton::clicked, m_logDlg, &QDialog::hide);

    btnLay->addStretch();
    btnLay->addWidget(clearBtn);
    btnLay->addWidget(closeBtn);
    lay->addWidget(btnRow);
}

/* ══════════════════════════════════════════════════════════════
   appendLog — aggiunge una riga al log con timestamp.
   Incrementa il badge se il dialog è nascosto.
   ══════════════════════════════════════════════════════════════ */
void MainWindow::appendLog(const QString& msg, LogCategory cat)
{
    ensureLogDialog();

    const QString ts   = QDateTime::currentDateTime().toString("HH:mm:ss");
    const QString line = QString("<span style='color:#888;'>%1</span> &nbsp;%2")
                         .arg(ts, msg);

    QTextEdit* view = (cat == LogAI) ? m_logViewAI : m_logViewSis;
    view->moveCursor(QTextCursor::End);
    view->insertHtml(line + "<br>");

    /* Badge non-letti — visibile solo se il dialog è chiuso */
    if (!m_logDlg->isVisible()) {
        m_logUnread++;
        const int cap = qMin(m_logUnread, 99);
        m_logBadge->setText(cap < 99 ? QString::number(cap) : "99+");
        m_logBadge->setVisible(true);
    }
}

/* ══════════════════════════════════════════════════════════════
   applyTabMode — aggiorna le etichette di m_mainTabs in tempo reale.
   Formato originale: "icona  testo" (separatore = 2 spazi).
   ══════════════════════════════════════════════════════════════ */
void MainWindow::applyTabMode(const QString& mode)
{
    if (!m_mainTabs || m_tabOrigLabels.isEmpty()) return;
    const int n = qMin(m_mainTabs->count(), m_tabOrigLabels.size());
    for (int i = 0; i < n; i++) {
        const QString& orig = m_tabOrigLabels.at(i);
        const int sep = orig.indexOf("  ");   /* 2 spazi tra icona e testo */
        if (sep < 0) { m_mainTabs->setTabText(i, orig); continue; }
        const QString icon = orig.left(sep);
        const QString text = orig.mid(sep + 2);
        QString label;
        if      (mode == "icons_only") label = icon;
        else if (mode == "text_icons") label = text + "  " + icon;
        else if (mode == "text_only")  label = text;
        else                           label = orig;  /* icons_text = default */
        m_mainTabs->setTabText(i, label);
    }
}

/* ══════════════════════════════════════════════════════════════
   applyExecBtnMode — aggiorna il testo di tutti i pulsanti di esecuzione.
   Scansiona l'albero dei widget cercando QPushButton con proprietà execIcon.
   ══════════════════════════════════════════════════════════════ */
void MainWindow::applyExecBtnMode(const QString& mode)
{
    const auto btns = findChildren<QPushButton*>();
    for (auto* btn : btns) {
        const QVariant iconVar = btn->property("execIcon");
        if (!iconVar.isValid() || iconVar.isNull()) continue;
        const QString icon = iconVar.toString();
        const QString text = btn->property("execText").toString();
        const QString full = btn->property("execFull").toString();
        if (mode == "icon_only") btn->setText(icon);
        else if (mode == "text_only") btn->setText(text);
        else btn->setText(full.isEmpty() ? icon + "  " + text : full);
    }
}

/* ══════════════════════════════════════════════════════════════
   applyNavStyle — alterna tra schede in alto e menù orizzontale.
   ══════════════════════════════════════════════════════════════ */
void MainWindow::applyNavStyle(const QString& style)
{
    if (!m_mainTabs) return;
    const bool isMenu = (style == "menu_main");
    m_mainTabs->tabBar()->setVisible(!isMenu);
    if (m_navMenuBar) m_navMenuBar->setVisible(isMenu);
}
