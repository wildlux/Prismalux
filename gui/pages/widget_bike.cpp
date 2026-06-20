#include "widget_bike.h"
#include "../dpi_utils.h"
#include "../theme_manager.h"
#include <QVBoxLayout>
#include <QHBoxLayout>
#include <QComboBox>
#include <QTextBrowser>
#include <QLabel>
#include <QFrame>
#include <QRegularExpression>
#include <QTextCursor>
#include <QPalette>

/* ─── Colori adattativi dal tema corrente ──────────────────────────── */
static BkColors themeColors()
{
    const bool light = ThemeManager::instance()->currentId().startsWith("light");
    BkColors c;
    if (light) {
        c.bg     = "#f8fafc";
        c.card   = "#f1f5f9";
        c.bdr    = "#cbd5e1";
        c.accent = "#2563eb";
        c.green  = "#15803d";
        c.amb    = "#92400e";
        c.red    = "#dc2626";
        c.txt    = "#1e293b";
        c.mut    = "#64748b";
    } else {
        c.bg     = "#0e1624";
        c.card   = "#111c2e";
        c.bdr    = "#1e3a5f";
        c.accent = "#3b82f6";
        c.green  = "#4ade80";
        c.amb    = "#fbbf24";
        c.red    = "#f87171";
        c.txt    = "#e2e8f0";
        c.mut    = "#94a3b8";
    }
    return c;
}

static QString card(const BkColors& c, const QString& titolo,
                    const QString& body, const QString& color)
{
    return QString(
        "<div style='background:%1;border:1px solid %2;border-left:4px solid %3;"
        "border-radius:6px;padding:10px 14px;margin:6px 0;'>"
        "<p style='color:%3;font-weight:bold;margin:0 0 6px 0;font-size:13px;'>%4</p>"
        "<div style='color:%5;font-size:12px;line-height:1.6;'>%6</div>"
        "</div>")
        .arg(c.card, c.bdr, color, titolo, c.txt, body);
}

static QString tip(const BkColors& c, const QString& testo)
{
    return QString("<p style='color:%1;margin:3px 0;'>"
                   "\xf0\x9f\x94\xa7 %2</p>").arg(c.green, testo);
}

static QString warn(const BkColors& c, const QString& testo)
{
    return QString("<p style='color:%1;margin:3px 0;'>"
                   "\xe2\x9a\xa0 %2</p>").arg(c.amb, testo);
}

static QString step(const BkColors& c, int n, const QString& testo)
{
    return QString("<p style='margin:4px 0;'>"
                   "<b style='color:%1;'>%2.</b> %3</p>")
        .arg(c.accent).arg(n).arg(testo);
}

/* ═══════════════════════════════════════════════════════════════════ */

BikeWidget::BikeWidget(QWidget* parent) : QWidget(parent)
{
    buildUi();
    connect(ThemeManager::instance(), &ThemeManager::changed,
            this, [this](const QString&){ updateContent(); });
}

