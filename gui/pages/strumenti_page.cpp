#include "strumenti_page.h"
#include "../prismalux_paths.h"
namespace P = PrismaluxPaths;
#include <QVBoxLayout>
#include <QHBoxLayout>
#include <QFrame>
#include <QButtonGroup>
#include <QStackedWidget>
#include <QFileDialog>
#include <QProcess>
#include <QFileInfo>
#include <QNetworkRequest>
#include <QNetworkReply>
#include <QNetworkInterface>
#include <QHostAddress>
#include <QJsonObject>
#include <QJsonDocument>
#include <QUrl>
#include <QTimer>
#include <QTcpSocket>
#include <QDir>
#include <QFile>
#include <QTextStream>
#include <QCoreApplication>
#include <QDialog>
#include <QTextBrowser>
#include <QScrollBar>
#include <QCheckBox>
#include <QRegularExpression>
#include <QHeaderView>

/* ══════════════════════════════════════════════════════════════
   Tabella system prompt: [navIdx][subIdx]
   ══════════════════════════════════════════════════════════════ */
static const char* kSysPrompts[10][10] = {
    /* 0 — Studio */
    {
        "Sei un tutor esperto. Spiega in modo chiaro e strutturato con esempi pratici. Rispondi in italiano.",
        "Crea 10 flashcard nel formato Q: ... R: ... dal testo fornito. Rispondi in italiano.",
        "Crea un riassunto breve del testo. Rispondi in italiano.",
        "Genera 10 domande d'esame probabili con risposta sintetica. Rispondi in italiano.",
        "Crea una mappa concettuale in formato albero ASCII del concetto o testo fornito. Rispondi in italiano.",
        "Crea 5 esercizi pratici progressivi con soluzione. Rispondi in italiano.",
        nullptr, nullptr
    },
    /* 1 — Scrittura Creativa */
    {
        "Sei uno scrittore creativo. Scrivi una storia coinvolgente basata sull'idea fornita. Rispondi in italiano.",
        "Continua la storia in modo coerente con lo stile e i personaggi esistenti. Rispondi in italiano.",
        "Crea un personaggio dettagliato: nome, aspetto, backstory, motivazioni, difetti e punti di forza. Rispondi in italiano.",
        "Scrivi una poesia originale sul tema indicato. Rispondi in italiano.",
        "Scrivi un dialogo naturale e coinvolgente tra personaggi in base alla scena descritta. Rispondi in italiano.",
        "Sviluppa una trama in 3 atti: setup, confronto, risoluzione. Rispondi in italiano.",
        nullptr, nullptr
    },
    /* 2 — Ricerca */
    {
        "Fai una ricerca approfondita sull'argomento. Struttura: panoramica, punti chiave, dettagli, fonti consigliate. Rispondi in italiano.",
        "Confronta le opzioni indicate con una tabella pro/contro per ciascuna. Rispondi in italiano.",
        "Verifica la veridicita' dell'affermazione. Indica: VERO/FALSO/PARZIALMENTE VERO con spiegazione. Rispondi in italiano.",
        "Genera una bibliografia in formato APA sull'argomento con 5-10 fonti plausibili. Rispondi in italiano.",
        "Analizza il problema da almeno 4 prospettive diverse (economica, sociale, tecnica, etica). Rispondi in italiano.",
        "Crea una guida passo-passo dettagliata. Rispondi in italiano.",
        nullptr, nullptr
    },
    /* 3 — Libri */
    {
        "Analizza il testo: temi principali, stile, struttura narrativa, simbolismi. Rispondi in italiano.",
        "Crea un riassunto strutturato per capitoli o sezioni del testo. Rispondi in italiano.",
        "Analizza i personaggi principali: caratterizzazione, archi narrativi, relazioni. Rispondi in italiano.",
        "Crea una scheda libro completa: titolo, autore, anno, genere, trama, temi, punti di forza/debolezza, voto /10. Rispondi in italiano.",
        "Genera 8 domande aperte per una discussione critica del libro. Rispondi in italiano.",
        "Suggerisci 5 libri simili per temi e stile, con breve motivazione. Rispondi in italiano.",
        nullptr, nullptr
    },
    /* 4 — Produttivita' */
    {
        "Crea un piano progetto dettagliato: obiettivi, WBS, milestone, rischi, risorse. Rispondi in italiano.",
        "Organizza i task in Must/Should/Could/Won't (MoSCoW) con priorita' e tempi stimati. Rispondi in italiano.",
        "Scrivi un'email professionale chiara e convincente basata sul briefing fornito. Rispondi in italiano.",
        "Crea un'agenda dettagliata con punti di discussione, tempi e obiettivi per la riunione. Rispondi in italiano.",
        "Fai un brainstorming usando la tecnica dei 6 cappelli di de Bono sull'argomento. Rispondi in italiano.",
        "Trasforma l'obiettivo vago in 3-5 obiettivi SMART. Rispondi in italiano.",
        "Crea una matrice decisionale con criteri ponderati per le opzioni indicate. Rispondi in italiano.",
        "Sei un assistente per la comunicazione medica. L'utente ti fornir\xc3\xa0 un testo emotivo o ansioso da inviare al medico. "
        "Riscrivilo in modo professionale, formale e privo di emotivit\xc3\xa0 eccessiva, mantenendo tutte le informazioni cliniche importanti. "
        "Il testo deve essere chiaro, conciso e appropriato per una comunicazione medica. "
        "Se il testo originale contiene paura, panico o drammatizzazione, sostituiscili con descrizioni oggettive dei sintomi. "
        "Rispondi in italiano.",
        "Sei un esperto di analisi del linguaggio emotivo e psicologia cognitiva. "
        "Analizza il testo o la situazione descritta dall'utente: identifica le emozioni predominanti, il livello di stress o ansia, "
        "eventuali distorsioni cognitive (catastrofismo, pensiero in bianco/nero, generalizzazione, ecc.) "
        "e il rischio di prendere decisioni impulsive o irrazionali. "
        "Fornisci un rapporto strutturato: 1) Emozioni rilevate, 2) Livello emotivo (1-10), "
        "3) Distorsioni cognitive identificate, 4) Raccomandazione (agire ora / attendere / riflettere). "
        "Rispondi in italiano.",
        nullptr
    },
    /* 5 — Documenti PDF */
    {
        "Analizza il documento fornito: struttura, temi principali, argomenti chiave e conclusioni. Rispondi in italiano.",
        "Crea un riassunto conciso e chiaro del documento. Rispondi in italiano.",
        "Estrai le informazioni chiave: fatti, dati, date, nomi e numeri importanti. Rispondi in italiano.",
        "Rispondi alle domande poste usando SOLO le informazioni contenute nel documento. Rispondi in italiano.",
        "Identifica punti deboli, contraddizioni, lacune argomentative o affermazioni non supportate. Rispondi in italiano.",
        "Ricava una lista di punti di azione, raccomandazioni o prossimi passi dal documento. Rispondi in italiano.",
        nullptr, nullptr
    },
    /* 6 — Blender MCP */
    {
        /* Cambia Materiale */
        "Sei un esperto di Blender Python API (bpy). Genera SOLO codice Python puro eseguibile in Blender. "
        "Crea o modifica un materiale Principled BSDF sull'oggetto attivo (bpy.context.active_object). "
        "Imposta base_color, metallic, roughness secondo la richiesta. "
        "Rispondi SOLO con il blocco codice Python tra ``` e ```, senza spiegazioni.",

        /* Trasla */
        "Sei un esperto di Blender Python API (bpy). Genera SOLO codice Python puro per spostare un oggetto in Blender. "
        "Usa bpy.context.active_object.location = (x, y, z). "
        "Rispondi SOLO con il blocco codice Python tra ``` e ```, senza spiegazioni.",

        /* Ruota */
        "Sei un esperto di Blender Python API (bpy). Genera SOLO codice Python puro per ruotare un oggetto in Blender. "
        "Usa bpy.context.active_object.rotation_euler = (rx, ry, rz) con angoli in radianti (import math; math.radians(...)). "
        "Rispondi SOLO con il blocco codice Python tra ``` e ```, senza spiegazioni.",

        /* Scala */
        "Sei un esperto di Blender Python API (bpy). Genera SOLO codice Python puro per scalare un oggetto in Blender. "
        "Usa bpy.context.active_object.scale = (sx, sy, sz). "
        "Rispondi SOLO con il blocco codice Python tra ``` e ```, senza spiegazioni.",

        /* Visibilita' */
        "Sei un esperto di Blender Python API (bpy). Genera SOLO codice Python puro per cambiare la visibilita' di oggetti. "
        "Usa obj.hide_viewport e obj.hide_render. Itera su bpy.data.objects se necessario. "
        "Rispondi SOLO con il blocco codice Python tra ``` e ```, senza spiegazioni.",

        /* Avvia Render */
        "Sei un esperto di Blender Python API (bpy). Genera SOLO codice Python puro per configurare e avviare un render. "
        "Usa bpy.context.scene.render per le impostazioni, bpy.ops.render.render(write_still=True) per avviare. "
        "Imposta l'output path se richiesto. "
        "Rispondi SOLO con il blocco codice Python tra ``` e ```, senza spiegazioni.",

        /* Script libero */
        "Sei un esperto di Blender Python API (bpy). Genera SOLO codice Python puro eseguibile in Blender via exec(). "
        "Il namespace disponibile: bpy, mathutils, Vector, Euler, Matrix. "
        "Rispondi SOLO con il blocco codice Python tra ``` e ```, senza spiegazioni.",

        nullptr
    },
    /* 7 — Office / LibreOffice (UNO o file-based) */
    {
        /* Crea documento Writer / Word */
        "Sei un esperto di LibreOffice e python-docx. Genera SOLO codice Python. "
        "PRIORITA': se 'desktop' e' nel namespace (modalita' UNO), usa LibreOffice Writer direttamente: "
        "  doc = desktop.loadComponentFromURL('private:factory/swriter', '_blank', 0, ()); "
        "  text = doc.getText(); cur = text.createTextCursor(); text.insertString(cur, '...', False); "
        "  out = Path.home()/'Desktop'/'documento.odt'; doc.storeToURL(systemPath(str(out)), ()); print(out). "
        "FALLBACK (no desktop): usa python-docx: Document() → salva in Path.home()/'Desktop'/'documento.docx'. "
        "Rispondi SOLO con il blocco codice Python tra ``` e ```, senza spiegazioni.",

        /* Crea foglio Calc / Excel */
        "Sei un esperto di LibreOffice Calc e openpyxl. Genera SOLO codice Python. "
        "PRIORITA' UNO: doc = desktop.loadComponentFromURL('private:factory/scalc', '_blank', 0, ()); "
        "  sheet = doc.Sheets.getByIndex(0); cell = sheet.getCellByPosition(col, row); "
        "  cell.setString('testo') oppure cell.setValue(numero); "
        "  out = Path.home()/'Desktop'/'foglio.ods'; doc.storeToURL(systemPath(str(out)), ()); print(out). "
        "FALLBACK: usa openpyxl Workbook → salva .xlsx in Path.home()/'Desktop'/. "
        "Rispondi SOLO con il blocco codice Python tra ``` e ```, senza spiegazioni.",

        /* Crea Presentazione Impress / PowerPoint */
        "Sei un esperto di LibreOffice Impress e python-pptx. Genera SOLO codice Python. "
        "PRIORITA' UNO: doc = desktop.loadComponentFromURL('private:factory/simpress', '_blank', 0, ()); "
        "  slide = doc.DrawPages[0]; slide.Name = 'Slide 1'; "
        "  out = Path.home()/'Desktop'/'presentazione.odp'; doc.storeToURL(systemPath(str(out)), ()); print(out). "
        "FALLBACK: usa python-pptx Presentation → salva .pptx in Path.home()/'Desktop'/. "
        "Rispondi SOLO con il blocco codice Python tra ``` e ```, senza spiegazioni.",

        /* Modifica documento aperto */
        "Sei un esperto di LibreOffice UNO. Genera SOLO codice Python. "
        "Con UNO apri il documento con desktop.loadComponentFromURL(systemPath(percorso), '_blank', 0, ()); "
        "modifica testo/celle/slide; salva con doc.store() (sovrascrive) o doc.storeToURL(url, ()). "
        "Se il percorso non e' fornito, usa Path.home()/'Desktop'/'documento.odt'. "
        "FALLBACK senza UNO: usa python-docx Document(path). "
        "Rispondi SOLO con il blocco codice Python tra ``` e ```, senza spiegazioni.",

        /* Tabella */
        "Sei un esperto di LibreOffice UNO e python-docx. Genera SOLO codice Python che crea una tabella formattata. "
        "Con UNO in Writer: doc.getText().insertTextContent(cur, doc.createInstance('com.sun.star.text.TextTable'), False). "
        "Con UNO in Calc: accedi alle celle con sheet.getCellByPosition(col, row). "
        "FALLBACK: python-docx Document + add_table oppure openpyxl Workbook. "
        "Salva e stampa il percorso. "
        "Rispondi SOLO con il blocco codice Python tra ``` e ```, senza spiegazioni.",

        /* Grafici e dati */
        "Sei un esperto di LibreOffice Calc UNO e openpyxl. Genera SOLO codice Python per grafici/analisi dati. "
        "Con UNO in Calc: inserisci dati nelle celle, poi crea grafico con createInstance('com.sun.star.chart.ChartDocument'). "
        "FALLBACK: usa openpyxl BarChart + Reference. "
        "Inserisci dati di esempio se non forniti. Salva e stampa il percorso. "
        "Rispondi SOLO con il blocco codice Python tra ``` e ```, senza spiegazioni.",

        /* Script libero */
        "Sei un esperto di LibreOffice UNO API. Genera SOLO codice Python eseguibile via exec(). "
        "Namespace UNO: desktop, uno, PropertyValue, createUnoService, systemPath, mkprops. "
        "Namespace file: Document/Pt (python-docx), Workbook/Font (openpyxl), Presentation/Inches (python-pptx). "
        "Namespace comune: Path, os, datetime. "
        "Salva sempre in Path.home()/'Desktop'/ e stampa il percorso o la conferma. "
        "Rispondi SOLO con il blocco codice Python tra ``` e ```, senza spiegazioni.",

        nullptr
    },
    /* 8 — FreeCAD MCP */
    {
        /* Crea Primitiva */
        "Sei un esperto di FreeCAD Python API. Genera SOLO codice Python puro eseguibile in FreeCAD. "
        "Usa: import FreeCAD, Part; doc = FreeCAD.activeDocument() or FreeCAD.newDocument('Doc'); "
        "aggiungi la primitiva richiesta (Part::Box, Part::Sphere, Part::Cylinder, ecc.); doc.recompute(). "
        "Rispondi SOLO con il blocco codice Python tra ``` e ```, senza spiegazioni.",

        /* Crea Schizzo */
        "Sei un esperto di FreeCAD Python API. Genera SOLO codice Python puro per creare uno schizzo (Sketcher). "
        "Usa: import FreeCAD, Sketcher; doc = FreeCAD.activeDocument(); "
        "sketch = doc.addObject('Sketcher::SketchObject','Sketch'); aggiungi geometria e vincoli; doc.recompute(). "
        "Rispondi SOLO con il blocco codice Python tra ``` e ```, senza spiegazioni.",

        /* Booleana */
        "Sei un esperto di FreeCAD Python API. Genera SOLO codice Python puro per operazioni booleane (unione, differenza, intersezione). "
        "Usa Part.makeCompound, Part.fuse(), Part.cut(), Part.common() oppure gli oggetti Part::Boolean di PartDesign. "
        "doc.recompute() alla fine. "
        "Rispondi SOLO con il blocco codice Python tra ``` e ```, senza spiegazioni.",

        /* Esporta STL/STEP */
        "Sei un esperto di FreeCAD Python API. Genera SOLO codice Python puro per esportare la geometria. "
        "Per STL: import Mesh; Mesh.export([obj], '/tmp/output.stl'). "
        "Per STEP: import Import; Import.export([obj], '/tmp/output.step'). "
        "Usa FreeCAD.activeDocument().ActiveObject per l'oggetto attivo. "
        "Rispondi SOLO con il blocco codice Python tra ``` e ```, senza spiegazioni.",

        /* Modifica proprieta' */
        "Sei un esperto di FreeCAD Python API. Genera SOLO codice Python puro per modificare proprieta' di oggetti. "
        "Accedi agli oggetti con FreeCAD.activeDocument().getObject('Nome') o .ActiveObject; "
        "modifica .Length, .Width, .Height, .Radius, .Placement, .Label, ecc.; doc.recompute(). "
        "Rispondi SOLO con il blocco codice Python tra ``` e ```, senza spiegazioni.",

        /* Vincoli e misure */
        "Sei un esperto di FreeCAD Python API. Genera SOLO codice Python puro per aggiungere vincoli e misure. "
        "Usa Sketcher per vincoli 2D: sketch.addConstraint(Sketcher.Constraint('Coincident', ...)); "
        "Per misure 3D usa Draft.makeDimension() o Part.makeLinearDimension(). doc.recompute(). "
        "Rispondi SOLO con il blocco codice Python tra ``` e ```, senza spiegazioni.",

        /* Script libero */
        "Sei un esperto di FreeCAD Python API. Genera SOLO codice Python puro eseguibile in FreeCAD via exec(). "
        "Namespace disponibile: FreeCAD, FreeCADGui, Part, PartDesign, Sketcher, Draft, Mesh, Import, App, Gui. "
        "Rispondi SOLO con il blocco codice Python tra ``` e ```, senza spiegazioni.",

        nullptr
    },
    /* 9 — CloudCompare (prossimamente) — stub */
    {
        "/* CloudCompare — funzionalit\xc3\xa0 non ancora disponibile */",
        nullptr, nullptr, nullptr, nullptr, nullptr, nullptr, nullptr
    },
};

