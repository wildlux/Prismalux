#include "settings_main.h"
#include "settings_model_profiles.h"
#include "main_mcp_manager.h"
#include "../widgets/lazy_tab_loader.h"
#include "../dpi_utils.h"
#include "../ai_memory.h"
#include "../log_bus.h"
#include "../widgets/toggle_switch.h"
#include "../widgets/stt_whisper.h"
#include "../widgets/widget_dep_check.h"
#include "../widgets/external_ai_import.h"
#include "main_customize.h"
#include "main_maintenance.h"
#include "main_graph.h"
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
#include <QHash>
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
#include <QPalette>
#include <QClipboard>
#include <QMessageBox>
#include <QProgressBar>
#include <QTextEdit>
#include <QTextBrowser>
#include <QDialogButtonBox>
#include <QDialog>
#include <QRegularExpression>
#include <QMouseEvent>
#include <cmath>

/* ── Helper: estrae i colori chiave dal QSS corrente dell'applicazione ──
   Tutti i QSS Prismalux hanno nella riga di commento:
     accent #XXXXXX | txt1 #XXXXXX | txt2 #XXXXXX
   e la riga:
     QMainWindow, QWidget { background-color: #XXXXXX; }
   Fallback robusto se un tema custom non segue questo schema.            */
static void extractQssColors(QString& bg, QString& bg2, QString& bg3,
                              QString& text, QString& muted,
                              QString& accent, QString& border)
{
    const QPalette pal = QGuiApplication::palette();

    bg     = pal.color(QPalette::Window).name();
    text   = pal.color(QPalette::WindowText).name();
    accent = pal.color(QPalette::Highlight).name();
    muted  = pal.color(QPalette::PlaceholderText).name();

    /* PlaceholderText può coincidere con Window in alcuni stili */
    if (muted == bg || muted == "#000000") {
        const QColor bgC(bg);
        muted = bgC.lightness() < 128
            ? bgC.lighter(210).name()
            : bgC.darker(175).name();
    }

    const QColor bgC(bg);
    const bool dark = bgC.lightness() < 128;
    bg2    = (dark ? bgC.lighter(118) : bgC.darker(107)).name();
    bg3    = (dark ? bgC.lighter(140) : bgC.darker(116)).name();
    border = (dark ? bgC.lighter(165) : bgC.darker(128)).name();
}

static void settingsDoResetOnboarding(QPushButton* btn)
{
    QSettings s("Prismalux", "GUI");
    s.remove("setup/done");
    btn->setText(QObject::tr("\xe2\x9c\x85  Reimpostato \xe2\x80\x94 riavvia Prismalux"));
    btn->setEnabled(false);
}

static void settingsRefreshAiMemoryLog(AIMemory* mem, QListWidget* logList)
{
    logList->clear();
    for (const QString& entry : mem->gitLog(30))
        logList->addItem(entry);
    if (logList->count() == 0)
        logList->addItem("(nessun commit \xe2\x80\x94 scrivi qualcosa nella chat AI per cominciare)");
}

static void settingsDoRevertAiMemory(AIMemory* mem, QListWidget* logList, QWidget* w)
{
    QListWidgetItem* item = logList->currentItem();
    if (!item || item->text().startsWith('(')) {
        QMessageBox::information(w, QObject::tr("Seleziona un commit"),
            QObject::tr("Seleziona un commit dalla lista per ripristinare le preferenze."));
        return;
    }
    const QString hash = item->text().split(' ').first();
    if (QMessageBox::question(w, QObject::tr("Ripristina preferenze"),
            QObject::tr("Ripristinare <code>profile/preferences.yaml</code> al commit <b>")
            + hash + "</b>?",
            QMessageBox::Yes | QMessageBox::No) == QMessageBox::Yes) {
        if (mem->revertFile(hash, "profile/preferences.yaml")) {
            QMessageBox::information(w, QObject::tr("Completato"),
                QObject::tr("preferences.yaml ripristinato al commit ") + hash + ".");
            settingsRefreshAiMemoryLog(mem, logList);
        } else {
            QMessageBox::warning(w, QObject::tr("Errore"),
                QObject::tr("Impossibile ripristinare. Verifica che git sia installato e il commit esista."));
        }
    }
}

static void settingsLoadFeedbackData(QTableWidget* table, QLabel* statLbl)
{
    const QString path = PrismaluxPaths::feedbackPath();
    QFile f(path);
    table->setRowCount(0);
    if (!f.exists()) {
        statLbl->setText(QObject::tr("\xe2\x84\xb9  Nessun feedback raccolto ancora."));
        return;
    }
    QMap<QString, QPair<int,int>> counts;
    if (f.open(QIODevice::ReadOnly | QIODevice::Text)) {
        while (!f.atEnd()) {
            const QByteArray line = f.readLine().trimmed();
            if (line.isEmpty()) continue;
            QJsonParseError pe;
            auto doc = QJsonDocument::fromJson(line, &pe);
            if (pe.error != QJsonParseError::NoError) continue;
            const QString model  = doc["model"].toString();
            const int     rating = doc["rating"].toInt();
            auto& p = counts[model];
            if (rating > 0) p.first++;
            else             p.second++;
        }
        f.close();
    }
    int totalPos = 0, totalNeg = 0;
    for (auto it = counts.cbegin(); it != counts.cend(); ++it) {
        const int row = table->rowCount();
        table->insertRow(row);
        table->setItem(row, 0, new QTableWidgetItem(it.key()));
        table->setItem(row, 1, new QTableWidgetItem(QString::number(it.value().first)));
        table->setItem(row, 2, new QTableWidgetItem(QString::number(it.value().second)));
        table->setItem(row, 3, new QTableWidgetItem(
            QString::number(it.value().first + it.value().second)));
        totalPos += it.value().first;
        totalNeg += it.value().second;
    }
    statLbl->setText(QString(
        QObject::tr("\xf0\x9f\x93\x88  Totale: %1 positivi / %2 negativi su %3 risposte valutate"))
        .arg(totalPos).arg(totalNeg).arg(totalPos + totalNeg));
}

static void settingsDoExportDpo(QWidget* w, QLabel* statLbl)
{
    const QString src = PrismaluxPaths::feedbackPath();
    QFile f(src);
    if (!f.exists() || !f.open(QIODevice::ReadOnly | QIODevice::Text)) {
        statLbl->setText(QObject::tr("\xe2\x9d\x8c  Nessun dato da esportare."));
        return;
    }
    QJsonArray dataset;
    QHash<QString, QJsonObject> positives;
    while (!f.atEnd()) {
        const QByteArray line = f.readLine().trimmed();
        if (line.isEmpty()) continue;
        QJsonParseError pe;
        auto doc = QJsonDocument::fromJson(line, &pe);
        if (pe.error != QJsonParseError::NoError) continue;
        const QString prompt   = doc["prompt"].toString();
        const QString response = doc["response"].toString();
        const int     rating   = doc["rating"].toInt();
        const QString model    = doc["model"].toString();
        if (rating > 0) {
            positives[prompt] = doc.object();
        } else if (rating < 0 && positives.contains(prompt)) {
            QJsonObject pair;
            pair["instruction"] = prompt;
            pair["chosen"]      = positives[prompt]["response"].toString();
            pair["rejected"]    = response;
            pair["model"]       = model;
            dataset.append(pair);
        }
    }
    f.close();
    const QString outPath = QDir::homePath()
        + "/.prismalux/dpo_dataset_"
        + QDateTime::currentDateTime().toString("yyyyMMdd_HHmmss")
        + ".json";
    QFile out(outPath);
    if (out.open(QIODevice::WriteOnly | QIODevice::Text)) {
        out.write(QJsonDocument(dataset).toJson(QJsonDocument::Indented));
        out.close();
        statLbl->setText(
            QString(QObject::tr("\xe2\x9c\x85  Dataset DPO esportato: %1 coppie in %2"))
            .arg(dataset.size()).arg(outPath));
    } else {
        statLbl->setText(QObject::tr("\xe2\x9d\x8c  Impossibile scrivere: ") + outPath);
    }
}

static void settingsDoClrFeedback(QWidget* w, QTableWidget* table, QLabel* statLbl)
{
    if (QMessageBox::question(w,
            QObject::tr("Cancella feedback"),
            QObject::tr("Eliminare tutti i dati di feedback?\nQuesta azione \xc3\xa8 irreversibile."),
            QMessageBox::Yes | QMessageBox::No) != QMessageBox::Yes)
        return;
    QFile::remove(PrismaluxPaths::feedbackPath());
    table->setRowCount(0);
    statLbl->setText(QObject::tr("\xf0\x9f\x97\x91  Dati feedback cancellati."));
}

/* ══════════════════════════════════════════════════════════════
   settingsDoImportExternalAi — importa export JSON di ChatGPT/Claude/
   formato generico OpenAI-compatible (DeepSeek/Qwen/altri, se esportati
   via strumenti terzi) nello storico chat locale. Vedi
   widgets/external_ai_import.h per il rilevamento formato.
   ══════════════════════════════════════════════════════════════ */
