# TODO — Test Team Pipeline MOE AI

> Modelli disponibili (localhost:11434):
> - **Reasoning/General**: `qwen3.5:4b`, `mistral:7b-instruct`, `qwen3:4b`, `gemma3:4b`
> - **Coding**: `qwen2.5-coder:7b`
> - **Matematica**: `qwen2-math:latest`, `deepseek-r1:1.5b`
> - **Cloud**: `qwen3.5:cloud`, `qwen3-coder-next:cloud`, `kimi-k2.5:cloud`
>
> Stato: ✅ = eseguito · 🔄 = in corso · ⬜ = da eseguire

---

## Test da eseguire

### ⬜ T01 — Pipeline BubbleSort (Coding, 4 agenti)
**Obiettivo del team**: Implementare BubbleSort in Python con documentazione e test.

| Agente | Ruolo | Modello |
|--------|-------|---------|
| 1 | Architetto | `qwen3.5:4b` |
| 2 | Sviluppatore | `qwen2.5-coder:7b` |
| 3 | Revisore | `qwen3.5:4b` |
| 4 | Tester | `qwen2.5-coder:7b` |

**Task**: *"Implementa l'algoritmo BubbleSort in Python con: firma tipizzata, docstring, gestione lista vuota, e test pytest con almeno 4 casi (lista ordinata, inversa, duplicati, vuota)."*

**Criteri di successo**:
- Agente 2 produce `def bubble_sort(arr: list) -> list`
- Agente 3 emette verdetto APPROVATO o RICHIEDE MODIFICHE
- Agente 4 produce almeno 4 funzioni `def test_`
- Ogni agente cita l'obiettivo del team nel ragionamento

---

### ⬜ T02 — REST API Flask (Coding, 4 agenti)
**Obiettivo del team**: Progettare e implementare una micro-API Flask per una lista TODO.

| Agente | Ruolo | Modello |
|--------|-------|---------|
| 1 | Architetto API | `qwen3.5:4b` |
| 2 | Backend Dev | `qwen2.5-coder:7b` |
| 3 | Security Reviewer | `mistral:7b-instruct` |
| 4 | Documentatore | `qwen3.5:4b` |

**Task**: *"Progetta e implementa una micro-API Flask con endpoint: GET /tasks, POST /tasks, DELETE /tasks/{id}. Dati in memoria (no DB). L'agente 3 deve verificare assenza di SQL injection e path traversal."*

**Criteri di successo**:
- Agente 2 produce endpoint Flask funzionanti
- Agente 3 identifica almeno un potenziale rischio sicurezza o dichiara il codice sicuro con motivazione
- Agente 4 produce documentazione OpenAPI/Swagger parziale o README chiaro

---

### ⬜ T03 — Bug Hunt & Fix (Coding, 3 agenti)
**Obiettivo del team**: Trovare, correggere e verificare i bug in un codice Python difettoso.

| Agente | Ruolo | Modello |
|--------|-------|---------|
| 1 | Debugger | `qwen3.5:4b` |
| 2 | Fixer | `qwen2.5-coder:7b` |
| 3 | Tester | `qwen2.5-coder:7b` |

**Task** (codice da analizzare):
```python
def calcola_media(numeri):
    totale = 0
    for n in numeri:
        totale += n
    return totale / len(numeri)  # BUG: ZeroDivisionError se lista vuota

def inverti_stringa(s):
    return s[::-0]  # BUG: slice errato, ritorna sempre ""

def cerca_elemento(lista, elem):
    for i in range(len(lista) + 1):  # BUG: IndexError off-by-one
        if lista[i] == elem:
            return i
    return -1
```
*"Il team deve: (1) elencare tutti i bug, (2) produrre il codice corretto, (3) scrivere test pytest che catturano ogni bug."*

**Criteri di successo**:
- Agente 1 identifica ≥ 3 bug
- Agente 2 risolve tutti i bug identificati
- Agente 3 scrive test che falliscono sul codice originale e passano sul codice corretto

---

### ⬜ T04 — Equazione Differenziale (Matematica, 3 agenti)
**Obiettivo del team**: Risolvere e verificare dy/dx = 2x con condizione iniziale y(0) = 1.

