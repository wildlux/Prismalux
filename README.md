<div align="center">

# 🍺 Prismalux

### *"Costruito per i mortali che aspirano alla saggezza."*

[![C++/Qt6](https://img.shields.io/badge/GUI-C%2B%2B%20%2F%20Qt6-green?style=flat-square&logo=qt)](https://www.qt.io/)
[![Version](https://img.shields.io/badge/versione-3.0-blue?style=flat-square)](CHANGELOG)
[![License](https://img.shields.io/badge/License-MIT-lightgrey?style=flat-square)](LICENSE)
[![Platform](https://img.shields.io/badge/Platform-Linux%20%7C%20Windows%20%7C%20Android%20(WIP)-informational?style=flat-square)](https://github.com/wildlux/Prismalux)

**Piattaforma AI locale. GUI in C++/Qt6.**  <br>
Multi-agente · Agente Autonomo ReAct · anti-allucinazione · RAG in-page + web · matematica locale · 63 simulazioni algoritmi.  <br>
Code Interpreter sandbox · Think mode inline · Memoria automatica · Feedback DPO · Wiki AI  <br>
🎨 Stable Diffusion (AUTOMATIC1111/Forge) · Personalità AI (Jarvis/KITT/Yoda/Snake/Sonic/Mario)  <br>
MCPs: Blender / Office / Anki / KiCAD / Arduino+ESP32 · Network Analyzer · Disegno→3D  <br>
Zero dipendenze cloud · Zero abbonamenti · Tutto locale sul tuo hardware

</div>

---

## Avvio rapido

**Linux / macOS**
```bash
git clone git@github.com:wildlux/Prismalux.git
cd Prismalux/gui
cmake -B build -DCMAKE_BUILD_TYPE=Release && cmake --build build -j$(nproc)
./build/Prismalux_GUI
```

**Windows** — nessuna installazione manuale richiesta:
```
1. Doppio clic su  build.bat           ← compila (una tantum, ~3 min)
2. Doppio clic su  Avvia_Prismalux.bat ← avvia il programma ogni volta
```
`build.bat` rileva automaticamente MSYS2 UCRT64/MINGW64, Qt installer o toolchain portatile.

Prerequisiti: `Qt6`, `cmake`, `gcc/clang`, `Ollama` + almeno un modello.

```bash
curl -fsSL https://ollama.com/install.sh | sh
ollama pull qwen3:8b   # ~5 GB, ottimo italiano + think nativo
```

---

## Build

**Linux (apt):** `sudo apt install qt6-base-dev cmake build-essential`  
**Linux (pacman):** `sudo pacman -S qt6-base cmake gcc`  
**Windows:** doppio clic su `build.bat` (rileva automaticamente MSYS2, Qt installer o toolchain portatile)

**Con test:**
```bash
cmake -B build_tests -DBUILD_TESTS=ON && cmake --build build_tests -j$(nproc)
# 26 suite disponibili — vedi tabella sotto
ctest --test-dir build_tests -j4          # esegui tutto
ctest --test-dir build_tests -R NomeSuite # esegui una suite
```

| Suite | Test | Cosa verifica |
|-------|------|---------------|
| `test_signal_lifetime` | 36+ | Dangling observer, signal leakage |
| `test_rag_engine` | 15 | Chunking, embedding, search, round-trip |
| `test_rag_engine_avanzato` | 35 | Proprietà JLT math, search edge, save/load 1000 chunk, perf |
| `test_code_utils` | 14+ | Estrazione Python da risposta AI |
| `test_random_tool` | 12 | `_inject_random`: keyword detection, range, decimali, grafico |
| `test_lavoro_page` | 37 | Isolamento AiClient, filtri, sincronizzazione modello |
| `test_lavoro_data` | 32 | kOfferte integrità dataset, offerteFiltrate, tipoIcon/livLabel |
| `test_tutor_data` | 24 | SubjectData invarianti strutturali, unicità cross-soggetto |
| `test_app_controller` | 100+ | extractCode, state machine, Anki JSON, KiCAD, MCU |
| `test_programmazione_page` | 46 | isIntentionalError, parseNumbers, widget+segnali |
| `test_thinking_detect` | 13 | _extractThinkingToken, classifyQuery, think-capable detect |
| `test_ai_integration` | 41 | AiClient REALE ↔ Ollama: classifyQuery, AiChatParams, chat¹ |
| `test_ai_stress` | 80+ | Matrice 24 param, stress 20 req, 14 edge case, qualità reale¹ |
| `test_team_collab` | 56+ | Pipeline 4 agenti: Architetto→Dev→Revisore→Tester |
| `test_simulatore_algos` | 54 | BubbleSort/sorting/ricerca/AlgoStep, BigO evalClass |
| `test_formula_parser` | 34 | eval(), sample(), tryExtract(), intervalli, punti |
| `test_matematica_page` | 35 | parseSeq, detectPatternLocal, numbersFromText, widget |
| `test_strumenti_rag` | 21 | ragChunkText, ragExtractText, sysPromptForAction |
| `test_chat_history` | 30 | CRUD sessioni, saveLog/loadLog, remove, edge case |
| `test_chat_history_stress` | 18 | Concorrenza 4 thread, durabilità, 200 append |
| `test_manutenzione_cron` | 23 | cronShouldRun, cronNextRun, detectConfigFmt |
| `test_theme_manager` | 14 | Singleton ThemeManager, lista temi, apply() |
| `test_hardware_monitor` | 11 | hw_detect struttura HWInfo, thread polling |
| `test_knowledge_injection` | 16 | prependKnowledge, cache 30s, toggle QSettings |
| `test_hw_detect_amd` | 14 | AMD detection via DRM sysfs, fallback VRAM 512 MB |
| `test_qr_code_widget` | 18 | QR code generazione, rendering Qt, dialog APK |

> ¹ Richiede Ollama attivo + modello installato. Default: variabile d'ambiente
> `PRISMALUX_TEST_MODEL` (es. `qwen2.5-coder:7b`). Senza modello: 92% suite passa,
> le suite `AiIntegration` e `AiStress` falliscono per "model not found".

---

## Tab GUI

| # | Tab | Shortcut | Contenuto |
|---|-----|----------|-----------|
| 0 | 🤖 Intelligenza Artificiale | Alt+1 | Pipeline 6 agenti · Byzantino · CHAT RAG · **🤖 Agente Autonomo** · TTS/STT |
| 1 | 🛠 Strumenti AI | Alt+2 | Studio · Scrittura · Ricerca · 💼 Cerca Lavoro · Libri · Produttività · Documenti · ⏰ Cron · **🗂 File AI** · **📖 Wiki** · **🎨 Stable Diffusion** · 📱 LAN Android |
| 2 | 💻 Programmazione | Alt+3 | Editor + correzione AI + Agentica + **🧪 Interpreter Python** |
| 3 | π Matematica | Alt+4 | Sequenze→Formula · espressioni locali · costanti alta precisione · 45+ grafici |
| 4 | 🔬 Ricerca | Alt+5 | Paper · Brevetti · Documenti tecnici |
| 5 | 🕹 APP Controller | Alt+6 | Blender · Office · Anki · KiCAD · TinyMCP (Arduino/ESP32/STM32…) · OpenCode |
| 6 | 📚 Impara | Alt+7 | Finanza · 730/IVA/Mutuo/PAC/Pensione · Impara con AI · Sfida te stesso |
| ⚙️ | Impostazioni | — | Hardware · AI Locale · RAG · Voce · Aspetto · Ollama LAN |

---

## Funzionalità principali

### Pipeline Multi-Agente
```
Ricercatore → Pianificatore → Prog.A ┐
                                      ├─ Tester (max 3x) → Ottimizzatore
                              Prog.B ┘
```
Guardie pre-pipeline: math locale zero-AI · tipo task · RAM check · pre-calcolo espressioni.  
Esecuzione codice Python generato dall'AI richiede dialog di conferma esplicita.

### Motore Byzantino (anti-allucinazione)
4 agenti logici: **A** genera · **B** avvocato del diavolo · **C** verifica indipendente · **D** giudica.  
`T = (A ∧ C) ∧ ¬B_valido` — se B trova falle, l'incertezza viene dichiarata, mai nascosta.

### RAG locale
- Formati: `.txt`, `.md`, `.pdf`, `.cpp`, `.py`, `.h`
- Pipeline: chunking → embedding (`nomic-embed-text`) → JLT 256-dim → cosine similarity
- Indicizzazione in background · stop cooperativo · privacy totale (zero cloud)
- **RAG condiviso tra agenti**: nel dialog ⚙️ Configura Agenti, area "📂 RAG condiviso" in cima — i documenti caricati qui sono visibili a *tutti* gli agenti della pipeline, in aggiunta al RAG specifico per ruolo

### Motore Matematico Locale
Parser ricorsivo zero-AI in microsecondi: `sin/cos/tan/asin/acos/atan`, `ln/log/log2/exp`, `sqrt/cbrt`, `gcd/lcm`, `abs/floor/ceil/round`, costanti `pi/e/phi`.  
Precedenza: `()` > `^` > `*/%` > `+-`.

### Simulatore Algoritmi — 63 simulazioni, 13 categorie
Ordinamento (15) · Ricerca (4) · Matematica (10) · Prog.Dinamica (5) · Grafi (7) · Tecniche (3) · Strutture Dati (5) · Stringhe (3) · Greedy (3) · Backtracking (2) · Visualizzazioni (3) · Fisica (1) · Chimica (3).

### Think Mode + Classificatore query
Toggle globale in **Impostazioni → AI Locale → Ragionamento AI**: tre bottoni **Off / Auto / On** + slider budget 1-4×.

| Modalità | Comportamento |
|----------|---------------|
| **Off** | `think=false` sempre, nessun ragionamento |
| **Auto** | Il classificatore decide in base alla complessità della query |
| **On** | `think=true` per i modelli che lo supportano |

Classificatore automatico (modalità Auto):

| Categoria | Criteri | `num_predict` | `think` |
|-----------|---------|---------------|---------|
| `Simple` | ≤30 char, no keyword | 512 | false |
| `Auto` | 30–200 char | default | — |
| `Complex` | >200 char o keyword complesse | ×budget* | true* |

\* Solo per modelli think-capable: `qwen3`, `qwen3.5`, `deepseek-r1`, `qwq`, `qwen2.5`.  
Il budget token (1-4×) è configurabile per gestire il consumo RAM su hardware limitato.

Il ragionamento appare inline nella bolla con toggle collassabile **▶️ Ragionamento (N par.)** → **🔻** per espandere. Funziona con entrambi i formati: campo `thinking` di Ollama e tag `<think>…</think>` di llama.cpp.

### Agente Autonomo (ReAct)

Toggle **🤖 Auto** nella toolbar di Intelligenza Artificiale. L'agente pianifica, usa strumenti e itera autonomamente senza richiedere conferma a ogni passo.

```
THOUGHT: analisi del problema
ACTION: strumento(input)
OBSERVATION: risultato
… (fino a 8 step)
FINAL_ANSWER: risposta finale
```

Strumenti disponibili: `calc` · `ricerca` · `python` · `leggi_file` · `lista_file` · `scrivi_file` (con conferma).

### Code Interpreter Python

Sub-tab **🧪 Interpreter** in Programmazione. Esegue codice Python generato dall'AI in un ambiente isolato.

- **Docker sandbox**: rete disabilitata, 256 MB RAM, nessun accesso al filesystem
- **Fallback locale**: se Docker non è disponibile, file temporaneo + timeout 30s
- Output matplotlib mostrato come immagine PNG inline
- Errore → auto-retry con fix AI (max 3 tentativi)

### RAG con Web Reading

Ogni area RAG ha un pulsante **🌐** per aggiungere contenuto da URL.

- Fetch asincrono con `QNetworkAccessManager`, strip HTML, max 12 KB per pagina
- Il contesto include `[N] dominio/path` come citazione per ogni fonte web
- Formati file: `.txt`, `.md`, `.pdf`, `.cpp`, `.py`, `.h` + qualsiasi URL

### Memoria Automatica Cross-Sessione

Ogni 4 scambi in CHAT RAG, un estrattore silenzioso analizza la conversazione e aggiorna automaticamente il profilo utente in `KNOWLEDGE_USER/user_knowledge.md`. Nessuna azione manuale richiesta.

### Personalità AI

In **Impostazioni → AI Locale** è possibile scegliere una personalità che modifica lo stile di risposta del modello per tutte le chat:

| Personalità | Stile |
|-------------|-------|
| 🚫 Nessuna | Risposta neutra standard |
| 🤖 Jarvis (Tony Stark) | Professionale, preciso, ironia britannica, chiama l'utente "signore" |
| 🚗 KITT (Knight Rider) | Sofisticato, calmo, formale, riferimenti alla guida e sicurezza |
| 🌿 Yoda (Star Wars) | Sintassi invertita, breve, saggio, sereno |
| 🎮 Snake (Metal Gear) | Diretto, tattico, cinismo controllato, frasi brevi e incisive |
| 💨 Sonic | Rapido, energico, spiritoso, leggermente impaziente |
| 🍄 Super Mario | Entusiasta, positivo, "Wahoo!" e "Mamma mia!", sempre incoraggiante |

Il pulsante vocale si aggiorna dinamicamente: **"🎙 Conversa"** → **"🎙 Conversa con Jarvis"** (o altra personalità selezionata).

### Stable Diffusion — Text-to-Image

Pannello **🎨 Stable Diffusion** in Strumenti AI. Si collega a qualsiasi server locale compatibile con l'API AUTOMATIC1111.

- Compatibile con: AUTOMATIC1111, Forge, SD.Next, Stability Matrix
- Lista modelli automatica (`GET /sdapi/v1/sd-models`)
- Generazione con prompt · negative prompt · steps · CFG scale · dimensioni · seed
- Output immagine inline nel pannello con possibilità di salvataggio

Avvia il server prima di usare il pannello (es. `python webui.py --api --listen`).

### Feedback 👍/👎

Ogni risposta AI ha bottoni 👍/👎 nella barra azioni della bolla. Il feedback viene salvato in `~/.prismalux/feedback.jsonl` (prompt · risposta · modello · rating · timestamp) come base per DPO/fine-tuning futuro.

### Icone modelli nelle combo
Tutte le combo di selezione modello mostrano un'icona di prefisso:
- ☁️ — modello cloud (size = 0 in Ollama, es. modelli API remoti)
- 🌍📍 — modello locale (pesa qualcosa, gira sul tuo hardware)

Il nome raw è sempre memorizzato in `Qt::UserRole` — l'icona è solo testo di presentazione.

### LAN Server Android
Due modalità per collegare PrismaluxMobile (app Android) al PC:

| Modalità | Dove si attiva | Porta | Note |
|----------|---------------|-------|------|
| **LAN Server** (proxy Qt) | Strumenti AI → 📱 LAN Android | 11500 (config.) | IP locale, client APK connessi, QR code |
| **Ollama LAN** (direct) | Impostazioni → Ollama LAN | 11434 | Espone Ollama su `0.0.0.0` con un clic |

Nel pannello **Strumenti AI → 📱 LAN Android** sono presenti due bottoni QR (abilitati con server attivo):

- **📱 QR Scarica APK** — scansiona per scaricare direttamente `PrismaluxMobile.apk`
- **🌐 QR Pagina Download** — scansiona per aprire la pagina di benvenuto nel browser del telefono, da cui scaricare l'APK con un tap

La pagina HTML è raggiungibile anche digitando `http://192.168.x.x:11500/` nel browser del telefono.

Il contatore **"Client connessi"** conta solo i dispositivi che hanno usato le API Ollama (app installata), non i browser che visitano la pagina di download.

### Modalità Calcolo LLM
Quattro bottoni in **Impostazioni → Hardware**:

| Modalità | `num_gpu` | Comportamento |
|----------|-----------|---------------|
| **GPU** | `= N layer` (da `/api/show`) | tutti i layer su VRAM dedicata |
| **CPU** | `0` | tutto su RAM, GPU ignorata |
| **Misto GPU+CPU** | `= min(layers, VRAM capacity)` | riempie la VRAM NVIDIA al massimo, overflow su RAM |
| **Doppia GPU** | `-2` (auto) | NVIDIA + Intel iGPU via llama-server CUDA+SYCL |

**GPU** e **Misto** sono sempre cliccabili indipendentemente dall'hardware rilevato — se non è presente una GPU dedicata, Ollama gestisce automaticamente il fallback su CPU.  
La selezione viene salvata su disco e ripristinata all'avvio.  
**Nota VRAM 4 GB**: modello 3–3.5 GB + KV cache + RAG ≈ 3.9 GB. Budget netto = VRAM − 470 MB. Usare **Misto** se il margine è < 200 MB.

### Presets RAM & Contesto lungo

**Impostazioni → AI Locale** include preset a un clic per scenari comuni:

| Preset | num_ctx | num_predict | temp | Note |
|--------|---------|-------------|------|------|
| **8 GB RAM** | 4096 | 1024 | 0.05 | Flash Attention ON — massimo su hardware limitato |
| **📜 Contesto lungo** | 16384 / 8192¹ | default | default | Flash Attention ON + SWA-full — per documenti lunghi |

¹ 16384 se RAM ≥ 16 GB, 8192 se RAM < 16 GB.

**Flag llama-server attivi per impostazione predefinita** (solo backend llama-server):
- `--swa-full` — disabilita la sliding-window attention per Qwen3/Gemma3/Mistral: evita perdita di contesto oltre la finestra SWA
- `cache_type_k/v = q8_0` — KV cache quantizzata a 8 bit → −25–35% RAM senza perdita misurabile di qualità
- `--mlock` — opzionale, toggle "Blocca modello in RAM" in Impostazioni → AI Locale; mantiene il modello in RAM fisica (no swap)
- `flash_attn` — toggle in Impostazioni → AI Locale; accelera l'attenzione su GPU con supporto CUDA/Metal

---

## Backend AI

| Backend | Endpoint | Note |
|---------|----------|------|
| **Ollama** | `127.0.0.1:11434` | Gestione automatica modelli |
| **llama-server** | host+porta personalizzabili | OpenAI-compatibile (llama.cpp) |
| **llama statico** | path binario + .gguf | Embedded, nessun server |

Il modello scelto viene persistito su QSettings e ripristinato all'avvio.

---

## Modelli consigliati

```bash
# GPU 4 GB VRAM (es. GTX 1650 / RX 580)
ollama pull qwen3:4b            # ~2.5 GB — veloce, think nativo  ← entra in VRAM
# Se il modello non entra del tutto, usare modalità Misto in Impostazioni

# 8 GB RAM (solo CPU)
ollama pull qwen3:4b            # ~2.5 GB — veloce, think nativo
# 16-32 GB
ollama pull qwen3:8b            # ~5 GB — ottimo italiano (consigliato)
ollama pull deepseek-r1:7b      # ~5 GB — ragionamento matematico
ollama pull qwen2.5-coder:7b    # ~5 GB — top coding 7B
# 64 GB (es. Xeon)
ollama pull qwen3:30b           # ~18 GB — qualità vicina ai modelli cloud
ollama pull deepseek-r1:32b     # ~20 GB — ragionamento avanzato
# Su AMD GPU lenta: OLLAMA_NUM_GPU=0 ollama serve  (forza CPU + AVX-512)
```

---

## Struttura

```
Prismalux/
├── gui/                    ← GUI C++/Qt6 (CMakeLists.txt, pages/, widgets/, themes/, tests/)
│   ├── lan_server.h/cpp    ← Server TCP LAN: proxy Ollama + serve APK + pagina HTML benvenuto
│   ├── pages/strumenti_page.cpp ← Pannello LAN Android: QR APK + QR Pagina + client APK-only
│   └── pages/manutenzione_page_lan.cpp ← Pannello LAN in Manutenzione (toggle, porta, IP)
├── ANDROID/                ← PrismaluxMobile — client Qt6/Android per Ollama su LAN
│   └── PrismaluxMobile.apk ← APK precompilato (servito via LAN Server)
├── COMPILE_WIN/            ← Toolchain portatile Windows zero-install (setup.bat)
├── llama.cpp/              ← clone llama.cpp
├── models/                 ← modelli GGUF
├── MCPs/                   ← plugin MCP (blender_addon, office_bridge)
├── RAG/                    ← documenti indicizzabili
├── Test/                   ← test Python (benchmark, RAG, onestà AI, math)
├── scripts/                ← crea_appimage.sh, crea_zip_windows.py
├── hybrid_llm/             ← ricerca BLHM (Bayesian Hybrid LLM Model)
├── build.bat               ← Windows: compila il sorgente
├── Avvia_Prismalux.bat     ← Windows: avvia il programma compilato
└── README.md · LICENSE · aggiorna.sh
```

---

## Android — PrismaluxMobile *(in sviluppo)*

Client Qt6/C++ per Android. Si connette a Ollama sul PC di casa via WiFi LAN.

| Schermata | Funzione |
|-----------|----------|
| 🤖 Chat | Streaming token · RAG keyword locale · history 6 turni |
| 📷 Camera | Preview live · analisi vision model · OCR → Chat |
| 🔋 Bluetooth | Scan BLE 8s · badge segnale · copia MAC |
| ⚙ Impostazioni | Host/porta Ollama · lista modelli · temperatura · RAG |

**Requisiti sviluppo:** Qt6 ≥ 6.5 for Android · NDK r25c+ · SDK API 26-34 · CMake ≥ 3.21  
**Codice sorgente:** `ANDROID/android_app/` — vedi [`README_ANDROID.md`](ANDROID/android_app/README_ANDROID.md)

> Implementazione completa pianificata quando il desktop sarà 100% stabile.

---

## Requisiti

**Linux:** `qt6-base-dev cmake build-essential ollama` + opzionale: `piper aplay poppler-utils python3`  
**Windows:** Ollama + (`build.bat` rileva automaticamente MSYS2 / Qt installer / toolchain portatile) + SAPI per TTS (integrato in Windows)

| Script Windows | Funzione |
|----------------|---------|
| `build.bat` | Compila il sorgente C++ (una tantum o dopo aggiornamenti) |
| `Avvia_Prismalux.bat` | Avvia il programma già compilato |
| `COMPILE_WIN\setup.bat` | Scarica toolchain portatile zero-install (~600 MB) |

---

<div align="center">

*"La birra è conoscenza divina — ogni sorso un dato in più."* 🍺

[![GitHub](https://img.shields.io/badge/GitHub-wildlux%2FPrismalux-181717?style=flat-square&logo=github)](https://github.com/wildlux/Prismalux)

</div>
