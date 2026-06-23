# Prismalux Mobile — Note Android

## Cartella `android/`

Contiene la configurazione nativa Android dell'app Qt6:

| File/Cartella | Scopo |
|---|---|
| `AndroidManifest.xml` | Permessi, nome app, icona, target SDK |
| `res/` | Icone launcher (hdpi/mdpi/xhdpi/xxhdpi/xxxhdpi), stili tema |
| `res/xml/network_security_config.xml` | Policy HTTP/HTTPS per la comunicazione LAN |
| `res/values/styles.xml` | Tema scuro nativo Android (evita flash bianco all'avvio) |

---

## Strumento di Analisi: claude.ai/design

**URL**: https://claude.ai/design

Strumento web di Anthropic accessibile dal browser, pensato per analisi visiva e design.

### Cosa può fare (da verificare nella versione corrente)

- Analisi di **immagini** catturate con il telefono (screenshot, foto UI, foto dell'hardware)
- Analisi di **video registrati** dal telefono — da aprire direttamente nel browser del PC o caricare il file
- Feedback su layout, UX, colori, accessibilità di interfacce mobile
- Revisione visiva di schermate dell'app Prismalux Mobile

### Come usarlo per Prismalux Mobile

1. Apri https://claude.ai/design nel browser
2. Carica il video o la foto registrata dal telefono (via USB o cloud)
3. Chiedi l'analisi: layout pagine, leggibilità testo, touch target, tema scuro
4. Porta i risultati nel codice Qt6 (`pages/*.cpp`, `style_mobile.qss`, `mobile_theme_manager.h`)

### Casi d'uso pratici per questo progetto

| Cosa registri | Cosa analizzi |
|---|---|
| Schermata hamburger menu ☰ | Leggibilità voci, dimensioni touch target |
| Navigazione tra pagine | Fluidità animazioni, orientamento |
| Quiz CCNA in fullscreen | Layout header nascosta, feedback risposta |
| Chat AI + streaming token | Scroll automatico, performance |
| BLE scan e connessione | UX pairing, messaggi di errore |
| Pagina Impostazioni | Overflow testo, scroll verticale |

---

## Permessi Android dichiarati

Vedi `AndroidManifest.xml` per la lista completa. Riepilogo:

| Permesso | Usato da |
|---|---|
| `INTERNET` | AiClient (Ollama LAN + cloud) |
| `CAMERA` | CameraPage (OCR visuale) |
| `BLUETOOTH_*` | BlePage (chat BLE RFCOMM) |
| `ACCESS_FINE_LOCATION` | BlePage (richiesto da Android per BLE scan) |
| `READ_EXTERNAL_STORAGE` | RagEngineSimple, AudioPage |
| `RECORD_AUDIO` | ⚠️ **DA AGGIUNGERE** — AudioPage usa QMediaRecorder |

> **TODO critico**: aggiungere `android.permission.RECORD_AUDIO` al manifest e il runtime permission request in `audio_page.cpp` prima di chiamare `m_recorder->record()`.

---

## Build APK

```bash
# Dalla cartella android_app/
cmake -B build-android \
  -DCMAKE_TOOLCHAIN_FILE=$ANDROID_NDK/build/cmake/android.toolchain.cmake \
  -DANDROID_ABI=arm64-v8a \
  -DANDROID_PLATFORM=android-26 \
  -DQT_ANDROID_BUILD_ALL_ABIS=OFF
cmake --build build-android
```

APK finale: `ANDROID/PrismaluxMobile.apk`

Script di test: `ANDROID/test_apk.sh` / `ANDROID/test_utente.sh`
Installazione Xiaomi: `ANDROID/installa_xiaomi.sh`
