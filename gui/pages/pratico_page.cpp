#include "pratico_page.h"
#include <QVBoxLayout>
#include <QHBoxLayout>
#include <QLabel>
#include <QFrame>
#include <QGridLayout>
#include <QPushButton>
#include <QGroupBox>
#include <QDoubleSpinBox>
#include <QSpinBox>
#include <QTextBrowser>
#include <QMenu>
#include <QGuiApplication>
#include <QClipboard>
#include <QProcess>
#include <QPainter>
#include <QPainterPath>
#include <QDate>
#include <QDateEdit>
#include <QCheckBox>
#include <QComboBox>
#include <QJsonDocument>
#include <QJsonObject>
#include <memory>
#include <cmath>
#include "../prismalux_paths.h"
namespace P = PrismaluxPaths;

/* ══════════════════════════════════════════════════════════════
   Chat panel riutilizzabile (730, P.IVA)
   ══════════════════════════════════════════════════════════════ */
QWidget* PraticoPage::buildChat(const QString& title,
                                 const QString& sysPrompt,
                                 const QString& placeholder) {
    auto* w   = new QWidget;
    auto* lay = new QVBoxLayout(w);
    lay->setContentsMargins(24, 20, 24, 16);
    lay->setSpacing(10);

    auto* hdr = new QWidget(w);
    auto* hdrL = new QHBoxLayout(hdr);
    hdrL->setContentsMargins(0,0,0,0);
    auto* back = new QPushButton("\xe2\x86\x90 Torna", hdr);
    back->setObjectName("actionBtn");
    auto* lbl  = new QLabel(title, hdr);
    lbl->setObjectName("pageTitle");
    hdrL->addWidget(back);
    hdrL->addWidget(lbl, 1);
    lay->addWidget(hdr);

    auto* div = new QFrame(w);
    div->setObjectName("pageDivider"); div->setFrameShape(QFrame::HLine);
    lay->addWidget(div);

    auto* log = new QTextEdit(w);
    log->setObjectName("chatLog");
    log->setReadOnly(true);
    log->setPlaceholderText(placeholder);
    lay->addWidget(log, 1);

    log->setContextMenuPolicy(Qt::CustomContextMenu);
    connect(log, &QTextEdit::customContextMenuRequested, w, [log](const QPoint& pos){
        const QString sel   = log->textCursor().selectedText();
        const bool hasSel   = !sel.isEmpty();
        const QString label = hasSel ? "selezione" : "tutto";
        QMenu menu(log);
        QAction* actCopy = menu.addAction("\xf0\x9f\x97\x82  Copia " + label);
        QAction* actRead = menu.addAction("\xf0\x9f\x8e\x99  Leggi " + label);
        QAction* chosen  = menu.exec(log->mapToGlobal(pos));
        const QString txt = hasSel ? sel : log->toPlainText();
        if (chosen == actCopy) {
            QGuiApplication::clipboard()->setText(txt);
        } else if (chosen == actRead) {
            QStringList words = txt.split(' ', Qt::SkipEmptyParts);
            if (words.size() > 400) words = words.mid(words.size() - 400);
            QProcess::startDetached("espeak-ng", {"-v", "it+f3", "--punct=none", words.join(" ")});
        }
    });

    auto* waitLbl = new QLabel("\xe2\x8f\xb3  Elaborazione in corso...", w);
    waitLbl->setStyleSheet("color:#E5C400; font-style:italic; padding:2px 0;");
    waitLbl->setVisible(false);
    lay->addWidget(waitLbl);

    auto* inputRow = new QWidget(w);
    auto* inL      = new QHBoxLayout(inputRow);
    inL->setContentsMargins(0,0,0,0); inL->setSpacing(8);
    auto* inp = new QLineEdit(inputRow);
    inp->setObjectName("chatInput");
    inp->setPlaceholderText("Scrivi la tua domanda...");
    inp->setFixedHeight(38);
    auto* send = new QPushButton("Invia \xe2\x96\xb6", inputRow);
    send->setObjectName("actionBtn");
    auto* stop = new QPushButton("\xe2\x8f\xb9", inputRow);
    stop->setObjectName("actionBtn");
    stop->setProperty("danger", true);
    stop->setFixedWidth(40);
    stop->setEnabled(false);
    inL->addWidget(inp, 1);
    inL->addWidget(send);
    inL->addWidget(stop);
    lay->addWidget(inputRow);

    auto active = std::make_shared<bool>(false);

    connect(back, &QPushButton::clicked, this, &PraticoPage::onBackToMenu);
    connect(stop, &QPushButton::clicked, m_ai, &AiClient::abort);

    auto sendFn = [=]{
        QString msg = inp->text().trimmed();
        if (msg.isEmpty()) return;
        log->append(QString("\n\xf0\x9f\x91\xa4  Tu: %1\n").arg(msg));
        log->append("\xf0\x9f\xa4\x96  AI: ");
        inp->clear();
        send->setEnabled(false); stop->setEnabled(true); waitLbl->setVisible(true);
        *active = true;
        m_ai->chat(sysPrompt, msg);
    };

    connect(send, &QPushButton::clicked, w, sendFn);
    connect(inp,  &QLineEdit::returnPressed, w, sendFn);

    connect(m_ai, &AiClient::token, w, [=](const QString& t){
        if (!*active) return;
        QTextCursor c(log->document()); c.movePosition(QTextCursor::End);
        c.insertText(t); log->ensureCursorVisible();
    });
    connect(m_ai, &AiClient::finished, w, [=](const QString&){
        if (!*active) return;
        *active = false;
        log->append("\n\xe2\x94\x80\xe2\x94\x80\xe2\x94\x80\xe2\x94\x80\xe2\x94\x80\xe2\x94\x80\xe2\x94\x80\xe2\x94\x80\xe2\x94\x80\xe2\x94\x80");
        send->setEnabled(true); stop->setEnabled(false); waitLbl->setVisible(false);
    });
    connect(m_ai, &AiClient::error, w, [=](const QString& err){
        if (!*active) return;
        *active = false;
        const QString el = err.toLower();
        if (el.contains("operation canceled") || el.contains("canceled")) {
            /* abort silenzioso */
        } else if (el.contains("ram critica")) {
            log->append("\n" + err);
        } else if (el.contains("connection refused") || el.contains("refused")) {
            log->append((m_ai->backend() == AiClient::Ollama)
                ? "\n\xe2\x9d\x8c  Backend non raggiungibile. Avvia Ollama: <code>ollama serve</code>"
                : "\n\xe2\x9d\x8c  Backend non raggiungibile. Avvia llama-server dalla tab \xf0\x9f\x94\x8c Backend.");
        } else if (el.contains("remotehostclosed") || el.contains("connection reset")) {
            log->append("\n\xe2\x9a\xa0  Connessione chiusa. Possibile crash del server o VRAM esaurita.");
        } else if (el.contains("timed out") || el.contains("timeout")) {
            log->append("\n\xe2\x8f\xb1  Timeout: nessuna risposta dal backend. Il modello potrebbe essere in caricamento.");
        } else {
            const QString hint = (m_ai->backend() == AiClient::Ollama)
                ? "\xf0\x9f\x92\xa1  Verifica che Ollama sia in esecuzione: ollama serve"
                : "\xf0\x9f\x92\xa1  Verifica che llama-server sia avviato sulla porta corretta.";
            log->append(QString("\n\xe2\x9d\x8c  Errore: %1\n%2").arg(err, hint));
        }
        send->setEnabled(true); stop->setEnabled(false); waitLbl->setVisible(false);
    });
    connect(m_ai, &AiClient::aborted, w, [=]{
        if (!*active) return;
        *active = false;
        log->append("\n\xe2\x8f\xb9  Interrotto.\n\xe2\x94\x80\xe2\x94\x80\xe2\x94\x80\xe2\x94\x80\xe2\x94\x80\xe2\x94\x80\xe2\x94\x80\xe2\x94\x80\xe2\x94\x80\xe2\x94\x80");
        send->setEnabled(true); stop->setEnabled(false); waitLbl->setVisible(false);
    });

    return w;
}

