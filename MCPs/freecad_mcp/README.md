# FreeCAD MCP — Prismalux

**Tipo**: HTTP Bridge (porta 9876, avviato da FreeCAD)

## Funzione
Crea modelli 3D parametrici, esporta STL/STEP/OBJ, modifica solidi
direttamente da Prismalux via AI — FreeCAD esegue il codice Python.

## Installazione

### 1. Dipendenze Python
Nessuna pip — usa `urllib` della stdlib.

### 2. Installa FreeCAD
- **Linux**: `sudo apt install freecad`  oppure  `flatpak install FreeCAD`
- **Windows**: scarica da https://www.freecad.org/downloads.php
- **macOS**: `brew install --cask freecad`

### 3. Copia il bridge nella cartella Macro di FreeCAD
```bash
# Linux
cp MCPs/freecad_mcp/freecad_bridge.py ~/.FreeCAD/Macro/

# Windows
copy MCPs\freecad_mcp\freecad_bridge.py %APPDATA%\FreeCAD\Macro\

# macOS
cp MCPs/freecad_mcp/freecad_bridge.py ~/Library/Preferences/FreeCAD/Macro/
```

### 4. Avvia il bridge da FreeCAD
1. Apri FreeCAD
2. Menu **Macro → Macros...**
3. Seleziona `freecad_bridge.py` → clicca **Execute**

Il server parte su `http://localhost:9876`.

```bash
curl http://localhost:9876/health
```

### 5. Avvio automatico (opzionale)
**Edit → Preferences → General → Macro** → aggiungi `freecad_bridge.py` all'avvio.

## Aggiungere a Claude Code
In `~/.claude/settings.json`:
```json
{
  "mcpServers": {
    "freecad": {
      "type": "stdio",
      "command": "python3",
      "args": ["/home/wildlux/Desktop/Prismalux/MCPs/freecad_mcp/server.py"]
    }
  }
}
```
