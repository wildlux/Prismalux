#!/usr/bin/env python3
"""Benchmark qualitativo dei due modelli ONNX inclusi nel Decision Model.

Il benchmark è un insieme curato di prompt *held-out* (non presenti nei
dataset di training del generatore): serve a misurare la generalizzazione,
non l'accuratezza sul training set.

    decision_classifier.onnx : testo -> A/B/C/D (coding/reasoning/chat/other)
    decision_verifier.onnx   : domanda+risposta -> N/Y (bocciata/approvata)

Uso:
    python3 gui/tools/eval_decision_onnx.py
    python3 gui/tools/eval_decision_onnx.py --json /tmp/onnx-quality.json
    python3 gui/tools/eval_decision_onnx.py --min-classifier 0.70 --min-verifier 0.95

Il verificatore è un modello lessicale: misura rifiuti, risposte vuote,
bassa pertinenza e risposte generiche. Non è un fact-checker e non può
sapere se "Berlino" è la risposta sbagliata alla domanda sulla Francia.
Il benchmark stampa due righe: modello grezzo e modello + guardie
deterministiche (il comportamento effettivo del backend), su un set dev
e su un set blind. I casi di fatto sbagliato sono elencati in
`OUT_OF_SCOPE` e non entrano nelle metriche.

Dipendenze: numpy, onnxruntime. Il file `gen_decision_onnx.py` nella stessa
cartella viene importato per usare le STESSE feature del runtime C++.
"""

from __future__ import annotations

import argparse
import importlib.util
import json
import os
import sys
from pathlib import Path

import numpy as np

try:
    import onnxruntime as ort
except ImportError:
    sys.exit("Serve onnxruntime (pip install onnxruntime)")

ROOT = Path(__file__).resolve().parents[2]
GEN_PATH = Path(__file__).with_name("gen_decision_onnx.py")
MODELS = ROOT / "Frameworks" / "onnx" / "models"

spec = importlib.util.spec_from_file_location("gen_decision_onnx", GEN_PATH)
gen = importlib.util.module_from_spec(spec)
spec.loader.exec_module(gen)

# ── Held-out classifier set ────────────────────────────────────────────
# 12 esempi per classe, italiano e inglese, parafrasi non usate nel
# training. Le etichette seguono la tassonomia del progetto:
#   A coding · B reasoning · C chat · D other
CLS = [
    ("A", "sistema questo bug nel codice C++"),
    ("A", "come posso ottimizzare una funzione ricorsiva in Java"),
    ("A", "scrivi uno script bash per rinominare i file"),
    ("A", "perché la mia query SQL restituisce duplicati"),
    ("A", "implementa una API GraphQL con Node.js"),
    ("A", "differenza tra thread e processo in C"),
    ("A", "come configuro una pipeline CI su GitHub Actions"),
    ("A", "il mio programma Python va in crash quando leggo un CSV"),
    ("A", "crea un componente React con TypeScript"),
    ("A", "explain the difference between a stack and a queue"),
    ("A", "how do I fix a null pointer exception in Java"),
    ("A", "write a Dockerfile for a Django application"),
    ("B", "calcola il limite di (1+1/n)^n per n che tende a infinito"),
    ("B", "dimostra che la somma degli angoli di un triangolo è 180 gradi"),
    ("B", "quale approccio conviene tra A e B e perché"),
    ("B", "risolvi il problema dei tre secchielli"),
    ("B", "spiega perché l'acqua bolle a 100 gradi a livello del mare"),
    ("B", "analizza il grafico e dimmi se la correlazione è significativa"),
    ("B", "se piove allora la strada è bagnata, la strada è bagnata, possiamo concludere che piove"),
    ("B", "quanto tempo impiega un'auto a percorrere 150 km a 75 km/h"),
    ("B", "derive the equation of motion for a pendulum"),
    ("B", "what is the probability of drawing two aces from a deck"),
    ("B", "explain the difference between correlation and causation"),
    ("B", "find the mistake in this logical argument"),
    ("C", "ehi, come va oggi"),
    ("C", "mi racconti qualcosa di divertente"),
    ("C", "cosa mi consigli per una serata tranquilla"),
    ("C", "scrivimi un messaggio per congratularmi con un amico"),
    ("C", "che ne pensi del cinema italiano"),
    ("C", "puoi tradurre questa frase in tedesco"),
    ("C", "dammi qualche idea per una cena veloce"),
    ("C", "come posso iniziare a meditare"),
    ("C", "hi there, how's your day going"),
    ("C", "recommend a good podcast for commuting"),
    ("C", "write a short poem about autumn"),
    ("C", "help me make this email sound friendlier"),
    ("D", "in che anno è stata fondata Roma"),
    ("D", "quanti abitanti ha Madrid"),
    ("D", "qual è il simbolo chimico del sodio"),
    ("D", "dove si trova il lago Baikal"),
    ("D", "quali sono gli orari dei treni per Firenze"),
    ("D", "come richiedo la carta d'identità elettronica"),
    ("D", "quanti metri ci sono in un miglio"),
    ("D", "chi ha dipinto la Gioconda"),
    ("D", "what is the capital of Australia"),
    ("D", "when is the next solar eclipse"),
    ("D", "what are the side effects of ibuprofen"),
    ("D", "how many calories are in an apple"),
]

