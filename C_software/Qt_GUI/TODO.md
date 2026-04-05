# TODO — Prismalux Qt GUI

## Completate ✅

### 1. Simboli strani nei grafici ✅
Rimossi i caratteri Unicode problematici (●, █, ◊, ○—○) dalla combo del tipo di grafico.
Le voci ora usano testo semplice: Cartesiano, Torta, Istogramma, Scatter 2D, Grafo 2D, Scatter 3D, Grafo 3D.

### 2. RAG — setup nelle Impostazioni ✅
Aggiunto tab "🔍 RAG" in ImpostazioniPage con:
- Cartella documenti: campo testo + pulsante Sfoglia
- Risultati massimi: QSpinBox (1-20, default 5)
- Stato indice: numero file e data ultima indicizzazione
- Pulsante "Reindicizza ora": conta ricorsivamente i file supportati
  (.txt .md .pdf .csv .json .py .cpp .h .c .rst)
- Impostazioni persistenti via QSettings("Prismalux","GUI") chiavi rag/*

### 3. Analizza fonti da PDF ✅
Aggiunta categoria "📄 Documenti" in StrumentiPage (6a categoria) con:
- 6 azioni: Analisi documento, Riassunto, Estrai informazioni, Q&A documento,
  Punti critici, Punti di azione
- Pulsante "📄 Carica PDF" (visibile solo per categoria Documenti)
- Estrazione testo via pdftotext (poppler-utils) prima della chiamata AI
- Il testo del PDF viene anteposto come contesto alla richiesta utente
- Prerequisito: sudo apt install poppler-utils

## Note tecniche
- pdftotext (poppler-utils) deve essere installato per l'analisi PDF
- Il RAG in Qt GUI è configurazione/indexing UI; la query RAG è ancora nella TUI C
