# TODO — Prismalux Mobile: Roadmap verso parità Desktop
> Aggiornato: 2026-06-11 (v2) | Legenda: 🔴 alta · 🟡 media · 🟢 bassa | ⬜ aperto · 🔄 in corso · ✅ fatto

---

## STATO ATTUALE (pagine Android, indici 0–14)

| # | Pagina | File | Funzionalità presenti |
|---|--------|------|-----------------------|
| 0 | 🤖 Chat AI | `chat_page.h/cpp` | Streaming, bolle, TTS read, RAG inject, stop, file allegati (da Camera), emoji modello 📱📶☁️ |
| 1 | 📚 Studio | `studio_page.h/cpp` | 15 materie, 6 modalità (Spiega/Quiz/Esercizio/Flashcard/Mappa/Riassumi), stats quiz, CCNA DB |
| 2 | 💼 Lavoro AI | `lavoro_page.h/cpp` | Analizza CV (PDF+zlib), Lettera candidatura, Prepara colloquio, Traduci CV EN |
| 3 | 🎬 OBS | `obs_page.h/cpp` | WebSocket OBS controllo scene/record |
| 4 | 📐 Misure | `misure_page.h/cpp` | Fotogrammetria, planimetria touch Shoelace |
| 5 | 📷 Camera | `camera_page.h/cpp` | OCR visuale → prependContext (HAVE_MULTIMEDIA) |
| 6 | 🔌 MCP | `mcpaddons_page.h/cpp` | Lista/avvio 18 MCP |
| 7 | 🔋 BLE | `ble_page.h/cpp` | Chat RFCOMM AES-256-GCM (HAVE_BLE) |
| 8 | 🎙️ Audio | `audio_page.h/cpp` | TTS QTextToSpeech + STT Whisper upload |
| 9 | ⚙️ Impostazioni | `settings_page.h/cpp` | Server, modello, temi, RAG, download LLM locale |
| 10 | ℹ️ Info | `info_page.h/cpp` | Versione, crediti, donazione |
| 11 | 🎓 Impara | `impara_page.h/cpp` | Tutor interattivo + Quiz AI con difficoltà |
| 12 | 🔬 Ricerca | `ricerca_page.h/cpp` | Paper scientifico / Brevetto / Doc tecnico |
| 13 | π Matematica | `matematica_page.h/cpp` | Sequenza/Costanti/N-esimo/Espressione + AI risoluzione |
| 14 | 🎵 Sintetizzatore | `sintetizzatore_page.h/cpp` | Generatore toni, frequenza, durata |

---

## 🔴 ALTA PRIORITÀ — Nuove pagine (funzionalità core mancanti)

### A1 ✅ Assistente AI Categorico — `pages/assistente_page.h/cpp`
**Equivalente desktop:** `main_tools.h` (StrumentiPage, tab Studio/Scrittura/Ricerca/Libri/Produttività/Documenti)

La funzione più usata del desktop: griglia di azioni AI per categoria, area I/O condivisa.

Categorie da implementare (array `kSysPrompts[6][8]` copiabile direttamente):
- **Studio** — spiega, flashcard, riassumi, domande esame, mappa ASCII, esercizi
- **Scrittura Creativa** — storia, continua storia, crea personaggio, poesia, dialogo, trama 3 atti
- **Ricerca** — ricerca approfondita, confronto pro/contro, fact-check, bibliografia APA, multi-prospettiva
- **Libri** — analisi testo, riassunto capitoli, analisi personaggi, scheda libro, domande discussione
- **Produttività** — piano progetto, MoSCoW, email professionale, agenda riunione, 6 cappelli de Bono, obiettivi SMART
- **Documenti PDF** — analizza, riassumi, estrai dati, rispondi dal doc, critica, azioni

Layout mobile: `QComboBox` categoria → griglia 2×N bottoni azione → `QTextEdit` input + output

Aggiunge in drawer: voce "🛠 Assistente" (indice 15)

---

### A2 ⬜ Oracle / Chat rapida con pillole — `pages/oracle_page.h/cpp`
**Equivalente desktop:** `main_oracle.h` (OracoloPage — chat singola con pillole rapide)

Chat LLM diretta senza pipeline, con azioni rapide tocco-singolo:
- Pillole (scrollabili orizzontalmente): Matematica / Finanza / Dev / Scrivi / Aiuto / Analizza / Codice
- Campo testo + Invia
- Allegato immagine (vision) — `QFileDialog` o integrazione Camera
- Bottone 📊 Grafico — se la risposta contiene formula `y=f(x)`, emette segnale → ChartPage (A6)
- TTS "Leggi" su ogni bolla

