/* ══════════════════════════════════════════════════════════════
   main_app_controller_telegram.cpp — AppControllerPage: Bot Telegram
   ======================================================================
   Bot Telegram locale via python-telegram-bot — builder + slot
   (avvio/stop, log runtime, contatti promozionali). Split da
   main_app_controller.cpp/main_app_controller_slots.cpp (TODO D-8).
   ══════════════════════════════════════════════════════════════ */
#include "main_app_controller.h"
#include "../prismalux_paths.h"
#include "../log_bus.h"
#include "../dpi_utils.h"
#include "../lan_server.h"

#include <QVBoxLayout>
#include <QHBoxLayout>
#include <QGroupBox>
#include <QLabel>
#include <QLineEdit>
#include <QCheckBox>
#include <QListWidget>
#include <QPushButton>
#include <QTextBrowser>
#include <QTextEdit>
#include <QFrame>
#include <QFileInfo>
#include <QScrollArea>
#include <QProcess>
#include <QProcessEnvironment>
#include <QNetworkAccessManager>
#include <QNetworkReply>
#include <QNetworkRequest>
#include <QJsonDocument>
#include <QJsonObject>
#include <QRegularExpression>

namespace P = PrismaluxPaths;

/* ══════════════════════════════════════════════════════════════
   buildTelegramTab — bot Telegram locale via python-telegram-bot
   ══════════════════════════════════════════════════════════════ */
