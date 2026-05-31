# Graphviz MCP — Prismalux

**Tipo**: Python library (locale, nessun server esterno)

## Funzione
Genera diagrammi DOT, grafi orientati/non orientati, mappe concettuali
e alberi in PNG, SVG o PDF.

## Installazione

### 1. Dipendenze Python
```bash
pip install graphviz
```

### 2. Installa il binario Graphviz nel sistema
Il pacchetto Python è solo un wrapper — il binario `dot` è obbligatorio.
```bash
# Linux
sudo apt install graphviz
# macOS
brew install graphviz
# Windows
winget install graphviz
# oppure: choco install graphviz
# oppure scarica da https://graphviz.org/download/
```

### 3. Verifica
```bash
dot -V
python3 -c "import graphviz; print('OK', graphviz.__version__)"
```

## Aggiungere a Claude Code
In `~/.claude/settings.json`:
```json
{
  "mcpServers": {
    "graphviz": {
      "type": "stdio",
      "command": "python3",
      "args": ["/home/wildlux/Desktop/Prismalux/MCPs/graphviz_mcp/server.py"]
    }
  }
}
```
