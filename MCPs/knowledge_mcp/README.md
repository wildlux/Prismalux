# Knowledge MCP — Prismalux

**Tipo**: stdio (nessun server esterno, nessuna porta)

## Funzione
Mantiene aggiornato `KNOWLEDGE_USER/user_knowledge.md` — la memoria persistente
dell'utente. Il file viene iniettato nel system prompt di ogni agente e chat
per evitare che l'AI "dimentichi" contesto, preferenze e procedure consolidate.

## Installazione

### 1. Dipendenze Python
Nessuna — usa solo la stdlib Python (json, os, re, fcntl).

### 2. Nessun software esterno richiesto
Il MCP legge e scrive direttamente su file locale.

### 3. File di memoria
Il file viene creato automaticamente alla prima scrittura:
```
Prismalux/KNOWLEDGE_USER/user_knowledge.md
```

Per pre-popolarlo con informazioni sull'utente, modificalo manualmente:
```markdown
# Conoscenza Utente

## Chi sono
Nome: Mario, sviluppatore Python senior.

## Preferenze
- Risposte concise
- Esempi pratici

## Procedure consolidate
- Build: python3 build.py
```

## Aggiungere a Claude Code
In `~/.claude/settings.json`:
```json
{
  "mcpServers": {
    "knowledge": {
      "type": "stdio",
      "command": "python3",
      "args": ["/home/wildlux/Desktop/Prismalux/MCPs/knowledge_mcp/server.py"]
    }
  }
}
```
