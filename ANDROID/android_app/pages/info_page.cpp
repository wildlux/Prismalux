#include "info_page.h"
#include <QVBoxLayout>
#include <QLabel>
#include <QScrollArea>
#include <QGroupBox>
#include <QFrame>
#include <QPixmap>
#include <QScroller>
#include <QScrollerProperties>

InfoPage::InfoPage(QWidget* parent) : QWidget(parent)
{
    setObjectName("InfoPage");

    auto* scrollArea = new QScrollArea(this);
    scrollArea->setWidgetResizable(true);
    scrollArea->setObjectName("SettingsScroll");
    scrollArea->setHorizontalScrollBarPolicy(Qt::ScrollBarAlwaysOff);
    scrollArea->setVerticalScrollBarPolicy(Qt::ScrollBarAlwaysOff);
    scrollArea->setFrameShape(QFrame::NoFrame);
    QScroller::grabGesture(scrollArea->viewport(), QScroller::TouchGesture);
    QScroller* qs = QScroller::scroller(scrollArea->viewport());
    QScrollerProperties sp = qs->scrollerProperties();
    sp.setScrollMetric(QScrollerProperties::OvershootDragDistanceFactor, 0.0);
    sp.setScrollMetric(QScrollerProperties::OvershootScrollDistanceFactor, 0.0);
    sp.setScrollMetric(QScrollerProperties::HorizontalOvershootPolicy,
        QVariant::fromValue<QScrollerProperties::OvershootPolicy>(
            QScrollerProperties::OvershootAlwaysOff));
    sp.setScrollMetric(QScrollerProperties::VerticalOvershootPolicy,
        QVariant::fromValue<QScrollerProperties::OvershootPolicy>(
            QScrollerProperties::OvershootAlwaysOff));
    qs->setScrollerProperties(sp);

    auto* inner = new QWidget;
    auto* vbox  = new QVBoxLayout(inner);
    vbox->setSpacing(14);
    vbox->setContentsMargins(12, 12, 12, 12);
    scrollArea->setWidget(inner);

    auto* outer = new QVBoxLayout(this);
    outer->setContentsMargins(0, 0, 0, 0);
    outer->addWidget(scrollArea);

    /* ── Logo + versione ── */
    auto* logoBox = new QVBoxLayout;
    logoBox->setAlignment(Qt::AlignCenter);
    auto* logoLbl = new QLabel(inner);
    QPixmap pm(":/images/prismalux_logo.png");
    logoLbl->setPixmap(pm.scaled(88, 88, Qt::KeepAspectRatio, Qt::SmoothTransformation));
    logoLbl->setAlignment(Qt::AlignCenter);
    auto* appNameLbl = new QLabel("PrismaluxMobile", inner);
    appNameLbl->setAlignment(Qt::AlignCenter);
    QFont nf = appNameLbl->font();
    nf.setPointSize(17); nf.setBold(true);
    appNameLbl->setFont(nf);
    auto* verLbl = new QLabel("v1.0  —  build Qt6 Android arm64", inner);
    verLbl->setAlignment(Qt::AlignCenter);
    verLbl->setStyleSheet("color: #8890a8; font-size: 13px;");
    auto* mottoLbl = new QLabel(
        QString::fromUtf8(
            "\xf0\x9f\x8d\xba"   /* 🍺 */
            "  Costruito per i mortali che aspirano alla saggezza."),
        inner);
    mottoLbl->setAlignment(Qt::AlignCenter);
    mottoLbl->setWordWrap(true);
    mottoLbl->setStyleSheet("color: #00c8ff; font-size: 14px; padding: 4px 0;");
    logoBox->addWidget(logoLbl);
    logoBox->addSpacing(6);
    logoBox->addWidget(appNameLbl);
    logoBox->addWidget(verLbl);
    logoBox->addWidget(mottoLbl);
    vbox->addLayout(logoBox);

    /* ── Guida rapida connessione ── */
    auto* guideGroup = new QGroupBox(
        "\xf0\x9f\x93\x8b  Come connettere l\xe2\x80\x99" "app al PC", inner);
    guideGroup->setObjectName("SettingsGroup");
    auto* guideLay = new QVBoxLayout(guideGroup);
    guideLay->setSpacing(6);

    auto* guideLbl = new QLabel(guideGroup);
    guideLbl->setTextFormat(Qt::RichText);
    guideLbl->setWordWrap(true);
    guideLbl->setOpenExternalLinks(false);
    guideLbl->setText(
        "<b>Sul PC (Prismalux Desktop):</b><br>"
        "<ol style='margin:4px 0 4px 16px; padding:0'>"
        "<li>Apri il tab <b>\xf0\x9f\x8c\x90 LAN &amp; WAN</b></li>"
        "<li>Attiva il toggle <b>Avvia server LAN</b></li>"
        "<li>Segna l\xe2\x80\x99<b>indirizzo IP</b> mostrato (es. 192.168.1.165)</li>"
        "<li>Copia il <b>token segreto</b> \xf0\x9f\x94\x91 mostrato nel pannello</li>"
        "</ol>"
        "<b>Su questo telefono:</b><br>"
        "<ol style='margin:4px 0 4px 16px; padding:0'>"
        "<li>Vai in <b>\xe2\x9a\x99\xef\xb8\x8f Impostazioni</b></li>"
        "<li>Inserisci l\xe2\x80\x99IP del PC nel campo <b>Indirizzo IP</b></li>"
        "<li>Lascia la porta <b>11500</b> (LAN server Prismalux)</li>"
        "<li>Incolla il token nel campo <b>\xf0\x9f\x94\x91 Token</b></li>"
        "<li>Premi <b>\xf0\x9f\x94\x8c Testa</b> \xe2\x80\x94 deve apparire \xe2\x9c\x85</li>"
        "<li>Premi <b>Applica</b> per salvare</li>"
        "</ol>"
        "<small>\xf0\x9f\x93\xb6 PC e telefono devono essere sulla stessa rete Wi-Fi.<br>"
        "\xf0\x9f\x94\x92 Il token non viene trasmesso in chiaro \xe2\x80\x94 "
        "usa la porta 11500 (non la 80).</small>");
    guideLay->addWidget(guideLbl);
    vbox->addWidget(guideGroup);

    /* ── Architettura ── */
    auto* archGroup = new QGroupBox(
        "\xf0\x9f\x94\xa7  Architettura", inner);
    archGroup->setObjectName("SettingsGroup");
    auto* archLay = new QVBoxLayout(archGroup);
    auto* archLbl = new QLabel(archGroup);
    archLbl->setTextFormat(Qt::RichText);
    archLbl->setWordWrap(true);
    archLbl->setText(
        "<b>Backend supportati:</b><br>"
        "<ul style='margin:4px 0 4px 16px; padding:0'>"
        "<li>\xf0\x9f\x93\xb6 <b>LAN server Prismalux</b> — porta 11500, token cifrato</li>"
        "<li>\xe2\x98\x81\xef\xb8\x8f <b>Ollama remoto</b> — qualsiasi IP pubblico, porta 11434</li>"
        "<li>\xf0\x9f\x93\xb1 <b>LLM locale</b> — Qwen2.5 GGUF su CPU arm64 NEON, offline</li>"
        "</ul>"
        "<b>RAG locale:</b> documenti .txt / .md / .pdf indicizzati sul dispositivo.<br>"
        "<b>OCR:</b> cattura testo da immagini via Camera e lo inietta nella chat.");
    archLay->addWidget(archLbl);
    vbox->addWidget(archGroup);

    /* ── Crediti ── */
    auto* creditGroup = new QGroupBox(
        "\xf0\x9f\x8f\x86  Crediti", inner);
    creditGroup->setObjectName("SettingsGroup");
    auto* creditLay  = new QVBoxLayout(creditGroup);
    auto* creditLbl  = new QLabel(
        "Qt 6  \xe2\x80\xa2  llama.cpp  \xe2\x80\xa2  ZXing-cpp<br>"
        "Modelli: Qwen2.5 (Alibaba Cloud)<br>"
        "Sviluppato da <b>wildlux</b>",
        creditGroup);
    creditLbl->setTextFormat(Qt::RichText);
    creditLbl->setWordWrap(true);
    creditLay->addWidget(creditLbl);
    vbox->addWidget(creditGroup);

    /* ── Ringraziamenti ── */
    auto* thanksGroup = new QGroupBox(
        "\xf0\x9f\x99\x8f  Ringraziamenti", inner);   /* 🙏 */
    thanksGroup->setObjectName("SettingsGroup");
    auto* thanksLay = new QVBoxLayout(thanksGroup);
    auto* thanksLbl = new QLabel(thanksGroup);
    thanksLbl->setTextFormat(Qt::RichText);
    thanksLbl->setWordWrap(true);
    thanksLbl->setText(
        "<b>Un ringraziamento speciale a:</b><br><br>"
        "\xf0\x9f\x8d\xba  <b>Alla comunità open source</b><br>"        /* 🍺 */
        "<small>llama.cpp, Qt, ZXing-cpp, SQLite — senza di voi nulla di questo sarebbe possibile.</small><br><br>"
        "\xf0\x9f\x92\xa1  <b>Georgi Gerganov</b><br>"                 /* 💡 */
        "<small>Creatore di llama.cpp — ha reso gli LLM accessibili a tutti.</small><br><br>"
        "\xf0\x9f\x94\xac  <b>Ollama Team</b><br>"                     /* 🔬 */
        "<small>Per aver semplificato il deployment dei modelli locali.</small><br><br>"
        "\xf0\x9f\x93\xb1  <b>Xiaomi / Redmi</b><br>"                  /* 📱 */
        "<small>Hardware accessibile che fa girare LLM sul palmo della mano.</small><br><br>"
        "\xf0\x9f\x8c\x90  <b>A te che usi Prismalux</b><br>"          /* 🌐 */
        "<small>\"Costruito per i mortali che aspirano alla saggezza.\" "
        "\xf0\x9f\x8d\xba</small>");                                    /* 🍺 */
    thanksLay->addWidget(thanksLbl);
    vbox->addWidget(thanksGroup);

    vbox->addStretch();
}