QWidget* AppControllerPage::buildTelegramTab()
{
    /* Scroll area esterna — la sezione è alta */
    auto* scroll = new QScrollArea;
    scroll->setWidgetResizable(true);
    scroll->setFrameShape(QFrame::NoFrame);

    auto* w   = new QWidget;
    auto* lay = new QVBoxLayout(w);
    lay->setContentsMargins(dpiScale(12), dpiScale(12),
                            dpiScale(12), dpiScale(12));
    lay->setSpacing(dpiScale(10));

    /* ── Intestazione ── */
    auto* descLbl = new QLabel(
        "\xf0\x9f\x93\xac  <b>Bot Telegram locale</b> \xe2\x80\x94 "
        "Prismalux risponde ai tuoi messaggi Telegram usando Ollama. "
        "Il token <b>non lascia mai il tuo PC</b>.", w);
    descLbl->setObjectName("hintLabel");
    descLbl->setTextFormat(Qt::RichText);
    descLbl->setWordWrap(true);
    lay->addWidget(descLbl);

    /* ── Banner python-telegram-bot mancante (async) ── */
    {
        auto* banner    = new QWidget(w);
        m_telegramModuleBanner = banner;
        auto* bannerLay = new QHBoxLayout(banner);
        bannerLay->setContentsMargins(8, 6, 8, 6);
        bannerLay->setSpacing(10);
        banner->setStyleSheet(
            "background:#78350f;border-radius:6px;border:1px solid #f59e0b;");
        banner->hide();

        auto* warnLbl = new QLabel(
            "\xe2\x9a\xa0  Modulo <b>python-telegram-bot</b> non installato \xe2\x80\x94 "
            "il bot non pu\xc3\xb2 avviarsi.", banner);
        warnLbl->setObjectName("hintLabel");
        warnLbl->setStyleSheet("color:#fcd34d;background:transparent;border:none;");
        warnLbl->setWordWrap(true);

        auto* goBtn = new QPushButton(
            tr("\xe2\x9a\x99\xef\xb8\x8f  Installa in Impostazioni"), banner);
        goBtn->setObjectName("actionBtn");
        goBtn->setFixedWidth(dpiScale(200));

        bannerLay->addWidget(warnLbl, 1);
        bannerLay->addWidget(goBtn);
        lay->addWidget(banner);

        QObject::connect(goBtn, &QPushButton::clicked, this,
            [this]() { emit openSettingsDipendenze("python-telegram-bot"); });

        auto* chk = new QProcess(w);
        QObject::connect(chk, QOverload<int,QProcess::ExitStatus>::of(&QProcess::finished),
            banner, [banner](int code, QProcess::ExitStatus) {
                if (code != 0) banner->show();
            });
        chk->start("python3", {"-c", "from telegram.ext import Application"});
    }

    /* ════════════════════════════════════════════════════════
       PASSO 1 — Token del bot
       ════════════════════════════════════════════════════════ */
    auto* step1 = new QGroupBox(
        tr("\xf0\x9f\x94\x91  Passo 1 \xe2\x80\x94 Token del bot"), w);
    auto* s1Lay = new QVBoxLayout(step1);
    s1Lay->setSpacing(dpiScale(6));

    auto* step1Hint = new QLabel(
        "Il token identifica il tuo bot presso Telegram. "
        "Ottienilo cos\xc3\xac:\n"
        "  1. Apri Telegram e cerca <b>@BotFather</b>\n"
        "  2. Scrivi <code>/newbot</code> e segui le istruzioni\n"
        "  3. Copia il token (formato: <code>7123456789:AAF-xxxx</code>)",
        step1);
    step1Hint->setObjectName("hintLabel");
    step1Hint->setTextFormat(Qt::RichText);
    step1Hint->setWordWrap(true);
    s1Lay->addWidget(step1Hint);

    auto* tokenRow = new QHBoxLayout;
    auto* tokenLbl = new QLabel(tr("Token:"), step1);
    tokenLbl->setFixedWidth(dpiScale(60));
    m_telegramTokenEdit = new QLineEdit(step1);
    m_telegramTokenEdit->setPlaceholderText(tr("7123456789:AAF-DEFxxx..."));
    m_telegramTokenEdit->setEchoMode(QLineEdit::Password);
    auto* eyeBtn = new QPushButton("\xf0\x9f\x91\x81", step1);   /* 👁 */
    eyeBtn->setToolTip(tr("Mostra / nascondi token"));
    eyeBtn->setFixedWidth(dpiScale(36));
    eyeBtn->setCheckable(true);
    auto* saveTokenBtn = new QPushButton(
        tr("\xf0\x9f\x92\xbe  Salva"), step1);
    saveTokenBtn->setObjectName("actionBtn");
    saveTokenBtn->setFixedWidth(dpiScale(80));
    tokenRow->addWidget(tokenLbl);
    tokenRow->addWidget(m_telegramTokenEdit, 1);
    tokenRow->addWidget(eyeBtn);
    tokenRow->addWidget(saveTokenBtn);
    s1Lay->addLayout(tokenRow);
    lay->addWidget(step1);

    /* ════════════════════════════════════════════════════════
       PASSO 2 — Chi può scrivere al bot (sicurezza)
       ════════════════════════════════════════════════════════ */
    auto* step2 = new QGroupBox(
        tr("\xf0\x9f\x9b\xa1  Passo 2 \xe2\x80\x94 Chi pu\xc3\xb2 scrivere al bot"), w);
    auto* s2Lay = new QVBoxLayout(step2);
    s2Lay->setSpacing(dpiScale(6));

    auto* step2Hint = new QLabel(
        "Inserisci gli <b>ID numerici</b> Telegram delle persone autorizzate, "
        "separati da virgola.<br>"
        "<b>Se lasci vuoto</b>, il bot risponde a chiunque gli scriva.<br>"
        "Per trovare il tuo ID: apri Telegram e scrivi a "
        "<b>@userinfobot</b> \xe2\x80\x94 risponde con il tuo ID numerico.",
        step2);
    step2Hint->setObjectName("hintLabel");
    step2Hint->setTextFormat(Qt::RichText);
    step2Hint->setWordWrap(true);
    s2Lay->addWidget(step2Hint);

    auto* wlRow = new QHBoxLayout;
    auto* wlLbl = new QLabel(tr("ID autorizzati:"), step2);
    wlLbl->setFixedWidth(dpiScale(100));
    m_telegramWhitelistEdit = new QLineEdit(step2);
    m_telegramWhitelistEdit->setPlaceholderText(
        tr("123456789, 987654321  \xe2\x80\x94 vuoto = tutti"));
    wlRow->addWidget(wlLbl);
    wlRow->addWidget(m_telegramWhitelistEdit, 1);
    s2Lay->addLayout(wlRow);
    lay->addWidget(step2);

    /* ════════════════════════════════════════════════════════
       Controllo avvia / ferma
       ════════════════════════════════════════════════════════ */
    auto* ctrlGroup = new QGroupBox(
        tr("\xe2\x9a\x99\xef\xb8\x8f  Controllo bot"), w);
    auto* ctrlLay = new QHBoxLayout(ctrlGroup);
    ctrlLay->setSpacing(dpiScale(8));

    m_telegramStartBtn = new QPushButton(
        tr("\xe2\x96\xb6  Avvia Bot"), ctrlGroup);
    m_telegramStartBtn->setObjectName("actionBtn");
    m_telegramStartBtn->setFixedWidth(dpiScale(120));

    m_telegramStopBtn = new QPushButton(
        tr("\xe2\x8f\xb9  Ferma Bot"), ctrlGroup);
    m_telegramStopBtn->setObjectName("actionBtn");
    m_telegramStopBtn->setFixedWidth(dpiScale(120));
    m_telegramStopBtn->setEnabled(false);

    m_telegramStatusLbl = new QLabel(
        tr("\xe2\x9a\xaa  Bot fermo"), ctrlGroup);
    m_telegramStatusLbl->setObjectName("statusLabel");

    ctrlLay->addWidget(m_telegramStartBtn);
    ctrlLay->addWidget(m_telegramStopBtn);
    ctrlLay->addWidget(m_telegramStatusLbl, 1);
    lay->addWidget(ctrlGroup);

    /* Log messaggi */
    auto* logGroup = new QGroupBox(
        tr("\xf0\x9f\x93\x9d  Log messaggi in tempo reale"), w);
    auto* logLay = new QVBoxLayout(logGroup);
    m_telegramLog = new QTextBrowser(logGroup);
    m_telegramLog->setReadOnly(true);
    m_telegramLog->setOpenLinks(false);
    m_telegramLog->setMinimumHeight(dpiScale(160));
    m_telegramLog->setPlaceholderText(
        tr("Qui appariranno i messaggi ricevuti e le risposte inviate..."));
    connect(m_telegramLog, &QTextBrowser::anchorClicked,
            this, &AppControllerPage::onPipLinkClicked);
    logLay->addWidget(m_telegramLog);
    lay->addWidget(logGroup, 1);

    /* ════════════════════════════════════════════════════════
       PASSO 3 — Invia messaggio a destinatari specifici
       ════════════════════════════════════════════════════════ */
    auto* step3 = new QGroupBox(
        tr("\xf0\x9f\x93\xa4  Passo 3 \xe2\x80\x94 Invia messaggio a destinatari"), w);
    auto* s3Lay = new QVBoxLayout(step3);
    s3Lay->setSpacing(dpiScale(6));

    auto* step3Hint = new QLabel(
        "<b>Come aggiungere un destinatario:</b><br><br>"
        "<span style='color:#f87171;'>"
        "\xe2\x9a\xa0  <b>@username NON funziona per gli utenti privati.</b>"
        "</span> Telegram richiede l\xe2\x80\x99<b>ID numerico</b> del contatto.<br><br>"
        "<b>Come ottenere l\xe2\x80\x99ID di un utente privato:</b><br>"
        "1. Chiedi al contatto di aprire il tuo bot su Telegram e inviare <code>/start</code><br>"
        "2. Il suo ID numerico apparir\xc3\xa0 nel log qui sopra: "
        "<code>\xf0\x9f\x93\xa9 [123456789] /start</code><br>"
        "3. Copia quel numero e incollalo nel campo qui sotto<br><br>"
        "<b>Canali e gruppi pubblici:</b> usa l\xe2\x80\x99ID negativo "
        "(es. <code>-1001234567890</code>) oppure <code>@nome_canale</code>. "
        "Per trovare l\xe2\x80\x99ID di un gruppo: aggiungi <b>@userinfobot</b> "
        "al gruppo e scrivi <code>/id</code>.",
        step3);
    step3Hint->setObjectName("hintLabel");
    step3Hint->setTextFormat(Qt::RichText);
    step3Hint->setWordWrap(true);
    s3Lay->addWidget(step3Hint);

    auto* promoAddRow = new QHBoxLayout;
    m_telegramPromoContactEdit = new QLineEdit(step3);
    m_telegramPromoContactEdit->setPlaceholderText(
        tr("ID numerico utente (es. 123456789) oppure -1001234567890 (gruppo/canale)"));
    auto* tgAddBtn = new QPushButton(
        tr("\xe2\x9e\x95  Aggiungi"), step3);
    tgAddBtn->setObjectName("actionBtn");
    tgAddBtn->setFixedWidth(dpiScale(100));
    auto* tgRemoveBtn = new QPushButton(
        tr("\xe2\x9e\x96  Rimuovi"), step3);
    tgRemoveBtn->setObjectName("actionBtn");
    tgRemoveBtn->setFixedWidth(dpiScale(100));
    promoAddRow->addWidget(m_telegramPromoContactEdit, 1);
    promoAddRow->addWidget(tgAddBtn);
    promoAddRow->addWidget(tgRemoveBtn);
    s3Lay->addLayout(promoAddRow);

    m_telegramContactList = new QListWidget(step3);
    m_telegramContactList->setFixedHeight(dpiScale(80));
    s3Lay->addWidget(m_telegramContactList);

    m_telegramAutoAddCheck = new QCheckBox(
        tr("\xf0\x9f\xa4\x96  Aggiungi automaticamente chi scrive al bot"), step3);
    m_telegramAutoAddCheck->setToolTip(
        "Quando un utente invia qualsiasi messaggio al bot (incluso /start),\n"
        "il suo ID numerico viene aggiunto automaticamente alla lista destinatari.");
    s3Lay->addWidget(m_telegramAutoAddCheck);

    auto* promoMsgLbl = new QLabel(
        tr("Messaggio da inviare:"), step3);
    s3Lay->addWidget(promoMsgLbl);
    m_telegramPromoMsgEdit = new QTextEdit(step3);
    m_telegramPromoMsgEdit->setFixedHeight(dpiScale(60));
    m_telegramPromoMsgEdit->setPlaceholderText(
        tr("Scrivi il testo del messaggio..."));
    s3Lay->addWidget(m_telegramPromoMsgEdit);

    auto* promoCtrlRow = new QHBoxLayout;
    auto* tgSendAllBtn = new QPushButton(
        tr("\xf0\x9f\x93\xa4  Invia a tutti i destinatari"), step3);
    tgSendAllBtn->setObjectName("actionBtn");
    m_telegramPromoStatusLbl = new QLabel("", step3);
    m_telegramPromoStatusLbl->setObjectName("statusLabel");
    promoCtrlRow->addWidget(tgSendAllBtn);
    promoCtrlRow->addWidget(m_telegramPromoStatusLbl, 1);
    s3Lay->addLayout(promoCtrlRow);
    lay->addWidget(step3);

    lay->addStretch();
    scroll->setWidget(w);

    /* ── Carica dati salvati ── */
    {
        QSettings s("Prismalux", "GUI");
        /* Token bot: storage sicuro (keychain / file 0600). Migra il vecchio valore
           in chiaro eventualmente presente in QSettings e lo rimuove. */
        QString savedToken = LanServer::loadSecret(QStringLiteral("telegram_token"));
        const QString legacyToken = s.value("telegram/token").toString();
        if (savedToken.isEmpty() && !legacyToken.isEmpty()) {
            savedToken = legacyToken;
            LanServer::saveSecret(QStringLiteral("telegram_token"), legacyToken);
            s.remove("telegram/token");
        }
        const QString savedWl = s.value("telegram/whitelist").toString();
        if (!savedToken.isEmpty())
            m_telegramTokenEdit->setText(savedToken);
        if (!savedWl.isEmpty())
            m_telegramWhitelistEdit->setText(savedWl);
        const QStringList contacts = s.value("telegram_contacts").toStringList();
        for (const QString& c : contacts)
            m_telegramContactList->addItem(c);
        m_telegramAutoAddCheck->setChecked(
            s.value("telegram/auto_add_contacts", true).toBool());
    }

    if (!m_telegramPromoNam)
        m_telegramPromoNam = new QNetworkAccessManager(this);

    /* ── Connessioni ── */
    connect(eyeBtn, &QPushButton::toggled, m_telegramTokenEdit,
            [this](bool show) {
        m_telegramTokenEdit->setEchoMode(
            show ? QLineEdit::Normal : QLineEdit::Password);
    });

    connect(saveTokenBtn, &QPushButton::clicked, this, [this]() {
        QSettings s("Prismalux", "GUI");
        const QString tok = m_telegramTokenEdit->text().trimmed();
        if (tok.isEmpty()) LanServer::deleteSecret(QStringLiteral("telegram_token"));
        else               LanServer::saveSecret(QStringLiteral("telegram_token"), tok);
        s.remove("telegram/token");   /* il token non resta mai in chiaro in QSettings */
        s.setValue("telegram/whitelist", m_telegramWhitelistEdit->text().trimmed());
        m_telegramStatusLbl->setText(tr("\xe2\x9c\x85  Token salvato"));
    });

    connect(m_telegramStartBtn, &QPushButton::clicked,
            this, &AppControllerPage::onTelegramStartClicked);
    connect(m_telegramStopBtn,  &QPushButton::clicked,
            this, &AppControllerPage::onTelegramStopClicked);
    connect(tgAddBtn, &QPushButton::clicked,
            this, &AppControllerPage::onTelegramAddContactClicked);
    connect(tgRemoveBtn, &QPushButton::clicked,
            this, &AppControllerPage::onTelegramRemoveContactClicked);
    connect(tgSendAllBtn, &QPushButton::clicked,
            this, &AppControllerPage::onTelegramSendPromoClicked);
    connect(m_telegramAutoAddCheck, &QCheckBox::toggled,
            this, [this](bool checked) {
        QSettings s("Prismalux", "GUI");
        s.setValue("telegram/auto_add_contacts", checked);
    });

    return scroll;
}