static const char* kSubActions[10][10] = {
    { "\xf0\x9f\x92\xa1 Spiega concetto",
      "\xf0\x9f\x83\x8f Flashcard Q&A",
      "\xf0\x9f\x93\x9d Riassunto",
      "\xe2\x9d\x93 Domande d'esame",
      "\xf0\x9f\x97\xba Mappa concettuale",
      "\xf0\x9f\x8f\x8b Esercizi pratici",
      nullptr, nullptr },
    { "\xf0\x9f\x93\x96 Genera storia",
      "\xe2\x9e\xa1 Continua storia",
      "\xf0\x9f\xa7\x99 Crea personaggio",
      "\xf0\x9f\x8c\xb8 Scrivi poesia",
      "\xf0\x9f\x92\xac Genera dialogo",
      "\xf0\x9f\x8e\xac Sviluppa trama",
      nullptr, nullptr },
    { "\xf0\x9f\x94\x8d Ricerca approfondita",
      "\xe2\x9a\x96 Confronta opzioni",
      "\xe2\x9c\x85 Fact-checking",
      "\xf0\x9f\x93\x9a Genera bibliografia",
      "\xf0\x9f\x94\xad Analisi multi-prospettiva",
      "\xf0\x9f\x9b\xa4 Guida how-to",
      nullptr, nullptr },
    { "\xf0\x9f\x93\x9c Analisi letteraria",
      "\xf0\x9f\x93\x91 Riassunto capitoli",
      "\xf0\x9f\xa7\x91 Studio personaggi",
      "\xf0\x9f\x93\x8b Scheda libro",
      "\xf0\x9f\x92\xad Domande discussione",
      "\xf0\x9f\x93\x9a Consigli lettura",
      nullptr, nullptr },
    { "\xf0\x9f\x93\x8a Piano progetto",
      "\xf0\x9f\x93\x8c Lista MoSCoW",
      "\xe2\x9c\x89 Bozza email",
      "\xf0\x9f\x97\x93 Prepara riunione",
      "\xf0\x9f\xa7\xa0 Brainstorming",
      "\xf0\x9f\x8e\xaf Obiettivi SMART",
      "\xe2\x9a\x96 Matrice decisionale",
      "\xf0\x9f\x91\xa8\xe2\x80\x8d\xe2\x9a\x95\xef\xb8\x8f Email al Medico",
      "\xf0\x9f\xa7\xa0 Analisi Sentimenti",
      nullptr },
    { "\xf0\x9f\x94\x8d Analisi documento",
      "\xf0\x9f\x93\x9d Riassunto",
      "\xf0\x9f\x94\x8e Estrai informazioni",
      "\xe2\x9d\x93 Q&A documento",
      "\xf0\x9f\x9a\xa8 Punti critici",
      "\xe2\x9c\x85 Punti di azione",
      nullptr, nullptr },
    /* 6 — Blender MCP */
    { "\xf0\x9f\x8e\xa8 Cambia Materiale",
      "\xe2\x86\x94 Trasla",
      "\xf0\x9f\x94\x84 Ruota",
      "\xf0\x9f\x93\x90 Scala",
      "\xf0\x9f\x91\x81 Visibilit\xc3\xa0",
      "\xf0\x9f\x8e\xac Avvia Render",
      "\xf0\x9f\x90\x8d Script libero",
      nullptr },
    /* 7 — Office / LibreOffice */
    { "\xf0\x9f\x93\x84 Crea documento Word",
      "\xf0\x9f\x93\x8a Crea foglio Excel",
      "\xf0\x9f\x96\xa5 Crea presentazione",
      "\xe2\x9c\x8f Modifica documento",
      "\xf0\x9f\x93\x8b Inserisci tabella",
      "\xf0\x9f\x93\x88 Grafici e dati",
      "\xf0\x9f\x94\xa7 Script libero",
      nullptr },
    /* 8 — FreeCAD MCP */
    { "\xf0\x9f\x9f\xa6 Crea Primitiva",
      "\xe2\x9c\x8f Crea Schizzo",
      "\xe2\x9c\x82 Booleana",
      "\xf0\x9f\x93\xa4 Esporta STL/STEP",
      "\xf0\x9f\x94\xa7 Modifica propriet\xc3\xa0",
      "\xf0\x9f\x93\x90 Vincoli & misure",
      "\xf0\x9f\x90\x8d Script libero",
      nullptr },
    /* 9 — CloudCompare (prossimamente) */
    { "\xf0\x9f\x94\xb5 Carica nuvola punti",
      "\xf0\x9f\x93\x90 Calcola normali",
      "\xf0\x9f\x94\x81 Registrazione ICP",
      "\xf0\x9f\x93\x8f Calcola distanze",
      "\xe2\x9c\x82 Filtra / Segmenta",
      "\xf0\x9f\x93\xa4 Esporta PLY/LAS",
      "\xf0\x9f\x90\x8d Script libero (CloudComPy)",
      nullptr },
};

static const char* kPlaceholders[10] = {
    "Incolla il testo o descrivi il concetto da studiare...",
    "Descrivi l'idea, il personaggio o la scena...",
    "Inserisci l'argomento da ricercare o l'affermazione da verificare...",
    "Incolla il testo del libro o il titolo e l'autore...",
    "Descrivi il progetto, il task o l'obiettivo...",
    "Incolla il testo del documento oppure carica un PDF con il pulsante sopra...",
    "Descrivi cosa fare in Blender (es. 'Cambia il cubo in rosso metallico', 'Sposta il piano a Y=3', 'Ruota 45 gradi sull asse Z')...",
    "Descrivi il documento da creare (es. 'Crea una lettera di presentazione professionale', 'Foglio Excel con budget mensile', 'Slide riassuntive su Python')...",
    "Descrivi cosa modellare in FreeCAD (es. 'Crea un box 20x10x5mm', 'Sfera di raggio 15mm', 'Esporta il modello attivo in STL')...",
    /* 9 — CloudCompare (prossimamente) */
    "CloudCompare — funzionalit\xc3\xa0 in arrivo. Bridge CloudComPy in sviluppo...",
};

