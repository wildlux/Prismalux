# Best Practice — Gestione LLM (Prismalux Qt GUI)

Pattern per interagire correttamente con Ollama e llama-server tramite AiClient.
Ogni regola ha un riferimento al file dove il pattern è implementato.

---

## 1. Classificazione query — usare classifyQuery, mai QueryAuto fisso

**Regola**: il wrapper `chat(sys, msg)` deve classificare automaticamente la query.
`QueryAuto` fisso spreca token e ignora l'ottimizzazione think/num_predict.

```cpp
// SBAGLIATO — QueryAuto fisso per tutte le chiamate
void MyPage::sendQuestion(const QString& q) {
    m_ai->chat(systemPrompt, q);  // chiama internamente QueryAuto
}

// CORRETTO — classifica e applica il tipo corretto
void MyPage::sendQuestion(const QString& q) {
    m_ai->chat(systemPrompt, q, {}, AiClient::classifyQuery(q));
}
```

**Tipi disponibili**:
| Tipo | Condizione | Effetto |
|------|-----------|---------|
| `QuerySimple` | ≤30 chars, nessuna keyword | `think=false`, `num_predict=512` |
| `QueryComplex` | keyword tecnica o query lunga | `think=true` (se modello supporta), `num_predict` dal config |
| `QueryAuto` | default legacy | nessun think, `num_predict` dal config |

**Riferimento**: `gui/ai_client.h` ~60, `gui/ai_client.cpp` — `classifyQuery()` e `chat()` wrapper.

---

## 2. Request ID — filtrare risposte stale

**Regola**: ogni pagina che può ricevere risposte da più richieste sovrapposti
(es. cambio tab durante streaming) deve catturare il `reqId` e filtrare i callback.

```cpp
// In setupAi():
connect(m_ai, &AiClient::token, this, [this](const QString& tok) {
    if (m_ai->currentReqId() != m_myReqId) return;  // risposta stale
    m_log->insertPlainText(tok);
});

// In sendRequest():
m_ai->chat(sys, msg);
m_myReqId = m_ai->currentReqId();  // cattura DOPO chat()
```

**Riferimento**: `gui/pages/lavoro_page.cpp` — `m_myReqId` con filtro `currentReqId() != m_myReqId`.
**Causa bug se non rispettato**: token di richieste precedenti appaiono nella risposta corrente.

---

## 3. Pattern connHolder — segnale one-shot senza accumulazione

**Regola**: per ricevere un segnale una sola volta (es. modelsReady dopo un fetch),
usare connHolder che si auto-distrugge. Evita connessioni permanenti che si accumulano.

```cpp
auto* connHolder = new QObject(this);
connect(m_ai, &AiClient::modelsReady, connHolder,
    [this, connHolder](const QStringList& models) {
        populateCombo(models);
        connHolder->deleteLater();  // disconnette automaticamente
    });
m_ai->fetchModels();
```

**Riferimento**: `gui/mainwindow.cpp` — `applyBackend()`.

---

## 4. Cambio modello senza cambio backend

**Regola**: per aggiornare solo il modello, preservare backend/host/port correnti.

```cpp
// SBAGLIATO — resetta a Ollama, rompe llama-server
m_ai->setBackend(AiClient::Ollama, "127.0.0.1", 11434, nuovoModello);

// CORRETTO
m_ai->setBackend(m_ai->backend(), m_ai->host(), m_ai->port(), nuovoModello);
```

**Riferimento**: `gui/pages/agenti_page_pipeline.cpp` — `runAgent()`.

---

## 5. Dual endpoint — /api/tags vs /v1/models

**Regola**: le chiamate di lista modelli devono usare l'endpoint corretto per
il backend attivo. Ollama e llama-server hanno API diverse.

```cpp
if (m_ai->backend() == AiClient::Ollama) {
    // GET /api/tags → parsing models[].{name, size}
    url = QString("http://%1:%2/api/tags").arg(host).arg(port);
} else {
    // GET /v1/models → parsing data[].{id}, size sempre 0
    url = QString("http://%1:%2/v1/models").arg(host).arg(port);
}
```

**Riferimento**: `gui/pages/agenti_page_models.cpp` — `autoAssignRoles()`.

---

## 6. Storia conversazione — compressione obbligatoria per chat lunghe

