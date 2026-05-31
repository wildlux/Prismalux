# Cytoscape MCP — Prismalux

**Tipo**: HTTP REST (CyREST API nativa Cytoscape)
**Porta**: 1234

## Funzione
Crea e analizza reti biologiche (proteomica, genomica, pathway) in Cytoscape:
aggiunge nodi/archi, applica layout, esporta immagini PNG/SVG.

## Installazione

### 1. Dipendenze Python
Nessuna pip — usa solo `urllib` della stdlib.

### 2. Installa Java (richiesto da Cytoscape)
```bash
# Linux
sudo apt install default-jre
# macOS
brew install openjdk
# Windows: scarica da https://adoptium.net/
```

### 3. Installa Cytoscape
- **Linux / Windows / macOS**: scarica da https://cytoscape.org/download.html

### 4. Avvio
Apri Cytoscape — CyREST è abilitato di default sulla porta 1234.

Verifica:
```bash
curl http://localhost:1234/v1/version
# risposta: {"apiVersion":"v1","cytoscapeVersion":"3.x.x"}
```

## Aggiungere a Claude Code
In `~/.claude/settings.json`:
```json
{
  "mcpServers": {
    "cytoscape": {
      "type": "stdio",
      "command": "python3",
      "args": ["/home/wildlux/Desktop/Prismalux/MCPs/cytoscape_mcp/server.py"]
    }
  }
}
```
