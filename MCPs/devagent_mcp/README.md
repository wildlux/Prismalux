# DevAgent MCP — Prismalux

Agente AI locale che modifica il codice di Prismalux in autonomia.
Riceve una task in linguaggio naturale, trova i file rilevanti, genera un diff con Ollama e lo applica compilando il progetto. Riprova fino a 3 volte in caso di errori di compilazione.

## Comunicazione con Qt

IPC JSON su stdin/stdout (stesso protocollo del Telegram bot).

**Input** (una riga JSON per richiesta):
```json
{"task": "Aggiungi un pulsante Reset nella SecurityAnalyzerPage",
 "model": "deepseek-coder:6.7b",
 "project_root": "/home/wildlux/Desktop/Prismalux"}
```

**Output** (righe JSON, eventi in streaming):
```json
{"event": "start",          "task": "...", "model": "...", "project_root": "..."}
{"event": "step",           "node": "read_context",   "files": ["gui/pages/..."]}
{"event": "step",           "node": "generate_patch", "preview": "--- a/...", "retries": 0}
{"event": "step",           "node": "apply_patch",    "files_modified": ["gui/pages/..."]}
{"event": "compile_output", "output": "...",           "ok": true}
{"event": "test_output",    "output": "..."}
{"event": "done",           "success": true,           "diff": "...", "message": "..."}
```

## Grafo

```
START -> read_context -> generate_patch -> apply_patch -> compile
                               ^  (errori, max 3 retry) <----------+
                               |                                    |
                               +---- compile_ok == False -----------+
                               |
                          run_tests -> done
```

## Dipendenze

**Obbligatorie** (solo stdlib Python):
- Python >= 3.10
- Ollama in esecuzione su `http://127.0.0.1:11434`
- build_gui/ gia configurato (`cmake -B build_gui gui/`)

**Opzionali** (installabili con `pip install -r requirements.txt`):
- `langgraph>=0.2.0` — grafo esplicito con routing condizionale
- `unidiff>=0.7.5` — parser diff alternativo (il server ha il proprio parser interno)

Senza le dipendenze opzionali il server usa un loop Python semplice equivalente.

## Avvio manuale (test)

```bash
cd /home/wildlux/Desktop/Prismalux/MCPs/devagent_mcp
echo '{"task":"Aggiungi log di debug in node_compile","model":"deepseek-coder:6.7b","project_root":"/home/wildlux/Desktop/Prismalux"}' | python3 server.py
```

## Integrazione Qt (AppController)

Il processo viene avviato come `QProcess` con lo stesso pattern del Telegram bot:

```cpp
m_devAgentProc = new QProcess(this);
m_devAgentProc->setProgram("python3");
m_devAgentProc->setArguments({P::root() + "/MCPs/devagent_mcp/server.py"});
m_devAgentProc->start();

// Invia task
QJsonObject req;
req["task"]         = m_taskEdit->toPlainText();
req["model"]        = m_modelCombo->currentText();
req["project_root"] = P::root();
m_devAgentProc->write((QJsonDocument(req).toJson(QJsonDocument::Compact) + "\n"));
```

## Note di sicurezza

- Il server esegue `cmake --build` e `ctest` nel progetto locale — non esegue comandi arbitrari da rete.
- I file originali vengono salvati in `/tmp/devagent_backup/` prima di ogni modifica.
- Rollback automatico se `apply_patch` fallisce a meta.
- Il diff viene applicato manualmente (no chiamata a `patch` di sistema) per portabilita.
