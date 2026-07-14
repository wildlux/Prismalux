#pragma once
// main_tools_p.h — header privato interno, incluso SOLO dai
// main_tools_*.cpp. Non fare mai #include "main_tools_p.h"
// da file esterni.
//
// Tabelle system prompt / sotto-azioni / placeholder condivise tra
// il builder (buildCatScrollArea) e gli slot (onCatGroupIdClicked,
// onActBtnClicked) di StrumentiPage. Split da main_tools.cpp (TODO D-8).

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
        "Per STL: import Mesh; from pathlib import Path; Mesh.export([obj], str(Path.home()/'Desktop'/'output.stl')). "
        "Per STEP: import Import; from pathlib import Path; Import.export([obj], str(Path.home()/'Desktop'/'output.step')). "
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
    { QT_TRANSLATE_NOOP("Tabelle", "\xf0\x9f\x92\xa1 Spiega concetto"),
      QT_TRANSLATE_NOOP("Tabelle", "\xf0\x9f\x83\x8f Flashcard Q&A"),
      QT_TRANSLATE_NOOP("Tabelle", "\xf0\x9f\x93\x9d Riassunto"),
      QT_TRANSLATE_NOOP("Tabelle", "\xe2\x9d\x93 Domande d'esame"),
      QT_TRANSLATE_NOOP("Tabelle", "\xf0\x9f\x97\xba Mappa concettuale"),
      QT_TRANSLATE_NOOP("Tabelle", "\xf0\x9f\x8f\x8b Esercizi pratici"),
      nullptr, nullptr },
    { QT_TRANSLATE_NOOP("Tabelle", "\xf0\x9f\x93\x96 Genera storia"),
      QT_TRANSLATE_NOOP("Tabelle", "\xe2\x9e\xa1 Continua storia"),
      QT_TRANSLATE_NOOP("Tabelle", "\xf0\x9f\xa7\x99 Crea personaggio"),
      QT_TRANSLATE_NOOP("Tabelle", "\xf0\x9f\x8c\xb8 Scrivi poesia"),
      QT_TRANSLATE_NOOP("Tabelle", "\xf0\x9f\x92\xac Genera dialogo"),
      QT_TRANSLATE_NOOP("Tabelle", "\xf0\x9f\x8e\xac Sviluppa trama"),
      nullptr, nullptr },
    { QT_TRANSLATE_NOOP("Tabelle", "\xf0\x9f\x94\x8d Ricerca approfondita"),
      QT_TRANSLATE_NOOP("Tabelle", "\xe2\x9a\x96 Confronta opzioni"),
      QT_TRANSLATE_NOOP("Tabelle", "\xe2\x9c\x85 Fact-checking"),
      QT_TRANSLATE_NOOP("Tabelle", "\xf0\x9f\x93\x9a Genera bibliografia"),
      QT_TRANSLATE_NOOP("Tabelle", "\xf0\x9f\x94\xad Analisi multi-prospettiva"),
      QT_TRANSLATE_NOOP("Tabelle", "\xf0\x9f\x9b\xa4 Guida how-to"),
      QT_TRANSLATE_NOOP("Tabelle", "\xf0\x9f\x94\x8f Verifica brevetto"),
      QT_TRANSLATE_NOOP("Tabelle", "\xf0\x9f\x93\x84 Verifica paper"),
      nullptr, nullptr },
    { QT_TRANSLATE_NOOP("Tabelle", "\xf0\x9f\x93\x9c Analisi letteraria"),
      QT_TRANSLATE_NOOP("Tabelle", "\xf0\x9f\x93\x91 Riassunto capitoli"),
      QT_TRANSLATE_NOOP("Tabelle", "\xf0\x9f\xa7\x91 Studio personaggi"),
      QT_TRANSLATE_NOOP("Tabelle", "\xf0\x9f\x93\x8b Scheda libro"),
      QT_TRANSLATE_NOOP("Tabelle", "\xf0\x9f\x92\xad Domande discussione"),
      QT_TRANSLATE_NOOP("Tabelle", "\xf0\x9f\x93\x9a Consigli lettura"),
      nullptr, nullptr },
    { QT_TRANSLATE_NOOP("Tabelle", "\xf0\x9f\x93\x8a Piano progetto"),
      QT_TRANSLATE_NOOP("Tabelle", "\xf0\x9f\x93\x8c Lista MoSCoW"),
      QT_TRANSLATE_NOOP("Tabelle", "\xe2\x9c\x89 Bozza email"),
      QT_TRANSLATE_NOOP("Tabelle", "\xf0\x9f\x97\x93 Prepara riunione"),
      QT_TRANSLATE_NOOP("Tabelle", "\xf0\x9f\xa7\xa0 Brainstorming"),
      QT_TRANSLATE_NOOP("Tabelle", "\xf0\x9f\x8e\xaf Obiettivi SMART"),
      QT_TRANSLATE_NOOP("Tabelle", "\xe2\x9a\x96 Matrice decisionale"),
      QT_TRANSLATE_NOOP("Tabelle", "\xf0\x9f\x91\xa8\xe2\x80\x8d\xe2\x9a\x95\xef\xb8\x8f Email al Medico"),
      QT_TRANSLATE_NOOP("Tabelle", "\xf0\x9f\xa7\xa0 Analisi Sentimenti"),
      nullptr },
    { QT_TRANSLATE_NOOP("Tabelle", "\xf0\x9f\x94\x8d Analisi documento"),
      QT_TRANSLATE_NOOP("Tabelle", "\xf0\x9f\x93\x9d Riassunto"),
      QT_TRANSLATE_NOOP("Tabelle", "\xf0\x9f\x94\x8e Estrai informazioni"),
      QT_TRANSLATE_NOOP("Tabelle", "\xe2\x9d\x93 Q&A documento"),
      QT_TRANSLATE_NOOP("Tabelle", "\xf0\x9f\x9a\xa8 Punti critici"),
      QT_TRANSLATE_NOOP("Tabelle", "\xe2\x9c\x85 Punti di azione"),
      nullptr, nullptr },
    /* 6 — Blender MCP */
    { QT_TRANSLATE_NOOP("Tabelle", "\xf0\x9f\x8e\xa8 Cambia Materiale"),
      QT_TRANSLATE_NOOP("Tabelle", "\xe2\x86\x94 Trasla"),
      QT_TRANSLATE_NOOP("Tabelle", "\xf0\x9f\x94\x84 Ruota"),
      QT_TRANSLATE_NOOP("Tabelle", "\xf0\x9f\x93\x90 Scala"),
      QT_TRANSLATE_NOOP("Tabelle", "\xf0\x9f\x91\x81 Visibilit\xc3\xa0"),
      QT_TRANSLATE_NOOP("Tabelle", "\xf0\x9f\x8e\xac Avvia Render"),
      QT_TRANSLATE_NOOP("Tabelle", "\xf0\x9f\x90\x8d Script libero"),
      nullptr },
    /* 7 — Office / LibreOffice */
    { QT_TRANSLATE_NOOP("Tabelle", "\xf0\x9f\x93\x84 Crea documento Word"),
      QT_TRANSLATE_NOOP("Tabelle", "\xf0\x9f\x93\x8a Crea foglio Excel"),
      QT_TRANSLATE_NOOP("Tabelle", "\xf0\x9f\x96\xa5 Crea presentazione"),
      QT_TRANSLATE_NOOP("Tabelle", "\xe2\x9c\x8f Modifica documento"),
      QT_TRANSLATE_NOOP("Tabelle", "\xf0\x9f\x93\x8b Inserisci tabella"),
      QT_TRANSLATE_NOOP("Tabelle", "\xf0\x9f\x93\x88 Grafici e dati"),
      QT_TRANSLATE_NOOP("Tabelle", "\xf0\x9f\x94\xa7 Script libero"),
      nullptr },
    /* 8 — FreeCAD MCP */
    { QT_TRANSLATE_NOOP("Tabelle", "\xf0\x9f\x9f\xa6 Crea Primitiva"),
      QT_TRANSLATE_NOOP("Tabelle", "\xe2\x9c\x8f Crea Schizzo"),
      QT_TRANSLATE_NOOP("Tabelle", "\xe2\x9c\x82 Booleana"),
      QT_TRANSLATE_NOOP("Tabelle", "\xf0\x9f\x93\xa4 Esporta STL/STEP"),
      QT_TRANSLATE_NOOP("Tabelle", "\xf0\x9f\x94\xa7 Modifica propriet\xc3\xa0"),
      QT_TRANSLATE_NOOP("Tabelle", "\xf0\x9f\x93\x90 Vincoli & misure"),
      QT_TRANSLATE_NOOP("Tabelle", "\xf0\x9f\x90\x8d Script libero"),
      nullptr },
    /* 9 — CloudCompare (prossimamente) */
    { QT_TRANSLATE_NOOP("Tabelle", "\xf0\x9f\x94\xb5 Carica nuvola punti"),
      QT_TRANSLATE_NOOP("Tabelle", "\xf0\x9f\x93\x90 Calcola normali"),
      QT_TRANSLATE_NOOP("Tabelle", "\xf0\x9f\x94\x81 Registrazione ICP"),
      QT_TRANSLATE_NOOP("Tabelle", "\xf0\x9f\x93\x8f Calcola distanze"),
      QT_TRANSLATE_NOOP("Tabelle", "\xe2\x9c\x82 Filtra / Segmenta"),
      QT_TRANSLATE_NOOP("Tabelle", "\xf0\x9f\x93\xa4 Esporta PLY/LAS"),
      QT_TRANSLATE_NOOP("Tabelle", "\xf0\x9f\x90\x8d Script libero (CloudComPy)"),
      nullptr },
};