void BikeWidget::buildUi()
{
    auto* lay = new QVBoxLayout(this);
    lay->setContentsMargins(0, 0, 0, 0);
    lay->setSpacing(0);

    /* header */
    auto* hdr = new QLabel(
        "  \xf0\x9f\x9a\xb4  <b>Assistente Bici</b>"
        "  <span style='color:gray;font-size:11px;'>"
        "Problemi comuni \xc2\xb7 Cambio \xc2\xb7 Freni \xc2\xb7 Manopole \xc2\xb7 Manutenzione"
        "</span>", this);
    hdr->setTextFormat(Qt::RichText);
    hdr->setObjectName("pageHeader");
    hdr->setFixedHeight(36);
    lay->addWidget(hdr);

    /* selettori */
    auto* selRow = new QWidget(this);
    auto* selLay = new QHBoxLayout(selRow);
    selLay->setContentsMargins(10, 6, 10, 4);
    selLay->setSpacing(10);

    auto* tipoLbl = new QLabel("Tipo bici:", selRow);
    tipoLbl->setObjectName("formLabel");
    m_tipoCombo = new QComboBox(selRow);
    /* UTF-8 reali — QComboBox non interpreta HTML entities */
    m_tipoCombo->addItem("\xf0\x9f\x9a\xb2  Pieghevole",          0);
    m_tipoCombo->addItem("\xf0\x9f\x8f\x94  Mountain Bike (MTB)", 1);
    m_tipoCombo->addItem("\xf0\x9f\x9a\xb4  Cross Country (XC)",  2);
    m_tipoCombo->addItem("\xf0\x9f\x8c\xbf  Gravel / Cicloturismo", 3);
    m_tipoCombo->addItem("\xf0\x9f\x8f\x99  City / Trekking",     4);
    m_tipoCombo->setMinimumWidth(dpiScale(220));

    auto* catLbl = new QLabel("Sezione:", selRow);
    catLbl->setObjectName("formLabel");
    m_catCombo = new QComboBox(selRow);
    m_catCombo->addItem("\xf0\x9f\x94\xa7  Problemi comuni",       0);
    m_catCombo->addItem("\xe2\x9a\x99  Regolazione cambio",        1);
    m_catCombo->addItem("\xf0\x9f\x9b\x91  Regolazione freni",    2);
    m_catCombo->addItem("\xf0\x9f\x94\xa9  Manopole e attacchi",    3);
    m_catCombo->addItem("\xf0\x9f\x93\x8b  Manutenzione periodica", 4);
    m_catCombo->setMinimumWidth(dpiScale(220));

    selLay->addWidget(tipoLbl);
    selLay->addWidget(m_tipoCombo);
    selLay->addSpacing(16);
    selLay->addWidget(catLbl);
    selLay->addWidget(m_catCombo);
    selLay->addStretch();
    lay->addWidget(selRow);

    /* separatore */
    auto* sep = new QFrame(this);
    sep->setFrameShape(QFrame::HLine);
    lay->addWidget(sep);

    /* contenuto — nessun colore fisso: il background viene dall'HTML tema-aware */
    m_info = new QTextBrowser(this);
    m_info->setOpenExternalLinks(false);
    m_info->setFrameShape(QFrame::NoFrame);
    m_info->setStyleSheet(
        "QTextBrowser { border:none; padding:12px 16px; font-size:12px; }");
    lay->addWidget(m_info, 1);

    connect(m_tipoCombo, QOverload<int>::of(&QComboBox::currentIndexChanged),
            this, &BikeWidget::onSelectionChanged);
    connect(m_catCombo,  QOverload<int>::of(&QComboBox::currentIndexChanged),
            this, &BikeWidget::onSelectionChanged);

    updateContent();
}

void BikeWidget::onSelectionChanged()
{
    updateContent();
}

void BikeWidget::updateContent()
{
    const BkColors c = themeColors();
    const int cat    = m_catCombo->currentIndex();
    const int tipo   = m_tipoCombo->currentData().toInt();

    /* aggiorna stylesheet QTextBrowser in base al tema */
    m_info->setStyleSheet(
        QString("QTextBrowser { background:%1; border:none;"
                "padding:12px 16px; font-size:12px; }").arg(c.bg));

    QString html = QString(
        "<html><body style='background:%1;color:%2;font-family:sans-serif;'>")
        .arg(c.bg, c.txt);

    switch (cat) {
    case 0: {
        auto sections = sectionsProblemi(tipo, c);
        for (const auto& s : sections)
            html += s.html;
        break;
    }
    case 1: html += htmlCambio(c);         break;
    case 2: html += htmlFreni(c);          break;
    case 3: html += htmlManopole(c);       break;
    case 4: html += htmlManutenzione(c);   break;
    default: break;
    }

    html += "</body></html>";
    m_info->setHtml(html);
    m_info->moveCursor(QTextCursor::Start);
}

