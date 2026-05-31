# Office Bridge MCP — Prismalux

**Tipo**: HTTP Bridge (porta 6790)

## Funzione
Crea e modifica documenti Word (.docx), Excel (.xlsx) e PowerPoint (.pptx)
via due modalità: UNO (LibreOffice diretto) o file-based (python-docx/openpyxl/pptx).

## Installazione

### 1. Dipendenze Python

**Modalità file (sempre disponibile):**
```bash
pip install python-docx openpyxl python-pptx
```

**Modalità UNO (controllo diretto LibreOffice, opzionale):**
```bash
# Linux
sudo apt install python3-uno libreoffice

# Windows/macOS: UNO è incluso con LibreOffice
# Scarica da https://www.libreoffice.org/download/
```

### 2. Installa LibreOffice (opzionale ma consigliato per UNO)
- **Linux**: `sudo apt install libreoffice`
- **Windows**: scarica da https://www.libreoffice.org/download/
- **macOS**: `brew install --cask libreoffice`

### 3. Avvia il bridge
```bash
python3 MCPs/office_bridge/prismalux_office_bridge.py
```
Il server parte su `http://localhost:6790`.

Verifica:
```bash
curl http://localhost:6790/health
# risposta: {"status": "ok", "mode": "uno"}  oppure  {"mode": "file"}
```

### 4. Modalità disponibili
| Modalità | Richiede | Funzionalità |
|----------|----------|--------------|
| **UNO** | LibreOffice aperto + python3-uno | Apertura, modifica, salvataggio file esistenti |
| **File** | python-docx, openpyxl, python-pptx | Crea nuovi documenti da zero |

Il bridge sceglie automaticamente UNO se disponibile, altrimenti usa la modalità file.

## Aggiungere a Claude Code
In `~/.claude/settings.json`:
```json
{
  "mcpServers": {
    "office": {
      "type": "stdio",
      "command": "python3",
      "args": ["/home/wildlux/Desktop/Prismalux/MCPs/office_bridge/prismalux_office_bridge.py"]
    }
  }
}
```
