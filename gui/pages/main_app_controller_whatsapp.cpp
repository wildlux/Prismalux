/* ══════════════════════════════════════════════════════════════
   main_app_controller_whatsapp.cpp — AppControllerPage: Bridge WhatsApp
   =========================================================================
   Bridge WhatsApp-MCP locale — builder + slot (contatti promozionali,
   bot rispondente AI). Split da
   main_app_controller.cpp/main_app_controller_slots.cpp (TODO D-8).
   ══════════════════════════════════════════════════════════════ */
#include "main_app_controller.h"
#include "../prismalux_paths.h"
#include "../log_bus.h"
#include "../dpi_utils.h"

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
#include <QTimer>
#include <QNetworkAccessManager>
#include <QNetworkReply>
#include <QNetworkRequest>
#include <QJsonDocument>
#include <QJsonObject>
#include <QJsonArray>

namespace P = PrismaluxPaths;

/* ══════════════════════════════════════════════════════════════
   buildWhatsAppTab — bridge WhatsApp-MCP locale
   ══════════════════════════════════════════════════════════════ */
QWidget* AppControllerPage::buildWhatsAppTab()
{
    auto* w   = new QWidget;
    auto* lay = new QVBoxLayout(w);
    lay->setContentsMargins(dpiScale(12), dpiScale(12),
                            dpiScale(12), dpiScale(12));
    lay->setSpacing(dpiScale(8));

    auto* descLbl = new QLabel(
        "\xf0\x9f\x92\xac  <i>WhatsApp Bot locale \xe2\x80\x94 "
        "Collega WhatsApp a Prismalux tramite bridge whatsapp-mcp "
        "(<a href='https://github.com/lharries/whatsapp-mcp'>"
        "github.com/lharries/whatsapp-mcp</a>). "
        "Nessun account Business richiesto.</i>", w);
    descLbl->setObjectName("hintLabel");
    descLbl->setTextFormat(Qt::RichText);
    descLbl->setWordWrap(true);
    descLbl->setOpenExternalLinks(true);
    lay->addWidget(descLbl);

    /* ── GroupBox Configurazione bridge ── */
    auto* cfgGroup = new QGroupBox(
        "\xf0\x9f\x94\xa7  Configurazione bridge", w);
    auto* cfgLay = new QVBoxLayout(cfgGroup);
    cfgLay->setSpacing(dpiScale(6));

    auto* bridgeRow = new QHBoxLayout;
    auto* bridgeLbl = new QLabel(tr("Bridge URL:"), cfgGroup);
    bridgeLbl->setFixedWidth(dpiScale(90));
    m_waBridgeUrlEdit = new QLineEdit(cfgGroup);
    m_waBridgeUrlEdit->setPlaceholderText(tr("http://localhost:3000"));
    auto* saveBridgeBtn = new QPushButton(
        "\xf0\x9f\x92\xbe  Salva", cfgGroup);
    saveBridgeBtn->setObjectName("actionBtn");
    saveBridgeBtn->setFixedWidth(dpiScale(80));
    bridgeRow->addWidget(bridgeLbl);
    bridgeRow->addWidget(m_waBridgeUrlEdit, 1);
    bridgeRow->addWidget(saveBridgeBtn);
    cfgLay->addLayout(bridgeRow);

    auto* bridgeHintLbl = new QLabel(
        "\xf0\x9f\x94\x92  Avvia prima il bridge: "
        "<code>cd whatsapp-mcp && npm start</code>. "
        "Scansiona il QR con WhatsApp \xe2\x86\x92 Dispositivi collegati.",
        cfgGroup);
    bridgeHintLbl->setObjectName("hintLabel");
    bridgeHintLbl->setTextFormat(Qt::RichText);
    bridgeHintLbl->setWordWrap(true);
    cfgLay->addWidget(bridgeHintLbl);

    lay->addWidget(cfgGroup);

    /* ── GroupBox Contatti promozionali ── */
    auto* promoGroup = new QGroupBox(
        "\xf0\x9f\x93\xa3  Contatti promozionali", w);
    auto* promoLay = new QVBoxLayout(promoGroup);
    promoLay->setSpacing(dpiScale(6));

    auto* promoHintLbl = new QLabel(
        "<i>Inserisci i numeri con prefisso internazionale "
        "(es. +393331234567). Il bridge invier\xc3\xa0 i messaggi tramite "
        "whatsapp-mcp POST /send.</i>",
        promoGroup);
    promoHintLbl->setObjectName("hintLabel");
    promoHintLbl->setTextFormat(Qt::RichText);
    promoHintLbl->setWordWrap(true);
    promoLay->addWidget(promoHintLbl);

    auto* promoAddRow = new QHBoxLayout;
    m_waPromoContactEdit = new QLineEdit(promoGroup);
    m_waPromoContactEdit->setPlaceholderText(tr("+393331234567"));
    auto* waAddBtn = new QPushButton(
        "\xe2\x9e\x95  Aggiungi", promoGroup);
    waAddBtn->setObjectName("actionBtn");
    waAddBtn->setFixedWidth(dpiScale(100));
    auto* waRemoveBtn = new QPushButton(
        "\xe2\x9e\x96  Rimuovi", promoGroup);
    waRemoveBtn->setObjectName("actionBtn");
    waRemoveBtn->setFixedWidth(dpiScale(100));
    promoAddRow->addWidget(m_waPromoContactEdit, 1);
    promoAddRow->addWidget(waAddBtn);
    promoAddRow->addWidget(waRemoveBtn);
    promoLay->addLayout(promoAddRow);

    m_waContactList = new QListWidget(promoGroup);
    m_waContactList->setFixedHeight(dpiScale(80));
    promoLay->addWidget(m_waContactList);

    auto* waMsgLbl = new QLabel(tr("Messaggio:"), promoGroup);
    promoLay->addWidget(waMsgLbl);
    m_waPromoMsgEdit = new QTextEdit(promoGroup);
    m_waPromoMsgEdit->setFixedHeight(dpiScale(60));
    m_waPromoMsgEdit->setPlaceholderText(
        "Testo del messaggio promozionale...");
    promoLay->addWidget(m_waPromoMsgEdit);

    auto* promoCtrlRow = new QHBoxLayout;
    auto* waSendAllBtn = new QPushButton(
        "\xf0\x9f\x93\xa4  Invia a tutti", promoGroup);
    waSendAllBtn->setObjectName("actionBtn");
    m_waPromoStatusLbl = new QLabel("", promoGroup);
    m_waPromoStatusLbl->setObjectName("statusLabel");
    promoCtrlRow->addWidget(waSendAllBtn);
    promoCtrlRow->addWidget(m_waPromoStatusLbl, 1);
    promoLay->addLayout(promoCtrlRow);

    lay->addWidget(promoGroup);

    /* ── GroupBox Bot AI Rispondente ── */
    auto* botGroup = new QGroupBox(
        "\xf0\x9f\xa4\x96  Bot AI Rispondente", w);
    auto* botLay = new QVBoxLayout(botGroup);
    botLay->setSpacing(dpiScale(6));

    auto* botHint = new QLabel(
        "<i>Quando il bot \xc3\xa8 attivo, ascolta i messaggi in entrata dal bridge "
        "e risponde automaticamente con l\xe2\x80\x99" "AI locale. "
        "Solo i numeri in whitelist ricevono risposta.</i>",
        botGroup);
    botHint->setObjectName("hintLabel");
    botHint->setTextFormat(Qt::RichText);
    botHint->setWordWrap(true);
    botLay->addWidget(botHint);

    auto* wlRow = new QHBoxLayout;
    auto* wlLbl = new QLabel(tr("Whitelist numeri:"), botGroup);
    wlLbl->setFixedWidth(dpiScale(120));
    m_waWhitelistEdit = new QLineEdit(botGroup);
    m_waWhitelistEdit->setPlaceholderText(tr("+393331234567, +391234567890 (virgola = separatore)"));
    wlRow->addWidget(wlLbl);
    wlRow->addWidget(m_waWhitelistEdit, 1);
    botLay->addLayout(wlRow);

    m_waAutoReplyCheck = new QCheckBox(
        "Abilita auto-risposta AI ai messaggi in entrata", botGroup);
    m_waAutoReplyCheck->setChecked(true);
    botLay->addWidget(m_waAutoReplyCheck);

    auto* botCtrlRow = new QHBoxLayout;
    m_waBotStartBtn = new QPushButton(
        "\xf0\x9f\x9f\xa2  Avvia Bot", botGroup);
    m_waBotStartBtn->setObjectName("actionBtn");
    m_waBotStartBtn->setFixedWidth(dpiScale(120));
    m_waBotStopBtn = new QPushButton(
        "\xf0\x9f\x94\xb4  Ferma Bot", botGroup);
    m_waBotStopBtn->setObjectName("actionBtn");
    m_waBotStopBtn->setFixedWidth(dpiScale(120));
    m_waBotStopBtn->setEnabled(false);
    m_waBotStatusLbl = new QLabel(
        "\xe2\x9a\xab  Bot non attivo", botGroup);
    m_waBotStatusLbl->setObjectName("statusLabel");
    botCtrlRow->addWidget(m_waBotStartBtn);
    botCtrlRow->addWidget(m_waBotStopBtn);
    botCtrlRow->addWidget(m_waBotStatusLbl, 1);
    botLay->addLayout(botCtrlRow);

    auto* botLogGroup = new QGroupBox(tr("Log messaggi"), botGroup);
    auto* botLogLay   = new QVBoxLayout(botLogGroup);
    m_waBotLog = new QTextBrowser(botLogGroup);
    m_waBotLog->setReadOnly(true);
    m_waBotLog->setOpenLinks(false);
    m_waBotLog->setMinimumHeight(dpiScale(120));
    m_waBotLog->setPlaceholderText(
        "I messaggi ricevuti/inviati dal bot appariranno qui...");
    connect(m_waBotLog, &QTextBrowser::anchorClicked,
            this, &AppControllerPage::onPipLinkClicked);
    botLogLay->addWidget(m_waBotLog);
    botLay->addWidget(botLogGroup);

    lay->addWidget(botGroup);
    lay->addStretch();

    /* ── Carica impostazioni salvate ── */
    {
        QSettings s("Prismalux", "GUI");
        const QString savedUrl = s.value("whatsapp/bridge_url",
                                         "http://localhost:3000").toString();
        m_waBridgeUrlEdit->setText(savedUrl);

        const QStringList contacts =
            s.value("whatsapp_contacts").toStringList();
        for (const QString& c : contacts)
            m_waContactList->addItem(c);

        m_waWhitelistEdit->setText(
            s.value("whatsapp/bot_whitelist").toString());
        m_waAutoReplyCheck->setChecked(
            s.value("whatsapp/bot_auto_reply", true).toBool());
    }

    if (!m_waPromoNam)
        m_waPromoNam = new QNetworkAccessManager(this);
    if (!m_waNam)
        m_waNam = new QNetworkAccessManager(this);

    /* ── Connessioni ── */
    connect(saveBridgeBtn, &QPushButton::clicked, this, [this]() {
        QSettings s("Prismalux", "GUI");
        s.setValue("whatsapp/bridge_url", m_waBridgeUrlEdit->text().trimmed());
        s.setValue("whatsapp/bot_whitelist", m_waWhitelistEdit->text().trimmed());
        s.setValue("whatsapp/bot_auto_reply", m_waAutoReplyCheck->isChecked());
        m_waPromoStatusLbl->setText(tr("\xe2\x9c\x85  URL salvato"));
    });

    connect(waAddBtn, &QPushButton::clicked,
            this, &AppControllerPage::onWaAddContactClicked);
    connect(waRemoveBtn, &QPushButton::clicked,
            this, &AppControllerPage::onWaRemoveContactClicked);
    connect(waSendAllBtn, &QPushButton::clicked,
            this, &AppControllerPage::onWaSendPromoClicked);
    connect(m_waBotStartBtn, &QPushButton::clicked,
            this, &AppControllerPage::onWaBotStartClicked);
    connect(m_waBotStopBtn, &QPushButton::clicked,
            this, &AppControllerPage::onWaBotStopClicked);

    return w;
}