/* ══════════════════════════════════════════════════════════════
   Menu principale Pratico
   ══════════════════════════════════════════════════════════════ */
QWidget* PraticoPage::buildMenu() {
    auto* w   = new QWidget;
    auto* lay = new QVBoxLayout(w);
    lay->setContentsMargins(24, 20, 24, 16);
    lay->setSpacing(12);

    auto* div = new QFrame(w); div->setObjectName("pageDivider");
    div->setFrameShape(QFrame::HLine); lay->addWidget(div);

    struct Item { QString icon, title, desc; int page; };
    QList<Item> items = {
        {"\xf0\x9f\x93\x84", "Assistente 730",
         "Guida alla compilazione del 730, detrazioni, rimborsi fiscali.", 1},
        {"\xf0\x9f\x92\xbc", "Partita IVA",
         "Regime forfettario, calcolo imposte, obblighi contributivi.", 2},
        {"\xf0\x9f\x92\xb0", "Calcolatore Finanza",
         "Mutuo, PAC, stima pensione INPS \xe2\x80\x94 calcoli locali istantanei.", 3},
        {"\xf0\x9f\x92\xbc", "Scheda TFR",
         "Trattamento di Fine Rapporto: inserisci i tuoi dati o compilali da RAG. "
         "Calcola il montante rivalutato e la fiscalit\xc3\xa0 (azienda / fondo pensione).", 4},
    };

    auto* grid = new QWidget(w);
    auto* gLay = new QVBoxLayout(grid);
    gLay->setSpacing(10);
    gLay->setContentsMargins(0,0,0,0);

    for (auto& it : items) {
        auto* card = new QFrame(grid);
        card->setObjectName("actionCard");
        auto* cl = new QHBoxLayout(card);
        cl->setContentsMargins(16, 14, 16, 14); cl->setSpacing(14);

        auto* ico = new QLabel(it.icon, card);
        ico->setObjectName("cardIcon"); ico->setFixedWidth(32);

        auto* txt  = new QWidget(card);
        auto* txtL = new QVBoxLayout(txt);
        txtL->setContentsMargins(0,0,0,0); txtL->setSpacing(3);
        auto* lt = new QLabel(it.title, txt); lt->setObjectName("cardTitle");
        auto* ld = new QLabel(it.desc,  txt); ld->setObjectName("cardDesc");
        ld->setWordWrap(true);
        txtL->addWidget(lt); txtL->addWidget(ld);

        auto* goBtn = new QPushButton("Apri \xe2\x86\x92", card);
        goBtn->setObjectName("actionBtn"); goBtn->setFixedWidth(80);
        goBtn->setProperty("pageIndex", it.page);
        connect(goBtn, &QPushButton::clicked, this, &PraticoPage::onMenuCardClicked);

        cl->addWidget(ico); cl->addWidget(txt, 1); cl->addWidget(goBtn);
        gLay->addWidget(card);
    }

    gLay->addStretch(1);
    lay->addWidget(grid, 1);
    return w;
}

/* ══════════════════════════════════════════════════════════════
   FinChart — grafico cartesiano QPainter (X = Anno Solare, Y = €)
   ══════════════════════════════════════════════════════════════ */
class FinChart : public QWidget {
public:
    explicit FinChart(QWidget* parent = nullptr) : QWidget(parent) {
        setMinimumHeight(240);
        setSizePolicy(QSizePolicy::Expanding, QSizePolicy::Fixed);
    }

    void setData(const QString& label, const QVector<QPointF>& pts, const QColor& col) {
        m_label = label;
        m_pts   = pts;
        m_color = col;
        update();
    }

protected:
    void paintEvent(QPaintEvent*) override {
        QPainter p(this);
        p.setRenderHint(QPainter::Antialiasing);

        const QRect  r    = rect();
        const bool   dark = palette().color(QPalette::Window).lightness() < 128;
        const QColor bg        = dark ? QColor(28, 31, 40)   : QColor(248, 249, 252);
        const QColor gridCol   = dark ? QColor(55, 62, 80)   : QColor(200, 208, 220);
        const QColor axisCol   = dark ? QColor(160, 170, 195): QColor(60, 75, 100);
        const QColor textCol   = axisCol;

        p.fillRect(r, bg);

        if (m_pts.isEmpty()) {
            p.setPen(textCol);
            QFont f = p.font(); f.setPointSizeF(9); p.setFont(f);
            p.drawText(r, Qt::AlignCenter,
                       "Clicca \xe2\x80\x9c" "Calcola\xe2\x80\x9d per visualizzare il grafico");
            return;
        }

        // margins: sinistra per etichette Y, basso per etichette X
        const int ML = 72, MB = 44, MT = 30, MR = 18;
        const QRect plot(ML, MT, r.width() - ML - MR, r.height() - MT - MB);

        // range X
        const double xMin = m_pts.first().x();
        const double xMax = m_pts.last().x();

        // range Y — ceil a potenza di 10 "bella"
        double yMax = 0.0;
        for (const QPointF& pt : m_pts) yMax = qMax(yMax, pt.y());
        if (yMax <= 0.0) yMax = 1.0;
        const double mag = std::pow(10.0, std::floor(std::log10(yMax)));
        yMax = std::ceil(yMax / mag) * mag;

        auto toPixel = [&](double x, double y) -> QPointF {
            const double px = plot.left()   + (x - xMin) / (xMax - xMin) * plot.width();
            const double py = plot.bottom() - y / yMax * plot.height();
            return { px, py };
        };

        // griglia orizzontale
        const int NY = 5;
        p.setPen(QPen(gridCol, 1, Qt::DotLine));
        for (int i = 0; i <= NY; ++i) {
            const double y  = yMax * i / NY;
            const QPointF l = toPixel(xMin, y);
            const QPointF rp = toPixel(xMax, y);
            p.drawLine(l, rp);
        }

        // assi
        p.setPen(QPen(axisCol, 1.5));
        p.drawLine(plot.bottomLeft(), plot.bottomRight());
        p.drawLine(plot.bottomLeft(), plot.topLeft());

        QFont fntSm = p.font(); fntSm.setPointSizeF(7.5); p.setFont(fntSm);
        p.setPen(textCol);

        // etichette Y
        for (int i = 0; i <= NY; ++i) {
            const double  y  = yMax * i / NY;
            const QPointF pt = toPixel(xMin, y);
            QString lbl;
            if      (y >= 1e6) lbl = QString::number(y / 1e6, 'f', 1) + "M";
            else if (y >= 1e3) lbl = QString::number(y / 1e3, 'f', 0) + "k";
            else               lbl = QString::number(y, 'f', 0);
            p.drawText(QRectF(0, pt.y() - 10, ML - 6, 20),
                       Qt::AlignRight | Qt::AlignVCenter, lbl);
        }

        // etichette X (anni solari)
        const int totalYears = qMax(1, (int)(xMax - xMin));
        const int step = qMax(1, totalYears / 8);
        for (int yr = (int)xMin; yr <= (int)xMax; yr += step) {
            const QPointF pt = toPixel((double)yr, 0.0);
            p.drawText(QRectF(pt.x() - 22, plot.bottom() + 4, 44, 18),
                       Qt::AlignCenter, QString::number(yr));
        }

        // titolo asse Y (ruotato)
        p.save();
        p.translate(10, plot.top() + plot.height() / 2);
        p.rotate(-90);
        QFont fntAx = fntSm; fntAx.setBold(true); p.setFont(fntAx);
        p.drawText(QRectF(-40, -10, 80, 20), Qt::AlignCenter,
                   "Euro \xe2\x82\xac");
        p.restore();

        // titolo asse X
        p.setFont(fntAx);
        p.drawText(QRectF(ML, r.height() - MB + 22, plot.width(), 18),
                   Qt::AlignCenter, "Anno Solare");

        // etichetta serie (in alto)
        QFont fntTitle = fntSm; fntTitle.setPointSizeF(8.5); fntTitle.setBold(true);
        p.setFont(fntTitle);
        p.setPen(m_color);
        p.drawText(QRectF(ML, 4, plot.width(), MT - 6), Qt::AlignCenter, m_label);

        if (m_pts.size() < 2) return;

        // area riempita
        QPainterPath area;
        area.moveTo(toPixel(m_pts.first().x(), 0.0));
        for (const QPointF& pt : m_pts) area.lineTo(toPixel(pt.x(), pt.y()));
        area.lineTo(toPixel(m_pts.last().x(), 0.0));
        area.closeSubpath();
        QColor fill = m_color; fill.setAlpha(45);
        p.fillPath(area, fill);

        // linea serie
        QPainterPath line;
        line.moveTo(toPixel(m_pts.first().x(), m_pts.first().y()));
        for (int i = 1; i < m_pts.size(); ++i)
            line.lineTo(toPixel(m_pts[i].x(), m_pts[i].y()));
        p.setPen(QPen(m_color, 2.5, Qt::SolidLine, Qt::RoundCap, Qt::RoundJoin));
        p.drawPath(line);

        // punti
        p.setBrush(m_color);
        p.setPen(Qt::NoPen);
        const int dotR = 3;
        for (const QPointF& pt : m_pts) {
            const QPointF px = toPixel(pt.x(), pt.y());
            p.drawEllipse(px, dotR, dotR);
        }
    }

private:
    QString          m_label;
    QVector<QPointF> m_pts;
    QColor           m_color { "#4c9be8" };
};

