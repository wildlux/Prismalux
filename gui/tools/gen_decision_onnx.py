#!/usr/bin/env python3
"""Genera i due modelli ONNX usati dal backend Decision Model.

    decision_classifier.onnx : testo -> logits [A, B, C, D]
    decision_verifier.onnx   : domanda+risposta -> logits [N, Y]

I modelli sono minuscoli (pesi lineari su feature hashing) e non
richiedono un tokenizer esterno: le feature sono calcolate in C++ da
`gui/decision_onnx.cpp` con la STESSA funzione riprodotta qui sotto.
Se cambi una delle due implementazioni, rigenera i modelli e riesegui
`test_decision_onnx`.

Uso:
    python3 gui/tools/gen_decision_onnx.py [--out DIR] [--check]

Dipendenze: numpy, onnx, onnxruntime (solo per la verifica).
"""

import argparse
import json
import math
import os
import sys

import numpy as np

try:
    import onnx
    from onnx import TensorProto, helper
except ImportError:
    sys.exit("Serve il pacchetto python 'onnx' (pip install onnx)")

# ── Deve combaciare con decision_onnx.cpp ──────────────────────────────
DIM = 512
CLASSIFIER_LABELS = ["A", "B", "C", "D"]   # coding, reasoning, chat, other
VERIFIER_LABELS = ["N", "Y"]


def normalize(text: str) -> str:
    out = []
    prev_space = True
    for ch in text:
        if ch.isalnum():
            out.append(ch.lower())
            prev_space = False
        elif not prev_space:
            out.append(" ")
            prev_space = True
    return "".join(out).strip()


def fnv1a(text: str) -> int:
    h = 2166136261
    for b in text.encode("utf-8"):
        h ^= b
        h = (h * 16777619) & 0xFFFFFFFF
    return h


def add(vec, key: str, weight: float = 1.0):
    vec[fnv1a(key) % DIM] += weight


def add_text(vec, text: str, prefix: str = ""):
    words = normalize(text).split()
    for i, w in enumerate(words):
        add(vec, prefix + "w:" + w)
        for j in range(len(w) - 3):
            add(vec, prefix + "c:" + w[j:j + 4], 0.5)
        if i + 1 < len(words):
            add(vec, prefix + "b:" + w + "_" + words[i + 1], 0.5)


def l2(vec):
    norm = math.sqrt(sum(v * v for v in vec))
    if norm > 1e-9:
        return [v / norm for v in vec]
    return vec


def features_text(text: str):
    vec = [0.0] * DIM
    add_text(vec, text)
    return l2(vec)


REFUSALS = (
    "non lo so", "non posso", "impossibile", "mi dispiace", "non ne ho idea",
    "i don't know", "cannot", "unable", "not sure", "no idea",
)


def features_pair(question: str, answer: str):
    """Feature del verificatore: testo di Q e A + segnali espliciti.

    I segnali (x_ov_*, x_len_*, x_ratio_*, x_refusal, x_empty) sono
    pseudo-token hashati: il modello impara un peso per ciascuno senza
    bisogno di ingressi numerici separati.
    """
    vec = [0.0] * DIM
    add_text(vec, question)
    add_text(vec, answer)
    add_text(vec, answer, "a:")

    qw = {w for w in normalize(question).split() if len(w) > 2}
    aw = {w for w in normalize(answer).split() if len(w) > 2}
    overlap = len(qw & aw) / max(1, len(qw)) if qw else 0.0
    add(vec, "x_ov_%d" % min(5, int(round(overlap * 5))), 2.0)

    alen = len(aw)
    len_bin = 0 if alen < 5 else 1 if alen < 20 else 2 if alen < 60 else 3
    add(vec, "x_len_%d" % len_bin, 2.0)

    ratio = alen / max(1, len(qw))
    r_bin = 0 if ratio < 0.3 else 1 if ratio < 1.0 else 2 if ratio < 3.0 else 3
    add(vec, "x_ratio_%d" % r_bin, 2.0)

    low = answer.lower()
    if any(t in low for t in REFUSALS):
        add(vec, "x_refusal", 3.0)
    if alen < 3:
        add(vec, "x_empty", 3.0)
    return l2(vec)