# ── Held-out verifier set ─────────────────────────────────────────────
# Y: risposta pertinente, anche corta e senza overlap lessicale.
# N: rifiuto, risposta vuota, fuori tema o generica.
# Le ultime voci sono negative "difficili": ripetono parole della domanda
# senza rispondere. Non ci sono negative basate su fatti sbagliati, perché
# un modello lessicale non può verificarli.
VERIFY = [
    ("Y", "come si ordina una lista in Python",
     "Usa sorted(lista) oppure lista.sort(); entrambi restituiscono o ordinano in place."),
    ("Y", "qual è la capitale del Giappone", "Tokyo."),
    ("Y", "quanto fa 17 per 23", "391."),
    ("Y", "come si cura il mal di testa",
     "Riposa in un ambiente silenzioso, bevi acqua e, se serve, usa un analgesico da banco seguendo le istruzioni."),
    ("Y", "spiega la fotosintesi",
     "Le piante trasformano luce, acqua e CO2 in glucosio e ossigeno usando la clorofilla."),
    ("Y", "write a Python function to reverse a string", "def reverse(s): return s[::-1]"),
    ("Y", "what is the capital of France", "Paris."),
    ("Y", "why is the sky blue",
     "Rayleigh scattering makes shorter blue wavelengths scatter more in the atmosphere."),
    ("Y", "consigliami un film",
     "Ti consiglio 'Il buono, il brutto, il cattivo': western classico, ottimo ritmo e fotografia."),
    ("Y", "come si usa git rebase",
     "Rebase riapplica i commit su una nuova base: git rebase main, poi risolvi eventuali conflitti."),
    ("Y", "calcola l'area di un cerchio di raggio 3", "Area = πr² = 9π ≈ 28,27."),
    ("Y", "chi ha scritto la Divina Commedia", "Dante Alighieri."),
    ("Y", "how do I sort a list in Python",
     "Use sorted(my_list) for a new list, or my_list.sort() to sort in place."),
    ("Y", "what is 15 percent of 240", "36."),
    ("Y", "explain the Pythagorean theorem",
     "In a right triangle, the square of the hypotenuse equals the sum of the squares of the other two sides: a²+b²=c²."),
    ("Y", "quanto fa 2+2", "4."),
    ("Y", "qual è il simbolo chimico dell'acqua", "H2O."),
    ("N", "come si ordina una lista in Python", "Non lo so."),
    ("N", "qual è la capitale del Giappone", "Il meteo oggi è soleggiato."),
    ("N", "quanto fa 17 per 23", "Non posso rispondere."),
    ("N", "come si cura il mal di testa", "Basta."),
    ("N", "spiega la fotosintesi", "La fotosintesi è un processo molto importante e interessante."),
    ("N", "write a Python function to reverse a string", "I cannot help with that."),
    ("N", "consigliami un film", "No idea, sorry."),
    ("N", "come si usa git rebase", "Git è uno strumento utile."),
    ("N", "calcola l'area di un cerchio di raggio 3", "Il risultato è un numero."),
    ("N", "chi ha scritto la Divina Commedia", "Non ho informazioni sufficienti."),
    ("N", "how do I sort a list in Python", "Sure."),
    ("N", "what is 15 percent of 240", "The weather is nice."),
    ("N", "explain the Pythagorean theorem", "It is a famous theorem."),
    ("N", "come si ordina una lista in Python", "Python è un linguaggio di programmazione."),
    ("N", "qual è la capitale del Giappone", "Il Giappone è un paese asiatico."),
    ("N", "calcola l'area di un cerchio di raggio 3", "Il cerchio ha un raggio e un diametro."),
]