/* ══════════════════════════════════════════════════════════════
   Costruttore — layout immediato:
   [tab categorie]
   [griglia bottoni azione — visibili tutti, cliccabili direttamente]
   [✅ Azione selezionata]
   [area input]  [▶ Esegui / ⏹ Stop]
   [output AI]
   ══════════════════════════════════════════════════════════════ */
StrumentiPage::StrumentiPage(AiClient* ai, QWidget* parent)
    : QWidget(parent), m_ai(ai)
{
    /* Widget interni nascosti — usati dai slot tramite row/index */
    m_navList = new QListWidget(this);
    m_navList->hide();
    for (int i = 0; i < 6; i++) m_navList->addItem("");
    m_navList->setCurrentRow(0);

    m_cmbSub = new QComboBox(this);
    m_cmbSub->hide();
    for (int i = 0; i < 6; i++) m_cmbSub->addItem("");
    m_cmbSub->setCurrentIndex(0);

    /* Layout principale */
    auto* lay = new QVBoxLayout(this);
    lay->setContentsMargins(16, 10, 16, 10);
    lay->setSpacing(8);

    /* ── Barra categorie: tab orizzontali ── */
    static const char* kCatLabels[] = {
        "\xf0\x9f\x93\x9a  Studio",
        "\xe2\x9c\x8d\xef\xb8\x8f  Scrittura",
        "\xf0\x9f\x94\x8d  Ricerca",
        "\xf0\x9f\x93\x96  Libri",
        "\xe2\x9a\xa1  Produttivit\xc3\xa0",
        "\xf0\x9f\x93\x84  Documenti",
        /* cats 6-9 (Blender, Office, FreeCAD, CloudCompare) spostate in AppControllerPage */
    };
    static const char* kCatTooltips[] = {
        "Studio — spiega concetti, riassumi argomenti, analizza testi",
        "Scrittura — crea email, articoli, storie e testi creativi",
        "Ricerca — cerca info, verifica fatti, analisi critiche",
        "Libri — riassunti, analisi romanzi, guide alla lettura",
        "Produttiv\xc3\xa0 — organizza task, crea liste, gestisci il tempo",
        "Documenti — analizza PDF/TXT, estrai dati, riassunti",
    };

    auto* catBar = new QWidget(this);
    auto* catLay = new QHBoxLayout(catBar);
    catLay->setContentsMargins(0, 0, 0, 0);
    catLay->setSpacing(0);

    auto* catGroup = new QButtonGroup(this);
    catGroup->setExclusive(true);

    /* Stack: una pagina per categoria con i bottoni azione */
    auto* actStack = new QStackedWidget(this);
    actStack->setMaximumHeight(180);


    /* Label azione corrente */
    auto* lblSel = new QLabel(this);
    lblSel->setObjectName("cardDesc");
    const QString firstAction = QString::fromUtf8(kSubActions[0][0]);
    lblSel->setText("\xe2\x9c\x85  <b>" + firstAction + "</b>");
    lblSel->setTextFormat(Qt::RichText);

    for (int cat = 0; cat < 6; cat++) {
        /* Tab categoria */
        auto* catBtn = new QPushButton(
            QString::fromUtf8(kCatLabels[cat]), catBar);
        catBtn->setCheckable(true);
        catBtn->setChecked(cat == 0);
        catBtn->setObjectName("strCatBtn");
        catBtn->setToolTip(QString::fromUtf8(kCatTooltips[cat]));
        catGroup->addButton(catBtn, cat);
        catLay->addWidget(catBtn);

        /* Pagina azioni per questa categoria */
        auto* page = new QWidget;
        auto* grid = new QGridLayout(page);
        grid->setContentsMargins(0, 6, 0, 2);
        grid->setSpacing(8);

        auto* actGroup = new QButtonGroup(page);
        actGroup->setExclusive(true);

        int col = 0, row = 0;
        for (int act = 0; kSubActions[cat][act] != nullptr; act++) {
            auto* abtn = new QPushButton(
                QString::fromUtf8(kSubActions[cat][act]), page);
            abtn->setCheckable(true);
            abtn->setChecked(act == 0);
            abtn->setObjectName("strActBtn");
            actGroup->addButton(abtn, act);
            grid->addWidget(abtn, row, col);
            if (++col > 2) { col = 0; row++; }

            connect(abtn, &QPushButton::clicked, this,
                    [this, cat, act, lblSel]() {
                m_currentCat = cat;
                m_navList->setCurrentRow(cat);
                m_cmbSub->setCurrentIndex(act);
                m_inputArea->setPlaceholderText(
                    QString::fromUtf8(kPlaceholders[cat]));
                lblSel->setText(
                    "\xe2\x9c\x85  <b>" +
                    QString::fromUtf8(kSubActions[cat][act]) +
                    "</b>");
            });
        }
        for (int c = 0; c < 3; c++) grid->setColumnStretch(c, 1);
        actStack->addWidget(page);
    }
    catLay->addStretch();

    /* Cambio categoria */
    connect(catGroup, QOverload<int>::of(&QButtonGroup::idClicked),
            this, [this, actStack, lblSel](int cat) {
        m_currentCat = cat;
        actStack->setCurrentIndex(cat);
        m_navList->setCurrentRow(cat);
        m_cmbSub->setCurrentIndex(0);
        m_inputArea->setPlaceholderText(
            QString::fromUtf8(kPlaceholders[cat]));
        lblSel->setText(
            "\xe2\x9c\x85  <b>" +
            QString::fromUtf8(kSubActions[cat][0]) +
            "</b>");
        m_pdfRow->setVisible(cat == 5);
        /* cats 6-9 (Blender/Office/FreeCAD/CloudCompare) spostate in AppControllerPage */
        m_ragRow->setVisible(true);
        m_codeModelRow->setVisible(false);
        m_btnRun->setEnabled(true);
        m_ai->fetchModels();

        /* Aggiorna hint raccomandazioni in base alla categoria */
        static const char* kModelHints[6] = {
            /* 0 Studio */
            "\xe2\x9c\xa8 Consigliati: <b>mistral</b>, <b>llama3</b>, <b>qwen3</b>"
            " \xe2\x80\x94 buona comprensione e spiegazione",
            /* 1 Scrittura */
            "\xe2\x9c\xa8 Consigliati: <b>mistral</b>, <b>llama3</b>, <b>gemma3</b>"
            " \xe2\x80\x94 creativit\xc3\xa0 e fluidit\xc3\xa0 narrativa",
            /* 2 Ricerca */
            "\xe2\x9c\xa8 Consigliati: <b>qwen3:30b</b>, <b>deepseek-r1</b>, <b>llama3</b>"
            " \xe2\x80\x94 ragionamento avanzato e fact-checking",
            /* 3 Libri */
            "\xe2\x9c\xa8 Consigliati: <b>mistral</b>, <b>llama3</b>, <b>qwen3</b>"
            " \xe2\x80\x94 analisi letteraria e critica",
            /* 4 Produttivita */
            "\xe2\x9c\xa8 Consigliati: <b>mistral</b>, <b>qwen3</b>, <b>phi4</b>"
            " \xe2\x80\x94 risposte strutturate e concise",
            /* 5 Documenti PDF */
            "\xe2\x9c\xa8 Consigliati: <b>llama3</b>, <b>qwen3</b>, <b>mistral</b>"
            " \xe2\x80\x94 estrazione e sintesi testi lunghi",
        };
        if (cat >= 0 && cat < 6)
            m_codeModelInfo->setText(
                QString::fromUtf8(kModelHints[cat]));
    });

    lay->addWidget(catBar);
    lay->addWidget(actStack);
    lay->addWidget(lblSel);

    /* ── Riga RAG in-page (visibile per tutte le categorie attive) ── */
    m_ragRow = new QWidget(this);
    m_ragRow->setObjectName("ragRow");
    auto* ragLay = new QHBoxLayout(m_ragRow);
    ragLay->setContentsMargins(8, 4, 8, 4);
    ragLay->setSpacing(8);

    m_ragCheck = new QCheckBox(
        "\xf0\x9f\x93\x9a  RAG documenti", m_ragRow);
    m_ragCheck->setToolTip(
        "Se attivo, i documenti caricati vengono usati come contesto\n"
        "per ogni richiesta AI in questa sezione.");
    ragLay->addWidget(m_ragCheck);

    auto* ragAddBtn = new QPushButton(
        "\xf0\x9f\x93\x82  Aggiungi", m_ragRow);
    ragAddBtn->setObjectName("actionBtn");
    ragAddBtn->setFixedWidth(100);
    ragAddBtn->setToolTip("Aggiungi PDF, TXT o Markdown all'indice RAG");
    ragLay->addWidget(ragAddBtn);

    m_ragInfoLbl = new QLabel("Nessun documento caricato", m_ragRow);
    m_ragInfoLbl->setObjectName("hintLabel");
    ragLay->addWidget(m_ragInfoLbl, 1);

    auto* ragClearBtn = new QPushButton(
        "\xf0\x9f\x97\x91  Svuota", m_ragRow);
    ragClearBtn->setObjectName("actionBtn");
    ragClearBtn->setFixedWidth(80);
    ragLay->addWidget(ragClearBtn);

    lay->addWidget(m_ragRow);

    /* Aggiungi documenti al RAG */
    connect(ragAddBtn, &QPushButton::clicked, this, [this]() {
        const QStringList paths = QFileDialog::getOpenFileNames(
            this,
            "Seleziona documenti per RAG",
            "",
            "Documenti (*.pdf *.txt *.md *.csv *.rst);;"
            "Tutti i file (*)");
        for (const QString& p : paths)
            ragAddFile(p);
    });

    /* Svuota indice RAG */
    connect(ragClearBtn, &QPushButton::clicked, this, [this]() {
        m_ragChunks.clear();
        m_ragFileNames.clear();
        m_ragCheck->setChecked(false);
        m_ragInfoLbl->setText("Nessun documento caricato");
    });

    /* ── Riga PDF picker (visibile solo per categoria Documenti) ── */
    m_pdfRow = new QWidget(this);
    auto* pdfLay = new QHBoxLayout(m_pdfRow);
    pdfLay->setContentsMargins(0, 4, 0, 0);
    pdfLay->setSpacing(8);

    auto* pdfBtn = new QPushButton("\xf0\x9f\x93\x84  Carica PDF", m_pdfRow);
    pdfBtn->setObjectName("actionBtn");
    pdfBtn->setFixedWidth(130);

    m_pdfPathLbl = new QLabel("Nessun PDF caricato", m_pdfRow);
    m_pdfPathLbl->setObjectName("hintLabel");
    m_pdfPathLbl->setWordWrap(false);

    pdfLay->addWidget(pdfBtn);
    pdfLay->addWidget(m_pdfPathLbl, 1);
    m_pdfRow->setVisible(false);
    lay->addWidget(m_pdfRow);

    connect(pdfBtn, &QPushButton::clicked, this, [this]() {
        QString path = QFileDialog::getOpenFileName(
            this, "Seleziona PDF", "",
            "PDF (*.pdf);;Tutti i file (*)");
        if (path.isEmpty()) return;
        m_pdfPath = path;
        m_pdfPathLbl->setText(
            QFileInfo(path).fileName());
    });

    /* ── Riga Blender Bridge (visibile solo per categoria Blender) ── */
    m_blenderRow = new QWidget(this);
    auto* blenderLay = new QHBoxLayout(m_blenderRow);
    blenderLay->setContentsMargins(0, 4, 0, 0);
    blenderLay->setSpacing(8);

    auto* blenderLbl = new QLabel("Blender:", m_blenderRow);
    blenderLbl->setObjectName("hintLabel");

    m_blenderHostEdit = new QLineEdit("localhost:6789", m_blenderRow);
    m_blenderHostEdit->setFixedWidth(160);
    m_blenderHostEdit->setPlaceholderText("localhost:6789");

    auto* blenderPingBtn = new QPushButton("\xf0\x9f\x94\x97  Verifica", m_blenderRow);
    blenderPingBtn->setObjectName("actionBtn");
    blenderPingBtn->setFixedWidth(100);

    m_blenderStatusLbl = new QLabel("\xe2\x9a\xaa  Non connesso", m_blenderRow);
    m_blenderStatusLbl->setObjectName("hintLabel");

    m_blenderExecBtn = new QPushButton(
        "\xe2\x96\xb6  Esegui in Blender", m_blenderRow);
    m_blenderExecBtn->setObjectName("actionBtn");
    m_blenderExecBtn->setFixedWidth(160);
    m_blenderExecBtn->setEnabled(false);

    blenderLay->addWidget(blenderLbl);
    blenderLay->addWidget(m_blenderHostEdit);
    blenderLay->addWidget(blenderPingBtn);
    blenderLay->addWidget(m_blenderStatusLbl, 1);
    blenderLay->addWidget(m_blenderExecBtn);

    auto* blenderHelpBtn = new QPushButton(
        "\xf0\x9f\x9b\x9f \xf0\x9f\x94\xa7  Aiuto", m_blenderRow);
    blenderHelpBtn->setObjectName("actionBtn");
    blenderHelpBtn->setFixedWidth(90);
    blenderLay->addWidget(blenderHelpBtn);

    connect(blenderHelpBtn, &QPushButton::clicked, this, [this]() {
        auto* dlg = new QDialog(this);
        dlg->setWindowTitle(
            "\xf0\x9f\x8e\xa8  Installazione Blender MCP");
        dlg->setAttribute(Qt::WA_DeleteOnClose);
        dlg->resize(560, 500);
        auto* dlay = new QVBoxLayout(dlg);
        auto* browser = new QTextBrowser(dlg);
        browser->setOpenExternalLinks(true);
        browser->setHtml(
            "<h3>\xf0\x9f\x8e\xa8 Installazione Blender MCP</h3>"
            "<p>Collega Blender a Prismalux/Claude Code per controllarlo via AI (bpy).</p>"
            "<hr>"
            "<h4>1. Installa Blender</h4>"
            "<p>Scarica da <a href='https://www.blender.org/download/'>blender.org/download</a>"
            " oppure: <code>sudo apt install blender</code></p>"
            "<h4>2. Scarica l&apos;addon Blender MCP</h4>"
            "<pre>git clone https://github.com/ahujasid/blender-mcp</pre>"
            "<h4>3. Installa l&apos;addon in Blender</h4>"
            "<ol>"
            "<li>Apri Blender</li>"
            "<li><b>Edit \xe2\x86\x92 Preferences \xe2\x86\x92 Add-ons \xe2\x86\x92 Install</b></li>"
            "<li>Seleziona il file <code>blender_mcp.py</code> dalla cartella clonata</li>"
            "<li>Abilita l&apos;addon <b>\"Blender MCP\"</b> nella lista</li>"
            "</ol>"
            "<h4>4. Avvia il server MCP in Blender</h4>"
            "<ol>"
            "<li>In 3D Viewport premi <b>N</b> per aprire il pannello laterale</li>"
            "<li>Seleziona il tab <b>MCP</b></li>"
            "<li>Clicca <b>Start MCP Server</b> (porta default: <b>6789</b>)</li>"
            "</ol>"
            "<h4>5. Collega Prismalux</h4>"
            "<p>Torna in Prismalux \xe2\x86\x92 <b>Strumenti \xe2\x86\x92 Blender</b>"
            " \xe2\x86\x92 clicca <b>\xf0\x9f\x94\x97 Verifica</b>.</p>"
            "<h4>6. (Opzionale) Registra in Claude Code</h4>"
            "<pre>claude mcp add blender node /percorso/blender-mcp/server.js</pre>"
            "<hr>"
            "<p>\xe2\x9c\x85 Una volta connesso, i pulsanti <b>Esegui in Blender</b>"
            " inviano il codice bpy generato dall&apos;AI direttamente a Blender.</p>");
        auto* btnClose = new QPushButton("\xe2\x9c\x95  Chiudi", dlg);
        btnClose->setObjectName("actionBtn");
        connect(btnClose, &QPushButton::clicked, dlg, &QDialog::accept);
        dlay->addWidget(browser);
        dlay->addWidget(btnClose);
        dlg->exec();
    });

    m_blenderRow->setVisible(false);
    lay->addWidget(m_blenderRow);

    /* ── Avviso installazione Blender MCP ── */
    m_blenderHintRow = new QWidget(this);
    auto* blenderHintLay = new QHBoxLayout(m_blenderHintRow);
    blenderHintLay->setContentsMargins(0, 0, 0, 4);
    blenderHintLay->setSpacing(0);
    auto* blenderHintLbl = new QLabel(m_blenderHintRow);
    blenderHintLbl->setObjectName("hintLabel");
    blenderHintLbl->setOpenExternalLinks(true);
    blenderHintLbl->setWordWrap(true);
    blenderHintLbl->setText(
        "\xf0\x9f\x93\xa6 <b>MCP non connesso?</b> "
        "Installa il server Blender MCP: "
        "<a href='https://github.com/ahujasid/blender-mcp'>"
        "github.com/ahujasid/blender-mcp</a> "
        "\xe2\x86\x92 Blender \xc2\xbb Preferences \xc2\xbb Add-ons \xc2\xbb Install "
        "\xe2\x86\x92 avvia il server dal pannello N.");
    blenderHintLay->addWidget(blenderHintLbl);
    m_blenderHintRow->setVisible(false);
    lay->addWidget(m_blenderHintRow);

    m_blenderNam = new QNetworkAccessManager(this);

    /* Ping /status → verifica connessione Blender */
    connect(blenderPingBtn, &QPushButton::clicked, this, [this]() {
        QString addr = m_blenderHostEdit->text().trimmed();
        if (addr.isEmpty()) addr = "localhost:6789";
        QNetworkRequest req(QUrl("http://" + addr + "/status"));
        req.setTransferTimeout(3000);
        auto* reply = m_blenderNam->get(req);
        connect(reply, &QNetworkReply::finished, this, [this, reply]() {
            reply->deleteLater();
            if (reply->error() == QNetworkReply::NoError) {
                QJsonObject obj = QJsonDocument::fromJson(
                    reply->readAll()).object();
                QString ver = obj.value("blender").toString("?");
                m_blenderStatusLbl->setText(
                    "\xe2\x9c\x85  Blender " + ver + " connesso");
            } else {
                m_blenderStatusLbl->setText(
                    "\xe2\x9d\x8c  " + reply->errorString());
            }
        });
    });

    /* POST /execute → invia codice bpy a Blender */
    connect(m_blenderExecBtn, &QPushButton::clicked, this, [this]() {
        if (m_blenderCode.isEmpty()) return;
        QString addr = m_blenderHostEdit->text().trimmed();
        if (addr.isEmpty()) addr = "localhost:6789";

        QJsonObject payload;
        payload["code"] = m_blenderCode;
        QByteArray body =
            QJsonDocument(payload).toJson(QJsonDocument::Compact);

        QNetworkRequest req(QUrl("http://" + addr + "/execute"));
        req.setHeader(QNetworkRequest::ContentTypeHeader,
                      "application/json; charset=utf-8");
        req.setTransferTimeout(20000);

        m_blenderExecBtn->setEnabled(false);
        m_blenderStatusLbl->setText(
            "\xf0\x9f\x94\x84  Invio a Blender...");

        auto* reply = m_blenderNam->post(req, body);
        connect(reply, &QNetworkReply::finished, this, [this, reply]() {
            reply->deleteLater();
            m_blenderExecBtn->setEnabled(true);
            if (reply->error() == QNetworkReply::NoError) {
                QJsonObject res = QJsonDocument::fromJson(
                    reply->readAll()).object();
                if (res["ok"].toBool()) {
                    m_blenderStatusLbl->setText("\xe2\x9c\x85  Eseguito");
                    QString out = res["output"].toString();
                    m_output->append(
                        "\n\xe2\x9c\x85  Blender: " +
                        (out.isEmpty() ? "OK" : out));
                } else {
                    m_blenderStatusLbl->setText("\xe2\x9d\x8c  Errore Blender");
                    m_output->append(
                        "\n\xe2\x9d\x8c  Blender errore:\n" +
                        res["error"].toString());
                }
            } else {
                m_blenderStatusLbl->setText(
                    "\xe2\x9d\x8c  " + reply->errorString());
                m_output->append(
                    "\n\xe2\x9d\x8c  Connessione a Blender fallita: " +
                    reply->errorString());
            }
        });
    });

    /* ── Riga Office Bridge (visibile solo per categoria Office) ── */
    m_officeRow = new QWidget(this);
    auto* officeLay = new QHBoxLayout(m_officeRow);
    officeLay->setContentsMargins(0, 4, 0, 0);
    officeLay->setSpacing(8);

    auto* officeLbl = new QLabel("Office:", m_officeRow);
    officeLbl->setObjectName("hintLabel");

    m_officeStartBtn = new QPushButton(
        "\xe2\x96\xb6  Avvia bridge", m_officeRow);
    m_officeStartBtn->setObjectName("actionBtn");
    m_officeStartBtn->setFixedWidth(120);

    m_officeStatusLbl = new QLabel(
        "\xe2\x9a\xaa  Bridge non avviato", m_officeRow);
    m_officeStatusLbl->setObjectName("hintLabel");

    m_officeExecBtn = new QPushButton(
        "\xf0\x9f\x96\xa5  Esegui in Office", m_officeRow);
    m_officeExecBtn->setObjectName("actionBtn");
    m_officeExecBtn->setFixedWidth(160);
    m_officeExecBtn->setEnabled(false);

    officeLay->addWidget(officeLbl);
    officeLay->addWidget(m_officeStartBtn);
    officeLay->addWidget(m_officeStatusLbl, 1);
    officeLay->addWidget(m_officeExecBtn);

    auto* officeHelpBtn = new QPushButton(
        "\xf0\x9f\x9b\x9f \xf0\x9f\x94\xa7  Aiuto", m_officeRow);
    officeHelpBtn->setObjectName("actionBtn");
    officeHelpBtn->setFixedWidth(90);
    officeLay->addWidget(officeHelpBtn);

    connect(officeHelpBtn, &QPushButton::clicked, this, [this]() {
        auto* dlg = new QDialog(this);
        dlg->setWindowTitle(
            "\xf0\x9f\x96\xa5  Setup Office \xe2\x80\x94 LibreOffice & Microsoft 365");
        dlg->setAttribute(Qt::WA_DeleteOnClose);
        dlg->resize(580, 560);
        auto* dlay = new QVBoxLayout(dlg);
        auto* browser = new QTextBrowser(dlg);
        browser->setOpenExternalLinks(true);
        browser->setHtml(
            "<h3>\xf0\x9f\x96\xa5 Setup Office: LibreOffice + Microsoft 365</h3>"
            "<hr>"
            "<h4>\xf0\x9f\x93\x97 LibreOffice \xe2\x80\x94 bridge integrato in Prismalux</h4>"
            "<p>Il bridge Python \xc3\xa8 gi\xc3\xa0 incluso. Nessun MCP esterno richiesto.</p>"
            "<ol>"
            "<li><b>Installa LibreOffice</b>:<br>"
            "<a href='https://www.libreoffice.org/download/libreoffice-fresh/'>"
            "libreoffice.org/download</a><br>"
            "oppure: <code>sudo apt install libreoffice</code></li>"
            "<li><b>Installa le dipendenze Python</b>:<br>"
            "<code>pip install python-docx openpyxl python-pptx</code></li>"
            "<li>In Prismalux \xe2\x86\x92 <b>Strumenti \xe2\x86\x92 Office</b>"
            " \xe2\x86\x92 clicca <b>\xe2\x96\xb6 Avvia bridge</b></li>"
            "<li>Il bridge si connette a LibreOffice via <b>UNO API</b></li>"
            "<li>Clicca <b>\xf0\x9f\x96\xa5 Esegui in Office</b> dopo aver generato il codice</li>"
            "</ol>"
            "<p>\xe2\x9c\x85 Supporta: Writer (.odt/.docx), Calc (.ods/.xlsx), Impress (.odp/.pptx)</p>"
            "<hr>"
            "<h4>\xf0\x9f\x93\x98 Microsoft Office 365 \xe2\x80\x94 MCP esterno</h4>"
            "<p>Per controllare Microsoft 365 (Word, Excel, PowerPoint online) serve un MCP dedicato.</p>"
            "<ol>"
            "<li><b>Installa Node.js</b>: <a href='https://nodejs.org/'>nodejs.org</a></li>"
            "<li><b>Cerca un server MCP Office 365</b> nella lista ufficiale:<br>"
            "<a href='https://github.com/modelcontextprotocol/servers'>"
            "github.com/modelcontextprotocol/servers</a></li>"
            "<li><b>Segui le istruzioni</b> del server scelto per installarlo</li>"
            "<li><b>Registra in Claude Code</b>:<br>"
            "<code>claude mcp add office365 node /percorso/al/server/index.js</code></li>"
            "<li>Richiede un account <b>Microsoft 365</b> attivo e le credenziali OAuth</li>"
            "</ol>"
            "<p>\xe2\x9a\xa0 Microsoft Office richiede autenticazione Microsoft 365 &mdash; "
            "non funziona con versioni desktop standalone senza cloud.</p>"
            "<hr>"
            "<p>\xf0\x9f\x92\xa1 <b>Consiglio</b>: per uso locale usa <b>LibreOffice</b> "
            "(gratuito, bridge immediato). Per collaborazione cloud usa <b>Microsoft 365 MCP</b>.</p>");
        auto* btnClose = new QPushButton("\xe2\x9c\x95  Chiudi", dlg);
        btnClose->setObjectName("actionBtn");
        connect(btnClose, &QPushButton::clicked, dlg, &QDialog::accept);
        dlay->addWidget(browser);
        dlay->addWidget(btnClose);
        dlg->exec();
    });

    m_officeRow->setVisible(false);
    lay->addWidget(m_officeRow);

    /* ── Avviso installazione MCP per Microsoft Office ── */
    m_officeHintRow = new QWidget(this);
    auto* officeHintLay = new QHBoxLayout(m_officeHintRow);
    officeHintLay->setContentsMargins(0, 0, 0, 4);
    officeHintLay->setSpacing(0);
    auto* officeHintLbl = new QLabel(m_officeHintRow);
    officeHintLbl->setObjectName("hintLabel");
    officeHintLbl->setOpenExternalLinks(true);
    officeHintLbl->setWordWrap(true);
    officeHintLbl->setText(
        "\xf0\x9f\x93\xa6 <b>Vuoi controllare Microsoft Office/365 via MCP?</b> "
        "Installa un server MCP dedicato: cerca <b>office365 MCP</b> su "
        "<a href='https://github.com/modelcontextprotocol/servers'>"
        "github.com/modelcontextprotocol/servers</a> "
        "oppure su npm (<code>npm install -g mcp-server-office365</code>), "
        "poi registralo con: <code>claude mcp add office365 node /path/to/server.js</code>. "
        "\xe2\x86\x92 Il bridge Python qui sopra funziona gi\xc3\xa0 con LibreOffice senza MCP.");
    officeHintLay->addWidget(officeHintLbl);
    m_officeHintRow->setVisible(false);
    lay->addWidget(m_officeHintRow);

    m_officeNam = new QNetworkAccessManager(this);

    /* ── Riga FreeCAD MCP (visibile solo per categoria FreeCAD, cat=8) ── */
    m_freecadRow = new QWidget(this);
    auto* freecadLay = new QHBoxLayout(m_freecadRow);
    freecadLay->setContentsMargins(0, 4, 0, 0);
    freecadLay->setSpacing(8);

    auto* freecadLbl = new QLabel("FreeCAD:", m_freecadRow);
    freecadLbl->setObjectName("hintLabel");

    m_freecadHostEdit = new QLineEdit("localhost:9876", m_freecadRow);
    m_freecadHostEdit->setFixedWidth(160);
    m_freecadHostEdit->setPlaceholderText("localhost:9876");

    auto* freecadPingBtn = new QPushButton("\xf0\x9f\x94\x97  Verifica", m_freecadRow);
    freecadPingBtn->setObjectName("actionBtn");
    freecadPingBtn->setFixedWidth(100);

    m_freecadStatusLbl = new QLabel("\xe2\x9a\xaa  Non connesso", m_freecadRow);
    m_freecadStatusLbl->setObjectName("hintLabel");

    m_freecadExecBtn = new QPushButton(
        "\xf0\x9f\x94\xa9  Esegui in FreeCAD", m_freecadRow);
    m_freecadExecBtn->setObjectName("actionBtn");
    m_freecadExecBtn->setFixedWidth(170);
    m_freecadExecBtn->setEnabled(false);

    freecadLay->addWidget(freecadLbl);
    freecadLay->addWidget(m_freecadHostEdit);
    freecadLay->addWidget(freecadPingBtn);
    freecadLay->addWidget(m_freecadStatusLbl, 1);
    freecadLay->addWidget(m_freecadExecBtn);

    auto* freecadHelpBtn = new QPushButton(
        "\xf0\x9f\x9b\x9f \xf0\x9f\x94\xa7  Aiuto", m_freecadRow);
    freecadHelpBtn->setObjectName("actionBtn");
    freecadHelpBtn->setFixedWidth(90);
    freecadLay->addWidget(freecadHelpBtn);

    connect(freecadHelpBtn, &QPushButton::clicked, this, [this]() {
        auto* dlg = new QDialog(this);
        dlg->setWindowTitle(
            "\xf0\x9f\x94\xa9  Installazione FreeCAD MCP");
        dlg->setAttribute(Qt::WA_DeleteOnClose);
        dlg->resize(560, 500);
        auto* dlay = new QVBoxLayout(dlg);
        auto* browser = new QTextBrowser(dlg);
        browser->setOpenExternalLinks(true);
        browser->setHtml(
            "<h3>\xf0\x9f\x94\xa9 Installazione FreeCAD MCP</h3>"
            "<p>Collega FreeCAD a Prismalux/Claude Code per modellazione 3D via AI.</p>"
            "<hr>"
            "<h4>1. Installa FreeCAD</h4>"
            "<p>Scarica da <a href='https://www.freecad.org/downloads.php'>freecad.org/downloads</a>"
            " oppure:<br>"
            "<code>sudo apt install freecad</code><br>"
            "Per la versione pi\xc3\xb9 recente usa FlatPak o AppImage dal sito ufficiale.</p>"
            "<h4>2. Installa il workbench FreeCAD MCP</h4>"
            "<pre>git clone https://github.com/bonninr/freecad_mcp \\\n"
            "      ~/.FreeCAD/Mod/freecad_mcp</pre>"
            "<h4>3. Riavvia FreeCAD</h4>"
            "<p>FreeCAD carica i workbench da <code>~/.FreeCAD/Mod/</code> all&apos;avvio.</p>"
            "<h4>4. Seleziona il workbench FreeCAD MCP</h4>"
            "<ol>"
            "<li>Dal menu workbench in alto seleziona <b>FreeCAD MCP</b></li>"
            "<li>Apparir\xc3\xa0 il pannello con il pulsante <b>Start RPC Server</b></li>"
            "<li>Clicca <b>Start RPC Server</b> (porta default: <b>9876</b>)</li>"
            "</ol>"
            "<h4>5. Collega Prismalux</h4>"
            "<p>Torna in Prismalux \xe2\x86\x92 <b>Strumenti \xe2\x86\x92 FreeCAD</b>"
            " \xe2\x86\x92 clicca <b>\xf0\x9f\x94\x97 Verifica</b>.</p>"
            "<h4>6. (Opzionale) Registra in Claude Code</h4>"
            "<pre>claude mcp add freecad python3 \\\n"
            "  ~/.FreeCAD/Mod/freecad_mcp/src/freecad_bridge.py</pre>"
            "<hr>"
            "<p>\xe2\x9c\x85 Una volta connesso, usa <b>Script libero</b> o le azioni predefinite"
            " per generare ed eseguire codice Python direttamente in FreeCAD.</p>"
            "<p>\xf0\x9f\x8f\x97 Usa anche il pannello <b>Disegno \xe2\x86\x92 Modello 3D</b>"
            " per generare modelli da schizzi o PDF con dimensioni.</p>");
        auto* btnClose = new QPushButton("\xe2\x9c\x95  Chiudi", dlg);
        btnClose->setObjectName("actionBtn");
        connect(btnClose, &QPushButton::clicked, dlg, &QDialog::accept);
        dlay->addWidget(browser);
        dlay->addWidget(btnClose);
        dlg->exec();
    });

    m_freecadRow->setVisible(false);
    lay->addWidget(m_freecadRow);

    /* ── Avviso installazione FreeCAD MCP ── */
    m_freecadHintRow = new QWidget(this);
    auto* freecadHintLay = new QHBoxLayout(m_freecadHintRow);
    freecadHintLay->setContentsMargins(0, 0, 0, 4);
    freecadHintLay->setSpacing(0);
    auto* freecadHintLbl = new QLabel(m_freecadHintRow);
    freecadHintLbl->setObjectName("hintLabel");
    freecadHintLbl->setOpenExternalLinks(true);
    freecadHintLbl->setWordWrap(true);
    freecadHintLbl->setText(
        "\xf0\x9f\x93\xa6 <b>MCP non connesso?</b> "
        "Installa il workbench FreeCAD MCP: "
        "<a href='https://github.com/bonninr/freecad_mcp'>"
        "github.com/bonninr/freecad_mcp</a> "
        "\xe2\x86\x92 copia in ~/.FreeCAD/Mod/freecad_mcp "
        "\xe2\x86\x92 apri FreeCAD \xc2\xbb seleziona workbench <b>FreeCAD MCP</b> "
        "\xe2\x86\x92 clicca <b>Start RPC Server</b> (porta 9876).");
    freecadHintLay->addWidget(freecadHintLbl);
    m_freecadHintRow->setVisible(false);
    lay->addWidget(m_freecadHintRow);

    /* ── Riga Sketch → 3D Model (Blender cat=6 + FreeCAD cat=8) ── */
    m_sketchRow = new QWidget(this);
    auto* sketchLay = new QHBoxLayout(m_sketchRow);
    sketchLay->setContentsMargins(0, 4, 0, 4);
    sketchLay->setSpacing(8);

    auto* sketchIconLbl = new QLabel("\xf0\x9f\x8f\x97  Disegno:", m_sketchRow);
    sketchIconLbl->setObjectName("hintLabel");
    sketchLay->addWidget(sketchIconLbl);

    auto* sketchFileBtn = new QPushButton(
        "\xf0\x9f\x93\x82  Carica disegno / PDF", m_sketchRow);
    sketchFileBtn->setObjectName("actionBtn");
    sketchFileBtn->setFixedWidth(190);
    sketchLay->addWidget(sketchFileBtn);

    m_sketchFileLbl = new QLabel("Nessun file", m_sketchRow);
    m_sketchFileLbl->setObjectName("hintLabel");
    sketchLay->addWidget(m_sketchFileLbl);

    m_sketchNotes = new QLineEdit(m_sketchRow);
    m_sketchNotes->setPlaceholderText(
        "Quote / note  es: 50x30x10mm, acciaio, foro \xc3\x98 8...");
    sketchLay->addWidget(m_sketchNotes, 1);

    m_btnSketchGen = new QPushButton(
        "\xf0\x9f\x8f\x97  Genera modello 3D", m_sketchRow);
    m_btnSketchGen->setObjectName("actionBtn");
    m_btnSketchGen->setFixedWidth(170);
    sketchLay->addWidget(m_btnSketchGen);

    m_sketchRow->setVisible(false);
    lay->addWidget(m_sketchRow);

    /* ── CloudCompare — pannello "prossimamente" (cat = 9) ── */
    m_cloudCompareRow = new QWidget(this);
    auto* ccLay = new QVBoxLayout(m_cloudCompareRow);
    ccLay->setContentsMargins(8, 8, 8, 8);
    ccLay->setSpacing(6);

    auto* ccBanner = new QLabel(m_cloudCompareRow);
    ccBanner->setObjectName("hintLabel");
    ccBanner->setOpenExternalLinks(true);
    ccBanner->setWordWrap(true);
    ccBanner->setAlignment(Qt::AlignCenter);
    ccBanner->setText(
        "<b>\xf0\x9f\x94\xb5 CloudCompare \xe2\x80\x94 in arrivo</b><br><br>"
        "\xf0\x9f\x95\x90 Questa sezione \xc3\xa8 <b>riservata a sviluppi futuri</b>. "
        "Non esiste ancora un bridge stabile per controllare CloudCompare via AI.<br><br>"
        "<b>Stato attuale:</b><br>"
        "\xe2\x80\xa2 Esiste un demo sperimentale: "
        "<a href='https://github.com/truebelief/CloudCompareMCP'>CloudCompareMCP</a> "
        "(richiede forte personalizzazione)<br>"
        "\xe2\x80\xa2 Il wrapper Python ufficiale \xc3\xa8 "
        "<a href='https://github.com/CloudCompare/CloudComPy'>CloudComPy</a> "
        "(base per qualsiasi bridge futuro)<br>"
        "\xe2\x80\xa2 Prismalux integrer\xc3\xa0 il bridge non appena sar\xc3\xa0 disponibile "
        "una versione stabile<br><br>"
        "<b>Funzionalit\xc3\xa0 pianificate:</b> "
        "carica nuvola punti \xe2\x80\xa2 calcola normali \xe2\x80\xa2 "
        "registrazione ICP \xe2\x80\xa2 distanze \xe2\x80\xa2 filtro/segmentazione \xe2\x80\xa2 "
        "esporta PLY/LAS \xe2\x80\xa2 script libero CloudComPy");
    ccLay->addWidget(ccBanner);

    m_cloudCompareRow->setVisible(false);
    lay->addWidget(m_cloudCompareRow);



    /* Selezione file disegno / PDF */
    connect(sketchFileBtn, &QPushButton::clicked, this, [this]() {
        const QString path = QFileDialog::getOpenFileName(
            this,
            "Seleziona disegno o schema",
            "",
            "Immagini e PDF (*.png *.jpg *.jpeg *.bmp *.webp *.pdf);;"
            "Tutti i file (*)");
        if (path.isEmpty()) return;
        m_sketchFilePath = path;
        m_sketchFileLbl->setText(QFileInfo(path).fileName());
        const QString ext = QFileInfo(path).suffix().toLower();
        m_sketchIsImage = (ext == "png" || ext == "jpg" || ext == "jpeg"
                        || ext == "bmp" || ext == "webp");
    });

    /* Genera modello 3D dal disegno */
    connect(m_btnSketchGen, &QPushButton::clicked, this, [this]() {
        if (m_ai->busy()) {
            m_output->append("\xe2\x9a\xa0  AI occupata, attendi.");
            return;
        }

        const bool isBlender = (m_currentCat == 6);
        const QString notes  = m_sketchNotes->text().trimmed();

        /* nessun input → avvisa */
        if (m_sketchFilePath.isEmpty() && notes.isEmpty()) {
            m_output->append(
                "\xe2\x9a\xa0  Carica un disegno (immagine o PDF) oppure "
                "inserisci quote e note del modello.");
            return;
        }

        const QString sysBlender =
            "Sei un esperto di Blender Python API (bpy). "
            "Analizza il disegno tecnico e genera SOLO codice Python puro "
            "eseguibile in Blender che ricrea il modello 3D corrispondente. "
            "Usa bpy.ops, bpy.data e bpy.context. "
            "Se le dimensioni non sono esplicite, usa proporzioni visive. "
            "Rispondi SOLO con il blocco codice Python tra ``` e ```, senza spiegazioni.";

        const QString sysFreecad =
            "Sei un esperto di FreeCAD Python API. "
            "Analizza il disegno tecnico e genera SOLO codice Python puro "
            "eseguibile in FreeCAD che ricrea il modello 3D corrispondente. "
            "Usa: import FreeCAD, Part; doc = FreeCAD.newDocument('Sketch3D'); "
            "aggiungi solidi, applica vincoli e chiama doc.recompute(). "
            "Se le dimensioni non sono esplicite, usa proporzioni visive. "
            "Rispondi SOLO con il blocco codice Python tra ``` e ```, senza spiegazioni.";

        const QString sys = isBlender ? sysBlender : sysFreecad;

        QString userMsg = notes.isEmpty()
            ? "Crea il modello 3D corrispondente a questo disegno."
            : QString("Crea il modello 3D da questo disegno. "
                      "Quote e note: %1").arg(notes);

        /* Applica modello codice selezionato */
        if (m_codeModelCombo && m_codeModelCombo->count() > 0) {
            const QString sel = m_codeModelCombo->currentData().toString();
            if (!sel.isEmpty() && sel != m_ai->model())
                m_ai->setBackend(m_ai->backend(), m_ai->host(),
                                 m_ai->port(), sel);
            m_codeModelInfo->setText(
                QString("\xf0\x9f\xa4\x96  Usando: <b>%1</b>").arg(
                    m_codeModelCombo->currentText()));
        }

        m_output->clear();
        _setRunBusy(true);
        m_waitLbl->setText("\xf0\x9f\x94\x84  Analisi disegno in corso...");
        m_waitLbl->setVisible(true);
        m_active = true;

        if (!m_sketchFilePath.isEmpty() && m_sketchIsImage) {
            /* Vision: passa immagine base64 */
            QFile f(m_sketchFilePath);
            if (!f.open(QIODevice::ReadOnly)) {
                m_output->append(
                    "\xe2\x9d\x8c  Impossibile aprire il file immagine.");
                m_active = false;
                _setRunBusy(false);
                m_waitLbl->setVisible(false);
                return;
            }
            const QByteArray raw  = f.readAll();
            f.close();
            const QString ext  = QFileInfo(m_sketchFilePath).suffix().toLower();
            const QString mime = (ext == "png")  ? "image/png"  :
                                 (ext == "webp") ? "image/webp" : "image/jpeg";
            m_ai->chatWithImage(sys, userMsg, raw.toBase64(), mime);

        } else if (!m_sketchFilePath.isEmpty() && !m_sketchIsImage) {
            /* PDF: estrai testo con pdftotext */
            QProcess proc;
            proc.start("pdftotext", {m_sketchFilePath, "-"});
            proc.waitForFinished(15000);
            const QString pdfText =
                QString::fromUtf8(proc.readAllStandardOutput()).trimmed();
            if (!pdfText.isEmpty())
                userMsg += "\n\nCONTENUTO SCHEMA PDF:\n" + pdfText.left(3000);
            else
                userMsg += "\n(Schema PDF allegato ma testo non estraibile — "
                           "usa un modello vision o inserisci le quote manualmente)";
            m_ai->chat(sys, userMsg);

        } else {
            /* Solo note/quote testuali */
            m_ai->chat(sys, userMsg);
        }
    });

    /* Ping FreeCAD: tenta connessione TCP su porta 9876 */
    connect(freecadPingBtn, &QPushButton::clicked, this, [this]() {
        QString addr = m_freecadHostEdit->text().trimmed();
        if (addr.isEmpty()) addr = "localhost:9876";
        const QString host = addr.contains(':') ? addr.section(':', 0, 0) : addr;
        const int port     = addr.contains(':') ? addr.section(':', 1).toInt() : 9876;

        m_freecadStatusLbl->setText("\xf0\x9f\x94\x84  Connessione...");
        auto* sock = new QTcpSocket(this);
        sock->connectToHost(host, static_cast<quint16>(port));

        connect(sock, &QTcpSocket::connected, this, [this, sock]() {
            sock->disconnectFromHost();
            sock->deleteLater();
            m_freecadStatusLbl->setText("\xe2\x9c\x85  FreeCAD connesso");
        });
        connect(sock, &QAbstractSocket::errorOccurred, this,
                [this, sock](QAbstractSocket::SocketError) {
            m_freecadStatusLbl->setText("\xe2\x9d\x8c  " + sock->errorString());
            sock->deleteLater();
        });
        QTimer::singleShot(3000, sock, [sock, this]() {
            if (sock->state() != QAbstractSocket::ConnectedState) {
                m_freecadStatusLbl->setText(
                    "\xe2\x9d\x8c  Timeout \xe2\x80\x94 FreeCAD non risponde");
                sock->abort();
                sock->deleteLater();
            }
        });
    });

    /* Esegui script Python in FreeCAD via TCP JSON */
    connect(m_freecadExecBtn, &QPushButton::clicked, this, [this]() {
        if (m_freecadCode.isEmpty()) return;
        QString addr = m_freecadHostEdit->text().trimmed();
        if (addr.isEmpty()) addr = "localhost:9876";
        const QString host = addr.contains(':') ? addr.section(':', 0, 0) : addr;
        const int port     = addr.contains(':') ? addr.section(':', 1).toInt() : 9876;

        QJsonObject payload;
        payload["type"] = "run_script";
        QJsonObject params;
        params["script"] = m_freecadCode;
        payload["params"] = params;
        const QByteArray body =
            QJsonDocument(payload).toJson(QJsonDocument::Compact);

        m_freecadExecBtn->setEnabled(false);
        m_freecadStatusLbl->setText("\xf0\x9f\x94\x84  Invio a FreeCAD...");

        auto* sock = new QTcpSocket(this);
        sock->connectToHost(host, static_cast<quint16>(port));

        connect(sock, &QTcpSocket::connected, this, [sock, body]() {
            sock->write(body);
            sock->flush();
        });
        connect(sock, &QTcpSocket::readyRead, this, [this, sock]() {
            const QByteArray data = sock->readAll();
            sock->disconnectFromHost();
            sock->deleteLater();
            m_freecadExecBtn->setEnabled(true);
            const QJsonObject res = QJsonDocument::fromJson(data).object();
            const QString status  = res["status"].toString();
            if (status == "ok" || status == "success") {
                m_freecadStatusLbl->setText("\xe2\x9c\x85  Eseguito");
                const QString out = res["result"].toString();
                m_output->moveCursor(QTextCursor::End);
                m_output->append(
                    "\n\xe2\x9c\x85  FreeCAD: " +
                    (out.isEmpty() ? "OK" : out));
            } else {
                const QString err = res["message"].toString();
                m_freecadStatusLbl->setText("\xe2\x9d\x8c  " + err);
                m_output->moveCursor(QTextCursor::End);
                m_output->append("\n\xe2\x9d\x8c  FreeCAD errore: " + err);
            }
        });
        connect(sock, &QAbstractSocket::errorOccurred, this,
                [this, sock](QAbstractSocket::SocketError) {
            m_freecadStatusLbl->setText("\xe2\x9d\x8c  " + sock->errorString());
            m_freecadExecBtn->setEnabled(true);
            sock->deleteLater();
        });
    });

    /* Legge il token Bearer scritto dal bridge in ~/.prismalux_office_token.
       Chiamata prima di ogni request — il token cambia ad ogni avvio del bridge. */
    auto _readToken = [this]() {
        const QString tokenFile =
            QDir::homePath() + "/.prismalux_office_token";
        QFile f(tokenFile);
        if (!f.open(QIODevice::ReadOnly | QIODevice::Text)) return;
        m_officeBridgeToken = QString::fromUtf8(f.readAll()).trimmed();
    };

    /* Trova il percorso del bridge relativo all'eseguibile */
    auto _bridgePath = []() -> QString {
        QDir d(QCoreApplication::applicationDirPath());
        for (int i = 0; i < 4; i++) {
            QString p = d.filePath("MCPs/office_bridge/prismalux_office_bridge.py");
            if (QFile::exists(p)) return p;
            d.cdUp();
        }
        return {};
    };

    /* Avvia / ferma bridge Python */
    connect(m_officeStartBtn, &QPushButton::clicked, this, [this, _bridgePath, _readToken]() {
        // Se già in esecuzione → ferma
        if (m_officeBridgeProc &&
            m_officeBridgeProc->state() == QProcess::Running) {
            m_officeBridgeProc->terminate();
            m_officeBridgeProc->waitForFinished(2000);
            m_officeStartBtn->setText("\xe2\x96\xb6  Avvia bridge");
            m_officeStatusLbl->setText("\xe2\x9a\xaa  Bridge fermato");
            return;
        }

        QString path = _bridgePath();
        if (path.isEmpty()) {
            m_officeStatusLbl->setText(
                "\xe2\x9d\x8c  prismalux_office_bridge.py non trovato");
            return;
        }

        if (!m_officeBridgeProc) {
            m_officeBridgeProc = new QProcess(this);
            m_officeBridgeProc->setProcessChannelMode(
                QProcess::MergedChannels);
            connect(m_officeBridgeProc,
                    QOverload<int,QProcess::ExitStatus>::of(
                        &QProcess::finished),
                    this, [this](int, QProcess::ExitStatus) {
                m_officeStartBtn->setText("\xe2\x96\xb6  Avvia bridge");
                m_officeStatusLbl->setText("\xe2\x9a\xaa  Bridge fermato");
            });
        }

        m_officeStatusLbl->setText("\xf0\x9f\x94\x84  Avvio bridge...");
        m_officeBridgeProc->start("python3", {path});
        if (!m_officeBridgeProc->waitForStarted(3000)) {
            // fallback a python su Windows
            m_officeBridgeProc->start("python", {path});
        }
        if (m_officeBridgeProc->state() == QProcess::Running) {
            m_officeStartBtn->setText("\xe2\x8f\xb9  Ferma bridge");
            // Verifica /status dopo 1 secondo
            QTimer::singleShot(1200, this, [this, _readToken]() {
                _readToken();
                QNetworkRequest req(
                    QUrl("http://localhost:6790/status"));
                req.setTransferTimeout(2000);
                req.setRawHeader("Authorization",
                    ("Bearer " + m_officeBridgeToken).toUtf8());
                auto* r = m_officeNam->get(req);
                connect(r, &QNetworkReply::finished, this, [this, r]() {
                    r->deleteLater();
                    if (r->error() == QNetworkReply::NoError) {
                        QJsonObject obj =
                            QJsonDocument::fromJson(r->readAll()).object();
                        QJsonObject libs = obj["libraries"].toObject();
                        QStringList ok;
                        for (auto it = libs.begin(); it != libs.end(); ++it)
                            if (it.value().toBool()) ok << it.key();
                        m_officeStatusLbl->setText(
                            "\xe2\x9c\x85  " +
                            (ok.isEmpty() ? "Bridge pronto (nessuna lib)" :
                             "Pronto: " + ok.join(", ")));
                    } else {
                        m_officeStatusLbl->setText(
                            "\xe2\x9a\xa0  Bridge avviato (verifica fallita)");
                    }
                });
            });
        } else {
            m_officeStatusLbl->setText(
                "\xe2\x9d\x8c  Errore avvio (python3 non trovato?)");
        }
    });

    /* POST /execute → invia codice office al bridge */
    connect(m_officeExecBtn, &QPushButton::clicked, this, [this, _readToken]() {
        if (m_officeCode.isEmpty()) return;

        _readToken();  // aggiorna token in caso il bridge sia stato riavviato
        QJsonObject payload;
        payload["code"] = m_officeCode;
        QByteArray body =
            QJsonDocument(payload).toJson(QJsonDocument::Compact);

        QNetworkRequest req(QUrl("http://localhost:6790/execute"));
        req.setHeader(QNetworkRequest::ContentTypeHeader,
                      "application/json; charset=utf-8");
        req.setRawHeader("Authorization",
            ("Bearer " + m_officeBridgeToken).toUtf8());
        req.setTransferTimeout(30000);

        m_officeExecBtn->setEnabled(false);
        m_officeStatusLbl->setText("\xf0\x9f\x94\x84  Esecuzione...");

        auto* reply = m_officeNam->post(req, body);
        connect(reply, &QNetworkReply::finished, this, [this, reply]() {
            reply->deleteLater();
            m_officeExecBtn->setEnabled(true);
            if (reply->error() == QNetworkReply::NoError) {
                QJsonObject res =
                    QJsonDocument::fromJson(reply->readAll()).object();
                if (res["ok"].toBool()) {
                    m_officeStatusLbl->setText("\xe2\x9c\x85  Completato");
                    m_output->append(
                        "\n\xe2\x9c\x85  Office: " +
                        res["output"].toString());
                } else {
                    m_officeStatusLbl->setText("\xe2\x9d\x8c  Errore");
                    m_output->append(
                        "\n\xe2\x9d\x8c  Office errore:\n" +
                        res["error"].toString());
                }
            } else {
                m_officeStatusLbl->setText(
                    "\xe2\x9d\x8c  Bridge non raggiungibile");
                m_output->append(
                    "\n\xe2\x9d\x8c  Bridge non raggiungibile (avvialo prima).");
            }
        });
    });

    /* ── Riga selezione modello (visibile per tutte le categorie) ── */
    m_codeModelRow = new QWidget(this);
    auto* codeModelLay = new QHBoxLayout(m_codeModelRow);
    codeModelLay->setContentsMargins(0, 2, 0, 2);
    codeModelLay->setSpacing(8);

    auto* codeModelLbl = new QLabel(
        "\xf0\x9f\xa4\x96  Modello:", m_codeModelRow);
    codeModelLbl->setObjectName("hintLabel");
    codeModelLay->addWidget(codeModelLbl);

    m_codeModelCombo = new QComboBox(m_codeModelRow);
    m_codeModelCombo->setMinimumWidth(200);
    m_codeModelCombo->setToolTip(
        "Modello LLM usato per questa sezione.\n"
        "Indipendente dal modello globale.\n"
        "I modelli evidenziati in verde sono consigliati per la categoria attiva.");
    /* Popola subito con il modello corrente come fallback */
    m_codeModelCombo->addItem(m_ai->model().isEmpty()
        ? "(modello globale)" : m_ai->model(), m_ai->model());
    codeModelLay->addWidget(m_codeModelCombo);

    /* Pulsante per aggiornare la lista modelli */
    auto* codeModelRefresh = new QPushButton(
        "\xf0\x9f\x94\x84", m_codeModelRow);
    codeModelRefresh->setFixedWidth(32);
    codeModelRefresh->setToolTip("Aggiorna lista modelli");
    codeModelLay->addWidget(codeModelRefresh);

    /* Etichetta con raccomandazioni */
    m_codeModelInfo = new QLabel(m_codeModelRow);
    m_codeModelInfo->setObjectName("hintLabel");
    m_codeModelInfo->setWordWrap(false);
    /* Hint iniziale per cat 0 — Studio */
    m_codeModelInfo->setText(
        "\xe2\x9c\xa8 Consigliati: <b>mistral</b>, <b>llama3</b>, <b>qwen3</b>"
        " \xe2\x80\x94 buona comprensione e spiegazione");
    m_codeModelInfo->setTextFormat(Qt::RichText);
    codeModelLay->addWidget(m_codeModelInfo, 1);

    m_codeModelRow->setVisible(true);
    lay->addWidget(m_codeModelRow);

    /* Aggiorna lista modelli su richiesta */
    connect(codeModelRefresh, &QPushButton::clicked, this, [this]() {
        m_ai->fetchModels();
    });
    connect(m_ai, &AiClient::modelsReady, this, [this](const QStringList& models) {
        const QString current = m_codeModelCombo->count() > 0
            ? m_codeModelCombo->currentData().toString() : m_ai->model();
        m_codeModelCombo->blockSignals(true);
        m_codeModelCombo->clear();
        for (const QString& m : models)
            m_codeModelCombo->addItem(m, m);
        /* Evidenzia i modelli consigliati per codice */
        /* Evidenzia in verde i modelli particolarmente adatti a qualsiasi categoria */
        static const QStringList kGoodModels = {
            "coder", "code", "deepseek", "starcoder", "codellama",
            "llava", "vision", "phi4",
            "mistral", "llama3", "qwen3", "qwen2.5", "gemma3",
            "deepseek-r1", "command-r"
        };
        for (int i = 0; i < m_codeModelCombo->count(); ++i) {
            const QString name = m_codeModelCombo->itemText(i).toLower();
            for (const QString& kw : kGoodModels) {
                if (name.contains(kw)) {
                    m_codeModelCombo->setItemData(
                        i, QColor("#00a37f"), Qt::ForegroundRole);
                    break;
                }
            }
        }
        /* Ripristina selezione precedente */
        int idx = m_codeModelCombo->findData(current);
        if (idx < 0) idx = m_codeModelCombo->findText(current);
        if (idx >= 0) m_codeModelCombo->setCurrentIndex(idx);
        m_codeModelCombo->blockSignals(false);
    });

    /* ── Input + pulsanti affiancati ── */
    auto* inputRow = new QWidget(this);
    auto* inputLay = new QHBoxLayout(inputRow);
    inputLay->setContentsMargins(0, 0, 0, 0);
    inputLay->setSpacing(8);

    m_inputArea = new QTextEdit(inputRow);
    m_inputArea->setObjectName("chatInput");
    m_inputArea->setPlaceholderText(QString::fromUtf8(kPlaceholders[0]));
    m_inputArea->setMaximumHeight(90);
    m_inputArea->setMinimumHeight(60);
    inputLay->addWidget(m_inputArea, 1);

    auto* btnCol = new QVBoxLayout;
    btnCol->setSpacing(6);

    m_btnRun = new QPushButton("\xe2\x96\xb6  Esegui", inputRow);
    m_btnRun->setObjectName("actionBtn");
    m_btnRun->setFixedWidth(110);

    m_waitLbl = new QLabel(inputRow);
    m_waitLbl->setStyleSheet(
        "color:#E5C400;font-style:italic;font-size:11px;");
    m_waitLbl->setVisible(false);
    m_waitLbl->setWordWrap(true);

    btnCol->addWidget(m_btnRun);
    btnCol->addWidget(m_waitLbl);
    btnCol->addStretch();
    inputLay->addLayout(btnCol);
    lay->addWidget(inputRow);

    /* ── Output AI ── */
    m_output = new QTextEdit(this);
    m_output->setObjectName("chatLog");
    m_output->setReadOnly(true);
    m_output->setPlaceholderText(
        "L'output dell'AI appare qui...\n\n"
        "\xf0\x9f\x8d\xba  Invocazione riuscita. Gli dei ascoltano.");
    lay->addWidget(m_output, 1);

    /* ── Avvia / Stop tool (bottone unificato) ── */
    connect(m_btnRun, &QPushButton::clicked, this, [this] {
        if (m_active) { m_ai->abort(); return; }
        const int navIdx = m_navList->currentRow();
        const int subIdx = m_cmbSub->currentIndex();
        if (navIdx < 0 || subIdx < 0) return;
        if (navIdx == 9) {
            m_output->setPlainText(
                "\xf0\x9f\x94\xb5 CloudCompare \xe2\x80\x94 funzionalit\xc3\xa0 non ancora disponibile.\n\n"
                "Il bridge CloudComPy \xc3\xa8 in sviluppo. "
                "Verr\xc3\xa0 integrato non appena sar\xc3\xa0 disponibile una versione stabile.");
            return;
        }


        QString userMsg = m_inputArea->toPlainText().trimmed();

        /* Categoria Documenti: estrae testo PDF se caricato */
        if (navIdx == 5 && !m_pdfPath.isEmpty()) {
            QProcess proc;
            proc.start("pdftotext", {m_pdfPath, "-"});
            if (!proc.waitForFinished(15000)) {
                m_output->append(
                    "\xe2\x9a\xa0  pdftotext non risponde. "
                    "Assicurati che poppler-utils sia installato "
                    "(sudo apt install poppler-utils).");
                return;
            }
            QString pdfText = QString::fromUtf8(
                proc.readAllStandardOutput()).trimmed();
            if (pdfText.isEmpty()) {
                m_output->append(
                    "\xe2\x9a\xa0  Impossibile estrarre testo dal PDF. "
                    "Il file potrebbe essere scansionato (immagine).");
                return;
            }
            /* Antepone il documento come contesto */
            userMsg = userMsg.isEmpty()
                ? "DOCUMENTO:\n" + pdfText
                : "DOCUMENTO:\n" + pdfText + "\n\nRICHIESTA:\n" + userMsg;
        }

        if (userMsg.isEmpty()) {
            m_output->append(
                "\xe2\x9a\xa0  Inserisci del testo oppure carica un PDF.");
            return;
        }
        const char* sys = kSysPrompts[navIdx][subIdx];
        if (!sys) return;
        runTool(QString::fromUtf8(sys), userMsg);
    });

    connect(m_ai, &AiClient::token,    this, &StrumentiPage::onToken);
    connect(m_ai, &AiClient::finished, this, &StrumentiPage::onFinished);
    connect(m_ai, &AiClient::error,    this, &StrumentiPage::onError);
    connect(m_ai, &AiClient::aborted,  this, [this] {
        m_active = false;
        m_waitLbl->setVisible(false);
        _setRunBusy(false);
        m_output->append("\n\xe2\x8f\xb9  Interrotto.");
    });
}

