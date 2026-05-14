#include "impostazioni_page.h"
#include "../widgets/toggle_switch.h"
#include "../widgets/stt_whisper.h"
#include "personalizza_page.h"
#include "manutenzione_page.h"
#include "grafico_page.h"
#include "../theme_manager.h"
#include "../prismalux_paths.h"
namespace P = PrismaluxPaths;
#include <QVBoxLayout>
#include <QHBoxLayout>
#include <QGridLayout>
#include <QFormLayout>
#include <QTabWidget>
#include <QScrollArea>
#include <QLabel>
#include <QPushButton>
#include <QFrame>
#include <QButtonGroup>
#include <QLineEdit>
#include <QComboBox>
#include <QSpinBox>
#include <QDoubleSpinBox>
#include <QSlider>
#include <QSettings>
#include <QDir>
#include <QDirIterator>
#include <QFile>
#include <QFileInfo>
#include <QProcess>
#include <QFileDialog>
#include <QDateTime>
#include <QStandardPaths>
#include <QDesktopServices>
#include <QUrl>
#include <QNetworkAccessManager>
#include <QNetworkRequest>
#include <QNetworkReply>
#include <functional>
#include <QCoreApplication>
#include <QTimer>
#include <QRadioButton>
#include <QCheckBox>
#include <QListWidget>
#include <QTableWidget>
#include <QHeaderView>
#include <QGroupBox>
#include <QPlainTextEdit>
#include <QThread>
#include <QStackedWidget>
#include <QColorDialog>
#include <QPainter>
#include <QPen>
#include <QJsonDocument>
#include <QJsonObject>
#include <QJsonArray>
#include <QGuiApplication>
#include <QClipboard>
#include <QMessageBox>
#include <QProgressBar>
#include <QTextEdit>
#include <QTextBrowser>
#include <QDialogButtonBox>
#include <QDialog>
#include <QRegularExpression>

struct ThemeVisual { QString id; QString bg; QString accent; QString bg2; };

static const ThemeVisual kVisuals[] = {
    { "dark_cyan",   "#13141f", "#00b8d9", "#1a1b28" },
    { "dark_amber",  "#17140c", "#ffb300", "#211c10" },
    { "dark_purple", "#130f1c", "#9c5ff0", "#1c1727" },
    { "light",       "#f0f2fa", "#0072c6", "#e0e4f0" },
};

static ThemeVisual visualFor(const QString& id) {
    for (const auto& v : kVisuals)
        if (v.id == id) return v;
    return { id, "#1e2030", "#607080", "#252a3a" };  /* fallback per custom */
}

