# OBS Studio MCP

**Porta**: 4455  
**Tipo**: WebSocket (obs-websocket plugin)

## Funzione
Permette a Prismalux di controllare OBS: cambia scene, avvia/ferma registrazione,
gestisce sorgenti, overlay e streaming.

## Installazione
1. OBS Studio → Tools → obs-websocket Settings
2. Abilita il server WebSocket sulla porta 4455
3. Imposta password (opzionale)

## Dipendenze Python
```bash
pip install obs-websocket-py
```

## TODO
- [ ] Scrivere `obs_mcp_server.py` — MCP stdio che usa obs-websocket-py
