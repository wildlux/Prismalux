# PrismaluxMobile — Client Qt6/Android per Ollama su LAN

App Android nativa in C++/Qt6. Si connette a Ollama sul PC di casa via WiFi,
esegue query RAG leggero locale e risponde con i modelli LLM più piccoli.

---

## Struttura del progetto

```
android_app/
├── CMakeLists.txt                  ← build Qt6, target Android
├── main.cpp
├── mainwindow.h / .cpp             ← finestra con bottom nav bar
├── ai_client.h / .cpp              ← HTTP client Ollama streaming
├── rag_engine_simple.h / .cpp      ← RAG keyword-based locale
├── style_mobile.qss                ← tema dark ottimizzato touch
│
├── pages/
│   ├── chat_page.h / .cpp          ← chat streaming + RAG
│   ├── camera_page.h / .cpp        ← scansione + vision model
│   ├── ble_page.h / .cpp           ← scanner Bluetooth LE
│   └── settings_page.h / .cpp      ← server, modello, RAG
│
└── android/
    ├── AndroidManifest.xml
    └── res/xml/
        └── network_security_config.xml
```

---

## Requisiti

### Sul PC (server)
```bash
# Installa Ollama
curl -fsSL https://ollama.com/install.sh | sh

# Scarica un modello leggero (consigliati per mobile)
ollama pull llama3.2:1b        # 1.3 GB — velocissimo
ollama pull qwen3:1.7b         # 1.4 GB — ottimo italiano
ollama pull gemma3:1b          # 815 MB — leggerissimo

# Avvia Ollama accessibile sulla LAN (non solo localhost)
OLLAMA_HOST=0.0.0.0 ollama serve
```

> **Nota**: `OLLAMA_HOST=0.0.0.0` è necessario per rendere Ollama
> raggiungibile dagli altri dispositivi sulla rete locale.

### Sul PC (sviluppo Android)

| Dipendenza | Versione | Note |
|---|---|---|
| Qt6 | ≥ 6.5 | Core, Widgets, Network, Bluetooth, Multimedia |
| Android NDK | r25c+ | tramite Android Studio o SDK Manager |
| Android SDK | API 26-34 | minSdk 26 (Android 8.0) |
| CMake | ≥ 3.21 | bundled con Android Studio va bene |
| Qt for Android | ≥ 6.5 | installato via Qt Maintenance Tool |

---

## Compilazione

### Da Qt Creator (consigliato)

1. Apri Qt Creator → File → Apri progetto → seleziona `CMakeLists.txt`
2. Configura un kit Android (Qt 6.x → Android ARM64)
3. Collega il telefono via USB con debug USB abilitato
4. Premi ▶ Run

### Da riga di comando

```bash
# Imposta le variabili d'ambiente Android
export ANDROID_SDK_ROOT=~/Android/Sdk
export ANDROID_NDK_ROOT=~/Android/Sdk/ndk/25.2.9519653

# Configura
cmake -B build-android \
  -DCMAKE_TOOLCHAIN_FILE=$ANDROID_NDK_ROOT/build/cmake/android.toolchain.cmake \
  -DANDROID_ABI=arm64-v8a \
  -DANDROID_PLATFORM=android-26 \
  -DQt6_DIR=~/Qt/6.7.0/android_arm64_v8a/lib/cmake/Qt6 \
  -DCMAKE_BUILD_TYPE=Release

# Compila
cmake --build build-android -j$(nproc)

# Installa sul telefono (richiede adb)
adb install build-android/android-build/PrismaluxMobile.apk
```

---

## Configurazione iniziale nell'app

1. Apri l'app → scheda **⚙ Impost.**
2. Inserisci l'IP del PC (es. `192.168.1.100`) e porta `11434`
3. Premi **🔌 Testa** — deve comparire ✅
4. Premi **🔄 Dal server** per caricare la lista modelli
5. Seleziona un modello (es. `llama3.2:1b`) → **Applica**
6. Opzionale: inserisci il percorso di una cartella con file `.txt` per il RAG

---

## Funzionalità per schermata

### 🤖 Chat
- Streaming token da Ollama in tempo reale
- RAG locale (toggle 📚): inietta i chunk più rilevanti nel system prompt
- History conversazione (ultimi 6 turni)
- Stop immediato della generazione
- Contesto da Camera: tap "Invia a Chat" dopo scansione

### 📷 Camera
- Preview live + cattura immagine
- Analisi via modello vision (es. `llama3.2-vision`)
- Estrazione testo OCR dalla risposta AI
- "Invia a Chat" → il testo diventa contesto del prossimo messaggio

### 🔋 Bluetooth
- Scan BLE 8 secondi con aggiornamento live
- Badge segnale: 🟢 vicino / 🟡 medio / 🔴 lontano
- Tap dispositivo → dettagli + copia MAC negli appunti

### ⚙ Impostazioni
- Host + porta Ollama con test connessione
- Lista modelli dal server con refresh
- Temperatura (0.00–1.00) e Max Token
- Carica documenti RAG da cartella locale

---

## Modelli consigliati per Android

| Modello | Dimensione | Velocità (Snapdragon 8) | Note |
|---|---|---|---|
| `gemma3:1b` | 815 MB | ~25 tok/s | leggerissimo |
| `llama3.2:1b` | 1.3 GB | ~18 tok/s | ottimo inglese |
| `qwen3:1.7b` | 1.4 GB | ~15 tok/s | ottimo italiano |
| `deepseek-r1:1.5b` | 1.1 GB | ~14 tok/s | ragionamento |
| `phi3.5:mini` | 2.2 GB | ~10 tok/s | bilanciato |

> Questi modelli girano sul PC via Ollama, non sul telefono.
> Il telefono fa solo da client HTTP (streaming).
> Su LAN WiFi ac/ax la latenza è < 10ms.

---

## RAG locale — come funziona

Il `RagEngineSimple` usa keyword scoring (TF semplificato) invece di
embedding vettoriali:

1. **Caricamento**: divide ogni `.txt` in chunk da 400 caratteri (overlap 80)
2. **Query**: tokenizza la domanda, calcola il punteggio per ogni chunk
3. **Iniezione**: i top-4 chunk vengono aggiunti al system prompt prima della richiesta

**Pro**: latenza zero, nessuna dipendenza da embedding server  
**Contro**: non capisce sinonimi semantici

Per RAG semantico completo, usa il `RagEngine` di Prismalux desktop
che usa embedding + JLT.

---

## Permessi Android richiesti

| Permesso | Motivo |
|---|---|
| `INTERNET` | Connessione HTTP a Ollama su LAN |
| `CAMERA` | Scansione documenti |
| `BLUETOOTH_SCAN` | Discovery dispositivi BLE |
| `BLUETOOTH_CONNECT` | Info dispositivi BLE |
| `READ_MEDIA_DOCUMENTS` | Caricamento file RAG |

---

## Sicurezza

- L'app comunica **solo con indirizzi LAN privati** (network_security_config.xml)
- Nessun dato viene inviato a server esterni
- Il traffico HTTP è consentito solo verso 192.168.x.x / 10.x.x.x / localhost
