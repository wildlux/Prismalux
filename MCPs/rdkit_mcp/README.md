# RDKit MCP — Prismalux

**Tipo**: Python library (locale, nessun server esterno)

## Funzione
Chimica computazionale: disegna molecole da SMILES, calcola proprietà
(peso molecolare, LogP, TPSA), confronta strutture con fingerprint Tanimoto,
genera immagini PNG delle molecole.

## Installazione

### 1. Dipendenze Python

**Via pip (più semplice, Python 3.10+):**
```bash
pip install rdkit
```

**Via conda (consigliata per ambienti scientifici):**
```bash
conda install -c conda-forge rdkit
```

**Via apt (Linux):**
```bash
sudo apt install python3-rdkit
```

### 2. Verifica
```bash
python3 -c "
from rdkit import Chem
from rdkit.Chem import Descriptors
mol = Chem.MolFromSmiles('CCO')
print('RDKit OK — peso etanolo:', Descriptors.MolWt(mol))
"
```

### 3. Immagini molecole (opzionale)
```bash
pip install Pillow
```

## Note
- Funziona completamente offline, nessun server esterno
- Output salvato in `~/.prismalux/rdkit_output/`

## Aggiungere a Claude Code
In `~/.claude/settings.json`:
```json
{
  "mcpServers": {
    "rdkit": {
      "type": "stdio",
      "command": "python3",
      "args": ["/home/wildlux/Desktop/Prismalux/MCPs/rdkit_mcp/server.py"]
    }
  }
}
```