static void settingsDoImportExternalAi(QWidget* w, QLabel* statLbl)
{
    const QString path = QFileDialog::getOpenFileName(w,
        QObject::tr("Importa conversazioni da AI esterne"),
        QDir::homePath(), QObject::tr("File JSON (*.json)"));
    if (path.isEmpty()) return;

    statLbl->setText(QObject::tr("\xe2\x8f\xb3  Analisi in corso..."));

    QString detectedFormat, errorMsg;
    const auto convs = ExternalAiImport::parseFile(path, detectedFormat, errorMsg);

    if (!errorMsg.isEmpty()) {
        statLbl->setText(QString("\xe2\x9d\x8c  ") + errorMsg);
        return;
    }
    if (convs.isEmpty()) {
        statLbl->setText(QObject::tr(
            "\xe2\x9a\xa0  Nessuna conversazione trovata nel file (formato riconosciuto: %1)."
            ).arg(detectedFormat));
        return;
    }

    const int saved = ExternalAiImport::importIntoHistory(convs);
    statLbl->setText(QString(
        "\xe2\x9c\x85  Importate %1 conversazioni (formato: %2) nello storico chat "
        "\xe2\x80\x94 visibili in \xf0\x9f\xa4\x96 Intelligenza Artificiale \xe2\x86\x92 Cronologia.")
        .arg(saved).arg(detectedFormat));
}

/* ══════════════════════════════════════════════════════════════
   Easter egg astrale — si attiva con doppio click su "saggezza"
   ══════════════════════════════════════════════════════════════ */
static void showAstroEaster(QWidget* parent)
{
    auto* dlg = new QDialog(parent, Qt::Dialog);
    dlg->setAttribute(Qt::WA_DeleteOnClose);
    dlg->setWindowTitle(QObject::tr("\xe2\x9c\xa8 Segreto delle Stelle"));
    dlg->setFixedSize(dpiScale(460), dpiScale(450));
    dlg->setStyleSheet("QDialog{background:#080818;} QLabel{color:#e8d5b7;}");

    const int SZ = dpiScale(290);
    QPixmap px(SZ, SZ);
    px.fill(QColor(8, 8, 24));
    {
        QPainter p(&px);
        p.setRenderHint(QPainter::Antialiasing);

        const QPointF ctr(SZ / 2.0, SZ / 2.0);
        const double R1 = SZ * 0.46;
        const double R2 = SZ * 0.37;
        const double R3 = SZ * 0.28;
        const double R4 = SZ * 0.17;

        /* stelle sfondo */
        p.setPen(QColor(255, 255, 255, 90));
        for (int i = 0; i < 50; ++i) {
            double a = i * 137.508 * M_PI / 180.0;
            double r = SZ * 0.04 + (i % 9) * SZ * 0.045;
            if (r >= R2 - 2) r = SZ * 0.03 + (i % 4) * SZ * 0.04;
            p.drawPoint(QPointF(ctr.x() + r * cos(a), ctr.y() + r * sin(a)));
        }

        /* cerchi */
        p.setBrush(Qt::NoBrush);
        p.setPen(QPen(QColor("#4a3f6b"), 1));
        p.drawEllipse(ctr, R1, R1);
        p.drawEllipse(ctr, R2, R2);
        p.drawEllipse(ctr, R3, R3);
        p.setPen(QPen(QColor("#7060a0"), 1));
        p.drawEllipse(ctr, R4, R4);

        /* 12 segni zodiacali */
        static const char* kZ[12] = {
            "\xe2\x99\x88","\xe2\x99\x89","\xe2\x99\x8a","\xe2\x99\x8b",
            "\xe2\x99\x8c","\xe2\x99\x8d","\xe2\x99\x8e","\xe2\x99\x8f",
            "\xe2\x99\x90","\xe2\x99\x91","\xe2\x99\x92","\xe2\x99\x93"
        };
        static const QColor kZC[12] = {
            QColor("#e84040"), QColor("#c27a00"), QColor("#e8c840"),
            QColor("#40a8e8"), QColor("#e8a000"), QColor("#60b060"),
            QColor("#e87090"), QColor("#9040b0"), QColor("#e86020"),
            QColor("#6090b0"), QColor("#40b0d0"), QColor("#8080d0")
        };
        QFont zf("DejaVu Sans", qMax(6, int(SZ * 0.045)));
        p.setFont(zf);
        for (int i = 0; i < 12; ++i) {
            double a0 = (-90.0 + i * 30.0) * M_PI / 180.0;
            double am = (-90.0 + i * 30.0 + 15.0) * M_PI / 180.0;
            p.setPen(QPen(QColor("#4a3f6b"), 1));
            p.drawLine(QPointF(ctr.x() + R2 * cos(a0), ctr.y() + R2 * sin(a0)),
                       QPointF(ctr.x() + R1 * cos(a0), ctr.y() + R1 * sin(a0)));
            double sr = (R1 + R2) / 2.0;
            QPointF sp(ctr.x() + sr * cos(am), ctr.y() + sr * sin(am));
            int fs = qMax(6, int(SZ * 0.045));
            QRectF sr2(sp.x() - fs, sp.y() - fs, fs * 2, fs * 2);
            p.setPen(kZC[i]);
            p.drawText(sr2, Qt::AlignCenter, QString::fromUtf8(kZ[i]));
        }

        /* aspetti */
        p.setPen(QPen(QColor(100, 80, 200, 100), 1, Qt::DotLine));
        int asp[][2] = {{0,6},{1,7},{2,8},{3,9},{4,10},{5,11},{0,4},{1,5}};
        for (auto& a : asp) {
            double a1 = (-90.0 + a[0] * 30.0 + 15.0) * M_PI / 180.0;
            double a2 = (-90.0 + a[1] * 30.0 + 15.0) * M_PI / 180.0;
            p.drawLine(QPointF(ctr.x() + R3 * cos(a1), ctr.y() + R3 * sin(a1)),
                       QPointF(ctr.x() + R3 * cos(a2), ctr.y() + R3 * sin(a2)));
        }

        /* 4 pianeti nascosti */
        struct PE { double lon; const char* name; QColor col; };
        static const PE kPE[] = {
            { 45.0,  "\xf0\x9f\x94\xac Ricerca",       QColor("#40d8c0") },
            { 135.0, "\xf0\x9f\x9b\xa0 Strumenti",     QColor("#e0c040") },
            { 225.0, "\xf0\x9f\x8c\x9f Astrologia",    QColor("#e08060") },
            { 315.0, "\xf0\x9f\x93\x90 Carta Astrale", QColor("#c080e0") },
        };
        QFont lf("DejaVu Sans", qMax(5, int(SZ * 0.038)));
        p.setFont(lf);
        for (const auto& pe : kPE) {
            double ar = (pe.lon - 90.0) * M_PI / 180.0;
            double r  = (R3 + R4) / 2.0;
            QPointF pt(ctr.x() + r * cos(ar), ctr.y() + r * sin(ar));
            p.setPen(Qt::NoPen);
            p.setBrush(pe.col);
            p.drawEllipse(pt, dpiScale(4), dpiScale(4));
            p.setPen(pe.col);
            int fw = qMax(40, int(SZ * 0.36));
            int fh = qMax(12, int(SZ * 0.075));
            p.drawText(QRectF(pt.x() - fw/2.0, pt.y() + dpiScale(5), fw, fh),
                       Qt::AlignCenter, QString::fromUtf8(pe.name));
        }

        /* logo centrale */
        QFont cf("DejaVu Sans", qMax(12, int(SZ * 0.07)), QFont::Bold);
        p.setFont(cf);
        p.setPen(QColor("#e8d5b7"));
        p.drawText(QRectF(ctr.x() - 20, ctr.y() - 12, 40, 24),
                   Qt::AlignCenter, "\xf0\x9f\x8d\xba");
    }

    auto* lay   = new QVBoxLayout(dlg);
    auto* title = new QLabel(QObject::tr("\xe2\x9c\xa8\xc2\xa0 Segreto delle Stelle \xc2\xa0\xe2\x9c\xa8"));
    title->setAlignment(Qt::AlignCenter);
    title->setStyleSheet("font-size:15px;font-weight:700;color:#e8d5b7;margin:6px 0 2px;");

    auto* canvas = new QLabel(dlg);
    canvas->setPixmap(px);
    canvas->setAlignment(Qt::AlignCenter);

    auto* sub = new QLabel(
        "<center>"
        "<span style='color:#9a8fba;font-size:12px;font-style:italic;'>"
        "\xf0\x9f\x8c\x9f\xc2\xa0"
        "Astrologia \xc2\xb7 Strumenti \xc2\xb7 Ricerca \xc2\xb7 Carta Astrale"
        "\xc2\xa0\xf0\x9f\x8c\x9f</span><br>"
        "<span style='color:#5a506a;font-size:11px;'>"
        "&ldquo;Le stelle custodiscono i segreti di chi cerca la saggezza.&rdquo;"
        "</span></center>");
    sub->setWordWrap(true);

    auto* btn = new QPushButton(QObject::tr("Chiudi"));
    btn->setStyleSheet(
        "QPushButton{background:#1a1232;color:#c8b8e8;border:1px solid #5a4f8a;"
        "border-radius:6px;padding:5px 22px;font-size:12px;}"
        "QPushButton:hover{background:#2a2042;}");
    QObject::connect(btn, &QPushButton::clicked, dlg, &QDialog::accept);

    lay->addWidget(title);
    lay->addWidget(canvas);
    lay->addWidget(sub);
    lay->addStretch();
    lay->addWidget(btn, 0, Qt::AlignCenter);
    lay->setContentsMargins(16, 6, 16, 12);
    lay->setSpacing(6);

    /* D-69: show() non-modale — exec() dentro un event filter blocca
       l'event loop (e i test); il dialog è decorativo, WA_DeleteOnClose
       fa già la pulizia. */
    dlg->show();
}