/* ══════════════════════════════════════════════════════════════
   runTool
   ══════════════════════════════════════════════════════════════ */
void StrumentiPage::runTool(const QString& sys, const QString& userMsg) {
    if (m_ai->busy()) {
        m_output->append(
            "\xe2\x9a\xa0  Un'altra operazione e' in corso. Attendi.");
        return;
    }
    /* Applica il modello scelto nella riga modello (per tutte le categorie) */
    if (m_codeModelCombo && m_codeModelCombo->count() > 0) {
        const QString sel = m_codeModelCombo->currentData().toString();
        if (!sel.isEmpty() && sel != m_ai->model())
            m_ai->setBackend(m_ai->backend(), m_ai->host(),
                             m_ai->port(), sel);
        m_codeModelInfo->setText(
            QString("\xf0\x9f\xa4\x96  Usando: <b>%1</b>").arg(
                m_codeModelCombo->currentText()));
    }

    /* Iniezione RAG: se attiva, prepend contesto al system prompt */
    QString finalSys = sys;
    if (m_ragCheck && m_ragCheck->isChecked() && !m_ragChunks.isEmpty()) {
        const QString ctx = ragBuildContext(userMsg);
        if (!ctx.isEmpty())
            finalSys = "Usa il seguente contesto estratto dai documenti dell'utente "
                       "per rispondere in modo preciso. "
                       "Cita il contesto quando rilevante.\n\n"
                       "CONTESTO DOCUMENTI:\n" + ctx +
                       "\n\n---\n\n" + sys;
    }

    m_output->clear();
    m_waitLbl->setText("\xf0\x9f\x94\x84  Elaborazione in corso...");
    m_waitLbl->setVisible(true);
    m_active = true;
    _setRunBusy(true);
    m_ai->chat(finalSys, userMsg);
}