Aggiunge in drawer: voce "🔮 Oracolo" (indice 16)

---

### A3 ✅ GraphMemory mobile — `graph_memory_mobile.h/cpp`
**Equivalente desktop:** `graph_memory.h/cpp`

Port quasi-diretto:
- SQLite (`~/.prismalux/graph_memory_mobile.db`) — `QtSql` già disponibile
- API: `addNode / addEdge / neighbours / searchNodes / toDot / exportTxt`
- Connessione univoca per istanza via `m_connName` (stesso pattern)
- Segnale `changed()` per refresh UI
- **Rimuovere**: `toDot` complesso (Graphviz non disponibile su Android) → `toDot` solo ritorna la stringa DOT, visualizzazione testuale
- Path: `QStandardPaths::AppDataLocation` invece di `~/.prismalux/`

Usato da: A4 (MultiAgent), A5 (Hermes), B3 (Chat memoria)

---

### A4 ⬜ Multi-Agente mobile — `pages/multi_agent_page.h/cpp`
**Equivalente desktop:** `agenti_multi_page.h/cpp` (tab [9])

Versione mobile semplificata:
- Campo "Obiettivo" → `Genera Piano` → MasterAgent → JSON `{"task":"...","subtasks":[...]}`
- Esecuzione sequenziale BFS su `depends_on`
- Lista sub-task con icone stato ⏳ ✅ ❌ (scrollable)
- Tab Risultato testo / JSON raw
- Se A3 disponibile: risultati → nodo GraphMemory + badge "memoria: N nodi"

Aggiunge in drawer: voce "🕸️ Multi-Agente" (indice 17)

---

### A5 ⬜ Hermes / Memoria utente — `pages/hermes_page.h/cpp`
**Equivalente desktop:** `main_ai_knowledge.cpp` + `KNOWLEDGE_USER/` + MCP `knowledge_mcp`

Sistema memoria persistente per la chat — funzionalità ad alto valore percepito:
- File `~/.prismalux_mobile/user_knowledge.md` — testo libero, sezioni (Bio / Preferenze / Progetti / Tecnico)
- Bottone "📖 Salva in Memoria" su ogni bolla AI → dialog sezione + append/replace
- Agente estrattore: dopo ogni risposta AI, analizza automaticamente se c'è qualcosa da memorizzare
- Bottone "🧠 Riflessione" → AI legge tutta la memoria e produce insights
- Toggle in ChatPage: "Usa memoria Hermes" → inietta `user_knowledge.md` nel system prompt
- Se A3 (GraphMemory) disponibile: memoria anche come nodi SQLite

Aggiunge in drawer: voce "🧠 Hermes Memoria" (indice 18) **oppure** sezione collassabile in SettingsPage

---

### A6 ⬜ Grafico / ChartWidget mobile — `pages/chart_page.h/cpp` o widget inline
**Equivalente desktop:** `main_graph.h` + `OracoloPage::requestShowInGrafico`

Visualizzazione grafica da formula o dati:
- Input: formula `y=f(x)` → campionamento → `QPainter` custom (no QtCharts necessario)
- Input: scatter `QVector<QPointF>` → plot punti
- Zoom/pan touch con `QScroller`
- Colori tema mobile (sfondo scuro, linea accent)
- Integrazione con OracoloPage (A2): pillola "Grafico" emette segnale, ChatPage mostra bottone 📊 sulle bolle con formule

Aggiunge in drawer: voce "📊 Grafico" (indice 19) **oppure** dialog overlay (più mobile-friendly)

---

### A7 ⬜ FinanzaPage — `pages/finanza_page.h/cpp`
**Equivalente desktop:** `pratico_calcs.h` (PraticoPage, tab [7] in StrumentiPage)

- **TFR**: anni servizio, RAL, coefficiente 13,5%, rivalutazione 1,5%+75% FOI, imposta sostitutiva
- **Codice Fiscale automatico**: `calcolaCodiceFiscale()` + `cercaBelfiore()` (~150 comuni; algoritmo D.M.1976)
- **730 semplificato**: scaglioni IRPEF 2024, detrazione per figli, calcolo imposta netta
- **Regime Forfettario / PIVA**: imposta sostitutiva 15%/5%, contributi INPS fissi
- **Compila da RAG**: AI → JSON → pre-fill form

Aggiunge in drawer: voce "💰 Finanza" (indice 20)

---

### A8 ⬜ FileAiPage — `pages/file_ai_page.h/cpp`
**Equivalente desktop:** `main_tools_file.h` (StrumentiFilePage)

