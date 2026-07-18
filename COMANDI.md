# 🍺 COMANDI.md — Prismalux

Raccolta di 253 comandi utili usati nel progetto, con descrizione.
Tutti i comandi si intendono lanciati dalla root del progetto (`/home/wildlux/Desktop/Prismalux`) salvo dove indicato.

> ⚠️ **Regola termica**: su questa macchina la CPU arriva a 90°C — mai `ctest` completo in parallelo.
> Preferire sempre `nice -n19`, suite mirate `-R`, `-j1`, oppure `./Test/ctest_lotti.sh`.
>
> ⚠️ **Porte**: mai hardcodare — nel codice usare sempre `P::*` da `prismalux_paths.h`
> (Ollama 11434 · llama-server 8081 · OpenCode 8092 · LAN server 11500 · WAN Compute 11600).

---

## 🔨 Build & Avvio

**1.** Configura la build GUI (prima volta, o dopo nuovi file / modifiche a CMakeLists):
```bash
cmake -B build_gui gui/ -DCMAKE_BUILD_TYPE=Release
```

**2.** Build incrementale (solo `.cpp`/`.h` modificati):
```bash
cmake --build build_gui -j$(nproc)
```

**3.** Build "gentile" che non scalda la CPU:
```bash
nice -n19 cmake --build build_gui -j4
```

**4.** Build completa + aggiornamento dei 2 file `.desktop` + icona (percorso standard consigliato):
```bash
./aggiorna.sh
```

**5.** Build + esecuzione suite di test:
```bash
./aggiorna.sh --test
```

**6.** Build multipiattaforma (Linux/Windows/macOS, motore unico):
```bash
python3 build.py
```

**7.** Avvio dell'app compilata:
```bash
./build_gui/Prismalux_GUI
```

**8.** Avvio dall'AppImage:
```bash
./EXPORT/linux/Prismalux-x86_64.AppImage
```

**9.** Pulizia completa della build dir (poi rifare la configurazione):
```bash
rm -rf build_gui && cmake -B build_gui gui/ -DCMAKE_BUILD_TYPE=Release
```

**10.** Log dell'ultima build (`build.py` scrive `errore.txt` in root se fallisce):
```bash
cat errore.txt 2>/dev/null; cat ~/.prismalux/last_build.log
```

---

## 🧪 Test

**11.** Configura la build dei test (percorso canonico `gui/build_tests`):
```bash
cmake -B gui/build_tests gui/ -DBUILD_TESTS=ON -DCMAKE_BUILD_TYPE=Release
```

**12.** Compila un solo target di test:
```bash
nice -n19 cmake --build gui/build_tests --target test_agenti_pipeline -j4
```

**13.** Suite mirata (regola termica: mai il ctest completo `-j4`):
```bash
nice -n19 ctest --test-dir gui/build_tests -R "AgentiPipeline" -j1 --output-on-failure
```

**14.** Più suite insieme via regex:
```bash
nice -n19 ctest --test-dir gui/build_tests -R "GraphMemory|RagEngine" -j1 --output-on-failure
```

**15.** Escludere i test che richiedono Ollama reale:
```bash
nice -n19 ctest --test-dir gui/build_tests -E "AiIntegration|AiStress|TeamCollab|MultiAgenteLive|AgenteAutonomoLive" -j1
```

**16.** Test a lotti con pausa termica automatica:
```bash
./Test/ctest_lotti.sh
```

**17.** Lotti escludendo alcune suite (variabile `ESCLUDI` = regex `-E`):
```bash
ESCLUDI="Vision3D|LanServer" ./Test/ctest_lotti.sh
```

**18.** Elenco dei test disponibili senza eseguirli:
```bash
ctest --test-dir gui/build_tests -N
```

**19.** Eseguire un singolo metodo di una suite QTest:
```bash
./gui/build_tests/test_agenti_pipeline nomeFunzioneDiTest
```

**20.** Override del modello nei test live (lenti, Ollama reale):
```bash
PRISMALUX_TEST_MODEL="qwen3:8b" ./gui/build_tests/test_chat_live
```

---

## 🌿 Git & GitHub

**21.** Stato sintetico del working tree:
```bash
git status --short
```

**22.** Ultimi 10 commit in una riga:
```bash
git log --oneline -10
```

**23.** Riepilogo delle modifiche non committate:
```bash
git diff --stat
```

**24.** Commit atomico (convenzione: `tipo(scope): messaggio` in italiano):
```bash
git add gui/pages/main_ai.h && git commit -m "fix(ai): descrizione breve"
```

**25.** Push su GitHub:
```bash
git push origin master
```

**26.** Mettere da parte modifiche non pronte (e riprenderle):
```bash
git stash push -m "wip descrizione"    # git stash pop per riprendere
```

**27.** Annullare le modifiche non committate a un file:
```bash
git restore gui/pages/main_ai_pipeline.cpp
```

**28.** ⚠️ Riallineare TUTTO al remoto (scarta le modifiche locali!):
```bash
git fetch origin && git reset --hard origin/master
```

**29.** Creare e pushare un tag di release:
```bash
git tag -a v3.1 -m "Release v3.1" && git push origin v3.1
```

**30.** Aprire il repository nel browser:
```bash
gh repo view --web
```

**31.** Creare una release GitHub con allegato:
```bash
gh release create v3.1 EXPORT/linux/Prismalux-x86_64.AppImage --title "Prismalux v3.1"
```

**32.** Storia di un singolo file (anche attraverso i rename):
```bash
git log --oneline --follow -- gui/pages/main_ai.h
```

---

## 🌐 Traduzioni (i18n)

**33.** Aggiornare il catalogo EN (lupdate Qt6 **mirato** ai sorgenti C++ — mai `-recursive`, pescherebbe i `.js`):
```bash
lupdate gui/*.cpp gui/*.h gui/pages/*.cpp gui/pages/*.h gui/widgets/*.h -no-obsolete -ts gui/i18n/prismalux_en.ts
```

