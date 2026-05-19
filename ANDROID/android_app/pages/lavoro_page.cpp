#include "lavoro_page.h"
#include <QVBoxLayout>
#include <QHBoxLayout>
#include <QGridLayout>
#include <QLabel>
#include <QFont>
#include <QTextCursor>
#include <QScrollBar>
#include <QFileDialog>
#include <QFile>
#include <QFileInfo>
#include <QRegularExpression>
#include <zlib.h>

/* Decomprime un singolo stream FlateDecode (formato zlib). */
static QByteArray inflatePdfStream(const QByteArray& compressed)
{
    QByteArray out(compressed.size() * 6 + 4096, '\0');
    z_stream zs{};
    zs.next_in  = reinterpret_cast<Bytef*>(const_cast<char*>(compressed.constData()));
    zs.avail_in = static_cast<uInt>(compressed.size());
    if (inflateInit(&zs) != Z_OK) return {};
    int ret;
    do {
        if (static_cast<int>(zs.total_out) >= out.size())
            out.resize(out.size() * 2);
        zs.next_out  = reinterpret_cast<Bytef*>(out.data() + zs.total_out);
        zs.avail_out = static_cast<uInt>(out.size() - zs.total_out);
        ret = inflate(&zs, Z_SYNC_FLUSH);
    } while (ret == Z_OK);
    const uLong total = zs.total_out;
    inflateEnd(&zs);
    if (ret != Z_STREAM_END && ret != Z_BUF_ERROR) return {};
    out.resize(static_cast<int>(total));
    return out;
}

/* Estrae testo leggibile da un PDF.
   Prima prova gli operatori Tj/TJ su byte non compressi;
   poi decomprime gli stream FlateDecode e li analizza di nuovo. */
static QString extractPdfText(const QByteArray& raw)
{
    static const QRegularExpression reBT(
        "BT\\b(.*?)ET\\b",
        QRegularExpression::DotMatchesEverythingOption);
    static const QRegularExpression reTJarr(
        R"(\[((?:[^\[\]]*?\([^)]*\)[^\[\]]*?)*)\]\s*TJ)");
    static const QRegularExpression reStrArr(
        R"(\(([^)\\]*(?:\\.[^)\\]*)*)\))");
    static const QRegularExpression reTj(
        R"(\(([^)\\]*(?:\\.[^)\\]*)*)\)\s*(?:Tj|T['"]))");

    auto extractBTET = [&](const QString& d) -> QString {
        QString r;
        auto itBT = reBT.globalMatch(d);
        while (itBT.hasNext()) {
            const QString block = itBT.next().captured(1);
            bool foundArr = false;
            auto itArr = reTJarr.globalMatch(block);
            while (itArr.hasNext()) {
                foundArr = true;
                auto itS = reStrArr.globalMatch(itArr.next().captured(1));
                while (itS.hasNext()) r += itS.next().captured(1);
                r += '\n';
            }
            if (!foundArr) {
                auto itTj = reTj.globalMatch(block);
                while (itTj.hasNext()) r += itTj.next().captured(1) + '\n';
            }
        }
        return r;
    };

    // 1. Pass diretto su dati non compressi
    QString result = extractBTET(QString::fromLatin1(raw.constData(), raw.size()));

    // 2. Cerca e decomprime ogni stream FlateDecode
    int pos = 0;
    while (pos < raw.size()) {
        const int kw = raw.indexOf("stream", pos);
        if (kw == -1) break;
        int dataStart = kw + 6;
        if (dataStart >= raw.size()) break;
        if (raw[dataStart] == '\r') ++dataStart;
        if (dataStart >= raw.size() || raw[dataStart] != '\n') {
            pos = kw + 6;
            continue;
        }
        ++dataStart;
        const int endKw = raw.indexOf("endstream", dataStart);
        if (endKw == -1) break;
        int dataEnd = endKw;
        while (dataEnd > dataStart &&
               (raw[dataEnd - 1] == '\n' || raw[dataEnd - 1] == '\r'))
            --dataEnd;
        if (dataEnd > dataStart) {
            const QByteArray dec = inflatePdfStream(
                raw.mid(dataStart, dataEnd - dataStart));
            if (!dec.isEmpty())
                result += extractBTET(
                    QString::fromLatin1(dec.constData(), dec.size()));
        }
        pos = endKw + 9;
    }

    result.replace(QLatin1String("\\n"), "\n");
    result.replace(QLatin1String("\\r"), "\n");
    result.replace(QLatin1String("\\t"), " ");
    result.replace(QLatin1String("\\("), "(");
    result.replace(QLatin1String("\\)"), ")");
    result.replace(QLatin1String("\\\\"), "\\");

    QStringList lines;
    for (const QString& ln : result.split('\n'))
        if (!ln.trimmed().isEmpty()) lines << ln.trimmed();
    return lines.join('\n');
}

