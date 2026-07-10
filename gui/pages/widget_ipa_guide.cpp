#include "widget_ipa_guide.h"
#include "../dpi_utils.h"
#include "../theme_manager.h"
#include "../widgets/tts_speak.h"
#include <QVBoxLayout>
#include <QLabel>
#include <QTextBrowser>
#include <QUrl>
#include <QByteArray>
#include <QScrollArea>
#include <QPushButton>
#include <QHBoxLayout>

namespace {

struct IpaColors { QString bg, card, bdr, accent, txt, mut; };

/* Ogni cella della griglia stile "Phonemic Chart" classica: simbolo,
   parola d'esempio, colore di categoria (breve/lunga/dittongo per le
   vocali, sonora/sorda per le consonanti) e mnemonic espeak-ng per la
   modalità fonemi "[[...]]" (NON IPA, l'alfabeto ASCII interno di
   espeak-ng) — derivato confrontando `--ipa` e `-x` su decine di parole
   inglesi reali per ogni fonema, non da una tabella di conversione
   trovata altrove. Vedi speakIpaSequence() in tts_speak.h. */
struct IpaCell { const char* symbol; const char* example; const char* bg; const char* mnemonic; };

constexpr const char* kBgShort    = "#fdf1e8";  /* vocale breve  */
constexpr const char* kBgLong     = "#fbddc0";  /* vocale lunga  */
constexpr const char* kBgDipth    = "#f7b57a";  /* dittongo      */
constexpr const char* kBgVoiced   = "#a8c3dc";  /* cons. sonora  */
constexpr const char* kBgUnvoiced = "#c5d9b3";  /* cons. sorda   */

/* Le 20 vocali (12 monottonghi + 8 dittonghi) nella stessa disposizione
   a griglia della tabella fonemica classica (British Council/Cambridge):
   la prima riga ha una cella in meno (nella tabella originale lì c'è il
   titolo "Phonemic Chart"). */
const IpaCell kVowelRow1[] = {
    {"iː", "sheep", kBgLong, "i:"}, {"ɪ", "ship", kBgShort, "I"}, {"ʊ", "good", kBgShort, "U"},
    {"uː", "moon", kBgLong, "u:"}, {"ɪə", "ear", kBgDipth, "i@"}, {"eɪ", "train", kBgDipth, "eI"},
};
const IpaCell kVowelRow2[] = {
    {"e", "bed", kBgShort, "E"}, {"ə", "about", kBgShort, "@"}, {"ɜː", "bird", kBgLong, "3:"},
    {"ɔː", "door", kBgLong, "o@"}, {"ʊə", "your", kBgDipth, "O@"}, {"ɔɪ", "boy", kBgDipth, "OI"},
    {"əʊ", "coat", kBgDipth, "oU"},
};
const IpaCell kVowelRow3[] = {
    {"æ", "apple", kBgShort, "a"}, {"ʌ", "up", kBgShort, "V"}, {"ɑː", "car", kBgLong, "A@"},
    {"ɒ", "not", kBgShort, "0"}, {"eə", "hair", kBgDipth, "e@"}, {"aɪ", "by", kBgDipth, "aI"},
    {"aʊ", "now", kBgDipth, "aU"},
};

/* Le 24 consonanti, righe organizzate per coppie sonora/sorda come
   nella tabella classica (ultima riga quasi tutta sonora tranne "h"). */
const IpaCell kConsRow1[] = {
    {"p", "pen", kBgUnvoiced, "p"}, {"b", "ball", kBgVoiced, "b"}, {"t", "table", kBgUnvoiced, "t"},
    {"d", "dog", kBgVoiced, "d"}, {"tʃ", "chips", kBgUnvoiced, "tS"}, {"dʒ", "jam", kBgVoiced, "dZ"},
    {"k", "key", kBgUnvoiced, "k"}, {"g", "green", kBgVoiced, "g"},
};
const IpaCell kConsRow2[] = {
    {"f", "fire", kBgUnvoiced, "f"}, {"v", "video", kBgVoiced, "v"}, {"θ", "thick", kBgUnvoiced, "T"},
    {"ð", "mother", kBgVoiced, "D"}, {"s", "see", kBgUnvoiced, "s"}, {"z", "zebra", kBgVoiced, "z"},
    {"ʃ", "shop", kBgUnvoiced, "S"}, {"ʒ", "television", kBgVoiced, "Z"},
};
const IpaCell kConsRow3[] = {
    {"m", "man", kBgVoiced, "m"}, {"n", "no", kBgVoiced, "n"}, {"ŋ", "sing", kBgVoiced, "N"},
    {"j", "yes", kBgVoiced, "j"}, {"l", "light", kBgVoiced, "l"}, {"r", "right", kBgVoiced, "r"},
    {"w", "win", kBgVoiced, "w"}, {"h", "house", kBgUnvoiced, "h"},
};

/* "DejaVu Sans" copre correttamente tutti i glifi IPA usati qui (ʃ ɜː ɑː
   ɒ ʌ ə ŋ ʒ θ ð ɪ ʊ ː — verificato via fontTools su tutti i cmap): senza
   forzare il font, Qt può scegliere un fallback che sostituisce simboli
   come ʃ con un glifo simile ma sbagliato (es. una sigma greca). */
const char* const kIpaFontFamily = "font-family:'DejaVu Sans','Noto Sans',sans-serif;";

/* Cella tabella: simbolo grande e parola d'esempio sotto sono due link
   <a> DISTINTI (non un unico anchor che avvolge entrambi) — entrambi
   puntano alla stessa parola d'esempio via speakEnglishWord(), perché
   espeak-ng 1.52 non sa isolare il suono del singolo fonema (SSML
   <phoneme> ignorato silenziosamente, vedi tts_speak.h); la separazione
   in due anchor è solo per rendere visivamente chiaro che l'intera
   cella (simbolo E parola) è cliccabile, non per suoni diversi.
   Sempre testo scuro leggibile perché lo sfondo pastello resta chiaro
   in entrambi i temi dell'app. */
QString ipaCellTd(const IpaCell& p)
{
    /* Payload "simbolo|mnemonic|parola" — il carattere '|' non compare
       mai in nessuno dei tre campi (simbolo IPA, mnemonic espeak-ng
       ASCII, parole inglesi semplici), quindi lo split in
       onAnchorClicked() è sicuro senza un encoding più complesso. */
    const QString b64 = QString::fromLatin1(
        (QByteArray(p.symbol) + "|" + QByteArray(p.mnemonic) + "|" + QByteArray(p.example))
            .toBase64(QByteArray::Base64UrlEncoding));
    const QString title = QString("title='Ascolta \xe2\x80\x9c%1\xe2\x80\x9d' ").arg(
        QString::fromUtf8(p.example));
    return QString(
        "<td style='background:%1;border:1px solid #ffffff80;text-align:center;"
        "padding:12px 4px;'>"
        "<a href='ipa:%2' %6 style='text-decoration:none;color:#1e293b;'>"
        "<span style='%5font-size:34px;font-weight:bold;'><u>%3</u></span>"
        "</a><br>"
        "<a href='ipa:%2' %6 style='text-decoration:none;color:#1e293b;'>"
        "<span style='font-size:14px;'><u>%4</u></span>"
        "</a></td>")
        .arg(QString::fromLatin1(p.bg), b64, QString::fromUtf8(p.symbol),
             QString::fromUtf8(p.example), QString::fromLatin1(kIpaFontFamily), title);
}

QString ipaEmptyTd()
{
    return "<td style='padding:12px 4px;'></td>";
}

template <size_t N>
QString ipaRowTr(const IpaCell (&row)[N])
{
    QString html = "<tr>";
    for (const auto& p : row) html += ipaCellTd(p);
    return html + "</tr>";
}

/* Etichetta laterale verticale (Vocali/Consonanti), rowspan sull'intera
   sezione — Qt rich text non supporta la rotazione testo (niente
   "writing-mode"), quindi il testo va a capo lettera per lettera. */
QString ipaSideLabelTd(const QString& label, const QString& bg, int rowspan)
{
    QString spaced;
    for (const QChar& ch : label) spaced += ch + QStringLiteral("<br>");
    return QString(
        "<td rowspan='%1' width='36' style='background:%2;color:#1e293b;text-align:center;"
        "vertical-align:middle;font-weight:bold;font-size:14px;"
        "padding:4px 2px;'>%3</td>")
        .arg(rowspan).arg(bg, spaced);
}

/* "display:inline-block" non è supportato dal rich text di Qt (uno <span>
   con solo width/height collassa a dimensione zero): il quadratino colore
   va reso con una cella di tabella vera, come il resto della griglia. */
QString ipaLegendCellPair(const QString& bg, const QString& label)
{
    return QString(
        "<td width='16' height='16' style='background:%1;'></td>"
        "<td style='padding:0 16px 0 5px;font-size:12px;white-space:nowrap;'>%2</td>")
        .arg(bg, label);
}

QString ipaLegendHtml()
{
    return "<table cellpadding='0' cellspacing='0' style='margin-top:10px;'><tr>"
        + ipaLegendCellPair(kBgShort, "breve")
        + ipaLegendCellPair(kBgLong, "lunga")
        + ipaLegendCellPair(kBgDipth, "dittongo")
        + ipaLegendCellPair(kBgVoiced, "sonora")
        + ipaLegendCellPair(kBgUnvoiced, "sorda")
        + "</tr></table>";
}

/* Tabella fonemica completa: due <table> (vocali/consonanti) + legenda
   colori, stessa struttura a griglia della tabella classica in
   Tools/ipa/phonemic_chart.jpg, ma ogni simbolo è un link "ipa:" cliccabile.
   width='100%' è un ATTRIBUTO HTML (non CSS style, che Qt rich text ignora
   sulla larghezza della tabella — stesso pattern già usato altrove nel
   progetto per le tabelle nelle bolle chat, vedi main_ai_bubbles.cpp). */
QString ipaChartHtml()
{
    QString html;

    html += "<table width='100%' cellpadding='0' cellspacing='0' "
            "style='border-collapse:collapse;margin-top:10px;'>";
    html += "<tr>" + ipaSideLabelTd("Vocali", "#f2914a", 3);
    for (const auto& p : kVowelRow1) html += ipaCellTd(p);
    html += ipaEmptyTd() + "</tr>";
    html += ipaRowTr(kVowelRow2);
    html += ipaRowTr(kVowelRow3);
    html += "</table>";

    html += "<table width='100%' cellpadding='0' cellspacing='0' "
            "style='border-collapse:collapse;margin-top:6px;'>";
    html += "<tr>" + ipaSideLabelTd("Consonanti", "#8fae6b", 3);
    for (const auto& p : kConsRow1) html += ipaCellTd(p);
    html += "</tr>";
    html += ipaRowTr(kConsRow2);
    html += ipaRowTr(kConsRow3);
    html += "</table>";

    html += ipaLegendHtml();

    return html;
}

IpaColors themeColors()
{
    const bool light = ThemeManager::instance()->currentId().startsWith("light");
    IpaColors c;
    if (light) {
        c.bg     = "#f8fafc";
        c.card   = "#f1f5f9";
        c.bdr    = "#cbd5e1";
        c.accent = "#2563eb";
        c.txt    = "#1e293b";
        c.mut    = "#64748b";
    } else {
        c.bg     = "#0e1624";
        c.card   = "#111c2e";
        c.bdr    = "#1e3a5f";
        c.accent = "#3b82f6";
        c.txt    = "#e2e8f0";
        c.mut    = "#94a3b8";
    }
    return c;
}

QString card(const IpaColors& c, const QString& titolo, const QString& body)
{
    return QString(
        "<div style='background:%1;border:1px solid %2;border-left:4px solid %3;"
        "border-radius:6px;padding:10px 14px;margin:6px 0;'>"
        "<p style='color:%3;font-weight:bold;margin:0 0 6px 0;font-size:13px;'>%4</p>"
        "<div style='color:%5;font-size:12px;line-height:1.7;'>%6</div>"
        "</div>")
        .arg(c.card, c.bdr, c.accent, titolo, c.txt, body);
}

} // namespace