/* ======================================================================
   Sezione 12 — Telegram Bot slots
   ====================================================================== */


void AppControllerPage::onTelegramStartClicked()
{
    m_telegramIntentionalStop = false;
    if (QProcess::execute(P::findPython(),
            {"-c", "from telegram.ext import Application"}) != 0) {
        m_telegramStatusLbl->setText(
            tr("\xe2\x9d\x8c  python-telegram-bot v20+ non installato"));
        m_telegramLog->append(
            "<span style='color:#f87171;'>"
            "\xe2\x9d\x8c  Modulo mancante \xe2\x80\x94 clicca qui per installarlo: "
            "<a href='pip://python-telegram-bot' style='color:#fbbf24;'>"
            "\xf0\x9f\x94\xa7 Installa python-telegram-bot</a>"
            "</span>");
        return;
    }

    const QString token = m_telegramTokenEdit->text().trimmed();
    if (token.isEmpty()) {
        m_telegramStatusLbl->setText(
            tr("\xe2\x9d\x8c  Inserisci il Bot Token prima di avviare."));
        return;
    }

    /* Se già in esecuzione, ferma prima */
    if (m_telegramProc && m_telegramProc->state() == QProcess::Running) {
        m_telegramProc->terminate();
        m_telegramProc->waitForFinished(P::kProcessStartTimeoutMs);
    }

    /* Usa direttamente il file versionato — non sovrascrivere */
    const QString scriptPath = P::root() + "/MCPs/telegram_bot_runtime.py";
    if (!QFileInfo::exists(scriptPath)) {
        m_telegramStatusLbl->setText(
            "\xe2\x9d\x8c  File non trovato: " + scriptPath);
        return;
    }

    /* Crea il QProcess se non esiste */
    if (!m_telegramProc) {
        m_telegramProc = new QProcess(this);
        m_telegramProc->setProcessChannelMode(QProcess::SeparateChannels);
        connect(m_telegramProc, &QProcess::readyReadStandardOutput,
                this, &AppControllerPage::onTelegramProcReadyRead);
        connect(m_telegramProc, &QProcess::readyReadStandardError,
                this, [this]() {
            const QString err =
                QString::fromUtf8(m_telegramProc->readAllStandardError()).trimmed();
            if (!err.isEmpty()) {
                m_telegramLog->append(
                    "<span style='color:#f87171;'>"
                    + err.toHtmlEscaped() + "</span>");
                LogBus::post("\xe2\x9d\x8c Telegram Bot: " + err);
            }
        });
        connect(m_telegramProc,
                QOverload<int,QProcess::ExitStatus>::of(&QProcess::finished),
                this, &AppControllerPage::onTelegramProcFinished);
    }

    /* Imposta le env var — token e whitelist fuori dal codice */
    QProcessEnvironment env = QProcessEnvironment::systemEnvironment();
    env.insert("TELEGRAM_TOKEN",     token);
    env.insert("TELEGRAM_WHITELIST", m_telegramWhitelistEdit->text().trimmed());
    m_telegramProc->setProcessEnvironment(env);

    m_telegramProc->start(P::findPython(), {scriptPath});

    if (m_telegramProc->waitForStarted(P::kProcessStartTimeoutMs)) {
        m_telegramStartBtn->setEnabled(false);
        m_telegramStopBtn->setEnabled(true);
        m_telegramStatusLbl->setText(
            tr("\xf0\x9f\x94\x84  Avvio in corso..."));
        m_telegramLog->append(
            "<b>\xe2\x96\xb6 Bot avviato.</b> Attendo conferma da Telegram...");
    } else {
        m_telegramStatusLbl->setText(
            tr("\xe2\x9d\x8c  Errore avvio (python3 non trovato?)"));
        m_telegramLog->append(
            "\xe2\x9d\x8c  python3 non trovato. Verifica l'installazione.");
    }
}

