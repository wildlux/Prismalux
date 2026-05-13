# Godot MCP

**Porta**: 9080  
**Tipo**: Plugin Godot (GDScript HTTP server)

## Funzione
Permette a Prismalux di creare scene Godot, aggiungere nodi, scrivere GDScript,
eseguire scene, esportare progetti tramite Godot Engine.

## Installazione
1. Copia `godot_bridge.gd` nel progetto Godot come AutoLoad
2. Il server HTTP parte su porta 9080 all'avvio del progetto
3. Oppure usa Godot con `--headless` + `--script godot_bridge.gd`

## TODO
- [ ] Scrivere `godot_bridge.gd` (GDScript, HTTPServer)
- [ ] Wrapper Python per il tab APP Controller