IpaGuideWidget::IpaGuideWidget(QWidget* parent) : QWidget(parent)
{
    buildUi();
    connect(ThemeManager::instance(), &ThemeManager::changed,
            this, [this](const QString&){ updateContent(); });
}

IpaGuideWidget::~IpaGuideWidget()
{
    /* QProcess figlio ancora attivo: ~QProcess emette finished() SINCRONO
       durante deleteChildren() con this già semi-distrutto — disconnettere
       prima di uccidere il processo, stesso pattern di ~VoiceClonerWidget
       (vedi gui/CLAUDE.md, "QProcess figli nei distruttori widget"). */
    const auto procs = findChildren<QProcess*>();
    for (QProcess* p : procs) {
        p->disconnect(this);
        p->blockSignals(true);
        p->kill();
        p->waitForFinished(1000);
    }
}

void IpaGuideWidget::buildUi()
{
    auto* lay = new QVBoxLayout(this);
    lay->setContentsMargins(0, 0, 0, 0);
    lay->setSpacing(0);

    auto* hdr = new QLabel(
        "  \xf0\x9f\x97\xa3\xef\xb8\x8f  <b>IPA \xe2\x80\x94 Parlare e scrivere in Inglese ed altre lingue</b>"
        "  <span style='color:gray;font-size:11px;'>"
        "Alfabeto Fonetico Internazionale \xc2\xb7 8 fasi per imparare una lingua"
        "</span>", this);
    hdr->setTextFormat(Qt::RichText);
    hdr->setObjectName("pageHeader");
    hdr->setFixedHeight(dpiScale(36));
    lay->addWidget(hdr);

    m_view = new QTextBrowser(this);
    m_view->setOpenLinks(false);
    m_view->setOpenExternalLinks(false);
    m_view->setContentsMargins(0, 0, 0, 0);
    lay->addWidget(m_view, /*stretch=*/1);

    connect(m_view, &QTextBrowser::anchorClicked,
            this, &IpaGuideWidget::onAnchorClicked);

    buildComposeBar(lay);

    updateContent();
}