# ══════════════════════════════════════════════════════════════════════
# Dataset classificatore
# ══════════════════════════════════════════════════════════════════════
CODING = [
    "scrivi una funzione python che ordina una lista",
    "come faccio a correggere questo errore di compilazione",
    "implementa un algoritmo di ricerca binaria in c++",
    "perché il mio codice javascript non funziona",
    "debug di un programma che va in segmentation fault",
    "crea una query sql per ottenere i clienti",
    "come si usa git rebase",
    "spiegami la differenza tra lista e dizionario in python",
    "scrivi un test unitario con pytest",
    "come faccio un ciclo for in rust",
    "refactoring di questa classe java",
    "api rest con flask",
    "docker compose per un database postgres",
    "come leggo un file json in node",
    "regex per validare una email",
    "ottimizzare una query lenta",
    "usare le promise in javascript",
    "errore import module not found",
    "come funziona la ricorsione nel codice",
    "scrivere un makefile",
    "cmake non trova la libreria",
    "typescript tipo generico",
    "gestire eccezioni in python",
    "compilare un programma c con gcc",
    "merge di due branch git",
    "scrivere documentazione per una funzione",
    "come faccio a fare il deploy su linux",
    "creare un endpoint in fastapi",
    "leggere argv in bash",
    "implementare una coda con priorità",
    "write a python function to sort a list",
    "fix this compile error in my code",
    "implement binary search in c++",
    "why does my javascript code not work",
    "debug a segmentation fault",
    "create a sql query to list customers",
    "how to use git rebase",
    "difference between list and dict in python",
    "write a unit test with pytest",
    "how to write a for loop in rust",
    "refactor this java class",
    "build a rest api with flask",
    "docker compose postgres database",
    "read a json file in node",
    "regex to validate an email",
    "optimize a slow database query",
    "javascript promises explained",
    "module not found error",
    "recursion in programming",
    "write a makefile",
    "cmake cannot find library",
    "typescript generic type",
    "handle exceptions in python",
    "compile c program with gcc",
    "merge two git branches",
    "write docstring for a function",
    "deploy an app on linux",
    "create a fastapi endpoint",
    "bash command line arguments",
    "implement a priority queue",
]

REASONING = [
    "calcola l'integrale di x al quadrato",
    "dimostra che la radice di 2 è irrazionale",
    "quanto fa il 15 per cento di 240",
    "spiegami il teorema di pitagora",
    "risolvi questa equazione di secondo grado",
    "qual è la derivata di sin x",
    "analizza la complessità temporale di questo algoritmo",
    "confronta pro e contro di due approcci",
    "ragiona su questo problema di logica",
    "se tutti gli uomini sono mortali e socrate è un uomo",
    "calcola la probabilità di ottenere due sei con i dadi",
    "spiegami la relatività ristretta",
    "perché il cielo è blu",
    "dimostra per induzione che la somma dei primi n numeri",
    "trova l'errore nel ragionamento",
    "qual è la causa di questo fenomeno",
    "valuta questa argomentazione",
    "stima quanto tempo serve per riempire la vasca",
    "quanti anni luce ci sono in un parsec",
    "risolvi il sistema di equazioni",
    "spiegami la differenza tra media e mediana",
    "calcola l'area di un cerchio di raggio 5",
    "teorema di bayes esempio",
    "analizza i dati e trova una tendenza",
    "deduci la regola della sequenza",
    "qual è la soluzione ottimale a questo problema",
    "spiegami la termodinamica",
    "perché 0.999 è uguale a 1",
    "problema di geometria con un triangolo rettangolo",
    "logica matematica e deduzione",
    "calculate the integral of x squared",
    "prove that the square root of 2 is irrational",
    "what is 15 percent of 240",
    "explain the pythagorean theorem",
    "solve this quadratic equation",
    "what is the derivative of sin x",
    "analyze the time complexity of this algorithm",
    "compare the pros and cons of two approaches",
    "reason about this logic puzzle",
    "all men are mortal and socrates is a man",
    "probability of rolling two sixes",
    "explain special relativity",
    "why is the sky blue",
    "prove by induction the sum of the first n numbers",
    "find the flaw in the reasoning",
    "what causes this phenomenon",
    "evaluate this argument",
    "estimate how long it takes to fill the tank",
    "how many light years in a parsec",
    "solve the system of equations",
    "difference between mean and median",
    "area of a circle with radius 5",
    "bayes theorem example",
    "analyze the data and find a trend",
    "deduce the rule of the sequence",
    "what is the optimal solution to this problem",
    "explain thermodynamics",
    "why does 0.999 equal 1",
    "geometry problem with a right triangle",
    "mathematical logic and deduction",
]