/* ══════════════════════════════════════════════════════════════
   buildFinanza — Calcolatori finanziari locali (0 token AI)
   ══════════════════════════════════════════════════════════════ */
static QWidget* buildFinanza(QStackedWidget* inner, AiClient* ai) {
    const int baseYear = QDate::currentDate().year();
    auto* w   = new QWidget;
    auto* lay = new QVBoxLayout(w);
    lay->setContentsMargins(20, 16, 20, 16);
    lay->setSpacing(14);

    auto* hdrW = new QWidget(w);
    auto* hdrL = new QHBoxLayout(hdrW);
    hdrL->setContentsMargins(0,0,0,0);
    auto* back = new QPushButton("\xe2\x86\x90 Torna", hdrW);
    back->setObjectName("actionBtn");
    QObject::connect(back, &QPushButton::clicked, w, [inner]{ inner->setCurrentIndex(0); });
    auto* titleLbl = new QLabel("\xf0\x9f\x92\xb0  Calcolatori Finanziari", hdrW);
    titleLbl->setObjectName("pageTitle");
    hdrL->addWidget(back); hdrL->addWidget(titleLbl, 1);
    lay->addWidget(hdrW);

    auto* div = new QFrame(w); div->setObjectName("pageDivider");
    div->setFrameShape(QFrame::HLine); lay->addWidget(div);

    auto* outLbl = new QTextBrowser(w);
    outLbl->setObjectName("chatLog");
    outLbl->setPlaceholderText("I risultati dei calcoli appariranno qui...");
    outLbl->setMinimumHeight(110);

    auto* chart = new FinChart(w);

    /* ══ Mutuo ══ */
    auto* mutGroup = new QGroupBox("\xf0\x9f\x8f\xa0  Calcolatore Mutuo", w);
    auto* mutLay   = new QHBoxLayout(mutGroup);
    mutLay->setSpacing(10);

    auto mkSpin = [](double min, double max, double val, int dec, const QString& suffix) -> QDoubleSpinBox* {
        auto* s = new QDoubleSpinBox;
        s->setRange(min, max); s->setValue(val); s->setDecimals(dec);
        s->setSuffix(suffix); s->setFixedWidth(140);
        return s;
    };

    auto* capSpin   = mkSpin(1000, 2000000, 150000, 0, " \xe2\x82\xac");
    auto* tassoSpin = mkSpin(0.01, 30.0,    3.5,    2, " %");
    auto* anniSpin  = new QSpinBox; anniSpin->setRange(1, 40); anniSpin->setValue(20);
    anniSpin->setSuffix(" anni"); anniSpin->setFixedWidth(110);
    auto* calcMut   = new QPushButton("Calcola", mutGroup);
    calcMut->setObjectName("actionBtn");

    mutLay->addWidget(new QLabel("Capitale:")); mutLay->addWidget(capSpin);
    mutLay->addWidget(new QLabel("Tasso annuo:")); mutLay->addWidget(tassoSpin);
    mutLay->addWidget(new QLabel("Durata:")); mutLay->addWidget(anniSpin);
    mutLay->addWidget(calcMut); mutLay->addStretch(1);

    QObject::connect(calcMut, &QPushButton::clicked, w, [=]{
        double C     = capSpin->value();
        double tasso = tassoSpin->value() / 12.0 / 100.0;
        int    n     = anniSpin->value() * 12;
        double rata;
        if (tasso == 0) rata = C / n;
        else            rata = C * (tasso * std::pow(1+tasso, n)) / (std::pow(1+tasso, n) - 1);
        double totPagato    = rata * n;
        double totInteressi = totPagato - C;
        QString txt = QString(
            "<b>\xf0\x9f\x8f\xa0 Mutuo: %1 \xe2\x82\xac \xe2\x80\x94 %2% \xe2\x80\x94 %3 anni</b><br>"
            "Rata mensile: <b>%4 \xe2\x82\xac</b><br>"
            "Totale pagato: <b>%5 \xe2\x82\xac</b><br>"
            "Totale interessi: <b>%6 \xe2\x82\xac</b><br><br>"
            "<b>Piano ammortamento (prime 12 rate):</b><br>")
            .arg(C,0,'f',0).arg(tassoSpin->value(),0,'f',2).arg(anniSpin->value())
            .arg(rata,0,'f',2).arg(totPagato,0,'f',2).arg(totInteressi,0,'f',2);
        double debRes = C;
        for (int i = 1; i <= qMin(12, n); i++) {
            double interesse = debRes * tasso;
            double quota     = rata - interesse;
            debRes          -= quota;
            txt += QString("Rata %1: \xe2\x82\xac%2 (cap: %3 \xe2\x80\x94 int: %4 \xe2\x80\x94 res: %5)<br>")
                   .arg(i).arg(rata,0,'f',2).arg(quota,0,'f',2)
                   .arg(interesse,0,'f',2).arg(qMax(0.0,debRes),0,'f',2);
        }
        // grafico: debito residuo a fine di ogni anno
        QVector<QPointF> pts;
        pts.append({ double(baseYear), C });
        double debChart = C;
        for (int yr = 1; yr <= anniSpin->value(); ++yr) {
            for (int m = 0; m < 12; ++m) {
                if ((yr - 1) * 12 + m >= n) break;
                double interesse = debChart * tasso;
                debChart        -= (rata - interesse);
            }
            pts.append({ double(baseYear + yr), qMax(0.0, debChart) });
        }
        chart->setData("\xf0\x9f\x8f\xa0 Debito residuo mutuo", pts, QColor("#e07040"));
        outLbl->setHtml(txt);
    });

    /* ══ PAC ══ */
    auto* pacGroup = new QGroupBox("\xf0\x9f\x93\x88  Piano Accumulo Capitale (PAC)", w);
    auto* pacLay   = new QHBoxLayout(pacGroup);
    pacLay->setSpacing(10);

    auto* rataPac  = mkSpin(10, 10000, 200, 0, " \xe2\x82\xac/mese");
    auto* rendSpin = mkSpin(0,  30,    5,   2, " %/anno");
    auto* anniPac  = new QSpinBox; anniPac->setRange(1, 50); anniPac->setValue(20);
    anniPac->setSuffix(" anni"); anniPac->setFixedWidth(110);
    auto* calcPac  = new QPushButton("Calcola", pacGroup);
    calcPac->setObjectName("actionBtn");

    pacLay->addWidget(new QLabel("Rata mensile:")); pacLay->addWidget(rataPac);
    pacLay->addWidget(new QLabel("Rendimento:")); pacLay->addWidget(rendSpin);
    pacLay->addWidget(new QLabel("Durata:")); pacLay->addWidget(anniPac);
    pacLay->addWidget(calcPac); pacLay->addStretch(1);

    QObject::connect(calcPac, &QPushButton::clicked, w, [=]{
        double rata = rataPac->value();
        double r    = rendSpin->value() / 12.0 / 100.0;
        int    n    = anniPac->value() * 12;
        double FV;
        if (r == 0) FV = rata * n;
        else        FV = rata * (std::pow(1+r, n) - 1) / r;
        double versato    = rata * n;
        double rendimento = FV - versato;
        // grafico: capitale accumulato a fine di ogni anno
        QVector<QPointF> pts;
        pts.append({ double(baseYear), 0.0 });
        for (int yr = 1; yr <= anniPac->value(); ++yr) {
            const int m  = yr * 12;
            const double fv = (r == 0) ? rata * m : rata * (std::pow(1+r, m) - 1) / r;
            pts.append({ double(baseYear + yr), fv });
        }
        chart->setData("\xf0\x9f\x93\x88 Capitale accumulato (PAC)", pts, QColor("#40b870"));
        outLbl->setHtml(QString(
            "<b>\xf0\x9f\x93\x88 PAC: %1 \xe2\x82\xac/mese \xe2\x80\x94 %2% \xe2\x80\x94 %3 anni</b><br>"
            "Capitale accumulato: <b>%4 \xe2\x82\xac</b><br>"
            "Capitale versato: <b>%5 \xe2\x82\xac</b><br>"
            "Rendimento totale: <b>%6 \xe2\x82\xac (%7%)</b>")
            .arg(rata,0,'f',0).arg(rendSpin->value(),0,'f',2).arg(anniPac->value())
            .arg(FV,0,'f',2).arg(versato,0,'f',2).arg(rendimento,0,'f',2)
            .arg(versato > 0 ? rendimento/versato*100 : 0, 0,'f',1));
    });

    /* ══ Pensione INPS ══ */
    auto* penGroup = new QGroupBox("\xf0\x9f\x91\xb4  Stima Pensione INPS (Metodo Contributivo)", w);
    auto* penLay   = new QHBoxLayout(penGroup);
    penLay->setSpacing(10);

    auto* stipSpin = mkSpin(10000, 500000, 35000, 0, " \xe2\x82\xac/anno");
    auto* anniLav  = new QSpinBox; anniLav->setRange(1, 50); anniLav->setValue(35);
    anniLav->setSuffix(" anni"); anniLav->setFixedWidth(110);
    auto* aliqSpin = mkSpin(1, 50, 33, 1, " %");
    auto* calcPen  = new QPushButton("Calcola", penGroup);
    calcPen->setObjectName("actionBtn");

    penLay->addWidget(new QLabel("Stipendio lordo:")); penLay->addWidget(stipSpin);
    penLay->addWidget(new QLabel("Anni lavoro:")); penLay->addWidget(anniLav);
    penLay->addWidget(new QLabel("Aliquota:")); penLay->addWidget(aliqSpin);
    penLay->addWidget(calcPen); penLay->addStretch(1);

    QObject::connect(calcPen, &QPushButton::clicked, w, [=]{
        double stip   = stipSpin->value();
        int    anni   = anniLav->value();
        double aliq   = aliqSpin->value() / 100.0;
        double contrib    = stip * aliq * anni;
        double pensAnnua  = contrib * 0.05723;
        double pensMensile = pensAnnua / 13.0;
        // grafico: contributi accumulati anno per anno
        QVector<QPointF> pts;
        pts.append({ double(baseYear), 0.0 });
        for (int yr = 1; yr <= anni; ++yr)
            pts.append({ double(baseYear + yr), stip * aliq * yr });
        chart->setData("\xf0\x9f\x91\xb4 Contributi accumulati (INPS)", pts, QColor("#5090e0"));
        outLbl->setHtml(QString(
            "<b>\xf0\x9f\x91\xb4 Stima Pensione INPS (metodologia contributiva semplificata)</b><br>"
            "Stipendio lordo annuo: %1 \xe2\x82\xac \xe2\x80\x94 Anni: %2 \xe2\x80\x94 Aliquota: %3%<br>"
            "Contributi totali stimati: <b>%4 \xe2\x82\xac</b><br>"
            "Pensione annua stimata: <b>%5 \xe2\x82\xac</b><br>"
            "Pensione mensile stimata (13 mensilit\xc3\xa0): <b>%6 \xe2\x82\xac</b><br><br>"
            "<i>Stima indicativa \xe2\x80\x94 coeff. 5.723% per pensionamento a 67 anni (INPS 2024).</i>")
            .arg(stip,0,'f',0).arg(anni).arg(aliqSpin->value(),0,'f',1)
            .arg(contrib,0,'f',0).arg(pensAnnua,0,'f',0).arg(pensMensile,0,'f',0));
    });

    lay->addWidget(mutGroup);
    lay->addWidget(pacGroup);
    lay->addWidget(penGroup);
    lay->addWidget(outLbl, 1);
    lay->addWidget(chart);
    return w;
}