/* Barra "Componi una parola" sotto la tabella: tessere accodate dai click
   sui fonemi (dentro una QScrollArea orizzontale, perché una parola può
   avere più fonemi di quanti ne stiano in larghezza) + pulsante play/pausa
   che riproduce la sequenza in ordine, un fonema isolato alla volta. */
void IpaGuideWidget::buildComposeBar(QVBoxLayout* lay)
{
    auto* bar = new QWidget(this);
    auto* barLay = new QVBoxLayout(bar);
    barLay->setContentsMargins(dpiScale(10), dpiScale(6), dpiScale(10), dpiScale(10));
    barLay->setSpacing(dpiScale(6));

    auto* title = new QLabel(
        "  \xf0\x9f\xa7\xa9  <b>Componi una parola</b>"
        "  <span style='color:gray;font-size:11px;'>"
        "Tocca un fonema nella tabella sopra per aggiungerlo qui \xc2\xb7 "
        "suono sintetico approssimato, fonemi concatenati senza coarticolazione naturale"
        "</span>", bar);
    title->setTextFormat(Qt::RichText);
    title->setWordWrap(true);
    barLay->addWidget(title);

    auto* scroll = new QScrollArea(bar);
    scroll->setWidgetResizable(true);
    scroll->setHorizontalScrollBarPolicy(Qt::ScrollBarAsNeeded);
    scroll->setVerticalScrollBarPolicy(Qt::ScrollBarAlwaysOff);
    scroll->setFixedHeight(dpiScale(56));

    m_tileContainer = new QWidget(scroll);
    m_tileLayout = new QHBoxLayout(m_tileContainer);
    m_tileLayout->setContentsMargins(dpiScale(4), dpiScale(4), dpiScale(4), dpiScale(4));
    m_tileLayout->setSpacing(dpiScale(4));
    m_tileLayout->addStretch(1);
    scroll->setWidget(m_tileContainer);
    barLay->addWidget(scroll);

    auto* btnRow = new QHBoxLayout();
    m_btnPlayCompose = new QPushButton("\xe2\x96\xb6 Ascolta", bar);
    m_btnClearCompose = new QPushButton("\xf0\x9f\x97\x91 Cancella", bar);
    btnRow->addWidget(m_btnPlayCompose);
    btnRow->addWidget(m_btnClearCompose);
    btnRow->addStretch(1);
    barLay->addLayout(btnRow);

    connect(m_btnPlayCompose, &QPushButton::clicked,
            this, &IpaGuideWidget::onPlayPauseComposeClicked);
    connect(m_btnClearCompose, &QPushButton::clicked,
            this, &IpaGuideWidget::onClearComposeClicked);

    lay->addWidget(bar);
}