/* ══════════════════════════════════════════════════════════════
   Slot AI
   ══════════════════════════════════════════════════════════════ */
void StrumentiPage::onToken(const QString& t) {
    if (!m_active) return;
    m_waitLbl->setVisible(false);
    QTextCursor c(m_output->document());
    c.movePosition(QTextCursor::End);
    c.insertText(t);
    m_output->ensureCursorVisible();
}

void StrumentiPage::onFinished(const QString& full) {
    if (!m_active) return;
    m_active = false;
    m_waitLbl->setVisible(false);
    _setRunBusy(false);
    m_output->append("\n" + QString(40, QChar(0x2500)));

    /* ── Blender / Office / FreeCAD: estrai codice Python dal blocco ```...``` ── */
    if ((m_currentCat == 6 || m_currentCat == 7 || m_currentCat == 8) && !full.isEmpty()) {
        // Lambda locale: estrae il primo blocco ```python...``` o ```...```
        auto extractCode = [](const QString& text) -> QString {
            int start = text.indexOf("```python");
            if (start != -1) {
                start = text.indexOf('\n', start) + 1;
                int end = text.indexOf("```", start);
                if (end != -1) return text.mid(start, end - start).trimmed();
            }
            start = text.indexOf("```");
            if (start != -1) {
                start += 3;
                if (start < text.size() && text[start] == '\n') start++;
                int end = text.indexOf("```", start);
                if (end != -1) return text.mid(start, end - start).trimmed();
            }
            return text.trimmed();
        };

        const QString code = extractCode(full);
        if (m_currentCat == 6) {
            m_blenderCode = code;
            if (!m_blenderCode.isEmpty()) {
                m_blenderExecBtn->setEnabled(true);
                m_blenderStatusLbl->setText(
                    "\xf0\x9f\x90\x8d  Codice pronto \xe2\x80\x94 premi Esegui in Blender");
            }
        } else if (m_currentCat == 7) { // Office
            m_officeCode = code;
            if (!m_officeCode.isEmpty()) {
                m_officeExecBtn->setEnabled(true);
                m_officeStatusLbl->setText(
                    "\xf0\x9f\x93\x84  Codice pronto \xe2\x80\x94 premi Esegui in Office");
            }
        } else { // cat 8 — FreeCAD
            m_freecadCode = code;
            if (!m_freecadCode.isEmpty()) {
                m_freecadExecBtn->setEnabled(true);
                m_freecadStatusLbl->setText(
                    "\xf0\x9f\x94\xa9  Codice pronto \xe2\x80\x94 premi Esegui in FreeCAD");
            }
        }
    }
}

