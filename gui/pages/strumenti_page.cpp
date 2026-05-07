#include "strumenti_page.h"
#include "stable_diffusion_widget.h"
#include "../prismalux_paths.h"
#include "../lan_server.h"
#include <QScrollArea>
#include <QSpinBox>
#include <QGroupBox>
#include <QNetworkInterface>
#include <QHostAddress>
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
#include <QDragEnterEvent>
#include <QDropEvent>
#include <QMimeData>
#include <QApplication>
#include <QClipboard>
#include "../widgets/qr_code_widget.h"

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
        /* 6 — Verifica brevetto */
        "Sei un esperto di propriet\xc3\xa0 intellettuale e brevetti. "
        "Analizza la descrizione tecnica fornita dall'utente e verifica: "
        "1) Esistono brevetti simili o identici (spiega le classi CPC/IPC pertinenti)? "
        "2) Il concetto \xc3\xa8 brevettabile? (requisiti: novit\xc3\xa0, attivit\xc3\xa0 inventiva, applicabilit\xc3\xa0 industriale) "
        "3) Suggerisci dove cercare: USPTO (patents.google.com), EPO Espacenet, UIBM (uibm.gov.it). "
        "4) Se l'utente incolla il testo di un brevetto: identifica le rivendicazioni principali (claim 1), lo stato, il titolare, la scadenza stimata. "
        "Rispondi in italiano con struttura chiara.",
        /* 7 — Verifica paper scientifico */
        "Sei un esperto di ricerca accademica e metodologia scientifica. "
        "Analizza il paper o l'abstract fornito dall'utente e verifica: "
        "1) Metodologia: il disegno sperimentale \xc3\xa8 robusto? Ci sono bias evidenti? "
        "2) Peer review e impact factor: dove \xc3\xa8 pubblicato? (indica se predatory o affidabile) "
        "3) Riproducibilit\xc3\xa0: dati e codice sono aperti? "
        "4) Citazioni chiave: identifica le fonti pi\xc3\xb9 citate nel campo. "
        "5) Retractions: verifica se il paper \xc3\xa8 stato ritrattato (suggerisci Retraction Watch, PubPeer). "
        "6) Se l'utente fornisce solo un titolo o un'idea: cerca paper esistenti correlati e valutane la rilevanza. "
        "Rispondi in italiano con struttura chiara.",
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
      "\xf0\x9f\x94\x8f Verifica brevetto",
      "\xf0\x9f\x93\x84 Verifica paper",
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
    setAcceptDrops(true);

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

    /* ── Pulsante Cron (non nella catGroup — stile separato) ── */
    auto* cronBtn = new QPushButton(
        "\xe2\x8f\xb1  Cron", catBar);  /* ⏱ */
    cronBtn->setCheckable(true);
    cronBtn->setObjectName("strCatBtn");
    cronBtn->setToolTip(
        "Pianifica comandi periodici con il Cron Scheduler integrato");
    catLay->addWidget(cronBtn);

    /* ── Pulsante LAN Android ── */
    auto* lanAndroidBtn = new QPushButton(
        "\xf0\x9f\x93\xb1  LAN Android", catBar);  /* 📱 */
    lanAndroidBtn->setCheckable(true);
    lanAndroidBtn->setObjectName("strCatBtn");
    lanAndroidBtn->setToolTip(
        "Server LAN per l\xe2\x80\x99" "app PrismaluxMobile Android");
    catLay->addWidget(lanAndroidBtn);

    /* ── Pulsante Stable Diffusion ── */
    auto* sdBtn = new QPushButton(
        "\xf0\x9f\x8e\xa8  Immagini AI", catBar);
    sdBtn->setCheckable(true);
    sdBtn->setObjectName("strCatBtn");
    sdBtn->setToolTip(
        "Text-to-Image con Stable Diffusion (AUTOMATIC1111 / Forge)");
    catLay->addWidget(sdBtn);

    /* ── Pulsante File Search AI ── */
    m_fileSearchBtn = new QPushButton(
        "\xf0\x9f\x97\x82  File AI", catBar);   /* 🗂 */
    m_fileSearchBtn->setCheckable(true);
    m_fileSearchBtn->setObjectName("strCatBtn");
    m_fileSearchBtn->setToolTip(
        "Ricerca file locali con AI: trova documenti rilevanti per parola chiave\n"
        "e falli analizzare dal modello selezionato");
    catLay->addWidget(m_fileSearchBtn);

    /* ── Pulsante Wiki ── */
    m_wikiBtn = new QPushButton(
        "\xf0\x9f\x93\x96  Wiki", catBar);   /* 📖 */
    m_wikiBtn->setCheckable(true);
    m_wikiBtn->setObjectName("strCatBtn");
    m_wikiBtn->setToolTip(
        "Wikipedia + AI: cerca un articolo e fallo elaborare dal modello\n"
        "Lingue: italiano, inglese, francese, tedesco, spagnolo");
    catLay->addWidget(m_wikiBtn);

    /* Cambio categoria */
    connect(catGroup, QOverload<int>::of(&QButtonGroup::idClicked),
            this, [this, actStack, lblSel, cronBtn, lanAndroidBtn, sdBtn](int cat) {
        if (cronBtn) cronBtn->setChecked(false);
        if (m_cronPanel) m_cronPanel->setVisible(false);
        if (lanAndroidBtn) lanAndroidBtn->setChecked(false);
        if (m_lanPanel) m_lanPanel->setVisible(false);
        if (sdBtn) sdBtn->setChecked(false);
        if (m_sdPanel) m_sdPanel->setVisible(false);
        if (m_fileSearchBtn) m_fileSearchBtn->setChecked(false);
        if (m_fileSearchPanel) m_fileSearchPanel->setVisible(false);
        if (m_wikiBtn) m_wikiBtn->setChecked(false);
        if (m_wikiPanel) m_wikiPanel->setVisible(false);
        actStack->setVisible(true);
        lblSel->setVisible(true);
        if (m_inputRow) m_inputRow->setVisible(true);
        m_output->setVisible(true);
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

    /* ── catBar in scroll area: tutti i tab sempre leggibili ── */
    auto* catScroll = new QScrollArea(this);
    catScroll->setFrameShape(QFrame::NoFrame);
    catScroll->setHorizontalScrollBarPolicy(Qt::ScrollBarAsNeeded);
    catScroll->setVerticalScrollBarPolicy(Qt::ScrollBarAlwaysOff);
    catScroll->setWidget(catBar);
    catScroll->setWidgetResizable(false);
    catScroll->setFixedHeight(36);

    lay->addWidget(catScroll);
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
        "per ogni richiesta AI in questa sezione.\n"
        "Puoi anche trascinare PDF/TXT/MD direttamente sulla finestra.");
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
            m_ai->chatWithImage(P::prependKnowledge(sys), userMsg, raw.toBase64(), mime);

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
            m_ai->chat(P::prependKnowledge(sys), userMsg);

        } else {
            /* Solo note/quote testuali */
            m_ai->chat(P::prependKnowledge(sys), userMsg);
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
        for (const QString& m : models) {
            const qint64 sz = m_ai->modelSizeBytes(m);
            m_codeModelCombo->addItem(P::modelIcon(sz, m) + m, m);
        }
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
    m_inputRow = new QWidget(this);
    auto* inputRow = m_inputRow;
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

    /* ── Pannello Cron inline ── */
    m_cronPanel = new QWidget(this);
    {
        auto* cl = new QVBoxLayout(m_cronPanel);
        cl->setContentsMargins(16, 16, 16, 16);
        auto* lbl = new QLabel(
            "\xe2\x8f\xb1  <b>Cron Scheduler</b> \xe2\x80\x94 "
            "Pianifica comandi da eseguire periodicamente.<br>"
            "Il pannello completo \xc3\xa8 disponibile in "
            "<b>Impostazioni \xe2\x86\x92 Sistema \xe2\x86\x92 Manutenzione \xe2\x86\x92 Cron</b>.");
        lbl->setTextFormat(Qt::RichText);
        lbl->setWordWrap(true);
        lbl->setObjectName("hintLabel");
        cl->addWidget(lbl);
        cl->addStretch();
    }
    m_cronPanel->setVisible(false);
    lay->addWidget(m_cronPanel, 1);

    connect(cronBtn, &QPushButton::clicked, this,
            [this, actStack, lblSel, lanAndroidBtn](bool checked) {
        actStack->setVisible(!checked);
        lblSel->setVisible(!checked);
        m_ragRow->setVisible(!checked);
        m_pdfRow->setVisible(false);
        if (m_inputRow) m_inputRow->setVisible(!checked);
        m_output->setVisible(!checked);
        m_cronPanel->setVisible(checked);
        if (checked && lanAndroidBtn) lanAndroidBtn->setChecked(false);
        if (m_lanPanel) m_lanPanel->setVisible(false);
        if (checked && m_fileSearchBtn) m_fileSearchBtn->setChecked(false);
        if (m_fileSearchPanel) m_fileSearchPanel->setVisible(false);
        if (checked && m_wikiBtn) m_wikiBtn->setChecked(false);
        if (m_wikiPanel) m_wikiPanel->setVisible(false);
    });

    /* ── Pannello LAN Android ── */
    m_lanPanel = new QWidget(this);
    {
        /* Helper: primo IP LAN preferendo 192.168.x.x su 10.x.x.x */
        auto localLanIP = []() -> QString {
            QString fallback10;
            for (const QNetworkInterface& iface : QNetworkInterface::allInterfaces()) {
                if (iface.flags().testFlag(QNetworkInterface::IsLoopBack)) continue;
                if (!iface.flags().testFlag(QNetworkInterface::IsUp))      continue;
                for (const QNetworkAddressEntry& e : iface.addressEntries()) {
                    if (e.ip().protocol() != QAbstractSocket::IPv4Protocol) continue;
                    const QString s = e.ip().toString();
                    if (s.startsWith("192.168.")) return s;
                    if ((s.startsWith("10.") || s.startsWith("172.")) && fallback10.isEmpty())
                        fallback10 = s;
                }
            }
            return fallback10.isEmpty() ? "127.0.0.1" : fallback10;
        };

        auto* vbox = new QVBoxLayout(m_lanPanel);
        vbox->setContentsMargins(12, 12, 12, 12);
        vbox->setSpacing(10);

        auto* titleLbl = new QLabel(
            "<b>\xf0\x9f\x93\xb1  Server LAN per Android</b>", m_lanPanel);
        titleLbl->setTextFormat(Qt::RichText);
        vbox->addWidget(titleLbl);

        auto* group = new QGroupBox(m_lanPanel);
        group->setObjectName("LanServerGroup");
        auto* gl = new QVBoxLayout(group);
        gl->setSpacing(8);

        auto* ctrlRow = new QHBoxLayout;
        m_lanToggleBtn = new QPushButton(
            "\xe2\x97\x8b  Server OFF", group);
        m_lanToggleBtn->setCheckable(true);
        m_lanToggleBtn->setObjectName("LanToggleBtn");

        m_lanPortSpin = new QSpinBox(group);
        m_lanPortSpin->setRange(1024, 65535);
        m_lanPortSpin->setValue(11500);
        m_lanPortSpin->setPrefix("Porta ");
        m_lanPortSpin->setObjectName("LanPortSpin");

        ctrlRow->addWidget(m_lanToggleBtn, 1);
        ctrlRow->addWidget(m_lanPortSpin);
        gl->addLayout(ctrlRow);

        m_lanStatusLbl = new QLabel("\xe2\x97\x8b  Fermo", group);
        m_lanStatusLbl->setStyleSheet("color: #9e9e9e;");
        gl->addWidget(m_lanStatusLbl);

        m_lanClientsLbl = new QLabel("Client connessi: 0", group);
        gl->addWidget(m_lanClientsLbl);

        const QString ip = localLanIP();
        auto* ipLbl = new QLabel(
            QString("IP del PC: <b>%1</b>").arg(ip), group);
        ipLbl->setTextFormat(Qt::RichText);
        gl->addWidget(ipLbl);

        auto* noteLbl = new QLabel(
            "<small>Nell\xe2\x80\x99" "app Android: IP = <b>" + ip + "</b>"
            ", Porta = <b>11500</b></small>", group);
        noteLbl->setTextFormat(Qt::RichText);
        noteLbl->setWordWrap(true);
        gl->addWidget(noteLbl);

        /* ── Due bottoni QR affiancati ── */
        auto* qrRow  = new QWidget(group);
        auto* qrRowL = new QHBoxLayout(qrRow);
        qrRowL->setContentsMargins(0, 0, 0, 0);
        qrRowL->setSpacing(8);

        auto* qrApkBtn = new QPushButton(
            "\xf0\x9f\x93\xb1" "  QR Scarica APK", qrRow);          /* 📱 */
        qrApkBtn->setObjectName("actionBtn");
        qrApkBtn->setToolTip("QR code per scaricare direttamente PrismaluxMobile.apk");
        qrApkBtn->setEnabled(false);

        auto* qrPageBtn = new QPushButton(
            "\xf0\x9f\x8c\x90" "  QR Pagina Download", qrRow);      /* 🌐 */
        qrPageBtn->setObjectName("actionBtn");
        qrPageBtn->setToolTip("QR code per aprire la pagina di download nel browser del telefono");
        qrPageBtn->setEnabled(false);

        qrRowL->addWidget(qrApkBtn, 1);
        qrRowL->addWidget(qrPageBtn, 1);
        gl->addWidget(qrRow);

        vbox->addWidget(group);
        vbox->addStretch();

        /* ── Helper: apre dialog QR generico ── */
        auto openQrDialog = [](QPushButton* parent, const QString& url,
                                const QString& title, const QString& subtitle,
                                const QString& note) {
            auto* dlg = new QDialog(parent->window());
            dlg->setWindowTitle(title);
            dlg->setAttribute(Qt::WA_DeleteOnClose);
            auto* vl = new QVBoxLayout(dlg);
            vl->setSpacing(12);
            vl->setContentsMargins(20, 20, 20, 20);

            auto* hdr = new QLabel("<b>" + subtitle + "</b>", dlg);
            hdr->setTextFormat(Qt::RichText);
            hdr->setAlignment(Qt::AlignCenter);
            vl->addWidget(hdr);

            auto* qrw = new QrCodeWidget(url, dlg);
            qrw->setFixedSize(260, 260);
            vl->addWidget(qrw, 0, Qt::AlignCenter);

            auto* urlLbl = new QLabel(QString("<code>%1</code>").arg(url), dlg);
            urlLbl->setTextFormat(Qt::RichText);
            urlLbl->setAlignment(Qt::AlignCenter);
            urlLbl->setTextInteractionFlags(Qt::TextSelectableByMouse);
            vl->addWidget(urlLbl);

            auto* copyBtn = new QPushButton("\xf0\x9f\x93\x8b" "  Copia URL", dlg);  /* 📋 */
            connect(copyBtn, &QPushButton::clicked, dlg, [url, copyBtn]() {
                QApplication::clipboard()->setText(url);
                copyBtn->setText("\xe2\x9c\x85" "  Copiato!");  /* ✅ */
            });
            vl->addWidget(copyBtn);

            if (!note.isEmpty()) {
                auto* noteLbl2 = new QLabel("<small><i>" + note + "</i></small>", dlg);
                noteLbl2->setTextFormat(Qt::RichText);
                noteLbl2->setAlignment(Qt::AlignCenter);
                noteLbl2->setWordWrap(true);
                vl->addWidget(noteLbl2);
            }

            dlg->resize(320, 460);
            dlg->exec();
        };

        /* ── QR 1: download diretto APK ── */
        connect(qrApkBtn, &QPushButton::clicked, this, [this, qrApkBtn, localLanIP, openQrDialog]() {
            if (!m_lanServer || !m_lanServer->isRunning()) return;
            const QString url = QString("http://%1:%2/apk")
                                    .arg(localLanIP()).arg(m_lanServer->port());
            openQrDialog(qrApkBtn, url,
                         "QR — Scarica APK",
                         "\xf0\x9f\x93\xb1" "  Scansiona per scaricare l'APK",  /* 📱 */
                         "Il server LAN deve rimanere attivo durante il download.<br>"
                         "Su Android: consenti installazione da sorgenti sconosciute.");
        });

        /* ── QR 2: pagina HTML di download ── */
        connect(qrPageBtn, &QPushButton::clicked, this, [this, qrPageBtn, localLanIP, openQrDialog]() {
            if (!m_lanServer || !m_lanServer->isRunning()) return;
            const QString url = QString("http://%1:%2/")
                                    .arg(localLanIP()).arg(m_lanServer->port());
            openQrDialog(qrPageBtn, url,
                         "QR — Pagina Download",
                         "\xf0\x9f\x8c\x90" "  Scansiona per aprire la pagina di download",  /* 🌐 */
                         "Si apre nel browser del telefono.<br>"
                         "Da lì puoi scaricare l'APK con un tap.");
        });

        /* ── Toggle ON/OFF server + abilita/disabilita entrambi i bottoni QR ── */
        connect(m_lanToggleBtn, &QPushButton::toggled, this,
                [this, qrApkBtn, qrPageBtn](bool on) {
            if (on) {
                if (!m_lanServer) {
                    m_lanServer = new LanServer(m_ai, this);
                    connect(m_lanServer, &LanServer::statusChanged,
                            this, [this, qrApkBtn, qrPageBtn](bool running) {
                        qrApkBtn->setEnabled(running);
                        qrPageBtn->setEnabled(running);
                        if (running) {
                            m_lanStatusLbl->setText(
                                "\xe2\x97\x8f  Attivo su porta " +
                                QString::number(m_lanServer->port()));
                            m_lanStatusLbl->setStyleSheet(
                                "color: #4caf50; font-weight: bold;");
                        } else {
                            m_lanStatusLbl->setText("\xe2\x97\x8b  Fermo");
                            m_lanStatusLbl->setStyleSheet("color: #9e9e9e;");
                            m_lanClientsLbl->setText("Client connessi: 0");
                        }
                    });
                    connect(m_lanServer, &LanServer::clientConnected,
                            this, [this](const QString&) {
                        m_lanClientsLbl->setText(
                            "Client connessi: " +
                            QString::number(m_lanServer->clientCount()));
                    });
                    connect(m_lanServer, &LanServer::clientDisconnected,
                            this, [this](const QString&) {
                        m_lanClientsLbl->setText(
                            "Client connessi: " +
                            QString::number(m_lanServer->clientCount()));
                    });
                }
                const quint16 port = static_cast<quint16>(m_lanPortSpin->value());
                if (m_lanServer->start(port)) {
                    m_lanToggleBtn->setText("\xe2\x97\x8f  Server ON");
                    m_lanPortSpin->setEnabled(false);
                } else {
                    m_lanToggleBtn->blockSignals(true);
                    m_lanToggleBtn->setChecked(false);
                    m_lanToggleBtn->blockSignals(false);
                    m_lanStatusLbl->setText("\xe2\x9d\x8c  Impossibile aprire la porta");
                    m_lanStatusLbl->setStyleSheet("color: #f44336;");
                }
            } else {
                if (m_lanServer) m_lanServer->stop();
                m_lanToggleBtn->setText("\xe2\x97\x8b  Server OFF");
                m_lanPortSpin->setEnabled(true);
                qrApkBtn->setEnabled(false);
                qrPageBtn->setEnabled(false);
            }
        });
    }
    m_lanPanel->setVisible(false);
    lay->addWidget(m_lanPanel, 1);

    connect(lanAndroidBtn, &QPushButton::clicked, this,
            [this, actStack, lblSel, cronBtn, sdBtn](bool checked) {
        actStack->setVisible(!checked);
        lblSel->setVisible(!checked);
        m_ragRow->setVisible(!checked);
        m_pdfRow->setVisible(false);
        m_codeModelRow->setVisible(false);
        if (m_inputRow) m_inputRow->setVisible(!checked);
        m_output->setVisible(!checked);
        m_lanPanel->setVisible(checked);
        if (checked && cronBtn) cronBtn->setChecked(false);
        if (m_cronPanel) m_cronPanel->setVisible(false);
        if (checked && sdBtn) sdBtn->setChecked(false);
        if (m_sdPanel) m_sdPanel->setVisible(false);
        if (checked && m_fileSearchBtn) m_fileSearchBtn->setChecked(false);
        if (m_fileSearchPanel) m_fileSearchPanel->setVisible(false);
        if (checked && m_wikiBtn) m_wikiBtn->setChecked(false);
        if (m_wikiPanel) m_wikiPanel->setVisible(false);
    });

    /* ── Stable Diffusion panel ── */
    {
        m_sdPanel = new StableDiffusionWidget(this);
        m_sdPanel->setVisible(false);
        lay->addWidget(m_sdPanel, 1);

        connect(sdBtn, &QPushButton::clicked, this,
                [this, actStack, lblSel, cronBtn, lanAndroidBtn](bool checked) {
            actStack->setVisible(!checked);
            lblSel->setVisible(!checked);
            m_ragRow->setVisible(!checked);
            m_pdfRow->setVisible(false);
            m_codeModelRow->setVisible(false);
            if (m_inputRow) m_inputRow->setVisible(!checked);
            m_output->setVisible(!checked);
            m_sdPanel->setVisible(checked);
                if (checked && cronBtn) cronBtn->setChecked(false);
            if (m_cronPanel) m_cronPanel->setVisible(false);
            if (checked && lanAndroidBtn) lanAndroidBtn->setChecked(false);
            if (m_lanPanel) m_lanPanel->setVisible(false);
            if (checked && m_fileSearchBtn) m_fileSearchBtn->setChecked(false);
            if (m_fileSearchPanel) m_fileSearchPanel->setVisible(false);
            if (checked && m_wikiBtn) m_wikiBtn->setChecked(false);
            if (m_wikiPanel) m_wikiPanel->setVisible(false);
        });
    }

    /* ── Pannello Agentic File Search ── */
    m_fileSearchPanel = buildFileSearchPanel();
    m_fileSearchPanel->setVisible(false);
    lay->addWidget(m_fileSearchPanel, 1);

    connect(m_fileSearchBtn, &QPushButton::clicked, this,
            [this, actStack, lblSel, cronBtn, lanAndroidBtn, sdBtn](bool checked) {
        actStack->setVisible(!checked);
        lblSel->setVisible(!checked);
        m_ragRow->setVisible(!checked);
        m_pdfRow->setVisible(false);
        m_codeModelRow->setVisible(false);
        if (m_inputRow) m_inputRow->setVisible(!checked);
        m_output->setVisible(!checked);
        m_fileSearchPanel->setVisible(checked);
        if (checked && cronBtn) cronBtn->setChecked(false);
        if (m_cronPanel) m_cronPanel->setVisible(false);
        if (checked && lanAndroidBtn) lanAndroidBtn->setChecked(false);
        if (m_lanPanel) m_lanPanel->setVisible(false);
        if (checked && sdBtn) sdBtn->setChecked(false);
        if (m_sdPanel) m_sdPanel->setVisible(false);
        if (checked && m_wikiBtn) m_wikiBtn->setChecked(false);
        if (m_wikiPanel) m_wikiPanel->setVisible(false);
    });

    /* ── Pannello LLM Wiki ── */
    m_wikiPanel = buildWikiPanel();
    m_wikiPanel->setVisible(false);
    lay->addWidget(m_wikiPanel, 1);

    connect(m_wikiBtn, &QPushButton::clicked, this,
            [this, actStack, lblSel, cronBtn, lanAndroidBtn, sdBtn](bool checked) {
        actStack->setVisible(!checked);
        lblSel->setVisible(!checked);
        m_ragRow->setVisible(!checked);
        m_pdfRow->setVisible(false);
        m_codeModelRow->setVisible(false);
        if (m_inputRow) m_inputRow->setVisible(!checked);
        m_output->setVisible(checked);    /* mostra output AI per elaborazione */
        m_wikiPanel->setVisible(checked);
        if (checked && cronBtn) cronBtn->setChecked(false);
        if (m_cronPanel) m_cronPanel->setVisible(false);
        if (checked && lanAndroidBtn) lanAndroidBtn->setChecked(false);
        if (m_lanPanel) m_lanPanel->setVisible(false);
        if (checked && sdBtn) sdBtn->setChecked(false);
        if (m_sdPanel) m_sdPanel->setVisible(false);
        if (checked && m_fileSearchBtn) m_fileSearchBtn->setChecked(false);
        if (m_fileSearchPanel) m_fileSearchPanel->setVisible(false);
    });

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
    m_ai->chat(P::prependKnowledge(finalSys), userMsg);
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

/* ══════════════════════════════════════════════════════════════
   buildFileSearchPanel — Agentic File Search
   Ricerca keyword-based nei file locali via Python subprocess,
   poi passa i risultati al modello AI per analisi.
   ══════════════════════════════════════════════════════════════ */
QWidget* StrumentiPage::buildFileSearchPanel()
{
    auto* panel = new QWidget(this);
    auto* lay = new QVBoxLayout(panel);
    lay->setContentsMargins(0, 6, 0, 6);
    lay->setSpacing(8);

    /* Titolo */
    auto* title = new QLabel(
        "\xf0\x9f\x97\x82  <b>Ricerca File con AI</b>  \xe2\x80\x94  "
        "cerca file locali per parola chiave e falli analizzare dal modello", panel);
    title->setTextFormat(Qt::RichText);
    title->setObjectName("cardDesc");
    lay->addWidget(title);

    /* Riga cartella */
    auto* dirRow = new QWidget(panel);
    auto* dirLay = new QHBoxLayout(dirRow);
    dirLay->setContentsMargins(0, 0, 0, 0);
    dirLay->setSpacing(6);
    auto* dirLbl = new QLabel("\xf0\x9f\x93\x81  Cartella:", dirRow);
    dirLbl->setObjectName("cardDesc");
    dirLay->addWidget(dirLbl);
    m_fileSearchDir = new QLineEdit(dirRow);
    m_fileSearchDir->setPlaceholderText(
        QDir::homePath() + "  (lascia vuoto per usare la home)");
    m_fileSearchDir->setText(QDir::homePath());
    dirLay->addWidget(m_fileSearchDir, 1);
    auto* btnBrowse = new QPushButton(
        "\xf0\x9f\x93\x82  Sfoglia", dirRow);
    btnBrowse->setObjectName("actionBtn");
    btnBrowse->setFixedWidth(90);
    dirLay->addWidget(btnBrowse);
    lay->addWidget(dirRow);

    /* Riga query + cerca */
    auto* qRow = new QWidget(panel);
    auto* qLay = new QHBoxLayout(qRow);
    qLay->setContentsMargins(0, 0, 0, 0);
    qLay->setSpacing(6);
    auto* qLbl = new QLabel("\xf0\x9f\x94\x8d  Cerca:", qRow);
    qLbl->setObjectName("cardDesc");
    qLay->addWidget(qLbl);
    m_fileSearchQuery = new QLineEdit(qRow);
    m_fileSearchQuery->setPlaceholderText(
        "cosa stai cercando? (es. \"configurazione database\")");
    qLay->addWidget(m_fileSearchQuery, 1);
    auto* btnSearch = new QPushButton(
        "\xf0\x9f\x94\x8d  Cerca", qRow);
    btnSearch->setObjectName("actionBtn");
    btnSearch->setFixedWidth(90);
    qLay->addWidget(btnSearch);
    auto* btnAI = new QPushButton(
        "\xf0\x9f\xa4\x96  Analisi AI", qRow);
    btnAI->setObjectName("actionBtn");
    btnAI->setFixedWidth(100);
    btnAI->setEnabled(false);
    btnAI->setToolTip("Manda i file trovati al modello AI per analisi");
    qLay->addWidget(btnAI);
    lay->addWidget(qRow);

    /* Area risultati */
    m_fileSearchOut = new QTextBrowser(panel);
    m_fileSearchOut->setPlaceholderText(
        "I file trovati appariranno qui...\n\n"
        "Suggerimento: usa termini precisi (nomi funzioni, variabili, argomenti)");
    m_fileSearchOut->setOpenLinks(false);
    m_fileSearchOut->setMinimumHeight(120);
    m_fileSearchOut->setStyleSheet(
        "QTextBrowser { background:#0f172a; color:#e2e8f0;"
        "border:1px solid #334155; border-radius:6px; padding:8px; "
        "font-family: 'JetBrains Mono','Fira Code',monospace; font-size:12px; }");
    lay->addWidget(m_fileSearchOut, 1);

    /* ── Connessioni ── */
    connect(btnBrowse, &QPushButton::clicked, this, [this]() {
        const QString dir = QFileDialog::getExistingDirectory(
            this, "Seleziona cartella da ricercare", m_fileSearchDir->text());
        if (!dir.isEmpty()) m_fileSearchDir->setText(dir);
    });

    connect(btnSearch, &QPushButton::clicked, this, [this, btnSearch, btnAI]() {
        const QString query = m_fileSearchQuery->text().trimmed();
        const QString root  = m_fileSearchDir->text().trimmed();
        if (query.isEmpty()) return;
        if (root.isEmpty() || !QDir(root).exists()) {
            m_fileSearchOut->setPlainText(
                "\xe2\x9d\x8c  Cartella non valida: " + root);
            return;
        }

        btnSearch->setEnabled(false);
        btnAI->setEnabled(false);
        m_fileSearchOut->setPlainText(
            "\xf0\x9f\x94\x8d  Ricerca in corso...");

        if (m_fileSearchProc) {
            m_fileSearchProc->kill();
            m_fileSearchProc->deleteLater();
            m_fileSearchProc = nullptr;
        }

        /* Script Python: walk ricorsivo, filtra per estensione,
           calcola score keyword, ritorna JSON top-8 */
        const QString script = QString::fromUtf8(
            "import os,sys,json\n"
            "root=sys.argv[1]; query=sys.argv[2].lower(); kw=query.split()\n"
            "exts={'.txt','.md','.py','.cpp','.h','.js','.ts','.json','.csv','.rst','.yaml','.toml','.ini','.cfg'}\n"
            "results=[]\n"
            "for dp,dirs,files in os.walk(root):\n"
            "    dirs[:]=[d for d in dirs if not d.startswith('.')]\n"
            "    for f in files:\n"
            "        if os.path.splitext(f)[1].lower() not in exts: continue\n"
            "        path=os.path.join(dp,f)\n"
            "        try:\n"
            "            with open(path,'r',errors='replace') as fp: c=fp.read(8000)\n"
            "            score=sum(1 for k in kw if k in c.lower() or k in f.lower())\n"
            "            if score>0: results.append({'file':path,'score':score,'snippet':c[:600]})\n"
            "        except: pass\n"
            "results.sort(key=lambda x:-x['score'])\n"
            "print(json.dumps(results[:8]))\n");

        m_fileSearchProc = new QProcess(this);
        m_fileSearchProc->setProcessChannelMode(QProcess::MergedChannels);
        connect(m_fileSearchProc,
                QOverload<int,QProcess::ExitStatus>::of(&QProcess::finished),
                this, [this, btnSearch, btnAI, query](int exitCode, QProcess::ExitStatus) {
            const QString raw = QString::fromUtf8(m_fileSearchProc->readAll()).trimmed();
            m_fileSearchProc->deleteLater();
            m_fileSearchProc = nullptr;
            btnSearch->setEnabled(true);

            if (exitCode != 0 || raw.isEmpty()) {
                m_fileSearchOut->setPlainText(
                    "\xe2\x9d\x8c  Errore nella ricerca:\n" + raw.left(300));
                return;
            }

            const QJsonDocument doc = QJsonDocument::fromJson(raw.toUtf8());
            if (!doc.isArray() || doc.array().isEmpty()) {
                m_fileSearchOut->setPlainText(
                    "\xf0\x9f\x8c\xab  Nessun file trovato per: \"" + query + "\"");
                return;
            }

            /* Costruisce HTML risultati */
            QString html;
            html += "<p style='color:#94a3b8;font-size:12px;'>"
                    "\xf0\x9f\x94\x8d  <b>Risultati per:</b> "
                    + query.toHtmlEscaped() + "</p>";

            m_wikiExtract.clear();  /* riuso m_wikiExtract per contesto AI */
            QString aiContext = QString("Query: %1\n\nFile rilevanti trovati:\n\n").arg(query);

            int i = 1;
            for (const QJsonValue& v : doc.array()) {
                const QJsonObject o = v.toObject();
                const QString file    = o["file"].toString();
                const QString snippet = o["snippet"].toString();
                const int score       = o["score"].toInt();

                html += QString(
                    "<div style='background:#1e293b;border:1px solid #334155;"
                    "border-radius:6px;padding:8px;margin:4px 0;'>"
                    "<p style='color:#60a5fa;font-size:11px;margin:0 0 4px 0;font-weight:bold;'>"
                    "[%1] %2 &nbsp;<span style='color:#64748b;font-weight:normal;'>"
                    "(score: %3)</span></p>"
                    "<pre style='color:#94a3b8;font-size:10px;margin:0;white-space:pre-wrap;'>"
                    "%4</pre></div>")
                    .arg(i).arg(file.toHtmlEscaped()).arg(score)
                    .arg(snippet.left(300).toHtmlEscaped());

                aiContext += QString("[%1] %2\n%3\n\n").arg(i).arg(file).arg(snippet);
                i++;
            }

            m_fileSearchOut->setHtml(html);
            /* Salva contesto per il pulsante AI */
            m_fileSearchQuery->setProperty("aiContext", aiContext);
            m_fileSearchQuery->setProperty("aiQuery", query);
            btnAI->setEnabled(true);
        });
        m_fileSearchProc->start(P::findPython(), {"-c", script, root, query});
        QTimer::singleShot(30000, this, [this, btnSearch]() {
            if (m_fileSearchProc) {
                m_fileSearchProc->kill();
                btnSearch->setEnabled(true);
                m_fileSearchOut->append(
                    "\n\xe2\x9a\xa0  Timeout: ricerca interrotta dopo 30s");
            }
        });
    });

    /* Analisi AI dei file trovati */
    connect(btnAI, &QPushButton::clicked, this, [this]() {
        const QString ctx   = m_fileSearchQuery->property("aiContext").toString();
        const QString query = m_fileSearchQuery->property("aiQuery").toString();
        if (ctx.isEmpty()) return;
        const QString sys =
            "Sei un assistente esperto nell'analisi di codice e documenti. "
            "Hai ricevuto snippet di file locali rilevanti per una query. "
            "Analizza i file e rispondi in modo preciso e utile. "
            "Cita i percorsi dei file quando fai riferimento a contenuti specifici. "
            "Rispondi in italiano.";
        runTool(P::prependKnowledge(sys),
                ctx + "\nDomanda: " + query);
    });

    return panel;
}

/* ══════════════════════════════════════════════════════════════
   buildWikiPanel — Wikipedia + AI
   Fetches article via Wikipedia REST API, mostra il contenuto
   e permette di elaborarlo con il modello AI locale.
   ══════════════════════════════════════════════════════════════ */
QWidget* StrumentiPage::buildWikiPanel()
{
    auto* panel = new QWidget(this);
    auto* lay = new QVBoxLayout(panel);
    lay->setContentsMargins(0, 6, 0, 6);
    lay->setSpacing(8);

    /* Titolo */
    auto* title = new QLabel(
        "\xf0\x9f\x93\x96  <b>Wikipedia + AI</b>  \xe2\x80\x94  "
        "cerca un articolo e fallo elaborare dal modello locale", panel);
    title->setTextFormat(Qt::RichText);
    title->setObjectName("cardDesc");
    lay->addWidget(title);

    /* Riga ricerca */
    auto* sRow = new QWidget(panel);
    auto* sLay = new QHBoxLayout(sRow);
    sLay->setContentsMargins(0, 0, 0, 0);
    sLay->setSpacing(6);

    m_wikiQuery = new QLineEdit(sRow);
    m_wikiQuery->setPlaceholderText(
        "argomento Wikipedia (es. \"Intelligenza artificiale\", \"Alan Turing\")");
    sLay->addWidget(m_wikiQuery, 1);

    m_wikiLangCombo = new QComboBox(sRow);
    m_wikiLangCombo->setObjectName("settingsCombo");
    m_wikiLangCombo->setFixedWidth(65);
    m_wikiLangCombo->addItem("it", "it");
    m_wikiLangCombo->addItem("en", "en");
    m_wikiLangCombo->addItem("fr", "fr");
    m_wikiLangCombo->addItem("de", "de");
    m_wikiLangCombo->addItem("es", "es");
    m_wikiLangCombo->setToolTip("Lingua Wikipedia");
    sLay->addWidget(m_wikiLangCombo);

    auto* btnFetch = new QPushButton(
        "\xf0\x9f\x94\x8d  Cerca", sRow);
    btnFetch->setObjectName("actionBtn");
    btnFetch->setFixedWidth(80);
    sLay->addWidget(btnFetch);

    auto* btnElabora = new QPushButton(
        "\xf0\x9f\xa4\x96  Elabora con AI", sRow);
    btnElabora->setObjectName("actionBtn");
    btnElabora->setEnabled(false);
    btnElabora->setToolTip(
        "Invia l\xe2\x80\x99" "articolo Wikipedia al modello per analisi, approfondimento o Q&A");
    sLay->addWidget(btnElabora);
    lay->addWidget(sRow);

    /* Riga azioni rapide (elaborate prompt preimpostati) */
    auto* actRow = new QWidget(panel);
    auto* actLay = new QHBoxLayout(actRow);
    actLay->setContentsMargins(0, 0, 0, 0);
    actLay->setSpacing(4);

    struct WikiAction { const char* label; const char* prompt; };
    static const WikiAction kActions[] = {
        { "\xf0\x9f\x93\x9d  Riassunto", "Riassumi l\xe2\x80\x99" "articolo in 5 punti chiave. Rispondi in italiano." },
        { "\xf0\x9f\xa7\xaa  Verifica fatti", "Identifica i fatti principali verificabili nell\xe2\x80\x99" "articolo e indica le fonti da controllare. Rispondi in italiano." },
        { "\xf0\x9f\x8f\xab  Spiega semplice", "Spiega l\xe2\x80\x99" "argomento come se parlassi a uno studente di liceo. Usa esempi concreti. Rispondi in italiano." },
        { "\xf0\x9f\x94\x97  Approfondisci", "Suggerisci 5 argomenti correlati da approfondire, con una breve spiegazione per ciascuno. Rispondi in italiano." },
    };
    for (const auto& a : kActions) {
        auto* ab = new QPushButton(QString::fromUtf8(a.label), actRow);
        ab->setObjectName("strActBtn");
        ab->setEnabled(false);
        ab->setProperty("wikiPrompt", QString::fromUtf8(a.prompt));
        actLay->addWidget(ab);
        connect(ab, &QPushButton::clicked, this, [this, ab]() {
            if (m_wikiExtract.isEmpty()) return;
            const QString prompt = ab->property("wikiPrompt").toString();
            const QString sys =
                "Sei un assistente esperto. Usa le informazioni dell\xe2\x80\x99" "articolo Wikipedia "
                "come contesto. Rispondi in modo preciso e utile. Rispondi in italiano.";
            runTool(P::prependKnowledge(sys),
                    "[Articolo Wikipedia]\n" + m_wikiExtract + "\n\n" + prompt);
        });
        /* Abilita i pulsanti azione quando l'articolo è caricato —
           uso setProperty per conoscerli poi, oppure teniamo il pointer */
        ab->setObjectName("wikiActBtn");  /* per findChildren in onFetch */
    }
    actLay->addStretch();
    lay->addWidget(actRow);

    /* Stato / wait */
    m_wikiWaitLbl = new QLabel(panel);
    m_wikiWaitLbl->setObjectName("cardDesc");
    m_wikiWaitLbl->setVisible(false);
    lay->addWidget(m_wikiWaitLbl);

    /* Area contenuto Wikipedia */
    m_wikiContent = new QTextBrowser(panel);
    m_wikiContent->setPlaceholderText(
        "Il testo dell\xe2\x80\x99" "articolo Wikipedia appare qui...\n\n"
        "Suggerimento: usa titoli precisi (es. \"Fisica quantistica\" e non \"fisica\")");
    m_wikiContent->setOpenLinks(false);
    m_wikiContent->setMinimumHeight(120);
    m_wikiContent->setStyleSheet(
        "QTextBrowser { background:#0f172a; color:#e2e8f0;"
        "border:1px solid #334155; border-radius:6px; padding:8px; "
        "font-size:13px; }");
    lay->addWidget(m_wikiContent, 1);

    /* ── Connessioni ── */
    /* Enter nel campo query = fetch */
    connect(m_wikiQuery, &QLineEdit::returnPressed, btnFetch, &QPushButton::click);

    auto* nam = new QNetworkAccessManager(panel);

    auto enableActions = [panel, btnElabora]() {
        btnElabora->setEnabled(true);
        const auto btns = panel->findChildren<QPushButton*>("wikiActBtn");
        for (auto* b : btns) b->setEnabled(true);
    };

    connect(btnFetch, &QPushButton::clicked, this, [this, nam, enableActions]() {
        const QString q    = m_wikiQuery->text().trimmed();
        const QString lang = m_wikiLangCombo->currentData().toString();
        if (q.isEmpty()) return;

        m_wikiContent->clear();
        m_wikiExtract.clear();
        m_wikiWaitLbl->setText(
            "\xe2\x8f\xb3  Ricerca su Wikipedia (" + lang + ")...");
        m_wikiWaitLbl->setVisible(true);

        /* Wikipedia REST v1 — summary endpoint */
        const QString encoded = QUrl::toPercentEncoding(q);
        const QUrl url(
            QString("https://%1.wikipedia.org/api/rest_v1/page/summary/%2")
            .arg(lang, encoded));
        QNetworkRequest req(url);
        req.setHeader(QNetworkRequest::UserAgentHeader,
                      "Prismalux/2.8 (wildlux@gmail.com)");
        req.setAttribute(QNetworkRequest::RedirectPolicyAttribute,
                         QNetworkRequest::NoLessSafeRedirectPolicy);

        auto* reply = nam->get(req);
        connect(reply, &QNetworkReply::finished, this,
                [this, reply, q, lang, enableActions]() {
            reply->deleteLater();
            m_wikiWaitLbl->setVisible(false);

            if (reply->error() != QNetworkReply::NoError) {
                m_wikiContent->setPlainText(
                    "\xe2\x9d\x8c  Errore: " + reply->errorString() +
                    "\n\nSuggerimento: verifica l\xe2\x80\x99" "ortografia del titolo "
                    "e prova in altra lingua.");
                return;
            }

            const QJsonDocument doc =
                QJsonDocument::fromJson(reply->readAll());
            const QJsonObject obj = doc.object();

            /* Gestione "disambiguation" e "not found" */
            const QString type = obj["type"].toString();
            if (type == "disambiguation") {
                m_wikiContent->setPlainText(
                    "\xf0\x9f\x93\x8b  Pagina di disambiguazione per \"" + q + "\".\n\n"
                    "Prova un titolo pi\xc3\xb9 specifico, ad esempio:\n"
                    + obj["extract"].toString().left(300));
                return;
            }
            if (type.contains("not_found") || obj["title"].toString().isEmpty()) {
                m_wikiContent->setPlainText(
                    "\xf0\x9f\x8c\xab  Articolo \"" + q + "\" non trovato in " + lang +
                    ".wikipedia.org.\n\n"
                    "Suggerimento: prova in inglese (en) o verifica il titolo.");
                return;
            }

            const QString title   = obj["title"].toString();
            const QString desc    = obj["description"].toString();
            const QString extract = obj["extract"].toString();

            /* Salva per uso AI */
            m_wikiExtract = QString("Titolo: %1\nDescrizione: %2\n\n%3")
                            .arg(title, desc, extract);

            /* Mostra in m_wikiContent */
            QString html;
            html += QString(
                "<h2 style='color:#60a5fa;margin-bottom:4px;'>%1</h2>"
                "<p style='color:#94a3b8;font-size:11px;margin:0 0 12px 0;"
                "font-style:italic;'>%2</p>")
                .arg(title.toHtmlEscaped(), desc.toHtmlEscaped());

            /* Paragrafi */
            for (const QString& para : extract.split("\n\n", Qt::SkipEmptyParts)) {
                html += "<p style='color:#e2e8f0;margin:0 0 8px 0;'>"
                      + para.toHtmlEscaped() + "</p>";
            }
            html += QString(
                "<p style='color:#64748b;font-size:10px;margin-top:12px;'>"
                "\xf0\x9f\x8c\x90  Fonte: <a href='https://%1.wikipedia.org/wiki/%2' "
                "style='color:#3b82f6;'>%1.wikipedia.org/%3</a></p>")
                .arg(lang,
                     QUrl::toPercentEncoding(title),
                     title.toHtmlEscaped());

            m_wikiContent->setHtml(html);
            enableActions();
        });
    });

    connect(btnElabora, &QPushButton::clicked, this, [this]() {
        if (m_wikiExtract.isEmpty()) return;
        const QString sys =
            "Sei un assistente esperto. Usa le informazioni dell\xe2\x80\x99" "articolo Wikipedia "
            "come contesto. L\xe2\x80\x99" "utente ti pone domande sull\xe2\x80\x99" "argomento. "
            "Rispondi in modo preciso, critico e utile. Rispondi in italiano.";
        /* Usa m_output (visibile quando wiki è attivo) tramite runTool */
        runTool(P::prependKnowledge(sys),
                "[Articolo Wikipedia]\n" + m_wikiExtract + "\n\n" +
                (m_fileSearchQuery ? m_fileSearchQuery->text().trimmed() : QString()));
    });

    return panel;
}
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
        if (pos + chunkSize >= t.size()) break;
        pos += chunkSize - overlap;
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

/* ══════════════════════════════════════════════════════════════
   Drag & Drop — rilascia PDF/TXT/MD/CSV direttamente sulla pagina
   per aggiungerli all'indice RAG senza passare dal file dialog.
   ══════════════════════════════════════════════════════════════ */
void StrumentiPage::dragEnterEvent(QDragEnterEvent* e)
{
    if (!e->mimeData()->hasUrls()) { e->ignore(); return; }

    static const QStringList kExt = { "pdf", "txt", "md", "csv", "rst" };
    for (const QUrl& u : e->mimeData()->urls()) {
        if (!u.isLocalFile()) continue;
        const QString ext = QFileInfo(u.toLocalFile()).suffix().toLower();
        if (kExt.contains(ext)) { e->acceptProposedAction(); return; }
    }
    e->ignore();
}

void StrumentiPage::dropEvent(QDropEvent* e)
{
    if (!e->mimeData()->hasUrls()) { e->ignore(); return; }

    static const QStringList kExt = { "pdf", "txt", "md", "csv", "rst" };
    bool added = false;
    for (const QUrl& u : e->mimeData()->urls()) {
        if (!u.isLocalFile()) continue;
        const QString path = u.toLocalFile();
        if (kExt.contains(QFileInfo(path).suffix().toLower())) {
            ragAddFile(path);
            added = true;
        }
    }
    if (added) {
        e->acceptProposedAction();
        /* Attiva automaticamente il checkbox RAG dopo il drop */
        if (m_ragCheck && !m_ragCheck->isChecked())
            m_ragCheck->setChecked(true);
    } else {
        e->ignore();
    }
}
