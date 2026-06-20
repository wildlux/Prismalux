#include "widget_bike.h"
#include "../dpi_utils.h"
#include <QVBoxLayout>
#include <QHBoxLayout>
#include <QComboBox>
#include <QTextBrowser>
#include <QLabel>
#include <QGroupBox>
#include <QScrollArea>
#include <QFrame>
#include <QRegularExpression>
#include <QTextCursor>

/* ─── palette colori coerente col tema ─────────────────────────────── */
static const char* kBg     = "#0e1624";
static const char* kCard   = "#111c2e";
static const char* kBdr    = "#1e3a5f";
static const char* kAccent = "#3b82f6";
static const char* kGreen  = "#4ade80";
static const char* kAmb    = "#fbbf24";
static const char* kRed    = "#f87171";
static const char* kTxt    = "#e2e8f0";
static const char* kMut    = "#94a3b8";

static QString card(const QString& titolo, const QString& body,
                    const char* color = "#3b82f6")
{
    return QString(
        "<div style='background:%1;border:1px solid %2;border-left:4px solid %3;"
        "border-radius:6px;padding:10px 14px;margin:6px 0;'>"
        "<p style='color:%3;font-weight:bold;margin:0 0 6px 0;font-size:13px;'>%4</p>"
        "<div style='color:%5;font-size:12px;line-height:1.6;'>%6</div>"
        "</div>")
        .arg(kCard).arg(kBdr).arg(color).arg(titolo).arg(kTxt).arg(body);
}

static QString tip(const QString& testo)
{
    return QString("<p style='color:%1;margin:3px 0;'>"
                   "&#128295; %2</p>").arg(kGreen).arg(testo);
}

static QString warn(const QString& testo)
{
    return QString("<p style='color:%1;margin:3px 0;'>"
                   "&#9888; %2</p>").arg(kAmb).arg(testo);
}

static QString step(int n, const QString& testo)
{
    return QString("<p style='margin:4px 0;'>"
                   "<b style='color:%1;'>%2.</b> %3</p>")
        .arg(kAccent).arg(n).arg(testo);
}

/* ═══════════════════════════════════════════════════════════════════ */

BikeWidget::BikeWidget(QWidget* parent) : QWidget(parent)
{
    buildUi();
}

