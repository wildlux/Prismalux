# Anki MCP — Prismalux

**Tipo**: HTTP (AnkiConnect REST)  
**Porta**: 8765

## Implementazioni disponibili

### 1. Python (server.py) — Python puro, nessuna dipendenza npm
Implementazione locale compatibile con API v0.18.5.
13 tool: get_decks, create_note, create_notes_bulk, search_notes, get_note, update_note, delete_notes, add_deck, get_stats, get_note_types, get_tags, sync_anki, get_cards_due.

### 2. @ankimcp/anki-mcp-server v0.18.5 — pacchetto npm ufficiale
Installato in `node_modules/` tramite npm. Usa `node_modules/.bin/anki-mcp-server`.

## Prerequisiti

### 1. Installa Anki
- **Linux**: `sudo apt install anki`  oppure scarica da https://apps.ankiweb.net
- **Windows**: scarica da https://apps.ankiweb.net
- **macOS**: `brew install --cask anki`

### 2. Installa l'addon AnkiConnect
1. Apri Anki
2. Menu **Tools → Add-ons → Get Add-ons...**
3. Inserisci il codice: **`2055492159`**
4. Riavvia Anki

### 3. Verifica funzionamento
```bash
curl http://localhost:8765 -X POST -d '{"action":"version","version":6}'
# risposta attesa: {"result":6,"error":null}
```

## Aggiungere a Claude Code

### Opzione A — Python (consigliata, nessuna dipendenza extra)
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

### Opzione B — npm @ankimcp/anki-mcp-server v0.18.5 (ufficiale)
In `~/.claude/settings.json`:
```json
{
  "mcpServers": {
    "anki": {
      "type": "stdio",
      "command": "node",
      "args": ["/home/wildlux/Desktop/Prismalux/MCPs/anki_mcp/node_modules/.bin/anki-mcp-server"]
    }
  }
}
```

## Re-installazione npm
```bash
cd /home/wildlux/Desktop/Prismalux/MCPs/anki_mcp
npm install @ankimcp/anki-mcp-server@0.18.5
```
