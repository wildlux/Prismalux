# Cytoscape MCP

**Porta**: 1234  
**Tipo**: CyREST API nativa Cytoscape

## Funzione
Permette a Prismalux di creare reti biologiche/molecolari, importare dati,
applicare layout, esportare immagini tramite Cytoscape desktop.

## Prerequisiti
Cytoscape deve essere in esecuzione con CyREST abilitato (default porta 1234).

## API CyREST
- `GET  /v1/networks` — lista reti
- `POST /v1/networks` — crea rete
- `POST /v1/networks/{id}/nodes` — aggiunge nodi
- `GET  /v1/apply/layouts/{algorithm}/{id}` — applica layout

## TODO
- [ ] Scrivere `cytoscape_mcp_server.py` — MCP stdio con requests verso localhost:1234
