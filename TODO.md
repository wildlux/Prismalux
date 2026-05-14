# Prismalux — TODO prossima sessione

> Aggiornato: 2026-05-14 | Build: `cd gui && cmake --build build -j$(nproc)`

## Test GUI da completare

### Intelligenza Artificiale
- [x] **Chat RAG** ✅ — testato 2026-05-14: streaming OK 46.6s, "Lavoro completato"; fix toggle ▶️ think block (2026-05-14: thinking via message.thinking non passava a extractedThink)
- [~] Pipeline agenti — rimosso/sostituito dall'Agente Autonomo (gestione automatica)
- [x] **Agente Autonomo** ✅ — testato 2026-05-14: "quanto fa 5×7?" → THOUGHT+ACTION(calc)+OBSERVATION+risposta "35"
- [x] **Think mode** ✅ — testato 2026-05-14: Off/Auto/On visibili, Auto attivo, budget slider 2×
- [x] **Tool use nativo** ✅ — 2026-05-14: checkbox "🔧 Tools" nella toolbar; in modalità Chat attiva Ollama function calling (leggi_file/lista_file/calc/cerca_web/python); in Agente Autonomo sempre attivo
- [x] **Drag & drop RAG inline** ✅ — 2026-05-14: pulsante "📎 RAG" nella barra input apre zona RagDropWidget collapsibile; contesto iniettato nella pipeline

### Strumenti / Multimedia
- [x] **Multimedia: Audio AI** ✅ — testato 2026-05-14: cattura voce OK
- [x] **Multimedia: Graphviz** ✅ — 2026-05-14: rendering DOT→PNG funziona (dot v14.1.2); shape "roundedbox" non esiste → usa "box" o "rectangle"
- [x] **Stable Diffusion locale** ✅ — 2026-05-14: fix deadlock pipe (no image_b64 su stdout con --out), progress bar step-by-step, callback_on_step_end; per testare: `pip install diffusers transformers accelerate torch` poi "Controlla"

### LAN & WAN
- [x] **Android server QR** ✅ — 2026-05-14: aggiunto "📱 QR Connetti" sempre visibile (mostra http://IP:porta senza avviare il server); QR APK e Pagina Download rimangono attivi solo con server ON
- [x] **App rete locale multi-PC** ✅ — 2026-05-14: route `/web` in LanServer serve pagina HTML chat (dark UI, streaming ndjson, history, system prompt); pulsante "🌐 Chat Web" apre browser
- [x] **GNS3** ✅ — 2026-05-14: server v2.2.59 su porta 3080 OK; topologia SW1+PC1+PC2 creata via REST API; fix: compute_id='local' obbligatorio in POST /nodes (aggiunto ai prompt AI); gns3fy installato

### Impostazioni
- [x] **Cambia tema** ✅ — testato, 23 temi funzionanti
- [x] **Modalità calcolo** ✅ — testato 2026-05-14: CPU/GPU/Misto visibili, salvataggio OK
- [~] NPU — N/A (hardware non presente)

### Generale
- [x] **Tutti i 10 tab** ✅ — testato 2026-05-14: nessun crash

## Implementati in questa sessione (2026-05-14 II)

- [x] **Cerca Paper/Brevetti** ✅ — Tab "🔍 Cerca Paper/Brevetti" in Ricerca; sorgenti: arXiv (Atom XML), Semantic Scholar (JSON), USPTO (JSON); "Analizza con AI" streaming; nessuna API key richiesta

## Già testato ✅
- Blender: "crea un cubo" + "crea una sfera" (2026-05-13)
- VAD con sox: "Conversa" in Intelligenza Artificiale (2026-05-14)
- Cron: job eseguito, fix `<think>` strip nel log (2026-05-14)

## Non testabile (software non installato)
- Anki, Godot GDScript, Cytoscape, RDKit, Bioconda, Avogadro