/* ======================================================================
   Sezione 14 — WhatsApp contatti promozionali
   ====================================================================== */

void AppControllerPage::onWaAddContactClicked()
{
    const QString contact = m_waPromoContactEdit->text().trimmed();
    if (contact.isEmpty())
        return;
    for (int i = 0; i < m_waContactList->count(); ++i) {
        if (m_waContactList->item(i)->text() == contact)
            return;
    }
    m_waContactList->addItem(contact);
    m_waPromoContactEdit->clear();

    QStringList all;
    all.reserve(m_waContactList->count());
    for (int i = 0; i < m_waContactList->count(); ++i)
        all << m_waContactList->item(i)->text();
    QSettings s("Prismalux", "GUI");
    s.setValue("whatsapp_contacts", all);
}

void AppControllerPage::onWaRemoveContactClicked()
{
    const int row = m_waContactList->currentRow();
    if (row < 0)
        return;
    delete m_waContactList->takeItem(row);

    QStringList all;
    all.reserve(m_waContactList->count());
    for (int i = 0; i < m_waContactList->count(); ++i)
        all << m_waContactList->item(i)->text();
    QSettings s("Prismalux", "GUI");
    s.setValue("whatsapp_contacts", all);
}

void AppControllerPage::onWaSendPromoClicked()
{
    const QString bridgeUrl = m_waBridgeUrlEdit->text().trimmed();
    if (bridgeUrl.isEmpty()) {
        m_waPromoStatusLbl->setText(
            "\xe2\x9d\x8c  Inserisci l\xe2\x80\x99" "URL del bridge.");
        return;
    }
    const QString msg = m_waPromoMsgEdit->toPlainText().trimmed();
    if (msg.isEmpty()) {
        m_waPromoStatusLbl->setText(
            "\xe2\x9d\x8c  Scrivi il messaggio prima di inviare.");
        return;
    }
    const int total = m_waContactList->count();
    if (total == 0) {
        m_waPromoStatusLbl->setText(
            "\xe2\x9d\x8c  Nessun contatto in lista.");
        return;
    }

    m_waPromoStatusLbl->setText(
        QString("\xf0\x9f\x93\xa4  Invio a %1 contatti...").arg(total));

    for (int i = 0; i < total; ++i) {
        const QString number = m_waContactList->item(i)->text();

        const QUrl url(bridgeUrl.endsWith('/') ?
                       bridgeUrl + "send" :
                       bridgeUrl + "/send");
        QNetworkRequest req(url);
        req.setHeader(QNetworkRequest::ContentTypeHeader, "application/json");

        const QJsonObject body{
            {"phone",   number},
            {"message", msg}
        };
        QNetworkReply* reply =
            m_waPromoNam->post(
                req, QJsonDocument(body).toJson(QJsonDocument::Compact));

        const int idx = i + 1;
        connect(reply, &QNetworkReply::finished,
                this, [this, reply, idx, total, number]() {
            reply->deleteLater();
            if (reply->error() != QNetworkReply::NoError) {
                const QString errMsg =
                    QString("\xe2\x9d\x8c  Errore su %1: %2")
                        .arg(number, reply->errorString());
                m_waPromoStatusLbl->setText(errMsg);
                LogBus::post("WhatsApp: " + errMsg);
            } else if (idx == total) {
                m_waPromoStatusLbl->setText(
                    QString("\xe2\x9c\x85  Inviato a %1 contatti.").arg(total));
            }
        });
    }
}

