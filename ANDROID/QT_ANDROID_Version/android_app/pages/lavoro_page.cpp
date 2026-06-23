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
#include <QPainter>
#include <QBitmap>
#include <QDialog>
#include <QDialogButtonBox>
#include <QFormLayout>
#include <QDateTime>
#include <QDir>
#include <QStandardPaths>
#include <QJsonDocument>
#include <QJsonObject>
#include <QMessageBox>
#include <QListWidgetItem>
#include <QScreen>
#include <zlib.h>

/* ══════════════════════════════════════════════════════════════
   Utility: estrazione testo PDF (FlateDecode)
   ══════════════════════════════════════════════════════════════ */
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

    QString result = extractBTET(QString::fromLatin1(raw.constData(), raw.size()));
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
   OffertaDialog
   ══════════════════════════════════════════════════════════════ */
OffertaDialog::OffertaDialog(QWidget* parent)
    : QDialog(parent, Qt::Dialog)
{
    setWindowTitle(QString::fromUtf8("\xe2\x9e\x95 Nuova offerta"));
    auto* vbox = new QVBoxLayout(this);
    vbox->setContentsMargins(16, 16, 16, 16);
    vbox->setSpacing(10);

    auto* form = new QFormLayout;
    form->setSpacing(8);

    m_aziendaEdit = new QLineEdit(this);
    m_aziendaEdit->setPlaceholderText("Es. Acme S.r.l.");
    m_aziendaEdit->setMinimumHeight(40);
    form->addRow("Azienda:", m_aziendaEdit);

    m_ruoloEdit = new QLineEdit(this);
    m_ruoloEdit->setPlaceholderText("Es. Sviluppatore Qt");
    m_ruoloEdit->setMinimumHeight(40);
    form->addRow("Ruolo:", m_ruoloEdit);

    m_statoCb = new QComboBox(this);
    m_statoCb->setMinimumHeight(40);
    m_statoCb->addItems({
        QString::fromUtf8("\xf0\x9f\x93\xa4 Inviata"),    /* 📤 */
        QString::fromUtf8("\xf0\x9f\x92\xac Colloquio"),  /* 💬 */
        QString::fromUtf8("\xe2\x9d\x8c Rifiutata"),      /* ❌ */
        QString::fromUtf8("\xf0\x9f\x8e\x89 Offerta")     /* 🎉 */
    });
    form->addRow("Stato:", m_statoCb);

    vbox->addLayout(form);

    auto* btnBox = new QDialogButtonBox(
        QDialogButtonBox::Ok | QDialogButtonBox::Cancel, this);
    btnBox->button(QDialogButtonBox::Ok)->setMinimumHeight(44);
    btnBox->button(QDialogButtonBox::Cancel)->setMinimumHeight(44);
    vbox->addWidget(btnBox);

    connect(btnBox, &QDialogButtonBox::accepted, this, &OffertaDialog::onConfirm);
    connect(btnBox, &QDialogButtonBox::rejected, this, &QDialog::reject);
}

void OffertaDialog::onConfirm()
{
    if (m_aziendaEdit->text().trimmed().isEmpty() ||
        m_ruoloEdit->text().trimmed().isEmpty()) {
        QMessageBox::warning(this, "Dati mancanti",
            "Inserisci almeno azienda e ruolo.");
        return;
    }
    accept();
}

QString OffertaDialog::azienda() const { return m_aziendaEdit->text().trimmed(); }
QString OffertaDialog::ruolo()   const { return m_ruoloEdit->text().trimmed(); }
QString OffertaDialog::stato()   const { return m_statoCb->currentText(); }

/* ══════════════════════════════════════════════════════════════
   LavoroPage — Costruttore
   ══════════════════════════════════════════════════════════════ */
