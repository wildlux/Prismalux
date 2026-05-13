# Prismalux — TODO
> Aggiornato: 2026-05-13 | Build: `gui/build` | Test: `gui/build_tests`

## Comandi rapidi

```bash
cmake --build build -j$(nproc)                                    # GUI
cmake -B build_tests -DBUILD_TESTS=ON && cmake --build build_tests -j$(nproc)  # Test
ctest --test-dir build_tests -j4 --output-on-failure              # Tutti i test
ctest --test-dir build_tests -R NomeSuite --output-on-failure     # Una suite
```

## Stato test (37 suite — 35/37 PASS, 2026-05-13)

- **35/37 PASS** — tutte le suite no-Ollama passano al 100%
- `SimulatoreAlgos` — FLAKY in `-j4` (contention CPU), PASS standalone 2.8s
- `AiStress` — FAIL per OOM RAM (1.8 GB modello vs ~300 MB liberi) — non bug del codice
- Test Ollama richiedono `mistral:7b-instruct` attivo → serializzati automaticamente via `RESOURCE_LOCK`

## Bug risolti ✅

| Bug | Fix |
|-----|-----|
| SIGSEGV shutdown LanServer | `blockSignals(true)` prima di `stop()` |
| SIGSEGV STT countdown timer | `QPointer<QTimer> safeTick` nelle lambda |
| ThemeManager ABRT Signal 6 | `static inst(nullptr)` — no `qApp` parent |
| Ringraziamenti caratteri ❓ | `"\xe2\x80\x9c" "Costruito..."` literal concat |
| GPU/Misto sempre disabilitati | abilitati anche senza GPU dedicata |
| Banner Agente Autonomo anticipato | spostato al primo invio effettivo |

## Feature completate (2026-05-13)

| Feature | File |
|---------|------|
| `AiErrorWidget` riutilizzabile + retry + ⚙ Impostazioni | `widgets/ai_error_widget.h` |
| Pannello errore in Multimedia, Ricerca, APP Controller, LAN&WAN | vari `*_page.cpp` |
| `MainWindow::openSettingsDialog()` Q_INVOKABLE | `mainwindow.h/cpp` |
| NPU Intel/AMD in Hardware | `manutenzione_page.cpp` |
| Knowledge MCP spostato in `MCPs/knowledge_mcp/` | `server.py`, `agenti_page_knowledge.cpp` |

## Checklist GUI manuale (golden path — non ancora verificata)

- [ ] Tutti i 10 tab si aprono senza crash
- [ ] Chat RAG streaming + abort funzionano
- [ ] Pipeline agenti 2+ agenti in sequenza
- [ ] Agente Autonomo ciclo ReAct completo
- [ ] Think mode Off/Auto/On + budget slider
- [ ] Multimedia: Audio AI, Stable Diffusion, Graphviz
- [ ] APP Controller: Blender, Anki, Godot GDScript
- [ ] Ricerca: Cytoscape, RDKit, Bioconda, Avogadro
- [ ] LAN & WAN: Android server QR, GNS3
- [ ] Impostazioni: presets, modalità calcolo, NPU, 23 temi

## Feature implementate (2026-05-13 sessione 3)

| Feature | Difficoltà | File |
|---------|-----------|------|
| Banner zRAM quando RAM < 20% | Facile | `manutenzione_page.h/cpp` |
| Tool use nativo Ollama (function calling) | Media | `ai_client.h/cpp`, `agenti_page_pipeline.cpp`, `agenti_page_tools.cpp` |
| VAD voice loop con sox (+ fallback 12s arecord) | Difficile | `agenti_page_stt.cpp` |

**Note implementazione:**
- **zRAM banner**: `updateHWLabel()` mostra il banner quando `avail_mb/mem_mb < 20%` — solo Linux
- **Tool use nativo**: `setActiveTools()` → Ollama invia `tool_calls` → `onNativeToolCall()` → `runToolCall()` → `replyWithTool()`. Attivo solo in CHAT singola con Tools ON + backend Ollama. Tools: calc, ricerca, python, leggi_file, lista_file
- **VAD**: se `sox` installato → `sox silence 1 0.1 1% 1 2.0 1%` con terminazione automatica al silenzio; altrimenti `arecord` da 12s (voice loop) / 6s (pulsante singolo). Installare sox: `sudo apt install sox`

## Feature completate (2026-05-13 sessione 4)

| Feature | File |
|---------|------|
| Stable Diffusion locale (diffusers, no A1111) — radio Locale/A1111 | `stable_diffusion_widget.h/cpp`, `MCPs/stable_diffusion_local/sd_local.py` |
| Cartelle MCP per tutti i 15 MCP referenziati (README + porte) | `MCPs/freecad_mcp/` `anki_mcp/` `kicad_mcp/` `tinymcp/` `obs_mcp/` `godot_mcp/` `gns3_mcp/` `cytoscape_mcp/` `rdkit_mcp/` `bioconda_mcp/` `avogadro_mcp/` `graphviz_mcp/` `stable_diffusion_local/` |
| Blender MCP community addon scaricato in ADDONS_INSTALLAZIONE | `MCPs/blender_addon/ADDONS_INSTALLAZIONE/blender_mcp_community.py` |
| Test cubo Blender background mode (Python API verificata) | `/tmp/prismalux_cube_test.blend` 94 KB ✅ |

## Pendente

- [ ] Test manuale golden path GUI completo
- [ ] Test tool use nativo con modello qwen3:8b (richiede Ollama attivo)
- [ ] Test VAD con sox installato (`sudo apt install sox`)
- [ ] Test SD locale: `pip install diffusers transformers accelerate torch` poi premi Controlla nel tab Genera Immagini