/* ─── Problemi comuni (varia per tipo bici) ────────────────────────── */
QList<BikeWidget::Section> BikeWidget::sectionsProblemi(int tipo, const BkColors& c)
{
    QList<Section> out;
    bool isFold  = (tipo == 0);
    bool isMTB   = (tipo == 1 || tipo == 2);
    bool isGravel= (tipo == 3);
    bool isCity  = (tipo == 4);

    out.append({"Catena", card(c, "Catena cigolante o che salta",
        tip(c, "Pulisci la catena con uno straccio asciutto e applica lubrificante specifico "
            "per catena. Non usare WD-40 come lubrificante a lungo termine.") +
        tip(c, "Controlla l'usura con un misuratore di catena (indicatore 0.5 = sostituzione consigliata).") +
        warn(c, "Catena che salta: controlla l'usura dei pignoni. Se i denti sono 'ad uncino', "
             "sostituisci pignoni + catena insieme."),
        c.accent)});

    out.append({"Gomme", card(c, "Foratura frequente / gomma che perde pressione",
        tip(c, "Controlla sempre la pressione con un manometro prima di ogni uscita.") +
        step(c, 1, "Smonta la ruota e immergi il copertone in acqua per trovare il foro.") +
        step(c, 2, "Con camera d'aria: usa le leve per smontare il copertone, sostituisci la camera.") +
        step(c, 3, "Tubeless: inietta il sigillante nel foro e ruota la ruota per distribuirlo.") +
        warn(c, "Pressione troppo bassa = rischio di 'snake bite' (doppia foratura a serpente).") +
        QString("<p style='color:%1;font-size:11px;margin-top:6px;'>"
                "<b>Pressioni consigliate:</b> Pieghevole 5-6 bar \xc2\xb7 MTB tubeless 1.5-2.5 bar \xc2\xb7 "
                "Gravel 2.5-4 bar \xc2\xb7 City 4-6 bar</p>").arg(c.mut),
        c.green)});

    out.append({"Freni", card(c, "Freni che stridono o mancano di potenza",
        tip(c, "Dischi: verifica che le pastiglie non siano contaminate da olio. "
            "Una piccola goccia di olio freni rovina la pastiglia permanentemente.") +
        tip(c, "Cantilever/V-brake: centra le ganasce e assicurati che tocchino il cerchio in piatto, "
            "non inclinati.") +
        warn(c, "Se il freno a disco scalda eccessivamente in discesa, usa la tecnica "
             "pulsata (strilla e rilascia) invece di tenere premuto costante."),
        c.red)});

    if (isFold) {
        out.append({"Piegatura", card(c, "Snodi e agganci pieghevoli",
            tip(c, "Lubrifica gli snodi ogni 3 mesi con grasso al litio. Non olio.") +
            tip(c, "Verifica il serraggio delle leve di chiusura: devono opporre resistenza "
                "ma non richiedere forza eccessiva.") +
            step(c, 1, "Chiudi la bici e verifica che non ci siano giochi laterali nel telaio.") +
            step(c, 2, "Se c'e' gioco: regola il dado di tensione dello snodo (di solito sotto la leva).") +
            warn(c, "Non superare mai il peso max indicato sul telaio (di solito 100-120 kg totale).") +
            warn(c, "Prima di ogni uscita controlla visivamente che tutti i quick-release siano chiusi."),
            c.amb)});

        out.append({"Ruote piccole", card(c, "Problemi specifici ruote piccole (16\"-20\")",
            tip(c, "Le ruote piccole amplificano le vibrazioni: usa pressioni nella fascia alta "
                "del range consigliato per ridurre la resistenza al rotolamento.") +
            tip(c, "Raggio piu' corto = raggi sotto maggiore tensione: controlla la centratura ruota "
                "ogni 6 mesi con un centratore.") +
            warn(c, "Con ruote piccole il manubrio risponde in modo piu' nervoso: impara a sterzare "
                 "con movimenti del corpo, non solo con le mani."),
            c.mut)});
    }

    if (isMTB) {
        out.append({"Ammortizzatori", card(c, "Ammortizzatore anteriore / posteriore",
            tip(c, "Controlla la pressione dell'aria (forcella e mono-ammortizzatore) "
                "con una pompa da shock. Non usare la pompa da pneumatici.") +
            step(c, 1, "Sag (affondamento statico): con il tuo peso sulla bici deve essere "
                    "25-30% (XC), 30-35% (Trail), 35-40% (Enduro).") +
            step(c, 2, "Regola la compressione (LSC/HSC) e il rimbalzo (LSR) partendo dai "
                    "valori medi consigliati dal produttore.") +
            tip(c, "Forcella che affonda troppo velocemente: aumenta la compressione (Low Speed Compression).") +
            warn(c, "Se la forcella perde olio (gocce sullo stelo): porta da un meccanico "
                 "per ricarica olio e sostituzione paraolio."),
            c.accent)});

        out.append({"Pignoni MTB", card(c, "Cassetta e guarnitura per fuoristrada",
            tip(c, "MTB moderna con 1x12: mantieni la catena pulita dopo ogni uscita nel fango.") +
            tip(c, "Se la catena cade: monta un chain guide o un anello con pins antideragliamento.") +
            warn(c, "Non usare mai un rapporto troppo duro in salita: rovinano ginocchia e guarnitura."),
            c.green)});
    }

    if (isGravel) {
        out.append({"Gravel specifico", card(c, "Consigli per gravel / cicloturismo",
            tip(c, "Tubeless consigliato: meno forature e pressioni piu' basse per miglior comfort.") +
            tip(c, "Verifica il centraggio del manubrio a goccia: i drop devono essere alla "
                "stessa altezza su entrambi i lati.") +
            warn(c, "In percorsi sterrati prolungati: controlla il serraggio di tutti i bulloni "
                 "ogni 50-100 km (le vibrazioni allentano tutto)."),
            c.amb)});
    }

    if (isCity) {
        out.append({"City specifico", card(c, "Bici da citta' e trekking",
            tip(c, "Parafanghi: controlla che non stridano sulla gomma. Regola i supporti.") +
            tip(c, "Portapacchi: il peso max e' scritto sul portapacchi (di solito 25-40 kg). "
                "Distribuisci il carico simmetricamente.") +
            tip(c, "Dinamo / luci: se la luce dinamo sfarfalla, controlla il contatto "
                "del rullo laterale con il fianco del copertone.") +
            warn(c, "Bici con cambio interno (Shimano Nexus/Alfine): non cambiare rapporto "
                 "mentre sei fermo, sempre in movimento."),
            c.mut)});
    }

    return out;
}

