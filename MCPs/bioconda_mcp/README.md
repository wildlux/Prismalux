# Bioconda MCP — Prismalux

**Tipo**: subprocess (conda/mamba locale)

## Funzione
Installa ed esegue tool bioinformatici (BLAST, FastQC, BWA, STAR, Trimmomatic,
MUSCLE, Samtools) tramite Miniconda con i canali bioconda e conda-forge.

## Installazione

### 1. Dipendenze Python
Nessuna pip — usa subprocess verso `conda` o `mamba`.

### 2. Installa Miniconda
```bash
# Linux / macOS
wget https://repo.anaconda.com/miniconda/Miniconda3-latest-Linux-x86_64.sh
bash Miniconda3-latest-Linux-x86_64.sh
```
- **Windows**: scarica `Miniconda3-latest-Windows-x86_64.exe` da https://docs.conda.io/en/latest/miniconda.html

### 3. Configura i canali bioconda
```bash
conda config --add channels defaults
conda config --add channels bioconda
conda config --add channels conda-forge
conda config --set channel_priority strict
```

### 4. Installa Mamba (consigliato, più veloce)
```bash
conda install -n base -c conda-forge mamba
```

### 5. Verifica
```bash
mamba search blast
conda info
```

### 6. Tool disponibili (installati on-demand dal MCP)
| Tool | Comando |
|------|---------|
| BLAST | `mamba install blast` |
| FastQC | `mamba install fastqc` |
| BWA | `mamba install bwa` |
| STAR | `mamba install star` |
| Trimmomatic | `mamba install trimmomatic` |
| Samtools | `mamba install samtools` |
| MUSCLE | `mamba install muscle` |

## Aggiungere a Claude Code
In `~/.claude/settings.json`:
```json
{
  "mcpServers": {
    "bioconda": {
      "type": "stdio",
      "command": "python3",
      "args": ["/home/wildlux/Desktop/Prismalux/MCPs/bioconda_mcp/server.py"]
    }
  }
}
```