void StrumentiPage::onError(const QString& msg) {
    if (!m_active) return;
    m_active = false;
    m_waitLbl->setVisible(false);
    _setRunBusy(false);
    m_output->append(
        QString("\n\xe2\x9d\x8c  Errore: %1").arg(msg));
    m_output->append(
        "\xf0\x9f\x92\xa1  Verifica la connessione al backend AI.");
}

void StrumentiPage::_setRunBusy(bool busy)
{
    if (busy) {
        m_btnRun->setText("\xe2\x8f\xb9  Stop");
        m_btnRun->setProperty("danger", true);
    } else {
        m_btnRun->setText("\xe2\x96\xb6  Esegui");
        m_btnRun->setProperty("danger", false);
    }
    m_btnRun->setEnabled(true);
    P::repolish(m_btnRun);
}

/* Stub richiesti dall'header */
QWidget* StrumentiPage::buildSubPage(const QStringList&, const QString&) { return nullptr; }
QWidget* StrumentiPage::buildStudio()           { return nullptr; }
QWidget* StrumentiPage::buildScritturaCreativa(){ return nullptr; }
QWidget* StrumentiPage::buildRicerca()          { return nullptr; }
QWidget* StrumentiPage::buildLibri()            { return nullptr; }
QWidget* StrumentiPage::buildProduttivita()     { return nullptr; }
QString  StrumentiPage::sysPromptForAction(int, int) { return {}; }