| Agente | Ruolo | Modello |
|--------|-------|---------|
| 1 | Enunciatore | `qwen2-math:latest` |
| 2 | Solver | `qwen2-math:latest` |
| 3 | Verificatore | `deepseek-r1:1.5b` |

**Task**: *"Risolvi l'ODE dy/dx = 2x, y(0)=1. L'enunciatore deve formalizzare il problema, il solver risolverlo passo per passo (metodo di separazione delle variabili), il verificatore deve controllare la soluzione derivando e sostituendo la condizione iniziale."*

**Criteri di successo**:
- Soluzione finale: `y = x² + 1`
- Agente 3 verifica: y'=2x ✓, y(0)=1 ✓
- Ogni passaggio è motivato simbolicamente

---

### ⬜ T05 — Grafi: Bipartito + Dijkstra (Matematica + Coding, 4 agenti)
**Obiettivo del team**: Analizzare un grafo, verificare bipartitezza e trovare il cammino minimo.

| Agente | Ruolo | Modello |
|--------|-------|---------|
| 1 | Analista Grafo | `qwen2-math:latest` |
| 2 | Implementatore | `qwen2.5-coder:7b` |
| 3 | Verificatore | `qwen3.5:4b` |
| 4 | Documentatore | `mistral:7b-instruct` |

**Task**: *"Dato il grafo G = {nodi: [A,B,C,D,E], archi pesati: A-B:4, A-C:2, B-D:5, C-D:1, C-E:3, D-E:2}: (1) verifica se è bipartito, (2) implementa Dijkstra da A in Python, (3) trova il cammino minimo A→E."*

**Criteri di successo**:
- Agente 1 determina correttamente se il grafo è bipartito
- Agente 2 produce implementazione Dijkstra funzionante
- Cammino minimo A→E = A→C→D→E con costo 5

---

### ⬜ T06 — Crittografia RSA Base (Matematica, 3 agenti)
**Obiettivo del team**: Spiegare, calcolare e verificare un esempio completo di RSA con piccoli numeri.

| Agente | Ruolo | Modello |
|--------|-------|---------|
| 1 | Teorico | `qwen2-math:latest` |
| 2 | Calcolatore | `qwen2-math:latest` |
| 3 | Verificatore | `deepseek-r1:1.5b` |

**Task**: *"Esegui RSA completo con p=11, q=13: (1) calcola n, φ(n), (2) scegli e coprimo con φ(n), (3) calcola d (inverso modulare di e), (4) cifra il messaggio m=7 con la chiave pubblica, (5) decifra e verifica che si ottenga m=7."*

**Criteri di successo**:
- n = 143, φ(n) = 120
- e e d soddisfano e·d ≡ 1 (mod 120)
- Cifratura e decifratura producono il valore originale

---

### ⬜ T07 — Ricerca Paper AI + Sintesi (MCP Hugging Face, 3 agenti)
**Obiettivo del team**: Ricercare, analizzare e sintetizzare lo stato dell'arte su "Mixture of Experts".

| Agente | Ruolo | Modello | Tool |
|--------|-------|---------|------|
| 1 | Ricercatore | `qwen3.5:4b` | MCP `hf_doc_search` / `paper_search` |
| 2 | Analista | `mistral:7b-instruct` | lettura output agente 1 |
| 3 | Sintetizzatore | `qwen3.5:4b` | — |

**Task**: *"Cerca i paper più importanti su 'Mixture of Experts in LLM' (ultimi 2 anni), identifica le architetture principali (Mixtral, DeepSeek-MoE, ecc.) e produci una sintesi di 300 parole con: motivazione MOE, numero esperti tipico, vantaggi vs modelli densi."*

**Criteri di successo**:
- Agente 1 trova ≥ 3 paper/modelli rilevanti
- Agente 2 identifica almeno 2 architetture MOE reali
- Agente 3 produce sintesi ≥ 200 parole con confronto quantitativo

---

### ⬜ T08 — Ricerca Lavoro + Ottimizzazione CV (MCP Indeed, 3 agenti)
**Obiettivo del team**: Trovare posizioni AI/ML in Italia e ottimizzare un CV per la migliore.

