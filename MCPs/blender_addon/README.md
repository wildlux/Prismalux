# Blender Addon — Prismalux Bridge

**Tipo**: HTTP Bridge (porta 6789, avviato da Blender)

## Funzione
Permette a Prismalux di eseguire codice Python direttamente dentro Blender:
crea oggetti 3D, modifica mesh, applica materiali, renderizza — tutto via AI.

## Installazione

### 1. Dipendenze Python
Nessuna pip — usa la Python API interna di Blender (`bpy`).

### 2. Installa Blender
- **Linux**: `sudo apt install blender`  oppure  `flatpak install blender`
- **Windows**: scarica da https://www.blender.org/download/
- **macOS**: `brew install --cask blender`

### 3. Installa l'addon in Blender
1. Apri Blender
2. Menu **Edit → Preferences → Add-ons**
3. Clicca **Install...**
4. Seleziona il file:
   ```
   Prismalux/MCPs/blender_addon/prismalux_bridge.py
   ```
5. Spunta la casella per abilitare l'addon **"Prismalux Bridge"**
6. Il server HTTP parte automaticamente su `http://localhost:6789`

### 4. Verifica che il bridge sia attivo
```bash
curl http://localhost:6789/health
# risposta: {"status": "ok"}
```

### 5. Avvio automatico
Per avviare il bridge ogni volta che apri Blender:
- Salva le preferenze con **Edit → Preferences → Save Preferences**

## Note
- Blender deve essere **aperto** affinché il MCP funzioni
- Il codice Python viene eseguito nel **thread principale** di Blender (sicuro per bpy)
- Output del codice disponibile nella **Blender Console** (Scripting workspace)

## Aggiungere a Claude Code
In `~/.claude/settings.json`:
```json
{
  "mcpServers": {
    "blender": {
      "type": "stdio",
      "command": "python3",
      "args": ["/home/wildlux/Desktop/Prismalux/MCPs/blender_addon/prismalux_bridge.py"]
    }
  }
}
```