- **Analisi PDF** — `QFileDialog` → PDF parser (usa `extractPdfText()` da `lavoro_page.cpp`, già presente!) → AI riassunto/domande/dati
- **Analisi CSV** — parsing, statistiche colonne, AI insights
- **Wiki & Web** — campo URL → fetch via `QNetworkAccessManager` → AI analisi
- **Share Intent Android** — riceve file da altre app: `ACTION_SEND` intent filter in `AndroidManifest.xml`
- Copia risultato negli appunti con un tap

Aggiunge in drawer: voce "📁 File AI" (indice 21)

---

## 🟡 MEDIA PRIORITÀ — Estensioni pagine esistenti

### B1 ⬜ ChatPage — storia persistente + TTFT + export + vision
**File:** `chat_page.h/cpp`

Sotto-task:
- **Storia SQLite**: `ChatHistory` mobile — ogni sessione in `QStandardPaths::AppDataLocation`; bottom sheet con lista sessioni (Nuova / Carica / Cancella)
- **TTFT label**: `QElapsedTimer` avviato a ogni `sendMessage()`, fermato a primo token → label `⚡ Nms` sopra bolla AI
- **Export**: bottone "Esporta" → menu (TXT / MD) → `QFileDialog::getSaveFileName` su Android
- **Vision integrata**: bottone "📷 Allega immagine" in ChatPage (separato da CameraPage) → `QFileDialog` → `toBase64()` → `AiClient::chatVision()`

---

### B2 ⬜ LavoroPage — estensioni desktop
**File:** `lavoro_page.h/cpp`

Funzionalità presenti su desktop ma assenti su Android:
- **Tracker offerte** (`QTableWidget`): azienda, ruolo, data, stato (Da inviare/Inviata/Colloquio/Rifiutata/Offerta), note — persistenza JSON
- **Cover letter separata**: secondo `QTextEdit` dedicato + "Genera Cover Letter" con AI
- **Foto profilo CV**: `QPushButton` → `QFileDialog` → `QLabel` con pixmap circolare 48×48
- **Email di candidatura**: bottone → body email pre-compilato con dati offerta

---

### B3 ⬜ ChatPage — integrazione Hermes/GraphMemory
**File:** `chat_page.h/cpp` — dipende da A3 e A5

- Toggle "Usa memoria" in header chat (collapse group)
- Prima di ogni invio: `GraphMemory::searchNodes(userInput)` → top-3 nodi → inietta nel system prompt
- Bottone "💾 Salva" su ogni bolla AI → dialog sezione → `append()` in `user_knowledge.md`

---

### B4 ⬜ MatematicaPage — Risolvi Passi + 52 formule
**File:** `matematica_page.h/cpp`

- `QComboBox` con 52 formule predefinite (fisica/geometria/algebra/chimica)
  - Esempi: Teorema di Pitagora, Energia cinetica, Legge di Ohm, Derivata, ecc.
- Bottone 🔀 → shuffle formula casuale
- Prompt: `"Risolvi passo-passo usando la formula: <formula>. Dati: <input utente>"`
- Output in `QTextEdit` con passi numerati

---

### B5 ⬜ RicercaPage — Analisi Fenomeni + Carta Astrale + GPS
**File:** `ricerca_page.h/cpp`

- **Analisi Fenomeni**: aggiungi voce al `m_modeCombo` esistente → prompt analisi scientifica + upload file testuale
- **Carta Astrale**: campi nome/data nascita/lat/lon → prompt strutturato → output carta testuale
- **GPS auto**: se permesso `ACCESS_FINE_LOCATION` già concesso (per BLE), leggi posizione → pre-compila lat/lon per Carta Astrale
  - `QGeoPositionInfoSource` — disponibile con `Qt::Positioning`

---

### B6 ⬜ Sicurezza mobile — `pages/security_page.h/cpp`
**Equivalente desktop:** `main_security.h`

4 agenti sequenziali (su mobile non paralleli — risparmio batteria):
- Injection (SQL/XSS/Command injection)
- Segreti (password, token, chiavi API nel codice)
- Memoria (buffer overflow, null pointer, use-after-free)
- Config (HTTPS, certificati, hardcoded)

Input: testo/codice → 4 analisi → sintetizzatore finale

Aggiunge in drawer: voce "🔒 Sicurezza" (indice 22)

---

### B7 ⬜ Simulatore Algoritmi mobile — `pages/simulatore_page.h/cpp`
**Equivalente desktop:** `main_simulator.h`

Visualizzazione step-by-step di algoritmi classici:
- Bubble Sort, Quick Sort, Binary Search, BFS, DFS, Dijkstra
- `QTimer` step-by-step (pulsanti: ▶ Avanti / ⏮ Reset / ⏩ Auto)
- Visualizzazione con rettangoli colorati `QPainter` (array/grafo semplice)
- Adatto allo studio CCNA (BFS/Dijkstra per routing)

