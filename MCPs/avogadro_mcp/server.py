#!/usr/bin/env python3
"""
Avogadro MCP — Prismalux
Visualizzazione e analisi molecole 3D via avogadro2 Python bindings.
Prerequisiti: pip install avogadro
"""
import sys, json
from pathlib import Path

OUT_DIR = Path.home() / ".prismalux" / "avogadro_output"
OUT_DIR.mkdir(parents=True, exist_ok=True)

def _avo():
    try:
        import avogadro
        return avogadro
    except ImportError:
        return None

TOOLS = [
    {"name": "load_molecule",
     "description": "Carica una molecola da file (SDF, XYZ, CML, PDB) e mostra informazioni.",
     "inputSchema": {"type": "object", "required": ["path"],
        "properties": {"path": {"type": "string", "description": "Percorso file molecola"}}}},
    {"name": "smiles_to_avogadro",
     "description": "Converte SMILES in molecola Avogadro (genera coordinate 3D) e la salva.",
     "inputSchema": {"type": "object", "required": ["smiles", "filename"],
        "properties": {
            "smiles":   {"type": "string"},
            "filename": {"type": "string"},
            "format":   {"type": "string", "enum": ["sdf","xyz","cml"], "default": "sdf"}}}},
    {"name": "optimize_geometry",
     "description": "Ottimizza la geometria molecolare con campo di forze (MMFF94 o UFF).",
     "inputSchema": {"type": "object", "required": ["path"],
        "properties": {
            "path":       {"type": "string"},
            "force_field": {"type": "string", "enum": ["mmff94","uff"], "default": "mmff94"},
            "steps":      {"type": "integer", "default": 500}}}},
    {"name": "calculate_energy",
     "description": "Calcola l'energia molecolare con il campo di forze specificato.",
     "inputSchema": {"type": "object", "required": ["path"],
        "properties": {
            "path":       {"type": "string"},
            "force_field": {"type": "string", "enum": ["mmff94","uff"], "default": "mmff94"}}}},
    {"name": "export_molecule",
     "description": "Converte un file molecola in un altro formato.",
     "inputSchema": {"type": "object", "required": ["input_path", "output_path"],
        "properties": {
            "input_path":  {"type": "string"},
            "output_path": {"type": "string", "description": "Estensione determina il formato: .sdf .xyz .cml .pdb"}}}},
]

def _check():
    if not _avo():
        return "[Errore] avogadro non installato. Esegui: pip install avogadro"
    return None

def tool_load_molecule(args):
    err = _check()
    if err: return err
    avo = _avo()
    try:
        mol = avo.io.read(args["path"])
        return (f"Molecola caricata: {args['path']}\n"
                f"  Atomi:  {mol.atomCount()}\n"
                f"  Legami: {mol.bondCount()}\n"
                f"  Formula: {mol.formula()}")
    except Exception as e:
        return f"[Errore] {e}"

def tool_smiles_to_avogadro(args):
    err = _check()
    if err: return err
    avo = _avo()
    try:
        mol = avo.Molecule()
        builder = avo.io.FileFormat.fileFormats().formatForMimeType("chemical/x-smiles")
        avo.io.read(mol, args["smiles"], "smi")
        avo.calc.generateCoordinates(mol)
        fmt = args.get("format", "sdf")
        out = OUT_DIR / (args["filename"].replace(" ","_") + "." + fmt)
        avo.io.write(mol, str(out))
        return f"Molecola salvata: {out}\n  Atomi: {mol.atomCount()}, Legami: {mol.bondCount()}"
    except Exception as e:
        return f"[Errore] {e}\nSuggerimento: usa rdkit_mcp per la conversione SMILES→3D."

def tool_optimize_geometry(args):
    err = _check()
    if err: return err
    avo = _avo()
    try:
        mol = avo.io.read(args["path"])
        ff = args.get("force_field", "mmff94")
        steps = args.get("steps", 500)
        calc = avo.calc.EnergyCalculator.create(ff)
        calc.setup(mol)
        opt = avo.calc.ConjugateGradients()
        opt.setup(mol, calc, steps)
        converged = opt.maximize()
        out = Path(args["path"]).with_suffix(".opt.sdf")
        avo.io.write(mol, str(out))
        return (f"Ottimizzazione completata ({ff}, {steps} passi).\n"
                f"  Convergenza: {'✅' if converged else '⚠ non convergita'}\n"
                f"  File ottimizzato: {out}")
    except Exception as e:
        return f"[Errore ottimizzazione] {e}"

def tool_calculate_energy(args):
    err = _check()
    if err: return err
    avo = _avo()
    try:
        mol = avo.io.read(args["path"])
        ff = args.get("force_field", "mmff94")
        calc = avo.calc.EnergyCalculator.create(ff)
        calc.setup(mol)
        energy = calc.value()
        return f"Energia ({ff}): {energy:.6f} kcal/mol\nMolecola: {args['path']}"
    except Exception as e:
        return f"[Errore] {e}"

def tool_export_molecule(args):
    err = _check()
    if err: return err
    avo = _avo()
    try:
        mol = avo.io.read(args["input_path"])
        avo.io.write(mol, args["output_path"])
        return f"Convertito: {args['input_path']} → {args['output_path']}"
    except Exception as e:
        return f"[Errore] {e}"

HANDLERS = {"load_molecule": tool_load_molecule, "smiles_to_avogadro": tool_smiles_to_avogadro,
            "optimize_geometry": tool_optimize_geometry, "calculate_energy": tool_calculate_energy,
            "export_molecule": tool_export_molecule}

def _send(obj): sys.stdout.write(json.dumps(obj) + "\n"); sys.stdout.flush()
def _error(rid, c, m): _send({"jsonrpc":"2.0","id":rid,"error":{"code":c,"message":m}})
def _result(rid, r):   _send({"jsonrpc":"2.0","id":rid,"result":r})

def handle(req):
    m, rid, p = req.get("method",""), req.get("id"), req.get("params",{}) or {}
    if m == "initialize":
        _result(rid, {"protocolVersion":"2024-11-05","capabilities":{"tools":{}},"serverInfo":{"name":"avogadro-mcp","version":"1.0.0"}})
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