/* ══════════════════════════════════════════════════════════════
   LavoroPage — Assistente AI per il lavoro
   ══════════════════════════════════════════════════════════════ */
LavoroPage::LavoroPage(AiClient* ai, QWidget* parent)
    : QWidget(parent), m_ai(ai)
{
    auto* vbox = new QVBoxLayout(this);
    vbox->setContentsMargins(8, 8, 8, 8);
    vbox->setSpacing(8);

    auto* title = new QLabel(QString::fromUtf8("\xf0\x9f\x92\xbc Assistente Lavoro AI"), this);
    QFont tf = title->font();
    tf.setPointSize(14);
    tf.setBold(true);
    title->setFont(tf);
    title->setAlignment(Qt::AlignCenter);
    vbox->addWidget(title);

    auto* inputRow = new QHBoxLayout;
    auto* inputLbl = new QLabel("CV / offerta / domanda:", this);
    inputRow->addWidget(inputLbl, 1);
    m_btnPdf = new QPushButton(
        QString::fromUtf8("\xf0\x9f\x93\x8e Carica PDF"), this);
    m_btnPdf->setFixedWidth(114);
    m_btnPdf->setMinimumHeight(36);
    inputRow->addWidget(m_btnPdf);
    vbox->addLayout(inputRow);

    m_input = new QTextEdit(this);
    m_input->setPlaceholderText(
        "Incolla il CV, l'offerta di lavoro, le tue competenze...\n"
        "Poi premi una delle azioni qui sotto.");
    m_input->setFixedHeight(100);
    vbox->addWidget(m_input);

    /* Griglia azioni 2×3 */
    struct Action { const char* icon; const char* label; const char* sys; };
    static const Action kActions[] = {
        { "\xf0\x9f\x93\x84", "Analizza CV",
          "Sei un esperto recruiter HR. Analizza il CV fornito: evidenzia punti di forza, "
          "lacune, competenze chiave e suggerisci miglioramenti specifici. Rispondi in italiano." },
        { "\xe2\x9c\x89\xef\xb8\x8f", "Lettera presentazione",
          "Sei un esperto di ricerca lavoro. Scrivi una lettera di presentazione professionale "
          "e convincente basata sulle informazioni fornite. Sii formale e personalizzato. Rispondi in italiano." },
        { "\xf0\x9f\x8e\xa4", "Prepara colloquio",
          "Sei un coach per colloqui di lavoro. Elenca le domande probabili per questo ruolo, "
          "suggerisci le risposte migliori con esempi e dai consigli pratici. Rispondi in italiano." },
        { "\xf0\x9f\x94\x8d", "Cerca offerte",
          "Sei un consulente per la job search. Suggerisci le piattaforme migliori, parole chiave "
          "per la ricerca, settori adatti al profilo e strategie pratiche. Rispondi in italiano." },
        { "\xf0\x9f\x92\xb0", "Stima stipendio",
          "Sei un esperto del mercato del lavoro italiano. Stima la RAL (Reddito Annuo Lordo) "
          "realistica per il profilo/ruolo/settore fornito, spiegando i fattori che incidono. Rispondi in italiano." },
        { "\xf0\x9f\x8c\x90", "Traduci CV (EN)",
          "Sei un traduttore HR professionale. Traduci il CV in inglese professionale, "
          "adattando la terminologia alle convenzioni del CV anglosassone. Mantieni struttura chiara." },
    };

    auto* grid = new QGridLayout;
    grid->setSpacing(6);
    int row = 0, col = 0;
    for (const auto& a : kActions) {
        auto* btn = new QPushButton(
            QString::fromUtf8(a.icon) + " " + a.label, this);
        btn->setSizePolicy(QSizePolicy::Expanding, QSizePolicy::Fixed);
        btn->setMinimumHeight(44);
        btn->setProperty("sysPrompt", QString::fromUtf8(a.sys));
        connect(btn, &QPushButton::clicked, this, &LavoroPage::onActionBtnClicked);
        grid->addWidget(btn, row, col);
        if (++col == 2) { col = 0; ++row; }
    }
    vbox->addLayout(grid);

    /* Status + progress */
    auto* statRow = new QHBoxLayout;
    m_status = new QLabel("", this);
    m_status->setVisible(false);
    statRow->addWidget(m_status, 1);
    m_btnStop = new QPushButton(QString::fromUtf8("\xe2\x8f\xb9 Stop"), this);
    m_btnStop->setVisible(false);
    m_btnStop->setFixedWidth(72);
    statRow->addWidget(m_btnStop);
    vbox->addLayout(statRow);

    m_progress = new QProgressBar(this);
    m_progress->setRange(0, 0);
    m_progress->setVisible(false);
    m_progress->setFixedHeight(4);
    vbox->addWidget(m_progress);

    /* Output */
    m_output = new QTextEdit(this);
    m_output->setReadOnly(true);
    m_output->setPlaceholderText("La risposta AI apparirà qui...");
    vbox->addWidget(m_output);

    connect(m_btnPdf,  &QPushButton::clicked, this, &LavoroPage::onLoadPdf);
    connect(m_btnStop, &QPushButton::clicked, m_ai, &AiClient::abort);
    connect(m_ai, &AiClient::token,    this, &LavoroPage::onToken);
    connect(m_ai, &AiClient::finished, this, &LavoroPage::onFinished);
    connect(m_ai, &AiClient::error,    this, &LavoroPage::onError);
    connect(m_ai, &AiClient::aborted,  this, &LavoroPage::onAborted);
}

