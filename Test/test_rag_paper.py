#!/usr/bin/env python3
"""
test_rag_paper.py — Domande sul documento RAG: Johnson–Lindenstrauss Transforms.

Simula il flusso RAG:
  1. Estrae testo dal PDF (pdftotext o pypdf2 come fallback)
  2. Inietta il chunk più rilevante nel system prompt
  3. Chiede domande al modello con il contesto
  4. Verifica che le risposte contengano i termini attesi

Esegui: python3 test_rag_paper.py
"""
import os
import re
import sys
import subprocess
import time

# ── Aggiunge la cartella Test al path per trovare test_utils ──────────
sys.path.insert(0, os.path.dirname(__file__))
from test_utils import ask_ollama, bar

# ── Configurazione ────────────────────────────────────────────────────
RAG_PDF   = os.path.join(os.path.dirname(__file__), "../RAG/2103.00564v1.pdf")
MODEL     = "mistral:7b-instruct"
PARAMS    = {
    "temperature": 0.1, "top_p": 0.9, "top_k": 20,
    "repeat_penalty": 1.1, "num_predict": 256, "num_ctx": 4096,
}
HEADER = "═" * 68

# ── Test: (id, domanda, parole_chiave_attese, nota) ───────────────────
# Le parole_chiave sono cercate nell'output (OR: basta una sola)
TESTS = [
    (1,
     "Cosa è una trasformazione Johnson-Lindenstrauss (JLT)?",
     ["riduzione", "dimensionalit", "distanze", "preserv", "casuale", "proiez",
      "random", "embed"],
     "Deve descrivere la riduzione di dimensionalità con preservazione delle distanze"),

    (2,
     "In che anno fu introdotto il lemma di Johnson-Lindenstrauss?",
     ["1984", "1980"],
     "Il lemma fu introdotto nel 1984"),

    (3,
     "Cosa preserva una trasformazione JLT tra i vettori originali?",
     ["distanze", "distanza", "euclide", "pairwise", "norma", "similitud"],
     "Preserva le distanze a coppie (pairwise distances)"),

    (4,
     "Cosa si intende con 'bag-of-words' nel paper?",
     ["vettore", "parole", "word", "occorrenze", "frequen", "testo", "rappresentaz"],
     "Rappresentazione di un testo come vettore di conteggio delle parole"),

    (5,
     "La JLT è adatta per ridurre la dimensionalità di dati ad alta dimensione?",
     ["sì", "si", "yes", "corrett", "adatt", "util", "efficac", "potent", "applic"],
     "Risposta affermativa: JLT è adatta"),
]


# ── Estrazione testo PDF ──────────────────────────────────────────────
def extract_pdf_text(path: str) -> str:
    """Estrae testo dal PDF via pdftotext (o fallback pypdf)."""
    try:
        result = subprocess.run(
            ["pdftotext", "-layout", path, "-"],
            capture_output=True, text=True, timeout=15
        )
        if result.returncode == 0 and result.stdout.strip():
            return result.stdout
    except Exception:
        pass
    # Fallback: pypdf
    try:
        import importlib
        pypdf = importlib.import_module("pypdf")
        text = []
        with open(path, "rb") as f:
            reader = pypdf.PdfReader(f)
            for page in reader.pages:
                text.append(page.extract_text() or "")
        return "\n".join(text)
    except Exception:
        pass
    return ""


def build_context(full_text: str, question: str, max_chars: int = 2000) -> str:
    """
    Seleziona i paragrafi più rilevanti per la domanda (keyword match greedy).
    Ritorna al massimo max_chars caratteri di contesto.
    """
    paragraphs = [p.strip() for p in re.split(r"\n{2,}", full_text) if len(p.strip()) > 60]
    question_words = set(re.findall(r"\w+", question.lower()))
    stop = {"il", "la", "di", "che", "in", "e", "a", "è", "un", "una", "per", "con", "del",
            "the", "a", "of", "in", "and", "to", "is", "for", "an", "are", "or", "be"}
    question_words -= stop

    scored = []
    for p in paragraphs:
        pw = set(re.findall(r"\w+", p.lower()))
        score = len(question_words & pw)
        scored.append((score, p))

    scored.sort(key=lambda x: -x[0])
    ctx = []
    total = 0
    for score, p in scored:
        if total + len(p) > max_chars:
            break
        ctx.append(p)
        total += len(p)

    return "\n\n".join(ctx)


# ── Check risposta ────────────────────────────────────────────────────
def check(answer: str, keywords: list[str]) -> bool:
    """Ritorna True se almeno una keyword (case-insensitive) è nella risposta."""
    ans_lower = answer.lower()
    return any(kw.lower() in ans_lower for kw in keywords)


# ── Main ──────────────────────────────────────────────────────────────
def main():
    print(HEADER)
    print("  📄  Test RAG — Johnson–Lindenstrauss Transforms Paper")
    print(f"  Modello : {MODEL}")
    print(f"  PDF     : {os.path.basename(RAG_PDF)}")
    print(HEADER)

    # Controlla il PDF
    if not os.path.exists(RAG_PDF):
        print(f"\n❌  PDF non trovato: {RAG_PDF}")
        sys.exit(1)

    print("\n⏳  Estrazione testo dal PDF...")
    full_text = extract_pdf_text(RAG_PDF)
    if not full_text.strip():
        print("❌  Impossibile estrarre testo dal PDF (installa pdftotext o pypdf).")
        sys.exit(1)
    print(f"✅  Estratti ~{len(full_text):,} caratteri\n")

    passed = 0
    results = []

    for tid, question, keywords, note in TESTS:
        print(f"── Test {tid:02d}: {question}")
        print(f"   Atteso: {note}")

        # Costruisce contesto RAG rilevante per la domanda
        context = build_context(full_text, question)
        system = (
            "Sei un assistente che risponde a domande basandosi SOLO sul contesto fornito.\n"
            "Contesto estratto dal documento:\n\n"
            f"{context}\n\n"
            "Rispondi in italiano, in modo conciso (2-3 frasi al massimo)."
        )

        t0 = time.time()
        answer = ask_ollama(question, system, MODEL, PARAMS, timeout=120)
        elapsed = time.time() - t0

        ok = check(answer, keywords)
        passed += ok
        results.append((tid, ok, question))

        status = "✅" if ok else "❌"
        short = (answer[:120] + "…") if len(answer) > 120 else answer
        print(f"   Risposta: {short}")
        print(f"   Esito   : {status}  ({elapsed:.1f}s)\n")

    print(HEADER)
    print(f"  📊  RISULTATO: {passed}/{len(TESTS)}")
    pct = int(100 * passed / len(TESTS))
    print(f"  {bar(passed, len(TESTS))}  {pct}%")
    for tid, ok, q in results:
        icon = "✅" if ok else "❌"
        print(f"  Test {tid:02d}: {icon}  {q[:60]}")
    print(HEADER)

    sys.exit(0 if passed == len(TESTS) else 1)


if __name__ == "__main__":
    main()