/* ══════════════════════════════════════════════════════════════
   Codice Fiscale — helper functions
   Algoritmo ufficiale Agenzia delle Entrate (D.M. 23/12/1976)
   ══════════════════════════════════════════════════════════════ */

/** Normalizza il nome comune: minuscolo, rimuove accenti, trim. */
static QString normalizzaComune(const QString& s)
{
    static const QHash<QChar,QChar> kAcc = {
        {u'\xe0','a'},{u'\xe1','a'},{u'\xe2','a'},
        {u'\xe8','e'},{u'\xe9','e'},{u'\xea','e'},
        {u'\xec','i'},{u'\xed','i'},{u'\xee','i'},
        {u'\xf2','o'},{u'\xf3','o'},{u'\xf4','o'},
        {u'\xf9','u'},{u'\xfa','u'},{u'\xfb','u'},
        {u'\xfc','u'}
    };
    QString r;
    for (QChar c : s.toLower().trimmed())
        r += kAcc.value(c, c);
    return r;
}

/** Restituisce il codice Belfiore (4 char) per il comune italiano,
 *  oppure stringa vuota se non trovato. */
static QString cercaBelfiore(const QString& comune)
{
    static const QHash<QString,QString> kBelfiore = {
        /* ── Capoluoghi di regione ──────────────────────── */
        {"torino","L219"},     {"milano","F205"},     {"venezia","L736"},
        {"genova","D969"},     {"bologna","A944"},     {"firenze","D612"},
        {"ancona","A271"},     {"roma","H501"},        {"napoli","F839"},
        {"bari","A662"},       {"potenza","G942"},     {"catanzaro","C374"},
        {"palermo","G273"},    {"cagliari","B354"},    {"trento","L378"},
        {"trieste","L424"},    {"perugia","G478"},     {"campobasso","B519"},
        {"aosta","A326"},      {"l'aquila","A345"},    {"aquila","A345"},
        /* ── Capoluoghi di provincia ─────────────────────── */
        {"alessandria","A182"},{"asti","A479"},        {"avellino","A509"},
        {"belluno","A757"},    {"benevento","A783"},   {"bergamo","A794"},
        {"biella","A859"},     {"bolzano","A952"},     {"bozen","A952"},
        {"brescia","B157"},    {"brindisi","B180"},    {"cagliari","B354"},
        {"caltanissetta","B429"},{"campobasso","B519"},{"caserta","B963"},
        {"catania","C351"},    {"chieti","C632"},      {"como","C933"},
        {"cosenza","D086"},    {"cremona","D150"},     {"crotone","D122"},
        {"cuneo","D205"},      {"enna","C342"},        {"fermo","D542"},
        {"ferrara","D548"},    {"foggia","D643"},      {"forli","D704"},
        {"frosinone","D810"},  {"genova","D969"},      {"gorizia","E098"},
        {"grosseto","E202"},   {"imperia","E290"},     {"isernia","E335"},
        {"la spezia","E463"},  {"laspezia","E463"},    {"latina","E472"},
        {"lecce","E794"},      {"lecco","E507"},       {"livorno","E625"},
        {"lodi","E648"},       {"lucca","E715"},       {"macerata","E783"},
        {"mantova","E897"},    {"massa","F023"},       {"matera","F052"},
        {"messina","F158"},    {"modena","F257"},      {"monza","F704"},
        {"napoli","F839"},     {"novara","F952"},      {"nuoro","F979"},
        {"oristano","G113"},   {"padova","G224"},      {"palermo","G273"},
        {"parma","G337"},      {"pavia","G388"},       {"perugia","G478"},
        {"pesaro","G453"},     {"pescara","G482"},     {"piacenza","G535"},
        {"pisa","G702"},       {"pistoia","G713"},     {"pordenone","G888"},
        {"potenza","G942"},    {"prato","G999"},       {"ragusa","H163"},
        {"ravenna","H199"},    {"reggio calabria","H224"},
        {"reggio emilia","H223"},{"regio emilia","H223"},
        {"rieti","H282"},      {"rimini","H294"},      {"roma","H501"},
        {"rovigo","H620"},     {"salerno","H703"},     {"sassari","I452"},
        {"savona","I480"},     {"siena","I726"},       {"siracusa","I754"},
        {"sondrio","I829"},    {"taranto","L049"},     {"teramo","L103"},
        {"terni","L117"},      {"torino","L219"},      {"trapani","L331"},
        {"trento","L378"},     {"treviso","L407"},     {"trieste","L424"},
        {"udine","L483"},      {"varese","L682"},      {"venezia","L736"},
        {"verbania","L746"},   {"vercelli","L750"},    {"verona","L781"},
        {"vibo valentia","F1104"},                     {"vicenza","L840"},
        {"viterbo","M082"},
        /* ── Grandi comuni extra ─────────────────────────── */
        {"giugliano in campania","E054"},
        {"catanzaro marina","C374"},
        {"andria","A285"},     {"barletta","A669"},    {"reggio","H224"},
        /* ── Paesi esteri (codici Z) ─────────────────────── */
        {"albania","Z100"},    {"algeria","Z301"},     {"argentina","Z600"},
        {"australia","Z700"},  {"austria","Z102"},     {"belgio","Z103"},
        {"brasile","Z602"},    {"canada","Z401"},      {"cina","Z210"},
        {"croazia","Z118"},    {"danimarca","Z104"},   {"egitto","Z336"},
        {"etiopia","Z315"},    {"filippine","Z216"},   {"finlandia","Z105"},
        {"francia","Z110"},    {"germania","Z112"},    {"giappone","Z219"},
        {"grecia","Z115"},     {"india","Z222"},       {"iran","Z224"},
        {"irlanda","Z116"},    {"israele","Z225"},     {"marocco","Z330"},
        {"messico","Z605"},    {"nigeria","Z325"},     {"norvegia","Z120"},
        {"paesi bassi","Z121"},{"olanda","Z121"},      {"pakistan","Z236"},
        {"polonia","Z127"},    {"portogallo","Z128"},  {"romania","Z129"},
        {"russia","Z130"},     {"senegal","Z343"},     {"somalia","Z342"},
        {"spagna","Z131"},     {"stati uniti","Z404"},{"usa","Z404"},
        {"america","Z404"},    {"svezia","Z132"},      {"svizzera","Z133"},
        {"tunisia","Z352"},    {"turchia","Z243"},     {"ucraina","Z138"},
        {"regno unito","Z114"},{"inghilterra","Z114"},{"gran bretagna","Z114"},
    };
    return kBelfiore.value(normalizzaComune(comune), "");
}

