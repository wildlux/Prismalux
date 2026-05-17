#include "impostazioni_page.h"
#include "../widgets/toggle_switch.h"
#include "../widgets/stt_whisper.h"
#include "personalizza_page.h"
#include "manutenzione_page.h"
#include "grafico_page.h"
#include "../theme_manager.h"
#include "../prismalux_paths.h"
namespace P = PrismaluxPaths;
#include <QVBoxLayout>
#include <QHBoxLayout>
#include <QGridLayout>
#include <QFormLayout>
#include <QTabWidget>
#include <QScrollArea>
#include <QLabel>
#include <QPushButton>
#include <QFrame>
#include <QButtonGroup>
#include <QLineEdit>
#include <QComboBox>
#include <QSpinBox>
#include <QDoubleSpinBox>
#include <QSlider>
#include <QSettings>
#include <QDir>
#include <QDirIterator>
#include <QFile>
#include <QFileInfo>
#include <QProcess>
#include <QFileDialog>
#include <QDateTime>
#include <QStandardPaths>
#include <QDesktopServices>
#include <QUrl>
#include <QNetworkAccessManager>
#include <QNetworkRequest>
#include <QNetworkReply>
#include <functional>
#include <QCoreApplication>
#include <QTimer>
#include <QRadioButton>
#include <QCheckBox>
#include <QListWidget>
#include <QTableWidget>
#include <QHeaderView>
#include <QGroupBox>
#include <QPlainTextEdit>
#include <QThread>
#include <QStackedWidget>
#include <QColorDialog>
#include <QPainter>
#include <QPen>
#include <QJsonDocument>
#include <QJsonObject>
#include <QJsonArray>
#include <QGuiApplication>
#include <QClipboard>
#include <QMessageBox>
#include <QProgressBar>
#include <QTextEdit>
#include <QTextBrowser>
#include <QDialogButtonBox>
#include <QDialog>
#include <QRegularExpression>

