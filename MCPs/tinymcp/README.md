# TinyMCP — Prismalux (Arduino / Microcontroller Bridge)

**Tipo**: subprocess (arduino-cli + pyserial)

## Funzione
Compila e carica sketch Arduino/ESP32/Raspberry Pi Pico, monitora la porta
seriale, gestisce librerie Arduino direttamente dall'AI di Prismalux.

## Installazione

### 1. Dipendenze Python
```bash
pip install pyserial
```

### 2. Installa arduino-cli

**Linux:**
```bash
curl -fsSL https://raw.githubusercontent.com/arduino/arduino-cli/master/install.sh | sh
```

**Windows:**
```powershell
winget install arduino.arduinocli
```

**macOS:**
```bash
brew install arduino-cli
```

### 3. Configura arduino-cli
```bash
arduino-cli config init
arduino-cli core update-index

# Arduino Uno / Nano / Mega
arduino-cli core install arduino:avr

# ESP32
arduino-cli core install esp32:esp32 \
  --additional-urls https://raw.githubusercontent.com/espressif/arduino-esp32/gh-pages/package_esp32_index.json

# Raspberry Pi Pico
arduino-cli core install rp2040:rp2040 \
  --additional-urls https://github.com/earlephilhower/arduino-pico/releases/download/global/package_rp2040_index.json
```

### 4. Permessi porta seriale (solo Linux)
```bash
sudo usermod -a -G dialout $USER
# poi esci e rientra nella sessione
```

### 5. Verifica
```bash
arduino-cli version
arduino-cli board list
python3 -c "import serial; print('pyserial OK', serial.__version__)"
```

## Aggiungere a Claude Code
In `~/.claude/settings.json`:
```json
{
  "mcpServers": {
    "tinymcp": {
      "type": "stdio",
      "command": "python3",
      "args": ["/home/wildlux/Desktop/Prismalux/MCPs/tinymcp/server.py"]
    }
  }
}
```