**34.** Rimuovere i tag `<location>` dal catalogo (convenzione del progetto):
```bash
lconvert -locations none -i gui/i18n/prismalux_en.ts -o gui/i18n/prismalux_en.ts
```

**35.** Contare le stringhe non ancora tradotte:
```bash
grep -c 'type="unfinished"' gui/i18n/prismalux_en.ts
```

**36.** Validare/compilare il catalogo in `.qm` (lo fa anche la build cmake):
```bash
lrelease gui/i18n/prismalux_en.ts
```

**37.** Totale stringhe presenti nel catalogo:
```bash
grep -c '<message' gui/i18n/prismalux_en.ts
```

---

## 🤖 Ollama & LLM

**38.** Modelli installati:
```bash
ollama list
```

**39.** Scaricare un modello:
```bash
ollama pull mistral:7b-instruct
```

**40.** Modelli attualmente caricati in RAM/VRAM:
```bash
ollama ps
```

**41.** Dettagli di un modello (parametri, layer, quantizzazione):
```bash
ollama show mistral:7b-instruct
```

**42.** Rimuovere un modello:
```bash
ollama rm nomemodello
```

**43.** Scaricare subito un modello dalla RAM (keep_alive=0, come fa `unloadModel()`):
```bash
curl -s http://localhost:11434/api/generate -d '{"model":"mistral:7b-instruct","keep_alive":0}'
```

**44.** Test rapido della chat API:
```bash
curl -s http://localhost:11434/api/chat -d '{"model":"mistral:7b-instruct","messages":[{"role":"user","content":"ciao"}],"stream":false}' | python3 -m json.tool
```

**45.** Test embedding (usato dal RAG):
```bash
curl -s http://localhost:11434/api/embeddings -d '{"model":"nomic-embed-text","prompt":"testo di prova"}' | head -c 300
```

**46.** Elenco modelli via API (quello che usa `fetchModels()`):
```bash
curl -s http://localhost:11434/api/tags | python3 -m json.tool | head -30
```

**47.** Stato / riavvio del servizio Ollama:
```bash
systemctl status ollama    # sudo systemctl restart ollama
```

**48.** Health check di llama-server (backend alternativo, porta 8081):
```bash
curl -s http://localhost:8081/health
```

---

## 📡 Rete / LAN / TLS

**49.** Porte Prismalux in ascolto:
```bash
ss -tlnp | grep -E "11434|11500|11600|8081|8092"
```

**50.** Chi sta occupando una porta ("Address already in use"):
```bash
lsof -i :11500
```

**51.** IP LAN del PC (da dare al telefono per web chat / Vision3D):
```bash
ip -4 addr show | grep 192.168
```

**52.** Test del LAN server (TLS, certificato self-signed → `-k`):
```bash
curl -sk https://127.0.0.1:11500/api/tags
```

**53.** Verifica del redirect 301 http→https sulla porta TLS:
```bash
curl -si http://127.0.0.1:11500/ | head -5
```

**54.** Scadenza e validità del certificato TLS del server:
```bash
openssl s_client -connect 127.0.0.1:11500 </dev/null 2>/dev/null | openssl x509 -noout -dates
```

**55.** Leggere il token LAN corrente (per URL web chat completo):
```bash
cat ~/.prismalux/lan_token.key
```

**56.** Ping alla camera WIBY (JS-P161):
```bash
ping -c3 192.168.1.222
```

**57.** Scansione dei dispositivi sulla rete locale:
```bash
nmap -sn 192.168.1.0/24
```

---

## 🔌 MCP & Python

**58.** Handshake + elenco tool di un plugin MCP (JSON-RPC 2.0 su stdio):
```bash
printf '%s\n' \
  '{"jsonrpc":"2.0","id":1,"method":"initialize","params":{"protocolVersion":"2024-11-05","capabilities":{},"clientInfo":{"name":"cli","version":"1"}}}' \
  '{"jsonrpc":"2.0","id":2,"method":"tools/list"}' \
  | python3 MCPs/knowledge_mcp/server.py
```

**59.** Chiamare un tool MCP da riga di comando:
```bash
printf '%s\n' \
  '{"jsonrpc":"2.0","id":1,"method":"initialize","params":{"protocolVersion":"2024-11-05","capabilities":{},"clientInfo":{"name":"cli","version":"1"}}}' \
  '{"jsonrpc":"2.0","id":2,"method":"tools/call","params":{"name":"get_knowledge","arguments":{}}}' \
  | python3 MCPs/knowledge_mcp/server.py
```

**60.** Creare il venv condiviso e installare le dipendenze (con constraints OS-8):
```bash
python3 -m venv MCPs/venv && MCPs/venv/bin/pip install -r requirements.txt -c MCPs/constraints.txt
```

**61.** Installare le dipendenze di un singolo plugin:
```bash
MCPs/venv/bin/pip install -r MCPs/ollama_mcp/requirements.txt -c MCPs/constraints.txt
```

**62.** Pacchetti Python obsoleti (scope `requirements.txt`):
```bash
pip list --outdated --format=columns
```

**63.** Verifica veloce che i moduli chiave si importino:
```bash
python3 -c "import cv2, sympy, numpy; print('ok')"
```

**64.** SymPy al volo (stesso motore della tab Matematica):
```bash
python3 -c "import sympy as sp; x=sp.symbols('x'); print(sp.integrate(sp.sin(x)**2, x))"
```

---

## 🎬 Multimedia / Vision3D / Voce

**65.** Trascrizione audio con faster-whisper (CPU):
```bash
python3 Tools/scripts/fast_whisper_transcribe.py registrazione.wav
```

**66.** Test rapido della voce espeak-ng (usata anche da IPA click-to-hear):
```bash
espeak-ng -v it "prova della sintesi vocale"
```