CHAT = [
    "ciao come stai",
    "buongiorno",
    "che cosa fai",
    "raccontami una barzelletta",
    "consigliami un film",
    "come ti senti oggi",
    "parliamo del più e del meno",
    "scrivi una poesia sull'amore",
    "traduci questa frase in inglese",
    "correggi il tono di questa email",
    "dammi un consiglio su come organizzarmi",
    "cosa pensi della musica classica",
    "qual è il tuo colore preferito",
    "scrivimi un messaggio di auguri",
    "riassumi questo testo in modo semplice",
    "rendi più formale questa frase",
    "inventa una storia per bambini",
    "quale libro mi consigli",
    "come posso rilassarmi",
    "sei simpatico",
    "grazie mille",
    "buona notte",
    "cosa cucino stasera",
    "consigli per un viaggio a roma",
    "scrivi una recensione positiva",
    "come si dice ciao in francese",
    "mi sento triste",
    "fammi una domanda",
    "parlami di te",
    "rispondi in modo gentile",
    "hello how are you",
    "good morning",
    "what do you do",
    "tell me a joke",
    "recommend a movie",
    "how do you feel today",
    "let us chat about anything",
    "write a poem about love",
    "translate this sentence into english",
    "fix the tone of this email",
    "advice on organizing my day",
    "what do you think of classical music",
    "what is your favorite color",
    "write a birthday message",
    "summarize this text simply",
    "make this sentence more formal",
    "invent a children's story",
    "recommend a book",
    "how can i relax",
    "you are funny",
    "thanks a lot",
    "good night",
    "what should i cook tonight",
    "travel tips for rome",
    "write a positive review",
    "how do you say hello in french",
    "i feel sad",
    "ask me a question",
    "tell me about yourself",
    "answer kindly",
]

OTHER = [
    "qual è la capitale della Francia",
    "chi ha scritto la Divina Commedia",
    "quando è caduto l'impero romano",
    "quanti abitanti ha l'Italia",
    "qual è il fiume più lungo del mondo",
    "che tempo fa domani",
    "dove si trova il monte Everest",
    "qual è la formula chimica dell'acqua",
    "come si chiama il presidente della Francia",
    "quanti giorni ha un anno bisestile",
    "qual è il significato di questa parola",
    "elenco delle regioni italiane",
    "chi ha inventato la stampa",
    "qual è la differenza tra virus e batteri",
    "come si cura il mal di testa",
    "quali sono i sintomi dell'influenza",
    "qual è la tariffa del treno",
    "orari di apertura del museo",
    "come richiedere il passaporto",
    "qual è la legge sull'affitto",
    "quanti grammi ci sono in una libbra",
    "converti 10 chilometri in miglia",
    "qual è la popolazione di Tokyo",
    "data di nascita di Einstein",
    "qual è il pianeta più grande",
    "come si scrive questa parola",
    "definizione di fotosintesi",
    "qual è il codice fiscale",
    "dove posso comprare un biglietto",
    "che cosa significa questo simbolo",
    "what is the capital of France",
    "who wrote the divine comedy",
    "when did the roman empire fall",
    "how many people live in Italy",
    "what is the longest river in the world",
    "what is the weather tomorrow",
    "where is mount everest",
    "what is the chemical formula of water",
    "who is the president of France",
    "how many days in a leap year",
    "what is the meaning of this word",
    "list the regions of Italy",
    "who invented the printing press",
    "difference between a virus and a bacterium",
    "how do you cure a headache",
    "what are the symptoms of flu",
    "what is the train fare",
    "museum opening hours",
    "how to apply for a passport",
    "what is the rent law",
    "how many grams in a pound",
    "convert 10 kilometers to miles",
    "what is the population of Tokyo",
    "when was Einstein born",
    "what is the largest planet",
    "how do you spell this word",
    "definition of photosynthesis",
    "what is a tax code",
    "where can i buy a ticket",
    "what does this symbol mean",
]