void IpaGuideWidget::onAnchorClicked(const QUrl& url)
{
    const QString s = url.toString();
    if (!s.startsWith("ipa:")) return;

    const QByteArray payload = QByteArray::fromBase64(
        s.mid(4).toLatin1(), QByteArray::Base64UrlEncoding);
    const QList<QByteArray> parts = payload.split('|');
    if (parts.size() != 3) return;

    const QString symbol = QString::fromUtf8(parts.at(0));
    const QString mnemonic = QString::fromUtf8(parts.at(1));
    const QString word = QString::fromUtf8(parts.at(2));

    TtsSpeak::speakIpaSequence(mnemonic, word);
    addComposedPhoneme(symbol, mnemonic);
}

void IpaGuideWidget::addComposedPhoneme(const QString& symbol, const QString& mnemonic)
{
    m_composedSymbols << symbol;
    m_composedMnemonics << mnemonic;
    rebuildTileRow();
}

void IpaGuideWidget::rebuildTileRow()
{
    QLayoutItem* item;
    while ((item = m_tileLayout->takeAt(0)) != nullptr) {
        if (item->widget()) item->widget()->deleteLater();
        delete item;
    }

    for (int i = 0; i < m_composedSymbols.size(); ++i) {
        auto* tile = new QPushButton(m_composedSymbols.at(i), m_tileContainer);
        tile->setProperty("tileIndex", i);
        tile->setToolTip(tr("Clicca per rimuovere"));
        tile->setFixedHeight(dpiScale(36));
        tile->setStyleSheet("font-size:16px;font-weight:bold;padding:2px 12px;");
        connect(tile, &QPushButton::clicked,
                this, &IpaGuideWidget::onPhonemeTileClicked);
        m_tileLayout->addWidget(tile);
    }
    m_tileLayout->addStretch(1);
}

