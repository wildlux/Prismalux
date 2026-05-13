# TinyMCP (Arduino / Microcontroller)

**Tipo**: Bridge seriale USB → Python MCP

## Funzione
Permette a Prismalux di programmare Arduino/ESP32/Raspberry Pi Pico,
compilare sketch, fare upload via avrdude/esptool, leggere serial monitor.

## Dipendenze
```bash
pip install pyserial
sudo apt install arduino-cli  # oppure Arduino IDE
```

## TODO
- [ ] Scrivere `tinymcp_server.py` — MCP stdio che wrappa arduino-cli
- [ ] Comandi: compile, upload, serial_monitor, list_ports