| Agente | Ruolo | Modello | Tool |
|--------|-------|---------|------|
| 1 | Ricercatore | `qwen3.5:4b` | MCP `indeed_search_jobs` |
| 2 | Analista Fit | `mistral:7b-instruct` | — |
| 3 | CV Coach | `qwen3.5:4b` | — |

**Task**: *"Cerca posizioni 'Machine Learning Engineer' o 'AI Developer' in Italia (remote ok). Analizza i requisiti più comuni. Proponi 5 punti concreti per migliorare un CV generico di un developer Python con 3 anni di esperienza."*

**Criteri di successo**:
- Agente 1 recupera ≥ 3 annunci concreti (o almeno titoli/aziende reali)
- Agente 2 identifica i requisiti più richiesti (es. PyTorch, LLM fine-tuning, MLOps)
- Agente 3 produce ≥ 5 suggerimenti specifici e non generici

---

### ⬜ T09 — Security Review: SQL Injection (Coding + Sicurezza, 4 agenti)
**Obiettivo del team**: Analizzare, correggere e testare vulnerabilità di sicurezza in un'API Python.

| Agente | Ruolo | Modello |
|--------|-------|---------|
| 1 | Security Analyst | `qwen3.5:4b` |
| 2 | Secure Coder | `qwen2.5-coder:7b` |
| 3 | Penetration Tester | `qwen2.5-coder:7b` |
| 4 | Code Reviewer | `mistral:7b-instruct` |

**Task** (codice vulnerabile):
```python
import sqlite3
from flask import Flask, request

app = Flask(__name__)

@app.route('/user')
def get_user():
    username = request.args.get('username')
    conn = sqlite3.connect('users.db')
    query = f"SELECT * FROM users WHERE name = '{username}'"  # SQL INJECTION
    result = conn.execute(query).fetchall()
    return str(result)

@app.route('/file')
def read_file():
    filename = request.args.get('name')
    with open(f"/var/data/{filename}") as f:  # PATH TRAVERSAL
        return f.read()
```
*"(1) elenca tutte le vulnerabilità con CVE o CWE di riferimento, (2) riscrivi il codice sicuro con parameterized queries e path sanitization, (3) scrivi exploit PoC per dimostrare le vulnerabilità originali in ambiente di test, (4) verifica che il codice corretto non sia più vulnerabile agli stessi exploit."*

**Criteri di successo**:
- Agente 1 identifica SQL Injection (CWE-89) e Path Traversal (CWE-22)
- Agente 2 usa query parametrizzate e `os.path.realpath` + whitelist
- Agente 3 produce PoC con payload come `' OR '1'='1` e `../../etc/passwd`
- Agente 4 conferma che il codice corretto resiste agli stessi payload

---

### ⬜ T10 — LRU Cache: Design → Impl → Test → Docs (Pipeline completa, 4 agenti)
**Obiettivo del team**: Costruire una cache LRU thread-safe in Python dalla progettazione alla documentazione.

| Agente | Ruolo | Modello |
|--------|-------|---------|
| 1 | Architetto | `qwen3.5:4b` |
| 2 | Implementatore | `qwen2.5-coder:7b` |
| 3 | Tester | `qwen2.5-coder:7b` |
| 4 | Documentatore | `mistral:7b-instruct` |

**Task**: *"Implementa una LRU Cache (Least Recently Used) thread-safe in Python puro (no functools.lru_cache): capacità configurabile, metodi get(key)/put(key,val), eviction O(1) tramite OrderedDict + threading.Lock. Testa concorrenza con 3+ thread simultanei. Documenta con docstrings e README."*

**Criteri di successo**:
- Agente 1 definisce l'interfaccia e spiega la scelta di OrderedDict vs doubly-linked list
- Agente 2 implementa `__init__`, `get`, `put` con Lock correttamente posizionato
- Agente 3 scrive test di concorrenza con `threading.Thread` (≥ 3 thread)
- Agente 4 produce README con complessità O(1) spiegata e esempio d'uso

---

## Test eseguiti

*(Sezione popolata automaticamente dopo ogni esecuzione)*

---
