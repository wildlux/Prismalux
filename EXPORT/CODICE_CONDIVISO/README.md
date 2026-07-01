# Prismalux — Guida installazione

## 1. Installa le dipendenze di sistema

```bash
sudo apt install \
    cmake ninja-build build-essential \
    qt6-base-dev qt6-tools-dev qt6-webengine-dev \
    libqt6sql6-sqlite qt6-multimedia-dev \
    libqt6bluetooth6 qt6-speech qt6keychain-dev \
    libonnxruntime-dev ffmpeg graphviz \
    sqlite3 openssl libssl-dev \
    python3 python3-pip python3-venv
```

## 2. Estrai il pacchetto

```bash
unzip Prismalux_completo_*.zip -d Prismalux
cd Prismalux
```

## 3. Installa i plugin Python (MCPs)

```bash
python3 -m venv ~/.prismalux/venv
~/.prismalux/venv/bin/pip install -r requirements.txt
```

## 4. Compila

```bash
python3 build.py
```

## 5. Avvia

```bash
./build_gui/Prismalux_GUI
```

---

## Per i modelli AI

Prismalux richiede **Ollama** per far girare i modelli linguistici locali:

```bash
curl -fsSL https://ollama.com/install.sh | sh
ollama pull mistral
```

Ollama deve essere in esecuzione prima di avviare Prismalux.