/* ─── Regolazione cambio ────────────────────────────────────────────── */
QString BikeWidget::htmlCambio(const BkColors& c)
{
    QString h;
    h += card(c, "Principio di funzionamento del cambio",
        "<p>Il cambio posteriore (deragliatore) e' regolato da 3 elementi:</p>"
        "<ul style='margin:4px 0 4px 18px;'>"
        "<li><b>Vite L</b> (Low) \xe2\x80\x94 limita la corsa verso il pignone piu' grande (rapporto basso)</li>"
        "<li><b>Vite H</b> (High) \xe2\x80\x94 limita la corsa verso il pignone piu' piccolo (rapporto alto)</li>"
        "<li><b>Barrel adjuster</b> (ghiera) \xe2\x80\x94 regola la tensione del cavo</li>"
        "</ul>"
        + warn(c, "Prima di regolare il cambio: lubrifica il cavo, controlla che non sia "
                  "arrugginito o con fili rotti."),
        c.accent);

    h += card(c, "Procedura completa regolazione cambio posteriore",
        step(c, 1, "Metti la catena sul pignone <b>piu' piccolo</b> e sulla corona piu' piccola.") +
        step(c, 2, "Allenta completamente la ghiera (barrel adjuster) ruotandola in senso orario "
                "fino in fondo, poi dai 2-3 giri in senso antiorario.") +
        step(c, 3, "Vite <b>H</b>: regolala finche' la puleggia di guida (rotella superiore) "
                "e' perfettamente allineata al pignone piu' piccolo. Guarda da dietro.") +
        step(c, 4, "Allenta il dado del cavo, tendi il cavo manualmente e riavvita il dado.") +
        step(c, 5, "Metti la catena sul pignone <b>piu' grande</b>.") +
        step(c, 6, "Vite <b>L</b>: regolala finche' la puleggia e' allineata al pignone piu' grande. "
                "La catena NON deve cadere oltre il pignone.") +
        step(c, 7, "Vai in posizione media e prova a scorrere tutti i rapporti. "
                "Se il cambio e' lento a salire: gira la ghiera <b>in senso antiorario</b> (aggiunge tensione). "
                "Se e' lento a scendere: gira <b>in senso orario</b>.") +
        tip(c, "Regola sempre in movimento: pedala e usa il pollice per capire il ritardo.") +
        warn(c, "Cambio che rifiuta di andare sul pignone piu' grande: vite L troppo stretta. "
             "Cambio che cade oltre il pignone grande: vite L troppo larga = rischio catena nel raggio."),
        c.green);

    h += card(c, "Regolazione cambio anteriore (se presente)",
        step(c, 1, "Metti la catena sulla corona <b>piu' grande</b>, pignone medio.") +
        step(c, 2, "Vite H: il piatto esterno del deragliatore deve essere a 1-2 mm dalla catena.") +
        step(c, 3, "Metti la catena sulla corona <b>piu' piccola</b>, pignone medio.") +
        step(c, 4, "Vite L: il piatto interno deve essere a 1-2 mm dalla catena.") +
        tip(c, "Altezza deragliatore anteriore: il bordo inferiore deve essere a 1-3 mm sopra i denti "
            "della corona piu' grande.") +
        warn(c, "Combinazioni da evitare: corona grande + pignone grande (catena troppo angolata). "
             "Corona piccola + pignone piccolo (stesso problema)."),
        c.amb);

    h += card(c, "Trucchi da meccanico",
        tip(c, "Cambio che scatta lentamente in salita: prima dell'uscita scalda il cavo con le dita "
            "per 30 secondi e lubrifica l'uscita dal cambio con una goccia d'olio.") +
        tip(c, "Ghiera di tensione persa: incolla un pezzetto di nastro isolante sulla ghiera "
            "per impedirle di girare con le vibrazioni.") +
        tip(c, "Cambio elettronico (Di2, eTap, AXS): se non scatta, controlla la carica "
            "batteria tramite l'app del produttore. Ricarica prima che arrivi al 10%.") +
        warn(c, "Bullone di fissaggio del cavo: usa un cacciavite per tenere fermo il cavo "
             "mentre stringi il bullone, altrimenti ruota e perde tensione."),
        c.accent);

    return h;
}

