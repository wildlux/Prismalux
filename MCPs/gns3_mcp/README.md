# GNS3 MCP

**Porta**: 3080  
**Tipo**: REST API nativa GNS3 Server

## Funzione
Permette a Prismalux di creare topologie di rete, aggiungere router/switch,
configurare link e avviare/fermare nodi GNS3 via REST API nativa.

## Prerequisiti
GNS3 Server deve essere in esecuzione:
```bash
gns3server  # porta 3080 di default
```

## API GNS3
- `GET  /v2/projects` — lista progetti
- `POST /v2/projects` — crea progetto
- `POST /v2/projects/{id}/nodes` — aggiunge nodo
- `POST /v2/projects/{id}/links` — collega nodi

## TODO
- [ ] Scrivere `gns3_mcp_server.py` — MCP stdio con requests verso localhost:3080