/** Calcola il Codice Fiscale italiano.
 *  Restituisce stringa vuota se i dati non sono validi. */
static QString calcolaCodiceFiscale(
    const QString& cognome, const QString& nome,
    const QDate& nascita, bool maschio, const QString& belfiore)
{
    if (cognome.trimmed().isEmpty() || nome.trimmed().isEmpty()
        || !nascita.isValid() || belfiore.length() != 4)
        return {};

    /* Estrai consonanti e vocali dal testo (solo lettere A-Z) */
    auto lettera = [](QChar c) -> bool { return c.isLetter(); };
    auto isVoc   = [](QChar c) -> bool {
        return QString("AEIOU").contains(c);
    };
    auto cons = [&](const QString& s) {
        QString r;
        for (QChar c : s.toUpper())
            if (lettera(c) && !isVoc(c)) r += c;
        return r;
    };
    auto voc = [&](const QString& s) {
        QString r;
        for (QChar c : s.toUpper())
            if (lettera(c) && isVoc(c)) r += c;
        return r;
    };

    /* Cognome: prime 3 tra consonanti + vocali + X */
    QString cog = cons(cognome) + voc(cognome);
    while (cog.length() < 3) cog += 'X';
    cog = cog.left(3);

    /* Nome: se >= 4 consonanti prendi 1a, 3a, 4a; altrimenti consonanti + vocali + X */
    const QString nc = cons(nome);
    const QString nv = voc(nome);
    QString nom;
    if (nc.length() >= 4)
        nom = QString(nc[0]) + nc[2] + nc[3];
    else {
        nom = nc + nv;
        while (nom.length() < 3) nom += 'X';
        nom = nom.left(3);
    }

    /* Anno (2 cifre), mese (lettera), giorno + sesso */
    const QString anno  = nascita.toString("yy");
    const char kMesi[]  = "ABCDEHLMPRST";
    const QChar  mese   = kMesi[nascita.month() - 1];
    const int    giorno = nascita.day() + (maschio ? 0 : 40);
    const QString gg    = QString("%1").arg(giorno, 2, 10, QChar('0'));

    /* Stringa 15 caratteri */
    const QString cf15 = cog + nom + anno + mese + gg + belfiore.toUpper();
    if (cf15.length() != 15) return {};

    /* Tabella dispari (posizioni 1,3,5,…) — indice = posizione in 0-35 */
    static const int kDisp[36] = {
         1, 0, 5, 7, 9,13,15,17,19,21,   /* 0-9   */
         1, 0, 5, 7, 9,13,15,17,19,21,   /* A-J   */
         2, 4,18,20,11, 3, 6, 8,12,14,   /* K-T   */
        16,10,22,25,24,23                /* U-Z   */
    };

    int somma = 0;
    for (int i = 0; i < 15; ++i) {
        const QChar c = cf15[i];
        int idx;
        if (c.isDigit()) idx = c.digitValue();
        else             idx = 10 + (c.unicode() - 'A');

        if (i % 2 == 0)   /* posizione dispari (1-indexed) */
            somma += kDisp[idx];
        else               /* posizione pari */
            somma += idx;
    }

    return cf15 + QChar('A' + (somma % 26));
}

/* ══════════════════════════════════════════════════════════════
   buildSchedaTFR — pagina dedicata TFR (index 4)
   ══════════════════════════════════════════════════════════════ */