def classification_dataset():
    X, y = [], []
    for label, rows in enumerate((CODING, REASONING, CHAT, OTHER)):
        for text in rows:
            X.append(features_text(text))
            y.append(label)
    return np.asarray(X, dtype=np.float32), np.asarray(y, dtype=np.int64)


# ══════════════════════════════════════════════════════════════════════
# Dataset verificatore (generato: positivo = risposta pertinente,
# negativo = rifiuto / risposta vuota / fuori tema)
# ══════════════════════════════════════════════════════════════════════
VERIFY_QUESTIONS = [
    ("come si ordina una lista in python", "lista python"),
    ("qual è la capitale della Francia", "capitale Francia"),
    ("calcola l'integrale di x al quadrato", "integrale x"),
    ("come si cura il mal di testa", "mal di testa"),
    ("spiegami il teorema di pitagora", "teorema pitagora"),
    ("quanti abitanti ha l'Italia", "abitanti Italia"),
    ("scrivi una funzione che legge un file json", "funzione file json"),
    ("perché il cielo è blu", "cielo blu"),
    ("consigliami un film stasera", "film stasera"),
    ("come si usa git rebase", "git rebase"),
    ("what is the capital of France", "capital France"),
    ("how do you sort a list in python", "sort list python"),
    ("calculate the integral of x squared", "integral x"),
    ("tell me a joke", "joke"),
    ("what is the population of Tokyo", "population Tokyo"),
]

POSITIVE_TEMPLATES = [
    # Risposte brevi: una risposta corretta può essere di una sola riga.
    "{kw}.",
    "La risposta è {kw}.",
    "Sì: {kw}.",
    "{kw}, ecco il risultato.",
    # Risposte medie/lunghe.
    "La risposta riguarda {kw}: si procede così, con un esempio pratico e una spiegazione passo per passo.",
    "{kw} funziona in questo modo: prima si imposta il valore, poi si applica la regola e si verifica il risultato.",
    "Sì, {kw} è corretto. Ecco la spiegazione completa con i passaggi e il risultato finale.",
    "Per {kw} la soluzione è questa: usa il comando appropriato, controlla l'output e correggi eventuali errori.",
    "In breve, {kw} dipende dal contesto. Il valore richiesto è illustrato qui sotto con i calcoli necessari.",
]

# Negativi brevi, medi e lunghi: la lunghezza da sola non deve decidere.
NEGATIVE_ANSWERS = [
    "Non lo so.",
    "Non posso rispondere a questa domanda.",
    "Impossibile.",
    "Ok.",
    "No.",
    "Nessuna idea.",
    "Il meteo è bello.",
    "Non ne ho idea, mi dispiace.",
    "Questa domanda non ha senso.",
    "Non ho informazioni sufficienti per rispondere in modo affidabile.",
    "Mi dispiace, ma non posso aiutarti con questa richiesta.",
    "Il risultato è un numero casuale senza alcuna spiegazione.",
    "Oggi il meteo è soleggiato e la temperatura è gradevole.",
    "I don't know.",
    "I cannot answer that.",
    "Sure.",
    "No.",
    "No idea, sorry.",
    "The weather is nice.",
    "I do not have enough information to answer reliably.",
    "The result is a random number without explanation.",
    "The weather is sunny and warm today.",
]


