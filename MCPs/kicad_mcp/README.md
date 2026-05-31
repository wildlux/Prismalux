# KiCAD MCP — Prismalux

**Tipo**: HTTP Bridge (porta 3000, avviato da KiCAD Scripting Console)

## Funzione
Genera schemi elettrici e layout PCB, aggiunge componenti, crea footprint
e riverifica le regole DRC direttamente da Prismalux via AI.

## Installazione

### 1. Dipendenze Python
Nessuna pip — usa `urllib` della stdlib. KiCAD include la propria Python.

### 2. Installa KiCAD
- **Linux**: `sudo apt install kicad`
- **Windows / macOS**: scarica da https://www.kicad.org/download/

### 3. Avvia il bridge dalla KiCAD Scripting Console
1. Apri KiCAD → **PCB Editor** (Pcbnew)
2. Menu **Tools → Scripting Console**
3. Digita nella console:
```python
exec(open('/home/wildlux/Desktop/Prismalux/MCPs/kicad_mcp/kicad_bridge.py').read())
```
Il server parte su `http://localhost:3000`.

Verifica:
```bash
curl http://localhost:3000/health
```

### 4. Avvio automatico (opzionale)
```bash
# Linux
cp MCPs/kicad_mcp/kicad_bridge.py ~/.config/kicad/scripting/plugins/

# Windows
copy MCPs\kicad_mcp\kicad_bridge.py %APPDATA%\kicad\scripting\plugins\
```

## Aggiungere a Claude Code
In `~/.claude/settings.json`:
```json
{
  "mcpServers": {
    "kicad": {
      "type": "stdio",
      "command": "python3",
      "args": ["/home/wildlux/Desktop/Prismalux/MCPs/kicad_mcp/server.py"]
    }
  }
}
```