LavoroPage::LavoroPage(AiClient* ai, QWidget* parent)
    : QWidget(parent), m_ai(ai)
{
    auto* rootVbox = new QVBoxLayout(this);
    rootVbox->setContentsMargins(0, 0, 0, 0);
    rootVbox->setSpacing(0);

    auto* tabs = new QTabWidget(this);
    tabs->setObjectName("LavoroTabs");
    rootVbox->addWidget(tabs);

    /* ════════════════════════════════
       TAB 0 — Assistente AI
       ════════════════════════════════ */
    auto* tabAI = new QWidget;
    {
        auto* vbox = new QVBoxLayout(tabAI);
        vbox->setContentsMargins(8, 8, 8, 8);
        vbox->setSpacing(8);

        /* Foto profilo CV (circolare) */
        {
            auto* fotoRow = new QHBoxLayout;
            m_fotoLbl = new QLabel(tabAI);
            m_fotoLbl->setFixedSize(52, 52);
            m_fotoLbl->setObjectName("FotoProfiloLbl");
            m_fotoLbl->setStyleSheet(
                "border:2px solid palette(mid); border-radius:26px;"
                "background:palette(base); font-size:22px;");
            m_fotoLbl->setAlignment(Qt::AlignCenter);
            m_fotoLbl->setText(QString::fromUtf8("\xf0\x9f\x91\xa4"));   /* 👤 */
            m_fotoBtn = new QPushButton(
                QString::fromUtf8("\xf0\x9f\x93\xb7") + " Foto", tabAI);  /* 📷 */
            m_fotoBtn->setFixedWidth(80);
            m_fotoBtn->setMinimumHeight(36);
            fotoRow->addWidget(m_fotoLbl);
            fotoRow->addWidget(m_fotoBtn);
            fotoRow->addStretch();
            vbox->addLayout(fotoRow);
        }

        auto* inputRow = new QHBoxLayout;
        auto* inputLbl = new QLabel("CV / offerta / domanda:", tabAI);
        inputRow->addWidget(inputLbl, 1);
        m_btnPdf = new QPushButton(
            QString::fromUtf8("\xf0\x9f\x93\x8e Carica PDF"), tabAI);
        m_btnPdf->setFixedWidth(114);
        m_btnPdf->setMinimumHeight(36);
        inputRow->addWidget(m_btnPdf);
        vbox->addLayout(inputRow);

        m_input = new QTextEdit(tabAI);
        m_input->setPlaceholderText(
            "Incolla il CV, l'offerta di lavoro, le tue competenze...\n"
            "Poi premi una delle azioni qui sotto.");
        m_input->setFixedHeight(90);
        vbox->addWidget(m_input);

        /* Griglia azioni 2×3 */
        struct Action { const char* icon; const char* label; const char* sys; };
        static const Action kActions[] = {
            { "\xf0\x9f\x93\x84", "Analizza CV",
              "Sei un esperto recruiter HR. Analizza il CV fornito: evidenzia punti di forza, "
              "lacune, competenze chiave e suggerisci miglioramenti specifici. Rispondi in italiano." },
            { "\xe2\x9c\x89\xef\xb8\x8f", "Lettera",
              "Sei un esperto di ricerca lavoro. Scrivi una lettera di presentazione professionale "
              "e convincente basata sulle informazioni fornite. Sii formale e personalizzato. Rispondi in italiano." },
            { "\xf0\x9f\x8e\xa4", "Colloquio",
              "Sei un coach per colloqui di lavoro. Elenca le domande probabili per questo ruolo, "
              "suggerisci le risposte migliori con esempi e dai consigli pratici. Rispondi in italiano." },
            { "\xf0\x9f\x94\x8d", "Cerca offerte",
              "Sei un consulente per la job search. Suggerisci le piattaforme migliori, parole chiave "
              "per la ricerca, settori adatti al profilo e strategie pratiche. Rispondi in italiano." },
            { "\xf0\x9f\x92\xb0", "Stipendio",
              "Sei un esperto del mercato del lavoro italiano. Stima la RAL realistica per il "
              "profilo/ruolo/settore fornito, spiegando i fattori che incidono. Rispondi in italiano." },
            { "\xf0\x9f\x8c\x90", "Traduci EN",
              "Sei un traduttore HR professionale. Traduci il CV in inglese professionale, "
              "adattando la terminologia alle convenzioni anglosassoni. Mantieni struttura chiara." },
        };

        auto* grid = new QGridLayout;
        grid->setSpacing(6);
        int row = 0, col = 0;
        for (const auto& a : kActions) {
            auto* btn = new QPushButton(
                QString::fromUtf8(a.icon) + " " + a.label, tabAI);
            btn->setSizePolicy(QSizePolicy::Expanding, QSizePolicy::Fixed);
            btn->setMinimumHeight(44);
            btn->setProperty("sysPrompt", QString::fromUtf8(a.sys));
            connect(btn, &QPushButton::clicked,
                    this, &LavoroPage::onActionBtnClicked);
            grid->addWidget(btn, row, col);
            if (++col == 2) { col = 0; ++row; }
        }
        vbox->addLayout(grid);

        /* Status + progress */
        auto* statRow = new QHBoxLayout;
        m_status = new QLabel("", tabAI);
        m_status->setVisible(false);
        statRow->addWidget(m_status, 1);
        m_btnStop = new QPushButton(
            QString::fromUtf8("\xe2\x8f\xb9 Stop"), tabAI);
        m_btnStop->setVisible(false);
        m_btnStop->setFixedWidth(72);
        statRow->addWidget(m_btnStop);
        vbox->addLayout(statRow);

        m_progress = new QProgressBar(tabAI);
        m_progress->setRange(0, 0);
        m_progress->setVisible(false);
        m_progress->setFixedHeight(4);
        vbox->addWidget(m_progress);

        m_output = new QTextEdit(tabAI);
        m_output->setReadOnly(true);
        m_output->setPlaceholderText("La risposta AI apparirà qui...");
        vbox->addWidget(m_output, 1);

        auto* emailBtn = new QPushButton(
            QString::fromUtf8("\xf0\x9f\x93\xa7") + "  Email candidatura", tabAI);  /* 📧 */
        emailBtn->setObjectName("SecondaryBtn");
        emailBtn->setMinimumHeight(44);
        vbox->addWidget(emailBtn);

        connect(m_btnPdf,  &QPushButton::clicked, this, &LavoroPage::onLoadPdf);
        connect(m_btnStop, &QPushButton::clicked, m_ai, &AiClient::abort);
        connect(m_fotoBtn, &QPushButton::clicked, this, &LavoroPage::onFotoBtnClicked);
        connect(emailBtn,  &QPushButton::clicked, this, &LavoroPage::onEmailBtnClicked);
    }
    tabs->addTab(tabAI,
        QString::fromUtf8("\xf0\x9f\xa4\x96") + " Assistente");  /* 🤖 */

    /* ════════════════════════════════
       TAB 1 — Tracker Offerte
       ════════════════════════════════ */
    auto* tabTracker = new QWidget;
    {
        auto* vbox = new QVBoxLayout(tabTracker);
        vbox->setContentsMargins(8, 8, 8, 8);
        vbox->setSpacing(8);

        auto* hdr = new QLabel(
            QString::fromUtf8("\xf0\x9f\x93\x8b") + " Tracker Offerte di Lavoro",
            tabTracker);  /* 📋 */
        {
            QFont f = hdr->font();
            f.setPointSize(13);
            f.setBold(true);
            hdr->setFont(f);
        }
        hdr->setAlignment(Qt::AlignCenter);
        vbox->addWidget(hdr);

        m_offerteList = new QListWidget(tabTracker);
        m_offerteList->setObjectName("OfferteList");
        m_offerteList->setSpacing(2);
        m_offerteList->setAlternatingRowColors(true);
        vbox->addWidget(m_offerteList, 1);

        auto* btnRow = new QHBoxLayout;
        m_addBtn = new QPushButton(
            QString::fromUtf8("\xe2\x9e\x95") + " Aggiungi", tabTracker);  /* ➕ */
        m_addBtn->setObjectName("PrimaryBtn");
        m_addBtn->setMinimumHeight(48);

        m_delBtn = new QPushButton(
            QString::fromUtf8("\xf0\x9f\x97\x91") + " Elimina", tabTracker);  /* 🗑 */
        m_delBtn->setObjectName("SecondaryBtn");
        m_delBtn->setMinimumHeight(48);
        m_delBtn->setEnabled(false);

        btnRow->addWidget(m_addBtn, 2);
        btnRow->addWidget(m_delBtn, 1);
        vbox->addLayout(btnRow);

        auto* hint = new QLabel(
            QString::fromUtf8("\xf0\x9f\x92\xa1")  /* 💡 */
            + " Tieni premuto un elemento per eliminarlo",
            tabTracker);
        hint->setAlignment(Qt::AlignCenter);
        {
            QFont f = hint->font();
            f.setPointSize(10);
            hint->setFont(f);
        }
        vbox->addWidget(hint);

        connect(m_addBtn, &QPushButton::clicked,
                this, &LavoroPage::onAddOfferta);
        connect(m_delBtn, &QPushButton::clicked,
                this, &LavoroPage::onDeleteOfferta);
        connect(m_offerteList, &QListWidget::itemActivated,
                this, &LavoroPage::onOffertaLongPressed);
        connect(m_offerteList, &QListWidget::itemSelectionChanged,
                this, [this]() {
                    m_delBtn->setEnabled(!m_offerteList->selectedItems().isEmpty());
                });
    }
    tabs->addTab(tabTracker,
        QString::fromUtf8("\xf0\x9f\x93\x8b") + " Offerte");  /* 📋 */

    /* ════════════════════════════════
       TAB 2 — Cover Letter
       ════════════════════════════════ */
    auto* tabCover = new QWidget;
    {
        auto* vbox = new QVBoxLayout(tabCover);
        vbox->setContentsMargins(8, 8, 8, 8);
        vbox->setSpacing(8);

        auto* hdr = new QLabel(
            QString::fromUtf8("\xe2\x9c\x89\xef\xb8\x8f") + " Cover Letter",
            tabCover);  /* ✉️ */
        {
            QFont f = hdr->font();
            f.setPointSize(13);
            f.setBold(true);
            hdr->setFont(f);
        }
        hdr->setAlignment(Qt::AlignCenter);
        vbox->addWidget(hdr);

        auto* contextLbl = new QLabel(
            "Contesto (ruolo, azienda, tue competenze):", tabCover);
        vbox->addWidget(contextLbl);

        m_coverEdit = new QTextEdit(tabCover);
        m_coverEdit->setPlaceholderText(
            "Descrivi il ruolo a cui ti candidi, l'azienda e le tue competenze chiave...");
        m_coverEdit->setFixedHeight(90);
        vbox->addWidget(m_coverEdit);

        auto* aiRow = new QHBoxLayout;
        m_coverAiBtn = new QPushButton(
            QString::fromUtf8("\xf0\x9f\xa4\x96") + " Genera da AI", tabCover);  /* 🤖 */
        m_coverAiBtn->setObjectName("PrimaryBtn");
        m_coverAiBtn->setMinimumHeight(44);
        aiRow->addWidget(m_coverAiBtn, 2);

        m_coverSaveBtn = new QPushButton(
            QString::fromUtf8("\xf0\x9f\x92\xbe") + " Salva .txt", tabCover);  /* 💾 */
        m_coverSaveBtn->setObjectName("SecondaryBtn");
        m_coverSaveBtn->setMinimumHeight(44);
        aiRow->addWidget(m_coverSaveBtn, 1);
        vbox->addLayout(aiRow);

        m_coverProgress = new QProgressBar(tabCover);
        m_coverProgress->setRange(0, 0);
        m_coverProgress->setVisible(false);
        m_coverProgress->setFixedHeight(4);
        vbox->addWidget(m_coverProgress);

        auto* outLbl = new QLabel("Cover Letter generata:", tabCover);
        vbox->addWidget(outLbl);

        m_coverOutput = new QTextEdit(tabCover);
        m_coverOutput->setPlaceholderText(
            "La cover letter apparirà qui. Puoi modificarla direttamente.");
        vbox->addWidget(m_coverOutput, 1);

        connect(m_coverAiBtn,  &QPushButton::clicked,
                this, &LavoroPage::onCoverAiClicked);
        connect(m_coverSaveBtn, &QPushButton::clicked,
                this, &LavoroPage::onCoverSaveClicked);
    }
    tabs->addTab(tabCover,
        QString::fromUtf8("\xe2\x9c\x89\xef\xb8\x8f") + " Cover Letter");  /* ✉️ */

    /* ── Connessioni AI globali ── */
    connect(m_ai, &AiClient::token,    this, &LavoroPage::onToken);
    connect(m_ai, &AiClient::finished, this, &LavoroPage::onFinished);
    connect(m_ai, &AiClient::error,    this, &LavoroPage::onError);
    connect(m_ai, &AiClient::aborted,  this, &LavoroPage::onAborted);

    /* Carica offerte salvate */
    loadOfferte();
}