/* Sblocca la sotto-tab "Carta Astrale" (nascosta di default, vedi
   RicercaPage/main_research.cpp) — persistente, effetto reale al
   prossimo avvio/riapertura della tab Ricerca: ImpostazioniPage e
   RicercaPage sono pagine sorelle senza riferimento diretto, cablare
   un segnale cross-page attraverso MainWindow per un easter egg
   sarebbe sproporzionato. Il messaggio lo dice esplicitamente. */
static void unlockAstraleEasterEgg(QWidget* parent)
{
    QSettings s("Prismalux", "GUI");
    if (!s.value(P::SK::kAstraleUnlocked, false).toBool()) {
        s.setValue(P::SK::kAstraleUnlocked, true);
        QMessageBox::information(parent, QObject::tr("\xf0\x9f\x94\x93  Sbloccato"),
            "Carta Astrale sbloccata \xe2\x80\x94 riavvia Prismalux (o riapri la "
            "scheda Ricerca) per vederla.");
    }
}

/* Filtro eventi: intercetta doppio click su "saggezza" (easter egg
   decorativo esistente, ruota zodiacale) o "conoscenza" (sblocco reale
   Carta Astrale, indipendente dal primo) nel QTextBrowser */
class AstroEggFilter : public QObject {
public:
    explicit AstroEggFilter(QObject* parent) : QObject(parent) {}
protected:
    bool eventFilter(QObject* obj, QEvent* ev) override {
        if (ev->type() == QEvent::MouseButtonDblClick) {
            /* D-69: i doppi click arrivano al VIEWPORT del QTextBrowser,
               non al browser — col filtro installato solo sul browser
               l'egg non scattava MAI (scoperto dal test CAT-F con eventi
               mouse reali). Si risale al parent quando obj è il viewport. */
            QTextBrowser* vb = qobject_cast<QTextBrowser*>(obj);
            if (!vb && obj->isWidgetType())
                vb = qobject_cast<QTextBrowser*>(
                         static_cast<QWidget*>(obj)->parentWidget());
            if (auto* b = vb) {
                QTextCursor cur = b->cursorForPosition(
                    static_cast<QMouseEvent*>(ev)->pos());
                cur.select(QTextCursor::WordUnderCursor);
                const QString word = cur.selectedText();
                if (word.contains("saggezza", Qt::CaseInsensitive)) {
                    showAstroEaster(b);
                    return true;
                }
                if (word.contains("conoscenza", Qt::CaseInsensitive)) {
                    unlockAstraleEasterEgg(b);
                    return true;
                }
            }
        }
        return QObject::eventFilter(obj, ev);
    }
};