Aggiunge in drawer: voce "⚙️ Simulatore" (indice 23)

---

### B8 ⬜ Voice Loop — ChatPage + AudioPage
**Equivalente desktop:** `main_ai_ui.cpp` → `buildToolbarVoiceLoop()` → `m_btnVoiceLoop`

- Toggle "🎙️ Loop Vocale" in ChatPage header
- Se attivo: dopo ogni risposta AI → TTS legge → `AudioPage::startRecording()` → STT → nuova domanda
- Richiede `RECORD_AUDIO` runtime permission (vedi C4)
- Utile per uso hands-free (cucina, palestra)

---

### B9 ⬜ WAN Compute client mobile
**File:** da aggiungere in `settings_page.cpp` o nuovo tab in drawer

- Campo "WAN Server" (IP:porta, default `<server-ip>:11600`) in Impostazioni
- Bottone "Connetti" → `QTcpSocket` → lista task `GET /tasks` HTTP
- Bottone "Esegui task" → invia JSON task → output in testo scrollabile

---

## 🟢 BASSA PRIORITÀ — UX, qualità, native Android

### C1 ⬜ Drawer con animazione smooth
**File:** `mainwindow.cpp::onToggleDrawer()`

```cpp
auto* anim = new QPropertyAnimation(m_drawer, "geometry", this);
anim->setDuration(220);
anim->setEasingCurve(QEasingCurve::OutCubic);
// da: rect con x = -drawerWidth
// a: rect con x = 0
```

---

### C2 ⬜ Auto-update notifica GitHub API
**File:** `mainwindow.cpp` costruttore

- `QTimer::singleShot(10000, ...)` → `GET https://api.github.com/repos/…/releases/latest`
- Compara `tag_name` → se > versione corrente → label "🆕 vX.Y" in header bar
- Tap → `QDesktopServices::openUrl()` pagina release

---

### C3 ⬜ RECORD_AUDIO permesso mancante
**File:** `android/AndroidManifest.xml`

```xml
<uses-permission android:name="android.permission.RECORD_AUDIO"/>
```
+ richiesta runtime in `audio_page.cpp` con `QtAndroidPrivate::requestPermission(...)`.
**Bloccante per B8 (Voice Loop) e AudioPage su Android ≥ 6.0.**

---

### C4 ⬜ Share Intent — ricevi file da altre app
**File:** `android/AndroidManifest.xml` + `mainwindow.cpp`

```xml
<intent-filter>
    <action android:name="android.intent.action.SEND"/>
    <category android:name="android.intent.category.DEFAULT"/>
    <data android:mimeType="*/*"/>
</intent-filter>
```
Gestione in `main.cpp` / `MainWindow` → forwarda a `FileAiPage (A8)` o `ChatPage`.

---

### C5 ⬜ GPS per Carta Astrale
**File:** `ricerca_page.cpp` (dipende da B5)

- `QGeoPositionInfoSource::createDefaultSource(this)` — già disponibile in `Qt::Positioning`
- Bottone "📍 Usa posizione" in Carta Astrale → pre-compila lat/lon automaticamente
- Fallback manuale se GPS non disponibile

---

### C6 ⬜ Shuffle 52 formule matematica
**File:** `matematica_page.cpp` (dipende da B4)

- Bottone 🔀 → `QRandomGenerator::global()->bounded(52)` → imposta formula nel ComboBox
- Animazione breve (font bold flash 300ms)

---

### C7 ⬜ ThermalMonitor integrazione UI
**File:** `mainwindow.cpp` + `thermal_monitor.cpp` (già esiste!)

- `ThermalMonitor` è già presente ma non collegato a nessuna UI
- Aggiungere badge temperatura nell'header bar: 🌡️ **Nms°C** colorato verde/arancio/rosso
- Nascondere se `thermal_monitor.isAvailable()` == false

---

### C8 ⬜ Test Android — nuove pagine
**File:** `tests/test_mobile_logic.cpp`

Aggiungere test unitari per:
- `GraphMemory mobile` (A3): CRUD nodi/archi, BFS depth-2, searchNodes, SQL injection safety
- `FinanzaPage` (A7): TFR con valori noti (es. 30.000€ × 7 anni = X€), C.F. noto
- `MultiAgentPage` (A4): parsing JSON piano, BFS depends_on
- `FileAiPage` (A8): `extractPdfText()` da PDF di test (già disponibile in `lavoro_page.cpp`)
- `AssistentePage` (A1): `sysPromptForAction(cat, idx)` non nullptr

