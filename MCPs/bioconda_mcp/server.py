#!/usr/bin/env python3
"""
Bioconda MCP — Prismalux
Tool bioinformatici via conda/mamba: BLAST, FastQC, MUSCLE, BWA, STAR, Trimmomatic.
Prerequisiti: Miniconda + canali bioconda e conda-forge configurati.
"""
import sys, json, subprocess, shutil, os
from pathlib import Path

OUT_DIR = Path.home() / ".prismalux" / "bioconda_output"
OUT_DIR.mkdir(parents=True, exist_ok=True)

def _conda():
    return shutil.which("mamba") or shutil.which("conda")

def _run(cmd, cwd=None, timeout=300):
    try:
        r = subprocess.run(cmd, shell=isinstance(cmd, str), capture_output=True,
                           text=True, timeout=timeout, cwd=cwd)
        return r.stdout.strip(), r.stderr.strip(), r.returncode
    except subprocess.TimeoutExpired:
        return "", "Timeout (300s)", 1
    except Exception as e:
        return "", str(e), 1

TOOLS = [
    {"name": "install_tool",
     "description": "Installa un tool bioinformatico via conda/mamba (es. blast, fastqc, bwa).",
     "inputSchema": {"type": "object", "required": ["tool"],
        "properties": {
            "tool": {"type": "string", "description": "Nome pacchetto bioconda (es. 'blast', 'fastqc', 'bwa', 'samtools', 'star')"},
            "env":  {"type": "string", "description": "Ambiente conda (default: base)"}}}},
    {"name": "list_tools",
     "description": "Lista i tool bioinformatici installati nell'ambiente conda.",
     "inputSchema": {"type": "object", "properties": {
        "env": {"type": "string", "description": "Ambiente conda (default: base)"}}}},
    {"name": "run_blast",
     "description": "Esegue BLAST locale su una sequenza query contro un database.",
     "inputSchema": {"type": "object", "required": ["query_file", "db"],
        "properties": {
            "query_file": {"type": "string", "description": "File FASTA con la query"},
            "db":         {"type": "string", "description": "Percorso database BLAST o 'nt'/'nr'"},
            "program":    {"type": "string", "enum": ["blastn","blastp","blastx","tblastn","tblastx"], "default": "blastn"},
            "evalue":     {"type": "number", "default": 0.001},
            "max_hits":   {"type": "integer", "default": 10},
            "output":     {"type": "string", "description": "File output (default: auto)"}}}},
    {"name": "run_fastqc",
     "description": "Esegue FastQC per il controllo qualità di file FASTQ.",
     "inputSchema": {"type": "object", "required": ["input_files"],
        "properties": {
            "input_files": {"type": "array", "items": {"type": "string"}},
            "output_dir":  {"type": "string", "description": "Directory output (default: ~/.prismalux/bioconda_output)"}}}},
    {"name": "run_command",
     "description": "Esegue un comando bioinformatico arbitrario nell'ambiente conda.",
     "inputSchema": {"type": "object", "required": ["command"],
        "properties": {
            "command": {"type": "string", "description": "Comando shell da eseguire (es. 'samtools view -h file.bam | head')"},
            "timeout": {"type": "integer", "default": 120}}}},
    {"name": "list_environments",
     "description": "Elenca tutti gli ambienti conda disponibili.",
     "inputSchema": {"type": "object", "properties": {}}},
]

def tool_install_tool(args):
    conda = _conda()
    if not conda: return "[Errore] conda/mamba non trovato. Installa Miniconda: https://docs.conda.io/projects/miniconda/"
    cmd = [conda, "install", "-y", "-c", "bioconda", "-c", "conda-forge", args["tool"]]
    if args.get("env"): cmd.extend(["-n", args["env"]])
    stdout, stderr, rc = _run(cmd, timeout=300)
    if rc != 0: return f"[Errore installazione {args['tool']}]\n{stderr[-1000:]}"
    return f"✅ {args['tool']} installato con successo.\n{stdout[-500:]}"

def tool_list_tools(args):
    conda = _conda()
    if not conda: return "[Errore] conda/mamba non trovato."
    cmd = [conda, "list"]
    if args.get("env"): cmd.extend(["-n", args["env"]])
    stdout, stderr, rc = _run(cmd)
    if rc != 0: return f"[Errore] {stderr}"
    bio_tools = ["blast","bwa","star","samtools","fastqc","muscle","trimmomatic","bowtie",
                 "hisat2","kallisto","salmon","bedtools","picard","gatk","snakemake","nextflow"]
    lines = ["Tool bioinformatici installati:"]
    for line in stdout.splitlines():
        name = line.split()[0] if line.split() else ""
        if any(t in name.lower() for t in bio_tools):
            lines.append(f"  ✅ {line}")
    if len(lines) == 1: lines.append("  (nessun tool bioinformatico rilevato)")
    return "\n".join(lines)