**Regola**: le pagine con chat multi-turno devono comprimere la storia quando
supera `kMaxRecentTurns`. Mandare tutta la storia all'LLM è lento e spreca token.

```cpp
constexpr int kMaxRecentTurns = 6;  // mantieni ultimi 3 scambi user/assistant

void MyPage::compressHistory() {
    if (m_history.size() <= kMaxRecentTurns) return;
    const int toCompress = m_history.size() - kMaxRecentTurns;
    // Costruisci riassunto dai turni vecchi
    QStringList lines;
    for (int i = 0; i < toCompress; ++i) {
        const auto& turn = m_history[i];
        lines << QString("[%1]: %2")
                 .arg(turn["role"].toString())
                 .arg(turn["content"].toString().left(120));
    }
    m_historySummary += lines.join("\n");
    m_history = m_history.mid(toCompress);
}
```

**Riferimento**: `gui/pages/oracolo_page.cpp` ~546 — `compressHistory()`.
**Nota**: pagine con chiamate singole (senza storia) non ne hanno bisogno.

---

## 7. OpMode — state machine esplicita, guard Idle obbligatoria

**Regola**: ogni pagina con pipeline multi-step deve usare un enum `OpMode`
e ritornare immediatamente se `m_opMode == Idle` in `onFinished()`.
Senza guard, un `finished(empty)` stale può lanciare agenti non richiesti.

```cpp
enum class OpMode { Idle, Step1, Step2, Byzantine };
OpMode m_opMode = OpMode::Idle;

void onFinished(const QString& full) {
    if (m_opMode == OpMode::Idle) return;  // guard obbligatoria
    switch (m_opMode) {
        case OpMode::Step1: startStep2(full); break;
        case OpMode::Byzantine: runJudge(full); break;
        default: break;
    }
}
```

**Riferimento**: `gui/pages/agenti_page_stream.cpp` — `onFinished()`.
**Bug senza guard**: modello "rotto" emette sia `error()` che `finished(empty)` →
`finished` lancia Byzantine con stringa vuota → loop di agenti.

---

## 8. Think mode — budget doppio per modelli con reasoning

**Regola**: i modelli think-capable (qwen3, deepseek-r1) consumano quasi tutto
`num_predict` nel blocco `<think>`. Per `QueryComplex`, raddoppiare il budget.

```cpp
// In ai_client.cpp — costruzione payload
if (qt == QueryComplex && isThinkCapable(m_model)) {
    params["num_predict"] = m_config.num_predict * 2;
    params["think"] = true;
} else if (qt == QuerySimple) {
    params["num_predict"] = 512;
    params["think"] = false;
}
```

**Riferimento**: `gui/ai_client.cpp` — gestione `QueryComplex` con moltiplicatore.
**Causa bug senza**: qwen3 usa 3900/4096 token in reasoning → `message.content` = 50 chars.

---

## 9. Embedding — modello dedicato, endpoint separato

**Regola**: gli embedding richiedono un modello dedicato (es. `nomic-embed-text`),
non il modello di chat. `fetchEmbedding()` usa `/api/embeddings` (Ollama) o
`/v1/embeddings` (llama-server). Con `LlamaLocal` emette subito `embeddingError`.

```cpp
// Corretto: fetchEmbedding usa il modello embedding, non m_model
m_ai->fetchEmbedding(text);  // endpoint separato, modello separato

// SBAGLIATO: non usare chat() per ottenere rappresentazioni vettoriali
m_ai->chat("Trasforma in vettore:", text);  // non produce embeddings reali
```

**Avviso UX da mostrare**: se embedding fallisce → `"Installa nomic-embed-text:\n  ollama pull nomic-embed-text"`.

**Riferimento**: `gui/ai_client.h` — `fetchEmbedding()`, segnali `embeddingReady` / `embeddingError`.

---

## 10. RAG injection — limitare lunghezza chunk nel system prompt

**Regola**: troncare ogni chunk a ~800 caratteri prima di iniettarlo nel system prompt.
Chunk non limitati da PDF grandi possono occupare metà del contesto disponibile.