/* ══════════════════════════════════════════════════════════════
   Helpers: path JSON, load/save offerte
   ══════════════════════════════════════════════════════════════ */
QString LavoroPage::offertePath()
{
    const QString dir =
        QStandardPaths::writableLocation(QStandardPaths::AppDataLocation);
    QDir().mkpath(dir);
    return dir + "/offerte.json";
}

void LavoroPage::loadOfferte()
{
    QFile f(offertePath());
    if (!f.open(QIODevice::ReadOnly)) return;
    const QJsonDocument doc = QJsonDocument::fromJson(f.readAll());
    f.close();
    if (doc.isArray()) m_offerte = doc.array();
    rebuildList();
}

void LavoroPage::saveOfferte()
{
    QFile f(offertePath());
    if (!f.open(QIODevice::WriteOnly | QIODevice::Truncate)) return;
    f.write(QJsonDocument(m_offerte).toJson(QJsonDocument::Compact));
}

void LavoroPage::rebuildList()
{
    if (!m_offerteList) return;
    m_offerteList->clear();
    for (int i = 0; i < m_offerte.size(); ++i) {
        const QJsonObject o = m_offerte[i].toObject();
        const QString azienda = o["azienda"].toString();
        const QString ruolo   = o["ruolo"].toString();
        const QString stato   = o["stato"].toString();
        const QString data    = o["data"].toString();
        const QString display =
            stato + "  " + azienda + " — " + ruolo + "\n" + data;
        auto* item = new QListWidgetItem(display, m_offerteList);
        item->setData(Qt::UserRole, i);
        item->setSizeHint(QSize(0, 60));
    }
    if (m_offerteList->count() == 0) {
        auto* placeholder = new QListWidgetItem(
            QString::fromUtf8(
                "\xf0\x9f\x93\x84")  /* 📄 */
            + "  Nessuna offerta salvata.\n"
              "  Premi \xe2\x9e\x95 per aggiungerne una.",
            m_offerteList);
        placeholder->setFlags(Qt::NoItemFlags);
    }
}