def tool_run_blast(args):
    if not shutil.which("blastn"): return "[Errore] BLAST non installato. Usa install_tool con 'blast'."
    prog = args.get("program", "blastn")
    out = args.get("output", str(OUT_DIR / "blast_results.txt"))
    cmd = [prog, "-query", args["query_file"], "-db", args["db"],
           "-evalue", str(args.get("evalue", 0.001)),
           "-num_alignments", str(args.get("max_hits", 10)),
           "-out", out, "-outfmt", "6 qseqid sseqid pident length evalue bitscore stitle"]
    stdout, stderr, rc = _run(cmd, timeout=300)
    if rc != 0: return f"[Errore BLAST] {stderr}"
    lines = open(out).readlines() if Path(out).exists() else []
    summary = f"BLAST completato. {len(lines)} hit trovati.\nOutput: {out}"
    if lines:
        summary += "\n\nPrimi hit:\n"
        for l in lines[:5]:
            parts = l.strip().split("\t")
            if len(parts) >= 6:
                summary += f"  {parts[1]} | id={parts[2]}% | e={parts[4]}\n"
    return summary

def tool_run_fastqc(args):
    if not shutil.which("fastqc"): return "[Errore] FastQC non installato. Usa install_tool con 'fastqc'."
    out_dir = args.get("output_dir", str(OUT_DIR))
    cmd = ["fastqc", "-o", out_dir] + args["input_files"]
    stdout, stderr, rc = _run(cmd, timeout=300)
    if rc != 0: return f"[Errore FastQC] {stderr}"
    return f"FastQC completato.\nReport in: {out_dir}\n{stdout[-500:]}"

def tool_run_command(args):
    stdout, stderr, rc = _run(args["command"], timeout=args.get("timeout", 120))
    result = stdout if stdout else stderr
    if rc != 0: return f"[Errore (rc={rc})]\n{result[-2000:]}"
    return result[-2000:] if result else "(nessun output)"

def tool_list_environments(_):
    conda = _conda()
    if not conda: return "[Errore] conda/mamba non trovato."
    stdout, stderr, rc = _run([conda, "env", "list"])
    if rc != 0: return f"[Errore] {stderr}"
    return "Ambienti conda:\n" + stdout

HANDLERS = {"install_tool": tool_install_tool, "list_tools": tool_list_tools,
            "run_blast": tool_run_blast, "run_fastqc": tool_run_fastqc,
            "run_command": tool_run_command, "list_environments": tool_list_environments}

def _send(obj): sys.stdout.write(json.dumps(obj) + "\n"); sys.stdout.flush()
def _error(rid, c, m): _send({"jsonrpc":"2.0","id":rid,"error":{"code":c,"message":m}})
def _result(rid, r):   _send({"jsonrpc":"2.0","id":rid,"result":r})

def handle(req):
    m, rid, p = req.get("method",""), req.get("id"), req.get("params",{}) or {}
    if m == "initialize":
        _result(rid, {"protocolVersion":"2024-11-05","capabilities":{"tools":{}},"serverInfo":{"name":"bioconda-mcp","version":"1.0.0"}})
    elif m == "tools/list": _result(rid, {"tools": TOOLS})
    elif m == "tools/call":
        name, args = p.get("name",""), p.get("arguments",{}) or {}
        h = HANDLERS.get(name)
        if not h: _error(rid, -32601, f"Strumento '{name}' non trovato."); return
        try: text = h(args)
        except Exception as e: text = f"[Errore] {e}"
        _result(rid, {"content":[{"type":"text","text":text}],"isError":text.startswith("[Errore")})
    elif rid is not None: _result(rid, {})

def main():
    for line in sys.stdin:
        line = line.strip()
        if not line: continue
        try: req = json.loads(line)
        except json.JSONDecodeError as e:
            _send({"jsonrpc":"2.0","id":None,"error":{"code":-32700,"message":str(e)}})
            continue
        try: handle(req)
        except Exception as e:
            if req.get("id"): _error(req["id"], -32603, str(e))

if __name__ == "__main__": main()