void IpaGuideWidget::onPhonemeTileClicked()
{
    auto* btn = qobject_cast<QPushButton*>(sender());
    if (!btn) return;
    const int idx = btn->property("tileIndex").toInt();
    if (idx < 0 || idx >= m_composedSymbols.size()) return;

    fullStopComposePlayback();
    m_composedSymbols.removeAt(idx);
    m_composedMnemonics.removeAt(idx);
    rebuildTileRow();
}

void IpaGuideWidget::playComposeStep()
{
    if (m_playIndex < 0 || m_playIndex >= m_composedMnemonics.size()) return;

    m_playProc = new QProcess(this);
    connect(m_playProc, QOverload<int,QProcess::ExitStatus>::of(&QProcess::finished),
            this, &IpaGuideWidget::onComposePhonemeFinished);
    m_playProc->start("espeak-ng",
        {"-v", "en-gb", QString("[[%1]]").arg(m_composedMnemonics.at(m_playIndex))});
}

void IpaGuideWidget::onComposePhonemeFinished(int, QProcess::ExitStatus)
{
    QProcess* finished = m_playProc;
    m_playProc = nullptr;
    if (finished) finished->deleteLater();

    if (m_manualStop) {
        m_manualStop = false;
        return;  /* pausa/rimozione: l'indice resta dov'era, gestito dal chiamante */
    }

    m_playIndex++;
    if (m_playIndex < m_composedMnemonics.size()) {
        playComposeStep();
    } else {
        m_playIndex = -1;
        m_btnPlayCompose->setText("\xe2\x96\xb6 Ascolta");
    }
}

void IpaGuideWidget::onPlayPauseComposeClicked()
{
    if (m_composedMnemonics.isEmpty()) return;

    if (m_playProc) {
        pauseComposePlayback();
        return;
    }

    if (m_playIndex < 0 || m_playIndex >= m_composedMnemonics.size())
        m_playIndex = 0;

    m_btnPlayCompose->setText("\xe2\x8f\xb8 Pausa");
    playComposeStep();
}

void IpaGuideWidget::onClearComposeClicked()
{
    fullStopComposePlayback();
    m_composedSymbols.clear();
    m_composedMnemonics.clear();
    rebuildTileRow();
}

void IpaGuideWidget::pauseComposePlayback()
{
    if (m_playProc) {
        m_manualStop = true;
        m_playProc->kill();
    }
    m_btnPlayCompose->setText("\xe2\x96\xb6 Ascolta");
}

void IpaGuideWidget::fullStopComposePlayback()
{
    pauseComposePlayback();
    m_playIndex = -1;
}