QWidget* ImpostazioniPage::buildRingraziamentiTab()
{
    /* ── CSS parametrizzato: %1=bg %2=text %3=accent %4=muted %5=border %6=bg3 %7=bg2 ── */
    static const char* kCssFmt =
        "body  { background:%1; color:%2; "
        "        font-family:'Segoe UI',system-ui,sans-serif; "
        "        font-size:13px; margin:0; padding:10px 14px; }"
        "h1    { color:%2; font-size:20px; font-weight:700; "
        "        text-align:center; margin:0 0 3px 0; }"
        ".sub  { text-align:center; color:%4; font-style:italic; "
        "        font-size:12px; margin:0 0 2px 0; }"
        ".ver  { text-align:center; color:%2; font-size:12px; "
        "        margin:0 0 2px 0; }"
        ".lnks { text-align:center; font-size:12px; margin:0 0 2px 0; }"
        ".bdg  { text-align:center; color:%4; font-size:11px; "
        "        margin:0 0 4px 0; }"
        "a     { color:%3; text-decoration:none; }"
        "hr    { border:none; border-top:1px solid %5; margin:7px 0; }"
        "h2    { color:%3; font-size:12px; font-weight:700; "
        "        letter-spacing:0.4px; margin:8px 0 3px 0; "
        "        text-transform:uppercase; }"
        "table { border-collapse:collapse; width:100%%; margin:0 0 4px 0; }"
        "th    { background:%6; color:%4; padding:5px 9px; "
        "        text-align:left; font-size:11px; font-weight:600; "
        "        letter-spacing:0.3px; border-bottom:1px solid %5; }"
        "td    { padding:5px 9px; vertical-align:top; "
        "        border-bottom:1px solid %5; font-size:12px; }"
        ".alt  { background:%7; }"
        ".nm   { font-weight:600; white-space:nowrap; }"
        ".lic  { color:%4; font-family:monospace; font-size:11px; "
        "        white-space:nowrap; }"
        ".pre  { background:%7; color:%4; font-family:monospace; "
        "        font-size:11px; padding:10px 14px; "
        "        border:1px solid %5; white-space:pre-wrap; "
        "        line-height:1.5; margin:0; }"
        ".mtt  { text-align:center; color:%4; font-style:italic; "
        "        font-size:12px; margin:6px 0 0 0; }";

    /* ── Dati librerie ── */
    struct LibRow { const char* nome; const char* scopo; const char* lic;
                    const char* lbl;  const char* url; };
    static const LibRow kLibs[] = {
        { "Qt6",              "Framework GUI cross-platform (Widgets, Network, Svg)",             "LGPL v3",    "github.com/qt/qtbase",             "https://github.com/qt/qtbase" },
        { "Ollama",           "Server locale per inferenza LLM via API REST",                     "MIT",        "github.com/ollama/ollama",         "https://github.com/ollama/ollama" },
        { "llama.cpp",        "Inferenza LLM ottimizzata CPU/GPU (GGUF) \xe2\x80\x94 G. Gerganov","MIT",        "github.com/ggerganov/llama.cpp",   "https://github.com/ggerganov/llama.cpp" },
        { "Piper TTS",        "Sintesi vocale locale offline ad alta qualit\xc3\xa0",              "MIT",        "github.com/rhasspy/piper",         "https://github.com/rhasspy/piper" },
        { "Whisper.cpp",      "Trascrizione audio STT locale (Georgi Gerganov)",                  "MIT",        "github.com/ggerganov/whisper.cpp", "https://github.com/ggerganov/whisper.cpp" },
        { "embeddinggemma",   "Modello embedding Google Gemma3 300M, 100+ lingue (RAG semantico)", "CC-BY-4.0",  "ollama.com/library/embeddinggemma", "https://ollama.com/library/embeddinggemma" },
        { "OpenBLAS",         "Algebra lineare ottimizzata per llama.cpp su CPU",                 "BSD 3-Clause","github.com/OpenMathLib/OpenBLAS",  "https://github.com/OpenMathLib/OpenBLAS" },
        { "md4c",             "Parser Markdown C (integrato in Qt6::Gui)",                        "MIT",        "github.com/mity/md4c",             "https://github.com/mity/md4c" },
        { "Poppler",          "Estrazione testo da PDF per indicizzazione RAG",                   "GPL v2/LGPL","gitlab.freedesktop.org/poppler",   "https://gitlab.freedesktop.org/poppler/poppler" },
        { "OpenCode",         "Agente coding AI con server HTTP+SSE integrato",                   "MIT",        "github.com/sst/opencode",          "https://github.com/sst/opencode" },
        { "Python 3",         "Esecuzione codice generato dall'AI in sandbox controllata",        "PSF",        "github.com/python/cpython",        "https://github.com/python/cpython" },
        { "ds4 (antirez)",    "Strutture dati compresse per LLM (bit packing) &mdash; ispirazione per il motore BLHM di Prismalux", "BSD", "github.com/antirez/ds4", "https://github.com/antirez/ds4" },
    };

    /* ── Dati plugin MCP ── */
    struct McpRow { const char* nome; const char* desc; const char* lbl; const char* url; };
    static const McpRow kMCPs[] = {
        { "\xf0\x9f\x8e\xa8 Blender",   "Controllo 3D bpy: crea/sposta/ruota oggetti, materiali, luci, render.",                                                           "blender.org",                    "https://www.blender.org" },
        { "\xf0\x9f\x96\xa5 Office",    "Automazione LibreOffice Writer/Calc/Impress via UNO, python-docx, openpyxl.",                                                     "libreoffice.org",                "https://www.libreoffice.org" },
        { "\xf0\x9f\x83\x8f Anki",      "Generazione mazzi flashcard da testi, appunti o spiegazioni AI.",                                                                 "github.com/ankitects/anki",      "https://github.com/ankitects/anki" },
        { "\xf0\x9f\x96\xa5 KiCAD",     "PCB design assistita: netlist, piazzamento componenti, script da linguaggio naturale.",                                            "gitlab.com/kicad",               "https://gitlab.com/kicad/code/kicad" },
        { "\xf0\x9f\xa4\x96 TinyMCP",   "Programmazione Arduino, ESP32, STM32: l'AI genera, compila e carica firmware.",                                                   "arduino.cc",                     "https://www.arduino.cc" },
        { "\xf0\x9f\x96\xa5 OpenCode",  "Agente coding con server HTTP+SSE (porta 8092): sessioni sviluppo in streaming.",                                                 "github.com/sst/opencode",        "https://github.com/sst/opencode" },
        { "\xf0\x9f\x93\xb9 OBS",       "Controllo OBS Studio: rec, streaming, cambio scene e sorgenti.",                                                                  "github.com/royshil/obs-mcp",     "https://github.com/royshil/obs-mcp" },
        { "\xf0\x9f\x92\xac WhatsApp",  "Invio/ricezione messaggi WhatsApp tramite bridge MCP, su autorizzazione.",                                                        "github.com/lharries/whatsapp-mcp","https://github.com/lharries/whatsapp-mcp" },
        { "\xf0\x9f\x93\xac Telegram",  "Bot Telegram: messaggi, canali, gruppi e notifiche automatiche.",                                                                 "github.com/chigwell/telegram-mcp","https://github.com/chigwell/telegram-mcp" },
        { "\xf0\x9f\x93\x9d WordPress", "Crea/modifica post, pagine e media WordPress senza aprire il browser.",                                                           "github.com/Automattic/wordpress-mcp","https://github.com/Automattic/wordpress-mcp" },
        { "\xf0\x9f\x8e\xae Godot",     "Crea scene, nodi e script GDScript; genera giochi 2D/3D da descrizioni testuali.",                                                "github.com/Coding-Solo/godot-mcp","https://github.com/Coding-Solo/godot-mcp" },
        { "\xf0\x9f\x8c\x90 GNS3",      "Topologie di rete (router, switch, firewall), configurazione e simulazione.",                                                     "github.com/ChistokhinSV/gns3-mcp","https://github.com/ChistokhinSV/gns3-mcp" },
        { "\xf0\x9f\x94\xac Cytoscape", "Reti biologiche/sociali: grafi, layout, statistiche, visualizzazioni.",                                                           "cytoscape.org",                  "https://cytoscape.org/" },
        { "\xf0\x9f\x94\xac RDKit",     "Chemioinformatica: SMILES, fingerprint, similarit\xc3\xa0 molecolare, propriet\xc3\xa0 chimiche.",                               "rdkit.org",                      "https://www.rdkit.org/" },
        { "\xf0\x9f\xa7\xaa Avogadro",  "Modellazione molecolare 3D: strutture, ottimizzazione geometrica. Formati SDF/PDB/XYZ.",                                          "avogadro.cc",                    "https://avogadro.cc/" },
        { "\xf0\x9f\x8c\xbf Bioconda",  "Tool bioinformatica (BLAST, BWA, GATK, Samtools) via canale conda. Pipeline genomiche.",                                          "bioconda.github.io",             "https://bioconda.github.io/" },
        { "\xf0\x9f\x97\xba Graphviz",  "Mappe concettuali e grafi DOT: diagrammi architettura, alberi decisionali, PNG/SVG inline.",                                     "graphviz.org",                   "https://graphviz.org/" },
    };

    /* ── Costruzione HTML (tema-adattiva) ── */
    auto* browser = new QTextBrowser;
    browser->setOpenExternalLinks(true);
    browser->setFrameShape(QFrame::NoFrame);
    browser->setObjectName("ringraziamentiView");
    {   /* D-69: filtro anche sul viewport — è lui che riceve i click */
        auto* egg = new AstroEggFilter(browser);
        browser->installEventFilter(egg);
        browser->viewport()->installEventFilter(egg);
    }

    auto updateHtml = [browser]() {
        QString bg, bg2, bg3, text, muted, accent, border;
        extractQssColors(bg, bg2, bg3, text, muted, accent, border);
        const QString css = QString(kCssFmt).arg(bg, text, accent, muted, border, bg3, bg2);

        QString h;
        h.reserve(32768);

        auto td  = [](const QString& cls, const QString& t) {
            return QString("<td class=\"%1\">%2</td>").arg(cls, t);
        };
        auto lnk = [](const QString& lbl, const QString& url) {
            return QString("<a href=\"%1\">%2</a>").arg(url, lbl);
        };

        int ri = 0;
        auto TR = [&](const QString& cells) -> QString {
            const QString cls = ((ri++ % 2) == 1) ? " class=\"alt\"" : "";
            return "<tr" + cls + ">" + cells + "</tr>\n";
        };

        h += "<html><head><style>" + css + "</style></head><body>\n";

    /* intestazione */
    h += "<h1>\xf0\x9f\x8d\xba&nbsp; Prismalux</h1>\n"
         "<p class=\"sub\">Piattaforma AI locale open-source &mdash; "
         "multi-agente &middot; RAG &middot; matematica &middot; grafici &middot; MCPs<br>"
         "&ldquo;Costruito per i mortali che aspirano alla saggezza.&rdquo;</p>\n"
         "<p class=\"ver\"><b>v2.9</b> &middot; Qt6/C++ &middot; Ollama &middot; "
         "llama.cpp &middot; Linux / Windows / macOS</p>\n"
         "<p class=\"lnks\">"
         "<a href=\"https://github.com/wildlux/Prismalux\">&#128279; github.com/wildlux/Prismalux</a>"
         " &nbsp;&middot;&nbsp; "
         "<a href=\"https://github.com/wildlux/Prismalux/releases\">&#128230; Release</a>"
         " &nbsp;&middot;&nbsp; "
         "<a href=\"https://github.com/wildlux/Prismalux/issues\">&#128027; Bug</a>"
         " &nbsp;&middot;&nbsp; "
         "<a href=\"https://github.com/wildlux/Prismalux/wiki\">&#128214; Wiki</a>"
         " &nbsp;&middot;&nbsp; "
         "<a href=\"https://github.com/wildlux/Prismalux/blob/master/LICENSE\">&#128220; MIT</a></p>\n"
         "<p class=\"bdg\">Pipeline 6 agenti &middot; Motore Byzantino &middot; "
         "110 algoritmi &middot; RAG locale &middot; Matematica offline &middot; "
         "TTS/STT &middot; Cron scheduler &middot; 17 MCP integrati</p>\n"
         "<hr>\n";

    /* donazione */
    h += "<div style=\"text-align:center;background:" + bg2 + ";"
         "border:1px solid " + accent + ";"
         "border-radius:8px;padding:10px 16px;margin:4px 0 8px 0\">\n"
         "<span style=\"font-size:15px\">&#9749;&nbsp; <b>Supporta Prismalux</b></span><br>"
         "<span style=\"font-size:12px;color:" + muted + "\">"
         "Prismalux &egrave; open-source e gratuito. Se ti &egrave; utile puoi offrire un caff&egrave;:</span><br><br>"
         "<a href=\"https://www.paypal.com/cgi-bin/webscr?cmd=_donations&business=wildlux%40gmail.com&item_name=Prismalux&currency_code=EUR&source=url\""
         " style=\"background:" + accent + ";color:#fff;padding:7px 22px;"
         "border-radius:6px;font-weight:700;font-size:13px;text-decoration:none\">"
         "&#128521;&nbsp; Dona con PayPal</a>"
         "</div><hr>\n";

    /* autore */
    h += "<h2>&#128100;&nbsp; Autore</h2>\n"
         "<table><tr>"
         "<th style=\"width:130px\">Nome</th>"
         "<th>Contributo</th>"
         "<th style=\"width:180px\">GitHub</th></tr>\n";
    ri = 0;
    h += TR(
        td("nm", "Paolo Lo Bello") +
        td("",   "Ideazione, progettazione e sviluppo integrale di Prismalux. "
                 "GUI C++/Qt6 &middot; Pipeline multi-agente &middot; Motore Byzantino "
                 "&middot; RAG locale &middot; Simulatore 110 algoritmi &middot; "
                 "Matematica locale &middot; Integrazione MCPs.") +
        td("",   lnk("github.com/wildlux", "https://github.com/wildlux"))
    );
    h += "</table><hr>\n";

    /* librerie */
    h += "<h2>&#128218;&nbsp; Librerie e backend</h2>\n"
         "<table><tr>"
         "<th style=\"width:130px\">Progetto</th>"
         "<th>Scopo</th>"
         "<th style=\"width:100px\">Licenza</th>"
         "<th style=\"width:210px\">Repository</th></tr>\n";
    ri = 0;
    for (const auto& r : kLibs) {
        h += TR(
            td("nm",  r.nome) +
            td("",    r.scopo) +
            td("lic", r.lic) +
            td("",    lnk(r.lbl, r.url))
        );
    }
    h += "</table><hr>\n";

    /* plugin MCP */
    h += "<h2>&#128300;&nbsp; Plugin MCP integrati</h2>\n"
         "<table><tr>"
         "<th style=\"width:140px\">Plugin</th>"
         "<th>Descrizione</th>"
         "<th style=\"width:200px\">Progetto</th></tr>\n";
    ri = 0;
    for (const auto& r : kMCPs) {
        h += TR(
            td("nm", r.nome) +
            td("",   r.desc) +
            td("",   lnk(r.lbl, r.url))
        );
    }
    h += "</table><hr>\n";

    /* ringraziamenti speciali */
    h += "<h2>&#11088;&nbsp; Ringraziamenti speciali</h2>\n"
         "<table><tr>"
         "<th style=\"width:180px\">Persona</th>"
         "<th>Contributo</th>"
         "<th style=\"width:210px\">Progetto</th></tr>\n";
    ri = 0;
    h += TR(
        td("nm", "Salvatore Sanfilippo") +
        td("",   "Creatore di Redis e autore di <b>ds4</b> &mdash; strutture dati compresse "
                 "per LLM con bit packing. Il suo lavoro ha ispirato il motore BLHM di Prismalux.") +
        td("",   lnk("github.com/antirez/ds4", "https://github.com/antirez/ds4"))
    );
    h += "</table><hr>\n";

    /* licenza */
    h += "<h2>&#128220;&nbsp; Licenza &mdash; MIT</h2>\n"
         "<pre class=\"pre\">"
         "Copyright (c) 2024-2026 Paolo Lo Bello\n\n"
         "Permission is hereby granted, free of charge, to any person obtaining a copy\n"
         "of this software and associated documentation files (the &quot;Software&quot;), to deal\n"
         "in the Software without restriction, including without limitation the rights\n"
         "to use, copy, modify, merge, publish, distribute, sublicense, and/or sell\n"
         "copies of the Software, and to permit persons to whom the Software is\n"
         "furnished to do so, subject to the following conditions:\n\n"
         "The above copyright notice and this permission notice shall be included in all\n"
         "copies or substantial portions of the Software.\n\n"
         "THE SOFTWARE IS PROVIDED &quot;AS IS&quot;, WITHOUT WARRANTY OF ANY KIND, EXPRESS OR\n"
         "IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,\n"
         "FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE\n"
         "AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER\n"
         "LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM,\n"
         "OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN THE\n"
         "SOFTWARE.</pre>\n";

    /* motto */
    h += "<p class=\"mtt\">&#127866;&nbsp; &ldquo;La birra &egrave; conoscenza divina "
         "&mdash; ogni sorso un dato in pi&ugrave;.&rdquo;</p>\n"
         "</body></html>\n";

        browser->setHtml(h);
    };

    updateHtml();

    connect(ThemeManager::instance(), &ThemeManager::changed,
            browser, [updateHtml](const QString&) { updateHtml(); });

    return browser;
}

