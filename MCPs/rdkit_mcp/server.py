#!/usr/bin/env python3
"""
RDKit MCP — Prismalux
Chimica computazionale: disegna molecole, calcola proprietà, confronta strutture.
Prerequisiti: pip install rdkit
"""
import sys, json, base64
from pathlib import Path

OUT_DIR = Path.home() / ".prismalux" / "rdkit_output"
OUT_DIR.mkdir(parents=True, exist_ok=True)

def _rd():
    try:
        from rdkit import Chem
        from rdkit.Chem import Draw, Descriptors, AllChem, DataStructs
        return Chem, Draw, Descriptors, AllChem, DataStructs
    except ImportError:
        return None

TOOLS = [
    {"name": "smiles_to_image",
     "description": "Converte una stringa SMILES in immagine PNG della molecola.",
     "inputSchema": {"type": "object", "required": ["smiles", "filename"],
        "properties": {
            "smiles":    {"type": "string", "description": "SMILES della molecola (es. 'CCO' per etanolo)"},
            "filename":  {"type": "string"},
            "width":     {"type": "integer", "default": 400},
            "height":    {"type": "integer", "default": 300}}}},
    {"name": "calculate_properties",
     "description": "Calcola proprietà fisico-chimiche da SMILES (MW, LogP, TPSA, HBD, HBA, rotori).",
     "inputSchema": {"type": "object", "required": ["smiles"],
        "properties": {"smiles": {"type": "string"}}}},
    {"name": "tanimoto_similarity",
     "description": "Calcola similarità di Tanimoto tra due molecole (0=diverso, 1=identico).",
     "inputSchema": {"type": "object", "required": ["smiles1", "smiles2"],
        "properties": {
            "smiles1": {"type": "string"},
            "smiles2": {"type": "string"}}}},
    {"name": "substructure_search",
     "description": "Cerca un sottostruttura (SMARTS) in una lista di molecole (SMILES).",
     "inputSchema": {"type": "object", "required": ["smarts", "molecules"],
        "properties": {
            "smarts":    {"type": "string", "description": "Pattern SMARTS da cercare"},
            "molecules": {"type": "array", "items": {"type": "string"}, "description": "Lista SMILES"}}}},
    {"name": "smiles_to_3d",
     "description": "Genera le coordinate 3D di una molecola e le salva in SDF.",
     "inputSchema": {"type": "object", "required": ["smiles", "filename"],
        "properties": {
            "smiles":   {"type": "string"},
            "filename": {"type": "string"}}}},
]

def tool_smiles_to_image(args):
    r = _rd()
    if not r: return "[Errore] rdkit non installato. Esegui: pip install rdkit"
    Chem, Draw, _, _, _ = r
    mol = Chem.MolFromSmiles(args["smiles"])
    if not mol: return f"[Errore] SMILES non valida: {args['smiles']}"
    w, h = args.get("width", 400), args.get("height", 300)
    img = Draw.MolToImage(mol, size=(w, h))
    out = OUT_DIR / (args["filename"].replace(" ","_") + ".png")
    img.save(str(out))
    return f"Immagine molecola salvata: {out}"

def tool_calculate_properties(args):
    r = _rd()
    if not r: return "[Errore] rdkit non installato."
    Chem, _, Desc, AllChem, _ = r
    mol = Chem.MolFromSmiles(args["smiles"])
    if not mol: return f"[Errore] SMILES non valida: {args['smiles']}"
    return (f"Molecola: {args['smiles']}\n"
            f"  Peso Molecolare (MW):   {Desc.MolWt(mol):.2f} g/mol\n"
            f"  LogP (Wildman-Crippen):  {Desc.MolLogP(mol):.2f}\n"
            f"  TPSA:                    {Desc.TPSA(mol):.2f} Å²\n"
            f"  H-Bond Donors:           {Desc.NumHDonors(mol)}\n"
            f"  H-Bond Acceptors:        {Desc.NumHAcceptors(mol)}\n"
            f"  Rotatable Bonds:         {Desc.NumRotatableBonds(mol)}\n"
            f"  Rings aromatici:         {Desc.NumAromaticRings(mol)}\n"
            f"  Regola di Lipinski:      {'✅ OK' if Desc.MolWt(mol)<500 and Desc.MolLogP(mol)<5 and Desc.NumHDonors(mol)<=5 and Desc.NumHAcceptors(mol)<=10 else '❌ Violata'}")

def tool_tanimoto_similarity(args):
    r = _rd()
    if not r: return "[Errore] rdkit non installato."
    Chem, _, _, AllChem, DataStructs = r
    m1 = Chem.MolFromSmiles(args["smiles1"])
    m2 = Chem.MolFromSmiles(args["smiles2"])
    if not m1 or not m2: return "[Errore] Una o entrambe le SMILES non sono valide."
    fp1 = AllChem.GetMorganFingerprintAsBitVect(m1, 2)
    fp2 = AllChem.GetMorganFingerprintAsBitVect(m2, 2)
    sim = DataStructs.TanimotoSimilarity(fp1, fp2)
    label = "identiche" if sim==1 else "molto simili" if sim>0.7 else "simili" if sim>0.4 else "diverse"
    return f"Tanimoto({args['smiles1']}, {args['smiles2']}) = {sim:.4f} ({label})"

def tool_substructure_search(args):
    r = _rd()
    if not r: return "[Errore] rdkit non installato."
    Chem, _, _, _, _ = r
    patt = Chem.MolFromSmarts(args["smarts"])
    if not patt: return f"[Errore] SMARTS non valida: {args['smarts']}"
    hits = [s for s in args["molecules"] if (m := Chem.MolFromSmiles(s)) and m.HasSubstructMatch(patt)]
    if not hits: return f"Nessun match per '{args['smarts']}' tra {len(args['molecules'])} molecole."
    return (f"Pattern '{args['smarts']}': {len(hits)}/{len(args['molecules'])} match:\n"
            + "\n".join(f"  ✅ {s}" for s in hits))

def tool_smiles_to_3d(args):
    r = _rd()
    if not r: return "[Errore] rdkit non installato."
    Chem, _, _, AllChem, _ = r
    mol = Chem.MolFromSmiles(args["smiles"])
    if not mol: return f"[Errore] SMILES non valida."
    mol = Chem.AddHs(mol)
    AllChem.EmbedMolecule(mol, AllChem.ETKDG())
    AllChem.UFFOptimizeMolecule(mol)
    out = OUT_DIR / (args["filename"].replace(" ","_") + ".sdf")
    with Chem.SDWriter(str(out)) as w: w.write(mol)
    return f"Struttura 3D salvata: {out}"

HANDLERS = {"smiles_to_image": tool_smiles_to_image, "calculate_properties": tool_calculate_properties,
            "tanimoto_similarity": tool_tanimoto_similarity, "substructure_search": tool_substructure_search,
            "smiles_to_3d": tool_smiles_to_3d}

def _send(obj): sys.stdout.write(json.dumps(obj) + "\n"); sys.stdout.flush()
def _error(rid, c, m): _send({"jsonrpc":"2.0","id":rid,"error":{"code":c,"message":m}})
def _result(rid, r):   _send({"jsonrpc":"2.0","id":rid,"result":r})

def handle(req):
    m, rid, p = req.get("method",""), req.get("id"), req.get("params",{}) or {}
    if m == "initialize":
        _result(rid, {"protocolVersion":"2024-11-05","capabilities":{"tools":{}},"serverInfo":{"name":"rdkit-mcp","version":"1.0.0"}})
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
