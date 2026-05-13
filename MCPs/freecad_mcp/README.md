# FreeCAD MCP

**Porta**: 9876  
**Tipo**: Addon FreeCAD (Python)

## Funzione
Permette a Prismalux di creare modelli 3D parametrici, esportare STL/STEP, 
gestire sketch e feature tree tramite comandi Python inviati a FreeCAD.

## Installazione
1. Apri FreeCAD → Tools → Addon Manager → cerca "PrismaluxBridge" oppure installa manualmente
2. Copia `freecad_bridge.py` in `~/.FreeCAD/Mod/PrismaluxBridge/`
3. Riavvia FreeCAD — il server HTTP parte su porta 9876

## Uso da Prismalux
```
APP Controller → FreeCAD → invia comandi Python
```

## TODO
- [ ] Scrivere `freecad_bridge.py` (analog a blender_addon/prismalux_bridge.py)