/* ══════════════════════════════════════════════════════════════
   WhatsApp Bot Rispondente AI — polling bridge + risposta AI
   ══════════════════════════════════════════════════════════════ */

void AppControllerPage::onWaBotStartClicked()
{
    const QString bridgeUrl = m_waBridgeUrlEdit->text().trimmed();
    if (bridgeUrl.isEmpty()) {
        m_waBotStatusLbl->setText(
            "\xe2\x9d\x8c  Inserisci l\xe2\x80\x99" "URL del bridge prima di avviare.");
        return;
    }

    m_waSeenMsgIds.clear();
    m_waBotLog->clear();
    m_waBotLog->append(
        QString("\xf0\x9f\x9f\xa2  Bot avviato \xe2\x80\x94 bridge: <b>%1</b>")
            .arg(bridgeUrl.toHtmlEscaped()));

    if (!m_waPollTimer) {
        m_waPollTimer = new QTimer(this);
        m_waPollTimer->setInterval(2000);
        connect(m_waPollTimer, &QTimer::timeout,
                this, &AppControllerPage::onWaPollTick);
    }
    m_waPollTimer->start();

    m_waBotStartBtn->setEnabled(false);
    m_waBotStopBtn->setEnabled(true);
    m_waBotStatusLbl->setText(tr("\xf0\x9f\x9f\xa2  Bot attivo — polling ogni 2s"));

    QSettings s("Prismalux", "GUI");
    s.setValue("whatsapp/bot_whitelist", m_waWhitelistEdit->text().trimmed());
    s.setValue("whatsapp/bot_auto_reply", m_waAutoReplyCheck->isChecked());
}

