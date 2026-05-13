# Prismalux — TODO prossima sessione

> Aggiornato: 2026-05-14 | Build: `cd gui && cmake --build build -j$(nproc)`

## Test GUI da completare

### Intelligenza Artificiale
- [x] **Chat RAG** ✅ — testato 2026-05-14: streaming OK 46.6s, "Lavoro completato"; fix toggle ▶️ think block (2026-05-14: thinking via message.thinking non passava a extractedThink)
- [~] Pipeline agenti — rimosso/sostituito dall'Agente Autonomo (gestione automatica)
- [x] **Agente Autonomo** ✅ — testato 2026-05-14: "quanto fa 5×7?" → THOUGHT+ACTION(calc)+OBSERVATION+risposta "35"
- [x] **Think mode** ✅ — testato 2026-05-14: Off/Auto/On visibili, Auto attivo, budget slider 2×
- [ ] Tool use nativo — testare con `qwen3:4b`: "qual è il contenuto del file /etc/hostname?" → CHAT con RAG → attende tool call leggi_file

### Strumenti / Multimedia
- [x] **Multimedia: Audio AI** ✅ — testato 2026-05-14: cattura voce OK
- [ ] Multimedia: Graphviz — genera un grafo
- [ ] Stable Diffusion locale — `pip install diffusers transformers accelerate torch` poi "Controlla"

### LAN & WAN
- [ ] Android server — avvia, scansiona QR
- [ ] GNS3 — connetti e crea topologia base

### Impostazioni
- [x] **Cambia tema** ✅ — testato, 23 temi funzionanti
- [x] **Modalità calcolo** ✅ — testato 2026-05-14: CPU/GPU/Misto visibili, salvataggio OK
- [~] NPU — N/A (hardware non presente)

### Generale
- [x] **Tutti i 10 tab** ✅ — testato 2026-05-14: nessun crash

## Già testato ✅
- Blender: "crea un cubo" + "crea una sfera" (2026-05-13)
- VAD con sox: "Conversa" in Intelligenza Artificiale (2026-05-14)
- Cron: job eseguito, fix `<think>` strip nel log (2026-05-14)

## Non testabile (software non installato)
- Anki, Godot GDScript, Cytoscape, RDKit, Bioconda, Avogadro