/* ─── Regolazione freni ────────────────────────────────────────────── */
QString BikeWidget::htmlFreni(const BkColors& c)
{
    QString h;
    h += card(c, "Freni a disco idraulici",
        step(c, 1, "Controlla il livello del liquido: il serbatoio e' sulla leva, apri il coperchio "
                "con la bici in piano. Livello min = 5 mm sotto il bordo.") +
        step(c, 2, "Spurgo: necessario quando la leva diventa spugnosa. Usa il kit Shimano/SRAM "
                "appropriato. Non mescolare mai liquidi diversi (minerale vs DOT).") +
        step(c, 3, "Centraggio disco: allenta i 2 bulloni della pinza, stringi la leva del freno "
                "e tienila stretta mentre riavviti i bulloni. Questo centra la pinza.") +
        tip(c, "Disco che striscia: ruota la ruota lentamente e ascolta dove striscia. "
            "Raddrizza il disco con le mani (usa un guanto) o con una chiave specifica.") +
        warn(c, "Non toccare mai le superfici di frizione del disco con le dita: il grasso "
             "contamina le pastiglie e non si recupera."),
        c.red);

    h += card(c, "Freni V-brake e cantilever",
        step(c, 1, "Regola la posizione delle ganasce: devono toccare il cerchio in piatto, "
                "1-2 mm sotto il bordo superiore della flangia.") +
        step(c, 2, "Centraggio molla: la piccola vite laterale sul braccio del freno regola "
                "la tensione della molla. Stringe = il braccio si allontana dal cerchio.") +
        step(c, 3, "Distanza dalla leva: regola il noodle (il tubo metallico del V-brake) "
                "per avere 2-4 mm di gioco prima che il freno morda.") +
        tip(c, "Freno che sfrega da un solo lato: regola la vite di tensione molla sul lato "
            "che non sfrega (aumenta la tensione per allontanarla)."),
        c.amb);

    return h;
}

