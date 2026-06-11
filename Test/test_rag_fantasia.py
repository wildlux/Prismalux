#!/usr/bin/env python3
"""
test_rag_fantasia.py — Domande RAG sul racconto "La città sospesa di Aethera".

Flusso:
  1. Legge RAG/fantasia.txt
  2. Seleziona il chunk più rilevante per ogni domanda
  3. Chiede al modello con contesto RAG
  4. Verifica la presenza di parole-chiave tratte dalle risposte attese

Esegui: python3 Test/test_rag_fantasia.py
"""
import os
import re
import sys
import time

sys.path.insert(0, os.path.dirname(__file__))
from test_utils import ask_ollama, bar

RAG_TXT = os.path.join(os.path.dirname(__file__), "../RAG/fantasia.txt")
MODEL   = "mistral:7b-instruct"
PARAMS  = {
    "temperature": 0.1, "top_p": 0.9, "top_k": 20,
    "repeat_penalty": 1.1, "num_predict": 300, "num_ctx": 2048,
}
HEADER = "═" * 70

# (id, domanda, parole_chiave_attese, nota)
TESTS = [
    (1,
     "Qual è il problema principale che affligge la città di Aethera?",
     ["affond", "abbassan", "mare", "onda", "inghiott", "sink"],
     "La città si abbassa verso il mare e rischia di essere inghiottita"),

    (2,
     "Cosa causa la perdita di energia dei cristalli antigravità?",
     ["faro", "memoria", "luce", "abbandon", "accend", "tower", "stellar"],
     "I cristalli hanno perso la memoria di luce perché il faro stellare è abbandonato"),

    (3,
     "Dove si trova il meccanismo in grado di salvare la città?",
     ["torre", "silente", "nubi", "nuvol", "tower"],
     "Nella Torre Silente tra le nubi"),

    (4,
     "Quale forma ha il meccanismo all'interno della Torre Silente?",
     ["fiore", "vetro", "flower", "glass"],
     "Ha la forma di un antico fiore di vetro"),

    (5,
     "Quali sono le professioni dei due protagonisti, Elowen e Kael?",
     ["inventor", "cartograf", "inventor", "mapmaker", "map"],
     "Elowen è inventore, Kael è cartografa"),

    (6,
     "Quale ricordo personale Kael decide di condividere con il fiore di vetro?",
     ["padre", "volo", "father", "flight", "nuvol", "defunt", "primo"],
     "Il ricordo del primo volo del padre defunto tra le nuvole"),

    (7,
     "Secondo il racconto, cosa dà vera energia ai cristalli?",
     ["storie", "ricordi", "umane", "immaginaz", "stories", "memories", "luce"],
     "La luce delle storie umane e dei ricordi"),

    (8,
     "Qual è il significato dell'affermazione: 'Aethera non è più solo una città sospesa: è un ricordo vivente'?",
     ["ricordo", "immaginaz", "memorie", "vivente", "abitanti", "storie", "emozion"],
     "Tenuta in aria dall'immaginazione e ricordi degli abitanti"),

    (9,
     "In che modo la storia collega memoria e sopravvivenza?",
     ["ricordi", "cristalli", "sopravviv", "faro", "affond", "tramand", "storie"],
     "Senza memoria il faro non si accende e la città affonda"),

    (10,
     "Cosa accadrebbe se un anno tutti gli abitanti dimenticassero di raccontare una storia al fiore?",
     ["affond", "cristall", "faro", "abbassan", "non si accend", "sink"],
     "Il faro non si attiverebbe e la città ricomincerebbe ad affondare"),
]


def build_context(text: str, question: str, max_chars: int = 1500) -> str:
    paragraphs = [p.strip() for p in re.split(r"\n{2,}", text) if len(p.strip()) > 30]
    qwords = set(re.findall(r"\w+", question.lower()))
    stop = {"il","la","di","che","in","e","a","è","un","una","per","con","del",
            "dei","lo","gli","le","da","su","al","alla","dei","degli"}
    qwords -= stop

    scored = []
    for p in paragraphs:
        pw = set(re.findall(r"\w+", p.lower()))
        score = len(qwords & pw)
        scored.append((score, p))
    scored.sort(key=lambda x: -x[0])

    ctx, total = [], 0
    for _, p in scored:
        if total + len(p) > max_chars:
            break
        ctx.append(p)
        total += len(p)
    return "\n\n".join(ctx) if ctx else text[:max_chars]


def check(answer: str, keywords: list) -> bool:
    al = answer.lower()
    return any(kw.lower() in al for kw in keywords)


def main():
    print(HEADER)
    print("  📖  Test RAG — La città sospesa di Aethera")
    print(f"  Modello : {MODEL}")
    print(f"  File    : RAG/fantasia.txt")
    print(HEADER)

    if not os.path.exists(RAG_TXT):
        print(f"\n❌  File non trovato: {RAG_TXT}")
        sys.exit(1)

    with open(RAG_TXT, encoding="utf-8") as f:
        full_text = f.read()
    print(f"\n✅  Testo caricato ({len(full_text)} caratteri)\n")

    # Verifica Ollama disponibile
    try:
        import urllib.request
        urllib.request.urlopen("http://127.0.0.1:11434/api/tags", timeout=3)
    except Exception:
        print("⚠️  Ollama non raggiungibile — test saltato (richiede Ollama attivo).")
        sys.exit(0)

    passed, results = 0, []
    for tid, question, keywords, note in TESTS:
        print(f"── Test {tid:02d}: {question}")
        print(f"   Atteso: {note}")

        context = build_context(full_text, question)
        system = (
            "Sei un assistente che risponde a domande basandosi SOLO sul contesto fornito.\n"
            "Contesto:\n\n"
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
        short = (answer[:130] + "…") if len(answer) > 130 else answer
        print(f"   Risposta: {short}")
        print(f"   Esito   : {status}  ({elapsed:.1f}s)\n")

    print(HEADER)
    print(f"  📊  RISULTATO: {passed}/{len(TESTS)}")
    pct = int(100 * passed / len(TESTS))
    print(f"  {bar(passed, len(TESTS))}  {pct}%")
    for tid, ok, q in results:
        icon = "✅" if ok else "❌"
        print(f"  Test {tid:02d}: {icon}  {q[:65]}")
    print(HEADER)
    sys.exit(0 if passed == len(TESTS) else 1)


if __name__ == "__main__":
    main()