---

## DIPENDENZE CRITICHE

```
A3 (GraphMemory) ──→ A4 (MultiAgent)
                  ──→ A5 (Hermes)
                  ──→ B3 (Chat memoria)

A5 (Hermes)      ──→ B3 (Chat toggle memoria)

A2 (Oracle)      ──→ A6 (Grafico — segnale requestShowInGrafico)

B4 (52 formule)  ──→ C6 (Shuffle)
B5 (Astrale)     ──→ C5 (GPS)
B8 (Voice Loop)  ──→ C3 (RECORD_AUDIO permesso)  [blocca B8 se non fatto]

A8 (FileAi)      ──→ C4 (Share Intent)
```

---

## ORDINE IMPLEMENTAZIONE CONSIGLIATO

```
━━━ FASE 1 — Base AI (±1 settimana) ━━━
  C3 RECORD_AUDIO (30 min — sblocca audio)
  A3 GraphMemory mobile (3-4h — sblocca A4/A5/B3)
  A1 Assistente AI Categorico (4-5h — il più impattante per utente)
  A2 Oracle + pillole rapide (3h)

━━━ FASE 2 — Memoria & Dati (±1 settimana) ━━━
  A5 Hermes memoria utente (4h)
  A7 FinanzaPage (4h — codice portabile dal desktop)
  A8 FileAiPage (3h — riusa extractPdfText da lavoro_page.cpp!)
  B1 ChatPage estensioni (3h: storia+TTFT+export+vision)

━━━ FASE 3 — AI avanzata (±1 settimana) ━━━
  A4 MultiAgentPage (5h — dipende A3)
  A6 Grafico/ChartWidget (3h — QPainter custom)
  B4 Matematica 52 formule (2h)
  B2 LavoroPage estensioni (3h: tracker+cover+foto)

━━━ FASE 4 — UX & native (±3 giorni) ━━━
  B5 Ricerca+Astrale+GPS (3h)
  B7 Simulatore Algoritmi (3h)
  C1 Drawer animazione (1h)
  C2 Auto-update (1h)
  C4 Share Intent (1h)
  C7 ThermalMonitor UI (30min)

━━━ FASE 5 — Sicurezza, WAN, Test (±1 settimana) ━━━
  B6 SecurityPage (4h)
  B8 Voice Loop (2h — dipende C3)
  B9 WAN client (2h)
  B3 ChatPage grafo (2h — dipende A3+A5)
  C8 Test suite (3h)
```

---

## NOTE TECNICHE RAPIDE

| Argomento | Nota |
|-----------|------|
| **DPI su Android** | `QScreen::devicePixelRatio()` — NON `dpiScale()` (non esiste su Android) |
| **Path dati** | `QStandardPaths::AppDataLocation` → `~/.prismalux_mobile/` |
| **Touch scroll** | `applyTouchScroll(sa)` — static helper già in ogni pagina |
| **No lambda senza context** | 4° arg sempre in `connect()` — regola progetto |
| **PDF parser** | `extractPdfText()` già in `lavoro_page.cpp:50-100` — copiare in `file_ai_page.cpp` |
| **CMakeLists.txt** | Aggiungere ogni nuovo `.cpp` in `ANDROID/android_app/CMakeLists.txt` SOURCES |
| **mainwindow.h** | Forward declare + membro puntatore per ogni nuova pagina |
| **DrawerNavItem** | Aggiungere struct `{ icon, label, index }` nell'array `kNavItems[]` in `buildDrawer()` |
| **Permessi nuovi** | Aggiungere in `android/AndroidManifest.xml` + runtime check con `QtAndroidPrivate` |
| **QtSql** | Già disponibile (usato in `chat_page.cpp`) — GraphMemory mobile funziona subito |
| **Qt::Positioning** | Verificare se `Qt6Positioning` è in `CMakeLists.txt` prima di usare `QGeoPositionInfoSource` |

---

## VALORE PER L'UTENTE (ordine impatto)

1. **A1 Assistente AI Categorico** — copre il 60% degli use case di Strumenti desktop
2. **A5 Hermes Memoria** — differenziante, nessuna altra app lo fa su Android
3. **B1 Chat estensioni** — storia+export molto richiesti
4. **A7 FinanzaPage** — valore pratico immediato (TFR, C.F.)
5. **A2 Oracle + pillole** — chat veloce senza aprire il menu
6. **A4 Multi-Agente** — avanzato ma spettacolare
7. **A8 FileAiPage** — analizza PDF/CSV ovunque
8. **B7 Simulatore** — utile per studio CCNA
