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
#include <memory>
#include <cmath>
/* ══════════════════════════════════════════════════════════════
   Chat panel riutilizzabile
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
    auto* back = new QPushButton("← Torna", hdr);
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

    /* ── Context menu: copia / leggi ── */
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

    /* Indicatore elaborazione */
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

    /* Flag per ignorare segnali non destinati a questo panel */
    auto active = std::make_shared<bool>(false);

    /* Connessioni */
    connect(back, &QPushButton::clicked, this, [this]{ m_inner->setCurrentIndex(0); });
    connect(stop, &QPushButton::clicked, m_ai, &AiClient::abort);

    auto sendFn = [=]{
        QString msg = inp->text().trimmed();
        if (msg.isEmpty()) return;
        log->append(QString("\n👤  Tu: %1\n").arg(msg));
        log->append("🤖  AI: ");
        inp->clear();
        send->setEnabled(false); stop->setEnabled(true); waitLbl->setVisible(true);
        *active = true;
        m_ai->chat(sysPrompt, msg);
    };

    connect(send, &QPushButton::clicked, this, sendFn);
    connect(inp,  &QLineEdit::returnPressed, this, sendFn);

    connect(m_ai, &AiClient::token, this, [=](const QString& t){
        if (!*active) return;
        QTextCursor c(log->document()); c.movePosition(QTextCursor::End);
        c.insertText(t); log->ensureCursorVisible();
    });
    connect(m_ai, &AiClient::finished, this, [=](const QString&){
        if (!*active) return;
        *active = false;
        log->append("\n──────────");
        send->setEnabled(true); stop->setEnabled(false); waitLbl->setVisible(false);
    });
    connect(m_ai, &AiClient::error, this, [=](const QString& err){
        if (!*active) return;
        *active = false;
        /* Categorie errore: abort silenzioso, RAM, rete, timeout, generico */
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
    connect(m_ai, &AiClient::aborted, this, [=]{
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
        {"📄", "Assistente 730",
         "Guida alla compilazione del 730, detrazioni, rimborsi fiscali.", 1},
        {"💼", "Partita IVA",
         "Regime forfettario, calcolo imposte, obblighi contributivi.", 2},
        {"🔍", "Cerca Lavoro",
         "Ricerca offerte, consigli CV, preparazione colloquio.", 3},
        {"\xf0\x9f\x92\xb0", "Calcolatore Finanza",
         "Mutuo, PAC, stima pensione INPS — calcoli locali istantanei.", 4},
    };

    auto* grid = new QWidget(w);
    auto* gLay = new QVBoxLayout(grid);
    gLay->setSpacing(10);
    gLay->setContentsMargins(0,0,0,0);

    for (auto& it : items) {
        /* Card con bottone "Apri →" integrato */
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

        int pg = it.page;
        auto* goBtn = new QPushButton("Apri →", card);
        goBtn->setObjectName("actionBtn"); goBtn->setFixedWidth(80);
        connect(goBtn, &QPushButton::clicked, this, [this, pg]{ m_inner->setCurrentIndex(pg); });

        cl->addWidget(ico); cl->addWidget(txt, 1); cl->addWidget(goBtn);
        gLay->addWidget(card);
    }

    gLay->addStretch(1);
    lay->addWidget(grid, 1);
    return w;
}

/* ══════════════════════════════════════════════════════════════
   buildFinanza — Calcolatori finanziari locali (0 token AI)
   ══════════════════════════════════════════════════════════════ */
static QWidget* buildFinanza(QStackedWidget* inner) {
    auto* w   = new QWidget;
    auto* lay = new QVBoxLayout(w);
    lay->setContentsMargins(20, 16, 20, 16);
    lay->setSpacing(14);

    /* Header con pulsante torna */
    auto* hdrW = new QWidget(w);
    auto* hdrL = new QHBoxLayout(hdrW);
    hdrL->setContentsMargins(0,0,0,0);
    auto* back = new QPushButton("\xe2\x86\x90 Torna", hdrW);
    back->setObjectName("actionBtn");
    QObject::connect(back, &QPushButton::clicked, hdrW, [inner]{ inner->setCurrentIndex(0); });
    auto* titleLbl = new QLabel("\xf0\x9f\x92\xb0  Calcolatori Finanziari", hdrW);
    titleLbl->setObjectName("pageTitle");
    hdrL->addWidget(back); hdrL->addWidget(titleLbl, 1);
    lay->addWidget(hdrW);

    auto* div = new QFrame(w); div->setObjectName("pageDivider");
    div->setFrameShape(QFrame::HLine); lay->addWidget(div);

    /* ── Area output comune ── */
    auto* outLbl = new QTextBrowser(w);
    outLbl->setObjectName("chatLog");
    outLbl->setPlaceholderText("I risultati dei calcoli appariranno qui...");
    outLbl->setMinimumHeight(140);

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

    auto* capSpin  = mkSpin(1000, 2000000, 150000, 0, " \xe2\x82\xac");
    auto* tassoSpin= mkSpin(0.01, 30.0,    3.5,    2, " %");
    auto* anniSpin = new QSpinBox; anniSpin->setRange(1, 40); anniSpin->setValue(20);
    anniSpin->setSuffix(" anni"); anniSpin->setFixedWidth(110);
    auto* calcMut  = new QPushButton("Calcola", mutGroup);
    calcMut->setObjectName("actionBtn");

    mutLay->addWidget(new QLabel("Capitale:")); mutLay->addWidget(capSpin);
    mutLay->addWidget(new QLabel("Tasso annuo:")); mutLay->addWidget(tassoSpin);
    mutLay->addWidget(new QLabel("Durata:")); mutLay->addWidget(anniSpin);
    mutLay->addWidget(calcMut); mutLay->addStretch(1);

    QObject::connect(calcMut, &QPushButton::clicked, w, [=]{
        double C = capSpin->value();
        double tasso = tassoSpin->value() / 12.0 / 100.0;
        int    n     = anniSpin->value() * 12;
        double rata;
        if (tasso == 0) rata = C / n;
        else            rata = C * (tasso * std::pow(1+tasso, n)) / (std::pow(1+tasso, n) - 1);
        double totPagato  = rata * n;
        double totInteressi = totPagato - C;

        QString txt = QString(
            "<b>\xf0\x9f\x8f\xa0 Mutuo: %1 \xe2\x82\xac — %2% — %3 anni</b><br>"
            "Rata mensile: <b>%4 \xe2\x82\xac</b><br>"
            "Totale pagato: <b>%5 \xe2\x82\xac</b><br>"
            "Totale interessi: <b>%6 \xe2\x82\xac</b><br><br>"
            "<b>Piano ammortamento (prime 12 rate):</b><br>")
            .arg(C, 0, 'f', 0).arg(tassoSpin->value(), 0, 'f', 2).arg(anniSpin->value())
            .arg(rata, 0, 'f', 2).arg(totPagato, 0, 'f', 2).arg(totInteressi, 0, 'f', 2);

        double debRes = C;
        for (int i = 1; i <= qMin(12, n); i++) {
            double interesse = debRes * tasso;
            double quota     = rata - interesse;
            debRes          -= quota;
            txt += QString("Rata %1: \xe2\x82\xac%2 (quota cap: %3 — int: %4 — residuo: %5)<br>")
                   .arg(i).arg(rata,0,'f',2).arg(quota,0,'f',2)
                   .arg(interesse,0,'f',2).arg(qMax(0.0, debRes),0,'f',2);
        }
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
        double versato  = rata * n;
        double rendimento = FV - versato;
        outLbl->setHtml(QString(
            "<b>\xf0\x9f\x93\x88 PAC: %1 \xe2\x82\xac/mese — %2% — %3 anni</b><br>"
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

    auto* stipSpin  = mkSpin(10000, 500000, 35000, 0, " \xe2\x82\xac/anno");
    auto* anniLav   = new QSpinBox; anniLav->setRange(1, 50); anniLav->setValue(35);
    anniLav->setSuffix(" anni"); anniLav->setFixedWidth(110);
    auto* aliqSpin  = mkSpin(1, 50, 33, 1, " %");
    auto* calcPen   = new QPushButton("Calcola", penGroup);
    calcPen->setObjectName("actionBtn");

    penLay->addWidget(new QLabel("Stipendio lordo:")); penLay->addWidget(stipSpin);
    penLay->addWidget(new QLabel("Anni lavoro:")); penLay->addWidget(anniLav);
    penLay->addWidget(new QLabel("Aliquota:")); penLay->addWidget(aliqSpin);
    penLay->addWidget(calcPen); penLay->addStretch(1);

    QObject::connect(calcPen, &QPushButton::clicked, w, [=]{
        double stip  = stipSpin->value();
        int    anni  = anniLav->value();
        double aliq  = aliqSpin->value() / 100.0;
        double contrib = stip * aliq * anni;
        /* Coefficiente per 67 anni: 5.723% (INPS 2024) */
        double pensAnnua  = contrib * 0.05723;
        double pensMensile = pensAnnua / 13.0;
        outLbl->setHtml(QString(
            "<b>\xf0\x9f\x91\xb4 Stima Pensione INPS (metodologia contributiva semplificata)</b><br>"
            "Stipendio lordo annuo: %1 \xe2\x82\xac — Anni: %2 — Aliquota: %3%<br>"
            "Contributi totali stimati: <b>%4 \xe2\x82\xac</b><br>"
            "Pensione annua stimata: <b>%5 \xe2\x82\xac</b><br>"
            "Pensione mensile stimata (13 mensilit\xc3\xa0): <b>%6 \xe2\x82\xac</b><br><br>"
            "<i>Stima indicativa — coeff. 5.723% per pensionamento a 67 anni (INPS 2024).</i>")
            .arg(stip,0,'f',0).arg(anni).arg(aliqSpin->value(),0,'f',1)
            .arg(contrib,0,'f',0).arg(pensAnnua,0,'f',0).arg(pensMensile,0,'f',0));
    });

    lay->addWidget(mutGroup);
    lay->addWidget(pacGroup);
    lay->addWidget(penGroup);
    lay->addWidget(outLbl, 1);
    return w;
}

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
        "📄  Assistente 730",
        "Sei un esperto fiscalista italiano specializzato nel modello 730. "
        "Fornisci guide chiare, cita gli articoli di legge quando rilevante. "
        "Rispondi SEMPRE e SOLO in italiano.",
        "Es: Posso detrarre le spese mediche per mio figlio?\n"
        "Es: Qual è la differenza tra 730 precompilato e ordinario?"));

    /* 2 = P.IVA */
    m_inner->addWidget(buildChat(
        "💼  Partita IVA — Regime Forfettario",
        "Sei un commercialista italiano esperto in regime forfettario e partita IVA. "
        "Fornisci calcoli precisi e consigli pratici. "
        "Rispondi SEMPRE e SOLO in italiano.",
        "Es: Come calcolo le tasse nel regime forfettario al 15%?\n"
        "Es: Quali sono i limiti di fatturato per restare nel forfettario?"));

    /* 3 = Cerca Lavoro */
    m_inner->addWidget(buildChat(
        "🔍  Cerca Lavoro",
        "Sei un career coach italiano esperto in ricerca del lavoro, CV e colloqui. "
        "Aiuta con strategie concrete e moderne. "
        "Rispondi SEMPRE e SOLO in italiano.",
        "Es: Come migliorare il mio CV per una posizione da sviluppatore?\n"
        "Es: Quali domande fanno di solito nei colloqui tecnici?"));

    /* 4 = Finanza */
    m_inner->addWidget(buildFinanza(m_inner));

    lay->addWidget(m_inner);
}