QWidget* ImpostazioniPage::buildTestTab()
{
    /* ── Struttura dati per ogni suite di test ───────────────────────────
       details: voci separate da '\n', mostrate come lista bullet
       kpi:     metriche chiave (tempi, soglie), mostrate in evidenza
    ─────────────────────────────────────────────────────────────────── */
    struct TestEntry {
        QString suite;
        int     passed;
        int     total;
        QString date;
        QString desc;          /* una riga: cosa testa */
        QString details;       /* voci separate da \n — lista bullet */
        QString kpi;           /* metriche numeriche in evidenza */
    };

    static const TestEntry kTests[] = {
        {
            "test_engine",
            18, 18, "2026-03-18",
            "TUI C \xe2\x80\x94 pipeline core: connessione TCP, parsing JSON, streaming AI, python3",
            "TCP helpers: connessione a Ollama e llama-server\n"
            "JSON encode/decode: js_str(), js_encode() con caratteri speciali\n"
            "ai_chat_stream Ollama: stream NDJSON completo\n"
            "ai_chat_stream llama-server: stream SSE (OpenAI API)\n"
            "python3 popen: esecuzione script e cattura output\n"
            "backend_ready(): rilevamento server attivo",
            "18/18 pass \xc2\xb7 nessuna dipendenza esterna richiesta"
        },
        {
            "test_multiagente",
            8, 8, "2026-03-18",
            "TUI C \xe2\x80\x94 pipeline 6 agenti end-to-end con modelli reali (qwen3:4b + qwen2.5-coder:7b)",
            "Ricercatore: raccoglie informazioni sul task\n"
            "Pianificatore: struttura il piano di sviluppo\n"
            "Programmatori A e B: lavoro parallelo su due implementazioni\n"
            "Tester: esegue il codice e cicla correzioni (max 3 tentativi)\n"
            "Ottimizzatore: revisione finale dell\xe2\x80\x99output\n"
            "Validazione: output Python eseguibile, nessun traceback",
            "8/8 pass \xc2\xb7 richiede Ollama attivo con i modelli installati"
        },
        {
            "test_buildsys.cpp",
            36, 36, "2026-03-22",
            "Qt GUI \xe2\x80\x94 _buildSys(): costruzione system prompt adattivo per famiglia LLM",
            "Modelli piccoli \xe2\x89\xa4" "4.5B: usa sysPromptSmall (2-3 frasi brevi)\n"
            "Modelli medi 7-8B: system prompt standard\n"
            "Modelli grandi 9B+: system prompt esteso con dettagli\n"
            "LlamaLocal: tronca a 2 frasi per compatibilit\xc3\xa0 contesto limitato\n"
            "Reasoning (qwen3/deepseek-r1): aggiunge nota /think diretta\n"
            "Iniezione math [=N]: aggiunge avviso valori pre-calcolati\n"
            "Edge case: modello senza dimensione, senza 'b' nel nome",
            "36/36 pass \xc2\xb7 isolato, no AI \xc2\xb7 7 famiglie LLM testate"
        },
        {
            "test_toon_config.c",
            55, 55, "2026-03-22",
            "TUI C \xe2\x80\x94 parser file .toon e persistenza configurazione Prismalux",
            "Lettura chiave semplice e prima riga del file\n"
            "toon_get_int(): conversione intera con fallback\n"
            "Overflow buffer: lunghezza valore > capacit\xc3\xa0 (no crash)\n"
            "CRLF e spazi trailing: normalizzazione automatica\n"
            "Valori con ':' (es. qwen3.5:9b): parsing corretto\n"
            "Chiave parziale: 'model' != 'ollama_model' (no falsi positivi)\n"
            "UTF-8: caratteri multibyte nei valori\n"
            "Round-trip write\xe2\x86\x92read: dati identici dopo salvataggio\n"
            "Stress: 200 chiavi, 10k ricerche",
            "55/55 pass \xc2\xb7 10k ricerche < 100ms \xc2\xb7 scenari reali Prismalux"
        },
        {
            "test_stress.c",
            74, 74, "2026-03-22",
            "TUI C \xe2\x80\x94 stress API pubblica: JSON, buffer, Unicode, input anomali",
            "js_str parser: stringhe vuote, escape, nidificazioni profonde\n"
            "js_encode escaping: caratteri di controllo, backslash, virgolette\n"
            "BackendCtx: modello vuoto, host nullo, porta fuori range\n"
            "IEEE 754: NaN, +Inf, -Inf, zero negativo\n"
            "Buffer overflow: protezione strlen > capacit\xc3\xa0\n"
            "UTF-8 multibyte: cinese, arabo, emoji nei prompt\n"
            "String ops interni: strncpy, snprintf con n=0\n"
            "Input TUI assurdi: 0, -1, 999, stringa vuota, solo spazi",
            "74/74 pass \xc2\xb7 nessun crash su 74 scenari anomali"
        },
        {
            "test_cs_random.c",
            17, 17, "2026-03-22",
            "TUI C \xe2\x80\x94 Context Store semantico: scrittura massiva, precision e recall",
            "Scrittura 500 chunk con chiavi semantiche casuali\n"
            "Precision: i risultati contengono almeno una chiave della query\n"
            "Recall: nessun chunk rilevante viene escluso\n"
            "Score ordering: chunk con pi\xc3\xb9 chiavi comuni viene restituito prima\n"
            "Persistenza: close() + reopen() restituisce gli stessi risultati\n"
            "Edge case: chiave inesistente (0 risultati), chunk da 10 KB\n"
            "Stress: 1000 query consecutive senza degrado",
            "17/17 pass \xc2\xb7 throughput > 2400 query/s"
        },
        {
            "test_guardia_math.c",
            93, 93, "2026-03-22",
            "TUI C \xe2\x80\x94 parser aritmetico guardia pre-pipeline (copiato in isolamento da multi_agente.c)",
            "Operazioni base: +, -, *, /, % con precedenza corretta\n"
            "Potenza e fattoriale: 2^10=1024, 5!=120\n"
            "Espressioni composite: ((3+4)*2-1)^2 = 169\n"
            "Divisione sicura: 1/0=0, 0%0=0 (no crash, no UB)\n"
            "Pattern sconto: '20% di sconto su 150' \xe2\x86\x92 120\n"
            "Pattern percentuale: '15% di 200' \xe2\x86\x92 30\n"
            "Numeri primi: primes(10,50) \xe2\x86\x92 [11,13,17,19,23,29,31,37,41,43,47]\n"
            "Sommatoria: sum(1,100) \xe2\x86\x92 5050 (formula Gauss)\n"
            "Linguaggio naturale: 'quanto fa 3+4', 'calcola 10*5'\n"
            "_task_sembra_codice(): riconosce task non-programmatici\n"
            "Edge case: range >100k, somma inversa, overflow",
            "93/93 pass \xc2\xb7 30k eval in 4ms (7.5M eval/s) \xc2\xb7 isolato, no AI"
        },
        {
            "test_math_locale.c",
            120, 120, "2026-03-22",
            "TUI C \xe2\x80\x94 motore matematico Tutor (copiato in isolamento da strumenti.c)",
            "_math_eval: parser ricorsivo completo con precedenza\n"
            "_is_prime: tutti i 1229 numeri primi tra 1 e 10000\n"
            "_fmt_num: interi mostrati senza .0 superfluo\n"
            "Sconto e ricarica: '30% sconto su 200' \xe2\x86\x92 140, '20% ricarica su 50' \xe2\x86\x92 60\n"
            "Percentuale semplice: '15% di 80' \xe2\x86\x92 12\n"
            "MCD/MCM Euclide: mcd(48,18)=6, mcm(4,6)=12\n"
            "Radice quadrata: sqrt(144)=12, sqrt(2)\xe2\x89\x88" "18 decimali\n"
            "Media: media di [1,2,3,4,5] = 3\n"
            "Sommatoria Gauss: sum(1,100) = 5050\n"
            "Fattoriale: 10! = 3628800, soglia overflow per n>20\n"
            "Edge case: numero negativo, espressione nulla, valore troppo grande",
            "120/120 pass \xc2\xb7 150k eval in 22ms \xc2\xb7 1229 primi verificati \xc2\xb7 isolato, no AI"
        },
        {
            "test_agent_scheduler.c",
            87, 87, "2026-03-22",
            "TUI C \xe2\x80\x94 Hot/Cold scheduler GGUF: API completa senza llama.cpp",
            "as_init: legge VRAM/RAM da HWInfo, imposta flag sequential se VRAM < 4 GB\n"
            "as_add: overflow slot (max 8), normalizzazione priorit\xc3\xa0, path Windows\n"
            "as_assign_layers: CPU-only=0, scaling proporzionale VRAM, sticky-hot=base, light=cap 16\n"
            "as_load: cache hit (hot_idx==idx) incrementa cache_hits e ritorna subito\n"
            "as_evict: evict doppio sicuro (no double-free)\n"
            "as_record: EMA latenza \xce\xb1=0.3, promozione sticky-hot a 3 chiamate consecutive\n"
            "vram_profile round-trip: salva/ricarica JSON con delta VRAM per modello\n"
            "Modalit\xc3\xa0 sequential: pipeline lineare quando VRAM insufficiente\n"
            "Stress: 1000 cicli load/evict/record",
            "87/87 pass \xc2\xb7 stress 1000 cicli \xc2\xb7 500 cache hit + 500 miss verificati"
        },
        {
            "test_perf.c",
            26, 26, "2026-03-22",
            "TUI C \xe2\x80\x94 Performance: TTFT, throughput token, cold start, benchmark timer",
            "Timer CLOCK_MONOTONIC: risoluzione misurata (0.133 \xc2\xb5s su questo sistema)\n"
            "Monotonicità: 100 campionamenti sempre crescenti\n"
            "TTFT scenario 50ms: primo token rilevato tra 40ms e 200ms\n"
            "TTFT scenario istantaneo: < 100ms, throughput simulato 86M tok/s\n"
            "TTFT scenario 15 tok/s: TTFT 200ms, throughput 25.8 tok/s\n"
            "Cold start scheduler: 1000 init in 79ms (0.079ms/init)\n"
            "vram_profile round-trip: 100 save/load in 2.1ms\n"
            "Contatore token: accuratezza \xc2\xb1" "10% su 100 frasi da 10 parole\n"
            "Benchmark now_ms(): 1M chiamate in 24ms (24 ns/call)",
            "26/26 pass \xc2\xb7 TTFT KPI < 3s \xc2\xb7 throughput KPI > 10 tok/s \xc2\xb7 timer 0.133\xc2\xb5s"
        },
        {
            "test_sha256.c",
            20, 20, "2026-03-22",
            "TUI C \xe2\x80\x94 SHA-256 puro C (FIPS 180-4) per verifica integrit\xc3\xa0 file .gguf",
            "Vettore NIST 1: SHA-256(\"\") = e3b0c442...\n"
            "Vettore NIST 2: SHA-256(\"abc\") = ba7816bf...\n"
            "Vettore NIST 3: SHA-256(stringa 448-bit) = 248d6a61...\n"
            "Vettore NIST 4: SHA-256(\"The quick brown fox...\") = d7a8fbb3...\n"
            "Vettore NIST 5: SHA-256(byte 0x00) = 6e340b9c...\n"
            "Consistenza: hash blocco singolo = aggiornamenti byte per byte\n"
            "File vuoto: hash corretto; file inesistente: ritorna -1\n"
            "File .gguf reali: Qwen2.5-3B (13s), SmolLM2-135M (0.7s)\n"
            "Avalanche effect: 1 byte modificato \xe2\x86\x92 32/32 byte digest diversi\n"
            "Formato HuggingFace: 'sha256:<hex>' (71 caratteri)",
            "20/20 pass \xc2\xb7 1 MB hashato in 7ms \xc2\xb7 5 vettori NIST verificati"
        },
        {
            "test_memory.c",
            12, 12, "2026-03-22",
            "TUI C \xe2\x80\x94 Rilevamento memory leak via /proc/self/status (campo VmRSS)",
            "Lettura VmRSS: baseline 1.7 MB, delta tra due letture consecutive = 0 KB\n"
            "Baseline malloc/free: 10k alloc con dimensioni 128-4096 byte, delta RSS = 0 KB\n"
            "Scheduler 1000 cicli: as_init+as_add+as_assign+as_load+as_record+as_evict, delta = 120 KB\n"
            "Context Store 5 cicli: 40 write + cs_close() ogni ciclo, delta = 4 KB\n"
            "Stress alloc 100k: pattern prompt AI (500B, 4KB, 8KB, 2KB, 256B), delta = 92 KB\n"
            "Pipeline alloc: 100 run da 6 agenti \xc3\x97" "16 KB ciascuno (cascade), delta finale 92 KB\n"
            "KPI finale: RSS processo test = 3.1 MB (soglia < 50 MB)",
            "12/12 pass \xc2\xb7 scheduler < 512 KB/1000 cicli \xc2\xb7 RSS finale 3.1 MB"
        },
        {
            "test_rag.c",
            30, 30, "2026-03-22",
            "TUI C \xe2\x80\x94 RAG locale: precision, score ordering, edge case, performance",
            "Dir inesistente: rag_query() ritorna 0 chunk, out_ctx vuoto (no crash)\n"
            "Dir vuota: stessa garanzia, out_ctx = stringa vuota\n"
            "Precision math: chunk restituiti contengono le parole 'matematica'/'equazioni'\n"
            "Precision Python: chunk contengono 'Python'/'programmazione'\n"
            "Precision storia: chunk contengono 'storia'/'medioevo'\n"
            "Score ordering: documento ad alta rilevanza incluso nel top-K\n"
            "File non-.txt ignorati: .json con dati rilevanti non viene restituito\n"
            "File .txt vuoto: no crash, 0 chunk\n"
            "rag_build_prompt: output inizia con 'CONTESTO DOCUMENTALE'\n"
            "rag_build_prompt senza RAG: ritorna solo il sys prompt base\n"
            "Edge case: query vuota, parole < 3 caratteri, buffer da 10 byte\n"
            "Documenti reali: rag_docs/730 e rag_docs/piva (3 chunk ciascuno)",
            "30/30 pass \xc2\xb7 5 query in 0.22ms \xc2\xb7 100 stress query in 3.7ms"
        },
        {
            "test_security.c",
            65, 65, "2026-03-22",
            "TUI C \xe2\x80\x94 Sicurezza: anti-prompt-injection e anti-data-leak (isolato)",
            "T01 ChatML/Qwen: <|im_start|>, <|im_end|>, <|endoftext|>, <|eot_id|>, ### rimossi\n"
            "T02 Llama2/Mistral: [INST], [/INST], <<SYS>>, <</SYS>>, <!-- rimossi\n"
            "T03 Role override: \\n\\nSystem:, \\n\\nAssistant:, \\n\\nUser:, \\n\\nHuman:, "
            "\\n\\nAI: neutralizzati (solo \\n\\n rimosso, testo preservato)\n"
            "T04 Pattern multipli: tutti i pattern rimossi da una stringa mista; "
            "### rimosso in tutte le occorrenze\n"
            "T05 Testo legittimo: codice Python, matematica, italiano con accenti, "
            "URL, newline singoli \xe2\x80\x94 NON modificati\n"
            "T06 Idempotenza: applicata 2 volte = stesso risultato di 1 volta\n"
            "T07 Edge case: NULL, bufmax=0, stringa vuota, soli spazi, buffer 1 byte \xe2\x80\x94 no crash\n"
            "T08 Confine buffer: ### al bordo del buffer \xe2\x80\x94 no overflow, null terminator intatto\n"
            "T09 js_encode nome modello: virgolette, newline, backslash, CR bloccati; "
            "modello legittimo 'qwen2.5-coder:7b' non alterato\n"
            "T10 host_is_local whitelist: 127.0.0.1, ::1, localhost, LOCALHOST accettati\n"
            "T11 host esterni bloccati: 8.8.8.8, 192.168.1.1, api.openai.com, "
            "127.0.0.2, URL injection credential, stringa vuota, NULL\n"
            "T12 Stress: 10k sanitizzazioni su 10 input casuali \xe2\x80\x94 0 injection sopravvivono",
            "65/65 pass \xc2\xb7 10k sanitizzazioni in 1.9ms \xc2\xb7 isolato, no AI, no rete"
        },
        {
            "test_golden.c",
            53, 53, "2026-03-23",
            "Golden Prompts \xe2\x80\x94 framework checker qualit\xc3\xa0 risposte AI (12 prompt, 5 categorie)",
            "T01\xe2\x80\x93T04 matematica: keyword '4','30','6','11/13/17/19' presenti; 'error/sorry' assenti\n"
            "T05\xe2\x80\x93T07 codice Python: 'def','for','range' presenti; '[]' o 'list()' per lista vuota; 'len' spiegato\n"
            "T08\xe2\x80\x93T09 lingua italiana: 'algoritmo/istruzioni/problema'; 'dato/struttura' \xe2\x80\x94 no parole inglesi\n"
            "T10\xe2\x80\x93T11 pratico: 'forfettario/regime'; '730/redditi/dichiarazione'\n"
            "T12 sicurezza: 'query/database/SQL/injection' \xe2\x80\x94 no 'I cannot/sorry'\n"
            "Mock positivi (4): tutte le keyword trovate, nessuna parola vietata \xe2\x80\x94 passano\n"
            "Mock negativi (4): keyword mancanti o parole vietate o testo troppo corto \xe2\x80\x94 falliscono\n"
            "Integrit\xc3\xa0 JSON: file golden_prompts.json letto, hash SHA-256 stabile",
            "53/53 pass \xc2\xb7 4 mock positivi OK \xc2\xb7 4 mock negativi rilevati \xc2\xb7 no AI, no rete"
        },
        {
            "test_version.c",
            35, 35, "2026-03-23",
            "Build Reproducibility \xe2\x80\x94 tracciabilit\xc3\xa0 e stabilit\xc3\xa0 del build",
            "T01 Sorgenti: tutti e 14 i file src/*.c esistono e sono leggibili\n"
            "T02 Header: 8 dichiarazioni critiche verificate (prompt_sanitize, ai_chat_stream, hw_detect, "
            "as_init, cs_open, rag_query, js_encode, tcp_connect)\n"
            "T03 Makefile: 10 target attesi presenti (http, clean, test, test_perf, test_sha256, "
            "test_memory, test_rag, test_security, test_golden, test_version)\n"
            "T04 Hash stabile: SHA-256 di 5 sorgenti identico tra due letture consecutive\n"
            "T05 Binario: 'prismalux' esiste, > 50 KB, < 50 MB \xe2\x80\x94 hash build calcolato per tracciabilit\xc3\xa0\n"
            "T06 golden_prompts.json: esiste, > 100 byte, hash stabile, contiene i campi attesi\n"
            "T07 Sorgenti non vuoti: nessun src/*.c da 0 byte\n"
            "T08 Suite completa: tutti i 15 file test_*.c presenti nella directory",
            "35/35 pass \xc2\xb7 14 src verificati \xc2\xb7 hash SHA-256 stabile \xc2\xb7 build fingerprint calcolato"
        },
        {
            "test_rag_engine.cpp",
            15, 15, "2026-04-13",
            "Qt GUI \xe2\x80\x94 RagEngine: JLT projection, addChunk, search, save/load, round-trip",
            "emptyCount: chunkCount() == 0 su engine vuoto\n"
            "projectDim: output JLT ha dimensione kTargetDim (256)\n"
            "addIncreasesCount: chunkCount cresce per ogni addChunk\n"
            "searchRoundTrip: top-1 ritrova chunk inserito con vettore identico\n"
            "searchKLargerThanCount: k > N \xe2\x86\x92 al massimo N risultati\n"
            "searchOnEmpty: engine vuoto \xe2\x86\x92 lista vuota\n"
            "saveLoad: round-trip file JSON, inputDim preservata\n"
            "loadMissing: file assente \xe2\x86\x92 false, engine vuoto\n"
            "clearResetsCount: clear() azzera l\xe2\x80\x99indice\n"
            "deterministicProjection: due engine indipendenti stessa query \xe2\x86\x92 stessa risposta\n"
            "alreadyIndexedSearch: save\xe2\x86\x92load\xe2\x86\x92search funziona tra sessioni diverse\n"
            "alreadyIndexedTopK: ordinamento top-k preservato dopo reload\n"
            "partialIndexPreservesCount: indicizzazione interrotta a met\xc3\xa0 \xe2\x86\x92 count corretto\n"
            "countPreservedOnFailedReindex: reindicizzazione fallita non sovrascrive file valido\n"
            "multiDocumentSearch: chunk di documenti diversi su assi separati",
            "15/15 pass \xc2\xb7 isolato, no AI \xc2\xb7 JLT 256-dim verificato"
        },
        {
            "test_app_controller.cpp",
            105, 105, "2026-04-22",
            "Qt GUI \xe2\x80\x94 AppControllerPage: extractCode, stato AI, routing tab, Anki/KiCAD/MCU, segnali",
            "A (20): extractCode \xe2\x80\x94 blocco python/generico/aperto/multiplo + 10 test adversariali\n"
            "B (14): stato macchina runAi \xe2\x80\x94 finished/error, 100 token, no duplicazione\n"
            "C (12): routing codice per tab \xe2\x80\x94 exec button, status label 'pronto'\n"
            "D (8): guard input vuoto \xe2\x80\x94 nessuna chiamata AI senza testo\n"
            "E (18): Anki JSON \xe2\x80\x94 5 carte, UTF-8, HTML, 100 carte, malformato\n"
            "F (12): KiCAD + MCU \xe2\x80\x94 combo items, button state, default host\n"
            "G (12): signal isolation \xe2\x80\x94 token dopo finished, 50 cicli, 7 tab\n"
            "H (10): rapid-fire clicks \xe2\x80\x94 10 click vuoti, Stop\xe2\x86\x92Run, tab switch durante stream\n"
            "I (10): LavoroPage \xe2\x80\x94 LLM label, combo, update su cambio modello, lista offerte",
            "105/105 pass \xc2\xb7 9 categorie \xc2\xb7 mock AI client isolato \xc2\xb7 no rete, no AI"
        },
        {
            "test_lavoro_page.cpp",
            37, 37, "2026-04-22",
            "Qt GUI \xe2\x80\x94 LavoroPage: isolamento segnali, filtri offerte, modello LLM, DB, stato UI",
            "CAT-A (8): isolamento segnali \xe2\x80\x94 token Byzantine non compaiono nel log LavoroPage\n"
            "CAT-B (10): filtri tipo \xc3\x97 livello \xe2\x80\x94 gerarchia diploma/laurea_t/laurea_m corretta\n"
            "CAT-C (7): combo modello LLM \xe2\x80\x94 non sovrascrive con 'Caricamento...', aggiorna su cambio\n"
            "CAT-D (8): integrit\xc3\xa0 DB offerte \xe2\x80\x94 tipi validi, livelli validi, email format, duplicati\n"
            "CAT-E (4): stato UI \xe2\x80\x94 copy button disabilitato prima di generate, tag sorgente nel log",
            "37/37 pass \xc2\xb7 mock AI isolato \xc2\xb7 73 asserzioni \xc2\xb7 no AI, no rete"
        },
        {
            "test_code_utils.cpp",
            11, 11, "2026-04-22",
            "Qt GUI \xe2\x80\x94 AgentiPage: extractPythonCode e _sanitizePyCode (isolati)",
            "A-G: extractPythonCode \xe2\x80\x94 python/generico/vuoto/non-chiuso/multiplo/spazio hint\n"
            "H-K: _sanitizePyCode \xe2\x80\x94 guardia __name__ malformata, senza doppi underscore, invariato, no pip auto",
            "11/11 pass \xc2\xb7 17 asserzioni \xc2\xb7 no AI, no rete"
        },
        {
            "test_signal_lifetime.cpp",
            22, 22, "2026-04-22",
            "Qt GUI \xe2\x80\x94 Regressione: dangling observer, signal leakage, invariant violation",
            "CAT-1 Dangling Observer: token inserito esattamente 1\xc3\x97 per sessione (no duplicazione)\n"
            "CAT-2 Signal Leakage: token ricevuti solo da componenti attivi (non in stato Idle)\n"
            "CAT-3 Invariant Violation: stato UI == stato logico (checkbox, pulsanti, visibilit\xc3\xa0)",
            "22/22 pass \xc2\xb7 3 categorie \xc2\xb7 bug silenziosi (no crash, output corrotto visivamente)"
        },
        {
            "test_random_tool.cpp",
            10, 10, "2026-04-22",
            "Qt GUI \xe2\x80\x94 _inject_random (agenti_page_tools.cpp): keyword detection, range, count",
            "1. Task senza keyword \xe2\x86\x92 passthrough invariato\n"
            "2. 'numeri casuali' \xe2\x86\x92 header iniettato, task originale preservato\n"
            "3. Conteggio esplicito '10 numeri casuali' \xe2\x86\x92 esattamente 10 valori\n"
            "4. Range 'tra 1 e 50' \xe2\x86\x92 tutti i valori \xe2\x88\x88 [1, 50]\n"
            "5. Decimali 'numeri decimali casuali' \xe2\x86\x92 formato float con virgola\n"
            "6. Grafico + random \xe2\x86\x92 default 50 punti (densit\xc3\xa0 visiva)\n"
            "7. Header iniettato prima del task originale\n"
            "8. Due chiamate \xe2\x86\x92 sequenze diverse (non deterministico)\n"
            "9. Range negativo 'tra -100 e -1' \xe2\x86\x92 valori negativi\n"
            "10. Conteggio limite '500 numeri' \xe2\x86\x92 esattamente 500 valori",
            "10/10 pass \xc2\xb7 17 asserzioni \xc2\xb7 no AI, no rete"
        },
        {
            "test_programmazione_page.cpp",
            40, 40, "2026-04-22",
            "Qt GUI \xe2\x80\x94 ProgrammazionePage: isIntentionalError, parseNumbers, stato widget",
            "CAT-A (15): isIntentionalError \xe2\x80\x94 SyntaxError intenzionale, eccezioni custom, classi definite/non definite\n"
            "CAT-B (15): parseNumbers \xe2\x80\x94 separatori (spazio/virgola/;), negativi, float, filtro testo vs numeri, multicolonna\n"
            "CAT-C (10): widget \xe2\x80\x94 costruzione, 5 tab, lingua Python default, template Fibonacci, pulsanti Run/Fix/Stop\n"
            "Bug rilevati: objectName 'codeEditor' duplicato su m_agentTask e m_revPreview (rinominati)",
            "40/40 pass \xc2\xb7 3 categorie \xc2\xb7 mock AI isolato \xc2\xb7 no AI, no rete"
        },
        {
            "test_ai_integration.cpp",
            41, 41, "2026-04-27",
            "Qt GUI \xe2\x80\x94 AiClient reale \xe2\x86\x94 Ollama: percorso completo classifyQuery \xe2\x86\x92 JSON \xe2\x86\x92 streaming \xe2\x86\x92 finished()",
            "Categoria A (9): classifyQuery \xe2\x80\x94 limite 30/31 char, fix Auto\xe2\x89\xa0Simple, keyword spiega/analizza/perch\xc3\xa9\n"
            "Categoria B (5): AiChatParams \xe2\x80\x94 default Brutal Honesty, save/load JSON, setChatParams round-trip\n"
            "Categoria C (5): AiClient chat reale \xe2\x80\x94 risposta matematica, pattern Observer, trappola logica BH, reqId incrementale, coding Python\n"
            "Categoria D (3): QueryType passthrough \xe2\x80\x94 Simple vs Complex, storia 2 turni, abort() durante streaming\n"
            "Categoria E (9): Costrutti interni (mock) \xe2\x80\x94 kHonestyPrefix, kCavemanPrefix, ordine prefissi, think on/off, num_predict\xc3\x972, iniezione data/ora\n"
            "OllamaMockServer: QTcpServer che intercetta l\xe2\x80\x99HTTP body esatto inviato da AiClient \xe2\x80\x94 verifica JSON senza rete reale",
            "41/41 pass \xc2\xb7 richiede Ollama attivo per Categ. C e D \xc2\xb7 Categ. E mock locale"
        },
        {
            "test_ai_stress.cpp",
            80, 80, "2026-04-27",
            "Qt GUI \xe2\x80\x94 AiClient stress: matrice 24 combinazioni di parametri, 20 richieste consecutive, 14 edge case, qualit\xc3\xa0 reale",
            "Categoria F (30): matrice parametri (mock) \xe2\x80\x94 ogni combinazione di onest\xc3\xa0/caveman/tipo query/modello think-capable (24 righe) + system prompt preservato (4 righe)\n"
            "Categoria G (9): stress sequenziale \xe2\x80\x94 20 richieste consecutive mock, throttle 400ms, abort+retry \xc3\x975, payload 13 KB, storia 50 turni, 10 richieste Ollama reali, abort al 2\xc2\xb0 token\n"
            "Categoria H (33): edge case (mock) \xe2\x80\x94 tutte le 9 keyword temporali iniettano data, system vuoto + prefisso, busy guard, abort idle, boundary 30/31 char, modelli think-capable (qwen3/deepseek-r1/qwen2.5) vs non-capable (gemma3)\n"
            "Categoria I (8): qualit\xc3\xa0 reale con Ollama \xe2\x80\x94 caveman senza convenevoli, trappola logica, num_predict 64 vs 512 (162 vs 1497 char), storia ritenuta, streaming \xe2\x89\xa5 5 token\n"
            "Bug trovato: 'valuta' non \xc3\xa8 keyword Complex in classifyQuery \xe2\x80\x94 rimosso dal test",
            "80/80 pass \xc2\xb7 Categ. F/G/H mock in ~1s \xc2\xb7 Categ. G reale ~45s \xc2\xb7 Categ. I ~4 min con Ollama"
        },
    };

    const int nTests = int(sizeof kTests / sizeof kTests[0]);
    int totPassed = 0, totTotal = 0;
    for (const auto& t : kTests) { totPassed += t.passed; totTotal += t.total; }

    auto* outer = new QWidget(this);
    auto* hlay  = new QHBoxLayout(outer);
    hlay->setContentsMargins(16, 16, 16, 16);
    hlay->setSpacing(12);

    /* ════════════════════════════════════════════════════════
       PANNELLO SINISTRO — intestazione + lista suite
       ════════════════════════════════════════════════════════ */
    auto* leftPanel = new QFrame(outer);
    leftPanel->setObjectName("cardGroup");
    auto* llay = new QVBoxLayout(leftPanel);
    llay->setContentsMargins(12, 14, 12, 12);
    llay->setSpacing(6);

    auto* title = new QLabel(
        "\xe2\x9c\x85  Registro Test", leftPanel);   /* ✅ */
    title->setObjectName("cardTitle");
    llay->addWidget(title);

    auto* summary = new QLabel(
        QString("\xf0\x9f\x93\x8a  <b>%1/%2</b> test \xc2\xb7 <b>%3</b> suite")  /* 📊 · */
            .arg(totPassed).arg(totTotal).arg(nTests),
        leftPanel);
    summary->setObjectName("cardDesc");
    summary->setTextFormat(Qt::RichText);
    llay->addWidget(summary);

    auto* sepH = new QFrame(leftPanel);
    sepH->setFrameShape(QFrame::HLine);
    sepH->setObjectName("cardSep");
    llay->addWidget(sepH);

    /* Lista suite scrollabile */
    auto* listScroll = new QScrollArea(leftPanel);
    listScroll->setWidgetResizable(true);
    listScroll->setFrameShape(QFrame::NoFrame);
    listScroll->setHorizontalScrollBarPolicy(Qt::ScrollBarAlwaysOff);
    auto* listWidget = new QWidget(listScroll);
    auto* listLay   = new QVBoxLayout(listWidget);
    listLay->setContentsMargins(0, 4, 0, 4);
    listLay->setSpacing(4);
    listScroll->setWidget(listWidget);
    llay->addWidget(listScroll, 1);

    /* ── Esecuzione test via GUI ── */
    {
        auto* sepRun = new QFrame(leftPanel);
        sepRun->setFrameShape(QFrame::HLine);
        sepRun->setObjectName("cardSep");
        llay->addWidget(sepRun);

        auto* runRow = new QWidget(leftPanel);
        auto* runLay = new QHBoxLayout(runRow);
        runLay->setContentsMargins(0, 0, 0, 0);
        runLay->setSpacing(6);

        auto* btnBuild  = new QPushButton(
            "\xf0\x9f\x94\xa8  Compila", runRow);      /* 🔨 */
        auto* btnRun    = new QPushButton(
            "\xe2\x96\xb6  Esegui tutti", runRow);     /* ▶ */
        auto* btnStop   = new QPushButton(
            "\xe2\x8f\xb9  Ferma", runRow);            /* ⏹ */
        btnBuild->setObjectName("actionBtn");
        btnRun->setObjectName("actionBtn");
        btnStop->setObjectName("actionBtn");
        btnStop->setEnabled(false);

        runLay->addWidget(btnBuild);
        runLay->addWidget(btnRun);
        runLay->addWidget(btnStop);
        llay->addWidget(runRow);

        auto* runOut = new QTextEdit(leftPanel);
        runOut->setReadOnly(true);
        runOut->setObjectName("outputView");
        runOut->setFixedHeight(110);
        runOut->setPlaceholderText("Output test appare qui...");
        llay->addWidget(runOut);

        /* QProcess vive finché il widget esiste */
        auto* proc = new QProcess(leftPanel);
        proc->setProcessChannelMode(QProcess::MergedChannels);

        const QString qtGuiDir = P::root() + "/gui";
        const QString buildDir = qtGuiDir + "/build_tests";

        /* Salva puntatori come member variables per i slot */
        m_testProc      = proc;
        m_testRunOut    = runOut;
        m_testBtnBuild  = btnBuild;
        m_testBtnRun    = btnRun;
        m_testBtnStop   = btnStop;
        m_testBuildDir  = buildDir;
        m_testQtGuiDir  = qtGuiDir;

        connect(proc,    &QProcess::readyRead,
                this,    &ImpostazioniPage::onTestProcReadyRead);
        connect(proc,    QOverload<int, QProcess::ExitStatus>::of(&QProcess::finished),
                this,    &ImpostazioniPage::onTestProcFinished);
        connect(btnBuild, &QPushButton::clicked,
                this,     &ImpostazioniPage::onTestBuildClicked);
        connect(btnRun,   &QPushButton::clicked,
                this,     &ImpostazioniPage::onTestRunClicked);
        connect(btnStop,  &QPushButton::clicked,
                this,     &ImpostazioniPage::onTestStopClicked);
    }

    /* ════════════════════════════════════════════════════════
       PANNELLO DESTRO — dettaglio suite selezionata
       ════════════════════════════════════════════════════════ */
    auto* rightPanel = new QFrame(outer);
    rightPanel->setObjectName("cardGroup");
    auto* rlay = new QVBoxLayout(rightPanel);
    rlay->setContentsMargins(0, 0, 0, 0);
    rlay->setSpacing(0);

    auto* detailScroll = new QScrollArea(rightPanel);
    detailScroll->setWidgetResizable(true);
    detailScroll->setFrameShape(QFrame::NoFrame);
    auto* detailStack = new QStackedWidget(detailScroll);
    detailScroll->setWidget(detailStack);
    rlay->addWidget(detailScroll);

    hlay->addWidget(leftPanel,  1);
    hlay->addWidget(rightPanel, 2);

    /* ════════════════════════════════════════════════════════
       Costruisce ogni voce della lista sinistra + pagina destra
       ════════════════════════════════════════════════════════ */
    QList<QFrame*> itemFrames;
    for (int i = 0; i < nTests; i++) {
        const auto& t   = kTests[i];
        const bool allOk  = (t.passed == t.total);
        const QString accentColor = allOk ? "#22c55e" : "#f59e0b";

        /* ── Voce lista sinistra ── */
        auto* item = new QFrame(listWidget);
        item->setObjectName("testListItem");
        item->setCursor(Qt::PointingHandCursor);
        auto* irow = new QHBoxLayout(item);
        irow->setContentsMargins(10, 8, 10, 8);
        irow->setSpacing(8);

        /* pallino colorato pass/fail */
        auto* dot = new QLabel(item);
        dot->setFixedSize(10, 10);
        dot->setStyleSheet(QString(
            "background:%1; border-radius:5px;").arg(accentColor));
        irow->addWidget(dot);

        auto* lblName = new QLabel(t.suite, item);
        lblName->setObjectName("cardDesc");
        lblName->setWordWrap(false);
        irow->addWidget(lblName, 1);

        auto* lblBadge = new QLabel(
            QString("<b>%1/%2</b>").arg(t.passed).arg(t.total), item);
        lblBadge->setTextFormat(Qt::RichText);
        lblBadge->setStyleSheet(QString("color:%1; font-size:11px;").arg(accentColor));
        irow->addWidget(lblBadge);

        listLay->addWidget(item);

        /* ── Pagina destra corrispondente ── */
        auto* page = new QWidget(detailStack);
        auto* play = new QVBoxLayout(page);
        play->setContentsMargins(16, 16, 16, 16);
        play->setSpacing(10);

        /* intestazione pagina */
        auto* phdr = new QFrame(page);
        phdr->setObjectName("cardFrame");
        auto* phdrLay = new QVBoxLayout(phdr);
        phdrLay->setContentsMargins(14, 12, 14, 12);
        phdrLay->setSpacing(4);

        auto* prow1 = new QHBoxLayout;
        auto* pSuite = new QLabel(
            QString("<b>%1</b>").arg(t.suite.toHtmlEscaped()), phdr);
        pSuite->setObjectName("cardTitle");
        pSuite->setTextFormat(Qt::RichText);
        prow1->addWidget(pSuite, 1);

        auto* pBadge = new QLabel(
            QString("<span style='color:%1; font-size:15px; font-weight:700;'>%2/%3</span>")
                .arg(accentColor).arg(t.passed).arg(t.total), phdr);
        pBadge->setTextFormat(Qt::RichText);
        prow1->addWidget(pBadge);

        auto* pDate = new QLabel(t.date, phdr);
        pDate->setObjectName("cardDesc");
        pDate->setStyleSheet("font-size:11px; margin-left:10px;");
        prow1->addWidget(pDate);
        phdrLay->addLayout(prow1);

        auto* pDesc = new QLabel(t.desc, phdr);
        pDesc->setObjectName("cardDesc");
        pDesc->setWordWrap(true);
        phdrLay->addWidget(pDesc);
        play->addWidget(phdr);

        /* casi testati */
        auto* pCard = new QFrame(page);
        pCard->setObjectName("cardFrame");
        auto* pCardLay = new QVBoxLayout(pCard);
        pCardLay->setContentsMargins(14, 12, 14, 12);
        pCardLay->setSpacing(6);

        auto* pCasesTitle = new QLabel(
            "\xf0\x9f\x94\xac  <b>Casi testati:</b>", pCard);   /* 🔬 */
        pCasesTitle->setObjectName("cardDesc");
        pCasesTitle->setTextFormat(Qt::RichText);
        pCardLay->addWidget(pCasesTitle);

        const QStringList items = t.details.split('\n', Qt::SkipEmptyParts);
        QString bulletHtml = "<ul style='margin:4px 0 0 0; padding-left:16px;'>";
        for (const QString& it : items)
            bulletHtml += "<li style='margin-bottom:3px;'>"
                        + it.toHtmlEscaped() + "</li>";
        bulletHtml += "</ul>";
        auto* pBullets = new QLabel(bulletHtml, pCard);
        pBullets->setObjectName("cardDesc");
        pBullets->setTextFormat(Qt::RichText);
        pBullets->setWordWrap(true);
        pCardLay->addWidget(pBullets);
        play->addWidget(pCard);

        /* KPI */
        if (!t.kpi.isEmpty()) {
            auto* pKpi = new QLabel(
                QString("\xe2\x9a\xa1  <b>KPI:</b> %1")   /* ⚡ */
                    .arg(t.kpi.toHtmlEscaped()), page);
            pKpi->setTextFormat(Qt::RichText);
            pKpi->setWordWrap(true);
            pKpi->setStyleSheet(
                QString("color:%1; font-size:11px; font-weight:600;"
                        " padding:4px 10px; border-radius:4px;"
                        " border:1px solid %1;").arg(accentColor));
            play->addWidget(pKpi);
        }

        play->addStretch();
        detailStack->addWidget(page);

        item->setProperty("testPageIdx", i);
        item->setProperty("accentColor", accentColor);
        itemFrames.append(item);
    }

    listLay->addStretch();

    /* ── Funzione highlight: aggiorna l'aspetto della voce selezionata ── */
    auto selectItem = [itemFrames, detailStack](int idx) mutable {
        detailStack->setCurrentIndex(idx);
        for (int k = 0; k < itemFrames.size(); k++) {
            QFrame* fr = itemFrames[k];
            if (k == idx) {
                /* bordo sinistro colorato con l'accent del test (verde o ambra) */
                const QString ac = fr->property("accentColor").toString();
                fr->setStyleSheet(
                    QString("QFrame#testListItem { border-radius:6px; border:1px solid %1;"
                            " border-left:3px solid %1; }").arg(ac));
            } else {
                fr->setStyleSheet(
                    "QFrame#testListItem { border-radius:6px;"
                    " border:1px solid rgba(255,255,255,0.07); }");
            }
        }
    };

    /* ── EventFilter click ── */
    class ClickFilter : public QObject {
    public:
        std::function<void(int)> fn;
        ClickFilter(std::function<void(int)> f, QObject* p)
            : QObject(p), fn(std::move(f)) {}
        bool eventFilter(QObject* obj, QEvent* ev) override {
            if (ev->type() == QEvent::MouseButtonRelease) {
                auto* fr = qobject_cast<QFrame*>(obj);
                if (fr) fn(fr->property("testPageIdx").toInt());
            }
            return false;
        }
    };
    auto* clickFilter = new ClickFilter(selectItem, outer);
    for (QFrame* fr : itemFrames)
        fr->installEventFilter(clickFilter);

    /* Seleziona la prima voce all'avvio */
    selectItem(0);

    return outer;
}