**67.** Generare i PDF dei bersagli ArUco per la scala Vision3D:
```bash
python3 Tools/scripts/make_aruco_targets.py
```

**68.** Depth map di una foto (stessa pipeline di Scan 3D):
```bash
python3 Tools/scripts/depth_infer.py foto.jpg
```

**69.** Conversione PLY → OBJ+MTL colorati (passo 3 della ricostruzione):
```bash
python3 Tools/scripts/ply_to_obj.py scan_output/Sessione/dense.ply
```

**70.** Verifica che COLMAP sia installato e funzionante:
```bash
colmap -h | head -5
```

**71.** Convertire audio in WAV 16 kHz mono (formato atteso da whisper):
```bash
ffmpeg -i input.ogg -ar 16000 -ac 1 output.wav
```

**72.** Elenco webcam disponibili (OCR webcam, Analizza Video):
```bash
v4l2-ctl --list-devices
```

---

## 📱 Android

**73.** Build dell'APK (33 MB, clang ARM64, android-34):
```bash
./EXPORT/android/build_apk.sh
```

**74.** Dispositivi Android collegati via USB:
```bash
adb devices
```

**75.** Installare l'APK sul telefono:
```bash
adb install -r ANDROID/PrismaluxMobile.apk    # oppure ./EXPORT/android/installa_xiaomi.sh
```

**76.** Log dell'app Android in diretta:
```bash
adb logcat | grep -i prismalux
```

**77.** Test automatico dell'APK:
```bash
./EXPORT/android/test_apk.sh
```

---

## 📦 Export & Release

**78.** Creare l'AppImage completa (build inclusa):
```bash
./EXPORT/linux/crea_appimage.sh
```

**79.** AppImage senza ricompilare (usa il binario già in `build_gui/`):
```bash
./EXPORT/linux/crea_appimage.sh --no-build
```

**80.** Creare lo ZIP per Windows:
```bash
python3 EXPORT/windows/crea_zip_windows.py
```

**81.** Checksum della release (da pubblicare accanto al file):
```bash
sha256sum EXPORT/linux/Prismalux-x86_64.AppImage
```

**82.** Installare il launcher desktop dall'AppImage:
```bash
./EXPORT/linux/install_launcher.sh
```

**83.** Verificare che al binario non manchino librerie:
```bash
ldd build_gui/Prismalux_GUI | grep -i "not found"
```

---

## 🌡️ Hardware & termica

**84.** Temperatura CPU attuale:
```bash
sensors | grep -E "Core|Package|Tctl"
```

**85.** Monitor termico continuo (ogni 2 secondi):
```bash
watch -n2 sensors
```

**86.** Stato GPU NVIDIA (VRAM, temperatura, processi):
```bash
nvidia-smi
```

**87.** RAM libera/occupata:
```bash
free -h
```

**88.** RAM davvero disponibile (stesso valore letto da `checkRam()`):
```bash
grep MemAvailable /proc/meminfo
```

**89.** Spazio disco della home:
```bash
df -h ~
```

**90.** Peso delle cartelle principali del progetto:
```bash
du -sh RAG/ MCPs/ EXPORT/ build_gui/ gui/build_tests/ 2>/dev/null
```

---

## 🐞 Debug & crash

**91.** Elenco degli ultimi coredump di sistema:
```bash
coredumpctl list | tail
```

**92.** Aprire gdb sull'ultimo crash (come per il coredump VoiceCloner D-20):
```bash
coredumpctl debug
```

**93.** Lanciare l'app sotto gdb con backtrace automatico al crash:
```bash
gdb -batch -ex run -ex bt ./build_gui/Prismalux_GUI
```

**94.** Controllo leak mirato su una suite di test:
```bash
valgrind --leak-check=summary ./gui/build_tests/test_graph_memory
```

**95.** Avvio con log Qt verbosi:
```bash
QT_LOGGING_RULES="*.debug=true" ./build_gui/Prismalux_GUI
```

**96.** Journal di sistema in diretta (servizi, OOM-killer, crash):
```bash
journalctl -f
```

---

## 🔍 Ricerca nel codice & dati

**97.** Cercare una parola in tutto il codice GUI:
```bash
grep -rn "parolaChiave" gui/ --include=*.cpp --include=*.h
```

**98.** Bonifica porte hardcoded (devono esserci solo in `prismalux_paths.h`):
```bash
grep -rn "11434\|11500\|11600\|8081\|8092" gui/ --include=*.cpp --include=*.h | grep -v prismalux_paths
```

**99.** Ispezionare i database del progetto (GraphMemory / Hermes / RAG):
```bash
sqlite3 ~/.prismalux/graph_memory.db ".tables"    # anche hermes_memory.db, rag_graph.db
```

