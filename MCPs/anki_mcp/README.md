# Anki MCP — Prismalux

**Tipo**: HTTP (AnkiConnect REST)
**Porta**: 8765

## Funzione
Crea mazzi, aggiunge carte, esegue revisioni e legge statistiche di Anki
direttamente dall'AI di Prismalux.

## Installazione

### 1. Dipendenze Python
Nessuna — usa solo la stdlib Python.

### 2. Installa Anki
- **Linux**: `sudo apt install anki`  oppure scarica da https://apps.ankiweb.net
- **Windows**: scarica da https://apps.ankiweb.net
- **macOS**: `brew install --cask anki`

### 3. Installa l'addon AnkiConnect
1. Apri Anki
2. Menu **Tools → Add-ons → Get Add-ons...**
3. Inserisci il codice: **`2055492159`**
4. Riavvia Anki

### 4. Avvio
Anki deve essere **aperto e in esecuzione**. Il server HTTP parte automaticamente su `http://localhost:8765`.

Verifica:
```bash
curl http://localhost:8765 -X POST -d '{"action":"version","version":6}'
# risposta attesa: {"result":6,"error":null}
```

## Aggiungere a Claude Code
In `~/.claude/settings.json`:
```json
{
  "mcpServers": {
    "anki": {
      "type": "stdio",
      "command": "python3",
      "args": ["/home/wildlux/Desktop/Prismalux/MCPs/anki_mcp/server.py"]
    }
  }
}
```
