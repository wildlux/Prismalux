#!/usr/bin/env python3
"""
ply_to_obj.py — converte la nuvola di punti PLY di COLMAP in OBJ + MTL
per Prismalux Vision3D (widget_vision3d.cpp, passo 3 della ricostruzione).

Uso:
  python3 ply_to_obj.py nuvola_punti.ply /percorso/modello
  → scrive /percorso/modello.obj  e  /percorso/modello.mtl

Nuvola di punti COLORATA ("scanner 3D simulato"): ogni vertice OBJ porta il
colore RGB come estensione `v x y z r g b` (0..1), letta da MeshLab,
CloudCompare e Blender. L'MTL definisce il materiale base richiamato dall'OBJ.

Zero dipendenze: parser PLY manuale (ascii e binary_little_endian, i due
formati che COLMAP produce). Le facce, se presenti, vengono ignorate:
l'input atteso è una nuvola di punti sparsa.
"""
import struct
import sys

TYPE_FMT = {          # tipo PLY → (fmt struct little-endian, dimensione)
    "char": ("b", 1), "int8": ("b", 1),
    "uchar": ("B", 1), "uint8": ("B", 1),
    "short": ("h", 2), "int16": ("h", 2),
    "ushort": ("H", 2), "uint16": ("H", 2),
    "int": ("i", 4), "int32": ("i", 4),
    "uint": ("I", 4), "uint32": ("I", 4),
    "float": ("f", 4), "float32": ("f", 4),
    "double": ("d", 8), "float64": ("d", 8),
}


def fail(msg):
    print(f"ply_to_obj: {msg}", file=sys.stderr)
    sys.exit(1)


def parse_header(fh):
    """Legge l'header PLY. Ritorna (formato, n_vertici, proprietà, offset_dati)."""
    if fh.readline().strip() != b"ply":
        fail("non è un file PLY")
    fmt = None
    n_vertices = 0
    props = []            # [(nome, tipo)] dell'elemento vertex
    in_vertex = False
    while True:
        line = fh.readline()
        if not line:
            fail("header PLY troncato")
        parts = line.decode("ascii", "replace").split()
        if not parts:
            continue
        if parts[0] == "format":
            fmt = parts[1]
        elif parts[0] == "element":
            in_vertex = (parts[1] == "vertex")
            if in_vertex:
                n_vertices = int(parts[2])
        elif parts[0] == "property" and in_vertex:
            if parts[1] == "list":
                fail("property list nell'elemento vertex: non supportata")
            props.append((parts[2], parts[1]))
        elif parts[0] == "end_header":
            return fmt, n_vertices, props


def read_vertices(fh, fmt, n, props):
    """Generatore di dict {nome_proprietà: valore} per ogni vertice."""
    names = [p[0] for p in props]
    if fmt == "ascii":
        for _ in range(n):
            vals = fh.readline().split()
            yield dict(zip(names, (float(v) for v in vals)))
    elif fmt == "binary_little_endian":
        sfmt = "<" + "".join(TYPE_FMT[t][0] for _, t in props)
        size = struct.calcsize(sfmt)
        for _ in range(n):
            raw = fh.read(size)
            if len(raw) < size:
                fail("dati PLY troncati")
            yield dict(zip(names, struct.unpack(sfmt, raw)))
    else:
        fail(f"formato PLY '{fmt}' non supportato")


def main():
    if len(sys.argv) != 3:
        fail("uso: ply_to_obj.py input.ply output_base")
    ply_path, out_base = sys.argv[1], sys.argv[2]

    with open(ply_path, "rb") as fh:
        fmt, n, props = parse_header(fh)
        if n == 0:
            fail("nessun vertice nel PLY (ricostruzione vuota?)")
        have_rgb = all(k in [p[0] for p in props] for k in ("red", "green", "blue"))

        mtl_name = out_base.rsplit("/", 1)[-1] + ".mtl"
        with open(out_base + ".obj", "w") as obj:
            obj.write("# Prismalux Vision3D — nuvola di punti colorata (fotogrammetria)\n")
            obj.write(f"# {n} punti da {ply_path.rsplit('/', 1)[-1]}\n")
            obj.write(f"mtllib {mtl_name}\nusemtl nuvola\n")
            for v in read_vertices(fh, fmt, n, props):
                if have_rgb:
                    obj.write(f"v {v['x']:.6f} {v['y']:.6f} {v['z']:.6f} "
                              f"{v['red']/255:.4f} {v['green']/255:.4f} {v['blue']/255:.4f}\n")
                else:
                    obj.write(f"v {v['x']:.6f} {v['y']:.6f} {v['z']:.6f}\n")

    with open(out_base + ".mtl", "w") as mtl:
        mtl.write("# Prismalux Vision3D — materiale base\n"
                  "newmtl nuvola\n"
                  "Ka 0.2 0.2 0.2\nKd 0.8 0.8 0.8\nKs 0.0 0.0 0.0\nd 1.0\nillum 1\n")

    print(f"OK {n} punti -> {out_base}.obj + {out_base}.mtl (colori: {have_rgb})")


if __name__ == "__main__":
    main()
