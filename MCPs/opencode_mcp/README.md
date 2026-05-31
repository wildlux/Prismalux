# OpenCode MCP — Prismalux

**Tipo**: subprocess (lancia OpenCode come processo)
**Porta OpenCode**: 8092

## Funzione
Delega task di programmazione a OpenCode (AI coding agent) con qualsiasi modello
Ollama. Prismalux manda il task, OpenCode lo esegue nel progetto, il risultato
torna all'AI di Prismalux.

## Installazione

### 1. Dipendenze Python
Nessuna pip — usa `subprocess` e `urllib` della stdlib.

### 2. Installa OpenCode

```bash
# Linux / macOS (npm)
npm install -g @opencode-ai/opencode

# oppure via script ufficiale
curl -fsSL https://opencode.ai/install | bash
```

- **Windows**: scarica l'installer da https://opencode.ai oppure usa:
  ```powershell
  winget install opencode
  ```

### 3. Installa Ollama (backend AI locale)
```bash
# Linux / macOS
curl -fsSL https://ollama.com/install.sh | sh
ollama pull qwen3:4b    # modello leggero consigliato per OpenCode
```

### 4. Configura OpenCode
```bash
opencode config set provider ollama
opencode config set model qwen3:4b
```

### 5. Verifica
```bash
opencode --version
ollama list   # deve mostrare il modello configurato
```

### 6. Porta e variabili d'ambiente
OpenCode espone un server SSE sulla porta `8092`.
Puoi cambiare modello e cartella default via env:
```bash
export OPENCODE_DEFAULT_MODEL="ollama/qwen3:4b"
export OPENCODE_DEFAULT_DIR="/home/wildlux/Desktop/Prismalux"
```

## Aggiungere a Claude Code
In `~/.claude/settings.json`:
```json
{
  "mcpServers": {
    "opencode": {
      "type": "stdio",
      "command": "python3",
      "args": ["/home/wildlux/Desktop/Prismalux/MCPs/opencode_mcp/server.py"],
      "env": {
        "OPENCODE_DEFAULT_MODEL": "ollama/qwen3:4b",
        "OPENCODE_DEFAULT_DIR": "/home/wildlux/Desktop/Prismalux"
      }
    }
  }
}
```