void ImpostazioniPage::onHWReady(HWInfo hw) {
    m_hwInfo = hw;
    if (m_manutenzione) m_manutenzione->onHWReady(hw);
    refreshLlmColors();
}

/* ══════════════════════════════════════════════════════════════
   refreshLlmColors — colora verde/giallo/rosso i modelli LLM
   in base alla RAM e VRAM rilevate da hw_detect.
   Verde  = entra in GPU (VRAM sufficiente)
   Giallo = entra in RAM (CPU mode)
   Rosso  = non compatibile con la macchina corrente
   ══════════════════════════════════════════════════════════════ */
void ImpostazioniPage::refreshLlmColors()
{
    if (m_hwInfo.count == 0) return;

    /* VRAM GPU dedicata (non Intel iGPU) */
    const double nvidiaVramGb =
        (m_hwInfo.secondary >= 0 &&
         m_hwInfo.secondary < m_hwInfo.count &&
         m_hwInfo.dev[m_hwInfo.secondary].type != DEV_INTEL)
        ? m_hwInfo.dev[m_hwInfo.secondary].avail_mb / 1024.0
        : 0.0;

    /* RAM totale CPU (margine 15% per OS e overhead) */
    const double totalRamGb =
        (m_hwInfo.primary >= 0 && m_hwInfo.primary < m_hwInfo.count)
        ? m_hwInfo.dev[m_hwInfo.primary].mem_mb / 1024.0 * 0.85
        : 0.0;

    /* VRAM necessaria ≈ ram_gb / 1.2 (Q4 pesi + KV-cache ~200 MB) */
    auto colorFor = [&](double ramGb) -> QColor {
        if (ramGb <= 0.0) return {};
        if (nvidiaVramGb > 0.0 && ramGb / 1.2 <= nvidiaVramGb)
            return QColor("#4ade80");   /* verde: entra in GPU */
        if (totalRamGb > 0.0 && ramGb <= totalRamGb)
            return QColor("#fbbf24");   /* giallo: entra in RAM */
        return QColor("#f87171");       /* rosso: non compatibile */
    };

    /* ── Tabella classifica ── */
    if (m_rankTable) {
        for (int row = 0; row < m_rankTable->rowCount(); row++) {
            auto* ri = m_rankTable->item(row, 0);
            if (!ri) continue;
            const double ramGb = ri->data(Qt::UserRole + 1).toDouble();
            const QColor col   = colorFor(ramGb);
            if (!col.isValid()) continue;
            /* Colora rank, nome e colonne RAM/VRAM — lascia score invariato */
            for (int c : {0, 1, 3, 7}) {
                auto* it = m_rankTable->item(row, c);
                if (it) it->setForeground(col);
            }
        }
    }

    /* ── Lista consigliati ── */
    if (m_consigliatiList) {
        for (int i = 0; i < m_consigliatiList->count(); i++) {
            auto* item = m_consigliatiList->item(i);
            if (!item) continue;
            const double ramGb = item->data(Qt::UserRole + 2).toDouble();
            const QColor col   = colorFor(ramGb);
            if (col.isValid()) item->setForeground(col);
        }
    }
}

