# Bioconda MCP

**Tipo**: MCP Python locale (subprocess conda/mamba)

## Funzione
Permette a Prismalux di installare tool bioinformatici (BLAST, MUSCLE, FastQC,
Trimmomatic, STAR, BWA), eseguire pipeline NGS, gestire ambienti conda.

## Prerequisiti
```bash
# Installa Miniconda/Mambaforge
conda config --add channels bioconda
conda config --add channels conda-forge
```

## TODO
- [ ] Scrivere `bioconda_mcp_server.py` — MCP stdio
- [ ] Strumenti: install_tool, run_blast, run_fastqc, list_environments
