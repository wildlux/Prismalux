#!/usr/bin/env python3
# =============================================================================
# ⚠️  STRUMENTO DIDATTICO — NON PRODUCE RISULTATI CLINICI
# Filtri di plausibilita' (struttura, CpG, codoni). NON sono "garanzie": sono
# indizi grezzi. Un vero design va validato con strumenti dedicati e in lab.
# =============================================================================
"""
safe_margins.py - Filtri di plausibilita' per le candidate (demo)

Nota bug corretto rispetto alla versione originale:
  il conteggio CpG considera SOLO il dinucleotide 'CG' (5'->3'). Il motivo 'GC'
  NON e' un sito CpG e non va contato.
"""

# La libreria ViennaRNA e' opzionale: se assente, il controllo termodinamico
# viene saltato con un avviso invece di far crashare lo script.
try:
    import RNA  # pip install ViennaRNA
    _HAS_VIENNA = True
except ImportError:
    _HAS_VIENNA = False


def thermodynamic_guarantee(sequence):
    """Se la guida si ripiega troppo (dG molto negativo), lega peggio."""
    if not _HAS_VIENNA:
        return True, "ViennaRNA non installato: controllo termodinamico saltato"
    _structure, delta_g = RNA.fold(sequence)
    if delta_g < -5.0:
        return False, f"Ripiegamento marcato (dG={delta_g:.1f})"
    return True, f"Struttura aperta (dG={delta_g:.1f})"


def immunogenicity_guarantee(sequence):
    """Conta i dinucleotidi CpG ('CG' 5'->3'); troppi -> possibile risposta immune."""
    cpg_count = sequence.count('CG')   # solo CG, NON GC
    if cpg_count > 3:
        return False, f"Troppi CpG ({cpg_count})"
    return True, f"CpG ok ({cpg_count})"


def codon_optimization_guarantee(rna_sequence):
    """Segnala codoni rari (lista di esempio non esaustiva)."""
    rare_codons = ['CGA', 'CGC', 'CGG', 'CGT', 'TTA']
    up = rna_sequence.upper()
    for rare in rare_codons:
        if rare in up:
            return False, f"Contiene codone raro: {rare}"
    return True, "Codoni ok (controllo indicativo)"


def apply_safe_margins(candidate_sequence, is_dna=False):
    """Applica i filtri; restituisce (approvato, motivo)."""
    seq = candidate_sequence.replace('T', 'U') if is_dna else candidate_sequence

    ok, msg = thermodynamic_guarantee(seq)
    if not ok:
        return False, f"STRUTTURA: {msg}"

    ok, msg = immunogenicity_guarantee(candidate_sequence)  # CpG si valuta sul DNA
    if not ok:
        return False, f"IMMUNITA: {msg}"

    if not is_dna:
        ok, msg = codon_optimization_guarantee(candidate_sequence)
        if not ok:
            return False, f"CODONI: {msg}"

    return True, "Filtri di plausibilita' superati (demo, non garanzia)"


if __name__ == "__main__":
    for s in ["GCGCGCGCGCGCGCGCGCGC", "ATGCATGCATGCATGCATGC", "AATTAATTAATTAATTAATT"]:
        print(s, "->", apply_safe_margins(s, is_dna=True))