/* ══════════════════════════════════════════════════════════════
   Slot — Tab 0: Assistente AI
   ══════════════════════════════════════════════════════════════ */
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
    m_status->setText(
        QString::fromUtf8("\xe2\x8f\xb3") + " Elaborazione in corso...");
    m_status->setVisible(true);
    m_progress->setVisible(true);
    m_btnStop->setVisible(true);
    m_ai->chat(sys, userMsg);
}

void LavoroPage::onToken(const QString& t)
{
    if (m_busy) {
        m_output->moveCursor(QTextCursor::End);
        m_output->insertPlainText(t);
        m_output->verticalScrollBar()->setValue(
            m_output->verticalScrollBar()->maximum());
    } else if (m_coverBusy) {
        m_coverOutput->moveCursor(QTextCursor::End);
        m_coverOutput->insertPlainText(t);
        m_coverOutput->verticalScrollBar()->setValue(
            m_coverOutput->verticalScrollBar()->maximum());
    }
}

void LavoroPage::onFinished(const QString&)
{
    if (m_busy) {
        m_busy = false;
        m_status->setVisible(false);
        m_progress->setVisible(false);
        m_btnStop->setVisible(false);
    } else if (m_coverBusy) {
        m_coverBusy = false;
        m_coverProgress->setVisible(false);
        m_coverAiBtn->setEnabled(true);
    }
}