# ── Secondo set held-out (blind) ──────────────────────────────────────
# Non usato né per il training né per scegliere le guardie. Il caso
# "Munich" è un errore di fatto ed è elencato in OUT_OF_SCOPE: un
# verificatore lessicale non può correggerlo.
BLIND = [
    ('Y', 'come si inverte una stringa in Python',
     'Puoi usare lo slicing: s[::-1].'),
    ('Y', 'qual è la capitale della Spagna',
     'Madrid.'),
    ('Y', 'quanto fa 24 per 3',
     '72.'),
    ('Y', 'come si cura il raffreddore',
     'Riposo, liquidi e, se serve, un decongestionante.'),
    ('Y', 'spiega la gravità',
     "La gravità è l'attrazione tra masse; sulla Terra ci tiene al suolo."),
    ('Y', 'write a JavaScript function to sum two numbers',
     'function sum(a,b){return a+b;}'),
    ('Y', 'what is the capital of Germany',
     'Berlin.'),
    ('Y', 'why do leaves turn yellow in autumn',
     'Chlorophyll breaks down, revealing yellow carotenoid pigments.'),
    ('Y', 'consigliami un libro',
     "Ti consiglio 'Il nome della rosa': mistero, storia e filosofia."),
    ('Y', 'come si crea un branch in git',
     'Usa git checkout -b nome-branch oppure git switch -c nome-branch.'),
    ('Y', 'calcola il perimetro di un quadrato di lato 5',
     'Perimetro = 4 × 5 = 20.'),
    ('Y', 'chi ha dipinto la Cappella Sistina',
     'Michelangelo Buonarroti.'),
    ('Y', 'how do I reverse a list in Python',
     'Use my_list[::-1] or my_list.reverse() to reverse in place.'),
    ('Y', 'what is 20 percent of 150',
     '30.'),
    ('Y', 'explain photosynthesis',
     'Plants use sunlight, water and CO2 to make glucose and release oxygen.'),
    ('Y', 'quanto fa 9+7',
     '16.'),
    ('N', 'come si inverte una stringa in Python',
     'Non lo so.'),
    ('N', 'qual è la capitale della Spagna',
     'Oggi piove.'),
    ('N', 'quanto fa 24 per 3',
     'Non posso rispondere.'),
    ('N', 'come si cura il raffreddore',
     'Basta.'),
    ('N', 'spiega la gravità',
     'La gravità è un argomento molto importante.'),
    ('N', 'write a JavaScript function to sum two numbers',
     'I cannot help with that.'),
    ('N', 'why do leaves turn yellow in autumn',
     'Because of the weather.'),
    ('N', 'consigliami un libro',
     'No idea, sorry.'),
    ('N', 'come si crea un branch in git',
     'Git è uno strumento utile.'),
    ('N', 'calcola il perimetro di un quadrato di lato 5',
     'Il risultato è un numero.'),
    ('N', 'chi ha dipinto la Cappella Sistina',
     'Non ho informazioni sufficienti.'),
    ('N', 'how do I reverse a list in Python',
     'Sure.'),
    ('N', 'what is 20 percent of 150',
     'The weather is nice.'),
    ('N', 'explain photosynthesis',
     'It is a famous process.'),
    ('N', 'come si inverte una stringa in Python',
     'Python è un linguaggio di programmazione.'),
    ('N', 'qual è la capitale della Spagna',
     'La Spagna è un paese europeo.'),
    ('N', 'calcola il perimetro di un quadrato di lato 5',
     'Il quadrato ha quattro lati.'),
    ('N', 'how do I reverse a list in Python',
     'Python is a programming language.'),
    ('N', 'what is 20 percent of 150',
     'The result is a number.'),
]

# Casi che il verificatore NON può giudicare: sono errori di fatto, non
# problemi di pertinenza. Li teniamo separati e fuori dal conteggio.
OUT_OF_SCOPE = [
    ("N", "what is the capital of France", "Berlin.", "fatto sbagliato"),
    ("N", "why is the sky blue", "Because of gravity.", "fatto sbagliato"),
    ("N", "what is the capital of Germany", "Munich.", "fatto sbagliato"),
]

def softmax(x: np.ndarray) -> np.ndarray:
    e = np.exp(x - x.max())
    return e / e.sum()


def metrics(y_true, y_pred, labels):
    y_true = np.asarray(y_true)
    y_pred = np.asarray(y_pred)
    confusion = np.zeros((len(labels), len(labels)), dtype=int)
    for t, p in zip(y_true, y_pred):
        confusion[labels.index(t), labels.index(p)] += 1

    per_class = {}
    for i, label in enumerate(labels):
        tp = confusion[i, i]
        fp = confusion[:, i].sum() - tp
        fn = confusion[i, :].sum() - tp
        precision = tp / (tp + fp) if tp + fp else 0.0
        recall = tp / (tp + fn) if tp + fn else 0.0
        f1 = 2 * precision * recall / (precision + recall) if precision + recall else 0.0
        per_class[label] = {
            "precision": precision,
            "recall": recall,
            "f1": f1,
            "support": int(confusion[i, :].sum()),
        }
    return {
        "accuracy": float((y_true == y_pred).mean()),
        "confusion": confusion.tolist(),
        "per_class": per_class,
    }