**100.** ⚠️ Reset delle impostazioni GUI (chiudere PRIMA l'app — fa backup, non cancella):
```bash
mv ~/.config/Prismalux/GUI.conf ~/.config/Prismalux/GUI.conf.bak
```

---

## 🐳 Docker & Sandbox

**101.** Container presenti sul sistema (sandbox Python + servizi):
```bash
docker ps -a
```

**102.** Scaricare/aggiornare l'immagine della sandbox Code Interpreter (default del progetto):
```bash
docker pull python:3.11-slim
```

**103.** Log di un container:
```bash
docker logs --tail 50 nomecontainer
```

**104.** Ricreare un container gestito da compose (come fa il pannello Aggiornamenti Sistema):
```bash
docker compose up -d --force-recreate
```

**105.** ⚠️ Pulizia immagini e cache Docker inutilizzate (chiede conferma):
```bash
docker system prune
```

---

## 🦙 llama.cpp & whisper.cpp

**106.** Avviare llama-server a mano (stesso comando del manager in MainWindow):
```bash
llama-server -m modello.gguf --port 8081 --host 127.0.0.1
```

**107.** Benchmark di un modello GGUF (token/s su questa macchina):
```bash
llama-bench -m modello.gguf
```

**108.** Nodo RPC per il cluster llama.cpp distribuito (calcolo su più PC):
```bash
rpc-server --host 0.0.0.0 --port 50052
```

**109.** Trascrizione con whisper.cpp compilato in locale:
```bash
~/.prismalux/whisper.cpp/build/bin/whisper-cli -m modello.bin -f audio.wav -l it
```

---

## 🗣️ TTS / STT extra

**110.** Sintesi vocale con Piper (voce neurale usata dalla GUI):
```bash
echo "ciao Paolo" | ~/.prismalux/piper/piper --model voce_it.onnx --output_file out.wav
```

**111.** Elenco delle voci italiane di espeak-ng:
```bash
espeak-ng --voices=it
```

**112.** Avviare a mano il demone STT persistente (debug — la GUI lo lancia da sola):
```bash
python3 Tools/scripts/stt_daemon.py
```

---

## 🖥️ Servizi & Web

**113.** Installare il servizio systemd del server LAN (avvio automatico al boot):
```bash
sudo cp EXPORT/linux/prismalux-server.service /etc/systemd/system/ && sudo systemctl daemon-reload && sudo systemctl enable --now prismalux-server
```

**114.** Log del servizio Ollama in diretta:
```bash
journalctl -u ollama -f
```

**115.** Scaricare l'APK dal LAN server (stesso percorso usato dal telefono):
```bash
curl -sk https://127.0.0.1:11500/apk -o PrismaluxMobile.apk
```

**116.** Stampare l'URL completo della web chat con token (da aprire sul telefono):
```bash
echo "https://$(ip -4 route get 1 | awk '{print $7; exit}'):11500/web?token=$(cat ~/.prismalux/lan_token.key)"
```

**117.** Verificare che il WAN Compute sia in ascolto (porta 11600):
```bash
nc -zv 127.0.0.1 11600
```

---

## 📄 Grafi & Documenti

**118.** Render di un file DOT Graphviz in PNG (stesso motore del viewer Grafo RAG):
```bash
dot -Tpng grafo.dot -o grafo.png
```

**119.** Estrarre il testo da un PDF (stesso tool usato da File AI):
```bash
pdftotext documento.pdf -
```

**120.** Backup a caldo di un DB SQLite (senza chiudere l'app):
```bash
sqlite3 ~/.prismalux/hermes_memory.db ".backup '/home/wildlux/hermes_backup.db'"
```

**121.** Backup compresso di RAG + memoria utente + dati app (roba NON in git!):
```bash
tar czf ~/backup_prismalux_$(date +%F).tar.gz RAG/ KNOWLEDGE_USER/ ~/.prismalux/
```

---

## 🎨 Qt & UI Debug

**122.** Test HiDPI — verifica che `dpiScale()` scali correttamente la UI:
```bash
QT_SCALE_FACTOR=2 ./build_gui/Prismalux_GUI
```

**123.** Forzare X11 (o Wayland) per confrontare i comportamenti:
```bash
QT_QPA_PLATFORM=xcb ./build_gui/Prismalux_GUI    # oppure QT_QPA_PLATFORM=wayland
```

**124.** Versione Qt6 realmente linkata dal binario:
```bash
ldd build_gui/Prismalux_GUI | grep libQt6Core
```

**125.** Pacchetti Qt6 installati sul sistema:
```bash
dpkg -l | grep -i qt6 | head
```

---

## 🪟 Windows (MSYS2 / UCRT64)

**126.** Prima installazione della toolchain portatile (una tantum, scarica ~600 MB):
```bat
COMPILE_WIN\setup.bat
```

**127.** Build su Windows (trova Python ed esegue `build.py`):
```bat
build.bat
```

**128.** Ollama CPU-only su Windows (la GPU del portatile non regge i modelli):
```bat
set OLLAMA_NUM_GPU=0 && ollama serve
```

**129.** Trovare quale Python è nel PATH di Windows:
```bat
where python
```

---

## 🧹 Sistema & Varie

**130.** File più grossi del progetto (escluse le build dir):
```bash
find . -size +100M -not -path "./build*" -not -path "./gui/build*" 2>/dev/null
```

**131.** Conteggio righe di codice della GUI:
```bash
find gui -name "*.cpp" -o -name "*.h" | xargs wc -l | tail -1
```

**132.** Carico GPU AMD via sysfs (stessa lettura di HwDetectAmd):
```bash
cat /sys/class/drm/card*/device/gpu_busy_percent
```

**133.** Misurare la durata di un comando (test, build, script):
```bash
time ./gui/build_tests/test_rag_engine
```

**134.** Aprire un file o una cartella con l'applicazione predefinita:
```bash
xdg-open EXPORT/linux/
```

**135.** Registrare nel menu di sistema i `.desktop` modificati a mano:
```bash
update-desktop-database ~/.local/share/applications
```

---

## 🧪 Test avanzati (QTest & ctest)

**136.** Rieseguire SOLO i test falliti dell'ultimo giro:
```bash
nice -n19 ctest --test-dir gui/build_tests --rerun-failed --output-on-failure
```

**137.** Output verboso completo di una suite (ogni riga del test, non solo i fail):
```bash
nice -n19 ctest --test-dir gui/build_tests -R "Vision3D" -V
```

**138.** Elencare le funzioni di test di una suite QTest (per poi lanciarne una sola, cmd 19):
```bash
./gui/build_tests/test_agenti_pipeline -functions
```

**139.** Modalità verbosa QTest (mostra ogni QVERIFY/QCOMPARE):
```bash
./gui/build_tests/test_graph_memory -v2
```

**140.** Salvare l'output di una suite QTest su file:
```bash
./gui/build_tests/test_rag_engine -o /tmp/risultato.txt,txt
```

**141.** Test headless senza display (utile via SSH o in script):
```bash
QT_QPA_PLATFORM=offscreen ./gui/build_tests/test_graph_memory
```

**142.** Kill-switch persistenza UI nei test (D-61 — evita che i test sporchino le QSettings):
```bash
PRISMALUX_NO_UI_PERSIST=1 ./gui/build_tests/test_impostazioni_page
```

**143.** Timeout personalizzato per suite lente:
```bash
nice -n19 ctest --test-dir gui/build_tests -R "ChatLive" -j1 --timeout 600 --output-on-failure
```

**144.** ⚠️ Compilare TUTTI i target di test (lungo, ~20 min — farlo in orario fresco):
```bash
nice -n19 cmake --build gui/build_tests -j4
```

**145.** Caccia ai test flaky — ripetere una suite finché non fallisce (max 5 giri):
```bash
nice -n19 ctest --test-dir gui/build_tests -R "SimulatoreAlgos" -j1 --repeat until-fail:5
```

---

## 🐍 Test Python & WAN (cartella `Test/`)

**146.** Lanciare tutti i test Python di integrazione AI:
```bash
python3 Test/run_all_tests.py
```

**147.** Test del protocollo WAN Compute da shell:
```bash
./Test/test_wan.sh    # e ./Test/test_wan_calc.sh per i task di calcolo
```

**148.** Test WAN Compute end-to-end in Python (porta 11600):
```bash
python3 Test/test_wan_compute.py
```

**149.** Test dei livelli di prompt sul modello locale (richiede Ollama):
```bash
python3 Test/test_prompt_levels.py
```

**150.** Test qualità RAG con documenti reali (paper e contenuti di fantasia):
```bash
python3 Test/test_rag_paper.py    # e python3 Test/test_rag_fantasia.py
```

---

## 🆘 Git di salvataggio

**151.** Recuperare un commit "perso" dopo un reset sbagliato (il reflog vede TUTTO):
```bash
git reflog    # poi: git checkout -b recupero <hash>
```

**152.** Trovare il commit che ha introdotto o rimosso una stringa (pickaxe):
```bash
git log -S "nomeFunzione" --oneline
```

**153.** Trovare il commit che ha rotto qualcosa (ricerca binaria automatica):
```bash
git bisect start && git bisect bad && git bisect good v3.0
# testa ogni checkout proposto, poi: git bisect good|bad — alla fine: git bisect reset
```

**154.** ⚠️ Anteprima pulizia file non tracciati (la `-n` mostra SOLTANTO — senza cancella davvero; MAI aggiungere `-x`: rimuoverebbe anche gli ignorati come `build_gui/` e `RAG/`):
```bash
git clean -fdn
```

---

## 🤖 Bot & Servizi esterni

**155.** Verificare che il token del bot Telegram sia valido:
```bash
curl -s "https://api.telegram.org/bot$(cat ~/.prismalux/telegram_token.key)/getMe" | python3 -m json.tool
```

**156.** GNS3 server attivo (tab LAN & WAN → GNS3 MCP):
```bash
curl -s http://localhost:3080/v2/version
```

**157.** OpenCode in ascolto sulla sua porta:
```bash
lsof -i :8092    # oppure: curl -s http://localhost:8092/ | head -c 200
```

**158.** Ispezionare i cron interni di Prismalux (tab Strumenti → Cron):
```bash
python3 -m json.tool ~/.prismalux/cron_jobs.json
```

---

## 🧠 Modelli LLM avanzati

**159.** Creare un modello Ollama personalizzato con system prompt fisso:
```bash
ollama create prismalux-ita -f Modelfile
# Modelfile minimo:  FROM mistral:7b-instruct  +  SYSTEM "Rispondi sempre in italiano."
```

**160.** Scaricare un GGUF da Hugging Face per llama-server:
```bash
huggingface-cli download TheBloke/Mistral-7B-Instruct-v0.2-GGUF --include "*Q4_K_M.gguf" --local-dir modelli/
```

**161.** Riquantizzare un GGUF (ridurre RAM necessaria, tab Fine-tuning):
```bash
llama-quantize modello-f16.gguf modello-q4.gguf Q4_K_M
```

---

## 🧊 Termica attiva (non solo monitoraggio)

**162.** Limitare la frequenza CPU PRIMA di build/test lunghi (meno 90°C):
```bash
sudo cpupower frequency-set -g powersave
```

**163.** Tornare al governor normale a lavoro finito:
```bash
sudo cpupower frequency-set -g performance    # o "schedutil", il default
```

**164.** Impedire lo standby del PC durante un lavoro lungo (AppImage, test a lotti):
```bash
systemd-inhibit --what=sleep:idle nice -n19 ./Test/ctest_lotti.sh
```

---

## 🕵️ Rete profonda / Debug protocolli

**165.** Vedere il traffico del LAN server in tempo reale (debug protocollo):
```bash
sudo tcpdump -i any port 11500 -A
```

**166.** Aprire la porta del LAN server nel firewall (se il telefono non si collega):
```bash
sudo ufw allow 11500/tcp && sudo ufw status
```

**167.** Test Nominatim da CLI (stesso servizio della Mappa OSM):
```bash
curl -s "https://nominatim.openstreetmap.org/search?q=Roma&format=json&limit=1" -H "User-Agent: Prismalux" | python3 -m json.tool
```

**168.** Test routing OSRM da CLI (Roma→Milano, stesso endpoint di WorldMapWidget):
```bash
curl -s "https://router.project-osrm.org/route/v1/driving/12.49,41.90;9.19,45.46?overview=false" | python3 -m json.tool | head -20
```

---

## 🎞️ Multimedia extra

**169.** Trascrizione IPA di una parola (combacia con la tab Media → IPA):
```bash
espeak-ng --ipa -q -v it "prova"
```

**170.** Estrarre un frame al secondo da un video (come Analizza Video):
```bash
ffmpeg -i video.mp4 -vf fps=1 frame_%03d.jpg
```

**171.** Registrare lo schermo per una demo (solo X11 — su Wayland usare Ctrl+Alt+Shift+R di GNOME):
```bash
ffmpeg -f x11grab -framerate 25 -i :0.0 demo.mp4
```

**172.** Scoprire i dispositivi Tuya sulla LAN (camera WIBY JS-P161):
```bash
python3 -m tinytuya scan
```

---

## 🧬 Bioinformatica

**173.** Installare i tool della pipeline bioinformatica via Bioconda:
```bash
conda install -c bioconda -c conda-forge samtools bwa fastqc
```

**174.** Verificare che RDKit funzioni (stesso motore della tab RDKit):
```bash
python3 -c "from rdkit import Chem; print(Chem.MolToSmiles(Chem.MolFromSmiles('c1ccccc1')))"
```

**175.** Versioni dei tool bioinformatici installati:
```bash
samtools --version | head -1; fastqc --version; bwa 2>&1 | head -3
```

---

## 📦 Dipendenze di sistema

**176.** Reinstallare le dipendenze di sistema su un PC nuovo (elenco indicativo — Qt6 dev a parte):
```bash
sudo apt install build-essential cmake git curl libjs-katex graphviz poppler-utils lm-sensors v4l-utils espeak-ng ffmpeg sqlite3 adb gh
```

**177.** Verificare che KaTeX di sistema sia presente (richiesto da LatexView):
```bash
dpkg -L libjs-katex | head
```

---

## 🔬 Profiling & App congelata

**178.** Profilare dove va davvero il tempo (poi aprire con kcachegrind):
```bash
valgrind --tool=callgrind ./gui/build_tests/test_rag_engine && kcachegrind callgrind.out.*
```

**179.** Vedere su quale syscall è bloccata l'app appesa (senza chiuderla):
```bash
strace -p $(pidof Prismalux_GUI)
```

**180.** Backtrace di TUTTI i thread di un processo vivo ma congelato:
```bash
gdb -p $(pidof Prismalux_GUI) -batch -ex "thread apply all bt"
```

**181.** Quale thread sta mangiando la CPU:
```bash
top -H -p $(pidof Prismalux_GUI)
```

---

## 🗃️ SQLite & Dati interni

**182.** Schema dei DB GraphMemory (tabelle `gm_nodes` / `gm_edges`):
```bash
sqlite3 ~/.prismalux/graph_memory.db ".schema"
```

**183.** Quanti ricordi ha Hermes (nodi e archi):
```bash
sqlite3 ~/.prismalux/hermes_memory.db "SELECT COUNT(*) FROM gm_nodes; SELECT COUNT(*) FROM gm_edges;"
```

**184.** Compattare un DB dopo un prune (recupera spazio su disco):
```bash
sqlite3 ~/.prismalux/rag_graph.db "VACUUM;"
```

**185.** Ultimi feedback registrati dall'app:
```bash
tail -n 3 ~/.prismalux/feedback.jsonl
```

**186.** Ispezionare la cache risposte esatte (D-25, query ripetute = zero token):
```bash
python3 -m json.tool ~/.prismalux/response_cache.json | head -30
```

---

## 🕹️ TeleComanda da CLI

**187.** Blender headless con script Python (come lo pilota il MCP):
```bash
blender --background --python script.py
```

**188.** FreeCAD senza GUI:
```bash
FreeCADCmd script.py    # su alcuni pacchetti il binario è "freecadcmd"
```

**189.** Godot headless:
```bash
godot --headless --script script.gd
```

**190.** Export PDF di uno schema KiCAD da CLI:
```bash
kicad-cli sch export pdf schema.kicad_sch -o schema.pdf
```

**191.** CloudCompare in batch (subsampling di una nuvola di punti):
```bash
CloudCompare -SILENT -O nuvola.ply -SS SPATIAL 0.01
```

**192.** Test AnkiConnect (porta 8765, la stessa usata dalla tab Anki — Anki deve essere aperto):
```bash
curl -s localhost:8765 -d '{"action":"version","version":6}'
```

---

## 🔬 Reverse Engineering (tab Reverse)

**193.** Identificare che cos'è davvero un file (tipo, architettura, stripped o no):
```bash
file build_gui/Prismalux_GUI
```

**194.** Testo leggibile dentro un binario:
```bash
strings binario | grep -i "password\|http"
```

**195.** Disassemblato di un eseguibile:
```bash
objdump -d binario | less
```

**196.** Dump esadecimale (i primi byte rivelano il formato — magic number):
```bash
xxd file | head
```

---

## 🔌 Driver & Bus hardware (tab Driver)

**197.** Dispositivi USB collegati:
```bash
lsusb
```

**198.** Scheda video e driver in uso:
```bash
lspci -nnk | grep -A3 VGA
```

**199.** Eventi kernel in diretta (chiavette, webcam, errori driver):
```bash
sudo dmesg --follow
```

**200.** Verificare che il modulo della webcam sia caricato:
```bash
lsmod | grep uvcvideo
```

---

## ⚙️ Tuning Ollama (variabili d'ambiente)

**201.** ⚠️ Esporre Ollama a tutta la LAN (altri PC/telefono — NESSUNA autenticazione, solo rete fidata). Se Ollama gira come servizio systemd, la variabile va in `sudo systemctl edit ollama` → `[Service] Environment=...`:
```bash
OLLAMA_HOST=0.0.0.0 ollama serve
```

**202.** Quanto tenere il modello caricato in RAM dopo l'ultima richiesta:
```bash
OLLAMA_KEEP_ALIVE=30m ollama serve
```

**203.** Spostare la cartella dei modelli su un altro disco:
```bash
OLLAMA_MODELS=/mnt/disco/modelli ollama serve
```

---

## 📲 Condivisione rapida PC ↔ Telefono

**204.** QR della web chat direttamente NEL TERMINALE (da inquadrare col telefono):
```bash
qrencode -t ansiutf8 "https://$(ip -4 route get 1 | awk '{print $7; exit}'):11500/web?token=$(cat ~/.prismalux/lan_token.key)"
```

**205.** Servire una cartella al telefono in 2 secondi (poi aprire `http://<IP-PC>:8000`):
```bash
python3 -m http.server 8000
```

**206.** Misurare la banda reale PC↔telefono — lato server (sul PC):
```bash
iperf3 -s
```

**207.** …e lato client (dall'altro dispositivo — app iperf3 su Android):
```bash
iperf3 -c 192.168.1.X
```

---

## 🌍 SSH & Cluster

**208.** Usare l'Ollama di un altro PC come fosse locale (tunnel sulla 11434):
```bash
ssh -L 11434:localhost:11434 utente@altropc
```

**209.** Distribuire l'AppImage a un altro PC (riprende anche i trasferimenti interrotti):
```bash
rsync -avP EXPORT/linux/Prismalux-x86_64.AppImage utente@altropc:~/
```

**210.** Avviare un nodo del cluster llama.cpp da remoto (resta vivo dopo il logout):
```bash
ssh utente@nodo 'nohup nice -n19 rpc-server --host 0.0.0.0 --port 50052 >/dev/null 2>&1 &'
```

---

## 🧽 Manutenzione disco & Journal

**211.** Quanto spazio mangia il journal di sistema (e ridurlo a 500 MB):
```bash
journalctl --disk-usage    # poi: sudo journalctl --vacuum-size=500M
```

**212.** Salute del disco (macchina datata → controllo periodico):
```bash
sudo smartctl -H /dev/sda
```

**213.** Backup notturno automatico (riga da aggiungere con `crontab -e` — ogni notte alle 3):
```bash
0 3 * * * tar czf ~/backup_prismalux_$(date +\%F).tar.gz -C /home/wildlux/Desktop/Prismalux RAG KNOWLEDGE_USER
```

---

## 📦 AppImage internals

**214.** Estrarre il contenuto dell'AppImage per ispezionarlo (crea `squashfs-root/`):
```bash
./EXPORT/linux/Prismalux-x86_64.AppImage --appimage-extract
```

**215.** Eseguire l'AppImage su sistemi senza FUSE:
```bash
./EXPORT/linux/Prismalux-x86_64.AppImage --appimage-extract-and-run
```

---

## 💬 API Web chat con token

**216.** Chat via curl con header Bearer (stesso canale del telefono — verificato in `lan_server.cpp`):
```bash
curl -sk https://127.0.0.1:11500/api/chat \
  -H "Authorization: Bearer $(cat ~/.prismalux/lan_token.key)" \
  -d '{"model":"mistral:7b-instruct","messages":[{"role":"user","content":"ciao"}],"stream":false}'
```

**217.** Autenticazione via query string (fallback `?token=`, stesso meccanismo del QR):
```bash
curl -sk "https://127.0.0.1:11500/api/tags?token=$(cat ~/.prismalux/lan_token.key)"
```

---

## 🍎 Build macOS (N10 — non ancora testata su hardware reale)

**218.** Installare i prerequisiti con Homebrew:
```bash
brew install qt6 cmake ninja
```

**219.** Configurare e compilare (Qt6 di Homebrew su Apple Silicon):
```bash
export PATH="/opt/homebrew/opt/qt6/bin:$PATH"
cmake -B gui/build_gui gui/ -DCMAKE_BUILD_TYPE=Release -DCMAKE_PREFIX_PATH=/opt/homebrew/opt/qt6
cmake --build gui/build_gui -j$(sysctl -n hw.ncpu)
```

**220.** Avviare il bundle (macdeployqt copia i framework automaticamente dalla build):
```bash
open gui/build_gui/Prismalux_GUI.app
```

---

## ⚡ Build più veloce & Analisi statica

**221.** Attivare ccache — le rebuild dopo una pulizia diventano quasi istantanee (meno tempo = meno gradi):
```bash
sudo apt install ccache
cmake -B build_gui gui/ -DCMAKE_BUILD_TYPE=Release -DCMAKE_CXX_COMPILER_LAUNCHER=ccache
```

**222.** Generare `compile_commands.json` per clangd/IDE (autocompletamento e diagnostica precisi):
```bash
cmake -B build_gui gui/ -DCMAKE_BUILD_TYPE=Release -DCMAKE_EXPORT_COMPILE_COMMANDS=ON
ln -sf build_gui/compile_commands.json .
```

**223.** Analisi clang-tidy su un singolo file (usa il compile_commands del punto sopra):
```bash
clang-tidy gui/pages/main_ai_pipeline.cpp -p build_gui
```

**224.** Analisi statica cppcheck su tutta la GUI (trova bug senza compilare):
```bash
cppcheck --enable=warning,performance gui/ 2>&1 | tail -30
```

---

## 🧨 Sanitizer (caccia ai SEGV)

**225.** Build con AddressSanitizer + UBSan — al primo use-after-free stampa la riga esatta (altro che coredump da interpretare):
```bash
cmake -B build_asan gui/ -DCMAKE_BUILD_TYPE=Debug \
      -DCMAKE_CXX_FLAGS="-fsanitize=address,undefined -g" \
      -DCMAKE_EXE_LINKER_FLAGS="-fsanitize=address,undefined"
nice -n19 cmake --build build_asan -j4
```

**226.** Eseguire l'app instrumentata (si ferma al primo errore di memoria):
```bash
ASAN_OPTIONS=halt_on_error=1 ./build_asan/Prismalux_GUI
```

---

## 🎤 Audio & Microfono (debug STT/TTS)

**227.** Il microfono funziona? Registra 3 secondi e riascolta (prima di incolpare whisper):
```bash
arecord -d 3 -f cd /tmp/test_mic.wav && aplay /tmp/test_mic.wav
```

**228.** Elencare microfoni e uscite audio (qual è il default?):
```bash
pactl list sources short    # microfoni — per le uscite: pactl list sinks short
```

**229.** Test webcam al volo (tab OCR / Analizza Video):
```bash
ffplay /dev/video0
```

---

## 🌐 Servizi live usati dai tool dell'app

**230.** OCR da CLI con tesseract (stesso motore della tab Multimedia e di `rag_graph.cpp`):
```bash
tesseract foto.jpg risultato -l ita && cat risultato.txt
```

**231.** Scaricare/aprire uno stream video (stesso motore di `streamlink_mcp`):
```bash
streamlink "https://www.twitch.tv/canale" best -o video.mp4
```

**232.** Test della ricerca web del tool `ricerca` (stesso endpoint di main_ai_tools_calls.cpp):
```bash
curl -s "https://lite.duckduckgo.com/lite/?q=prismalux" | head -c 500
```

**233.** Test della fonte tassi di cambio del tool `cambio_valuta` (frankfurter.app/BCE):
```bash
curl -s "https://api.frankfurter.app/latest?amount=100&from=EUR&to=USD"
```

---

## 🪟 MSYS2 / pacman (ambiente UCRT64 Windows)

**234.** Aggiornare tutto MSYS2 (da rilanciare se chiede di riavviare la shell):
```bash
pacman -Syu
```

**235.** Installare toolchain e Qt6 per la build nativa Windows:
```bash
pacman -S mingw-w64-ucrt-x86_64-toolchain mingw-w64-ucrt-x86_64-cmake mingw-w64-ucrt-x86_64-qt6
```

---

## 🐳 Sandbox Docker manuale

**236.** Replicare a mano quello che fa il Code Interpreter (stessa immagine, stesso isolamento):
```bash
docker run --rm python:3.11-slim python3 -c "print(2+2)"
```

---

## 📊 Monitor moderni

**237.** Monitor GPU interattivo (più leggibile di nvidia-smi, supporta anche AMD):
```bash
nvtop
```

**238.** nvidia-smi in loop ogni 2 secondi (VRAM mentre Ollama carica un modello):
```bash
nvidia-smi -l 2
```

---

## 🖼️ Qt & GUI — Debug finale

**239.** DevTools Chrome dentro le viste WebEngine (webchat.html, KaTeX di LatexView) — poi aprire `chrome://inspect` nel browser:
```bash
QTWEBENGINE_REMOTE_DEBUGGING=9222 ./build_gui/Prismalux_GUI
```

**240.** Diagnosticare "could not load platform plugin xcb" (classico delle AppImage):
```bash
QT_DEBUG_PLUGINS=1 ./build_gui/Prismalux_GUI 2>&1 | grep -i "plugin\|error" | head
```

**241.** Font emoji installati (se le emoji della UI/TriModeButton escono come quadratini):
```bash
fc-list | grep -i emoji    # dopo aver installato font: fc-cache -f
```

---

## 📄 LibreOffice headless (tab TeleComanda → Office)

**242.** Convertire documenti da CLI senza aprire LibreOffice:
```bash
soffice --headless --convert-to pdf documento.odt
```

---

## 🧠 RAM, Swap & OOM (modelli grossi)

**243.** L'app o Ollama sono spariti da soli? Controllare se è stato l'OOM-killer del kernel:
```bash
journalctl -k | grep -i "out of memory\|oom" | tail
```

**244.** Quanta swap c'è (e quanta se ne sta usando):
```bash
swapon --show && free -h
```

**245.** Aggiungere 8 GB di swap per far girare modelli oltre la RAM fisica (lenti ma caricabili):
```bash
sudo fallocate -l 8G /swapfile && sudo chmod 600 /swapfile && sudo mkswap /swapfile && sudo swapon /swapfile
```

---

## 🗄️ Backup — Verifica & Ripristino

**246.** Verificare il contenuto di un backup SENZA estrarlo (il backup del comando 121/213):
```bash
tar -tzf ~/backup_prismalux_2026-07-18.tar.gz | head -20
```

**247.** Ripristinare il backup in una cartella di appoggio (mai direttamente sopra l'originale):
```bash
mkdir ~/ripristino && tar -xzf ~/backup_prismalux_2026-07-18.tar.gz -C ~/ripristino
```

---

## ⚖️ Peso della release

**248.** Cosa pesa davvero dentro l'AppImage (dopo `--appimage-extract`, comando 214):
```bash
du -sh squashfs-root/* squashfs-root/usr/* 2>/dev/null | sort -h | tail -15
```

**249.** Ridurre il binario rimuovendo i simboli di debug (⚠️ i backtrace diventano illeggibili — farlo solo sulla copia da distribuire):
```bash
strip build_gui/Prismalux_GUI && ls -la build_gui/Prismalux_GUI
```

---

## 🌿 Git — Ultimi attrezzi

**250.** Lavorare su due rami contemporaneamente senza stash (seconda copia collegata allo stesso repo):
```bash
git worktree add ../Prismalux-hotfix master    # a fine lavoro: git worktree remove ../Prismalux-hotfix
```

**251.** Esportare uno snapshot pulito dei sorgenti (senza `.git`, senza file locali):
```bash
git archive --format=tar.gz HEAD -o ~/prismalux_src.tar.gz
```

---

## 🧰 Dev loop

**252.** Abilitare i coredump nella shell corrente (senza, un crash fuori da systemd non lascia traccia):
```bash
ulimit -c unlimited
```

**253.** Indicizzazione RAG o job disco-intensivi senza rallentare il resto del sistema:
```bash
ionice -c3 nice -n19 comando_pesante
```
