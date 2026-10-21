# Prismalux — Note di rilascio

**Data:** 21 ottobre 2026  
**Stato:** bozza per il prossimo rilascio  
**Base:** v3.0 + aggiornamenti post-release

---

## In breve

Questo rilascio porta Prismalux più vicino a un'installazione "chiavi in mano":

- **Docker** per eseguire la GUI e per compilare l'AppImage in modo riproducibile.
- **Decision Model** con quattro backend: Ollama, DwarfStar, ONNX e llama.cpp.
- **Temi** desktop/web allineati e cambio tema più rapido.
- **DwarfStar (ds4) di Salvatore San Filippo / antirez** integrato come backend di inferenza.
- **Ollama, llama.cpp e ONNX** ricontrollati e fissati nei punti critici.

---

## Docker

### GUI in container

Il runtime Docker usa Ubuntu 24.04 con Qt 6.4 e include:

- avvio della GUI con display X11 o Wayland;
- accesso a Ollama sul host tramite `network_mode: host`;
- volume persistente `prismalux_data` per impostazioni, cronologia, RAG e knowledge;
- utente non-root `prismalux`;
- temi e database Quiz CCNA già inclusi nell'immagine.

Comandi:

```bash
# Build + avvio
docker compose -f Tools/docker/docker-compose.yml build
bash Tools/docker/avvia_docker.sh

# Solo build
docker build -t prismalux:latest -f Tools/docker/Dockerfile .

# Debug nel container
bash Tools/docker/avvia_docker.sh --shell
```

L'immagine runtime è stata compilata e testata in modalità offscreen. Dimensione indicativa: **1,3 GB**.

### AppImage builder

È disponibile un secondo Dockerfile, separato dal runtime, che riproduce l'ambiente della CI:

```bash
bash Tools/docker/build_appimage_docker.sh
```

Output:

- `EXPORT/linux/Prismalux-x86_64.AppImage`
- `EXPORT/linux/Prismalux-x86_64.AppImage.sha256`

Il builder usa **Ubuntu 24.04 + Qt 6.4**, quindi produce un'AppImage più portabile rispetto a una build fatta sulla macchina di sviluppo. L'artefatto include Qt WebEngine e pesa circa **135 MB**.

> Nota tecnica: le librerie NSS/NSPR non vengono incluse nell'AppImage, perché Qt WebEngine deve caricare i moduli NSS del sistema e versioni miste causano errori tipo `NSSUTIL_3.108 not found`.

---

## Decision Model

Il Decision Model è ora configurabile da **Impostazioni → LLM & DM → Decision Model** e supporta quattro backend:

| Backend | Stato | Note |
|---|---|---|
| Ollama | ✅ | Test live con `tev1:4b` e `qwen2.5-coder:1.5b` |
| DwarfStar (ds4) | ✅ implementato | Richiede modelli grandi e hardware specifico |
| ONNX | ✅ | Classificatore + verificatore locali, con guardie deterministiche |
| llama.cpp / llama-server | ✅ | Supporto `--tensor-split` e split CPU/GPU |

Funzionalità incluse:

- classificazione dell'intento e verifica della risposta;
- badge nella toolbar della chat con intento, confidenza, cache ed esito della verifica;
- retry automatico quando la verifica boccia il primo tentativo;
- modello di verifica diverso per **coding / reasoning / chat / other**;
- cache SQLite e fail-open se il backend non è disponibile;
- nessun fallback silenzioso su Ollama: gli errori sono espliciti.

### Qualità ONNX

- Verificatore: **90,9%** sul dev set e **88,6%** sul blind set senza guardie; con le guardie deterministiche **100%** su entrambi i set.
- Il dato 100% è il comportamento ibrido modello + guardie, non l'accuratezza del solo modello appreso.
- Classificatore: **77,1%**; resta il punto da migliorare con un dataset etichettato più ampio.
- Il verificatore è **relevance/refusal-only**, non un fact-checker.

---

## DwarfStar (ds4) di Salvatore San Filippo

È stato aggiunto il backend **DwarfStar**, il motore di inferenza di Salvatore San Filippo (antirez), noto anche come **ds4**.

Caratteristiche dell'integrazione:

- API OpenAI-compatible: `POST /v1/chat/completions` e `GET /v1/models`;
- riuso del percorso HTTP già usato per llama-server;
- download dei modelli tramite `./download_model.sh`;
- supporto a `think:false`;
- warning in Impostazioni: DwarfStar è pensato per modelli molto grandi (DeepSeek V4 Flash/PRO, GLM 5.2/5.3, Qwen3.8 Flash Next) e non per un modello leggero 4B.

> DwarfStar è implementato e compilato, ma la verifica con hardware reale (Metal / CUDA / ROCm) resta da fare.

---

## Temi

- Parità **26/26** tra temi Qt (`gui/themes/*.qss`) e webchat LAN.
- Aggiunti `dark`, `dracula` e `nord` lato Qt.
- Griglia temi compatta a 6 colonne, con tutte le card visibili in 5 righe.
- Anteprime lette dal QSS reale, non più da un fallback grigio.
- Cambio tema più rapido:
  - `apply()` non riapplica lo stesso tema;
  - i top-level vengono congelati durante `setStyleSheet()`;
  - `recolorLog()` viene differito e limitato ai passaggi dark ↔ light.
- Redis valutato e **non adottato**: il collo di bottiglia è il repolish Qt, non il lookup o l'I/O del QSS.

---

## Ollama, llama.cpp e ONNX

- **Ollama** — ricontrollato il percorso Decision Model con modelli reali; diagnosticato e gestito il caso `403` da header `Origin` non consentito.
- **llama.cpp** — verificati avvio, selezione dispositivi e split CPU/GPU; il piano `--tensor-split` viene salvato e riapplicato. La verifica multi-GPU resta limitata dall'hardware disponibile.
- **ONNX** — parità feature C++/Python, inferenza A/B/C/D e N/Y, cache, fail-open e guardie deterministiche coperte da test.

---

## AppImage

- Build CI su Ubuntu 24.04 / Qt 6.4: workflow **#307 verde**.
- Fix Qt 6.4: aggiunto `#include <QTimeZone>` in `gui/lan_server.cpp`.
- AppImage generata anche localmente e tramite Docker builder.
- Checksum SHA-256 prodotto accanto all'AppImage.
- Avvio offscreen verificato senza librerie Qt mancanti; resta un warning non bloccante su un modulo gvfs del host.

---

## Test e verifiche

| Area | Esito |
|---|---|
| Docker runtime | ✅ build e avvio offscreen |
| Docker AppImage builder | ✅ build e generazione AppImage |
| AppImage offscreen | ✅ avvio, nessuna libreria Qt mancante; resta un warning non bloccante su un modulo gvfs del host |
| Decision Model ONNX | ✅ test e benchmark |
| Decision pipeline | ✅ 10/10 |
| Decision badge | ✅ 10/10 |
| Temi Qt ↔ web | ✅ 26/26 |
| Ollama live | ✅ pipeline L-1 verde |
| DwarfStar hardware reale | ⬜ da verificare |
| GUI su display fisico | 🟡 verifica finale da fare |

---

## Limitazioni note

| Voce | Stato | Dettaglio |
|---|---|---|
| Verifica temi su display fisico | 🟡 | Solo screenshot/offscreen finora |
| Decision badge L-2 live | 🟡 | Dipende dai tempi CPU del modello |
| DwarfStar con hardware reale | ⬜ | Implementato, non testato su GPU/Metal |
| Multi-GPU llama.cpp | ⬜ | Una sola GPU disponibile; backend CPU-only |
| Runtime Docker su display reale | 🟡 | Testato offscreen; fare un avvio X11/Wayland |

---

## Comandi rapidi

```bash
# AppImage locale
chmod +x EXPORT/linux/Prismalux-x86_64.AppImage
./EXPORT/linux/Prismalux-x86_64.AppImage

# Senza FUSE 2
APPIMAGE_EXTRACT_AND_RUN=1 ./EXPORT/linux/Prismalux-x86_64.AppImage

# Docker GUI
bash Tools/docker/avvia_docker.sh

# Docker AppImage builder
bash Tools/docker/build_appimage_docker.sh
```

---

*Prismalux è software open-source. Se lo trovi utile, considera una donazione via PayPal (badge nel README).*