def run_classifier(session):
    labels = gen.CLASSIFIER_LABELS
    input_name = session.get_inputs()[0].name
    rows = []
    for expected, text in CLS:
        x = np.asarray([gen.features_text(text)], dtype=np.float32)
        logits = session.run(None, {input_name: x})[0][0]
        prob = softmax(logits)
        pred = labels[int(prob.argmax())]
        rows.append({
            "expected": expected,
            "predicted": pred,
            "confidence": float(prob.max()),
            "text": text,
        })
    result = metrics([r["expected"] for r in rows],
                     [r["predicted"] for r in rows], labels)
    result.update({
        "count": len(rows),
        "correct": sum(r["expected"] == r["predicted"] for r in rows),
        "mean_confidence": float(np.mean([r["confidence"] for r in rows])),
        "rows": rows,
        "labels": labels,
    })
    return result


def _rows_metrics(rows, labels):
    result = metrics([r["expected"] for r in rows],
                     [r["predicted"] for r in rows], labels)
    result.update({
        "count": len(rows),
        "correct": sum(r["expected"] == r["predicted"] for r in rows),
        "mean_confidence": float(np.mean([r["confidence"] for r in rows])),
        "rows": rows,
        "labels": labels,
    })
    return result


def run_verifier(session, cases):
    """Valuta modello grezzo e pipeline effettiva (modello + guardie)."""
    labels = gen.VERIFIER_LABELS
    input_name = session.get_inputs()[0].name
    raw_rows, guarded_rows = [], []
    for expected, question, answer in cases:
        x = np.asarray([gen.features_pair(question, answer)], dtype=np.float32)
        logits = session.run(None, {input_name: x})[0][0]
        prob = softmax(logits)
        raw_pred = labels[int(prob.argmax())]
        raw_conf = float(prob.max())

        guard_label, guard_reason = gen.relevance_guard(question, answer)
        final_pred = guard_label or raw_pred

        base = {
            "expected": expected,
            "question": question,
            "answer": answer,
        }
        raw_rows.append({**base,
                         "predicted": raw_pred,
                         "confidence": raw_conf,
                         "guard_reason": ""})
        guarded_rows.append({**base,
                             "predicted": final_pred,
                             "confidence": 1.0 if guard_label else raw_conf,
                             "guard_reason": guard_reason,
                             "guard_applied": bool(guard_label)})

    return {
        "raw": _rows_metrics(raw_rows, labels),
        "guarded": _rows_metrics(guarded_rows, labels),
    }


def print_verifier(title, result, show_errors=True):
    print_section(title, result, show_errors)
    applied = sum(1 for r in result["rows"] if r.get("guard_applied"))
    if applied:
        print(f"guardie deterministiche applicate: {applied}/{result['count']}")


def print_section(title, result, show_errors=True):
    print(f"\n{title}: {result['correct']}/{result['count']} = {result['accuracy'] * 100:.1f}%")
    print("confusione (righe=vero, colonne=predetto):")
    print("      " + " ".join(f"{label:>4}" for label in result["labels"]))
    for label, row in zip(result["labels"], result["confusion"]):
        print(f"  {label:>2}  " + " ".join(f"{v:4d}" for v in row))
    print("per classe:")
    print("  classe  precision  recall    f1  support")
    for label, m in result["per_class"].items():
        print(f"  {label:>6}  {m['precision']:9.3f}  {m['recall']:6.3f}  {m['f1']:5.3f}  {m['support']:7d}")
    print(f"confidenza media: {result['mean_confidence']:.3f}")

    if show_errors:
        errors = [r for r in result["rows"] if r["expected"] != r["predicted"]]
        if errors:
            print(f"errori ({len(errors)}):")
            for r in errors:
                if "text" in r:
                    print(f"  {r['expected']}->{r['predicted']} {r['confidence']:.2f}  {r['text']}")
                else:
                    print(f"  {r['expected']}->{r['predicted']} {r['confidence']:.2f}  "
                          f"Q={r['question']} | A={r['answer'][:90]}")


