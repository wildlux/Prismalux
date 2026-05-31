# Ollama MCP — Prismalux

**Tipo**: stdio + HTTP verso Ollama
**Porta Ollama**: 11434

## Funzione
Cache SQLite della lista modelli Ollama con ricerca e info dettagliate.
Espone 5 tool: `list_models`, `get_model_info`, `search_models`, `sync`, `pull_model`.
Riduce le chiamate HTTP a `/api/tags` ad ogni avvio grazie alla cache locale.

## Installazione

### 1. Dipendenze Python
Nessuna pip — usa solo `urllib` e `sqlite3` della stdlib.

### 2. Installa Ollama
```bash
# Linux / macOS
curl -fsSL https://ollama.com/install.sh | sh

# Windows
# Scarica da https://ollama.com/download
```

### 3. Avvia Ollama
```bash
ollama serve
```

Per **GPU AMD lenta o CPU Xeon** (forza CPU):
```bash
# Linux/macOS
OLLAMA_NUM_GPU=0 ollama serve

# Windows PowerShell
$env:OLLAMA_NUM_GPU=0; ollama serve
```

### 4. Scarica almeno un modello
```bash
ollama pull mistral          # 4 GB — buon bilanciamento
ollama pull qwen3:4b         # 2 GB — leggero
ollama pull deepseek-r1:7b   # 4 GB — ragionamento
```

### 5. Verifica
```bash
curl http://localhost:11434/api/tags
# risposta JSON con lista modelli installati
```

## Cache locale
Il MCP salva la cache in: `~/.prismalux/ollama_cache.db`
Per forzare la sincronizzazione: usa il tool `sync` dal MCP.

## Aggiungere a Claude Code
In `~/.claude/settings.json`:
```json
{
  "mcpServers": {
    "ollama": {
      "type": "stdio",
      "command": "python3",
      "args": ["/home/wildlux/Desktop/Prismalux/MCPs/ollama_mcp/server.py"]
    }
  }
}
```