/* ══════════════════════════════════════════════════════════════
   buildMcpGroupTab — tab esterna "MCP": Configurazione + Gestione
   in 2 sotto-tab (prima erano 2 tab esterne separate). buildMcpTab()
   legge solo ~/.claude/settings.json (leggero, eager); McpManagerPage
   scansiona MCPs/ su disco (~50 plugin, resta lazy).
   ══════════════════════════════════════════════════════════════ */
QWidget* ImpostazioniPage::buildMcpGroupTab(QTabWidget* parentTabs)
{
    auto* t = new QTabWidget;
    t->setObjectName("settingsInnerTabs");
    t->setDocumentMode(true);
    t->setUsesScrollButtons(true);
    m_tabMcp = t;

    auto* inner = new LazyTabLoader(t, this);
    inner->addEager("\xe2\x9a\x99\xef\xb8\x8f  Configurazione", buildMcpTab());
    inner->addLazy("\xf0\x9f\x97\x82\xef\xb8\x8f  Gestione", [parentTabs]{
        return new McpManagerPage(parentTabs);
    });
    return t;
}

/* ══════════════════════════════════════════════════════════════
   buildMcpTab — configurazione Model Context Protocol
   ══════════════════════════════════════════════════════════════ */
QWidget* ImpostazioniPage::buildMcpTab()
{
    const QString claudeCfgPath =
        QStandardPaths::writableLocation(QStandardPaths::HomeLocation)
        + "/.claude/settings.json";

    /* helpers read/write settings.json */
    auto readCfg = [claudeCfgPath]() -> QJsonObject {
        QFile f(claudeCfgPath);
        if (!f.open(QIODevice::ReadOnly)) return {};
        return QJsonDocument::fromJson(f.readAll()).object();
    };
    auto writeCfg = [claudeCfgPath](const QJsonObject& obj) {
        QFileInfo fi(claudeCfgPath);
        QDir().mkpath(fi.absolutePath());
        QFile f(claudeCfgPath);
        if (f.open(QIODevice::WriteOnly | QIODevice::Truncate))
            f.write(QJsonDocument(obj).toJson());
    };

    auto* page = new QWidget;
    auto* vlay = new QVBoxLayout(page);
    vlay->setContentsMargins(16, 16, 16, 16);
    vlay->setSpacing(14);

    /* ── Layout a 2 colonne ─────────────────────────────────────── */
    auto* leftL  = new QVBoxLayout;  leftL->setSpacing(14);
    auto* rightL = new QVBoxLayout;  rightL->setSpacing(14);

    /* ── 1. MCP configurati in Claude Code ─────────────────────── */
    {
        auto* box  = new QGroupBox(
            tr("\xf0\x9f\x93\x8b  MCP configurati in Claude Code  (~/.claude/settings.json)"));
        auto* blay = new QVBoxLayout(box);

        auto* table = new QTableWidget(0, 3);
        table->setHorizontalHeaderLabels({tr("Nome"), tr("Tipo"), tr("Comando / Args")});
        table->horizontalHeader()->setSectionResizeMode(0, QHeaderView::ResizeToContents);
        table->horizontalHeader()->setSectionResizeMode(1, QHeaderView::ResizeToContents);
        table->horizontalHeader()->setSectionResizeMode(2, QHeaderView::Stretch);
        table->setSelectionBehavior(QAbstractItemView::SelectRows);
        table->setEditTriggers(QAbstractItemView::NoEditTriggers);
        table->setAlternatingRowColors(true);
        table->setMaximumHeight(180);

        auto refreshTable = [table, readCfg]() {
            table->setRowCount(0);
            const QJsonObject mcp =
                readCfg().value("mcpServers").toObject();
            for (const QString& name : mcp.keys()) {
                const QJsonObject srv = mcp.value(name).toObject();
                const int r = table->rowCount();
                table->insertRow(r);
                table->setItem(r, 0, new QTableWidgetItem(name));
                table->setItem(r, 1, new QTableWidgetItem(
                    srv.value("type").toString("stdio")));
                QString cmd = srv.value("command").toString();
                for (const auto& a : srv.value("args").toArray())
                    cmd += " " + a.toString();
                table->setItem(r, 2, new QTableWidgetItem(cmd));
            }
        };
        refreshTable();

        auto* btnRow = new QWidget;
        auto* btnLay = new QHBoxLayout(btnRow);
        btnLay->setContentsMargins(0, 0, 0, 0);

        auto* btnRimuovi  = new QPushButton(tr("\xe2\x9d\x8c  Rimuovi selezionato"));
        auto* btnApri     = new QPushButton(tr("\xf0\x9f\x93\x82  Apri settings.json"));
        auto* btnRefresh  = new QPushButton(tr("\xf0\x9f\x94\x84  Aggiorna"));
        btnRimuovi->setObjectName("actionBtn");
        btnApri->setObjectName("actionBtn");
        btnRefresh->setObjectName("actionBtn");
        btnLay->addWidget(btnRimuovi);
        btnLay->addWidget(btnApri);
        btnLay->addStretch();
        btnLay->addWidget(btnRefresh);

        connect(btnRefresh, &QPushButton::clicked, page,
                [refreshTable]{ refreshTable(); });
        connect(btnApri, &QPushButton::clicked, page, [claudeCfgPath]{
            QProcess::startDetached("xdg-open", {claudeCfgPath});
        });
        connect(btnRimuovi, &QPushButton::clicked, page,
                [table, readCfg, writeCfg, refreshTable]{
            const int r = table->currentRow();
            if (r < 0) return;
            const QString nome = table->item(r, 0)->text();
            QJsonObject cfg = readCfg();
            QJsonObject mcp = cfg.value("mcpServers").toObject();
            mcp.remove(nome);
            cfg["mcpServers"] = mcp;
            writeCfg(cfg);
            refreshTable();
        });

        /* ── Riga 2: Esporta / Importa lista MCP ── */
        auto* btnRow2 = new QWidget;
        auto* btnLay2 = new QHBoxLayout(btnRow2);
        btnLay2->setContentsMargins(0, 0, 0, 0);
        btnLay2->setSpacing(6);

        auto* btnExport = new QPushButton(
            tr("\xf0\x9f\x92\xbe  Esporta lista MCP..."));
        auto* btnImport = new QPushButton(
            tr("\xf0\x9f\x93\x82  Importa lista MCP..."));
        auto* fbkIo = new QLabel;
        fbkIo->setStyleSheet("font-size: 11px;");
        btnExport->setObjectName("actionBtn");
        btnImport->setObjectName("actionBtn");
        btnLay2->addWidget(btnExport);
        btnLay2->addWidget(btnImport);
        btnLay2->addWidget(fbkIo, 1);

        connect(btnExport, &QPushButton::clicked, page,
                [readCfg, fbkIo, page]{
            const QString path = QFileDialog::getSaveFileName(
                page,
                "Esporta lista MCP",
                QDir::homePath() + "/mcp_list.json",
                "JSON (*.json)");
            if (path.isEmpty()) return;
            QJsonObject root;
            root["mcpServers"] = readCfg().value("mcpServers").toObject();
            QFile f(path);
            if (f.open(QIODevice::WriteOnly | QIODevice::Truncate)) {
                f.write(QJsonDocument(root).toJson());
                fbkIo->setStyleSheet("color: green; font-size:11px;");
                fbkIo->setText(tr("\xe2\x9c\x85  Salvato in ") +
                    QFileInfo(path).fileName());
            } else {
                fbkIo->setStyleSheet("color: red; font-size:11px;");
                fbkIo->setText(tr("\xe2\x9d\x8c  Errore scrittura file"));
                LogBus::post("\xe2\x9d\x8c Impostazioni: Errore scrittura file MCP JSON");
            }
        });

        connect(btnImport, &QPushButton::clicked, page,
                [readCfg, writeCfg, refreshTable, fbkIo, page]{
            const QString path = QFileDialog::getOpenFileName(
                page,
                "Importa lista MCP",
                QDir::homePath(),
                "JSON (*.json)");
            if (path.isEmpty()) return;
            QFile f(path);
            if (!f.open(QIODevice::ReadOnly)) {
                fbkIo->setStyleSheet("color: red; font-size:11px;");
                fbkIo->setText(tr("\xe2\x9d\x8c  File non leggibile"));
                LogBus::post("\xe2\x9d\x8c Impostazioni: File MCP non leggibile");
                return;
            }
            const QJsonObject loaded =
                QJsonDocument::fromJson(f.readAll()).object();
            /* accetta sia {"mcpServers":{…}} che direttamente {nome:{…}} */
            const QJsonObject imported = loaded.contains("mcpServers")
                ? loaded.value("mcpServers").toObject()
                : loaded;
            if (imported.isEmpty()) {
                fbkIo->setStyleSheet("color: red; font-size:11px;");
                fbkIo->setText(tr("\xe2\x9d\x8c  Nessun MCP trovato nel file"));
                LogBus::post("\xe2\x9d\x8c Impostazioni: Nessun MCP trovato nel file importato");
                return;
            }
            QJsonObject cfg = readCfg();
            QJsonObject mcp = cfg.value("mcpServers").toObject();
            for (const QString& k : imported.keys())
                mcp[k] = imported.value(k);
            cfg["mcpServers"] = mcp;
            writeCfg(cfg);
            refreshTable();
            fbkIo->setStyleSheet("color: green; font-size:11px;");
            fbkIo->setText(QString("\xe2\x9c\x85  %1 MCP importati")
                .arg(imported.size()));
        });

        blay->addWidget(table);
        blay->addWidget(btnRow);
        blay->addWidget(btnRow2);
        leftL->addWidget(box);
    }

    /* ── 2. MCP Locali Prismalux ────────────────────────────────── */
    {
        auto* box  = new QGroupBox(
            tr("\xf0\x9f\x8f\xa0  MCP Locali Prismalux  (pronti da usare)"));
        auto* blay = new QVBoxLayout(box);

        struct McpLocal {
            QString name, desc, scriptRel, cfgType, cfgCmd;
            QStringList cfgArgs;
            int port;   /* 0 = stdio puro, >0 = HTTP */
        };
        const QString root = P::root();
        const QList<McpLocal> cards = {
            { "Blender Bridge",
              "Controllo Blender tramite HTTP porta 6789. Avvia prima l'addon in Blender.",
              "MCPs/blender_addon/prismalux_bridge.py",
              "sse", "python3",
              { root + "/MCPs/blender_addon/prismalux_bridge.py" },
              6789 },
            { "Office Bridge",
              "Controllo LibreOffice/MS Office via UNO bridge (stdio).",
              "MCPs/office_bridge/prismalux_office_bridge.py",
              "stdio", "python3",
              { root + "/MCPs/office_bridge/prismalux_office_bridge.py" },
              0 },
            { "OpenCode MCP",
              "Bridge Claude Code \xe2\x86\x94 Prismalux stdio JSON-RPC. "
              "Gi\xc3\xa0 integrato in APP Controller.",
              "MCPs/opencode_mcp/server.py",
              "stdio", "python3",
              { root + "/MCPs/opencode_mcp/server.py" },
              0 },
        };

        for (const McpLocal& c : cards) {
            auto* card = new QFrame;
            card->setFrameShape(QFrame::StyledPanel);
            auto* cLay = new QHBoxLayout(card);

            auto* info  = new QVBoxLayout;
            auto* title = new QLabel(tr("<b>") + c.name + "</b>");
            auto* desc  = new QLabel(c.desc);
            desc->setWordWrap(true);
            desc->setStyleSheet("color: gray; font-size: 11px;");
            info->addWidget(title);
            info->addWidget(desc);
            cLay->addLayout(info, 1);

            auto* btns = new QVBoxLayout;
            btns->setSpacing(4);

            auto* btnAgg = new QPushButton(
                tr("\xe2\x9e\x95  Aggiungi a Claude"));
            btnAgg->setObjectName("actionBtn");
            btnAgg->setFixedWidth(dpiScale(165));
            btns->addWidget(btnAgg);

            /* QProcess per MCP HTTP (avviabile da Prismalux) */
            QProcess* proc = nullptr;
            if (c.port != 0) {
                proc = new QProcess(card);
                auto* btnStart = new QPushButton(
                    tr("\xe2\x96\xb6  Avvia server"));
                btnStart->setObjectName("actionBtn");
                btnStart->setFixedWidth(dpiScale(165));
                btns->addWidget(btnStart);

                const QString scriptPath = root + "/" + c.scriptRel;
                connect(btnStart, &QPushButton::clicked, card,
                        [proc, btnStart, scriptPath]{
                    if (proc->state() == QProcess::NotRunning) {
                        proc->start(P::findPython(), {scriptPath});
                        btnStart->setText(tr("\xe2\x96\xa0  Ferma server"));
                    } else {
                        proc->terminate();
                        btnStart->setText(tr("\xe2\x96\xb6  Avvia server"));
                    }
                });
                connect(proc, &QProcess::stateChanged, card,
                        [btnStart](QProcess::ProcessState st){
                    if (st == QProcess::NotRunning)
                        btnStart->setText(tr("\xe2\x96\xb6  Avvia server"));
                });
                connect(proc, &QProcess::errorOccurred, card,
                    [btnStart](QProcess::ProcessError err) {
                        if (err == QProcess::FailedToStart)
                            btnStart->setText(tr("\xe2\x9d\x8c  Python non trovato"));
                    });
            }

            McpLocal cap = c;
            connect(btnAgg, &QPushButton::clicked, page,
                    [cap, readCfg, writeCfg]{
                QJsonObject cfg = readCfg();
                QJsonObject mcp = cfg.value("mcpServers").toObject();
                QJsonObject srv;
                srv["type"]    = cap.cfgType;
                srv["command"] = cap.cfgCmd;
                QJsonArray arr;
                for (const QString& a : cap.cfgArgs) arr.append(a);
                srv["args"] = arr;
                mcp[cap.name] = srv;
                cfg["mcpServers"] = mcp;
                writeCfg(cfg);
            });

            cLay->addLayout(btns);
            blay->addWidget(card);
        }
        rightL->addWidget(box);
    }

    /* ── 2b. Ollama in locale ──────────────────────────────────── */
    {
        auto* box  = new QGroupBox(
            tr("\xf0\x9f\xa6\x99  Ollama in locale \xe2\x80\x94 usa i modelli locali in Claude Code"));
        auto* blay = new QVBoxLayout(box);

        auto* desc = new QLabel(
            "Aggiunge un server MCP che espone i modelli Ollama locali come strumenti "
            "disponibili in Claude Code. Richiede <code>npm</code> (Node.js).");
        desc->setWordWrap(true);
        desc->setTextFormat(Qt::RichText);
        blay->addWidget(desc);

        auto* cmdLbl = new QLabel(
            "<b>Comando MCP:</b><br>"
            "<code>npx -y ollama-mcp-server</code>"
            "<br><span style='color:gray;font-size:11px;'>"
            "Host Ollama: http://localhost:11434 (predefinito)</span>");
        cmdLbl->setTextFormat(Qt::RichText);
        cmdLbl->setWordWrap(true);
        blay->addWidget(cmdLbl);

        auto* btnRow = new QWidget;
        auto* btnL   = new QHBoxLayout(btnRow);
        btnL->setContentsMargins(0,0,0,0); btnL->setSpacing(8);

        auto* btnAggOllama = new QPushButton(tr("\xe2\x9e\x95  Aggiungi a Claude"));
        btnAggOllama->setObjectName("actionBtn");
        btnAggOllama->setToolTip(tr("Aggiunge questo endpoint Ollama come MCP in Claude Desktop settings.json"));
        auto* btnCopia = new QPushButton(tr("\xf0\x9f\x93\x8b  Copia comando"));
        btnCopia->setObjectName("actionBtn");
        btnCopia->setToolTip(tr("Copia il comando curl per testare Ollama negli appunti"));
        auto* fbkOllama = new QLabel;

        btnL->addWidget(btnAggOllama);
        btnL->addWidget(btnCopia);
        btnL->addWidget(fbkOllama, 1);
        blay->addWidget(btnRow);

        connect(btnAggOllama, &QPushButton::clicked, page,
                [readCfg, writeCfg, fbkOllama]{
            QJsonObject cfg = readCfg();
            QJsonObject mcp = cfg.value("mcpServers").toObject();
            QJsonObject srv;
            srv["type"]    = QString("stdio");
            srv["command"] = QString("npx");
            QJsonArray args;
            args.append("-y");
            args.append("ollama-mcp-server");
            srv["args"] = args;
            mcp["ollama-locale"] = srv;
            cfg["mcpServers"] = mcp;
            writeCfg(cfg);
            fbkOllama->setStyleSheet("color: green;");
            fbkOllama->setText(tr("\xe2\x9c\x85  Aggiunto a settings.json"));
        });
        connect(btnCopia, &QPushButton::clicked, page, []{
            QGuiApplication::clipboard()->setText(tr("npx -y ollama-mcp-server"));
        });

        rightL->addWidget(box);
    }

    /* ── 3. Aggiungi MCP personalizzato ─────────────────────────── */
    {
        auto* box  = new QGroupBox(
            tr("\xe2\x9e\x95  Aggiungi MCP personalizzato a Claude Code"));
        auto* form = new QFormLayout(box);
        form->setSpacing(8);

        auto* editNome = new QLineEdit;
        editNome->setPlaceholderText(tr("es. my-mcp"));
        auto* cmbTipo = new QComboBox;
        cmbTipo->addItems({"stdio", "sse", "http"});
        auto* editCmd = new QLineEdit;
        editCmd->setPlaceholderText(tr("es. python3   oppure   npx"));
        auto* editArgs = new QLineEdit;
        editArgs->setPlaceholderText(
            tr("es. /percorso/server.py   (separati da spazio)"));
        auto* editEnv = new QLineEdit;
        editEnv->setPlaceholderText(
            tr("es. API_KEY=abc TOKEN=xyz   (opzionale)"));

        form->addRow("Nome:", editNome);
        form->addRow("Tipo:", cmbTipo);
        form->addRow("Comando:", editCmd);
        form->addRow("Args:", editArgs);
        form->addRow("Env:", editEnv);

        auto* btnSalva = new QPushButton(
            tr("\xf0\x9f\x92\xbe  Salva in settings.json"));
        btnSalva->setObjectName("actionBtn");
        auto* fbk = new QLabel;

        form->addRow("", btnSalva);
        form->addRow("", fbk);

        connect(btnSalva, &QPushButton::clicked, page,
                [editNome, cmbTipo, editCmd, editArgs, editEnv,
                 fbk, readCfg, writeCfg]{
            const QString nome = editNome->text().trimmed();
            const QString cmd  = editCmd->text().trimmed();
            if (nome.isEmpty() || cmd.isEmpty()) {
                fbk->setStyleSheet("color: red;");
                fbk->setText(tr("Nome e Comando sono obbligatori."));
                return;
            }
            QJsonObject cfg = readCfg();
            QJsonObject mcp = cfg.value("mcpServers").toObject();
            QJsonObject srv;
            srv["type"]    = cmbTipo->currentText();
            srv["command"] = cmd;
            QJsonArray arr;
            for (const QString& a :
                 editArgs->text().split(' ', Qt::SkipEmptyParts))
                arr.append(a);
            srv["args"] = arr;
            QJsonObject env;
            for (const QString& kv :
                 editEnv->text().split(' ', Qt::SkipEmptyParts)) {
                const int idx = kv.indexOf('=');
                if (idx > 0) env[kv.left(idx)] = kv.mid(idx + 1);
            }
            if (!env.isEmpty()) srv["env"] = env;
            mcp[nome] = srv;
            cfg["mcpServers"] = mcp;
            writeCfg(cfg);
            fbk->setStyleSheet("color: green;");
            fbk->setText(tr("\xe2\x9c\x85  \"") + nome +
                "\" aggiunto a settings.json");
            editNome->clear();
            editCmd->clear();
            editArgs->clear();
            editEnv->clear();
        });

        leftL->addWidget(box);
    }

    /* ── 4. MCP Popolari (hub ufficiale) ────────────────────────── */
    {
        auto* box  = new QGroupBox(
            tr("\xf0\x9f\x8c\x90  MCP Popolari  (copia il comando e installalo nel terminale)"));
        auto* blay = new QVBoxLayout(box);

        auto* intro = new QLabel(
            tr("Premi \xf0\x9f\x93\x8b per copiare il comando npx/uvx negli appunti. "
            "Poi aggiungilo con la sezione \"Aggiungi MCP personalizzato\" sopra."));
        intro->setWordWrap(true);
        intro->setStyleSheet("color: gray; font-size: 11px;");
        blay->addWidget(intro);

        struct PopMcp { QString name, desc, installCmd; };
        const QList<PopMcp> pop = {
            { "filesystem",
              "Accesso al filesystem locale",
              "npx @modelcontextprotocol/server-filesystem" },
            { "brave-search",
              "Ricerca web tramite Brave Search API",
              "npx @modelcontextprotocol/server-brave-search" },
            { "github",
              "GitHub API (repo, PR, issue, code search)",
              "npx @modelcontextprotocol/server-github" },
            { "sqlite",
              "Query e gestione database SQLite",
              "npx @modelcontextprotocol/server-sqlite" },
            { "puppeteer",
              "Browser automation headless (Chromium)",
              "npx @modelcontextprotocol/server-puppeteer" },
            { "memory",
              "Memoria KV persistente per Claude",
              "npx @modelcontextprotocol/server-memory" },
            { "fetch",
              "Fetch URL / HTTP generico da Claude",
              "npx @modelcontextprotocol/server-fetch" },
            { "sequential-thinking",
              "Ragionamento strutturato a step",
              "npx @modelcontextprotocol/server-sequentialthinking" },
            { "postgres",
              "Query PostgreSQL dirette da Claude",
              "npx @modelcontextprotocol/server-postgres" },
            { "slack",
              "Lettura e invio messaggi Slack",
              "npx @modelcontextprotocol/server-slack" },
            { "godot-mcp",
              "Controllo motore Godot: scene, script GDScript, anteprima gioco",
              "npx -y godot-mcp" },
            { "ollama-mcp",
              "Usa modelli Ollama locali come strumenti in Claude Code",
              "npx -y ollama-mcp-server" },
        };

        auto* grid = new QGridLayout;
        grid->setHorizontalSpacing(8);
        grid->setVerticalSpacing(4);
        int row = 0;
        for (const PopMcp& p : pop) {
            auto* lbl = new QLabel(
                "<b>" + p.name + "</b>  \xe2\x80\x94  " + p.desc);
            lbl->setTextFormat(Qt::RichText);
            auto* btnCopy = new QPushButton("\xf0\x9f\x93\x8b");
            btnCopy->setToolTip(tr("Copia: ") + p.installCmd);
            btnCopy->setFixedSize(28, 28);
            PopMcp cap = p;
            connect(btnCopy, &QPushButton::clicked, page, [cap]{
                QGuiApplication::clipboard()->setText(cap.installCmd);
            });
            grid->addWidget(lbl,     row, 0);
            grid->addWidget(btnCopy, row, 1);
            ++row;
        }
        blay->addLayout(grid);
        rightL->addWidget(box);
    }

    /* ── Assembla 2 colonne ──────────────────────────────────────── */
    leftL->addStretch();
    rightL->addStretch();
    auto* colsLay = new QHBoxLayout;
    colsLay->setSpacing(12);
    colsLay->addLayout(leftL,  52);
    colsLay->addLayout(rightL, 48);
    vlay->addLayout(colsLay);

    /* ── Dipendenze Python richieste dagli MCP ──────────────────── */
    {
        using Dep = DepCheckPanel::Dep;
        const QList<Dep> mcpDeps = {
            { "requests",       "requests",        "requests",        "", "HTTP client (usato da quasi tutti gli MCP)" },
            { "yt-dlp",         "yt_dlp",          "yt-dlp",          "", "Stream live YouTube/Twitch (streamlink_mcp)" },
            { "faster-whisper", "faster_whisper",  "faster-whisper",  "", "STT 2-4\xc3\x97 pi\xc3\xb9 veloce (audio MCP)" },
            { "webrtcvad",      "webrtcvad",        "webrtcvad",       "", "Rilevamento silenzio/voce VAD (audio MCP)" },
            { "simple-diarizer","simple_diarizer",  "simple-diarizer", "", "Diarizzazione speaker offline" },
            { "Pillow",         "PIL",              "Pillow",          "", "Elaborazione immagini (video_caption, SD)" },
            { "graphviz",       "graphviz",         "graphviz",        "", "Rendering grafi DOT (graphviz MCP)" },
            { "openpyxl",       "openpyxl",         "openpyxl",        "", "Lettura/scrittura Excel (.xlsx)" },
            { "python-docx",    "docx",             "python-docx",     "", "Lettura/scrittura Word (.docx)" },
            { "ffmpeg",         "",                 "",                "ffmpeg", "Mux/demux audio-video (richiesto da streamlink_mcp)" },
        };
        auto* depPanel = new DepCheckPanel(mcpDeps, page);
        vlay->addWidget(depPanel);
        QTimer::singleShot(400, depPanel, &DepCheckPanel::runAllChecks);
    }

    /* ── Pulsante ripristina messaggio di benvenuto ─────────────── */
    auto* onbSep = new QFrame(page);
    onbSep->setFrameShape(QFrame::HLine);
    onbSep->setFrameShadow(QFrame::Sunken);
    vlay->addWidget(onbSep);

    auto* onbRow = new QHBoxLayout;
    auto* onbBtn = new QPushButton(
        tr("\xf0\x9f\x8d\xba  Mostra di nuovo il messaggio di benvenuto"), page);
    onbBtn->setToolTip(
        tr("Reimposta il flag di primo avvio: alla prossima apertura\n"
        "di Prismalux apparir\xc3\xa0 di nuovo la schermata di benvenuto."));
    connect(onbBtn, &QPushButton::clicked, onbBtn,
            [onbBtn](){ settingsDoResetOnboarding(onbBtn); });
    onbRow->addWidget(onbBtn);
    onbRow->addStretch();
    vlay->addLayout(onbRow);

    auto* sc = new QScrollArea;
    sc->setWidgetResizable(true);
    sc->setFrameShape(QFrame::NoFrame);
    sc->setWidget(page);
    return sc;
}