/* ─── Manopole e attacchi ───────────────────────────────────────────── */
QString BikeWidget::htmlManopole(const BkColors& c)
{
    QString h;
    h += card(c, "Manopole che fanno gioco / ruotano",
        "<p>Le manopole che scivolano sono pericolose. Ecco come fissarle definitivamente:</p>" +
        step(c, 1, "Rimuovi la manopola vecchia: usa un cacciavite piatto sotto il bordo e "
                "soffia aria compressa per sfilare. Oppure taglia e sostituisci.") +
        step(c, 2, "Pulisci il manubrio con alcol isopropilico (90%+). Lascia asciugare 2 minuti.") +
        step(c, 3, "Applica lacca da capelli all'interno della nuova manopola e sul manubrio. "
                "Infila rapidamente la manopola prima che asciughi.") +
        tip(c, "Alternativa professionale: usa il collante specifico per manopole o hair spray "
            "Extra Strong. Non usare grasso o WD-40 (fanno scivolare ancora di piu').") +
        tip(c, "Manopole con vite di bloccaggio (lock-on): stretta della vite a T20 o T25 con "
            "coppia 1-2 Nm. Non esagerare: il collar si rompe.") +
        warn(c, "Se la manopola ruota anche con la vite stretta: il manubrio potrebbe essere "
             "consumato. Avvolgi con nastro biadesivo sottile prima di rimontare la manopola."),
        c.accent);

    h += card(c, "Manubrio che fa gioco sullo stem (attacco manubrio)",
        step(c, 1, "Identifica il tipo di attacco: threadless (moderno, 2-4 bulloni faceplate) "
                "o filettato (vecchio, dado sopra la forcella).") +
        QString("<p style='color:%1;font-weight:bold;margin:6px 0 4px;'>"
                "Threadless (faceplate):</p>").arg(c.amb) +
        step(c, 2, "Allenta i bulloni del faceplate e riposiziona il manubrio. "
                "Stringa in croce con coppia 5-6 Nm (alluminio) o 4-5 Nm (carbonio).") +
        step(c, 3, "Verifica il gap faceplate: deve essere uniforme sopra e sotto.") +
        QString("<p style='color:%1;font-weight:bold;margin:6px 0 4px;'>"
                "Filettato (vecchio):</p>").arg(c.amb) +
        step(c, 4, "Stacca il dado espandente (bullone al centro del manubrio). "
                "Batti leggermente con un martelletto per liberare l'espandente.") +
        step(c, 5, "Riallinea il manubrio con la ruota anteriore e ristrigi il bullone espandente.") +
        tip(c, "Test sicurezza: blocca la ruota anteriore tra le gambe e prova a ruotare il "
            "manubrio con forza. Non deve muoversi di un millimetro.") +
        warn(c, "Carbonio: usa sempre un torsiometro. Coppie tipiche: 4 Nm."),
        c.green);

    h += card(c, "Leve freno e cambio che ruotano",
        step(c, 1, "Identifica il bullone di serraggio: di solito un esagonale M5 sul collare "
                "della leva (chiave a brugola 4 o 5 mm).") +
        step(c, 2, "Allenta, posiziona la leva all'angolo corretto (indice e medio devono "
                "raggiungere la leva comodamente senza spostare le mani).") +
        step(c, 3, "Regola l'angolo: circa 45 gradi rispetto al manubrio e' un buon punto "
                "di partenza. Regola in base alla tua anatomia.") +
        tip(c, "Se il bullone si allenta continuamente: usa Loctite 243 (media forza) "
            "sul filetto. Basta una piccola goccia.") +
        warn(c, "Non serrare eccessivamente: il collare in alluminio si deforma. "
             "Coppia massima 5-6 Nm."),
        c.amb);

    h += card(c, "Sella che scende o ruota",
        step(c, 1, "Reggisella: strigi il morsetto con la corretta coppia (5-8 Nm per alluminio). "
                "Usa un segno con pennarello per sapere se si e' abbassato.") +
        step(c, 2, "Testa sella (rotazione): strigi i 2 bulloni sotto la sella in alternanza "
                "e in modo uniforme.") +
        tip(c, "Sella che scende anche con il morsetto stretto: il reggisella potrebbe essere "
            "consumato (ovalizzato). Sostituiscilo o usa del nastro di rame come shim.") +
        tip(c, "Reggisella telescopico (dropper): se non sale o non scende bene, "
            "lubrifica il cavo e pulisci la sede del reggisella con alcol."),
        c.mut);

    return h;
}

