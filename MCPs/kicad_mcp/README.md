# KiCAD MCP

**Porta**: 3000  
**Tipo**: Plugin KiCAD (Python scripting console)

## Funzione
Permette a Prismalux di generare schemi elettrici, PCB layout, componenti
e netlist tramite KiCAD Python API.

## Installazione
1. KiCAD ha una Python scripting console integrata
2. Copia `kicad_bridge.py` nella cartella scripting di KiCAD
3. Avvia il server: Tools → Scripting Console → `import kicad_bridge; kicad_bridge.start()`

## TODO
- [ ] Scrivere `kicad_bridge.py`