def main() -> int:
    parser = argparse.ArgumentParser(description="Benchmark qualitativo Decision ONNX")
    parser.add_argument("--json", metavar="FILE",
                        help="salva i risultati completi in JSON")
    parser.add_argument("--min-classifier", type=float, default=0.0,
                        help="soglia minima di accuratezza classificatore (0-1)")
    parser.add_argument("--min-verifier", type=float, default=0.0,
                        help="soglia minima per il verificatore effettivo "
                             "(modello + guardie) sul set dev (0-1)")
    parser.add_argument("--min-verifier-raw", type=float, default=0.0,
                        help="soglia minima per il solo modello verificatore "
                             "sul set dev (0-1)")
    parser.add_argument("--min-verifier-blind", type=float, default=0.0,
                        help="soglia minima per il verificatore effettivo "
                             "sul set blind (0-1)")
    parser.add_argument("--quiet-errors", action="store_true",
                        help="non stampare l'elenco degli errori")
    args = parser.parse_args()

    clf_path = MODELS / "decision_classifier.onnx"
    ver_path = MODELS / "decision_verifier.onnx"
    for path in (clf_path, ver_path):
        if not path.is_file():
            sys.exit(f"Modello mancante: {path}")

    clf_session = ort.InferenceSession(str(clf_path), providers=["CPUExecutionProvider"])
    ver_session = ort.InferenceSession(str(ver_path), providers=["CPUExecutionProvider"])

    classifier = run_classifier(clf_session)
    verifier = run_verifier(ver_session, VERIFY)
    verifier_blind = run_verifier(ver_session, BLIND)
    out_of_scope = run_verifier(
        ver_session, [(e, q, a) for e, q, a, _ in OUT_OF_SCOPE])

    print("Decision ONNX — benchmark held-out")
    print(f"modelli: {clf_path.name} ({clf_path.stat().st_size} B), "
          f"{ver_path.name} ({ver_path.stat().st_size} B)")
    print_section("CLASSIFIER", classifier, not args.quiet_errors)
    print_verifier("VERIFIER dev — modello grezzo", verifier["raw"],
                   not args.quiet_errors)
    print_verifier("VERIFIER dev — modello + guardie", verifier["guarded"],
                   not args.quiet_errors)
    print_verifier("VERIFIER blind — modello grezzo", verifier_blind["raw"],
                   not args.quiet_errors)
    print_verifier("VERIFIER blind — modello + guardie", verifier_blind["guarded"],
                   not args.quiet_errors)

    print("\nFuori scope (errori di fatto, NON contati nelle metriche):")
    for r in out_of_scope["guarded"]["rows"]:
        print(f"  atteso {r['expected']} -> effettivo {r['predicted']}  "
              f"Q={r['question']} | A={r['answer']}")

    print("\nNota: il verificatore misura rifiuti/risposte vuote/bassa pertinenza; "
          "non è un fact-checker. Le guardie sono deterministiche e non usano rete.")
    print("La riga 'modello grezzo' è la qualità del modello ONNX; la riga "
          "'modello + guardie' è il comportamento effettivo del backend. "
          "Su questi set le guardie coprono molti/tutti i casi: non è "
          "accuratezza appresa dal modello.")

    if args.json:
        payload = {
            "models": {
                "classifier": str(clf_path),
                "verifier": str(ver_path),
            },
            "classifier": classifier,
            "verifier": verifier,
            "verifier_blind": verifier_blind,
            "out_of_scope": out_of_scope,
        }
        Path(args.json).write_text(json.dumps(payload, indent=2, ensure_ascii=False),
                                   encoding="utf-8")
        print(f"JSON scritto in {args.json}")

    failed = False
    if args.min_classifier and classifier["accuracy"] < args.min_classifier:
        print(f"❌ classifier {classifier['accuracy']:.3f} < {args.min_classifier:.3f}")
        failed = True
    guarded_acc = verifier["guarded"]["accuracy"]
    raw_acc = verifier["raw"]["accuracy"]
    blind_acc = verifier_blind["guarded"]["accuracy"]
    if args.min_verifier and guarded_acc < args.min_verifier:
        print(f"❌ verifier guarded {guarded_acc:.3f} < {args.min_verifier:.3f}")
        failed = True
    if args.min_verifier_raw and raw_acc < args.min_verifier_raw:
        print(f"❌ verifier raw {raw_acc:.3f} < {args.min_verifier_raw:.3f}")
        failed = True
    if args.min_verifier_blind and blind_acc < args.min_verifier_blind:
        print(f"❌ verifier blind {blind_acc:.3f} < {args.min_verifier_blind:.3f}")
        failed = True
    return 1 if failed else 0


if __name__ == "__main__":
    raise SystemExit(main())
