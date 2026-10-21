#!/usr/bin/env python3
"""Genera i due modelli ONNX usati dal backend Decision Model.

    decision_classifier.onnx : testo -> logits [A, B, C, D]
    decision_verifier.onnx   : domanda+risposta -> logits [N, Y]

I modelli sono minuscoli (pesi lineari su feature hashing) e non
richiedono un tokenizer esterno: le feature sono calcolate in C++ da
`gui/decision_onnx.cpp` con la STESSA funzione riprodotta qui sotto.
Il verificatore usa 64 bucket riservati a segnali strutturati e 448
bucket per il testo; `DecisionOnnx::verify()` applica poi guardie
deterministiche per rifiuti, risposte brevi/generiche, domande
numeriche e capitali. Non è un fact-checker.

Se cambi una delle due implementazioni, rigenera i modelli e la
fixture (`--fixture gui/tests/data/decision_onnx_features.json`) e
riesegui `test_decision_onnx`.

Uso:
    python3 gui/tools/gen_decision_onnx.py [--out DIR] [--check]
    python3 gui/tools/gen_decision_onnx.py --fixture gui/tests/data/decision_onnx_features.json

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
    "non ho informazioni", "non ho abbastanza",
)

# ── Verificatore: feature strutturate ─────────────────────────────────
# Le feature testuali usano 448 bucket (64..511); i primi 64 bucket sono
# riservati ai segnali strutturati. In questo modo un segnale come
# "domanda numerica + risposta senza cifre" non collide con una parola.
VERIFIER_TEXT_OFFSET = 64
VERIFIER_TEXT_DIM = DIM - VERIFIER_TEXT_OFFSET

VERIFIER_SIGNAL_INDEX = {
    "x_ov_0": 0, "x_ov_1": 1, "x_ov_2": 2, "x_ov_3": 3,
    "x_ov_4": 4, "x_ov_5": 5,
    "x_len_0": 6, "x_len_1": 7, "x_len_2": 8, "x_len_3": 9,
    "x_ratio_0": 10, "x_ratio_1": 11, "x_ratio_2": 12, "x_ratio_3": 13,
    "x_refusal": 14, "x_empty": 15,
    "x_q_num": 16, "x_q_who": 17, "x_q_where": 18, "x_q_when": 19,
    "x_q_how": 20, "x_q_why": 21, "x_q_open": 22, "x_q_fact": 23,
    "x_a_num": 24, "x_a_proper": 25, "x_a_code": 26, "x_a_proc": 27,
    "x_a_causal": 28, "x_a_vague": 29, "x_a_short": 30,
    "x_a_veryshort": 31,
    "x_num_num": 32, "x_num_nonum": 33,
    "x_who_proper": 34, "x_who_noproper": 35,
    "x_where_proper": 36, "x_where_noproper": 37,
    "x_fact_proper": 38, "x_fact_num": 39, "x_fact_noproper": 40,
    "x_when_num": 41, "x_when_nonum": 42,
    "x_how_proc": 43, "x_how_noproc": 44,
    "x_why_causal": 45, "x_why_nocausal": 46,
    "x_open_long": 47, "x_open_short": 48,
    "x_short_proper": 49, "x_short_num": 50, "x_vague_neg": 51,
    "x_noov_short_specific": 52, "x_noov_short_generic": 53,
    "x_overlap_vague": 54,
    "x_explain_long": 55, "x_explain_short": 56,
    "x_a_digit": 57, "x_num_digit": 58, "x_num_nodigit": 59,
    "x_short_proper_noov": 60, "x_short_num_noov": 61,
    "x_q_explain": 62, "x_q_capital": 63,
}
assert max(VERIFIER_SIGNAL_INDEX.values()) < VERIFIER_TEXT_OFFSET


def add_verifier_signal(vec, key: str, weight: float = 1.0):
    idx = VERIFIER_SIGNAL_INDEX.get(key)
    if idx is None:
        raise KeyError("segnale verificatore sconosciuto: %s" % key)
    vec[idx] += weight


def add_verifier_text(vec, text: str, prefix: str = ""):
    words = normalize(text).split()
    for i, w in enumerate(words):
        key = prefix + "w:" + w
        vec[fnv1a(key) % VERIFIER_TEXT_DIM + VERIFIER_TEXT_OFFSET] += 1.0
        for j in range(len(w) - 3):
            key = prefix + "c:" + w[j:j + 4]
            vec[fnv1a(key) % VERIFIER_TEXT_DIM + VERIFIER_TEXT_OFFSET] += 0.5
        if i + 1 < len(words):
            key = prefix + "b:" + w + "_" + words[i + 1]
            vec[fnv1a(key) % VERIFIER_TEXT_DIM + VERIFIER_TEXT_OFFSET] += 0.5


# Elenchi condivisi con decision_onnx.cpp. Le stringhe sono confrontate
# in minuscolo sul testo grezzo (non normalizzato) per i cue.
NUMBER_WORDS = (
    "zero", "uno", "due", "tre", "quattro", "cinque", "sei", "sette",
    "otto", "nove", "dieci", "cento", "mille",
    "one", "two", "three", "four", "five", "six", "seven", "eight",
    "nine", "ten", "hundred", "thousand",
)

VAGUE_CUES = (
    "importante", "interessante", "utile", "argomento", "aspetto",
    "numero casuale", "casuale", "famoso", "famosa", "famous",
    "thing", "stuff", "sure", "basta", "tema", "discusso",
    "strumento", "processo molto", "senza alcuna spiegazione",
    "random number", "linguaggio di programmazione",
    "programming language", "weather",
)

PROC_CUES = (
    "usa ", "usare", "puoi ", "devi ", "sort", "sorted", "funzione",
    "comando", "passo", "poi ", "prima ", "installa", "esegui",
    "chiama", "aggiungi", "crea ", "definisci", "return ", "def ",
    "riposo", "liquidi", "bevi", "mangia", "dormi", "evita",
    "prendi", "assumi", "trattamento", "terapia", "medico", "farmaco",
    "analgesico", "decongestionante", "checkout", "switch -c",
    "reverse", "slicing",
)

CAUSAL_CUES = (
    "perche", "perché", "because", "dovuto", "a causa", "causa",
    "scattering", "gravita", "gravità", "pressione", "temperatura",
    "breaks down", "reveals", "produce", "caused", "grazie a",
    "dipende da",
)

# Parole comuni: un token capitalizzato che compare qui non è un nome
# proprio. Serve a non trattare "Basta.", "Sure." o "Oggi" come entità.
COMMON_WORDS = set("""
il la lo un uno una i gli le the a an this that these those questo
questa questi queste it he she they we you i me him her us them my your
his its our their oggi ieri domani today tomorrow yesterday basta sure ok
okay yes no si sì certo vero falso perché perche because grazie salve ciao
ecco tutto tutti tutta tutte nulla niente nessuno sempre mai molto poco
bene male meglio peggio prima dopo poi anche solo come quando dove quindi
allora sono è era ha hanno può deve vuole fa fanno essere avere fare dire
andare venire stare is are was were be been have has had do does did can
could will would should may might must there here now then very really just
only also and or but if of to in on at for with from by as about into over
after before between under above again further once all any both each few
more most other some such nor not too don esatto giusto corretto perfetto assolutamente certamente ovviamente naturalmente infatti appunto esattamente forse probabilmente chiaramente semplicemente volentieri prego scusa scusi buongiorno buonasera buonanotte arrivederci right correct exactly absolutely certainly obviously indeed true false maybe perhaps probably clearly simply please sorry hello hi goodbye welcome
""".split())


def content_words(text: str):
    return {w for w in normalize(text).split() if len(w) > 2}


def question_type(question: str) -> str:
    """Tipologia usata dal verificatore (diversa dalle 4 classi AiClient)."""
    import re
    n = normalize(question)
    if re.search(r"\b(quanto|quanti|quante|how many|how much|percent|"
                 r"probabilita|probabilità|probability|calcola|calculate|"
                 r"risolvi|solve)\b", n):
        return "num"
    if re.search(r"\b(chi|who)\b", n):
        return "who"
    if re.search(r"\b(dove|where)\b", n):
        return "where"
    if re.search(r"\b(quando|when|in che anno)\b", n):
        return "when"
    if re.search(r"\b(come|how)\b", n):
        return "how"
    if re.search(r"\b(perche|perché|why)\b", n):
        return "why"
    if re.search(r"\b(spiega|spiegami|explain|describe|descrivi|riassumi|"
                 r"summarize)\b", n):
        return "explain"
    if re.search(r"\b(consigliami|recommend|che ne pensi|raccontami|"
                 r"scrivimi|write|tell me|help me|puoi tradurre|translate|"
                 r"parlami|dimmi|inventa)\b", n):
        return "open"
    if re.search(r"\b(capitale|capital)\b", n):
        return "capital"
    return "fact"


def has_digit(text: str) -> bool:
    import re
    return bool(re.search(r"\d", text))


def has_number(text: str) -> bool:
    import re
    low = text.lower()
    return has_digit(text) or any(
        re.search(r"\b" + w + r"\b", low) for w in NUMBER_WORDS)


def proper_tokens(text: str):
    """Token capitalizzati che non sono parole comuni."""
    import re
    toks = re.findall(r"[A-ZÀ-Ý][\wÀ-ÿ'-]+", text)
    out = []
    for tok in toks:
        if len(tok) <= 1 or tok.lower() in COMMON_WORDS:
            continue
        out.append(tok)
    return out


def pair_signals(question: str, answer: str):
    import re
    qw = content_words(question)
    aw = content_words(answer)
    overlap = len(qw & aw) / max(1, len(qw)) if qw else 0.0
    low = answer.lower()
    qtype = question_type(question)
    num = has_number(answer)
    digit = has_digit(answer)
    props = proper_tokens(answer)
    # Un nome proprio che compare già nella domanda è il tema, non una
    # nuova entità che risponde (es. "Git è uno strumento utile").
    new_props = [p for p in props if p.lower() not in qw]
    code = bool(re.search(r"(def |return |[{};]|\(\)|=>|==|import |"
                          r"printf|console\.)", answer))
    proc = any(c in low for c in PROC_CUES)
    causal = any(c in low for c in CAUSAL_CUES)
    vague = any(c in low for c in VAGUE_CUES)
    refusal = any(c in low for c in REFUSALS)
    alen = len(aw)
    return {
        "qwords": qw,
        "awords": aw,
        "overlap": overlap,
        "alen": alen,
        "qtype": qtype,
        "num": num,
        "digit": digit,
        "proper": bool(props),
        "new_props": new_props,
        "code": code,
        "proc": proc,
        "causal": causal,
        "vague": vague,
        "refusal": refusal,
        "short": alen <= 4,
        "veryshort": alen <= 2,
        "empty": alen < 3,
    }


def features_pair(question: str, answer: str):
    """Feature del verificatore: testo Q/A + segnali strutturati.

    I primi 64 bucket sono riservati ai segnali (indice fisso), il testo
    usa i bucket 64..511. La stessa funzione è in decision_onnx.cpp.
    """
    vec = [0.0] * DIM
    add_verifier_text(vec, question)
    add_verifier_text(vec, answer)
    add_verifier_text(vec, answer, "a:")

    s = pair_signals(question, answer)
    overlap = s["overlap"]
    alen = s["alen"]
    qtype = s["qtype"]
    num = s["num"]
    digit = s["digit"]
    prop = s["proper"]
    code = s["code"]
    proc = s["proc"]
    causal = s["causal"]
    vague = s["vague"]

    ov_bin = min(5, int(overlap * 5 + 0.5))
    add_verifier_signal(vec, "x_ov_%d" % ov_bin, 2.0)

    len_bin = 0 if alen < 5 else 1 if alen < 20 else 2 if alen < 60 else 3
    add_verifier_signal(vec, "x_len_%d" % len_bin, 2.0)

    ratio = alen / max(1, len(s["qwords"]))
    r_bin = 0 if ratio < 0.3 else 1 if ratio < 1.0 else 2 if ratio < 3.0 else 3
    add_verifier_signal(vec, "x_ratio_%d" % r_bin, 2.0)

    if s["refusal"]:
        add_verifier_signal(vec, "x_refusal", 3.0)
    if s["empty"]:
        add_verifier_signal(vec, "x_empty", 3.0)

    add_verifier_signal(vec, "x_q_" + qtype, 1.5)
    if num:
        add_verifier_signal(vec, "x_a_num", 2.0)
    if digit:
        add_verifier_signal(vec, "x_a_digit", 2.0)
    if prop:
        add_verifier_signal(vec, "x_a_proper", 2.0)
    if code:
        add_verifier_signal(vec, "x_a_code", 2.0)
    if proc:
        add_verifier_signal(vec, "x_a_proc", 1.5)
    if causal:
        add_verifier_signal(vec, "x_a_causal", 1.5)
    if vague:
        add_verifier_signal(vec, "x_a_vague", 2.0)
    if s["short"]:
        add_verifier_signal(vec, "x_a_short", 1.5)
    if s["veryshort"]:
        add_verifier_signal(vec, "x_a_veryshort", 1.0)

    if qtype == "num":
        add_verifier_signal(vec, "x_num_num" if num else "x_num_nonum", 3.0)
        add_verifier_signal(vec, "x_num_digit" if digit else "x_num_nodigit", 3.0)
    if qtype in ("who", "where", "fact"):
        if prop:
            add_verifier_signal(vec, "x_%s_proper" % qtype, 3.0)
        elif qtype == "fact" and num:
            add_verifier_signal(vec, "x_fact_num", 3.0)
        else:
            add_verifier_signal(vec, "x_%s_noproper" % qtype, 2.0)
    if qtype == "when":
        add_verifier_signal(vec, "x_when_num" if num else "x_when_nonum", 3.0)
    if qtype == "how":
        add_verifier_signal(vec, "x_how_proc" if (proc or code)
                            else "x_how_noproc", 3.0)
    if qtype == "why":
        add_verifier_signal(vec, "x_why_causal" if causal
                            else "x_why_nocausal", 3.0)
    if qtype == "explain":
        add_verifier_signal(vec, "x_explain_long" if alen >= 5
                            else "x_explain_short", 3.0)
    if qtype == "open":
        add_verifier_signal(vec, "x_open_long" if alen >= 4
                            else "x_open_short", 2.0)
    if s["short"] and prop:
        add_verifier_signal(vec, "x_short_proper", 2.0)
    if s["short"] and num:
        add_verifier_signal(vec, "x_short_num", 2.0)
    if vague and not (num or prop or code):
        add_verifier_signal(vec, "x_vague_neg", 2.5)
    if overlap == 0 and s["short"] and (num or prop):
        add_verifier_signal(vec, "x_noov_short_specific", 4.0)
    if overlap == 0 and s["short"] and prop:
        add_verifier_signal(vec, "x_short_proper_noov", 3.0)
    if overlap == 0 and s["short"] and num:
        add_verifier_signal(vec, "x_short_num_noov", 3.0)
    if overlap == 0 and s["short"] and not (num or prop):
        add_verifier_signal(vec, "x_noov_short_generic", 2.0)
    if overlap > 0.25 and vague and not (num or prop or code):
        add_verifier_signal(vec, "x_overlap_vague", 2.0)

    return l2(vec)


def relevance_guard(question: str, answer: str):
    """Guardie deterministiche applicate dopo il modello ONNX.

    Ritorna (label, reason) se una regola decide con certezza, altrimenti
    (None, ""). Non è un fact-checker: non può sapere che "Berlino" non è
    la capitale della Francia.
    """
    s = pair_signals(question, answer)
    if not answer.strip() or s["refusal"]:
        return "N", "rifiuto o risposta vuota"
    if s["qtype"] == "num":
        if s["digit"]:
            return "Y", "domanda numerica con cifre"
        if s["num"] and s["alen"] <= 2:
            return "Y", "domanda numerica con numero"
        return "N", "domanda numerica senza cifre"
    if s["qtype"] == "capital":
        if s["new_props"]:
            return "Y", "capitale indicata"
        return "N", "nessuna capitale nella risposta"
    if s["veryshort"]:
        if s["digit"] or s["new_props"]:
            return "Y", "risposta breve specifica"
        return "N", "risposta breve generica"
    if s["vague"] and not (s["digit"] or s["new_props"] or s["code"]
                           or s["proc"]):
        return "N", "risposta generica"
    if s["qtype"] in ("how", "why", "explain"):
        if s["code"] or s["proc"] or s["causal"]:
            return "Y", "risposta procedurale o causale"
        if s["alen"] >= 6 and not s["vague"]:
            return "Y", "spiegazione articolata"
        return "N", "manca procedura/spiegazione"
    if s["qtype"] == "open":
        if s["alen"] >= 3 and not s["vague"]:
            return "Y", "risposta aperta articolata"
        return "N", "risposta aperta troppo generica"
    return None, ""


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


SHORT_FACT_QUESTIONS = [
    ("qual è la capitale del Giappone", "Tokyo"),
    ("qual è la capitale della Francia", "Parigi"),
    ("qual è la capitale della Spagna", "Madrid"),
    ("qual è la capitale della Germania", "Berlino"),
    ("chi ha dipinto la Gioconda", "Leonardo da Vinci"),
    ("chi ha scritto la Divina Commedia", "Dante Alighieri"),
    ("chi ha scoperto la penicillina", "Alexander Fleming"),
    ("quanto fa 17 per 23", "391"),
    ("quanto fa 12 per 12", "144"),
    ("quanto fa 2+2", "4"),
    ("qual è il simbolo chimico dell'acqua", "H2O"),
    ("qual è il simbolo chimico del sodio", "Na"),
    ("in che anno è caduto l'impero romano d'Occidente", "476"),
    ("quanti giorni ha un anno bisestile", "366"),
    ("quanti metri ci sono in un chilometro", "1000"),
    ("what is the capital of Japan", "Tokyo"),
    ("what is the capital of France", "Paris"),
    ("who painted the Mona Lisa", "Leonardo da Vinci"),
    ("what is 15 percent of 240", "36"),
    ("what is the chemical symbol for water", "H2O"),
    ("how many days are in a leap year", "366"),
]

GENERIC_NEGATIVES = [
    "La domanda riguarda {kw}.",
    "{kw} è un argomento importante e interessante.",
    "Questo riguarda {kw} e altri aspetti simili.",
    "Ci sono molti fattori da considerare su {kw}.",
]

# Parafrasi positive non coperte dai template: servono a insegnare che
# una risposta breve e specifica è valida anche senza ripetere la domanda.
MANUAL_POSITIVES = [
    ("come si ordina una lista in Python",
     "Puoi usare sorted() per crearne una nuova, oppure il metodo sort() "
     "per modificarla sul posto."),
    ("qual è la capitale del Giappone", "Tokyo è la città che cercavi."),
    ("quanto fa 17 per 23", "Il prodotto è 391."),
    ("come si cura il mal di testa",
     "Riposo, idratazione e un analgesico da banco possono aiutare; "
     "se persiste consulta un medico."),
    ("spiega la fotosintesi",
     "Le piante catturano la luce solare e la usano per produrre zuccheri, "
     "rilasciando ossigeno."),
    ("what is the capital of France", "Paris is the city you are looking for."),
    ("why is the sky blue",
     "Shorter wavelengths of sunlight are scattered more strongly by air "
     "molecules."),
    ("come si usa git rebase",
     "Sposta i tuoi commit su una nuova base e richiede la risoluzione dei "
     "conflitti."),
    ("calcola l'area di un cerchio di raggio 3",
     "Il valore è 28,27 unità quadrate."),
    ("chi ha scritto la Divina Commedia", "L'autore è Dante Alighieri."),
]


def verification_dataset():
    """Dataset verificatore: positivi pertinenti + negativi difficili.

    I negativi includono rifiuti, risposte vuote, fuori tema e risposte
    che ripetono parole della domanda senza rispondere. Non ci sono
    negativi basati su fatti sbagliati: un verificatore lessicale non può
    distinguerli e non deve fingere di farlo.
    """
    rows = []

    def add(q, a, label):
        rows.append((features_pair(q, a), label))

    for q, kw in VERIFY_QUESTIONS:
        for tmpl in POSITIVE_TEMPLATES:
            add(q, tmpl.format(kw=kw), 1)
        for ans in NEGATIVE_ANSWERS:
            add(q, ans, 0)
        for tmpl in GENERIC_NEGATIVES[:2]:
            add(q, tmpl.format(kw=kw), 0)

        t = question_type(q)
        if t == "how":
            for ans in (f"{kw} è uno strumento utile.",
                        f"{kw} è un argomento importante.",
                        "Questo è un linguaggio di programmazione.",
                        "Si tratta di un sistema molto diffuso."):
                add(q, ans, 0)
            if "python" in normalize(q):
                add(q, "Python è un linguaggio di programmazione.", 0)
        if t == "num":
            for ans in ("Il risultato è un numero.",
                        "La risposta dipende dai valori.",
                        "Non serve un calcolo preciso.",
                        "Il valore è descritto nel testo."):
                add(q, ans, 0)
        if t == "fact":
            for ans in (f"{kw} è un paese asiatico.",
                        f"{kw} è una città importante.",
                        "È un argomento molto interessante.",
                        "Si tratta di un elemento naturale."):
                add(q, ans, 0)
            words = normalize(q).split()
            subj = words[-1] if words else kw
            for ans in (f"{subj.capitalize()} è un paese.",
                        f"{subj.capitalize()} è una città importante.",
                        f"{subj.capitalize()} è un argomento interessante."):
                add(q, ans, 0)

    for q, a in SHORT_FACT_QUESTIONS:
        for ans in (a + ".", "La risposta è " + a + ".",
                    a + " è corretto.", "Sì: " + a + "."):
            add(q, ans, 1)
        for ans in ("Non lo so.", "Ok.", "Basta.", "Non posso rispondere."):
            add(q, ans, 0)
        words = normalize(q).split()
        kw = " ".join(words[-3:])
        for tmpl in GENERIC_NEGATIVES[:2]:
            add(q, tmpl.format(kw=kw), 0)
        if question_type(q) == "num":
            for ans in ("Il risultato è un numero.",
                        "La figura ha un raggio e un diametro."):
                add(q, ans, 0)

    for q, a in MANUAL_POSITIVES:
        add(q, a, 1)

    return (np.asarray([r[0] for r in rows], dtype=np.float32),
            np.asarray([r[1] for r in rows], dtype=np.int64))


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


def write_fixture(path):
    """Fixture JSON per test_decision_onnx (parità C++ ↔ Python)."""
    texts = {
        "ascii": "scrivi una funzione python che legge un file json",
        "accented": "perché il cielo è blu e la radice di 2 è irrazionale",
    }
    pairs = {
        "relevant": ("come si ordina una lista in python",
                     "Per ordinare una lista in python usa sorted e verifica "
                     "il risultato."),
        "refusal": ("qual è la capitale della Francia", "Non lo so."),
        "empty": ("spiegami il teorema di pitagora", "Ok."),
        "short_factual": ("qual è la capitale della Spagna", "Madrid."),
        "numeric": ("quanto fa 24 per 3", "72."),
        "code": ("write a JavaScript function to sum two numbers",
                 "function sum(a,b){return a+b;}"),
        "how_generic": ("come si crea un branch in git",
                        "Git è uno strumento utile."),
        "capital_generic": ("qual è la capitale della Spagna",
                            "La Spagna è un paese europeo."),
        "numeric_generic": ("calcola il perimetro di un quadrato di lato 5",
                            "Il quadrato ha quattro lati."),
        "explain": ("explain photosynthesis",
                    "Plants use sunlight, water and CO2 to make glucose and "
                    "release oxygen."),
        "why": ("why do leaves turn yellow in autumn",
                "Chlorophyll breaks down, revealing yellow carotenoid pigments."),
        "short_negative": ("come si cura il raffreddore", "Basta."),
        "english_generic": ("how do I reverse a list in Python", "Sure."),
        "accented_proper": ("chi ha scritto la Divina Commedia",
                            "L'autore è Dante Alighieri."),
        "accented_why": ("perché il cielo è blu",
                         "La luce blu viene diffusa più delle altre lunghezze d'onda."),
        "accented_numeric": ("qual è la probabilità di estrarre due assi",
                             "Circa 0,45%."),
        "where": ("dove si trova la Torre Eiffel",
                  "La Torre Eiffel si trova a Parigi."),
        "when": ("in che anno è caduto l'impero romano d'Occidente",
                 "Nel 476."),
        "fact": ("what is the chemical symbol for water", "H2O."),
        "open": ("consigliami un film", "Ti consiglio 'Il nome della rosa'."),
        "wrong_fact": ("what is the capital of France", "Berlin."),
    }

    root = {
        "feature_dim": DIM,
        "texts": {
            key: {"text": text, "features": features_text(text)}
            for key, text in texts.items()
        },
        "pairs": {
            key: {
                "question": q,
                "answer": a,
                "features": features_pair(q, a),
            }
            for key, (q, a) in pairs.items()
        },
        # Casi in cui una guardia deterministica decide (o deve decidere)
        # indipendentemente dal modello. "model" = lascia decidere il modello.
        "guards": [
            {"question": "qual è la capitale della Spagna", "answer": "Madrid.",
             "expected": "Y", "reason_contains": "capitale indicata"},
            {"question": "quanto fa 24 per 3", "answer": "72.",
             "expected": "Y", "reason_contains": "numerica con cifre"},
            {"question": "write a JavaScript function to sum two numbers",
             "answer": "function sum(a,b){return a+b;}",
             "expected": "Y", "reason_contains": "aperta"},
            {"question": "come si crea un branch in git",
             "answer": "Git è uno strumento utile.",
             "expected": "N", "reason_contains": "generica"},
            {"question": "qual è la capitale della Spagna",
             "answer": "La Spagna è un paese europeo.",
             "expected": "N", "reason_contains": "capitale"},
            {"question": "calcola il perimetro di un quadrato di lato 5",
             "answer": "Il quadrato ha quattro lati.",
             "expected": "N", "reason_contains": "numerica senza cifre"},
            {"question": "explain photosynthesis",
             "answer": "Plants use sunlight, water and CO2 to make glucose "
                       "and release oxygen.",
             "expected": "Y", "reason_contains": "spiegazione"},
            {"question": "why do leaves turn yellow in autumn",
             "answer": "Chlorophyll breaks down, revealing yellow carotenoid "
                       "pigments.",
             "expected": "Y", "reason_contains": "causale"},
            {"question": "come si cura il raffreddore", "answer": "Basta.",
             "expected": "N", "reason_contains": "breve generica"},
            {"question": "how do I reverse a list in Python", "answer": "Sure.",
             "expected": "N", "reason_contains": "breve generica"},
            # Fuori scope: il verificatore lessicale non è un fact-checker.
            {"question": "what is the capital of France", "answer": "Berlin.",
             "expected": "Y", "reason_contains": "capitale"},
        ],
    }
    with open(path, "w", encoding="utf-8") as f:
        json.dump(root, f, indent=2, ensure_ascii=False)
        f.write("\n")


def main():
    ap = argparse.ArgumentParser()
    ap.add_argument("--out", default=None,
                    help="cartella di output (default: Frameworks/onnx/models)")
    ap.add_argument("--check", action="store_true",
                    help="stampa alcune predizioni di controllo")
    ap.add_argument("--fixture", default=None, metavar="FILE",
                    help="scrive la fixture di parità per test_decision_onnx")
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
    Wv, bv = train_softmax(Xv, yv, len(VERIFIER_LABELS), epochs=1600)
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

    if args.fixture:
        write_fixture(args.fixture)
        print("fixture scritta in %s" % args.fixture)

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