/* ══════════════════════════════════════════════════════════════
   RAG in-page — indicizzazione e retrieval keyword-based
   ══════════════════════════════════════════════════════════════ */

/* Estrae testo grezzo da PDF (pdftotext), TXT o MD */
QString StrumentiPage::ragExtractText(const QString& path)
{
    const QString ext = QFileInfo(path).suffix().toLower();
    if (ext == "pdf") {
        QProcess proc;
        proc.start("pdftotext", {path, "-"});
        if (!proc.waitForFinished(15000)) return {};
        return QString::fromUtf8(proc.readAllStandardOutput());
    }
    /* TXT / MD / CSV / RST — lettura diretta */
    QFile f(path);
    if (!f.open(QIODevice::ReadOnly | QIODevice::Text)) return {};
    QTextStream ts(&f);
    return ts.readAll();
}

/* Divide il testo in chunk di ~chunkSize caratteri con overlap */
QStringList StrumentiPage::ragChunkText(const QString& text,
                                        int chunkSize, int overlap)
{
    QStringList chunks;
    const QString t = text.simplified();
    if (t.isEmpty()) return chunks;
    int pos = 0;
    while (pos < t.size()) {
        chunks << t.mid(pos, chunkSize);
        pos += chunkSize - overlap;
        if (pos >= t.size()) break;
    }
    return chunks;
}