void AppControllerPage::onTelegramStopClicked()
{
    m_telegramIntentionalStop = true;
    if (m_telegramProc && m_telegramProc->state() == QProcess::Running) {
        m_telegramProc->terminate();
        if (!m_telegramProc->waitForFinished(P::kProcessStartTimeoutMs))
            m_telegramProc->kill();
    }
    /* onTelegramProcFinished aggiornerà lo stato */
}

void AppControllerPage::onTelegramProcReadyRead()
{
    while (m_telegramProc->canReadLine()) {
        const QString line =
            QString::fromUtf8(m_telegramProc->readLine()).trimmed();
        if (line.isEmpty()) continue;

        /* Tenta parse JSON */
        const QJsonObject obj =
            QJsonDocument::fromJson(line.toUtf8()).object();

        const QString type = obj.value("type").toString();

        if (type == "ready") {
            m_telegramStatusLbl->setText(
                tr("\xe2\x9c\x85  Bot attivo \xe2\x80\x94 in ascolto"));
            m_telegramLog->append(
                "<b style='color:#4ade80;'>"
                "\xe2\x9c\x85 Bot pronto.</b>");
            continue;
        }

        if (type == "error") {
            const QString msg = obj.value("msg").toString();
            m_telegramStatusLbl->setText(tr("\xe2\x9d\x8c  Errore bot"));
            LogBus::post("\xe2\x9d\x8c Telegram Bot: " + msg);
            const bool isMissing = msg.contains("Modulo mancante",
                                                Qt::CaseInsensitive) ||
                                   msg.contains("No module named",
                                                Qt::CaseInsensitive) ||
                                   msg.contains("non installato",
                                                Qt::CaseInsensitive);
            if (isMissing) {
                m_telegramLog->append(
                    "<span style='color:#f87171;'>"
                    "\xe2\x9d\x8c  " + msg.toHtmlEscaped() + "<br>"
                    "Clicca per installare: "
                    "<a href='pip://python-telegram-bot' style='color:#fbbf24;'>"
                    "\xf0\x9f\x94\xa7 Installa python-telegram-bot</a>"
                    "</span>");
            } else {
                m_telegramLog->append(
                    "<span style='color:#f87171;'>"
                    "\xe2\x9d\x8c  " + msg.toHtmlEscaped() + "</span>");
            }
            continue;
        }

        if (type == "query") {
            const int     chatId = obj.value("chat_id").toInt();
            const QString text   = obj.value("text").toString();
            m_telegramChatId = chatId;

            m_telegramLog->append(
                QString("<b>\xf0\x9f\x93\xa9 [%1]</b> %2")
                    .arg(chatId)
                    .arg(text.toHtmlEscaped()));

            /* Invia al modello AI locale */
            if (m_telegramAiHolder) {
                m_telegramAiHolder->deleteLater();
                m_telegramAiHolder = nullptr;
            }
            m_telegramAiHolder = new QObject(this);

            const int savedChatId = chatId;

            connect(m_ai, &AiClient::finished,
                    m_telegramAiHolder,
                    [this, savedChatId](const QString& result) {
                if (m_telegramAiHolder) {
                    m_telegramAiHolder->deleteLater();
                    m_telegramAiHolder = nullptr;
                }
                /* Rimuovi blocchi <think> se presenti */
                QString clean = result;
                static const QRegularExpression reThink(
                    "<think>[\\s\\S]*?</think>",
                    QRegularExpression::CaseInsensitiveOption);
                clean.remove(reThink);
                clean = clean.trimmed();

                m_telegramLog->append(
                    QString("<b>\xf0\x9f\xa4\x96 Bot \xe2\x86\x92 [%1]</b> %2")
                        .arg(savedChatId)
                        .arg(clean.left(200).toHtmlEscaped()
                             + (clean.length() > 200 ? "..." : "")));

                /* Scrivi risposta sullo stdin del processo */
                if (m_telegramProc &&
                    m_telegramProc->state() == QProcess::Running) {
                    const QJsonObject resp{
                        {"chat_id", savedChatId},
                        {"reply",   clean}
                    };
                    const QByteArray payload =
                        QJsonDocument(resp).toJson(QJsonDocument::Compact)
                        + "\n";
                    m_telegramProc->write(payload);
                }
            });

            connect(m_ai, &AiClient::error,
                    m_telegramAiHolder,
                    [this, savedChatId](const QString& errMsg) {
                if (m_telegramAiHolder) {
                    m_telegramAiHolder->deleteLater();
                    m_telegramAiHolder = nullptr;
                }
                m_telegramLog->append(
                    "<span style='color:#f87171;'>"
                    "\xe2\x9d\x8c AI error: "
                    + errMsg.toHtmlEscaped() + "</span>");

                if (m_telegramProc &&
                    m_telegramProc->state() == QProcess::Running) {
                    const QJsonObject resp{
                        {"chat_id", savedChatId},
                        {"reply",   "Errore AI: " + errMsg}
                    };
                    m_telegramProc->write(
                        QJsonDocument(resp).toJson(QJsonDocument::Compact)
                        + "\n");
                }
            });

            m_ai->chat(
                "Sei Prismalux, un assistente AI locale. "
                "Rispondi in modo conciso e utile.",
                text);
            continue;
        }

        if (type == "new_contact") {
            const QString cid  = QString::number(obj.value("chat_id").toInt());
            const QString user = obj.value("username").toString();
            const QString name = obj.value("first_name").toString();

            /* Logga sempre nel pannello bot */
            const QString displayName = user.isEmpty() ? name : "@" + user;
            m_telegramLog->append(
                QString("<span style='color:#86efac;'>"
                        "\xf0\x9f\x91\xa4 Nuovo contatto: <b>%1</b> (ID: %2)</span>")
                    .arg(displayName.toHtmlEscaped(), cid));

            /* Aggiunge alla lista solo se checkbox attivo e ID non già presente */
            if (m_telegramAutoAddCheck && m_telegramAutoAddCheck->isChecked()) {
                bool found = false;
                for (int i = 0; i < m_telegramContactList->count(); ++i) {
                    if (m_telegramContactList->item(i)->text() == cid) {
                        found = true; break;
                    }
                }
                if (!found) {
                    m_telegramContactList->addItem(cid);
                    QSettings s("Prismalux", "GUI");
                    QStringList all;
                    all.reserve(m_telegramContactList->count());
                    for (int i = 0; i < m_telegramContactList->count(); ++i)
                        all << m_telegramContactList->item(i)->text();
                    s.setValue("telegram_contacts", all);
                    m_telegramLog->append(
                        QString("<span style='color:#86efac;'>"
                                "\xe2\x9c\x85 ID %1 aggiunto ai destinatari.</span>")
                            .arg(cid));
                }
            }
            continue;
        }

        /* Messaggio generico dal processo */
        m_telegramLog->append(line.toHtmlEscaped());
    }
}