void LavoroPage::onError(const QString& e)
{
    if (m_busy) {
        m_busy = false;
        m_status->setVisible(false);
        m_progress->setVisible(false);
        m_btnStop->setVisible(false);
        m_output->setPlainText(
            QString::fromUtf8("\xe2\x9d\x8c") + " Errore: " + e);
    } else if (m_coverBusy) {
        m_coverBusy = false;
        m_coverProgress->setVisible(false);
        m_coverAiBtn->setEnabled(true);
        m_coverOutput->append(
            QString::fromUtf8("\xe2\x9d\x8c") + " Errore: " + e);
    }
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
            + " Testo estratto troppo breve. Il PDF e' probabilmente compresso o "
              "basato su immagini. Copia e incolla il testo manualmente.");
        return;
    }
    m_input->setPlainText(text);
    m_output->setPlainText(
        QString::fromUtf8("\xe2\x9c\x85")
        + " PDF caricato: " + QFileInfo(path).fileName()
        + " (" + QString::number(text.length()) + " caratteri). "
          "Premi un'azione per elaborarlo.");
}

/* ── B2: foto profilo CV circolare ── */
void LavoroPage::onFotoBtnClicked()
{
    const QString path = QFileDialog::getOpenFileName(
        this, "Foto profilo CV", QString(),
        "Immagini (*.png *.jpg *.jpeg *.bmp *.webp)");
    if (path.isEmpty()) return;

    QPixmap pm(path);
    if (pm.isNull()) return;

    /* Calcola dimensione in pixel fisici tenendo conto del DPR dello schermo */
    const qreal dpr = m_fotoLbl->devicePixelRatioF();
    const int   sz  = static_cast<int>(44 * dpr);

    pm = pm.scaled(sz, sz,
        Qt::KeepAspectRatioByExpanding, Qt::SmoothTransformation);

    QPixmap result(sz, sz);
    result.setDevicePixelRatio(dpr);
    result.fill(Qt::transparent);
    QPainter p(&result);
    p.setRenderHint(QPainter::Antialiasing);
    p.setClipRegion(QRegion(0, 0, sz, sz, QRegion::Ellipse));
    p.drawPixmap(0, 0, pm);
    p.end();

    m_fotoLbl->setPixmap(result);
    m_fotoLbl->setText("");
}