/* ══════════════════════════════════════════════════════════════
   Piper TTS — helper statici
   ══════════════════════════════════════════════════════════════ */
/* Cartella piper/ accanto all'eseguibile — tutto autocontenuto nel progetto */

QWidget* ImpostazioniPage::buildLlmConsigliatiTab()
{
    namespace P = PrismaluxPaths;

    /* ── Strutture dati modelli ─────────────────────────────────── */
    struct OllamaEntry {
        const char* ollama;
        const char* display;
        const char* size;
        const char* category;  /* Generale / Coding / Matematica / Vision / Ragionamento */
        const char* badge;     /* colore CSS hex */
        const char* desc;
    };

    struct GgufEntry {
        const char* display;
        const char* filename;  /* nome file .gguf atteso nella models dir */
        const char* url;       /* URL diretto HuggingFace */
        const char* size;
        const char* category;  /* Generale / Coding / Matematica / Vision / Traduzione */
        const char* badge;
        const char* desc;
    };

    static const OllamaEntry OLLAMA[] = {
        /* ── Generale ── */
        { "qwen3:4b",            "Qwen3 4B \xe2\x98\x85",        "~2.6 GB", "Generale",     "#0e4d92",
          "Consigliato per macchine con 8 GB RAM. Ragionamento e italiano eccellenti. "
          "Thinking attivo: usare con parsimonia su 8 GB (pu\xc3\xb2 raddoppiare i token generati)." },
        { "qwen3:8b",            "Qwen3 8B",           "~5.2 GB", "Generale",     "#0e4d92",
          "Ragionamento avanzato, ottimo italiano, coding e analisi. Consigliato per uso quotidiano." },
        { "llama3.2:3b",         "Llama 3.2 3B",       "~2.0 GB", "Generale",     "#0e4d92",
          "Meta. Leggero e veloce, buona comprensione italiano. Ideale su PC con poca RAM." },
        { "gemma3:4b",           "Gemma 3 4B",         "~3.3 GB", "Generale",     "#0e4d92",
          "Google DeepMind. Ottimo su testo, riassunti e domande aperte." },
        { "phi4:14b",            "Phi-4 14B",          "~9.1 GB", "Generale",     "#0e4d92",
          "Microsoft. Modello compatto ma molto potente, eccellente ragionamento." },
        { "mistral:7b",          "Mistral 7B",         "~4.1 GB", "Generale",     "#0e4d92",
          "Veloce e bilanciato. Buono su italiano e testi lunghi." },
        /* ── Coding ── */
        { "qwen2.5-coder:7b",    "Qwen2.5 Coder 7B",  "~4.7 GB", "Coding",       "#166534",
          "Alibaba. Specializzato in codice C/C++/Python/JS. Top nella categoria ~7B." },
        { "deepseek-coder:6.7b", "DeepSeek Coder 6.7B","~3.8 GB","Coding",       "#166534",
          "DeepSeek. Eccellente su algoritmi, completamento e refactoring." },
        { "codellama:7b",        "Code Llama 7B",      "~3.8 GB", "Coding",       "#166534",
          "Meta. Ottimo per generazione e spiegazione di codice generico." },
        /* ── Matematica ── */
        { "qwen2.5-math:7b",     "Qwen2.5 Math 7B",   "~4.7 GB", "Matematica",   "#92400e",
          "Specializzato su matematica scolastica e universitaria. Ottimo con calcolo simbolico." },
        { "deepseek-r1:7b",      "DeepSeek R1 7B",    "~4.7 GB", "Matematica",   "#92400e",
          "Ragionamento matematico passo-passo. Chain-of-thought nativo." },
        /* ── Vision ── */
        { "llama3.2-vision",     "Llama 3.2 Vision \xe2\x98\x85","~7.9 GB","Vision","#581c87",
          "Meta. Il pi\xc3\xb9 stabile su Ollama. Analizza immagini, grafici e schemi. Consigliato come primo modello vision." },
        { "qwen2-vl:7b",         "Qwen2-VL 7B \xe2\x98\x85",  "~5.5 GB", "Vision","#581c87",
          "Il migliore locale per grafici e formule matematiche da immagine. Stabile su Ollama." },
        { "minicpm-v:8b",        "MiniCPM-V 8B",      "~5.4 GB", "Vision",       "#581c87",
          "Compatto e preciso. Ottimo per documenti e immagini con testo." },
        { "llava:7b",            "LLaVA 7B",          "~4.5 GB", "Vision",       "#581c87",
          "Buono su descrizione immagini e analisi visiva generale." },
        { "moondream2",          "Moondream 2",        "~1.7 GB", "Vision",       "#581c87",
          "Ultraleggero per vision. Veloce su PC con poca RAM. Qualit\xc3\xa0 limitata su testi complessi." },
        /* NOTA: deepseek-janus / deepseek-vl NON sono supportati — non includere */
        /* ── Ragionamento ── */
        { "deepseek-r1:14b",     "DeepSeek R1 14B",   "~9.0 GB", "Ragionamento", "#7c2d12",
          "Ragionamento avanzato, problemi complessi. Mostra il processo di pensiero." },
        { "qwen3:30b",           "Qwen3 30B",         "~20 GB",  "Ragionamento", "#7c2d12",
          "Modello grande, qualit\xc3\xa0 vicina ai cloud. Richiede almeno 24 GB RAM." },
        /* ── Xeon / 64 GB ── */
        { "qwen3:30b",           "Qwen3 30B \xe2\x98\x85",    "~20 GB",  "Xeon / 64 GB", "#1e3a5f",
          "Ottimo italiano e ragionamento. Su Xeon 64 GB: avvia con OLLAMA_NUM_GPU=0 per modalit\xc3\xa0 CPU pura. Velocit\xc3\xa0 ottimale con AVX-512." },
        { "deepseek-r1:32b",     "DeepSeek R1 32B \xe2\x98\x85","~20 GB","Xeon / 64 GB","#1e3a5f",
          "Ragionamento top su CPU. Q4 a 20 GB entra comodamente nella RAM — nessuna GPU necessaria. Su Xeon 64 GB supera modelli cloud da 7B." },
        { "qwen2.5-coder:32b",   "Qwen2.5 Coder 32B \xe2\x98\x85","~20 GB","Xeon / 64 GB","#1e3a5f",
          "Miglior modello coding locale. Su Xeon 64 GB gira interamente in RAM. AVX-512 Xeon accelera i calcoli di matrice: ~3-5 token/s su 32 core." },
        { "mixtral:8x7b",        "Mixtral 8x7B (MoE) \xe2\x98\x85","~26 GB","Xeon / 64 GB","#1e3a5f",
          "Mixture of Experts: attiva solo 2/8 esperti per token \xe2\x80\x94 veloce nonostante i 46B parametri totali. Ottimo su italiano, coding e ragionamento misto." },
        { "llama3.1:70b",        "Llama 3.1 70B",     "~40 GB",  "Xeon / 64 GB", "#1e3a5f",
          "Meta, qualit\xc3\xa0 vicina ai modelli cloud. Richiede 48+ GB RAM. Su Xeon 64 GB: perfetto in CPU mode. Avvia con: OLLAMA_NUM_GPU=0 ollama serve." },
        { "deepseek-coder-v2:16b","DeepSeek Coder V2 16B","~9.1 GB","Xeon / 64 GB","#1e3a5f",
          "Coding avanzato, veloce su Xeon. Solo 9 GB in RAM \xe2\x80\x94 rimane spazio abbondante per pipeline multi-agente con modelli multipli in parallelo." },
        { "qwen2.5:72b",         "Qwen2.5 72B",       "~44 GB",  "Xeon / 64 GB", "#1e3a5f",
          "Modello generale di alta qualit\xc3\xa0. 44 GB Q4 entra in 64 GB RAM con margine. Prestazioni eccellenti in italiano, analisi e codice." },
    };

    static const GgufEntry GGUF[] = {
        /* ── Generale ── */
        { "SmolLM2-135M (Q4_K_M)", "SmolLM2-135M-Instruct-Q4_K_M.gguf",
          "https://huggingface.co/bartowski/SmolLM2-135M-Instruct-GGUF/resolve/main/SmolLM2-135M-Instruct-Q4_K_M.gguf",
          "~80 MB", "Generale", "#0e4d92",
          "Ultraleggero (80 MB). Ideale per test su PC con pochissima RAM." },
        { "Qwen2.5-3B (Q4_K_M)", "qwen2.5-3b-instruct-q4_k_m.gguf",
          "https://huggingface.co/Qwen/Qwen2.5-3B-Instruct-GGUF/resolve/main/qwen2.5-3b-instruct-q4_k_m.gguf",
          "~1.9 GB", "Generale", "#0e4d92",
          "Buon equilibrio velocit\xc3\xa0/qualit\xc3\xa0. Ideale per laptop con 8 GB RAM." },
        { "Qwen2.5-7B (Q4_K_M)", "qwen2.5-7b-instruct-q4_k_m.gguf",
          "https://huggingface.co/Qwen/Qwen2.5-7B-Instruct-GGUF/resolve/main/qwen2.5-7b-instruct-q4_k_m.gguf",
          "~4.7 GB", "Generale", "#0e4d92",
          "Ottimo per uso quotidiano, ottimo italiano. Richiede 8+ GB RAM." },
        { "Mistral-7B (Q4_K_M)", "mistral-7b-instruct-v0.2.Q4_K_M.gguf",
          "https://huggingface.co/TheBloke/Mistral-7B-Instruct-v0.2-GGUF/resolve/main/mistral-7b-instruct-v0.2.Q4_K_M.gguf",
          "~4.1 GB", "Generale", "#0e4d92",
          "Veloce e bilanciato, ottimo per testi lunghi." },
        /* ── Coding ── */
        { "Phi-3-mini (Q4_K_M)", "Phi-3-mini-4k-instruct-q4.gguf",
          "https://huggingface.co/microsoft/Phi-3-mini-4k-instruct-gguf/resolve/main/Phi-3-mini-4k-instruct-q4.gguf",
          "~2.2 GB", "Coding", "#166534",
          "Microsoft. Compatto e capace su codice e ragionamento." },
        { "Llama-3.2-3B (Q4_K_M)", "Llama-3.2-3B-Instruct-Q4_K_M.gguf",
          "https://huggingface.co/bartowski/Llama-3.2-3B-Instruct-GGUF/resolve/main/Llama-3.2-3B-Instruct-Q4_K_M.gguf",
          "~2.0 GB", "Coding", "#166534",
          "Meta. Leggero, buono per completamento codice e Q&A." },
        /* ── Matematica ── */
        { "DeepSeek-R1-1.5B (Q4_K_M)", "DeepSeek-R1-Distill-Qwen-1.5B-Q4_K_M.gguf",
          "https://huggingface.co/bartowski/DeepSeek-R1-Distill-Qwen-1.5B-GGUF/resolve/main/DeepSeek-R1-Distill-Qwen-1.5B-Q4_K_M.gguf",
          "~1.1 GB", "Matematica", "#92400e",
          "Piccolo e veloce. Ragionamento base con chain-of-thought." },
        { "Qwen2.5-Math-7B (Q4_K_M)", "qwen2.5-math-7b-instruct-q4_k_m.gguf",
          "https://huggingface.co/Qwen/Qwen2.5-Math-7B-Instruct-GGUF/resolve/main/qwen2.5-math-7b-instruct-q4_k_m.gguf",
          "~4.7 GB", "Matematica", "#92400e",
          "Specializzato su matematica scolastica e universitaria." },
        { "DeepSeek-R1-7B (Q4_K_M)", "DeepSeek-R1-Distill-Qwen-7B-Q4_K_M.gguf",
          "https://huggingface.co/bartowski/DeepSeek-R1-Distill-Qwen-7B-GGUF/resolve/main/DeepSeek-R1-Distill-Qwen-7B-Q4_K_M.gguf",
          "~4.7 GB", "Matematica", "#92400e",
          "Ragionamento avanzato, derivato DeepSeek-R1. Eccellente su prove formali." },
        { "phi-4 (Q4_K_M)", "phi-4-Q4_K_M.gguf",
          "https://huggingface.co/bartowski/phi-4-GGUF/resolve/main/phi-4-Q4_K_M.gguf",
          "~8.4 GB", "Matematica", "#92400e",
          "Microsoft phi-4 — eccellente su matematica e logica formale." },
        /* ── Vision ── */
        { "LLaVA-1.5-7B (Q4_K_M)", "llava-1.5-7b-q4_k_m.gguf",
          "https://huggingface.co/mys/ggml_llava-v1.5-7b/resolve/main/ggml-model-q4_k.gguf",
          "~4.1 GB", "Vision", "#581c87",
          "Analisi immagini e grafici. Compatibile con llama-server con mmproj." },
        { "BakLLaVA-1 (Q4_K_M)", "bakllava-1-Q4_K_M.gguf",
          "https://huggingface.co/mys/ggml_bakllava-1/resolve/main/ggml-model-q4_k.gguf",
          "~4.2 GB", "Vision", "#581c87",
          "Vision multimodale basato su Mistral. Buono su descrizione immagini." },
        /* ── Ragionamento ── */
        { "QwQ-32B (Q4_K_M)", "QwQ-32B-Q4_K_M.gguf",
          "https://huggingface.co/bartowski/QwQ-32B-GGUF/resolve/main/QwQ-32B-Q4_K_M.gguf",
          "~19.9 GB", "Ragionamento", "#7c2d12",
          "Ragionamento profondo di alta qualit\xc3\xa0. Richiede 24+ GB RAM." },
        /* ── Xeon / 64 GB ── */
        { "DeepSeek-R1-32B (Q5_K_M) \xe2\x98\x85", "DeepSeek-R1-Distill-Qwen-32B-Q5_K_M.gguf",
          "https://huggingface.co/bartowski/DeepSeek-R1-Distill-Qwen-32B-GGUF/resolve/main/DeepSeek-R1-Distill-Qwen-32B-Q5_K_M.gguf",
          "~22 GB", "Xeon / 64 GB", "#1e3a5f",
          "Ragionamento top in Q5 \xe2\x80\x94 qualit\xc3\xa0 superiore al Q4. Su Xeon 64 GB carica tutto in RAM senza toccare la VRAM. Chain-of-thought nativo." },
        { "Qwen2.5-Coder-32B (Q5_K_M) \xe2\x98\x85", "qwen2.5-coder-32b-instruct-q5_k_m.gguf",
          "https://huggingface.co/Qwen/Qwen2.5-Coder-32B-Instruct-GGUF/resolve/main/qwen2.5-coder-32b-instruct-q5_k_m.gguf",
          "~22 GB", "Xeon / 64 GB", "#1e3a5f",
          "Top coding locale in Q5. Su Xeon con AVX-512 raggiunge ~4-6 token/s. Pipeline a 6 agenti con questo modello produce codice di qualit\xc3\xa0 vicina a GPT-4." },
        { "Mixtral-8x7B (Q4_K_M) \xe2\x98\x85", "mixtral-8x7b-instruct-v0.1.Q4_K_M.gguf",
          "https://huggingface.co/TheBloke/Mixtral-8x7B-Instruct-v0.1-GGUF/resolve/main/mixtral-8x7b-instruct-v0.1.Q4_K_M.gguf",
          "~26 GB", "Xeon / 64 GB", "#1e3a5f",
          "Mixture of Experts 46B \xe2\x80\x94 veloce nonostante la taglia. Ottimo italiano, coding e analisi. Xeon multi-core gestisce bene i pesi distribuiti." },
        { "Llama-3.1-70B (Q4_K_M)", "Meta-Llama-3.1-70B-Instruct-Q4_K_M.gguf",
          "https://huggingface.co/bartowski/Meta-Llama-3.1-70B-Instruct-GGUF/resolve/main/Meta-Llama-3.1-70B-Instruct-Q4_K_M.gguf",
          "~40 GB", "Xeon / 64 GB", "#1e3a5f",
          "Meta top-of-line locale. 40 GB in RAM \xe2\x80\x94 su Xeon 64 GB con -ngl 0 (CPU puro). Velocit\xc3\xa0 ~1-2 token/s, qualit\xc3\xa0 superiore. Usa contesto lungo (128k)." },
        { "Qwen2.5-72B (Q4_K_M)", "qwen2.5-72b-instruct-q4_k_m.gguf",
          "https://huggingface.co/Qwen/Qwen2.5-72B-Instruct-GGUF/resolve/main/qwen2.5-72b-instruct-q4_k_m.gguf",
          "~44 GB", "Xeon / 64 GB", "#1e3a5f",
          "Massima qualit\xc3\xa0 generale: italiano, analisi, codice. 44 GB Q4 \xe2\x80\x94 entra in 64 GB RAM con margine per il sistema operativo." },
    };

    const int NO = (int)(sizeof(OLLAMA) / sizeof(OLLAMA[0]));
    const int NG = (int)(sizeof(GGUF)   / sizeof(GGUF[0]));

    /* ══════════════════════════════════════════════════════════════
       Layout compatto 2 colonne — nessun scroll necessario:
         Sinistra : filtro gestore + categoria
         Destra   : lista compatta + pannello dettaglio
       ══════════════════════════════════════════════════════════════ */
    auto* page    = new QWidget;
    auto* mainLay = new QVBoxLayout(page);
    mainLay->setContentsMargins(16, 14, 16, 14);
    mainLay->setSpacing(10);

    /* ── Titolo ── */
    auto* titleLbl = new QLabel(
        "\xf0\x9f\xa4\x96  LLM Consigliati  \xe2\x80\x94  "
        "<span style='color:#94a3b8;font-size:12px;font-weight:normal;'>"
        "Seleziona gestore e categoria, poi clicca per installare.</span>", page);
    titleLbl->setObjectName("sectionTitle");
    titleLbl->setTextFormat(Qt::RichText);
    mainLay->addWidget(titleLbl);

    /* ── 2 colonne ── */
    auto* colsRow = new QWidget(page);
    auto* colsLay = new QHBoxLayout(colsRow);
    colsLay->setContentsMargins(0, 0, 0, 0);
    colsLay->setSpacing(14);

    /* ════════════════════════════════════════════════════════
       Colonna sinistra — Filtro gestore + categoria
       ════════════════════════════════════════════════════════ */
    auto* leftGroup = new QGroupBox("\xf0\x9f\x94\xa7  Filtro", colsRow);
    leftGroup->setObjectName("cardGroup");
    leftGroup->setFixedWidth(190);
    auto* leftLay = new QVBoxLayout(leftGroup);
    leftLay->setSpacing(5);

    auto* lblGest = new QLabel("Gestore:", leftGroup);
    lblGest->setObjectName("cardDesc");
    auto* btnOllama = new QRadioButton("\xf0\x9f\x90\xb3  Ollama", leftGroup);
    btnOllama->setChecked(true);
    auto* btnGguf   = new QRadioButton("\xf0\x9f\xa6\x99  GGUF (llama.cpp)", leftGroup);

    auto* sepFilt = new QFrame(leftGroup);
    sepFilt->setFrameShape(QFrame::HLine);
    sepFilt->setObjectName("sidebarSep");

    auto* lblCat = new QLabel("Categoria:", leftGroup);
    lblCat->setObjectName("cardDesc");

    leftLay->addWidget(lblGest);
    leftLay->addWidget(btnOllama);
    leftLay->addWidget(btnGguf);
    leftLay->addWidget(sepFilt);
    leftLay->addWidget(lblCat);

    static const char* CATS[] = {
        "Tutti", "Generale", "Coding",
        "Matematica", "Vision", "Ragionamento",
        "\xf0\x9f\x96\xa5  Xeon / 64 GB"   /* categoria hardware dedicata — etichetta UI */
    };
    /* Chiavi di filtro: devono corrispondere esattamente a m.category */
    static const char* CATS_KEY[] = {
        "", "Generale", "Coding",
        "Matematica", "Vision", "Ragionamento",
        "Xeon / 64 GB"
    };
    static const int N_CATS = 7;
    auto* catBtnGroup = new QButtonGroup(leftGroup);
    for (int i = 0; i < N_CATS; i++) {
        auto* rb = new QRadioButton(CATS[i], leftGroup);
        catBtnGroup->addButton(rb, i);
        if (i == 0) rb->setChecked(true);
        /* Evidenzia la categoria Xeon */
        if (i == N_CATS - 1) {
            rb->setStyleSheet("color:#60a5fa;font-weight:bold;");
            /* Separatore prima della voce Xeon */
            auto* sepX = new QFrame(leftGroup);
            sepX->setFrameShape(QFrame::HLine);
            sepX->setObjectName("sidebarSep");
            leftLay->addWidget(sepX);
        }
        leftLay->addWidget(rb);
    }
    leftLay->addStretch(1);
    colsLay->addWidget(leftGroup);

    /* ════════════════════════════════════════════════════════
       Colonna destra — lista + pannello dettaglio
       ════════════════════════════════════════════════════════ */
    auto* rightGroup = new QGroupBox("\xf0\x9f\x93\xa6  Modelli disponibili", colsRow);
    rightGroup->setObjectName("cardGroup");
    auto* rightLay = new QVBoxLayout(rightGroup);
    rightLay->setSpacing(8);

    auto* modelList = new QListWidget(rightGroup);
    modelList->setObjectName("modelsList");
    modelList->setAlternatingRowColors(true);
    m_consigliatiList = modelList;

    /* ── Legenda compatibilità hardware ── */
    auto* legendCons = new QLabel(rightGroup);
    legendCons->setObjectName("hintLabel");
    legendCons->setTextFormat(Qt::RichText);
    legendCons->setText(
        "<span style='color:#4ade80;'>\xe2\x96\xa0 GPU</span>"
        "&nbsp;&nbsp;<span style='color:#fbbf24;'>\xe2\x96\xa0 RAM/CPU</span>"
        "&nbsp;&nbsp;<span style='color:#f87171;'>\xe2\x96\xa0 Non compatibile</span>");
    rightLay->addWidget(legendCons);
    rightLay->addWidget(modelList, 1);

    /* ── Pannello dettaglio (sotto la lista) ── */
    auto* detSep = new QFrame(rightGroup);
    detSep->setFrameShape(QFrame::HLine);
    detSep->setObjectName("sidebarSep");
    rightLay->addWidget(detSep);

    auto* detRow = new QWidget(rightGroup);
    auto* detLay = new QHBoxLayout(detRow);
    detLay->setContentsMargins(0, 4, 0, 0);
    detLay->setSpacing(10);

    auto* detailLbl = new QLabel("Seleziona un modello per i dettagli.", rightGroup);
    detailLbl->setObjectName("cardDesc");
    detailLbl->setWordWrap(true);
    detailLbl->setTextFormat(Qt::RichText);
    detailLbl->setMinimumHeight(44);

    auto* installBtn = new QPushButton("\xe2\xac\x87  Installa", rightGroup);
    installBtn->setObjectName("actionBtn");
    installBtn->setFixedWidth(110);
    installBtn->setEnabled(false);

    detLay->addWidget(detailLbl, 1);
    detLay->addWidget(installBtn);
    rightLay->addWidget(detRow);

    /* ── Log download ── */
    auto* logOut = new QLabel(rightGroup);
    logOut->setWordWrap(true);
    logOut->setVisible(false);
    logOut->setStyleSheet(
        "background:#0f172a;color:#86efac;font-family:monospace;"
        "font-size:11px;padding:6px 10px;border-radius:6px;");
    rightLay->addWidget(logOut);

    /* ── Download GGUF da URL personalizzato ── */
    auto* customSep = new QFrame(rightGroup);
    customSep->setFrameShape(QFrame::HLine);
    customSep->setObjectName("sidebarSep");
    rightLay->addWidget(customSep);

    auto* customLbl = new QLabel(
        "\xf0\x9f\x94\x97  Scarica GGUF da URL personalizzato (HuggingFace):", rightGroup);
    customLbl->setObjectName("hintLabel");
    rightLay->addWidget(customLbl);

    auto* customRow = new QWidget(rightGroup);
    auto* customRowLay = new QHBoxLayout(customRow);
    customRowLay->setContentsMargins(0, 0, 0, 0);
    customRowLay->setSpacing(6);

    auto* customEdit = new QLineEdit(rightGroup);
    customEdit->setObjectName("chatInput");
    customEdit->setPlaceholderText(
        "https://huggingface.co/.../resolve/main/modello.gguf");
    auto* customDlBtn = new QPushButton("\xe2\xac\x87  Scarica", rightGroup);
    customDlBtn->setObjectName("actionBtn");
    customDlBtn->setFixedWidth(100);
    customRowLay->addWidget(customEdit, 1);
    customRowLay->addWidget(customDlBtn);
    rightLay->addWidget(customRow);

    connect(customDlBtn, &QPushButton::clicked, page, [=]() {
        const QString url = customEdit->text().trimmed();
        if (url.isEmpty()) return;
        const QString filename = QUrl(url).fileName();
        if (filename.isEmpty() || !filename.endsWith(".gguf", Qt::CaseInsensitive)) {
            logOut->setText("\xe2\x9a\xa0  L'URL deve puntare a un file .gguf");
            logOut->setVisible(true);
            return;
        }
        const QString dest = PrismaluxPaths::modelsDir() + "/" + filename;
        logOut->setText(QString("\xf0\x9f\x93\xa5  Scarico %1...").arg(filename));
        logOut->setVisible(true);
        customDlBtn->setEnabled(false);

        auto* proc = new QProcess(page);
        proc->setProcessChannelMode(QProcess::MergedChannels);
        connect(proc, &QProcess::readyRead, page, [proc, logOut]() {
            const QString s = QString::fromUtf8(proc->readAll()).trimmed();
            if (!s.isEmpty()) logOut->setText(s);
        });
        connect(proc, QOverload<int,QProcess::ExitStatus>::of(&QProcess::finished),
                page, [proc, logOut, customDlBtn, filename](int code, QProcess::ExitStatus) {
            if (code == 0)
                logOut->setText(QString("\xe2\x9c\x85  %1 scaricato.").arg(filename));
            else
                logOut->setText("\xe2\x9d\x8c  Download fallito. Controlla URL e connessione.");
            customDlBtn->setEnabled(true);
            proc->deleteLater();
        });

        /* wget preferito, poi curl come fallback */
        if (!QStandardPaths::findExecutable("wget").isEmpty())
            proc->start("wget", {"-c", "--show-progress", "-O", dest, url});
        else
            proc->start("curl", {"-L", "-C", "-", "--progress-bar", "-o", dest, url});
    });

    colsLay->addWidget(rightGroup, 1);
    mainLay->addWidget(colsRow, 1);

    /* ── Pannello consigli Xeon / 64 GB (visibile solo quando selezionata quella categoria) ── */
    /* ── Pannello avviso Vision (visibile solo su categoria Vision) ── */
    auto* visionPanel = new QWidget(page);
    visionPanel->setVisible(false);
    auto* visionPanelLay = new QVBoxLayout(visionPanel);
    visionPanelLay->setContentsMargins(0, 4, 0, 0);
    visionPanelLay->setSpacing(0);
    auto* visionWarnLbl = new QLabel(visionPanel);
    visionWarnLbl->setTextFormat(Qt::RichText);
    visionWarnLbl->setWordWrap(true);
    visionWarnLbl->setStyleSheet(
        "background:#1a0a00;border:1px solid #92400e;border-radius:8px;"
        "padding:10px 14px;color:#e2e8f0;font-size:12px;");
    visionWarnLbl->setText(
        "<b style='color:#fb923c;'>\xe2\x9a\xa0  Modelli Vision — cosa funziona e cosa no</b><br><br>"
        "<b style='color:#fcd34d;'>Compatibili con Ollama:</b> "
        "llama3.2-vision \xe2\x98\x85 \xe2\x80\x94 qwen2-vl:7b \xe2\x98\x85 \xe2\x80\x94 minicpm-v:8b \xe2\x80\x94 llava:7b \xe2\x80\x94 moondream2<br>"
        "<b style='color:#f87171;'>NON compatibili (errori certi):</b> "
        "deepseek-janus \xe2\x80\x94 deepseek-vl \xe2\x80\x94 deepseek-r1 \xe2\x80\x94 deepseek-coder "
        "\xe2\x80\x94 qualsiasi modello DeepSeek<br><br>"
        "<span style='color:#94a3b8;font-size:11px;'>"
        "I modelli DeepSeek non supportano il campo \"images\" dell'API Ollama. "
        "Se li usi con immagini riceverai l'errore: "
        "<i>\"model does not support vision\"</i>. "
        "Prismalux ora mostra questo errore in modo leggibile invece di bloccarsi."
        "</span>");
    visionPanelLay->addWidget(visionWarnLbl);
    mainLay->addWidget(visionPanel);

    /* ── Pannello consigli Xeon / 64 GB ── */
    auto* xeonPanel = new QWidget(page);
    xeonPanel->setVisible(false);
    auto* xeonLay = new QVBoxLayout(xeonPanel);
    xeonLay->setContentsMargins(0, 4, 0, 0);
    xeonLay->setSpacing(0);

    auto* xeonLbl = new QLabel(xeonPanel);
    xeonLbl->setTextFormat(Qt::RichText);
    xeonLbl->setWordWrap(true);
    xeonLbl->setOpenExternalLinks(false);
    xeonLbl->setStyleSheet(
        "background:#0c1e3a;border:1px solid #1e3a5f;border-radius:8px;"
        "padding:12px 16px;color:#e2e8f0;font-size:12px;line-height:160%;");
    xeonLbl->setText(
        "<b style='color:#60a5fa;font-size:13px;'>"
        "\xf0\x9f\x96\xa5  Configurazione consigliata: Xeon + 64 GB RAM</b><br><br>"

        "<b style='color:#93c5fd;'>Strategia ottimale per la tua macchina:</b><br>"
        "\xe2\x80\xa2  Usa <b>CPU pura</b> (ignora la GPU AMD lenta): "
        "<code style='background:#0f172a;padding:1px 5px;border-radius:3px;color:#4ade80;'>"
        "set OLLAMA_NUM_GPU=0 &amp;&amp; ollama serve</code><br>"
        "\xe2\x80\xa2  Con 64 GB RAM puoi caricare modelli da <b>20-44 GB</b> senza toccare la VRAM<br>"
        "\xe2\x80\xa2  Il Xeon ha spesso <b>AVX-512</b>: verifica con "
        "<code style='background:#0f172a;padding:1px 5px;border-radius:3px;color:#4ade80;'>"
        "wmic cpu get name</code> e cerca E5/E7/Scalable/Gold/Silver<br><br>"

        "<b style='color:#93c5fd;'>Modelli raccomandati (in ordine di priorit\xc3\xa0):</b><br>"
        "\xe2\x98\x85  <b>Qwen2.5-Coder-32B Q5</b> (~22 GB) \xe2\x80\x94 coding top, ~4-6 token/s su Xeon<br>"
        "\xe2\x98\x85  <b>DeepSeek-R1-32B Q5</b> (~22 GB) \xe2\x80\x94 ragionamento/matematica avanzata<br>"
        "\xe2\x98\x85  <b>Mixtral 8x7B Q4</b> (~26 GB) \xe2\x80\x94 generico veloce (MoE attiva 2/8 esperti)<br>"
        "       <b>Llama-3.1-70B Q4</b> (~40 GB) \xe2\x80\x94 massima qualit\xc3\xa0, ~1-2 token/s<br><br>"

        "<b style='color:#93c5fd;'>Pipeline multi-agente consigliata su Xeon 64 GB:</b><br>"
        "\xe2\x80\xa2  Agente 1 (Ricercatore): <b>deepseek-r1:14b</b> \xe2\x80\x94 veloce per analisi<br>"
        "\xe2\x80\xa2  Agenti 2-3 (Programmatori): <b>deepseek-coder-v2:16b</b> \xe2\x80\x94 potente e leggero<br>"
        "\xe2\x80\xa2  Agente 5 (Tester): <b>qwen2.5-coder:32b</b> \xe2\x80\x94 massima precisione sul codice<br>"
        "\xe2\x80\xa2  Agente 6 (Ottimizzatore): <b>qwen3:30b</b> \xe2\x80\x94 qualit\xc3\xa0 di ragionamento superiore<br><br>"

        "<span style='color:#64748b;font-size:11px;'>"
        "Nota: modelli con \xe2\x98\x85 sono testati e ottimizzati per CPU Xeon multi-core."
        "</span>");
    xeonLay->addWidget(xeonLbl);
    mainLay->addWidget(xeonPanel);

    /* ════════════════════════════════════════════════════════
       Logica — popola lista in base ai filtri selezionati
       ════════════════════════════════════════════════════════ */

    /* Converte stringa size (es. "~5.2 GB", "~80 MB") in GB float */
    auto parseSizeGb = [](const char* s) -> double {
        QString str = QString(s).remove('~').trimmed();
        if (str.endsWith("MB", Qt::CaseInsensitive))
            return str.remove("MB", Qt::CaseInsensitive).trimmed().toDouble() / 1024.0;
        return str.remove("GB", Qt::CaseInsensitive).trimmed().toDouble();
    };

    auto populate = [=]() {
        modelList->clear();
        installBtn->setEnabled(false);
        detailLbl->setText("Seleziona un modello per i dettagli.");
        logOut->setVisible(false);

        const bool isOllama  = btnOllama->isChecked();
        const int  catId     = catBtnGroup->checkedId();
        /* Usa CATS_KEY (senza emoji) per la comparazione con m.category */
        const QString filter = (catId == 0) ? QString() : QString(CATS_KEY[catId]);

        /* Mostra/nascondi pannelli contestuali */
        xeonPanel->setVisible(catId == N_CATS - 1);
        /* Categoria Vision (indice 4 = "Vision" in CATS_KEY) */
        const int kVisionCat = 4;
        visionPanel->setVisible(catId == kVisionCat);

        if (isOllama) {
            for (int i = 0; i < NO; i++) {
                const auto& m = OLLAMA[i];
                if (!filter.isEmpty() && filter != m.category) continue;
                auto* item = new QListWidgetItem(
                    QString("[%1]  %2  \xe2\x80\x94  %3")
                    .arg(m.category, m.display, m.size));
                item->setData(Qt::UserRole,     i);
                item->setData(Qt::UserRole + 1, true);
                item->setData(Qt::UserRole + 2, parseSizeGb(m.size));
                modelList->addItem(item);
            }
        } else {
            for (int i = 0; i < NG; i++) {
                const auto& m = GGUF[i];
                if (!filter.isEmpty() && filter != m.category) continue;
                const bool inst = QFile::exists(
                    PrismaluxPaths::modelsDir() + "/" + m.filename);
                auto* item = new QListWidgetItem(
                    QString("[%1]  %2  \xe2\x80\x94  %3%4")
                    .arg(m.category, m.display, m.size,
                         inst ? "  \xe2\x9c\x94" : ""));
                item->setData(Qt::UserRole,     i);
                item->setData(Qt::UserRole + 1, false);
                item->setData(Qt::UserRole + 2, parseSizeGb(m.size));
                modelList->addItem(item);
            }
        }
        refreshLlmColors();
    };

    /* Selezione modello → aggiorna pannello dettaglio */
    connect(modelList, &QListWidget::currentItemChanged, page,
            [=](QListWidgetItem* cur, QListWidgetItem*) {
        if (!cur) {
            installBtn->setEnabled(false);
            detailLbl->setText("Seleziona un modello per i dettagli.");
            return;
        }
        const int  idx   = cur->data(Qt::UserRole).toInt();
        const bool isOll = cur->data(Qt::UserRole + 1).toBool();
        installBtn->setEnabled(true);
        installBtn->setText("\xe2\xac\x87  Installa");
        installBtn->setStyleSheet("");

        if (isOll) {
            const auto& m = OLLAMA[idx];
            detailLbl->setText(
                QString("<b>%1</b>  <span style='color:#64748b;'>%2</span><br>"
                        "<span style='color:#94a3b8;font-size:11px;'>%3</span>")
                .arg(m.display, m.size, m.desc));
        } else {
            const auto& m = GGUF[idx];
            const bool inst = QFile::exists(
                PrismaluxPaths::modelsDir() + "/" + m.filename);
            detailLbl->setText(
                QString("<b>%1</b>  <span style='color:#64748b;'>%2</span>"
                        "%4<br>"
                        "<span style='color:#94a3b8;font-size:11px;'>%3</span>")
                .arg(m.display, m.size, m.desc,
                     inst ? "  <span style='color:#4ade80;'>\xe2\x9c\x94 presente</span>"
                          : ""));
        }
    });

    /* Pulsante Installa */
    connect(installBtn, &QPushButton::clicked, page, [=]() {
        auto* cur = modelList->currentItem();
        if (!cur) return;
        const int  idx   = cur->data(Qt::UserRole).toInt();
        const bool isOll = cur->data(Qt::UserRole + 1).toBool();

        installBtn->setEnabled(false);
        installBtn->setText("\xe2\x8f\xb3  ...");
        logOut->setVisible(true);

        if (isOll) {
            const auto& m = OLLAMA[idx];
            const QString ollamaName(m.ollama);
            logOut->setText(QString("\xf0\x9f\x93\xa5  ollama pull %1").arg(ollamaName));
            auto* proc = new QProcess(page);
            proc->setProcessChannelMode(QProcess::MergedChannels);
            connect(proc, &QProcess::readyRead, page, [proc, logOut]() {
                const QString txt = QString::fromUtf8(proc->readAll()).trimmed();
                if (!txt.isEmpty()) logOut->setText(txt);
            });
            connect(proc, QOverload<int,QProcess::ExitStatus>::of(&QProcess::finished),
                    page, [proc, installBtn, logOut, ollamaName](int code, QProcess::ExitStatus) {
                if (code == 0) {
                    installBtn->setText("\xe2\x9c\x94  Installato");
                    installBtn->setStyleSheet("color:#4ade80;");
                    logOut->setText(QString("\xe2\x9c\x85  %1 installato.").arg(ollamaName));
                } else {
                    installBtn->setEnabled(true);
                    installBtn->setText("\xe2\xac\x87  Installa");
                    logOut->setText("\xe2\x9d\x8c  Errore. Assicurati che ollama sia in esecuzione.");
                }
                proc->deleteLater();
            });
            proc->start("ollama", {"pull", ollamaName});
        } else {
            const auto& m = GGUF[idx];
            const QString ggufUrl(m.url);
            const QString ggufFile(m.filename);
            const QString dest = PrismaluxPaths::modelsDir() + "/" + ggufFile;
            logOut->setText(QString("\xf0\x9f\x93\xa5  Scarico %1...").arg(ggufFile));
            auto* proc = new QProcess(page);
            proc->setProcessChannelMode(QProcess::MergedChannels);
            connect(proc, &QProcess::readyRead, page, [proc, logOut]() {
                const QString txt = QString::fromUtf8(proc->readAll()).trimmed();
                if (!txt.isEmpty()) logOut->setText(txt);
            });
            connect(proc, QOverload<int,QProcess::ExitStatus>::of(&QProcess::finished),
                    page, [proc, installBtn, logOut, ggufFile](int code, QProcess::ExitStatus) {
                if (code == 0) {
                    installBtn->setText("\xe2\x9c\x94  Scaricato");
                    installBtn->setStyleSheet("color:#4ade80;");
                    logOut->setText(QString("\xe2\x9c\x85  %1 scaricato.").arg(ggufFile));
                } else {
                    installBtn->setEnabled(true);
                    installBtn->setText("\xe2\xac\x87  Installa");
                    logOut->setText("\xe2\x9d\x8c  Errore download. Controlla wget/connessione.");
                }
                proc->deleteLater();
            });
            proc->start("wget", {"-c", "--show-progress", "-O", dest, ggufUrl});
            if (!proc->waitForStarted(3000)) {
                proc->deleteLater();
                auto* curl = new QProcess(page);
                curl->setProcessChannelMode(QProcess::MergedChannels);
                connect(curl, &QProcess::readyRead, page, [curl, logOut]() {
                    const QString txt = QString::fromUtf8(curl->readAll()).trimmed();
                    if (!txt.isEmpty()) logOut->setText(txt);
                });
                connect(curl, QOverload<int,QProcess::ExitStatus>::of(&QProcess::finished),
                        page, [curl, installBtn, logOut, ggufFile](int code, QProcess::ExitStatus) {
                    if (code == 0) {
                        installBtn->setText("\xe2\x9c\x94  Scaricato");
                        installBtn->setStyleSheet("color:#4ade80;");
                        logOut->setText(QString("\xe2\x9c\x85  %1 scaricato.").arg(ggufFile));
                    } else {
                        installBtn->setEnabled(true);
                        installBtn->setText("\xe2\xac\x87  Installa");
                        logOut->setText("\xe2\x9d\x8c  Errore: installa wget o curl.");
                    }
                    curl->deleteLater();
                });
                curl->start("curl", {"-L", "-C", "-", "--progress-bar",
                                     "-o", dest, ggufUrl});
            }
        }
    });

    /* Cambi filtro → ripopola */
    connect(btnOllama, &QRadioButton::toggled, page, [=](bool on){ if (on) populate(); });
    connect(btnGguf,   &QRadioButton::toggled, page, [=](bool on){ if (on) populate(); });
    for (QAbstractButton* b : catBtnGroup->buttons())
        connect(b, &QAbstractButton::toggled, page, [=](bool on){ if (on) populate(); });

    /* Popola al primo caricamento */
    QTimer::singleShot(0, page, populate);

    /* ── Banner "Modelli con problemi noti" (sempre visibile) ── */
    auto* brokenSep = new QFrame(page);
    brokenSep->setFrameShape(QFrame::HLine);
    brokenSep->setObjectName("sidebarSep");
    mainLay->addWidget(brokenSep);

    auto* brokenLbl = new QLabel(page);
    brokenLbl->setTextFormat(Qt::RichText);
    brokenLbl->setWordWrap(true);
    brokenLbl->setStyleSheet(
        "background:#1a0505;border:1px solid #7f1d1d;"
        "border-radius:8px;padding:8px 14px;color:#e2e8f0;font-size:11px;");
    brokenLbl->setText(
        "<b style='color:#f87171;'>\xe2\x9a\xa0  Modelli con problemi noti (da evitare)</b><br>"
        "<b style='color:#fca5a5;'>qwen3.5:0.8b</b> \xe2\x80\x94 "
        "thinking loop infinito su Ollama: genera solo pensiero interno, "
        "message.content sempre vuoto anche con 2048 token. "
        "Prismalux mostra un errore esplicito se lo selezioni. "
        "<span style='color:#94a3b8;'>Alternative: "
        "llama3.2:3b (~50 tok/s, 80% matematica) "
        "oppure deepseek-r1:1.5b (~41 tok/s).</span>");
    mainLay->addWidget(brokenLbl);

    return page;
}