void AppControllerPage::onTelegramProcFinished(
    int code, QProcess::ExitStatus /*status*/)
{
    if (!m_telegramStartBtn || !m_telegramStopBtn) return;
    m_telegramStartBtn->setEnabled(true);
    m_telegramStopBtn->setEnabled(false);

    /* Libera l'holder AI se era attivo */
    if (m_telegramAiHolder) {
        m_telegramAiHolder->deleteLater();
        m_telegramAiHolder = nullptr;
    }

    if (m_telegramIntentionalStop) {
        m_telegramStatusLbl->setText(tr("\xe2\x9a\xaa  Bot fermato"));
        m_telegramLog->append("<b>\xe2\x8f\xb9 Bot fermato.</b>");
        return;
    }

    /* Crash inatteso — mostra notifica con link riavvio */
    const QString exitInfo = (code == 0)
        ? tr("exit 0 (riavvio inatteso)")
        : QString("exit %1").arg(code);
    m_telegramStatusLbl->setText(
        QString("\xe2\x9a\xa0\xef\xb8\x8f  Bot crashato (%1)").arg(exitInfo));
    m_telegramLog->append(
        QString("<span style='color:#f87171;'>"
                "\xe2\x9a\xa0\xef\xb8\x8f  <b>Bot terminato inaspettatamente</b> (%1). "
                "Controlla il log sopra per la causa."
                "</span><br>"
                "<a href='tg-restart://' style='color:#fbbf24;'>"
                "\xf0\x9f\x94\x84 Riavvia bot Telegram</a>")
        .arg(exitInfo.toHtmlEscaped()));
    LogBus::post(
        QString("\xe2\x9a\xa0 Telegram Bot crashato (%1) — riavvio disponibile in app").arg(exitInfo));
}