def verification_dataset():
    X, y = [], []
    for q, kw in VERIFY_QUESTIONS:
        for tmpl in POSITIVE_TEMPLATES:
            X.append(features_pair(q, tmpl.format(kw=kw)))
            y.append(1)
        for ans in NEGATIVE_ANSWERS:
            X.append(features_pair(q, ans))
            y.append(0)
    return np.asarray(X, dtype=np.float32), np.asarray(y, dtype=np.int64)


# ══════════════════════════════════════════════════════════════════════
# Training + export
# ══════════════════════════════════════════════════════════════════════
def train_softmax(X, y, num_labels, epochs=1200, lr=0.02, l2=1e-4, seed=42):
    rng = np.random.default_rng(seed)
    n, d = X.shape
    W = rng.normal(0.0, 0.01, size=(num_labels, d)).astype(np.float64)
    b = np.zeros(num_labels, dtype=np.float64)

    counts = np.bincount(y, minlength=num_labels)
    class_weight = n / (num_labels * np.maximum(counts, 1))
    sample_weight = class_weight[y]

    mW, vW = np.zeros_like(W), np.zeros_like(W)
    mb, vb = np.zeros_like(b), np.zeros_like(b)
    beta1, beta2, eps = 0.9, 0.999, 1e-8

    for t in range(1, epochs + 1):
        logits = X @ W.T + b
        logits -= logits.max(axis=1, keepdims=True)
        p = np.exp(logits)
        p /= p.sum(axis=1, keepdims=True)

        grad = p
        grad[np.arange(n), y] -= 1.0
        grad *= sample_weight[:, None]
        grad /= n

        gW = grad.T @ X + l2 * W
        gb = grad.sum(axis=0)

        mW = beta1 * mW + (1 - beta1) * gW
        vW = beta2 * vW + (1 - beta2) * (gW * gW)
        mb = beta1 * mb + (1 - beta1) * gb
        vb = beta2 * vb + (1 - beta2) * (gb * gb)

        mW_hat = mW / (1 - beta1 ** t)
        vW_hat = vW / (1 - beta2 ** t)
        mb_hat = mb / (1 - beta1 ** t)
        vb_hat = vb / (1 - beta2 ** t)

        W -= lr * mW_hat / (np.sqrt(vW_hat) + eps)
        b -= lr * mb_hat / (np.sqrt(vb_hat) + eps)

    return W.astype(np.float32), b.astype(np.float32)


def export_onnx(path, W, b, name):
    num_labels, dim = W.shape
    node = helper.make_node("Gemm", ["features", "W", "b"], ["logits"], transB=1)
    graph = helper.make_graph(
        [node],
        name,
        [helper.make_tensor_value_info("features", TensorProto.FLOAT, [1, dim])],
        [helper.make_tensor_value_info("logits", TensorProto.FLOAT, [1, num_labels])],
        [
            helper.make_tensor("W", TensorProto.FLOAT, W.shape, W.flatten().tolist()),
            helper.make_tensor("b", TensorProto.FLOAT, b.shape, b.tolist()),
        ],
    )
    model = helper.make_model(graph, opset_imports=[helper.make_opsetid("", 13)])
    model.ir_version = 8
    model.producer_name = "prismalux-decision-onnx"
    onnx.checker.check_model(model)
    onnx.save(model, path)
    return model


def evaluate(W, b, X, y, labels):
    logits = X @ W.T + b
    pred = logits.argmax(axis=1)
    acc = float((pred == y).mean())
    confusion = np.zeros((len(labels), len(labels)), dtype=int)
    for t, p in zip(y, pred):
        confusion[t, p] += 1
    return acc, confusion