```cpp
// In ogni pagina che usa RAG
const auto results = m_rag.search(embedding, 3);
QStringList context;
for (const auto& r : results) {
    QString chunk = r.text.left(800);
    if (r.text.size() > 800) chunk += "...";
    context << chunk;
}
const QString sysWith = systemPrompt + "\n\nContesto:\n" + context.join("\n---\n");
m_ai->chat(sysWith, userMsg);
```

**Riferimento**: da applicare ovunque si chiama `m_rag.search()` (strumenti_page, oracolo_page).

---

## 11. generate() vs chat() — quando usare quale

**Regola**: `generate()` usa `/api/generate` (Ollama), è più leggero per query
RAG singole senza storia. `chat()` usa `/api/chat` con storia. Per query one-shot
con contesto RAG: preferire `generate()`.

```cpp
// Query one-shot con RAG (no storia necessaria)
m_ai->generate(systemWithContext, userQuestion, AiClient::QueryComplex);

// Chat con storia (oracolo, tutor multi-turno)
m_ai->chat(systemPrompt, userMessage, m_history, AiClient::classifyQuery(userMessage));
```

**Nota**: `generate()` fa fallback automatico a `chat()` se backend != Ollama.

**Riferimento**: `gui/ai_client.h` ~83 — documentazione `generate()`.

---

## 12. Auto-assign ruoli — disabilitare con llama-server mono-modello

**Regola**: con llama-server è caricato un solo modello alla volta. L'auto-assign
non può scegliere modelli diversi per ruoli diversi. Disabilitare il pulsante
o mostrare un messaggio esplicativo.

```cpp
const bool multiModel = (m_ai->backend() == AiClient::Ollama);
m_btnAutoAssign->setEnabled(multiModel);
if (!multiModel)
    m_btnAutoAssign->setToolTip("Auto-assign richiede Ollama (più modelli disponibili).");
```

**Riferimento**: `gui/pages/agenti_page_ui.cpp` — TODO identificato in CLAUDE.md.

---

## 13. Abort — sempre gestire il segnale aborted separatamente

**Regola**: `m_ai->abort()` NON emette `finished()`. Se il codice gestisce
solo `finished`, lo stop non pulisce lo stato correttamente.

```cpp
// Obbligatorio in ogni pagina con streaming
connect(m_ai, &AiClient::aborted, this, [this]() {
    m_opMode = OpMode::Idle;
    updateButtons();
    // Pulisci eventuale testo parziale nel log
});

// NON fare:
// connect(m_ai, &AiClient::finished, ...) e aspettarsi che scatti dopo abort
```

**Riferimento**: `gui/pages/agenti_page_stream.cpp` — `onAborted()`.

---

## 14. Prompt system — corto e preciso

**Regola**: i system prompt devono essere sotto i 200 token. Prompt lunghi
consumano contesto prezioso e rallentano l'inferenza. Il modello risponde
meglio a istruzioni concise.

```cpp
// SBAGLIATO — 2KB di istruzioni generiche
const QString sys = "Sei un assistente AI avanzato con conoscenza profonda di "
                    "matematica, fisica, chimica... [500 parole] ...";

// CORRETTO — 20 parole essenziali
const QString sys = "Rispondi in italiano. Sii preciso e conciso. "
                    "Per matematica: usa LaTeX inline ($formula$).";
```

**Riferimento**: `gui/pages/` — system prompt nei vari `runAgent()`, `runOracolo()`.
Vedi anche il pattern tutor C: `_SYS_TUTOR[]` = 18 parole (da MEMORY.md).

---

## 15. Modello non disponibile — graceful degradation

**Regola**: se il modello selezionato non è nella lista Ollama, non crashare.
Continuare con il primo modello disponibile o mostrare un avviso specifico.

```cpp
connect(m_ai, &AiClient::modelsReady, this, [this](const QStringList& models) {
    if (models.isEmpty()) {
        showWarning("Ollama non risponde o nessun modello installato.\n"
                    "Avvia Ollama e scarica un modello: ollama pull qwen3:4b");
        return;
    }
    const QString preferred = m_settings.value("ai/activeModel").toString();
    const QString toUse = models.contains(preferred) ? preferred : models.first();
    m_ai->setBackend(m_ai->backend(), m_ai->host(), m_ai->port(), toUse);
});
```

**Riferimento**: `gui/mainwindow.cpp` — `applyBackend()` con fallback al primo modello.