/* ══════════════════════════════════════════════════════════════
   buildAiParamsTab — parametri di campionamento anti-allucinazione.
   I valori sono salvati in QSettings("Prismalux","GUI") con prefisso "ai/".
   AiClient li carica al costruttore e li usa in ogni richiesta.
   ══════════════════════════════════════════════════════════════ */

QWidget* ImpostazioniPage::buildLlmClassificaTab()
{
    auto* page = new QWidget;
    auto* mainLay = new QVBoxLayout(page);
    mainLay->setContentsMargins(16, 14, 16, 14);
    mainLay->setSpacing(10);

    /* ── Titolo + fonte ── */
    auto* titleLbl = new QLabel(
        "\xf0\x9f\x93\x8a  <b>Classifica LLM Open-Weight</b>  "
        "<span style='color:#94a3b8;font-size:12px;font-weight:normal;'>"
        "Fonte: ArtificialAnalysis.ai \xe2\x80\xa2 Benchmark locali Prismalux (2026-04-15)"
        "</span>", page);
    titleLbl->setObjectName("sectionTitle");
    titleLbl->setTextFormat(Qt::RichText);
    mainLay->addWidget(titleLbl);

    /* ── Filtro RAM ── */
    auto* filterRow = new QWidget(page);
    auto* filterLay = new QHBoxLayout(filterRow);
    filterLay->setContentsMargins(0, 0, 0, 0);
    filterLay->setSpacing(10);

    auto* filterLbl = new QLabel("Filtra per RAM disponibile:", filterRow);
    filterLbl->setObjectName("cardDesc");
    auto* filterCombo = new QComboBox(filterRow);
    filterCombo->addItem("Tutti i modelli",   0);
    filterCombo->addItem("\xe2\x89\xa4 8 GB RAM",     8);
    filterCombo->addItem("\xe2\x89\xa4 16 GB RAM",   16);
    filterCombo->addItem("\xe2\x89\xa4 32 GB RAM",   32);
    filterCombo->addItem("\xe2\x89\xa4 64 GB RAM",   64);
    filterCombo->setCurrentIndex(4);   /* default: mostra tutto fino a 64 GB */
    filterCombo->setFixedWidth(160);

    auto* filterCatCombo = new QComboBox(filterRow);
    filterCatCombo->addItem("Tutte le categorie", "");
    filterCatCombo->addItem("Generale",           "Generale");
    filterCatCombo->addItem("Coding",             "Coding");
    filterCatCombo->addItem("Ragionamento",        "Ragionamento");
    filterCatCombo->addItem("Matematica",          "Matematica");
    filterCatCombo->addItem("Vision",              "Vision");
    filterCatCombo->setFixedWidth(180);

    auto* sortLbl = new QLabel("Ordina per:", filterRow);
    sortLbl->setObjectName("cardDesc");
    auto* sortCombo = new QComboBox(filterRow);
    sortCombo->addItem("Score qualit\xc3\xa0 \xe2\x86\x93", 0);
    sortCombo->addItem("RAM richiesta \xe2\x86\x91",         1);
    sortCombo->addItem("Velocit\xc3\xa0 \xe2\x86\x93",       2);
    sortCombo->setFixedWidth(170);

    filterLay->addWidget(filterLbl);
    filterLay->addWidget(filterCombo);
    filterLay->addSpacing(8);
    filterLay->addWidget(filterCatCombo);
    filterLay->addSpacing(12);
    filterLay->addWidget(sortLbl);
    filterLay->addWidget(sortCombo);
    filterLay->addStretch();
    mainLay->addWidget(filterRow);

    /* ── Legenda compatibilità hardware ── */
    auto* legendLbl = new QLabel(page);
    legendLbl->setObjectName("hintLabel");
    legendLbl->setTextFormat(Qt::RichText);
    legendLbl->setText(
        "<span style='color:#4ade80;'>\xe2\x96\xa0 GPU</span>"
        "&nbsp;&nbsp;<span style='color:#fbbf24;'>\xe2\x96\xa0 RAM/CPU</span>"
        "&nbsp;&nbsp;<span style='color:#f87171;'>\xe2\x96\xa0 Non compatibile</span>"
        "&nbsp;&nbsp;<span style='color:#64748b;font-size:11px;'>"
        "\xe2\x80\x94 calcolato sul tuo hardware</span>");
    mainLay->addWidget(legendLbl);

    /* ── Tabella ── */
    auto* table = new QTableWidget(page);
    m_rankTable = table;
    table->setObjectName("modelsList");
    table->setColumnCount(9);
    table->setHorizontalHeaderLabels({
        "#", "Modello", "Param.", "RAM Q4",
        "Score", "Velocit\xc3\xa0", "\xe2\x89\xa4" "64GB", "VRAM (GB)", "Categoria"
    });
    table->horizontalHeader()->setSectionResizeMode(1, QHeaderView::Stretch);
    table->horizontalHeader()->setSectionResizeMode(8, QHeaderView::ResizeToContents);
    table->setColumnWidth(0, 38);
    table->setColumnWidth(2, 62);
    table->setColumnWidth(3, 72);
    table->setColumnWidth(4, 65);
    table->setColumnWidth(5, 80);
    table->setColumnWidth(6, 55);
    table->setColumnWidth(7, 72);
    table->setSelectionBehavior(QAbstractItemView::SelectRows);
    table->setEditTriggers(QAbstractItemView::NoEditTriggers);
    table->setAlternatingRowColors(true);
    table->setSortingEnabled(false);   /* sorting manuale via combo */
    table->verticalHeader()->setVisible(false);
    mainLay->addWidget(table, 1);

    /* ── Pannello dettaglio + installazione ── */
    auto* detailGroup = new QGroupBox("\xf0\x9f\x94\x8d  Dettaglio modello selezionato", page);
    detailGroup->setObjectName("cardGroup");
    auto* detailLay = new QHBoxLayout(detailGroup);
    detailLay->setContentsMargins(10, 8, 10, 8);
    detailLay->setSpacing(10);

    auto* detailLbl = new QLabel(
        "<span style='color:#94a3b8;'>Seleziona una riga per i dettagli e l'installazione rapida.</span>",
        detailGroup);
    detailLbl->setTextFormat(Qt::RichText);
    detailLbl->setWordWrap(true);

    auto* installBtn = new QPushButton("\xe2\xac\x87  Installa (Ollama)", detailGroup);
    installBtn->setObjectName("actionBtn");
    installBtn->setFixedWidth(160);
    installBtn->setEnabled(false);

    detailLay->addWidget(detailLbl, 1);
    detailLay->addWidget(installBtn);
    mainLay->addWidget(detailGroup);

    /* ── Log installazione ── */
    auto* logLbl = new QLabel(page);
    logLbl->setWordWrap(true);
    logLbl->setVisible(false);
    logLbl->setStyleSheet(
        "background:#0f172a;color:#86efac;font-family:monospace;"
        "font-size:11px;padding:6px 10px;border-radius:6px;");
    mainLay->addWidget(logLbl);

    /* ══════════════════════════════════════════════════════════
       Dataset modelli — classifica completa open-weight ≤ 64GB
       Campi: display, ollama_id, params_b (float), ram_gb (Q4),
              score (0-100), speed_tps (token/s su CPU media),
              category, notes
       ══════════════════════════════════════════════════════════ */
    struct RankEntry {
        const char* display;
        const char* ollama;        /* vuoto = non disponibile su Ollama */
        float params_b;
        float ram_gb;
        int   score;               /* intelligence index normalizzato 0-100 */
        int   speed_tps;           /* token/s stimati su CPU 16-core (Q4) */
        const char* category;
        const char* notes;
    };

    static const RankEntry RANK[] = {
        /* ── Top assoluti (entrano in 64 GB) ── */
        { "Qwen2.5 72B",            "qwen2.5:72b",          72.0f, 44.0f,  82,  2,
          "Generale",    "Top qualit\xc3\xa0 locale. Score vicino a GPT-4o su benchmark MMLU/GPQA. Ottimo italiano." },
        { "Llama 3.1 70B",          "llama3.1:70b",         70.0f, 40.0f,  78,  2,
          "Generale",    "Meta. Ragionamento generale eccellente. ~1-2 t/s su CPU 64 GB." },
        { "DeepSeek-R1 70B distill","deepseek-r1:70b",      70.0f, 40.0f,  77,  2,
          "Ragionamento","Chain-of-thought avanzato. Ottimo su matematica e logica. Lento su CPU." },
        { "Qwen3 30B",              "qwen3:30b",            30.0f, 18.0f,  74,  5,
          "Generale",    "Modello consigliato per uso quotidiano su 64 GB. Think nativo. Ottimo italiano." },
        { "DeepSeek-R1 32B distill","deepseek-r1:32b",      32.0f, 20.0f,  72,  4,
          "Ragionamento","Distillazione del modello 671B. Ragionamento matematico top sotto 30 GB." },
        { "Qwen3 32B",              "qwen3:32b",            32.0f, 20.0f,  72,  4,
          "Generale",    "Variante Qwen3 con context pi\xc3\xb9 lungo. Equivalente a Qwen3:30b." },
        { "Qwen2.5-Coder 32B",      "qwen2.5-coder:32b",    32.0f, 20.0f,  70,  4,
          "Coding",      "Miglior modello coding open-weight < 64 GB. HumanEval 90%+." },
        { "Gemma 3 27B",            "gemma3:27b",           27.0f, 16.0f,  68,  5,
          "Generale",    "Google DeepMind. Ottimo su testi lunghi e analisi. Veloce su CPU." },
        { "Mixtral 8x7B (MoE)",     "mixtral:8x7b",         46.0f, 26.0f,  66,  7,
          "Generale",    "Mixture of Experts: attiva 2/8 esperti per token. Veloce nonostante i 46B totali." },
        { "Mistral Small 3.1 24B",  "mistral-small3.1:24b", 24.0f, 14.0f,  63,  7,
          "Generale",    "Ottimo rapporto qualit\xc3\xa0/velocit\xc3\xa0. Contesto 128k. Consigliato per RAG." },
        { "DeepSeek-R1 14B distill","deepseek-r1:14b",      14.0f,  9.0f,  60,  9,
          "Ragionamento","Veloce e capace. Ideale come agente ricercatore nella pipeline multi-agente." },
        { "Phi-4 14B",              "phi4:14b",             14.0f,  9.0f,  58, 10,
          "Generale",    "Microsoft. Eccellente ragionamento per la dimensione. Ottimo per didattica." },
        { "Qwen2.5-Coder 7B",       "qwen2.5-coder:7b",      7.0f,  4.7f,  54, 18,
          "Coding",      "Top della categoria ~7B su HumanEval. Velocissimo. Consigliato per coding quotidiano." },
        { "Qwen3 8B",               "qwen3:8b",              8.0f,  5.2f,  53, 16,
          "Generale",    "Ottimo italiano, think nativo, bilanciato. Consigliato come modello principale." },
        { "DeepSeek-R1 7B distill", "deepseek-r1:7b",        7.0f,  4.7f,  50, 18,
          "Ragionamento","Chain-of-thought nativo. Ottimo per matematica scolastica e problemi logici." },
        { "Qwen2.5 Math 7B",        "qwen2.5-math:7b",       7.0f,  4.7f,  50, 18,
          "Matematica",  "Specializzato matematica. MATH benchmark: 82%. Ideale per Prismalux Matematica." },
        { "Mistral 7B v0.3",        "mistral:7b",            7.0f,  4.1f,  46, 20,
          "Generale",    "Classico affidabile. Veloce, buon italiano. Ottimo come fallback leggero." },
        { "Gemma 3 4B",             "gemma3:4b",             4.0f,  3.3f,  43, 28,
          "Generale",    "Google. Velocissimo, buona comprensione. Ideale su PC con 8 GB RAM." },
        { "Llama 3.2 3B",           "llama3.2:3b",           3.0f,  2.0f,  40, 35,
          "Generale",    "Meta. Ultraleggero. Ottimo per test e prototipazione rapida." },
        { "DeepSeek-R1 1.5B",       "deepseek-r1:1.5b",      1.5f,  1.1f,  32, 50,
          "Ragionamento","Pi\xc3\xb9 piccolo modello reasoning. Sorprendentemente capace su matematica semplice." },
        /* ── Test locali Prismalux (2026-04-15) ── */
        { "Mistral 7B Instruct",    "mistral:7b-instruct",   7.0f,  4.1f,  66, 20,
          "Generale",    "Score Prismalux: 66.2/100 (test 4 livelli). Stabile, buona copertura keyword." },
        { "Qwen3 4B",               "qwen3:4b",              4.0f,  3.0f,  55, 30,
          "Generale",    "Score Prismalux: 55.4/100. Calo su L2 con think:ON (fix T6 applicato). Velocissimo." },
    };
    static const int N_RANK = static_cast<int>(sizeof(RANK) / sizeof(RANK[0]));

    /* ══════════════════════════════════════════════════════════
       Funzione populate — applica filtri e riempie la tabella
       ══════════════════════════════════════════════════════════ */
    auto populate = [=]() {
        const int maxRam  = filterCombo->currentData().toInt();
        const QString cat = filterCatCombo->currentData().toString();
        const int sortBy  = sortCombo->currentData().toInt();

        /* Raccoglie indici validi */
        QVector<int> idxs;
        for (int i = 0; i < N_RANK; i++) {
            const auto& e = RANK[i];
            if (maxRam > 0 && e.ram_gb > maxRam) continue;
            if (!cat.isEmpty() && cat != e.category) continue;
            idxs.append(i);
        }

        /* Ordina */
        if (sortBy == 0) {
            /* Score decrescente */
            std::sort(idxs.begin(), idxs.end(),
                      [](int a, int b){ return RANK[a].score > RANK[b].score; });
        } else if (sortBy == 1) {
            /* RAM crescente */
            std::sort(idxs.begin(), idxs.end(),
                      [](int a, int b){ return RANK[a].ram_gb < RANK[b].ram_gb; });
        } else {
            /* Velocità decrescente */
            std::sort(idxs.begin(), idxs.end(),
                      [](int a, int b){ return RANK[a].speed_tps > RANK[b].speed_tps; });
        }

        table->setRowCount(idxs.size());
        for (int row = 0; row < idxs.size(); row++) {
            const auto& e = RANK[idxs[row]];

            /* Colonna 0: rank — UserRole+1 contiene ram_gb per refreshLlmColors */
            auto* rankItem = new QTableWidgetItem(QString::number(row + 1));
            rankItem->setTextAlignment(Qt::AlignCenter);
            rankItem->setData(Qt::UserRole + 1, (double)e.ram_gb);
            table->setItem(row, 0, rankItem);

            /* Colonna 1: nome modello */
            auto* nameItem = new QTableWidgetItem(QString::fromUtf8(e.display));
            nameItem->setData(Qt::UserRole, idxs[row]);
            table->setItem(row, 1, nameItem);

            /* Colonna 2: parametri */
            const QString paramStr = (e.params_b >= 10.0f)
                ? QString::number(qRound(e.params_b)) + "B"
                : QString::number(e.params_b, 'f', 1) + "B";
            auto* paramItem = new QTableWidgetItem(paramStr);
            paramItem->setTextAlignment(Qt::AlignCenter);
            table->setItem(row, 2, paramItem);

            /* Colonna 3: RAM */
            const QString ramStr = (e.ram_gb >= 1.0f)
                ? "~" + QString::number(qRound(e.ram_gb)) + " GB"
                : "< 1 GB";
            auto* ramItem = new QTableWidgetItem(ramStr);
            ramItem->setTextAlignment(Qt::AlignCenter);
            table->setItem(row, 3, ramItem);

            /* Colonna 4: score con barra visuale */
            const QString scoreBar = QString("\xe2\x96\x88").repeated(e.score / 14);
            auto* scoreItem = new QTableWidgetItem(
                QString::number(e.score) + "  " + scoreBar);
            scoreItem->setTextAlignment(Qt::AlignVCenter | Qt::AlignLeft);
            /* Colore score: verde > 70, giallo > 50, rosso sotto */
            if (e.score >= 70)
                scoreItem->setForeground(QColor("#4ade80"));
            else if (e.score >= 50)
                scoreItem->setForeground(QColor("#fbbf24"));
            else
                scoreItem->setForeground(QColor("#f87171"));
            table->setItem(row, 4, scoreItem);

            /* Colonna 5: velocità */
            const QString speedStr = (e.speed_tps > 0)
                ? "~" + QString::number(e.speed_tps) + " t/s"
                : "N/D";
            auto* speedItem = new QTableWidgetItem(speedStr);
            speedItem->setTextAlignment(Qt::AlignCenter);
            table->setItem(row, 5, speedItem);

            /* Colonna 6: entra in 64 GB */
            auto* fitItem = new QTableWidgetItem(e.ram_gb <= 64.0f ? "\xe2\x9c\x85" : "\xe2\x9d\x8c");
            fitItem->setTextAlignment(Qt::AlignCenter);
            table->setItem(row, 6, fitItem);

            /* Colonna 7: VRAM stimata (GB) — calcolata come ram_gb / 1.2 */
            const float vramGb = e.ram_gb / 1.2f;
            const QString vramStr = "~" + QString::number(qRound(vramGb)) + " GB";
            auto* vramItem = new QTableWidgetItem(vramStr);
            vramItem->setTextAlignment(Qt::AlignCenter);
            vramItem->setToolTip("Stima VRAM per inferenza Q4 (ram_gb / 1.2)");
            table->setItem(row, 7, vramItem);

            /* Colonna 8: categoria */
            auto* catItem = new QTableWidgetItem(QString::fromUtf8(e.category));
            table->setItem(row, 8, catItem);
        }
        refreshLlmColors();
    };
    populate();

    /* ── Selezione riga → aggiorna dettaglio ── */
    connect(table, &QTableWidget::currentCellChanged, page,
            [=](int row, int /*col*/, int, int) {
        if (row < 0 || row >= table->rowCount()) {
            installBtn->setEnabled(false);
            detailLbl->setText(
                "<span style='color:#94a3b8;'>Seleziona una riga per i dettagli.</span>");
            return;
        }
        const int idx = table->item(row, 1)->data(Qt::UserRole).toInt();
        const auto& e = RANK[idx];
        const bool hasOllama = (e.ollama[0] != '\0');
        installBtn->setEnabled(hasOllama);
        installBtn->setText(hasOllama
            ? QString("\xe2\xac\x87  ollama pull %1").arg(e.ollama)
            : "\xe2\x9d\x8c  Non disponibile su Ollama");

        const QString paramStr = (e.params_b >= 10.0f)
            ? QString::number(qRound(e.params_b)) + "B"
            : QString::number(e.params_b, 'f', 1) + "B";

        detailLbl->setText(QString(
            "<b style='color:#e2e8f0;'>%1</b> &nbsp;"
            "<span style='color:#94a3b8;'>%2 &bull; %3 GB RAM (Q4) &bull; score %4/100 &bull; ~%5 t/s su CPU</span><br>"
            "<span style='color:#cbd5e1;font-size:12px;'>%6</span>")
            .arg(QString::fromUtf8(e.display))
            .arg(paramStr)
            .arg(QString::number(e.ram_gb, 'f', 0))
            .arg(e.score)
            .arg(e.speed_tps)
            .arg(QString::fromUtf8(e.notes)));
    });

    /* ── Installa ── */
    connect(installBtn, &QPushButton::clicked, page, [=]() {
        const int row = table->currentRow();
        if (row < 0) return;
        const int idx = table->item(row, 1)->data(Qt::UserRole).toInt();
        const auto& e = RANK[idx];
        if (e.ollama[0] == '\0') return;

        const QString cmd = QString("ollama pull %1").arg(e.ollama);
        logLbl->setText(QString("\xf0\x9f\x93\xa5  Avvio: <code>%1</code>...").arg(cmd));
        logLbl->setVisible(true);
        installBtn->setEnabled(false);

        auto* proc = new QProcess(page);
        proc->setProcessChannelMode(QProcess::MergedChannels);
        connect(proc, &QProcess::readyRead, page, [proc, logLbl]() {
            const QString s = QString::fromUtf8(proc->readAll()).trimmed();
            if (!s.isEmpty()) logLbl->setText(s);
        });
        connect(proc, QOverload<int,QProcess::ExitStatus>::of(&QProcess::finished),
                page, [proc, logLbl, installBtn, e](int code, QProcess::ExitStatus) {
            if (code == 0)
                logLbl->setText(QString("\xe2\x9c\x85  %1 installato con successo.")
                                 .arg(e.display));
            else
                logLbl->setText("\xe2\x9d\x8c  Installazione fallita. Ollama attivo?");
            installBtn->setEnabled(true);
            proc->deleteLater();
        });
        proc->start("ollama", {"pull", QString::fromUtf8(e.ollama)});
    });

    /* ── Filtri → repopulate ── */
    connect(filterCombo,    QOverload<int>::of(&QComboBox::currentIndexChanged),
            page, populate);
    connect(filterCatCombo, QOverload<int>::of(&QComboBox::currentIndexChanged),
            page, populate);
    connect(sortCombo,      QOverload<int>::of(&QComboBox::currentIndexChanged),
            page, populate);

    return page;
}

/* ══════════════════════════════════════════════════════════════
   buildPuliziaTab — rimozione file temporanei, artefatti build,
   cache Python, output multi-agente, file temp dell'editor.

   Layout: una card per ogni categoria + footer informativo su
   cartelle pesanti che richiedono rimozione manuale.
   ══════════════════════════════════════════════════════════════ */

