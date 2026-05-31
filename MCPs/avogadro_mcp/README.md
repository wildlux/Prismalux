# Avogadro MCP — Prismalux

**Tipo**: Python library (locale, nessun server esterno)

## Funzione
Carica molecole (SDF, XYZ, PDB, CML), converte SMILES in coordinate 3D,
calcola proprietà molecolari e genera immagini via Avogadro2.

## Installazione

### 1. Dipendenze Python
```bash
pip install avogadro
```
Su alcune piattaforme:
```bash
pip install avogadro2
# oppure
conda install -c conda-forge avogadro2
```

### 2. Software opzionale (visualizzazione 3D interattiva)
- **Linux**: `sudo apt install avogadro`
- **Windows / macOS**: scarica da https://www.openchemistry.org/downloads/

### 3. Verifica
```bash
python3 -c "import avogadro; print('Avogadro OK', avogadro.__version__)"
```

## Aggiungere a Claude Code
In `~/.claude/settings.json`:
```json
{
  "mcpServers": {
    "avogadro": {
      "type": "stdio",
      "command": "python3",
      "args": ["/home/wildlux/Desktop/Prismalux/MCPs/avogadro_mcp/server.py"]
    }
  }
}
```