static QWidget* buildSchedaTFR(QStackedWidget* inner, AiClient* ai)
{
    const int baseYear = QDate::currentDate().year();
    auto* w   = new QWidget;
    auto* lay = new QVBoxLayout(w);
    lay->setContentsMargins(20, 16, 20, 16);
    lay->setSpacing(10);

    /* ── Header con back button ── */
    auto* hdrW = new QWidget(w);
    auto* hdrL = new QHBoxLayout(hdrW);
    hdrL->setContentsMargins(0,0,0,0);
    auto* back = new QPushButton("\xe2\x86\x90 Torna", hdrW);
    back->setObjectName("actionBtn");
    QObject::connect(back, &QPushButton::clicked, w, [inner]{ inner->setCurrentIndex(0); });
    auto* titleLbl = new QLabel("\xf0\x9f\x92\xbc  Scheda TFR \xe2\x80\x94 Trattamento di Fine Rapporto", hdrW);
    titleLbl->setObjectName("pageTitle");
    hdrL->addWidget(back); hdrL->addWidget(titleLbl, 1);
    lay->addWidget(hdrW);
    auto* divLine = new QFrame(w); divLine->setObjectName("pageDivider");
    divLine->setFrameShape(QFrame::HLine); lay->addWidget(divLine);

    auto mkSpin = [](double min, double max, double val, int dec, const QString& suffix) -> QDoubleSpinBox* {
        auto* s = new QDoubleSpinBox;
        s->setRange(min, max); s->setValue(val); s->setDecimals(dec);
        s->setSuffix(suffix); s->setFixedWidth(140);
        return s;
    };

    /* ── Dati anagrafici + C.F. calcolato ── */
    auto* anagrGroup = new QGroupBox("\xf0\x9f\x91\xa4  Dati personali");
    auto* anagrVLay  = new QVBoxLayout(anagrGroup);
    anagrVLay->setSpacing(6);

    /* Riga 1: Nome | Cognome */
    auto* ar1 = new QWidget; auto* arl1 = new QHBoxLayout(ar1);
    arl1->setContentsMargins(0,0,0,0); arl1->setSpacing(8);
    auto* nomeEdit = new QLineEdit; nomeEdit->setPlaceholderText("Nome");
    auto* cognEdit = new QLineEdit; cognEdit->setPlaceholderText("Cognome");
    arl1->addWidget(new QLabel("Nome:"));    arl1->addWidget(nomeEdit, 2);
    arl1->addWidget(new QLabel("Cognome:")); arl1->addWidget(cognEdit, 2);
    anagrVLay->addWidget(ar1);

    /* Riga 2: Data nascita | Sesso | Comune | Belfiore */
    auto* ar2 = new QWidget; auto* arl2 = new QHBoxLayout(ar2);
    arl2->setContentsMargins(0,0,0,0); arl2->setSpacing(8);
    auto* dataNascitaEdit = new QDateEdit(QDate(1980, 1, 1));
    dataNascitaEdit->setDisplayFormat("dd/MM/yyyy");
    dataNascitaEdit->setCalendarPopup(true);
    dataNascitaEdit->setMaximumWidth(130);
    auto* sessoCombo = new QComboBox;
    sessoCombo->addItem("M \xe2\x80\x94 Maschio", false);
    sessoCombo->addItem("F \xe2\x80\x94 Femmina", true);
    sessoCombo->setFixedWidth(130);
    auto* comuneEdit  = new QLineEdit; comuneEdit->setPlaceholderText("Comune di nascita");
    auto* belfioreEdit = new QLineEdit; belfioreEdit->setPlaceholderText("Belfiore");
    belfioreEdit->setMaximumWidth(70);
    belfioreEdit->setToolTip("Codice Belfiore (4 char) — compilato automaticamente dal comune");
    arl2->addWidget(new QLabel("Nascita:")); arl2->addWidget(dataNascitaEdit);
    arl2->addWidget(sessoCombo);
    arl2->addWidget(new QLabel("Comune:")); arl2->addWidget(comuneEdit, 2);
    arl2->addWidget(belfioreEdit);
    anagrVLay->addWidget(ar2);

    /* Riga 3: C.F. calcolato (read-only) */
    auto* ar3 = new QWidget; auto* arl3 = new QHBoxLayout(ar3);
    arl3->setContentsMargins(0,0,0,0); arl3->setSpacing(8);
    auto* cfEdit = new QLineEdit;
    cfEdit->setPlaceholderText("Codice Fiscale calcolato automaticamente");
    cfEdit->setReadOnly(true);
    cfEdit->setStyleSheet("QLineEdit { font-family: monospace; letter-spacing: 1px; }");
    cfEdit->setMaximumWidth(200);
    auto* cfEditBtn = new QPushButton("\xf0\x9f\x94\x93");  /* 🔓 */
    cfEditBtn->setToolTip("Sblocca per modifica manuale");
    cfEditBtn->setCheckable(true);
    cfEditBtn->setFixedWidth(32);
    auto* cfStatusLbl = new QLabel;
    cfStatusLbl->setStyleSheet("font-size:11px;");
    arl3->addWidget(new QLabel("C.F.:"));
    arl3->addWidget(cfEdit, 1);
    arl3->addWidget(cfEditBtn);
    arl3->addWidget(cfStatusLbl, 1);
    anagrVLay->addWidget(ar3);

    /* Aggiornamento automatico C.F. */
    auto aggiornaCF = [=]{
        if (cfEditBtn->isChecked()) return;   /* modalità override manuale */
        const QString belf = belfioreEdit->text().trimmed().toUpper();
        const bool    masc = !sessoCombo->currentData().toBool();
        const QString cf   = calcolaCodiceFiscale(
            cognEdit->text(), nomeEdit->text(),
            dataNascitaEdit->date(), masc, belf);
        if (cf.isEmpty()) {
            cfEdit->clear();
            cfStatusLbl->setText(
                belf.length() == 4
                ? "<span style='color:#E5C400;'>\xe2\x9a\xa0 Dati incompleti</span>"
                : "<span style='color:gray;'>inserisci comune o codice Belfiore</span>");
        } else {
            cfEdit->setText(cf);
            cfStatusLbl->setText("<span style='color:#4caf50;'>\xe2\x9c\x94 calcolato</span>");
        }
    };

    /* Auto-lookup Belfiore quando il comune cambia */
    QObject::connect(comuneEdit, &QLineEdit::textEdited, anagrGroup, [=](const QString& txt){
        const QString belf = cercaBelfiore(txt);
        if (!belf.isEmpty()) {
            belfioreEdit->setText(belf);
        } else if (txt.trimmed().length() >= 3) {
            belfioreEdit->clear();
        }
        aggiornaCF();
    });

    /* Ricalcola CF al cambio degli altri campi */
    QObject::connect(nomeEdit,        &QLineEdit::textEdited,  anagrGroup, [=]{ aggiornaCF(); });
    QObject::connect(cognEdit,        &QLineEdit::textEdited,  anagrGroup, [=]{ aggiornaCF(); });
    QObject::connect(belfioreEdit,    &QLineEdit::textEdited,  anagrGroup, [=]{
        belfioreEdit->setText(belfioreEdit->text().toUpper().left(4));
        aggiornaCF();
    });
    QObject::connect(dataNascitaEdit, &QDateEdit::dateChanged, anagrGroup, [=]{ aggiornaCF(); });
    QObject::connect(sessoCombo, QOverload<int>::of(&QComboBox::currentIndexChanged),
                     anagrGroup, [=]{ aggiornaCF(); });

    /* Sblocco manuale */
    QObject::connect(cfEditBtn, &QPushButton::toggled, anagrGroup, [=](bool locked){
        cfEdit->setReadOnly(!locked);
        cfEditBtn->setToolTip(locked
            ? "Ripristina calcolo automatico"
            : "Sblocca per modifica manuale");
        if (!locked) aggiornaCF();
    });

    lay->addWidget(anagrGroup);

    /* ── Dati lavorativi ── */
    auto* lavGroup = new QGroupBox("\xf0\x9f\x8f\xa2  Rapporto di lavoro");
    auto* lavLay = new QVBoxLayout(lavGroup);
    lavLay->setSpacing(6);
    auto* lavRow1 = new QWidget; auto* lavLay1 = new QHBoxLayout(lavRow1);
    lavLay1->setContentsMargins(0,0,0,0); lavLay1->setSpacing(8);
    auto* datoreEdit = new QLineEdit; datoreEdit->setPlaceholderText("Datore di lavoro / Azienda");
    auto* ccnlEdit   = new QLineEdit; ccnlEdit->setPlaceholderText("CCNL / Tipo contratto");
    lavLay1->addWidget(new QLabel("Datore:")); lavLay1->addWidget(datoreEdit, 2);
    lavLay1->addWidget(new QLabel("CCNL:")); lavLay1->addWidget(ccnlEdit, 2);
    auto* lavRow2 = new QWidget; auto* lavLay2 = new QHBoxLayout(lavRow2);
    lavLay2->setContentsMargins(0,0,0,0); lavLay2->setSpacing(8);
    auto* dataIniEdit  = new QDateEdit(QDate::currentDate().addYears(-10));
    dataIniEdit->setDisplayFormat("dd/MM/yyyy"); dataIniEdit->setCalendarPopup(true);
    auto* dataFineEdit = new QDateEdit(QDate::currentDate());
    dataFineEdit->setDisplayFormat("dd/MM/yyyy"); dataFineEdit->setCalendarPopup(true);
    auto* inCorsoChk = new QCheckBox("Ancora in corso");
    lavLay2->addWidget(new QLabel("Data assunzione:")); lavLay2->addWidget(dataIniEdit);
    lavLay2->addWidget(new QLabel("Data cessazione:")); lavLay2->addWidget(dataFineEdit);
    lavLay2->addWidget(inCorsoChk); lavLay2->addStretch(1);
    lavLay->addWidget(lavRow1);
    lavLay->addWidget(lavRow2);
    lay->addWidget(lavGroup);

    /* ── Parametri calcolo ── */
    auto* paramGroup = new QGroupBox("\xe2\x9a\x99  Parametri calcolo");
    auto* paramLay = new QHBoxLayout(paramGroup);
    paramLay->setSpacing(8);
    auto* tfrStipSpin = mkSpin(5000, 500000, 30000, 0, " \xe2\x82\xac/anno");
    auto* tfrInflSpin = mkSpin(0, 15, 2.0, 1, " %/anno");
    auto* destCombo   = new QComboBox;
    destCombo->addItem("In azienda (art. 2120 c.c.)");
    destCombo->addItem("Fondo pensione complementare");
    paramLay->addWidget(new QLabel("Stipendio lordo:")); paramLay->addWidget(tfrStipSpin);
    paramLay->addWidget(new QLabel("Inflazione ISTAT:")); paramLay->addWidget(tfrInflSpin);
    paramLay->addWidget(new QLabel("Destinazione:")); paramLay->addWidget(destCombo, 1);
    lay->addWidget(paramGroup);

    /* ── Bottoni ── */
    auto* btnRow = new QWidget;
    auto* btnLay = new QHBoxLayout(btnRow);
    btnLay->setContentsMargins(0,0,0,0); btnLay->setSpacing(8);
    auto* calcBtn    = new QPushButton("\xf0\x9f\x94\xa2  Calcola TFR"); calcBtn->setObjectName("actionBtn");
    auto* ragBtn     = new QPushButton("\xe2\x9a\xa1  Compila da RAG"); ragBtn->setObjectName("actionBtn");
    auto* stopRagBtn = new QPushButton("\xe2\x8f\xb9"); stopRagBtn->setObjectName("actionBtn");
    stopRagBtn->setProperty("danger", true); stopRagBtn->setFixedWidth(40); stopRagBtn->setEnabled(false);
    auto* copyBtn    = new QPushButton("\xf0\x9f\x93\x8b  Copia");
    auto* waitLbl    = new QLabel("\xe2\x8f\xb3  Lettura RAG...");
    waitLbl->setStyleSheet("color:#E5C400;font-style:italic;"); waitLbl->setVisible(false);
    btnLay->addWidget(calcBtn); btnLay->addWidget(ragBtn);
    btnLay->addWidget(stopRagBtn); btnLay->addWidget(waitLbl);
    btnLay->addStretch(1); btnLay->addWidget(copyBtn);
    lay->addWidget(btnRow);

    /* ── Output ── */
    auto* tfrOut = new QTextBrowser(w);
    tfrOut->setObjectName("chatLog");
    tfrOut->setPlaceholderText(
        "Compila i campi sopra e premi \xe2\x80\x9c" "Calcola TFR\xe2\x80\x9d.\n\n"
        "Oppure premi \xe2\x80\x9c" "Compila da RAG\xe2\x80\x9d per estrarre automaticamente i dati "
        "dalla tua Knowledge Base (Impostazioni \xe2\x86\x92 Memoria).");
    lay->addWidget(tfrOut, 1);

    /* ── Grafico ── */
    auto* tfrChart = new FinChart(w);
    lay->addWidget(tfrChart);

    /* — inCorsoChk — */
    QObject::connect(inCorsoChk, &QCheckBox::toggled, w, [=](bool on){
        dataFineEdit->setEnabled(!on);
    });

    /* — Calcola TFR — */
    QObject::connect(calcBtn, &QPushButton::clicked, w, [=]{
        const QDate ini  = dataIniEdit->date();
        const QDate fine = inCorsoChk->isChecked() ? QDate::currentDate() : dataFineEdit->date();
        const int anni   = qMax(1, static_cast<int>(ini.daysTo(fine) / 365.25));

        const double stip       = tfrStipSpin->value();
        const double infl       = tfrInflSpin->value() / 100.0;
        const double quotaAnnua = stip / 13.5;
        const double tassoRival = 0.015 + 0.75 * infl;
        double fondo = 0.0;
        QVector<QPointF> pts;
        pts.append({ double(baseYear), 0.0 });
        for (int yr = 1; yr <= anni; ++yr) {
            fondo += quotaAnnua;
            fondo *= (1.0 + tassoRival);
            pts.append({ double(baseYear + yr), fondo });
        }
        const double tfrSemplice = quotaAnnua * anni;
        tfrChart->setData("\xf0\x9f\x92\xbc TFR rivalutato", pts, QColor("#c060e0"));

        const QString nome = (nomeEdit->text() + " " + cognEdit->text()).trimmed();
        const bool fondoPensione = (destCombo->currentIndex() == 1);

        QString hdrHtml;
        if (!nome.isEmpty())
            hdrHtml += "<b>Beneficiario:</b> " + nome.toHtmlEscaped() +
                       (cfEdit->text().isEmpty() ? "" : " &mdash; C.F.: " + cfEdit->text().toHtmlEscaped()) + "<br>";
        if (!datoreEdit->text().isEmpty())
            hdrHtml += "<b>Datore:</b> " + datoreEdit->text().toHtmlEscaped() +
                       (ccnlEdit->text().isEmpty() ? "" : " &mdash; CCNL: " + ccnlEdit->text().toHtmlEscaped()) + "<br>";
        hdrHtml += QString("<b>Periodo:</b> %1 &rarr; %2 (%3 anni)")
                   .arg(ini.toString("dd/MM/yyyy"))
                   .arg(inCorsoChk->isChecked() ? QString("in corso") : fine.toString("dd/MM/yyyy"))
                   .arg(anni);

        const QString fiscHtml = fondoPensione
            ? QString("Destinazione: <b>Fondo Pensione</b> &mdash; imposta sostitutiva 15%"
                      " (riduzione 0,3%/anno oltre il 15&deg;, min 9%).<br>"
                      "TFR netto stimato: <b>%1 \xe2\x82\xac</b>").arg(fondo * 0.88, 0,'f',2)
            : QString("Destinazione: <b>In azienda</b> &mdash; tassazione separata art. 17 TUIR.<br>"
                      "Aliquota stimata 23% &mdash; TFR netto indicativo: <b>%1 \xe2\x82\xac</b>").arg(fondo * 0.77, 0,'f',2);

        tfrOut->setHtml(QString(
            "<b>\xf0\x9f\x92\xbc SCHEDA TFR &mdash; Trattamento di Fine Rapporto</b><br><br>"
            "%1<br><br>"
            "<b>Parametri:</b><br>"
            "Stipendio lordo annuo: %2 \xe2\x82\xac &mdash; Inflazione: %3%<br>"
            "Quota annua (stip &divide; 13,5): <b>%4 \xe2\x82\xac</b><br>"
            "Tasso rivalutazione (1,5% + 75%&times;inf.): <b>%5%</b><br><br>"
            "<b>Risultato:</b><br>"
            "TFR senza rivalutazione: %6 \xe2\x82\xac<br>"
            "TFR rivalutato stimato: <b>%7 \xe2\x82\xac</b><br>"
            "Plusvalenza: %8 \xe2\x82\xac<br><br>"
            "<b>Fiscalit\xc3\xa0:</b><br>%9<br><br>"
            "<i>Calcolo indicativo (art. 2120 c.c.).</i>")
            .arg(hdrHtml)
            .arg(stip,0,'f',0).arg(tfrInflSpin->value(),0,'f',1)
            .arg(quotaAnnua,0,'f',2)
            .arg(tassoRival*100,0,'f',2)
            .arg(tfrSemplice,0,'f',2)
            .arg(fondo,0,'f',2)
            .arg(fondo - tfrSemplice,0,'f',2)
            .arg(fiscHtml));
    });

    /* — Copia — */
    QObject::connect(copyBtn, &QPushButton::clicked, w, [=]{
        QGuiApplication::clipboard()->setText(tfrOut->toPlainText());
    });

    /* — Compila da RAG — */
    auto tfrActive = std::make_shared<bool>(false);
    auto tfrBuf    = std::make_shared<QString>();

    QObject::connect(ragBtn, &QPushButton::clicked, w, [=]{
        const QString knowledge = P::readUserKnowledge();
        const QString sys =
            "Sei un assistente che estrae dati anagrafici e lavorativi per il calcolo del TFR. "
            "Analizza il testo e restituisci SOLO un oggetto JSON con queste chiavi esatte: "
            "nome, cognome, data_nascita (YYYY-MM-DD), sesso (M o F), "
            "comune_nascita (nome del comune italiano o stato estero), "
            "codice_belfiore (4 char, es. H501 per Roma — solo se presente esplicitamente nel testo), "
            "datore, ccnl, data_inizio (YYYY-MM-DD), "
            "data_fine (YYYY-MM-DD o in_corso), stipendio_lordo_annuo (intero in euro). "
            "Valori mancanti: stringa vuota o 0. Solo JSON valido, nient'altro.";
        ragBtn->setEnabled(false); stopRagBtn->setEnabled(true); waitLbl->setVisible(true);
        *tfrActive = true; tfrBuf->clear();
        ai->chat(sys, knowledge.isEmpty() ? "Nessun documento in Knowledge Base." : knowledge);
    });

    QObject::connect(stopRagBtn, &QPushButton::clicked, ai, &AiClient::abort);

    QObject::connect(ai, &AiClient::token, w, [=](const QString& t){
        if (!*tfrActive) return;
        tfrBuf->append(t);
    });

    QObject::connect(ai, &AiClient::finished, w, [=](const QString&){
        if (!*tfrActive) return;
        *tfrActive = false;
        ragBtn->setEnabled(true); stopRagBtn->setEnabled(false); waitLbl->setVisible(false);

        QString raw = *tfrBuf;
        const int js = raw.indexOf('{'), je = raw.lastIndexOf('}');
        if (js >= 0 && je > js) raw = raw.mid(js, je - js + 1);
        QJsonParseError jerr;
        const QJsonDocument doc = QJsonDocument::fromJson(raw.toUtf8(), &jerr);
        if (!doc.isObject()) {
            tfrOut->setHtml(
                "\xe2\x9a\xa0 JSON non ricevuto. Aggiungi dati lavorativi in Impostazioni &rarr; Memoria e riprova.<br>"
                "<small><i>" + tfrBuf->left(300).toHtmlEscaped() + "</i></small>");
            return;
        }
        const QJsonObject obj = doc.object();
        /* Dati anagrafici */
        if (!obj["nome"].toString().isEmpty())     nomeEdit->setText(obj["nome"].toString());
        if (!obj["cognome"].toString().isEmpty())   cognEdit->setText(obj["cognome"].toString());
        const QDate dn = QDate::fromString(obj["data_nascita"].toString(), "yyyy-MM-dd");
        if (dn.isValid()) dataNascitaEdit->setDate(dn);
        const QString sesso = obj["sesso"].toString().toUpper();
        if (sesso == "F") sessoCombo->setCurrentIndex(1);
        else if (sesso == "M") sessoCombo->setCurrentIndex(0);
        if (!obj["comune_nascita"].toString().isEmpty()) {
            comuneEdit->setText(obj["comune_nascita"].toString());
            /* auto-lookup Belfiore dal comune */
            const QString belf = cercaBelfiore(obj["comune_nascita"].toString());
            if (!belf.isEmpty()) belfioreEdit->setText(belf);
        }
        if (!obj["codice_belfiore"].toString().isEmpty())
            belfioreEdit->setText(obj["codice_belfiore"].toString().toUpper().left(4));
        /* Dati lavorativi */
        if (!obj["datore"].toString().isEmpty())    datoreEdit->setText(obj["datore"].toString());
        if (!obj["ccnl"].toString().isEmpty())      ccnlEdit->setText(obj["ccnl"].toString());
        const QDate di = QDate::fromString(obj["data_inizio"].toString(), "yyyy-MM-dd");
        if (di.isValid()) dataIniEdit->setDate(di);
        const QString df = obj["data_fine"].toString();
        if (df == "in_corso" || df.isEmpty()) {
            inCorsoChk->setChecked(true);
        } else {
            const QDate d = QDate::fromString(df, "yyyy-MM-dd");
            if (d.isValid()) { inCorsoChk->setChecked(false); dataFineEdit->setDate(d); }
        }
        const int stip = obj["stipendio_lordo_annuo"].toInt();
        if (stip > 0) tfrStipSpin->setValue(stip);
        /* Ricalcola C.F. con i nuovi dati */
        aggiornaCF();
        tfrOut->setHtml("\xe2\x9c\x85 Dati compilati da RAG. Verifica e premi <b>Calcola TFR</b>.");
    });

    QObject::connect(ai, &AiClient::error, w, [=](const QString& err){
        if (!*tfrActive) return;
        *tfrActive = false;
        ragBtn->setEnabled(true); stopRagBtn->setEnabled(false); waitLbl->setVisible(false);
        tfrOut->setHtml("\xe2\x9d\x8c Errore: " + err.toHtmlEscaped());
    });

    QObject::connect(ai, &AiClient::aborted, w, [=]{
        if (!*tfrActive) return;
        *tfrActive = false;
        ragBtn->setEnabled(true); stopRagBtn->setEnabled(false); waitLbl->setVisible(false);
        tfrOut->setHtml("\xe2\x8f\xb9 Interrotto.");
    });

    return w;
}

