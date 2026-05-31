# Godot MCP — Prismalux

**Tipo**: HTTP Bridge (porta 9080, avviato da Godot)

## Funzione
Crea scene Godot, aggiunge nodi, esegue GDScript, modifica proprietà
direttamente da Prismalux via AI.

## Installazione

### 1. Dipendenze Python
Nessuna pip — usa `urllib` della stdlib.

### 2. Installa Godot
- **Linux**: `sudo apt install godot3`  oppure scarica Godot 4 da https://godotengine.org/download
- **Windows / macOS**: scarica da https://godotengine.org/download

### 3. Configura il bridge GDScript
1. Apri il tuo **progetto Godot**
2. Copia `godot_bridge.gd` in `res://godot_bridge.gd`
3. **Project → Project Settings → AutoLoad**
4. Aggiungi `res://godot_bridge.gd` con nome `PrismaluxBridge`
5. Avvia il progetto — server HTTP su `http://localhost:9080`

Verifica:
```bash
curl http://localhost:9080/health
```

## Note
- Il progetto Godot deve essere **in esecuzione** (Play Scene)
- Compatibile con Godot 3.x e Godot 4.x

## Aggiungere a Claude Code
In `~/.claude/settings.json`:
```json
{
  "mcpServers": {
    "godot": {
      "type": "stdio",
      "command": "python3",
      "args": ["/home/wildlux/Desktop/Prismalux/MCPs/godot_mcp/server.py"]
    }
  }
}
```
