# Prismalux Vision3D — modulo Qt6 (C++)

Scheda `QWidget` con **server HTTPS integrato** (`QSslServer`) che riceve foto
dal telefono/tablet e le analizza sul PC:
- **Descrizione VLM** → Ollama (`/api/generate`, immagine base64) via `QNetworkAccessManager`
- **Box oggetti** → OpenCV nativo C++ (opzionale; se OpenCV manca, si disattiva)
- **Depth map** → script Python (`depth_infer.py`, MiDaS) via `QProcess`

La stessa pagina HTML dei prototipi Python è servita direttamente dal C++.

## File
| File | Ruolo |
|------|-------|
| `PrismaluxVision3DWidget.h/.cpp` | La scheda + server + pipeline |
| `depth_infer.py` | Worker depth (stdin JPEG → stdout JPEG colormap) |
| `CMakeLists.txt` | Build come libreria statica (+ demo opzionale) |
| `main_demo.cpp` | App di test standalone |

## Integrazione in Prismalux
1. Copia i 4 file nel progetto (o aggiungi la sottocartella con `add_subdirectory`).
2. Nel tuo `CMakeLists.txt` principale:
   ```cmake
   add_subdirectory(prismalux_vision3d_qt)
   target_link_libraries(prismalux PRIVATE prismalux_vision3d)
   ```
3. Nel codice, dove costruisci l'interfaccia:
   ```cpp
   #include "PrismaluxVision3DWidget.h"

   auto* scheda = new PrismaluxVision3DWidget(this);
   scheda->setVlmModel("qwen2.5-vl:3b");     // o "moondream"
   scheda->setDepthScript("depth_infer.py"); // path assoluto consigliato
   tabWidget->addTab(scheda, "Vision3D");     // aggiungila come tab/scheda

   // avvia quando vuoi (o lega a un pulsante tuo):
   scheda->start(8443);                       // HTTPS self-signed auto

   // reagisci ai risultati:
   connect(scheda, &PrismaluxVision3DWidget::analysisReady,
           this, [](const Vision3DResult& r){
       // r.description, r.boxesJpeg, r.depthJpeg, r.savedPath ...
   });
   ```

## Multi-device (più telefoni, un progetto)
Sì: più telefoni possono scansionare lo stesso progetto insieme.
- Ogni telefono che apre l'URL riceve un **ID automatico** (`tel01`, `tel02`, …)
  salvato come **cookie persistente**: riconnettendosi mantiene lo stesso ID.
- Basta usare lo **stesso nome sessione** su tutti i telefoni.
- Le foto vanno in `scan_output/<sessione>/<deviceId>/` e l'ID è anche nel
  nome file: `scan1_tel02_003_a000.jpg`.
- La numerazione è protetta da `QMutex`: due telefoni che scattano insieme non
  si sovrascrivono.
- La scheda Qt mostra un pannello **"Device attivi"** con ID, numero foto,
  ultimo scatto e tipo di dispositivo.

Per la fotogrammetria è comodo: due persone girano intorno all'oggetto da lati
opposti e raddoppiano la copertura nello stesso progetto.

## Sensori di movimento (angolo + inclinazione per foto)
Ogni telefono, dopo aver premuto **🧭 Sensori**, legge giroscopio/bussola e
allega a ogni scatto: `heading_deg` (bussola 0–359), `pitch_deg` (avanti/indietro),
`roll_deg` (laterale). Questi finiscono in un **sidecar `.json`** accanto a ogni
foto (stesso nome, estensione `.json`) e nel log della scheda.

A cosa serve davvero: **non** sostituisce il calcolo della posa delle camere
(quello lo fa la fotogrammetria dalle immagini, molto meglio). Serve a:
- ordinare/raggruppare gli scatti per angolo quando più telefoni lavorano insieme;
- capire subito se hai coperto tutti i 360° o se restano buchi;
- dare un hint iniziale ad alcuni tool.
Nota onesta: la bussola magnetica è rumorosa e va tarata; consideralo un
riferimento indicativo, non una misura di precisione.

## Dare la SCALA reale al modello — ArUco (automatico)
La fotogrammetria ricostruisce la **forma** ma non la **dimensione**: il modello
esce senza unità. Con un marker di dimensione nota nella scena, il modulo calcola
la scala **da solo**.