void BikeWidget::buildUi()
{
    auto* lay = new QVBoxLayout(this);
    lay->setContentsMargins(0, 0, 0, 0);
    lay->setSpacing(0);

    /* header */
    auto* hdr = new QLabel(
        "  &#128690;  <b>Assistente Bici</b>"
        "  <span style='color:gray;font-size:11px;'>"
        "Problemi comuni &#183; Cambio &#183; Manopole &#183; Manutenzione"
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
    tipoLbl->setStyleSheet(QString("color:%1;font-size:12px;").arg(kMut));
    m_tipoCombo = new QComboBox(selRow);
    m_tipoCombo->addItems({
        "&#128690; Pieghevole",
        "&#127956; Mountain Bike (MTB)",
        "&#128692; Cross Country (XC)",
        "&#127807; Gravel / Cicloturismo",
        "&#127968; City / Trekking"
    });
    m_tipoCombo->setMinimumWidth(dpiScale(200));

    auto* catLbl = new QLabel("Sezione:", selRow);
    catLbl->setStyleSheet(QString("color:%1;font-size:12px;").arg(kMut));
    m_catCombo = new QComboBox(selRow);
    m_catCombo->addItems({
        "&#128295; Problemi comuni",
        "&#9881; Regolazione cambio",
        "&#128200; Regolazione freni",
        "&#9000; Manopole e attacchi",
        "&#128203; Manutenzione periodica"
    });
    m_catCombo->setMinimumWidth(dpiScale(200));

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
    sep->setStyleSheet(QString("color:%1;").arg(kBdr));
    lay->addWidget(sep);

    /* contenuto */
    m_info = new QTextBrowser(this);
    m_info->setOpenExternalLinks(false);
    m_info->setStyleSheet(
        QString("QTextBrowser { background:%1; color:%2; border:none;"
                "padding:12px 16px; font-size:12px; }").arg(kBg).arg(kTxt));
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
    const int cat  = m_catCombo->currentIndex();
    QString tipo = m_tipoCombo->currentText();
    tipo.remove(QRegularExpression("<[^>]*>"));
    tipo = tipo.trimmed();

    QString html = QString(
        "<html><body style='background:%1;color:%2;font-family:sans-serif;'>")
        .arg(kBg).arg(kTxt);

    switch (cat) {
    case 0: {
        auto sections = sectionsProblemi(tipo);
        for (const auto& s : sections)
            html += s.html;
        break;
    }
    case 1: html += htmlCambio();      break;
    case 2: html += htmlFreni();       break;
    case 3: html += htmlManopole();    break;
    case 4: html += htmlManutenzione(); break;
    default: break;
    }

    html += "</body></html>";
    m_info->setHtml(html);
    m_info->moveCursor(QTextCursor::Start);
}

/* ─── Problemi comuni (varia per tipo bici) ────────────────────────── */
QList<BikeWidget::Section> BikeWidget::sectionsProblemi(const QString& tipo)
{
    QList<Section> out;
    bool isFold  = tipo.contains("Pieghevole");
    bool isMTB   = tipo.contains("Mountain") || tipo.contains("XC")
                || tipo.contains("Cross");
    bool isGravel= tipo.contains("Gravel");
    bool isCity  = tipo.contains("City");

    /* Problemi comuni a tutte le bici */
    out.append({"Catena", card("Catena cigolante o che salta",
        tip("Pulisci la catena con uno straccio asciutto e applica lubrificante specifico "
            "per catena. Non usare WD-40 come lubrificante a lungo termine.") +
        tip("Controlla l'usura con un misuratore di catena (indicatore 0.5 = sostituzione consigliata).") +
        warn("Catena che salta: controlla l'usura dei pignoni. Se i denti sono 'ad uncino', "
             "sostituisci pignoni + catena insieme."),
        kAccent)});

    out.append({"Gomme", card("Foratura frequente / gomma che perde pressione",
        tip("Controlla sempre la pressione con un manometro prima di ogni uscita.") +
        step(1, "Smonta la ruota e immergi il copertone in acqua per trovare il foro.") +
        step(2, "Con camera d'aria: usa le leve per smontare il copertone, sostituisci la camera.") +
        step(3, "Tubeless: inietta il sigillante nel foro e ruota la ruota per distribuirlo.") +
        warn("Pressione troppo bassa = rischio di 'snake bite' (doppia foratura a serpente).") +
        "<p style='color:" + QString(kMut) + ";font-size:11px;margin-top:6px;'>"
        "<b>Pressioni consigliate:</b> Pieghevole 5-6 bar &#183; MTB tubeless 1.5-2.5 bar &#183; "
        "Gravel 2.5-4 bar &#183; City 4-6 bar</p>",
        kGreen)});

    out.append({"Freni", card("Freni che stridono o mancano di potenza",
        tip("Dischi: verifica che le pastiglie non siano contaminate da olio. "
            "Una piccola goccia di olio freni rovina la pastiglia permanentemente.") +
        tip("Cantilever/V-brake: centra le ganasce e assicurati che tocchino il cerchio in piatto, "
            "non inclinati.") +
        warn("Se il freno a disco scalda eccessivamente in discesa, usa la tecnica "
             "pulsata (strilla e rilascia) invece di tenere premuto costante."),
        kRed)});

    if (isFold) {
        out.append({"Piegatura", card("Snodi e agganci pieghevoli",
            tip("Lubrifica gli snodi ogni 3 mesi con grasso al litio. Non olio.") +
            tip("Verifica il serraggio delle leve di chiusura: devono opporre resistenza "
                "ma non richiedere forza eccessiva.") +
            step(1, "Chiudi la bici e verifica che non ci siano giochi laterali nel telaio.") +
            step(2, "Se c'e' gioco: regola il dado di tensione dello snodo (di solito sotto la leva).") +
            warn("Non superare mai il peso max indicato sul telaio (di solito 100-120 kg totale).") +
            warn("Prima di ogni uscita controlla visivamente che tutti i quick-release siano chiusi."),
            kAmb)});

        out.append({"Ruote piccole", card("Problemi specifici ruote piccole (16\"-20\")",
            tip("Le ruote piccole amplificano le vibrazioni: usa pressioni nella fascia alta "
                "del range consigliato per ridurre la resistenza al rotolamento.") +
            tip("Raggio più corto = raggi sotto maggiore tensione: controlla la centratura ruota "
                "ogni 6 mesi con un centratore.") +
            warn("Con ruote piccole il manubrio risponde in modo piu' nervoso: impara a sterzare "
                 "con movimenti del corpo, non solo con le mani."),
            kMut)});
    }

    if (isMTB) {
        out.append({"Ammortizzatori", card("Ammortizzatore anteriore / posteriore",
            tip("Controlla la pressione dell'aria (forcella e mono-ammortizzatore) "
                "con una pompa da shock. Non usare la pompa da pneumatici.") +
            step(1, "Sag (affondamento statico): con il tuo peso sulla bici deve essere "
                    "25-30% (XC), 30-35% (Trail), 35-40% (Enduro).") +
            step(2, "Regola la compressione (LSC/HSC) e il rimbalzo (LSR) partendo dai "
                    "valori medi consigliati dal produttore.") +
            tip("Forcella che affonda troppo velocemente: aumenta la compressione (Low Speed Compression).") +
            warn("Se la forcella perde olio (gocce sullo stelo): porta da un meccanico "
                 "per ricarica olio e sostituzione paraolio."),
            kAccent)});

        out.append({"Pignoni MTB", card("Cassetta e guarnitura per fuoristrada",
            tip("MTB moderna con 1x12: mantieni la catena pulita dopo ogni uscita nel fango.") +
            tip("Se la catena cade: monta un chain guide o un anello con pins antideragliamento.") +
            warn("Non usare mai un rapporto troppo duro in salita: rovinano ginocchia e guarnitura."),
            kGreen)});
    }

    if (isGravel) {
        out.append({"Gravel specifico", card("Consigli per gravel / cicloturismo",
            tip("Tubeless consigliato: meno forature e pressioni piu' basse per miglior comfort.") +
            tip("Verifica il centraggio del manubrio a goccia: i drop devono essere alla "
                "stessa altezza su entrambi i lati.") +
            warn("In percorsi sterrati prolungati: controlla il serraggio di tutti i bulloni "
                 "ogni 50-100 km (le vibrazioni allentano tutto)."),
            kAmb)});
    }

    if (isCity) {
        out.append({"City specifico", card("Bici da citta' e trekking",
            tip("Parafanghi: controlla che non stridano sulla gomma. Regola i supporti.") +
            tip("Portapacchi: il peso max e' scritto sul portapacchi (di solito 25-40 kg). "
                "Distribuisci il carico simmetricamente.") +
            tip("Dinamo / luci: se la luce dinamo sfarfalla, controlla il contatto "
                "del rullo laterale con il fianco del copertone.") +
            warn("Bici con cambio interno (Shimano Nexus/Alfine): non cambiare rapporto "
                 "mentre sei fermo, sempre in movimento."),
            kMut)});
    }

    return out;
}

/* ─── Regolazione cambio ────────────────────────────────────────────── */
QString BikeWidget::htmlCambio()
{
    QString h;
    h += card("Principio di funzionamento del cambio",
        "<p>Il cambio posteriore (deragliatore) e' regolato da 3 elementi:</p>"
        "<ul style='margin:4px 0 4px 18px;'>"
        "<li><b>Vite L</b> (Low) — limita la corsa verso il pignone piu' grande (rapporto basso)</li>"
        "<li><b>Vite H</b> (High) — limita la corsa verso il pignone piu' piccolo (rapporto alto)</li>"
        "<li><b>Barrel adjuster</b> (ghiera) — regola la tensione del cavo</li>"
        "</ul>"
        "<p style='color:" + QString(kAmb) + ";'>Prima di regolare il cambio: "
        "lubrifica il cavo, controlla che non sia arrugginito o con fili rotti.</p>",
        kAccent);

    h += card("Procedura completa regolazione cambio posteriore",
        step(1, "Metti la catena sul pignone <b>piu' piccolo</b> e sulla corona piu' piccola.") +
        step(2, "Allenta completamente la ghiera (barrel adjuster) ruotandola in senso orario "
                "fino in fondo, poi dai 2-3 giri in senso antiorario.") +
        step(3, "Vite <b>H</b>: regolala finche' la puleggia di guida (rotella superiore) "
                "e' perfettamente allineata al pignone piu' piccolo. Guarda da dietro.") +
        step(4, "Allenta il dado del cavo, tendi il cavo manualmente e riavvita il dado.") +
        step(5, "Metti la catena sul pignone <b>piu' grande</b>.") +
        step(6, "Vite <b>L</b>: regolala finche' la puleggia e' allineata al pignone piu' grande. "
                "La catena NON deve cadere oltre il pignone.") +
        step(7, "Vai in posizione media e prova a scorrere tutti i rapporti. "
                "Se il cambio e' lento a salire (verso pignoni grandi): gira la ghiera "
                "<b>in senso antiorario</b> (aggiunge tensione). "
                "Se e' lento a scendere: gira <b>in senso orario</b>.") +
        tip("Regola sempre in movimento: pedala e usa il pollice per capire il ritardo.") +
        warn("Cambio che rifiuta di andare sul pignone piu' grande: vite L troppo stretta. "
             "Cambio che cade oltre il pignone grande: vite L troppo larga = rischio catena nel raggio."),
        kGreen);

    h += card("Regolazione cambio anteriore (se presente)",
        step(1, "Metti la catena sulla corona <b>piu' grande</b>, pignone medio.") +
        step(2, "Vite H: il piatto esterno del deragliatore deve essere a 1-2 mm dalla catena.") +
        step(3, "Metti la catena sulla corona <b>piu' piccola</b>, pignone medio.") +
        step(4, "Vite L: il piatto interno deve essere a 1-2 mm dalla catena.") +
        tip("Altezza deragliatore anteriore: il bordo inferiore deve essere a 1-3 mm sopra i denti "
            "della corona piu' grande.") +
        warn("Combinazioni da evitare: corona grande + pignone grande (catena troppo angolata). "
             "Corona piccola + pignone piccolo (stesso problema)."),
        kAmb);

    h += card("Trucchi da meccanico",
        tip("Cambio che scatta lentamente in salita (MTB / gravel): "
            "prima dell'uscita scalda il cavo con le dita per 30 secondi e poi lubrifica l'uscita "
            "dal cambio con una goccia d'olio.") +
        tip("Ghiera di tensione persa: incolla un pezzetto di nastro isolante sulla ghiera "
            "per impedirle di girare con le vibrazioni.") +
        tip("Cambio elettronico (Di2, eTap, AXS): se non scatta, controlla la carica "
            "batteria tramite l'app del produttore. Ricarica prima che arrivi al 10%.") +
        warn("Bullone di fissaggio del cavo al cambio: usa un cacciavite per tenere fermo il "
             "cavo mentre stringi il bullone, altrimenti il cavo ruota e perde tensione."),
        kAccent);

    return h;
}

/* ─── Regolazione freni ────────────────────────────────────────────── */
QString BikeWidget::htmlFreni()
{
    QString h;
    h += card("Freni a disco idraulici",
        step(1, "Controlla il livello del liquido: il serbatoio e' sulla leva, apri il coperchio "
                "con la bici in piano. Livello min = 5 mm sotto il bordo.") +
        step(2, "Spurgo: necessario quando la leva diventa spugnosa. Usa il kit Shimano/SRAM "
                "appropriato. Non mescolare mai liquidi diversi (minerale vs DOT).") +
        step(3, "Centraggio disco: allenta i 2 bulloni della pinza, stringi la leva del freno "
                "e tienila stretta mentre riavviti i bulloni. Questo centra la pinza.") +
        tip("Disco che striscia: ruota la ruota lentamente e ascolta dove striscia. "
            "Raddrizza il disco con le mani (usa un guanto) o con una chiave specifica.") +
        warn("Non toccare mai le superfici di frizione del disco con le dita: il grasso "
             "contamina le pastiglie e non si recupera."),
        kRed);

    h += card("Freni V-brake e cantilever",
        step(1, "Regola la posizione delle ganasce: devono toccare il cerchio in piatto, "
                "1-2 mm sotto il bordo superiore della flangia.") +
        step(2, "Centraggio molla: la piccola vite laterale sul braccio del freno regola "
                "la tensione della molla. Stringe = il braccio si allontana dal cerchio.") +
        step(3, "Distanza dalla leva: regola il noodle (il tubo metallico del V-brake) "
                "per avere 2-4 mm di gioco prima che il freno morda.") +
        tip("Freno che sfrega da un solo lato: regola la vite di tensione molla sul lato "
            "che non sfrega (aumenta la tensione per allontanarla)."),
        kAmb);

    return h;
}

/* ─── Manopole e attacchi ───────────────────────────────────────────── */
QString BikeWidget::htmlManopole()
{
    QString h;
    h += card("Manopole che fanno gioco / ruotano",
        "<p>Le manopole che scivolano sono pericolose. Ecco come fissarle definitivamente:</p>" +
        step(1, "Rimuovi la manopola vecchia: usa un cacciavite piatto sotto il bordo e "
                "soffia aria compressa per sfilare. Oppure taglia e sostituisci.") +
        step(2, "Pulisci il manubrio con alcol isopropilico (90%+). Lascia asciugare 2 minuti.") +
        step(3, "Applica lacca da capelli all'interno della nuova manopola e sul manubrio. "
                "Infila rapidamente la manopola prima che asciughi.") +
        tip("Alternativa professionale: usa il collante specifico per manopole o hair spray "
            "Extra Strong. Non usare grasso o WD-40 (fanno scivolare ancora di piu').") +
        tip("Manopole con vite di bloccaggio (lock-on): stretta della vite a T20 o T25 con "
            "coppia 1-2 Nm. Non esagerare: il collar si rompe.") +
        warn("Se la manopola ruota anche con la vite stretta: il manubrio potrebbe essere "
             "consumato. Avvolgi con nastro biadesivo sottile prima di rimontare la manopola."),
        kAccent);

    h += card("Manubrio che fa gioco sullo stem (attacco manubrio)",
        step(1, "Identifica il tipo di attacco: threadless (moderno, 2-4 bulloni faceplate) "
                "o filettato (vecchio, dado sopra la forcella).") +
        "<p style='color:" + QString(kAmb) + ";font-weight:bold;margin:6px 0 4px;'>Threadless (faceplate):</p>" +
        step(2, "Allenta i bulloni del faceplate e riposiziona il manubrio. "
                "Stringa in croce con coppia 5-6 Nm (alluminio) o 4-5 Nm (carbonio).") +
        step(3, "Verifica il gap faceplate: deve essere uniforme sopra e sotto (non deve "
                "toccare da un lato prima dell'altro).") +
        "<p style='color:" + QString(kAmb) + ";font-weight:bold;margin:6px 0 4px;'>Filettato (vecchio):</p>" +
        step(4, "Stacca il dado espandente (bullone al centro del manubrio). "
                "Batti leggermente con un martelletto per liberare l'espandente.") +
        step(5, "Riallinea il manubrio con la ruota anteriore e ristrigi il bullone espandente.") +
        tip("Test sicurezza: blocca la ruota anteriore tra le gambe e prova a ruotare il "
            "manubrio con forza. Non deve muoversi di un millimetro.") +
        warn("Carbonio: usa sempre un torsiometro. Stringere a mano spesso non basta o "
             "danneggia il materiale. Coppie tipiche: 4 Nm."),
        kGreen);

    h += card("Leve freno e cambio che ruotano",
        step(1, "Identifica il bullone di serraggio: di solito un esagonale M5 sul collare "
                "della leva (chiave a brugola 4 o 5 mm).") +
        step(2, "Allenta, posiziona la leva all'angolo corretto (indice e medio devono "
                "raggiungere la leva comodamente senza spostare le mani).") +
        step(3, "Regola l'angolo: circa 45 gradi rispetto al manubrio e' un buon punto "
                "di partenza. Regola in base alla tua anatomia.") +
        tip("Se il bullone si allenta continuamente: usa Loctite 243 (media forza) "
            "sul filetto. Basta una piccola goccia.") +
        warn("Non serrare eccessivamente: il collare in alluminio si deforma. "
             "Coppia massima 5-6 Nm."),
        kAmb);

    h += card("Sella che scende o ruota",
        step(1, "Reggisella: strigi il morsetto con la corretta coppia (5-8 Nm per alluminio). "
                "Usa un segno con pennarello per sapere se si e' abbassato.") +
        step(2, "Testa sella (rotazione): strigi i 2 bulloni sotto la sella in alternanza "
                "e in modo uniforme. Su selle con rail ovalizzati usa la chiave corretta.") +
        tip("Sella che scende anche con il morsetto stretto: il reggisella potrebbe essere "
            "consumato (ovalizzato). Sostituiscilo o usa del nastro di rame come shim.") +
        tip("Reggisella telescopico (dropper): se non sale o non scende bene, "
            "lubrifica il cavo e pulisci la sede del reggisella con alcol."),
        kMut);

    return h;
}

/* ─── Manutenzione periodica ────────────────────────────────────────── */
QString BikeWidget::htmlManutenzione()
{
    QString h;

    h += card("Ogni uscita (2 minuti)",
        "<ul style='margin:4px 0 4px 18px;color:" + QString(kTxt) + ";'>"
        "<li>Premi i freni: deve frenare con forza prima di toccare il manubrio</li>"
        "<li>Ruota le ruote: nessun attrito, nessun rumore</li>"
        "<li>Controlla la pressione delle gomme (a occhio / manometro)</li>"
        "<li>Bici pieghevole: verifica che tutti gli snodi siano bloccati</li>"
        "</ul>",
        kGreen);

    h += card("Ogni settimana / 100 km",
        "<ul style='margin:4px 0 4px 18px;color:" + QString(kTxt) + ";'>"
        "<li>Lubrifica la catena (sgrassore + lubrificante specifico)</li>"
        "<li>Pulisci cerchi/dischi con panno alcol</li>"
        "<li>Controlla usura pastiglie freno (spessore minimo 1 mm)</li>"
        "<li>Verifica serraggio bulloni manubrio e sella</li>"
        "</ul>",
        kAccent);

    h += card("Ogni mese / 500 km",
        "<ul style='margin:4px 0 4px 18px;color:" + QString(kTxt) + ";'>"
        "<li>Controlla tensione raggi (risuonano tutti allo stesso modo)</li>"
        "<li>Verifica centratura ruote</li>"
        "<li>Lubrifica cavi (filo + guaina, usa olio sottile)</li>"
        "<li>Controlla usura catena con apposito strumento</li>"
        "<li>MTB: controlla pressione e tenuta ammortizzatori</li>"
        "</ul>",
        kAmb);

    h += card("Ogni anno / 3000 km",
        "<ul style='margin:4px 0 4px 18px;color:" + QString(kTxt) + ";'>"
        "<li>Sostituzione catena (se misuratore indica 0.5-0.75)</li>"
        "<li>Revisione movimento centrale (cuscinetti)</li>"
        "<li>Revisione sterzo (cuscinetti serie sterzo)</li>"
        "<li>Revisione mozzi (lubrificazione cuscinetti)</li>"
        "<li>MTB: revisione forcella (olio e paraolio) e ammortizzatore</li>"
        "<li>Sostituzione cavi e guaine se rigidi o arrugginiti</li>"
        "</ul>",
        kRed);

    h += card("Kit base indispensabile per casa",
        tip("Set chiavi brugola (2-8 mm) — indispensabile per tutto") +
        tip("Chiave dinamometrica — evita di rompere bulloni in carbonio/alluminio") +
        tip("Misuratore di catena (2 euro) — sai quando sostituirla") +
        tip("Sgrassatore + lubrificante catena (bio-degradabile per uso regolare)") +
        tip("Camera d'aria di ricambio + leve smontacopertone + co2 o pompa") +
        tip("Multitool da bici con brugole e cacciavite") +
        warn("Torsiometro: obbligatorio se hai parti in carbonio"),
        kMut);

    return h;
}