/* ══════════════════════════════════════════════════════════════
   buildAiMemoryTab — storia preferenze AIMemory (gitLog + revertFile)
   ══════════════════════════════════════════════════════════════ */
QWidget* ImpostazioniPage::buildAiMemoryTab()
{
    auto* w    = new QWidget;
    auto* vbox = new QVBoxLayout(w);
    vbox->setContentsMargins(12, 12, 12, 12);
    vbox->setSpacing(10);

    auto* titleLbl = new QLabel(
        tr("<b>" "\xf0\x9f\xa7\xa0" " Memoria AI — storia versionata</b>"), w);
    titleLbl->setTextFormat(Qt::RichText);
    vbox->addWidget(titleLbl);

    auto* infoLbl = new QLabel(
        tr("<small>Ogni preferenza appresa e feedback " "\xf0\x9f\x91\x8d\xf0\x9f\x91\x8e"
        " viene salvato come commit Git in <code>~/.ai-memory/</code>.<br>"
        "Seleziona un commit e usa <b>Ripristina</b> per tornare alle preferenze di quella versione.</small>"),
        w);
    infoLbl->setTextFormat(Qt::RichText);
    infoLbl->setWordWrap(true);
    vbox->addWidget(infoLbl);

    auto* mem = new AIMemory(w);
    mem->initialize();

    auto* logList = new QListWidget(w);
    logList->setAlternatingRowColors(true);
    logList->setFont(QFont("monospace", 9));
    vbox->addWidget(logList, 1);

    settingsRefreshAiMemoryLog(mem, logList);

    /* ── pulsanti ── */
    auto* btnRow = new QHBoxLayout;

    auto* refreshBtn = new QPushButton(tr("\xf0\x9f\x94\x84  Aggiorna"), w);
    connect(refreshBtn, &QPushButton::clicked, w,
            [mem, logList](){ settingsRefreshAiMemoryLog(mem, logList); });
    btnRow->addWidget(refreshBtn);

    auto* revertBtn = new QPushButton(tr("\xe2\x86\xa9  Ripristina preferences.yaml"), w);
    revertBtn->setToolTip(
        "Ripristina il file profile/preferences.yaml al commit selezionato.\n"
        "Gli altri file (interactions/, context/) non vengono toccati.");
    connect(revertBtn, &QPushButton::clicked, w,
            [mem, logList, w](){ settingsDoRevertAiMemory(mem, logList, w); });
    btnRow->addWidget(revertBtn);

    auto* openBtn = new QPushButton(tr("\xf0\x9f\x93\x82  Apri cartella"), w);
    openBtn->setToolTip(tr("Apre ~/.ai-memory/ nel gestore file di sistema"));
    connect(openBtn, &QPushButton::clicked, w, []() {
        QDesktopServices::openUrl(
            QUrl::fromLocalFile(QDir::homePath() + "/.ai-memory"));
    });
    btnRow->addWidget(openBtn);

    vbox->addLayout(btnRow);
    return w;
}