/* ======================================================================
   Sezione 13 — Telegram contatti promozionali
   ====================================================================== */

void AppControllerPage::onTelegramAddContactClicked()
{
    const QString contact = m_telegramPromoContactEdit->text().trimmed();
    if (contact.isEmpty())
        return;
    for (int i = 0; i < m_telegramContactList->count(); ++i) {
        if (m_telegramContactList->item(i)->text() == contact)
            return;
    }
    m_telegramContactList->addItem(contact);
    m_telegramPromoContactEdit->clear();

    QStringList all;
    all.reserve(m_telegramContactList->count());
    for (int i = 0; i < m_telegramContactList->count(); ++i)
        all << m_telegramContactList->item(i)->text();
    QSettings s("Prismalux", "GUI");
    s.setValue("telegram_contacts", all);
}

void AppControllerPage::onTelegramRemoveContactClicked()
{
    const int row = m_telegramContactList->currentRow();
    if (row < 0)
        return;
    delete m_telegramContactList->takeItem(row);

    QStringList all;
    all.reserve(m_telegramContactList->count());
    for (int i = 0; i < m_telegramContactList->count(); ++i)
        all << m_telegramContactList->item(i)->text();
    QSettings s("Prismalux", "GUI");
    s.setValue("telegram_contacts", all);
}