/* ── B2: email candidatura ── */
void LavoroPage::onEmailBtnClicked()
{
    const QString sys =
        "Sei un esperto di ricerca lavoro. Scrivi una email di candidatura "
        "professionale e personale basata sulle informazioni fornite. "
        "Struttura: Oggetto, Corpo (apertura + competenze chiave + motivazione + chiusura). "
        "Tono: formale ma diretto. Rispondi in italiano.";
    runAction(sys);
}

/* ══════════════════════════════════════════════════════════════
   Slot — Tab 1: Tracker Offerte
   ══════════════════════════════════════════════════════════════ */
void LavoroPage::onAddOfferta()
{
    auto* dlg = new OffertaDialog(this);
    if (dlg->exec() != QDialog::Accepted) {
        dlg->deleteLater();
        return;
    }
    QJsonObject o;
    o["azienda"] = dlg->azienda();
    o["ruolo"]   = dlg->ruolo();
    o["stato"]   = dlg->stato();
    o["data"]    = QDateTime::currentDateTime().toString("dd/MM/yyyy");
    dlg->deleteLater();

    m_offerte.append(o);
    saveOfferte();
    rebuildList();
}

void LavoroPage::onDeleteOfferta()
{
    const int row = m_offerteList->currentRow();
    if (row < 0) return;
    auto* item = m_offerteList->item(row);
    if (!item) return;
    const int idx = item->data(Qt::UserRole).toInt();
    if (idx < 0 || idx >= m_offerte.size()) return;

    const QJsonObject o = m_offerte[idx].toObject();
    const int ans = QMessageBox::question(
        this, "Elimina offerta",
        QString("Eliminare l'offerta per \"%1\" da \"%2\"?")
            .arg(o["ruolo"].toString(), o["azienda"].toString()),
        QMessageBox::Yes | QMessageBox::No);
    if (ans != QMessageBox::Yes) return;

    m_offerte.removeAt(idx);
    saveOfferte();
    rebuildList();
    m_delBtn->setEnabled(false);
}