/* ══════════════════════════════════════════════════════════════
   buildProfiliGroupTab — tab esterna "Profili Modello": Profili
   (con preset "📋 Predefiniti") + Le tue preferenze, in 2 sotto-tab
   (prima erano 2 tab esterne separate). ModelProfilesTab è il
   concetto primario (ha i preset) → eager; Feedback resta lazy.
   ══════════════════════════════════════════════════════════════ */
QWidget* ImpostazioniPage::buildProfiliGroupTab(QTabWidget* parentTabs)
{
    auto* t = new QTabWidget;
    t->setObjectName("settingsInnerTabs");
    t->setDocumentMode(true);
    t->setUsesScrollButtons(true);
    m_tabProfili = t;

    auto* inner = new LazyTabLoader(t, this);
    inner->addEager("\xf0\x9f\x93\x8b  Profili", new ModelProfilesTab(parentTabs));
    inner->addLazy("\xf0\x9f\x93\x8a  Le tue preferenze", [this]{ return buildFeedbackTab(); });
    return t;
}

/* ══════════════════════════════════════════════════════════════
   buildFeedbackTab — analytics 👍/👎 + export DPO dataset
   Legge ~/.prismalux/feedback.jsonl e mostra statistiche aggregate
   per modello. Pulsante export genera dataset ShareGPT/Alpaca.
   ══════════════════════════════════════════════════════════════ */