void LavoroPage::runAction(const QString& sys)
{
    if (m_busy || m_ai->busy()) {
        m_output->setPlainText(
            QString::fromUtf8("\xe2\x9a\xa0\xef\xb8\x8f")
            + " L'AI sta elaborando. Attendi il termine oppure premi Stop.");
        return;
    }
    const QString userMsg = m_input->toPlainText().trimmed();
    if (userMsg.isEmpty()) {
        m_output->setPlainText(
            QString::fromUtf8("\xe2\x9a\xa0\xef\xb8\x8f")
            + " Inserisci prima le informazioni nel campo di testo sopra.");
        return;
    }
    m_busy = true;
    m_output->clear();
    m_status->setText(QString::fromUtf8("\xe2\x8f\xb3") + " Elaborazione in corso...");
    m_status->setVisible(true);
    m_progress->setVisible(true);
    m_btnStop->setVisible(true);
    m_ai->chat(sys, userMsg);
}

void LavoroPage::onToken(const QString& t)
{
    if (!m_busy) return;
    m_output->moveCursor(QTextCursor::End);
    m_output->insertPlainText(t);
    m_output->verticalScrollBar()->setValue(m_output->verticalScrollBar()->maximum());
}

void LavoroPage::onFinished(const QString&)
{
    if (!m_busy) return;
    m_busy = false;
    m_status->setVisible(false);
    m_progress->setVisible(false);
    m_btnStop->setVisible(false);
}

void LavoroPage::onError(const QString& e)
{
    if (!m_busy) return;
    m_busy = false;
    m_status->setVisible(false);
    m_progress->setVisible(false);
    m_btnStop->setVisible(false);
    m_output->setPlainText(
        QString::fromUtf8("\xe2\x9d\x8c") + " Errore: " + e);
}

void LavoroPage::onAborted()
{
    onFinished("");
}

void LavoroPage::onActionBtnClicked()
{
    auto* btn = qobject_cast<QPushButton*>(sender());
    if (!btn) return;
    runAction(btn->property("sysPrompt").toString());
}

void LavoroPage::onLoadPdf()
{
    const QString path = QFileDialog::getOpenFileName(
        this, "Seleziona PDF", QString(),
        "PDF (*.pdf);;Tutti i file (*)");
    if (path.isEmpty()) return;

    QFile f(path);
    if (!f.open(QIODevice::ReadOnly)) {
        m_output->setPlainText(
            QString::fromUtf8("\xe2\x9d\x8c")
            + " Impossibile aprire il file: " + f.errorString());
        return;
    }
    const QByteArray raw = f.readAll();
    f.close();

    const QString text = extractPdfText(raw);
    if (text.length() < 40) {
        m_output->setPlainText(
            QString::fromUtf8("\xe2\x9a\xa0\xef\xb8\x8f")
            + " Testo estratto troppo breve. Il PDF è probabilmente compresso o "
              "basato su immagini scansionate. Copia e incolla il testo manualmente.");
        return;
    }
    m_input->setPlainText(text);
    m_output->setPlainText(
        QString::fromUtf8("\xe2\x9c\x85")
        + " PDF caricato: " + QFileInfo(path).fileName()
        + " (" + QString::number(text.length()) + " caratteri). "
          "Premi un'azione per elaborarlo.");
}