/* Long-press su lista = selezione + abilita Delete */
void LavoroPage::onOffertaLongPressed(QListWidgetItem* item)
{
    if (!item || !item->flags().testFlag(Qt::ItemIsEnabled)) return;
    m_offerteList->setCurrentItem(item);
    m_delBtn->setEnabled(true);
}

/* ══════════════════════════════════════════════════════════════
   Slot — Tab 2: Cover Letter
   ══════════════════════════════════════════════════════════════ */
void LavoroPage::onCoverAiClicked()
{
    if (m_busy || m_ai->busy()) {
        m_coverOutput->setPlainText(
            QString::fromUtf8("\xe2\x9a\xa0\xef\xb8\x8f")
            + " L'AI sta gia' elaborando. Attendi.");
        return;
    }
    const QString ctx = m_coverEdit->toPlainText().trimmed();
    if (ctx.isEmpty()) {
        m_coverOutput->setPlainText(
            QString::fromUtf8("\xe2\x9a\xa0\xef\xb8\x8f")
            + " Inserisci il contesto (ruolo, azienda, competenze) sopra.");
        return;
    }

    const QString sys =
        "Sei un esperto di carriera e comunicazione professionale. "
        "Scrivi una cover letter (lettera di presentazione) completa e convincente "
        "basata sul contesto fornito. "
        "Struttura: apertura personalizzata + esperienze rilevanti + motivazione + chiusura. "
        "Massimo 350 parole. Tono: professionale, diretto, entusiasta. Rispondi in italiano.";

    m_coverBusy = true;
    m_coverOutput->clear();
    m_coverProgress->setVisible(true);
    m_coverAiBtn->setEnabled(false);

    m_ai->chat(sys, ctx);
}

void LavoroPage::onCoverSaveClicked()
{
    const QString text = m_coverOutput->toPlainText().trimmed();
    if (text.isEmpty()) {
        QMessageBox::information(this, "Salva",
            "La cover letter e' vuota.");
        return;
    }
    const QString path = QFileDialog::getSaveFileName(
        this, "Salva Cover Letter",
        QStandardPaths::writableLocation(QStandardPaths::DocumentsLocation)
            + "/cover_letter_"
            + QDateTime::currentDateTime().toString("yyyyMMdd_HHmm") + ".txt",
        "Testo (*.txt);;Tutti i file (*)");
    if (path.isEmpty()) return;

    QFile f(path);
    if (!f.open(QIODevice::WriteOnly | QIODevice::Text)) {
        QMessageBox::critical(this, "Errore",
            "Impossibile scrivere:\n" + path);
        return;
    }
    QTextStream(&f) << text;
    f.close();

    QMessageBox::information(this, "Salvato",
        QString::fromUtf8("\xe2\x9c\x85")
        + " Cover letter salvata:\n" + QFileInfo(path).fileName());
}