void IpaGuideWidget::updateContent()
{
    const IpaColors c = themeColors();

    QString html;
    html += QString("<html><body style='background:%1;color:%2;'>").arg(c.bg, c.txt);

    /* Tabella fonemica in cima, prima di Introduzione/Fase 1 — l'utente
       vuole il grafico come primo elemento visibile, i testi sotto. */
    html += card(c, "Tabella Fonemica IPA",
        "<b>Clicca su un simbolo per ascoltarne il suono</b> \xe2\x80\x94 ogni fonema si accoda anche "
        "come tessera nella riga \xe2\x80\x9c" "Componi una parola" "\xe2\x80\x9d in fondo alla pagina."
        + ipaChartHtml());

    html += card(c, "Introduzione",
        "Come si impara una nuova lingua \xe2\x80\x94 inglese o qualsiasi altra \xe2\x80\x94 partendo "
        "dal <i>Linguaggio Plasma</i>: il pensiero modifica il suono e quindi la lingua parlata. "
        "Tenendo a mente la frase condensata di Sapir-Whorf, l'unico modo indiretto per entrare "
        "davvero in una lingua \xc3\xa8 tramite la pronuncia.");

    html += card(c, "Fase 1 \xe2\x80\x94 Fonemi",
        "Nel 1886 in Francia si volle ricreare una tabella riassuntiva che inglobasse i suoni "
        "delle lingue europee: la tabella IPA (International Phonetic Alphabet) qui sopra \xc3\xa8 la "
        "base per capire come si pronuncia correttamente qualsiasi parola del vocabolario. "
        "Imparata a memoria, va pronunciata con la stessa voglia con cui abbiamo imparato la "
        "nostra lingua madre.");

    html += card(c, "Fase 2 \xe2\x80\x94 Collegare emozioni con le altre entit\xc3\xa0",
        "Si studiano alcune parole con la giusta pronuncia. Quando si studia non bisogna "
        "utilizzare troppo sforzo: oggi o questo mese potrai ricordarlo, ma il mese successivo "
        "ancora no. Consiglio di unire mentalmente 3 concetti tra loro, cos\xc3\xac da dare qualche "
        "connessione in pi\xc3\xb9 alle sinapsi: 1) nome \xc2\xb7 2) fonetica \xc2\xb7 3) emozione. La terza far\xc3\xa0 s\xc3\xac "
        "che ci sia almeno un collegamento tra emozione-fonetica, emozione-nome (3-1, 3-2).");

    html += card(c, "Fase 3 \xe2\x80\x94 Emozioni su carta",
        "Il processo di apprendimento \xc3\xa8 un continuo amplificarsi e stupirsi: linguaggi che "
        "inizialmente sembravano distanti anni luce, in verit\xc3\xa0 sono molto simili. Ogni linguaggio "
        "\xc3\xa8, secondo questo metodo, creato secondo le emozioni: si pu\xc3\xb2 fare una mappa delle "
        "principali emozioni su carta, con qualsiasi metodo (elenco puntato, schema tabellare o "
        "disegni). Il metodo a disegni \xc3\xa8 pi\xc3\xb9 semplice da ricordare per chi \xc3\xa8 dislessico, perch\xc3\xa9 "
        "memorizza pi\xc3\xb9 facilmente sotto forma di immagini.");

    html += card(c, "Fase 4 \xe2\x80\x94 Locazione e azioni",
        "Spostarsi o fare qualcosa spiega cosa vorremmo o comanderemo a noi o ad altri. Anche "
        "qui conviene creare uno schema andando a indagare come si traducono i luoghi e le "
        "azioni di uso quotidiano.");

    html += card(c, "Fase 5 \xe2\x80\x94 L'eccezione della traduzione",
        "Non sempre si pu\xc3\xb2 tradurre 1 a 1: esistono modi di dire che non combaciano con l'idea "
        "che abbiamo noi. Il motivo \xc3\xa8 che si sta usando il dizionario della propria lingua madre "
        "e non quello della lingua che si vuole studiare \xe2\x80\x94 non fossilizzarsi su queste "
        "piccolezze, arrivano con l'esperienza.");

    html += card(c, "Fase 6 \xe2\x80\x94 Piccole frasi e grandi frasi",
        "Studiare le regole grammaticali cos\xc3\xac da poter pensare e iniziare a creare piccole "
        "frasi nella propria testa. Per aumentare la complessit\xc3\xa0 si possono concatenare pi\xc3\xb9 "
        "frasi con i connettivi logici.");

    html += card(c, "Fase 7 \xe2\x80\x94 Dialogare con altre persone",
        "Fare una passeggiata con amici e un gioco di gruppo: invitare una ventina di persone e "
        "creare una giornata all'insegna della lingua straniera scelta, con una penitenza per chi "
        "parla nella lingua madre. In alternativa, partire all'estero da soli o in gruppo per "
        "risparmiare e imparare sempre qualcosa di nuovo. Le regole ci vogliono, ma cambiarle un "
        "po' o trasgredirle ogni tanto non guasta.");

    html += card(c, "Fase 8 \xe2\x80\x94 Rilassarsi (e continuare a studiare)",
        "La bellezza pi\xc3\xb9 grande esiste in colui che \xc3\xa8 curioso di sapere \xe2\x80\x94 la curiosit\xc3\xa0 \xc3\xa8 "
        "quello che spinge sempre a migliorarsi, sia nelle lingue che nella vita.");

    html += "</body></html>";
    m_view->setHtml(html);
}