/* Carica un file e aggiunge i chunk all'indice */
void StrumentiPage::ragAddFile(const QString& path)
{
    const QString text = ragExtractText(path);
    if (text.trimmed().isEmpty()) {
        m_ragInfoLbl->setText(
            QString("\xe2\x9d\x8c  Impossibile estrarre testo da: %1")
            .arg(QFileInfo(path).fileName()));
        return;
    }
    const QStringList newChunks = ragChunkText(text);
    m_ragChunks << newChunks;
    const QString fname = QFileInfo(path).fileName();
    if (!m_ragFileNames.contains(fname))
        m_ragFileNames << fname;
    m_ragCheck->setChecked(true);
    m_ragInfoLbl->setText(
        QString("\xf0\x9f\x93\x9a  %1 chunk  \xe2\x80\x94  %2")
        .arg(m_ragChunks.size())
        .arg(m_ragFileNames.count() <= 3
            ? m_ragFileNames.join(", ")
            : m_ragFileNames.first() +
              QString(" +%1 altri").arg(m_ragFileNames.count() - 1)));
}

/* Restituisce i top-k chunk per keyword scoring rispetto alla query */
QString StrumentiPage::ragBuildContext(const QString& query, int topK) const
{
    if (m_ragChunks.isEmpty()) return {};

    /* Parole chiave della query (len > 3, no stopword banali) */
    static const QSet<QString> stopwords = {
        "che","con","del","della","delle","dei","gli","per","una",
        "uno","non","nel","nei","sul","sui","dalla","dalle","sulle"
    };
    const QStringList words = query.toLower()
        .split(QRegularExpression("[\\W_]+"), Qt::SkipEmptyParts);
    QStringList keywords;
    for (const QString& w : words)
        if (w.length() > 3 && !stopwords.contains(w))
            keywords << w;

    if (keywords.isEmpty()) {
        /* Nessuna keyword utile: restituisci i primi topK chunk */
        QStringList out;
        for (int i = 0; i < qMin(topK, m_ragChunks.size()); ++i)
            out << m_ragChunks[i];
        return out.join("\n\n---\n\n");
    }

    /* Punteggio: quante keyword appaiono nel chunk */
    QVector<QPair<int,int>> scores; /* (score, idx) */
    for (int i = 0; i < m_ragChunks.size(); ++i) {
        const QString low = m_ragChunks[i].toLower();
        int score = 0;
        for (const QString& kw : keywords)
            if (low.contains(kw)) score++;
        if (score > 0) scores.append({score, i});
    }

    /* Ordina per score decrescente */
    std::sort(scores.begin(), scores.end(),
              [](const QPair<int,int>& a, const QPair<int,int>& b){
                  return a.first > b.first; });

    QStringList out;
    const int n = qMin(topK, static_cast<int>(scores.size()));
    for (int i = 0; i < n; ++i)
        out << m_ragChunks[scores[i].second];

    /* Fallback se nessun chunk ha score > 0 */
    if (out.isEmpty())
        for (int i = 0; i < qMin(topK, m_ragChunks.size()); ++i)
            out << m_ragChunks[i];

    return out.join("\n\n---\n\n");
}