def ort_check(path, samples, labels, is_pair):
    try:
        import onnxruntime as ort
    except ImportError:
        print("  (onnxruntime non installato: salto la verifica runtime)")
        return
    sess = ort.InferenceSession(path, providers=["CPUExecutionProvider"])
    name = sess.get_inputs()[0].name
    for text, expected in samples:
        if is_pair:
            q, a = text
            vec = features_pair(q, a)
        else:
            vec = features_text(text)
        x = np.asarray([vec], dtype=np.float32)
        logits = sess.run(None, {name: x})[0][0]
        probs = np.exp(logits - logits.max())
        probs /= probs.sum()
        got = labels[int(probs.argmax())]
        print("  %-60s -> %s (%.2f) atteso %s"
              % (str(text)[:60], got, float(probs.max()), expected))


def main():
    ap = argparse.ArgumentParser()
    ap.add_argument("--out", default=None,
                    help="cartella di output (default: Frameworks/onnx/models)")
    ap.add_argument("--check", action="store_true",
                    help="stampa alcune predizioni di controllo")
    args = ap.parse_args()

    here = os.path.dirname(os.path.abspath(__file__))
    root = os.path.abspath(os.path.join(here, "..", ".."))
    out_dir = args.out or os.path.join(root, "Frameworks", "onnx", "models")
    os.makedirs(out_dir, exist_ok=True)

    # ── Classificatore ──
    X, y = classification_dataset()
    W, b = train_softmax(X, y, len(CLASSIFIER_LABELS))
    acc, conf = evaluate(W, b, X, y, CLASSIFIER_LABELS)
    print("classifier: %d esempi, accuratezza train %.3f" % (len(y), acc))
    print("  matrice (righe=vero, colonne=predetto) A B C D:")
    for row in conf:
        print("   ", " ".join("%4d" % v for v in row))
    cpath = os.path.join(out_dir, "decision_classifier.onnx")
    export_onnx(cpath, W, b, "decision_classifier")

    # ── Verificatore ──
    Xv, yv = verification_dataset()
    Wv, bv = train_softmax(Xv, yv, len(VERIFIER_LABELS))
    accv, confv = evaluate(Wv, bv, Xv, yv, VERIFIER_LABELS)
    print("verifier: %d esempi, accuratezza train %.3f" % (len(yv), accv))
    print("  matrice (righe=vero, colonne=predetto) N Y:")
    for row in confv:
        print("   ", " ".join("%4d" % v for v in row))
    vpath = os.path.join(out_dir, "decision_verifier.onnx")
    export_onnx(vpath, Wv, bv, "decision_verifier")

    meta = {
        "feature_dim": DIM,
        "classifier_labels": CLASSIFIER_LABELS,
        "verifier_labels": VERIFIER_LABELS,
        "classifier_train_accuracy": acc,
        "verifier_train_accuracy": accv,
        "generator": "gui/tools/gen_decision_onnx.py",
    }
    with open(os.path.join(out_dir, "decision_onnx_meta.json"), "w",
              encoding="utf-8") as f:
        json.dump(meta, f, indent=2)
        f.write("\n")

    print("scritti:")
    for p in (cpath, vpath):
        print("  %s (%d byte)" % (p, os.path.getsize(p)))

    if args.check:
        print("controllo classificatore:")
        ort_check(cpath, [
            ("scrivi una funzione python che legge un file json", "A"),
            ("dimostra che la radice di 2 è irrazionale", "B"),
            ("ciao, raccontami una barzelletta", "C"),
            ("qual è la capitale del Giappone", "D"),
        ], CLASSIFIER_LABELS, False)
        print("controllo verificatore:")
        ort_check(vpath, [
            (("come si ordina una lista in python",
              "Per ordinare una lista in python usa sorted(lista) oppure lista.sort()."),
             "Y"),
            (("come si ordina una lista in python", "Non lo so."), "N"),
            (("qual è la capitale della Francia",
              "La capitale della Francia è Parigi."), "Y"),
            (("qual è la capitale della Francia", "Ok."), "N"),
        ], VERIFIER_LABELS, True)


if __name__ == "__main__":
    main()