void AppControllerPage::onWaBotStopClicked()
{
    if (m_waPollTimer) {
        m_waPollTimer->stop();
    }
    if (m_waBotAiHolder) {
        m_waBotAiHolder->deleteLater();
        m_waBotAiHolder = nullptr;
    }
    m_waBotStartBtn->setEnabled(true);
    m_waBotStopBtn->setEnabled(false);
    m_waBotStatusLbl->setText(tr("\xe2\x9a\xab  Bot fermato"));
    m_waBotLog->append("\xf0\x9f\x94\xb4  Bot fermato.");
}

void AppControllerPage::onWaPollTick()
{
    const QString bridgeUrl = m_waBridgeUrlEdit->text().trimmed();
    if (bridgeUrl.isEmpty()) return;

    const QUrl url(bridgeUrl.endsWith('/')
        ? bridgeUrl + "messages"
        : bridgeUrl + "/messages");

    QNetworkRequest req(url);
    req.setTransferTimeout(1500);
    auto* reply = m_waNam->get(req);
    connect(reply, &QNetworkReply::finished,
            this, &AppControllerPage::onWaPollReply);
}

void AppControllerPage::onWaPollReply()
{
    auto* reply = qobject_cast<QNetworkReply*>(sender());
    if (!reply) return;
    reply->deleteLater();

    if (reply->error() != QNetworkReply::NoError) {
        ++m_waPollFailCount;
        m_waBotStatusLbl->setText(
            QString("\xe2\x9a\xa0\xef\xb8\x8f  Bridge non raggiungibile: %1")
                .arg(reply->errorString()));
        /* Dopo 5 fallimenti consecutivi offri restart del polling */
        if (m_waPollFailCount == 5) {
            m_waBotLog->append(
                "<span style='color:#f87171;'>"
                "\xe2\x9a\xa0\xef\xb8\x8f  <b>Bridge irraggiungibile da 5 poll consecutivi.</b><br>"
                "Verifica che il server WhatsApp bridge sia avviato, poi:"
                "</span><br>"
                "<a href='wa-restart://' style='color:#fbbf24;'>"
                "\xf0\x9f\x94\x84 Riavvia polling WhatsApp</a>");
        }
        return;
    }
    m_waPollFailCount = 0;

    if (!m_waAutoReplyCheck || !m_waAutoReplyCheck->isChecked()) return;

    const QByteArray body = reply->readAll();
    const auto doc = QJsonDocument::fromJson(body);
    if (!doc.isArray()) return;

    const QStringList whitelist = m_waWhitelistEdit
        ? m_waWhitelistEdit->text().split(',', Qt::SkipEmptyParts)
        : QStringList{};

    const QJsonArray msgs = doc.array();
    for (const QJsonValue& v : msgs) {
        const QJsonObject msg = v.toObject();
        const QString id     = msg.value("id").toString();
        const QString from   = msg.value("from").toString().trimmed();
        const QString text   = msg.value("body").toString().trimmed();

        if (id.isEmpty() || m_waSeenMsgIds.contains(id)) continue;
        m_waSeenMsgIds.insert(id);

        if (text.isEmpty()) continue;

        /* Fail-closed: whitelist vuota = nessuno autorizzato (WhatsApp è rete pubblica).
           Confronto sul numero normalizzato (solo cifre) — niente match per sottostringa,
           che permetteva a una voce parziale di autorizzare numeri non previsti. */
        if (whitelist.isEmpty()) continue;
        QString fromNorm; for (const QChar& c : from) if (c.isDigit()) fromNorm += c;
        bool authorized = false;
        for (const QString& wl : whitelist) {
            QString wlNorm; for (const QChar& c : wl) if (c.isDigit()) wlNorm += c;
            if (wlNorm.size() < 6) continue;  // ignora voci troppo corte (evita match larghi)
            if (fromNorm == wlNorm || (wlNorm.size() >= 9 && fromNorm.endsWith(wlNorm))) {
                authorized = true; break;
            }
        }
        if (!authorized) continue;

        m_waBotLog->append(
            QString("\xf0\x9f\x93\xa8  <b>%1</b>: %2")
                .arg(from.toHtmlEscaped(), text.toHtmlEscaped()));

        /* Invia all'AI locale per la risposta */
        if (m_waBotAiHolder) {
            m_waBotAiHolder->deleteLater();
            m_waBotAiHolder = nullptr;
        }
        m_waBotAiHolder = new QObject(this);

        const QString sysPrompt =
            "Sei un assistente AI su WhatsApp. "
            "Rispondi in modo conciso e utile al messaggio dell'utente.";
        const QString fromCopy = from;

        connect(m_ai, &AiClient::finished, m_waBotAiHolder,
                [this, fromCopy](const QString& replyText) {
            if (m_waBotAiHolder) {
                m_waBotAiHolder->deleteLater();
                m_waBotAiHolder = nullptr;
            }
            onWaBotSendReply(fromCopy, replyText);
        });
        connect(m_ai, &AiClient::error, m_waBotAiHolder,
                [this](const QString& err) {
            if (m_waBotAiHolder) {
                m_waBotAiHolder->deleteLater();
                m_waBotAiHolder = nullptr;
            }
            m_waBotLog->append(
                QString("\xe2\x9d\x8c  AI error: %1").arg(err.toHtmlEscaped()));
        });
        m_ai->chat(sysPrompt, text);
        break;   /* un messaggio alla volta per non sovraccaricare l'AI */
    }
}

void AppControllerPage::onWaBotSendReply(const QString& toNumber,
                                          const QString& replyText)
{
    const QString bridgeUrl = m_waBridgeUrlEdit->text().trimmed();
    if (bridgeUrl.isEmpty() || replyText.trimmed().isEmpty()) return;

    m_waBotLog->append(
        QString("\xf0\x9f\xa4\x96  Risposta a <b>%1</b>: %2")
            .arg(toNumber.toHtmlEscaped(), replyText.left(120).toHtmlEscaped()));

    const QUrl url(bridgeUrl.endsWith('/')
        ? bridgeUrl + "send"
        : bridgeUrl + "/send");
    QNetworkRequest req(url);
    req.setHeader(QNetworkRequest::ContentTypeHeader, "application/json");
    req.setTransferTimeout(5000);

    const QJsonObject body{
        {"phone",   toNumber},
        {"message", replyText}
    };
    auto* reply = m_waNam->post(req,
        QJsonDocument(body).toJson(QJsonDocument::Compact));
    connect(reply, &QNetworkReply::finished, reply, [reply]() {
        reply->deleteLater();
    });
}