/* ══════════════════════════════════════════════════════════════
   Costruttore PraticoPage
   ══════════════════════════════════════════════════════════════ */
PraticoPage::PraticoPage(AiClient* ai, QWidget* parent)
    : QWidget(parent), m_ai(ai)
{
    auto* lay = new QVBoxLayout(this);
    lay->setContentsMargins(0,0,0,0);

    m_inner = new QStackedWidget(this);

    /* 0 = menu */
    m_inner->addWidget(buildMenu());

    /* 1 = 730 */
    m_inner->addWidget(buildChat(
        "\xf0\x9f\x93\x84  Assistente 730",
        "Sei un esperto fiscalista italiano specializzato nel modello 730. "
        "Fornisci guide chiare, cita gli articoli di legge quando rilevante. "
        "Rispondi SEMPRE e SOLO in italiano.",
        "Es: Posso detrarre le spese mediche per mio figlio?\n"
        "Es: Qual \xc3\xa8 la differenza tra 730 precompilato e ordinario?"));

    /* 2 = P.IVA */
    m_inner->addWidget(buildChat(
        "\xf0\x9f\x92\xbc  Partita IVA \xe2\x80\x94 Regime Forfettario",
        "Sei un commercialista italiano esperto in regime forfettario e partita IVA. "
        "Fornisci calcoli precisi e consigli pratici. "
        "Rispondi SEMPRE e SOLO in italiano.",
        "Es: Come calcolo le tasse nel regime forfettario al 15%?\n"
        "Es: Quali sono i limiti di fatturato per restare nel forfettario?"));

    /* 3 = Finanza */
    m_inner->addWidget(buildFinanza(m_inner, m_ai));

    /* 4 = Scheda TFR */
    m_inner->addWidget(buildSchedaTFR(m_inner, m_ai));

    lay->addWidget(m_inner);
}

void PraticoPage::onBackToMenu()
{
    if (m_inner) m_inner->setCurrentIndex(0);
}

void PraticoPage::onMenuCardClicked()
{
    auto* btn = qobject_cast<QPushButton*>(sender());
    if (!btn || !m_inner) return;
    m_inner->setCurrentIndex(btn->property("pageIndex").toInt());
}