QWidget* ImpostazioniPage::buildFeedbackTab()
{
    auto* w    = new QWidget;
    auto* vbox = new QVBoxLayout(w);
    vbox->setContentsMargins(16, 16, 16, 16);
    vbox->setSpacing(10);

    /* Intestazione */
    auto* titleLbl = new QLabel(
        "\xf0\x9f\x93\x8a  <b>Analytics Feedback 👍 / 👎</b><br>"
        "<small>Il sistema raccoglie il feedback per ogni risposta AI. "
        "I dati restano solo sul tuo dispositivo.</small>", w);
    titleLbl->setTextFormat(Qt::RichText);
    titleLbl->setWordWrap(true);
    vbox->addWidget(titleLbl);

    /* Tabella statistiche */
    auto* table = new QTableWidget(0, 4, w);
    table->setHorizontalHeaderLabels({
        tr("Modello"), tr("\xf0\x9f\x91\x8d Positivi"), tr("\xf0\x9f\x91\x8e Negativi"), tr("Totale")
    });
    table->horizontalHeader()->setStretchLastSection(true);
    table->setEditTriggers(QAbstractItemView::NoEditTriggers);
    table->setSelectionBehavior(QAbstractItemView::SelectRows);
    table->setObjectName("outputView");
    vbox->addWidget(table, 1);

    /* Riga bassa: pulsanti */
    auto* btnRow = new QHBoxLayout;
    auto* btnRefresh = new QPushButton(tr("\xf0\x9f\x94\x84  Aggiorna"), w);
    auto* btnExport  = new QPushButton(tr("\xf0\x9f\x93\xa4  Esporta dataset DPO (ShareGPT)"), w);
    auto* btnClear   = new QPushButton(tr("\xf0\x9f\x97\x91  Cancella dati"), w);
    btnRefresh->setObjectName("actionBtn");
    btnRefresh->setToolTip(tr("Ricarica la tabella dei feedback da chat_feedback.jsonl"));
    btnExport->setObjectName("actionBtn");
    btnExport->setToolTip(tr("Esporta le conversazioni con feedback nel formato ShareGPT per il fine-tuning DPO"));
    btnClear->setObjectName("actionBtn");
    btnClear->setToolTip(tr("Cancella tutti i dati di feedback (chat_feedback.jsonl)"));
    auto* statLbl    = new QLabel(w);
    statLbl->setWordWrap(true);
    btnRow->addWidget(btnRefresh);
    btnRow->addWidget(btnExport);
    btnRow->addStretch();
    btnRow->addWidget(btnClear);
    vbox->addWidget(statLbl);
    vbox->addLayout(btnRow);

    /* ── Importa da AI esterne (ChatGPT/Claude/generico) ── */
    auto* sep = new QFrame(w);
    sep->setFrameShape(QFrame::HLine);
    sep->setObjectName("sidebarSep");
    vbox->addWidget(sep);

    auto* importLbl = new QLabel(
        tr("\xf0\x9f\x93\xa5  <b>Importa da AI esterne</b><br>"
        "<small>Carica un file JSON esportato da ChatGPT o Claude (export dati "
        "account), oppure un JSON generico {\"role\",\"content\"} (DeepSeek, Qwen "
        "e altri, se esportato con strumenti terzi \xe2\x80\x94 non hanno un export "
        "ufficiale proprio). Le conversazioni finiscono nello storico chat locale.</small>"), w);
    importLbl->setTextFormat(Qt::RichText);
    importLbl->setWordWrap(true);
    vbox->addWidget(importLbl);

    auto* importRow = new QHBoxLayout;
    auto* btnImport = new QPushButton(tr("\xf0\x9f\x93\xa5  Importa da AI esterne"), w);
    btnImport->setObjectName("actionBtn");
    btnImport->setToolTip(tr("Importa un file JSON esportato da OpenAI/Anthropic/altri servizi AI"));
    auto* importStatLbl = new QLabel(w);
    importStatLbl->setWordWrap(true);
    importRow->addWidget(btnImport);
    importRow->addWidget(importStatLbl, 1);
    vbox->addLayout(importRow);

    connect(btnImport, &QPushButton::clicked, w,
            [w, importStatLbl](){ settingsDoImportExternalAi(w, importStatLbl); });

    settingsLoadFeedbackData(table, statLbl);

    connect(btnRefresh, &QPushButton::clicked, w,
            [table, statLbl](){ settingsLoadFeedbackData(table, statLbl); });

    connect(btnExport, &QPushButton::clicked, w,
            [w, statLbl](){ settingsDoExportDpo(w, statLbl); });

    connect(btnClear, &QPushButton::clicked, w,
            [w, table, statLbl](){ settingsDoClrFeedback(w, table, statLbl); });

    return w;
}

/* ══════════════════════════════════════════════════════════════
   buildSandboxTab — Docker sandbox per esecuzione codice AI
   ══════════════════════════════════════════════════════════════ */