void AppControllerPage::onTelegramSendPromoClicked()
{
    const QString token = m_telegramTokenEdit->text().trimmed();
    if (token.isEmpty()) {
        m_telegramPromoStatusLbl->setText(
            tr("\xe2\x9d\x8c  Inserisci il Bot Token prima di inviare."));
        return;
    }
    const QString msg = m_telegramPromoMsgEdit->toPlainText().trimmed();
    if (msg.isEmpty()) {
        m_telegramPromoStatusLbl->setText(
            tr("\xe2\x9d\x8c  Scrivi il messaggio prima di inviare."));
        return;
    }
    const int total = m_telegramContactList->count();
    if (total == 0) {
        m_telegramPromoStatusLbl->setText(
            tr("\xe2\x9d\x8c  Nessun contatto in lista."));
        return;
    }

    m_telegramPromoStatusLbl->setText(
        QString("\xf0\x9f\x93\xa4  Invio a %1 contatti...").arg(total));

    for (int i = 0; i < total; ++i) {
        const QString chatId = m_telegramContactList->item(i)->text();

        const QUrl url(QString("https://api.telegram.org/bot%1/sendMessage")
                       .arg(token));
        QNetworkRequest req(url);
        req.setHeader(QNetworkRequest::ContentTypeHeader, "application/json");

        const QJsonObject body{
            {"chat_id", chatId},
            {"text",    msg},
            {"parse_mode", "HTML"}
        };
        QNetworkReply* reply =
            m_telegramPromoNam->post(
                req, QJsonDocument(body).toJson(QJsonDocument::Compact));

        const int idx = i + 1;
        connect(reply, &QNetworkReply::finished,
                this, [this, reply, idx, total, chatId]() {
            const QByteArray body = reply->readAll();
            reply->deleteLater();
            if (reply->error() != QNetworkReply::NoError) {
                /* Legge la descrizione reale dall'API Telegram */
                const QJsonObject tgObj = QJsonDocument::fromJson(body).object();
                const int errCode = tgObj.value("error_code").toInt(
                    reply->attribute(QNetworkRequest::HttpStatusCodeAttribute).toInt());
                const QString tgDesc = tgObj.value("description").toString();

                QString hint;
                if (errCode == 403) {
                    if (tgDesc.contains("initiate"))
                        hint = " \xe2\x86\x92 il contatto deve inviare /start al bot";
                    else if (tgDesc.contains("blocked"))
                        hint = " \xe2\x86\x92 il contatto ha bloccato il bot";
                    else if (tgDesc.contains("not a member"))
                        hint = " \xe2\x86\x92 il bot non \xc3\xa8 membro del gruppo";
                    else if (chatId.startsWith('@') && !chatId.startsWith("-"))
                        hint = " \xe2\x86\x92 i bot non possono ricevere messaggi da altri bot";
                } else if (errCode == 400) {
                    if (tgDesc.contains("chat not found"))
                        hint = " \xe2\x86\x92 ID non trovato: usa l'ID numerico dopo che il contatto ha inviato /start";
                    else if (!chatId.at(0).isDigit() && !chatId.startsWith('-'))
                        hint = " \xe2\x86\x92 usa l'ID numerico, non @username";
                }

                const QString detail = tgDesc.isEmpty() ? reply->errorString()
                                                        : tgDesc;
                const QString errMsg =
                    QString("\xe2\x9d\x8c  Errore su %1 (%2): %3%4")
                        .arg(chatId).arg(errCode).arg(detail, hint);
                m_telegramPromoStatusLbl->setText(errMsg);
                LogBus::post("Telegram: " + errMsg);
            } else if (idx == total) {
                m_telegramPromoStatusLbl->setText(
                    QString("\xe2\x9c\x85  Inviato a %1 contatti.").arg(total));
            }
        });
    }
}
