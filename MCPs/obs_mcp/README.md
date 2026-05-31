# OBS Studio MCP — Prismalux

**Tipo**: WebSocket (obs-websocket)
**Porta**: 4455

## Funzione
Controlla OBS Studio: cambia scene, avvia/ferma registrazione e streaming,
gestisce sorgenti, overlay e filtri direttamente dall'AI di Prismalux.

## Installazione

### 1. Dipendenze Python
```bash
pip install obsws-python
```

### 2. Installa OBS Studio
- **Linux**: `sudo apt install obs-studio`  oppure  `flatpak install com.obsproject.Studio`
- **Windows / macOS**: scarica da https://obsproject.com/

### 3. Abilita obs-websocket in OBS
obs-websocket è **incluso** in OBS Studio 28+.
1. Apri OBS
2. Menu **Tools → WebSocket Server Settings**
3. Spunta **Enable WebSocket server**
4. Porta: `4455`
5. Imposta una **password** (opzionale ma consigliata)
6. Clicca **OK**

### 4. Verifica connessione
```bash
python3 -c "
import obsws_python as obs
cl = obs.ReqClient(host='localhost', port=4455, password='', timeout=5)
print('OBS versione:', cl.get_version().obs_version)
"
```

### 5. Configurare la password nel MCP
Se hai impostato una password, passala come variabile d'ambiente:
```bash
export OBS_PASSWORD="la_tua_password"
python3 MCPs/obs_mcp/server.py
```

## Aggiungere a Claude Code
In `~/.claude/settings.json`:
```json
{
  "mcpServers": {
    "obs": {
      "type": "stdio",
      "command": "python3",
      "args": ["/home/wildlux/Desktop/Prismalux/MCPs/obs_mcp/server.py"],
      "env": { "OBS_PASSWORD": "la_tua_password" }
    }
  }
}
```