### Cosa stampare (già pronto in questa cartella)
- `charuco_board_A4.pdf` — board CharUco: mettila **sotto** l'oggetto. Ne basta
  una, resta sempre parzialmente visibile mentre giri.
- `aruco_markers_A4.pdf` — 6 marker singoli da **4 cm**: ritagliali (lascia il
  bordo bianco!) e disponili **a cerchio** intorno all'oggetto.
Ne basta anche solo uno dei due; entrambi insieme danno il risultato migliore.

**Perché non la rosa dei venti:** un pattern simmetrico è ambiguo per rotazione,
il software non capisce "dov'è l'alto". I marker ArUco sono asimmetrici e hanno
un **ID univoco** con orientamento non ambiguo — il senso di direzione che
volevi, ma leggibile dalla macchina.

### Regole di stampa (importanti)
- Stampa **al 100% / "dimensione reale"**, MAI "adatta alla pagina".
- Su ogni foglio c'è una **linea di verifica da 100 mm**: misurala col righello
  dopo la stampa. Se non è esatta, la scala sarà sbagliata in proporzione.
- Marker ~4 cm = buoni per oggetti da tavolo. Regola: il lato del marker dovrebbe
  occupare almeno il ~10% dell'inquadratura per essere letto bene.

### Come funziona nel modulo
Ad ogni foto il server rileva i marker (`cv::aruco`, DICT_4X4_50) e calcola
`scale_mm_per_unit` = quanti **mm reali** corrisponde 1 px alla distanza del
marker (mediana su tutti i marker visti, robusta agli outlier). Finisce nel
sidecar `.json`, nel log e nella risposta al telefono (che mostra "scala OK").
```cpp
scheda->setArucoMarkerMm(40.0);   // dì al modulo quanto è grande il tuo marker
```
Se un marker di lato noto è visibile in almeno una foto, hai la scala per l'intero
modello: in Meshroom/CloudCompare imposti quel fattore e il modello è in mm reali.

### Rigenerare i bersagli (marker diversi / altre misure)
```bash
pip install --break-system-packages opencv-contrib-python numpy reportlab
python3 make_aruco_targets.py --marker-mm 40 --dict 4X4_50
```

### Requisito di build per la scala automatica
Serve **opencv-contrib** (modulo `aruco`). Il CMake lo rileva e, se presente,
definisce `VISION3D_USE_ARUCO`. Se manca, tutto il resto funziona e la scala
va fatta a mano (misuri un oggetto noto sul modello finale). Fallback pulito.

## Certificato HTTPS
Hai detto che l'HTTPS lo fai già: passa i tuoi file a `start(port, cert, key)`.
Se li ometti, il widget genera un self-signed con `openssl` in `outputDir`.

## Setup runtime sul PC
```bash
# VLM
ollama pull qwen2.5-vl:3b   # oppure moondream (~1.7GB, leggero per i 6GB RAM)
ollama serve

# depth (solo se usi la chip Depth)
pip install --break-system-packages torch torchvision timm opencv-python-headless numpy
```

## Note di design (perché così)
- **Niente torch linkato in C++**: la depth gira in un processo Python separato
  (`QProcess`), così il widget resta leggero e il modello si può cambiare senza
  ricompilare. Comunica via stdin/stdout in JPEG binario.
- **VLM via HTTP**, non libreria: usi Ollama che hai già, zero dipendenze extra.
- **OpenCV opzionale** via `#ifdef VISION3D_USE_OPENCV` (settato da CMake se trovato):
  se non c'è, i box si disattivano ma tutto il resto funziona.
- **Server HTTP minimale** scritto a mano sopra `QSslSocket` (gestisce Content-Length
  e body parziali): nessuna dipendenza web esterna.

## Limite hardware (invariato)
LiDAR / IR / ToF del telefono **non sono accessibili da browser** — solo app
native ARKit/ARCore. Qui la depth è **stimata via software** (MiDaS), che per
riconoscimento oggetti + VLM è la strada giusta. Se un domani fai un'app nativa
che manda la depth vera, l'endpoint `/upload` è già pronto a riceverla come
campo aggiuntivo.

## Attenzione RAM (6 GB)
`qwen2.5-vl:3b` + MiDaS insieme possono essere stretti. Opzioni:
- usa `moondream` per il VLM, e `MiDaS_small` per la depth;
- oppure deseleziona la chip Depth nell'app quando non serve.