static const char* kPlaceholders[10] = {
    QT_TRANSLATE_NOOP("Tabelle", "Incolla il testo o descrivi il concetto da studiare..."),
    QT_TRANSLATE_NOOP("Tabelle", "Descrivi l'idea, il personaggio o la scena..."),
    QT_TRANSLATE_NOOP("Tabelle", "Inserisci l'argomento da ricercare o l'affermazione da verificare..."),
    QT_TRANSLATE_NOOP("Tabelle", "Incolla il testo del libro o il titolo e l'autore..."),
    QT_TRANSLATE_NOOP("Tabelle", "Descrivi il progetto, il task o l'obiettivo..."),
    QT_TRANSLATE_NOOP("Tabelle", "Incolla il testo del documento oppure carica un PDF con il pulsante sopra..."),
    QT_TRANSLATE_NOOP("Tabelle", "Descrivi cosa fare in Blender (es. 'Cambia il cubo in rosso metallico', 'Sposta il piano a Y=3', 'Ruota 45 gradi sull asse Z')..."),
    QT_TRANSLATE_NOOP("Tabelle", "Descrivi il documento da creare (es. 'Crea una lettera di presentazione professionale', 'Foglio Excel con budget mensile', 'Slide riassuntive su Python')..."),
    QT_TRANSLATE_NOOP("Tabelle", "Descrivi cosa modellare in FreeCAD (es. 'Crea un box 20x10x5mm', 'Sfera di raggio 15mm', 'Esporta il modello attivo in STL')..."),
    /* 9 — CloudCompare (prossimamente) */
    QT_TRANSLATE_NOOP("Tabelle", "CloudCompare — funzionalit\xc3\xa0 in arrivo. Bridge CloudComPy in sviluppo..."),
};