QWidget* ImpostazioniPage::buildTemaTab() {
    auto* outer = new QWidget(this);
    auto* vlay  = new QVBoxLayout(outer);
    vlay->setContentsMargins(20, 20, 20, 20);
    vlay->setSpacing(16);

    /* ── Titolo ── */
    auto* title = new QLabel("\xf0\x9f\x8e\xa8  Seleziona tema", outer);
    title->setStyleSheet("font-size:16px; font-weight:700; color:#e5e7eb;");
    vlay->addWidget(title);

    auto* hint = new QLabel(
        "Copia file <b>.qss</b> nella cartella <code>themes/</code> accanto "
        "all'eseguibile per aggiungere temi personalizzati.", outer);
    hint->setWordWrap(true);
    hint->setStyleSheet("color:#9ca3af; font-size:12px;");
    vlay->addWidget(hint);

    /* ── Griglia card (4 per riga) ── */
    auto* scroll = new QScrollArea(outer);
    scroll->setWidgetResizable(true);
    scroll->setFrameShape(QFrame::NoFrame);
    auto* grid_w = new QWidget(scroll);
    auto* grid   = new QGridLayout(grid_w);
    grid->setSpacing(14);
    grid->setContentsMargins(0, 8, 0, 8);
    scroll->setWidget(grid_w);
    vlay->addWidget(scroll);

    ThemeManager* tm      = ThemeManager::instance();
    const auto&   themes  = tm->themes();
    const QString current = tm->currentId();

    /* Raggruppa i bottoni per gestire la selezione esclusiva */
    auto* group = new QButtonGroup(outer);
    group->setExclusive(true);

    const int COLS = 4;
    for (int i = 0; i < themes.size(); ++i) {
        const auto& t = themes[i];
        ThemeVisual v = visualFor(t.id);

        /* ── Contenitore card ── */
        auto* card = new QPushButton(grid_w);
        card->setCheckable(true);
        card->setChecked(t.id == current);
        card->setFixedSize(130, 100);
        card->setObjectName("themeCard");
        card->setProperty("themeId", t.id);

        /* Stile card: anteprima colori via QSS inline */
        const QString borderColor = (t.id == current) ? v.accent : "#404040";
        card->setStyleSheet(QString(
            "QPushButton#themeCard {"
            "  background: qlineargradient(x1:0,y1:0,x2:0,y2:1,"
            "    stop:0 %1, stop:0.65 %2, stop:0.66 %3, stop:1 %3);"
            "  border: 2px solid %4; border-radius: 10px;"
            "  color: %5; font-weight: 600; font-size: 11px;"
            "  padding-top: 54px;"
            "  text-align: center;"
            "}"
            "QPushButton#themeCard:checked {"
            "  border: 3px solid %3; border-radius: 10px;"
            "}"
            "QPushButton#themeCard:hover {"
            "  border-color: %3;"
            "}"
        ).arg(v.bg2, v.bg, v.accent, borderColor,
              (t.id == "light") ? "#1a1e30" : "#e5e7eb"));

        card->setText(t.label);
        group->addButton(card, i);

        /* Checkmark sovrapposto (visibile se selezionato) */
        auto* check = new QLabel("\xe2\x9c\x93", card);
        check->setAlignment(Qt::AlignTop | Qt::AlignRight);
        check->setStyleSheet(QString(
            "color: %1; font-size: 16px; font-weight: 700; "
            "background: transparent; padding: 4px 6px 0 0;")
            .arg(v.accent));
        check->setFixedSize(130, 30);
        check->setVisible(t.id == current);

        /* Connessione: cambia tema al click */
        connect(card, &QPushButton::clicked, card,
            [tm, t, themes, group, grid_w]() {
                tm->apply(t.id);
                /* Aggiorna border di tutte le card */
                for (int j = 0; j < themes.size(); ++j) {
                    auto* btn = qobject_cast<QPushButton*>(group->button(j));
                    if (!btn) continue;
                    ThemeVisual vj = visualFor(themes[j].id);
                    const QString bc = (themes[j].id == t.id) ? vj.accent : "#404040";
                    /* Aggiorna solo la border-color senza ricostruire tutto lo stile */
                    QString s = btn->styleSheet();
                    /* checkmark visibility */
                    const auto children = btn->findChildren<QLabel*>();
                    for (auto* lbl : children)
                        lbl->setVisible(themes[j].id == t.id);
                    (void)bc; (void)s;
                }
            });

        grid->addWidget(card, i / COLS, i % COLS);
    }

    /* Riga espandibile in fondo */
    grid->setRowStretch(themes.size() / COLS + 1, 1);
    /* ── Sezione: Aspetto bolle chat ── */
    auto* secBolle = new QFrame(outer);
    secBolle->setObjectName("cardFrame");
    auto* bolleLay = new QVBoxLayout(secBolle);
    bolleLay->setContentsMargins(16, 10, 16, 10);
    bolleLay->setSpacing(10);

    auto* bolleTitle = new QLabel(
        "\xf0\x9f\x92\xac  <b>Aspetto bolle chat</b>", secBolle);
    bolleTitle->setObjectName("cardTitle");
    bolleTitle->setTextFormat(Qt::RichText);
    bolleLay->addWidget(bolleTitle);

    auto* radiusRow = new QWidget(secBolle);
    auto* radiusLay = new QHBoxLayout(radiusRow);
    radiusLay->setContentsMargins(0, 0, 0, 0);
    radiusLay->setSpacing(10);

    auto* radiusLbl = new QLabel("Raggio bordi (px):", secBolle);
    radiusLbl->setObjectName("cardDesc");

    auto* radiusSpin = new QSpinBox(secBolle);
    radiusSpin->setRange(0, 24);
    radiusSpin->setSuffix(" px");
    radiusSpin->setFixedWidth(80);
    {
        QSettings s("Prismalux", "GUI");
        radiusSpin->setValue(s.value(P::SK::kBubbleRadius, 10).toInt());
    }

    auto* radiusPreview = new QLabel(secBolle);
    radiusPreview->setObjectName("cardDesc");
    radiusPreview->setText("(applicato alle nuove bolle)");

    auto updateRadius = [radiusSpin, radiusPreview]() {
        QSettings("Prismalux", "GUI").setValue(P::SK::kBubbleRadius, radiusSpin->value());
        radiusPreview->setText(
            radiusSpin->value() == 0
                ? "Bolle con angoli netti"
                : radiusSpin->value() <= 6
                    ? "Bolle leggermente arrotondate"
                    : radiusSpin->value() <= 14
                        ? "Bolle arrotondate (default)"
                        : "Bolle molto arrotondate");
    };
    updateRadius();

    QObject::connect(radiusSpin, QOverload<int>::of(&QSpinBox::valueChanged),
                     secBolle, [updateRadius](int){ updateRadius(); });

    radiusLay->addWidget(radiusLbl);
    radiusLay->addWidget(radiusSpin);
    radiusLay->addWidget(radiusPreview);
    radiusLay->addStretch();
    bolleLay->addWidget(radiusRow);
    vlay->addWidget(secBolle);

    /* ── Sezione: Modalità etichette barra di navigazione ── */
    auto* secNav = new QFrame(outer);
    secNav->setObjectName("cardFrame");
    auto* navLay = new QVBoxLayout(secNav);
    navLay->setContentsMargins(16, 10, 16, 10);
    navLay->setSpacing(8);

    auto* navTitle = new QLabel(
        "\xf0\x9f\x8f\xb7\xef\xb8\x8f  <b>Modalit\xc3\xa0 etichette tab</b>", secNav);
    navTitle->setObjectName("cardTitle");
    navTitle->setTextFormat(Qt::RichText);
    navLay->addWidget(navTitle);

    auto* navHint = new QLabel(
        "Scegli come visualizzare le etichette dei tab principali.", secNav);
    navHint->setObjectName("hintLabel");
    navLay->addWidget(navHint);

    struct NavMode { const char* label; const char* value; };
    static const NavMode kNavModes[] = {
        { "\xf0\x9f\x94\xa4  Solo icone",              "icons_only"   },
        { "\xf0\x9f\x94\xa4 Testo  Icone + testo",     "icons_text"   },
        { "Testo \xf0\x9f\x94\xa4  Testo + icone",     "text_icons"   },
        { "Aa  Solo testo",                             "text_only"    },
    };

    QSettings navSett("Prismalux", "GUI");
    const QString curNavMode = navSett.value(P::SK::kNavTabMode, "icons_text").toString();

    auto* navGroup = new QButtonGroup(secNav);
    auto* navRowW  = new QWidget(secNav);
    auto* navRowL  = new QHBoxLayout(navRowW);
    navRowL->setContentsMargins(0, 0, 0, 0);
    navRowL->setSpacing(8);

    for (const auto& nm : kNavModes) {
        auto* rb = new QRadioButton(QString::fromUtf8(nm.label), secNav);
        rb->setObjectName("cardDesc");
        rb->setChecked(curNavMode == nm.value);
        navGroup->addButton(rb);
        navRowL->addWidget(rb);

        connect(rb, &QRadioButton::toggled, this, [this, nm](bool checked){
            if (!checked) return;
            QSettings s("Prismalux", "GUI");
            s.setValue(P::SK::kNavTabMode, nm.value);
            emit tabModeChanged(QString::fromLatin1(nm.value));
        });
    }
    navRowL->addStretch();
    navLay->addWidget(navRowW);

    vlay->addWidget(secNav);

    /* ── Sezione: Stile navigazione principale ── */
    {
        auto* secStyle = new QFrame(outer);
        secStyle->setObjectName("cardFrame");
        auto* sLay = new QVBoxLayout(secStyle);
        sLay->setContentsMargins(16, 10, 16, 10);
        sLay->setSpacing(8);

        auto* sTitle = new QLabel(
            "\xf0\x9f\x97\x82\xef\xb8\x8f  <b>Stile navigazione</b>", secStyle);
        sTitle->setObjectName("cardTitle");
        sTitle->setTextFormat(Qt::RichText);
        sLay->addWidget(sTitle);

        auto* sHint = new QLabel(
            "Schede in alto (predefinito) oppure men\xc3\xb9 orizzontale a pulsanti con categorie.",
            secStyle);
        sHint->setObjectName("hintLabel");
        sHint->setWordWrap(true);
        sLay->addWidget(sHint);

        struct NavStyleOpt { const char* label; const char* value; };
        static const NavStyleOpt kStyles[] = {
            { "\xf0\x9f\x93\x91  Schede in alto",       "tabs_top"   },
            { "\xf0\x9f\x93\x8b  Men\xc3\xb9 principale", "menu_main" },
        };

        QSettings navSett2("Prismalux", "GUI");
        const QString curStyle = navSett2.value(P::SK::kNavStyle, "tabs_top").toString();

        auto* styleGroup = new QButtonGroup(secStyle);
        auto* styleRow   = new QWidget(secStyle);
        auto* styleRowL  = new QHBoxLayout(styleRow);
        styleRowL->setContentsMargins(0, 0, 0, 0);
        styleRowL->setSpacing(16);

        for (const auto& ns : kStyles) {
            auto* rb = new QRadioButton(QString::fromUtf8(ns.label), secStyle);
            rb->setObjectName("cardDesc");
            rb->setChecked(curStyle == ns.value);
            styleGroup->addButton(rb);
            styleRowL->addWidget(rb);
            connect(rb, &QRadioButton::toggled, this, [this, ns](bool checked){
                if (!checked) return;
                QSettings s("Prismalux", "GUI");
                s.setValue(P::SK::kNavStyle, ns.value);
                emit navStyleChanged(QString::fromLatin1(ns.value));
            });
        }
        styleRowL->addStretch();
        sLay->addWidget(styleRow);
        vlay->addWidget(secStyle);
    }

    /* ── Sezione: Pulsanti di esecuzione ── */
    {
        auto* secExec = new QFrame(outer);
        secExec->setObjectName("cardFrame");
        auto* eLay = new QVBoxLayout(secExec);
        eLay->setContentsMargins(16, 10, 16, 10);
        eLay->setSpacing(8);

        auto* eTitle = new QLabel(
            "\xe2\x96\xb6  <b>Pulsanti di esecuzione</b>", secExec);
        eTitle->setObjectName("cardTitle");
        eTitle->setTextFormat(Qt::RichText);
        eLay->addWidget(eTitle);

        auto* eHint = new QLabel(
            "Avvia, Stop, Esegui, Calcola e simili \xe2\x80\x94 in tutte le schede.", secExec);
        eHint->setObjectName("hintLabel");
        eHint->setWordWrap(true);
        eLay->addWidget(eHint);

        struct ExecMode { const char* label; const char* value; };
        static const ExecMode kExecModes[] = {
            { "\xf0\x9f\x94\xa4  Solo icone",  "icon_only"  },
            { "Aa  Solo testo",                 "text_only"  },
            { "\xf0\x9f\x94\xa4 Aa  Icona + testo", "icon_text" },
        };

        QSettings exSett("Prismalux", "GUI");
        const QString curExec = exSett.value(P::SK::kNavExecBtnMode, "icon_text").toString();

        auto* execGroup = new QButtonGroup(secExec);
        auto* execRow   = new QWidget(secExec);
        auto* execRowL  = new QHBoxLayout(execRow);
        execRowL->setContentsMargins(0, 0, 0, 0);
        execRowL->setSpacing(16);

        for (const auto& em : kExecModes) {
            auto* rb = new QRadioButton(QString::fromUtf8(em.label), secExec);
            rb->setObjectName("cardDesc");
            rb->setChecked(curExec == em.value);
            execGroup->addButton(rb);
            execRowL->addWidget(rb);
            connect(rb, &QRadioButton::toggled, this, [this, em](bool checked){
                if (!checked) return;
                QSettings s("Prismalux", "GUI");
                s.setValue(P::SK::kNavExecBtnMode, em.value);
                emit execBtnModeChanged(QString::fromLatin1(em.value));
            });
        }
        execRowL->addStretch();
        eLay->addWidget(execRow);
        vlay->addWidget(secExec);
    }

    vlay->addStretch();
    return outer;
}