/* ─── Manutenzione periodica ────────────────────────────────────────── */
QString BikeWidget::htmlManutenzione(const BkColors& c)
{
    QString h;

    const QString li = QString("<li style='color:%1;margin:3px 0;'>").arg(c.txt);
    const QString ul = "<ul style='margin:4px 0 4px 18px;'>";

    h += card(c, "Ogni uscita (2 minuti)",
        ul + li + "Premi i freni: deve frenare con forza prima di toccare il manubrio</li>"
        + li + "Ruota le ruote: nessun attrito, nessun rumore</li>"
        + li + "Controlla la pressione delle gomme (a occhio / manometro)</li>"
        + li + "Bici pieghevole: verifica che tutti gli snodi siano bloccati</li></ul>",
        c.green);

    h += card(c, "Ogni settimana / 100 km",
        ul + li + "Lubrifica la catena (sgrassore + lubrificante specifico)</li>"
        + li + "Pulisci cerchi/dischi con panno alcol</li>"
        + li + "Controlla usura pastiglie freno (spessore minimo 1 mm)</li>"
        + li + "Verifica serraggio bulloni manubrio e sella</li></ul>",
        c.accent);

    h += card(c, "Ogni mese / 500 km",
        ul + li + "Controlla tensione raggi (risuonano tutti allo stesso modo)</li>"
        + li + "Verifica centratura ruote</li>"
        + li + "Lubrifica cavi (filo + guaina, usa olio sottile)</li>"
        + li + "Controlla usura catena con apposito strumento</li>"
        + li + "MTB: controlla pressione e tenuta ammortizzatori</li></ul>",
        c.amb);

    h += card(c, "Ogni anno / 3000 km",
        ul + li + "Sostituzione catena (se misuratore indica 0.5-0.75)</li>"
        + li + "Revisione movimento centrale (cuscinetti)</li>"
        + li + "Revisione sterzo (cuscinetti serie sterzo)</li>"
        + li + "Revisione mozzi (lubrificazione cuscinetti)</li>"
        + li + "MTB: revisione forcella (olio e paraolio) e ammortizzatore</li>"
        + li + "Sostituzione cavi e guaine se rigidi o arrugginiti</li></ul>",
        c.red);

    h += card(c, "Kit base indispensabile per casa",
        tip(c, "Set chiavi brugola (2-8 mm) \xe2\x80\x94 indispensabile per tutto") +
        tip(c, "Chiave dinamometrica \xe2\x80\x94 evita di rompere bulloni in carbonio/alluminio") +
        tip(c, "Misuratore di catena (2 euro) \xe2\x80\x94 sai quando sostituirla") +
        tip(c, "Sgrassatore + lubrificante catena (bio-degradabile per uso regolare)") +
        tip(c, "Camera d'aria di ricambio + leve smontacopertone + co2 o pompa") +
        tip(c, "Multitool da bici con brugole e cacciavite") +
        warn(c, "Torsiometro: obbligatorio se hai parti in carbonio"),
        c.mut);

    return h;
}
