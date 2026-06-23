#include "file_ai_page.h"
#include <QVBoxLayout>
#include <QHBoxLayout>
#include <QScrollArea>
#include <QFont>
#include <QFile>
#include <QFileInfo>
#include <QFileDialog>
#include <QTextStream>
#include <QTextCursor>
#include <QRegularExpression>
#include <QScroller>
#include <zlib.h>
#ifdef Q_OS_ANDROID
#  include <QJniObject>
#  include <QJniEnvironment>
#endif

/* ── Decomprime stream FlateDecode (copiato da lavoro_page.cpp) ── */
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

/* ── Estrae testo da PDF (copiato da lavoro_page.cpp) ─────────── */
QString FileAiPage::extractPdfText(const QByteArray& raw)
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
        if (dataStart >= raw.size() || raw[dataStart] != '\n') { pos = kw + 6; continue; }
        ++dataStart;
        const int endKw = raw.indexOf("endstream", dataStart);
        if (endKw == -1) break;
        int dataEnd = endKw;
        while (dataEnd > dataStart &&
               (raw[dataEnd - 1] == '\n' || raw[dataEnd - 1] == '\r'))
            --dataEnd;
        if (dataEnd > dataStart) {
            const QByteArray dec = inflatePdfStream(raw.mid(dataStart, dataEnd - dataStart));
            if (!dec.isEmpty())
                result += extractBTET(QString::fromLatin1(dec.constData(), dec.size()));
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

/* ── Pillole azione rapida ──────────────────────────────────── */
static const struct { const char* icon; const char* label; const char* prompt; } kQuick[] = {
    { "\xf0\x9f\x93\x9d", "Riassumi",    "Riassumi il documento in 5 punti chiave."           },
    { "\xf0\x9f\x94\x91", "Punti chiave","Elenca i punti e concetti più importanti."           },
    { "\xe2\x9d\x93",     "Domande",     "Genera 5 domande significative sul contenuto."       },
    { "\xf0\x9f\x92\xa1", "Insights",    "Quali insights o conclusioni si possono trarre?"     },
    { "\xf0\x9f\x93\x90", "Struttura",   "Descrivi la struttura e le sezioni del documento."  },
    { "\xf0\x9f\x8c\x90", "Traduci",     "Traduci il contenuto principale in italiano."        },
    { "\xf0\x9f\x94\x8d", "Cerca",       "Cerca le informazioni più rilevanti nel documento." },
};
static constexpr int kQuickCount = static_cast<int>(sizeof(kQuick) / sizeof(kQuick[0]));
static constexpr int kMaxCtx     = 5000; /* caratteri massimi del file nel contesto */

FileAiPage::FileAiPage(AiClient* ai, QWidget* parent)
    : QWidget(parent), m_ai(ai)
{
    auto* vbox = new QVBoxLayout(this);
    vbox->setContentsMargins(10, 8, 10, 8);
    vbox->setSpacing(8);

    /* Titolo */
    auto* title = new QLabel(
        QString::fromUtf8("\xf0\x9f\x93\x81") + " File AI \xe2\x80\x94 Analisi Documenti", this);
    {
        QFont f = title->font();
        f.setPointSize(14);
        f.setBold(true);
        title->setFont(f);
    }
    vbox->addWidget(title);

    /* Riga carica file */
    auto* loadRow = new QHBoxLayout;
    loadRow->setSpacing(8);
    auto* loadBtn = new QPushButton(
        QString::fromUtf8("\xf0\x9f\x93\x82") + " Carica File", this);
    loadBtn->setObjectName("PrimaryBtn");
    loadBtn->setMinimumHeight(40);
    connect(loadBtn, &QPushButton::clicked, this, &FileAiPage::onLoadFile);
    loadRow->addWidget(loadBtn);

    m_fileLbl = new QLabel("Nessun file caricato", this);
    m_fileLbl->setObjectName("StatusLabel");
    m_fileLbl->setWordWrap(true);
    loadRow->addWidget(m_fileLbl, 1);
    vbox->addLayout(loadRow);

    /* Anteprima contenuto */
    m_previewLbl = new QLabel("", this);
    m_previewLbl->setObjectName("StatusLabel");
    m_previewLbl->setWordWrap(true);
    m_previewLbl->setMaximumHeight(60);
    m_previewLbl->setVisible(false);
    vbox->addWidget(m_previewLbl);

    /* Pillole azione rapida */
    auto* pillScroll = new QScrollArea(this);
    pillScroll->setHorizontalScrollBarPolicy(Qt::ScrollBarAlwaysOff);
    pillScroll->setVerticalScrollBarPolicy(Qt::ScrollBarAlwaysOff);
    pillScroll->setFixedHeight(52);
    pillScroll->setWidgetResizable(true);
    QScroller::grabGesture(pillScroll->viewport(), QScroller::TouchGesture);

    auto* pillW   = new QWidget;
    auto* pillRow = new QHBoxLayout(pillW);
    pillRow->setContentsMargins(4, 4, 4, 4);
    pillRow->setSpacing(6);
    for (int i = 0; i < kQuickCount; ++i) {
        auto* btn = new QPushButton(
            QString::fromUtf8(kQuick[i].icon) + " " + kQuick[i].label, pillW);
        btn->setObjectName("PillBtn");
        btn->setFixedHeight(36);
        btn->setProperty("quickIndex", i);
        connect(btn, &QPushButton::clicked, this, &FileAiPage::onQuickClicked);
        pillRow->addWidget(btn);
    }
    pillRow->addStretch();
    pillScroll->setWidget(pillW);
    vbox->addWidget(pillScroll);

    /* Output streaming */
    m_output = new QTextEdit(this);
    m_output->setReadOnly(true);
    m_output->setObjectName("OutputEdit");
    m_output->setPlaceholderText("Carica un file, poi fai domande...");
    {
        QFont f = m_output->font();
        f.setPointSize(11);
        m_output->setFont(f);
    }
    QScroller::grabGesture(m_output->viewport(), QScroller::TouchGesture);
    vbox->addWidget(m_output, 1);

    /* Input + Invia/Stop */
    auto* inputRow = new QHBoxLayout;
    inputRow->setSpacing(6);

    m_input = new QTextEdit(this);
    m_input->setFixedHeight(64);
    m_input->setPlaceholderText("Fai una domanda sul file...");
    m_input->setObjectName("InputEdit");
    inputRow->addWidget(m_input, 1);

    auto* btnCol = new QVBoxLayout;
    btnCol->setSpacing(4);

    m_sendBtn = new QPushButton(
        QString::fromUtf8("\xe2\x9e\xa4") + " Chiedi", this);
    m_sendBtn->setObjectName("PrimaryBtn");
    m_sendBtn->setMinimumHeight(30);
    m_sendBtn->setEnabled(false);
    connect(m_sendBtn, &QPushButton::clicked, this, &FileAiPage::onSendClicked);
    btnCol->addWidget(m_sendBtn);

    m_stopBtn = new QPushButton(
        QString::fromUtf8("\xe2\x8f\xb9") + " Stop", this);
    m_stopBtn->setObjectName("DangerBtn");
    m_stopBtn->setMinimumHeight(30);
    m_stopBtn->setVisible(false);
    connect(m_stopBtn, &QPushButton::clicked, m_ai, &AiClient::abort);
    btnCol->addWidget(m_stopBtn);

    inputRow->addLayout(btnCol);
    vbox->addLayout(inputRow);

    /* Status */
    m_statusLbl = new QLabel("", this);
    m_statusLbl->setObjectName("StatusLabel");
    vbox->addWidget(m_statusLbl);

    connect(m_ai, &AiClient::token,    this, &FileAiPage::onToken);
    connect(m_ai, &AiClient::finished, this, &FileAiPage::onFinished);
    connect(m_ai, &AiClient::error,    this, &FileAiPage::onAiError);
    connect(m_ai, &AiClient::aborted,  this, &FileAiPage::onAborted);
}

void FileAiPage::onLoadFile()
{
    const QString path = QFileDialog::getOpenFileName(
        this, "Carica documento",
        {},
        "Documenti (*.pdf *.txt *.md *.csv *.log);;Tutti i file (*)");
    if (path.isEmpty()) return;

    const QFileInfo fi(path);
    QFile f(path);
    if (!f.open(QIODevice::ReadOnly)) {
        m_statusLbl->setText(
            QString::fromUtf8("\xe2\x9d\x8c") + " Impossibile aprire il file.");
        return;
    }

    QString content;
    if (fi.suffix().toLower() == "pdf") {
        content = extractPdfText(f.readAll());
        if (content.isEmpty()) {
            m_statusLbl->setText(
                QString::fromUtf8("\xe2\x9a\xa0\xef\xb8\x8f") +
                " PDF senza testo estraibile (probabilmente scansione).");
        }
    } else {
        QTextStream ts(&f);
        content = ts.readAll();
    }
    applyFile(fi.fileName(), content);
}

void FileAiPage::applyFile(const QString& name, const QString& content)
{
    m_fileName    = name;
    m_fileContent = content;
    m_fileLbl->setText(
        QString::fromUtf8("\xf0\x9f\x93\x84") + " " + name
        + "  (" + QString::number(content.length()) + " car.)");

    const QString preview = content.left(200).replace('\n', ' ');
    m_previewLbl->setText(preview + (content.length() > 200 ? "..." : ""));
    m_previewLbl->setVisible(true);
    m_sendBtn->setEnabled(true);
    m_output->clear();
    m_statusLbl->setText(
        QString::fromUtf8("\xe2\x9c\x85") + " File caricato.");
}

void FileAiPage::onQuickClicked()
{
    if (m_fileContent.isEmpty()) {
        m_statusLbl->setText("Carica prima un file.");
        return;
    }
    auto* btn = qobject_cast<QPushButton*>(sender());
    if (!btn) return;
    const int idx = btn->property("quickIndex").toInt();
    if (idx < 0 || idx >= kQuickCount) return;
    m_input->setPlainText(QString::fromUtf8(kQuick[idx].prompt));
    onSendClicked();
}

void FileAiPage::onSendClicked()
{
    const QString question = m_input->toPlainText().trimmed();
    if (question.isEmpty() || m_fileContent.isEmpty()) return;

    const QString ctx = m_fileContent.left(kMaxCtx);
    const QString sys =
        "Sei un assistente di analisi documenti. "
        "Il documento caricato si chiama '" + m_fileName + "'.\n\n"
        "=== CONTENUTO DOCUMENTO ===\n" + ctx
        + (m_fileContent.length() > kMaxCtx ? "\n[... troncato ...]" : "")
        + "\n=== FINE DOCUMENTO ===\n\n"
        "Rispondi alle domande basandoti esclusivamente sul documento. "
        "Sii preciso, conciso e usa Markdown quando utile.";

    setBusy(true);
    m_output->clear();
    m_ai->chat(sys, question);
}

void FileAiPage::onToken(const QString& t)
{
    m_output->moveCursor(QTextCursor::End);
    m_output->insertPlainText(t);
    m_output->moveCursor(QTextCursor::End);
}

void FileAiPage::onFinished(const QString&)
{
    setBusy(false);
    m_statusLbl->setText("");
}

void FileAiPage::onAiError(const QString& msg)
{
    setBusy(false);
    m_statusLbl->setText(QString::fromUtf8("\xe2\x9d\x8c") + " " + msg);
}

void FileAiPage::onAborted()
{
    setBusy(false);
    m_statusLbl->setText("Risposta interrotta.");
}

void FileAiPage::setBusy(bool busy)
{
    m_sendBtn->setEnabled(!busy && !m_fileContent.isEmpty());
    m_stopBtn->setVisible(busy);
    m_input->setEnabled(!busy);
    if (busy)
        m_statusLbl->setText(
            QString::fromUtf8("\xe2\x8f\xb3") + " Analisi in corso...");
}

/* ══════════════════════════════════════════════════════════════
   C4 — loadSharedText / loadSharedUri
   Chiamati da MainWindow::handleShareIntent() quando un'altra
   app condivide testo o un file tramite ACTION_SEND.
   ══════════════════════════════════════════════════════════════ */
void FileAiPage::loadSharedText(const QString& text)
{
    if (text.isEmpty()) return;
    applyFile("[testo condiviso]", text);
}

void FileAiPage::loadSharedUri(const QString& uri)
{
    if (uri.isEmpty()) return;

#ifdef Q_OS_ANDROID
    /* Su Android legge il file tramite ContentResolver usando l'URI */
    QJniObject jUri = QJniObject::callStaticObjectMethod(
        "android/net/Uri", "parse",
        "(Ljava/lang/String;)Landroid/net/Uri;",
        QJniObject::fromString(uri).object<jstring>());
    if (!jUri.isValid()) { applyFile(uri, ""); return; }

    QJniObject activity = QJniObject::callStaticObjectMethod(
        "org/qtproject/qt/android/QtNative",
        "activity",
        "()Landroid/app/Activity;");
    if (!activity.isValid()) { applyFile(uri, ""); return; }

    QJniObject resolver = activity.callObjectMethod(
        "getContentResolver", "()Landroid/content/ContentResolver;");
    if (!resolver.isValid()) { applyFile(uri, ""); return; }

    QJniObject inputStream = resolver.callObjectMethod(
        "openInputStream",
        "(Landroid/net/Uri;)Ljava/io/InputStream;",
        jUri.object());
    if (!inputStream.isValid()) { applyFile(uri, ""); return; }

    /* Legge tutti i byte via InputStream.read() */
    QByteArray raw;
    raw.reserve(1024 * 64);
    QJniEnvironment env;
    jbyteArray jbuf = env->NewByteArray(4096);
    while (true) {
        jint n = inputStream.callMethod<jint>(
            "read", "([B)I", jbuf);
        if (n <= 0) break;
        jbyte* buf = env->GetByteArrayElements(jbuf, nullptr);
        raw.append(reinterpret_cast<const char*>(buf), static_cast<int>(n));
        env->ReleaseByteArrayElements(jbuf, buf, JNI_ABORT);
    }
    env->DeleteLocalRef(jbuf);
    inputStream.callMethod<void>("close");

    /* Deriva il nome file dall'URI */
    const QString displayName = QUrl(uri).fileName().isEmpty()
        ? "[file condiviso]" : QUrl(uri).fileName();

    /* Se sembra un PDF, usa l'estrattore; altrimenti testo grezzo */
    const QString lower = displayName.toLower();
    const QString content = lower.endsWith(".pdf")
        ? extractPdfText(raw)
        : QString::fromUtf8(raw);

    applyFile(displayName, content);
#else
    /* Desktop: apri come file locale */
    const QString localPath = QUrl(uri).toLocalFile();
    if (localPath.isEmpty()) { applyFile(uri, ""); return; }

    const QFileInfo fi(localPath);
    QFile f(localPath);
    if (!f.open(QIODevice::ReadOnly)) { applyFile(fi.fileName(), ""); return; }

    const QString content = fi.suffix().toLower() == "pdf"
        ? extractPdfText(f.readAll())
        : QTextStream(&f).readAll();
    applyFile(fi.fileName(), content);
#endif
}
