# Prismalux Mobile — Guida Rapida Sviluppo
> Aggiornato: 2026-06-11 | Leggi prima di ogni sessione Android

---

## BUILD & TEST

```bash
# Build APK debug
cd ANDROID/android_app
cmake -B build-android -DCMAKE_TOOLCHAIN_FILE=$ANDROID_NDK/build/cmake/android.toolchain.cmake \
  -DANDROID_ABI=arm64-v8a -DANDROID_PLATFORM=android-26 ..
cmake --build build-android -j$(nproc)

# Test logici (no device)
cd build-android && ctest --output-on-failure -L no_ble
```

---

## AGGIUNGERE UNA NUOVA PAGINA — Checklist

1. `ANDROID/android_app/pages/nome_page.h` — class + signals/slots
2. `ANDROID/android_app/pages/nome_page.cpp` — implementazione
3. **`CMakeLists.txt`** → aggiungi `pages/nome_page.cpp` in `SOURCES`
4. **`mainwindow.h`** → forward declare + `NomePage* m_nomePage = nullptr;`
5. **`mainwindow.cpp`** → `#include`, costruzione, `m_stack->addWidget(m_nomePage, idx)`, voce drawer
6. **`buildDrawer()`** in `mainwindow.cpp` → aggiungi struct in `kNavItems[]`
7. `android/AndroidManifest.xml` → eventuali nuovi permessi
8. **Test** → `tests/test_mobile_logic.cpp`

---

## PATTERN RICORRENTI DA COPIARE

### Touch scroll (ogni pagina)
```cpp
static void applyTouchScroll(QScrollArea* sa) {
    QScroller::grabGesture(sa->viewport(), QScroller::TouchGesture);
    // ... già in ogni pagina — copia blocco intero
}
```

### Richiesta AI asincrona (pattern standard)
```cpp
// Connect
connect(m_ai, &AiClient::tokenReceived,  this, &MyPage::onToken);
connect(m_ai, &AiClient::finished,       this, &MyPage::onFinished);
connect(m_ai, &AiClient::errorOccurred,  this, &MyPage::onError);
// Start
m_ai->chat(sysPrompt, userMsg);
// Stop
m_ai->abort();
```

### PDF parser (già in lavoro_page.cpp)
```cpp
// extractPdfText() è già implementata in lavoro_page.cpp:50-100
// Per FileAiPage: copiare la funzione statica e usare direttamente
```

### Path dati mobile
```cpp
const QString dataDir = QStandardPaths::writableLocation(
    QStandardPaths::AppDataLocation) + "/prismalux/";
QDir().mkpath(dataDir);
```

---

## STATO TASK PRIORITARI (aggiorna qui)

| Task | Stato | Note |
|------|-------|------|
| C3 RECORD_AUDIO permesso | ✅ | già presente nel Manifest |
| A3 GraphMemory mobile | ✅ | `graph_memory_mobile.h/cpp` |
| A1 Assistente AI Categorico | ✅ | `pages/assistente_page.h/cpp` (indice 15) |
| A2 Oracle + pillole | ✅ | `pages/oracle_page.h/cpp` (indice 16) |
| A5 Hermes memoria | ✅ | `pages/hermes_page.h/cpp` (indice 17) |
| A7 FinanzaPage | ✅ | `pages/finanza_page.h/cpp` (indice 19) |
| A8 FileAiPage | ✅ | `pages/file_ai_page.h/cpp` (indice 18) |
| B1 Chat estensioni | ⬜ | 3h: storia+TTFT+export+vision |
| A4 MultiAgent | ⬜ | 5h — dopo A3 |
| A6 ChartWidget | ⬜ | 3h — QPainter custom |
| B4 Matematica 52 formule | ⬜ | 2h |
| B2 LavoroPage estensioni | ⬜ | 3h: tracker+cover+foto |
| B5 Ricerca+Astrale+GPS | ⬜ | 3h |
| B7 Simulatore Algoritmi | ⬜ | 3h |
| C1 Drawer animazione | ⬜ | 1h — QPropertyAnimation |
| B6 SecurityPage | ⬜ | 4h |
| B8 Voice Loop | ⬜ | 2h — dopo C3 |
| B9 WAN client | ⬜ | 2h |
| C8 Test nuove pagine | ⬜ | 3h |

---

## INDICI STACK (aggiornare ad ogni nuova pagina)

| # | Pagina |
|---|--------|
| 0 | ChatPage |
| 1 | StudioPage |
| 2 | LavoroPage |
| 3 | ObsPage |
| 4 | MisurePage |
| 5 | CameraPage |
| 6 | McpAddonsPage |
| 7 | BlePage |
| 8 | AudioPage |
| 9 | SettingsPage |
| 10 | InfoPage |
| 11 | ImparaPage |
| 12 | RicercaPage |
| 13 | MatematicaPage |
| 14 | SintetizzatorePage |
| 15 | AssistentePage ✅ |
| 16 | OraclePage ✅ |
| 17 | HermesPage ✅ |
| 18 | FileAiPage ✅ |
| 19 | FinanzaPage ✅ |
| 20 | MultiAgentPage *(da aggiungere A4)* |
| 21 | ChartPage *(da aggiungere A6)* |
| 22 | SecurityPage *(da aggiungere B6)* |
| 23 | SimulatorePage *(da aggiungere B7)* |

---

## REGOLE PROGETTO (non dimenticare)

- **No lambda senza context object** nel 4° arg di `connect()` — regola ferrea
- **No `dpiScale()`** su Android — usa `QScreen::devicePixelRatio()` o valori fissi
- **Nomi file**: `nome_page.h/cpp` (non `nome_page_widget.h`) per pagine Android
- **Emoji in C++**: `"\xe2\x80\x9c" "Testo"` non `"\xe2\x80\x9cTesto"` (C è cifra hex)
- **ThemeManager mobile**: `MobileThemeManager::instance()` — mai `new`

---

## COSA HA ANDROID CHE IL DESKTOP NON HA

*Ricorda questi punti di forza — non toglierli!*

- 📱 **LLM on-device** — llama.cpp embedded, inferenza offline
- 🔋 **BLE chat AES-256** — comunicazione peer-to-peer cifrata
- 📷 **Camera OCR** — foto → testo → chat AI
- 📐 **Planimetria touch** — Shoelace formula con touch multipoint
- 🌡️ **ThermalMonitor** — già implementato (`thermal_monitor.cpp`)
- 🤖 **Emoji modello** — 📱 locale / 📶 LAN / ☁️ cloud nel model picker
