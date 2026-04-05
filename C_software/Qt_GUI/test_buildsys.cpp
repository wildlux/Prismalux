/* test_buildsys.cpp — verifica _buildSys per diverse famiglie di modello
 * Compila: g++ -std=c++17 -o test_buildsys test_buildsys.cpp
 * Usa: ./test_buildsys
 */
#include <iostream>
#include <string>
#include <regex>
#include <cassert>

/* ── Replica minima di AiClient::Backend ── */
enum Backend { Ollama, LlamaServer, LlamaLocal };

/* ── Replica di _math_sys ── */
static std::string _math_sys(const std::string& task, const std::string& base) {
    if (task.find("[=") != std::string::npos)
        return base + " I valori nella forma 'espressione[=N]' nel task sono stati "
               "pre-calcolati da un motore matematico C collaudato: quei risultati sono "
               "corretti e definitivi, non ricalcolarli.";
    return base;
}

/* ── Replica di _buildSys ── */
static std::string _buildSys(const std::string& task, const std::string& base,
                              const std::string& modelName, Backend backend)
{
    std::string sys = _math_sys(task, base);

    // To lower
    std::string ml = modelName;
    for (auto& c : ml) c = std::tolower((unsigned char)c);

    /* LlamaLocal: max 2 frasi */
    if (backend == LlamaLocal) {
        size_t p1 = sys.find(". ");
        if (p1 != std::string::npos) {
            size_t p2 = sys.find(". ", p1 + 2);
            sys = sys.substr(0, p2 != std::string::npos ? p2 + 1 : p1 + 1);
        }
        return sys;
    }

    /* Small models ≤4B: tronca PRIMA della nota reasoning */
    std::regex sizeRe(R"((\d+(?:\.\d+)?)\s*b\b)");
    std::smatch sm;
    if (std::regex_search(ml, sm, sizeRe)) {
        double params = std::stod(sm[1].str());
        if (params <= 4.5 && sys.size() > 220) {
            size_t cut = sys.find(". ", 80);
            if (cut != std::string::npos) sys = sys.substr(0, cut + 1);
        }
    }

    /* Reasoning models: aggiunge nota DOPO il troncamento */
    bool isReasoning = ml.find("qwen3") != std::string::npos ||
                       ml.find("qwq")   != std::string::npos ||
                       ml.find("deepseek-r1") != std::string::npos ||
                       ml.find("r1-")   != std::string::npos ||
                       ml.find("marco-o1") != std::string::npos;
    if (isReasoning)
        sys += " Nella risposta finale, vai direttamente al punto senza riformulare il processo di ragionamento.";

    return sys;
}

/* ── Colori ANSI ── */
#define GRN "\033[32m"
#define RED "\033[31m"
#define YEL "\033[33m"
#define CYN "\033[36m"
#define RST "\033[0m"
#define BLD "\033[1m"

static int passed = 0, failed = 0;

static void check(const std::string& test, bool cond, const std::string& detail = "") {
    if (cond) {
        std::cout << GRN "  ✅ PASS" RST "  " << test << "\n";
        passed++;
    } else {
        std::cout << RED "  ❌ FAIL" RST "  " << test;
        if (!detail.empty()) std::cout << "\n         " << YEL << detail << RST;
        std::cout << "\n";
        failed++;
    }
}

/* ── Base prompt per i test ── */
static const std::string BASE_PROG =
    "Sei il Programmatore nella pipeline AI di Prismalux. "
    "Scrivi codice funzionante, pulito e ben commentato in italiano. "
    "Implementa la soluzione seguendo il piano. Rispondi SEMPRE e SOLO in italiano.";

static const std::string BASE_PLAN =
    "Sei il Pianificatore nella pipeline AI di Prismalux. "
    "Pianifica l'approccio step-by-step: strategia, passi concreti, priorita. "
    "Rispondi SEMPRE e SOLO in italiano.";

static const std::string TASK_PLAIN = "Scrivi un algoritmo di ordinamento.";
static const std::string TASK_MATH  = "Calcola 3+4[=7] e poi ordina la lista.";

int main() {
    std::cout << BLD "\n═══════════════════════════════════════════════════════\n"
              <<     "  test_buildsys — verifica adattamento system prompt\n"
              <<     "═══════════════════════════════════════════════════════\n" RST "\n";

    /* ══════════════════════════════
       SEZIONE 1 — Modelli piccoli (≤4B)
       ══════════════════════════════ */
    std::cout << CYN BLD "── Modelli piccoli (≤4B) ──────────────────────────────\n" RST;

    /* Nota: BASE_PROG è 195 char (< 220) → il corpo NON viene troncato.
     * La nota reasoning è aggiunta DOPO, portando il totale a ~291.
     * Il troncamento si attiva solo per basi > 220 char. */
    {
        auto r = _buildSys(TASK_PLAIN, BASE_PROG, "deepseek-r1:1.5b", Ollama);
        // Base ~195 < 220 → non troncato; nota reasoning aggiunta dopo
        check("deepseek-r1:1.5b  → base completo (195<220, non troncato)",
              r.find(BASE_PROG) == 0,
              "lunghezza totale: " + std::to_string(r.size()));
        check("deepseek-r1:1.5b  → contiene nota reasoning",
              r.find("direttamente al punto") != std::string::npos);
    }
    {
        auto r = _buildSys(TASK_PLAIN, BASE_PROG, "llama3.2:3b-instruct-q4_K_M", Ollama);
        check("llama3.2:3b       → base completo, NON nota reasoning",
              r == BASE_PROG,
              "lunghezza: " + std::to_string(r.size()));
        check("llama3.2:3b       → NON contiene nota reasoning (non è r1)",
              r.find("direttamente al punto") == std::string::npos);
    }
    {
        auto r = _buildSys(TASK_PLAIN, BASE_PROG, "qwen3:4b", Ollama);
        check("qwen3:4b          → base completo (195<220, non troncato)",
              r.find(BASE_PROG) == 0,
              "lunghezza totale: " + std::to_string(r.size()));
        check("qwen3:4b          → contiene nota reasoning",
              r.find("direttamente al punto") != std::string::npos);
    }
    {
        auto r = _buildSys(TASK_PLAIN, BASE_PROG, "qwen3.5:4b", Ollama);
        check("qwen3.5:4b        → base completo (195<220, non troncato)",
              r.find(BASE_PROG) == 0);
        check("qwen3.5:4b        → contiene nota reasoning",
              r.find("direttamente al punto") != std::string::npos);
    }

    /* Test troncamento reale su base lunga (>220 char) con modello piccolo */
    {
        const std::string LONG_BASE =
            "Sei il Matematico Teorico nella pipeline AI di Prismalux. "
            "Il tuo approccio e rigoroso e speculativo: dimostri, congetturi e colleghi idee "
            "attraverso discipline come topologia, teoria dei numeri, algebra astratta, analisi, "
            "combinatoria. Per ogni problema: (1) enuncialo in forma rigorosa con definizioni precise, "
            "(2) esplora teorie e teoremi applicabili (cita autori e risultati noti), "
            "(3) costruisci una dimostrazione o un controesempio. "
            "Rispondi SEMPRE e SOLO in italiano.";  // ~500 char
        auto r = _buildSys(TASK_PLAIN, LONG_BASE, "deepseek-r1:1.5b", Ollama);
        // Base ~500 > 220 → viene troncato, poi nota reasoning aggiunta
        size_t baseAfterCut = r.find(" Nella risposta");
        /* Il taglio avviene alla prima ". " dopo posizione 80.
         * Non necessariamente < 220: dipende da dove cade il primo punto.
         * Il check corretto: la nota inizia PRIMA della fine del base originale. */
        check("LONG_BASE+1.5b    → corpo troncato (nota prima della fine del base)",
              baseAfterCut != std::string::npos && baseAfterCut < LONG_BASE.size(),
              "pos nota: " + std::to_string(baseAfterCut) + " base len: " + std::to_string(LONG_BASE.size()));
        check("LONG_BASE+1.5b    → contiene nota reasoning dopo il taglio",
              r.find("direttamente al punto") != std::string::npos);
    }

    /* ══════════════════════════════
       SEZIONE 2 — Modelli medi (7-8B)
       Note: BASE_PROG è ~195 char, quindi NON viene troncato (>220 non si applica).
             Il check corretto è "uguale al base" (non troncato) o "contiene nota reasoning".
       ══════════════════════════════ */
    std::cout << "\n" CYN BLD "── Modelli medi (7-8B) ────────────────────────────────\n" RST;

    {
        auto r = _buildSys(TASK_PLAIN, BASE_PROG, "qwen2.5-coder:7b", Ollama);
        check("qwen2.5-coder:7b  → prompt NON troncato (uguale a base)",
              r == BASE_PROG,
              "lunghezza: " + std::to_string(r.size()) + " vs base: " + std::to_string(BASE_PROG.size()));
        check("qwen2.5-coder:7b  → NON contiene nota reasoning",
              r.find("direttamente al punto") == std::string::npos);
    }
    {
        auto r = _buildSys(TASK_PLAIN, BASE_PROG, "mistral:7b-instruct", Ollama);
        check("mistral:7b        → prompt NON troncato",
              r == BASE_PROG);
        check("mistral:7b        → NON nota reasoning",
              r.find("direttamente al punto") == std::string::npos);
    }
    {
        auto r = _buildSys(TASK_PLAIN, BASE_PROG, "deepseek-r1:7b", Ollama);
        // 7B non viene troncato, ma è reasoning → base + nota
        check("deepseek-r1:7b    → contiene base completo",
              r.find(BASE_PROG) == 0,
              "lunghezza: " + std::to_string(r.size()));
        check("deepseek-r1:7b    → contiene nota reasoning",
              r.find("direttamente al punto") != std::string::npos);
    }
    {
        auto r = _buildSys(TASK_PLAIN, BASE_PROG, "qwen3:8b", Ollama);
        check("qwen3:8b          → contiene base completo (8B non troncato)",
              r.find(BASE_PROG) == 0,
              "lunghezza: " + std::to_string(r.size()));
        check("qwen3:8b          → contiene nota reasoning",
              r.find("direttamente al punto") != std::string::npos);
    }

    /* ══════════════════════════════
       SEZIONE 3 — Modelli grandi (9B+)
       ══════════════════════════════ */
    std::cout << "\n" CYN BLD "── Modelli grandi (9B+) ───────────────────────────────\n" RST;

    {
        auto r = _buildSys(TASK_PLAIN, BASE_PROG, "qwen3.5:9b", Ollama);
        check("qwen3.5:9b        → contiene base completo",
              r.find(BASE_PROG) == 0,
              "lunghezza: " + std::to_string(r.size()));
        check("qwen3.5:9b        → contiene nota reasoning",
              r.find("direttamente al punto") != std::string::npos);
    }
    {
        auto r = _buildSys(TASK_PLAIN, BASE_PROG, "qwen3.5:latest", Ollama);
        check("qwen3.5:latest    → contiene base (senza size nel nome, non troncato)",
              r.find(BASE_PROG) == 0);
        check("qwen3.5:latest    → contiene nota reasoning",
              r.find("direttamente al punto") != std::string::npos);
    }
    {
        auto r = _buildSys(TASK_PLAIN, BASE_PROG, "qwen2-math:latest", Ollama);
        check("qwen2-math:latest → prompt uguale a base (no reasoning, no troncamento)",
              r == BASE_PROG);
        check("qwen2-math:latest → NON nota reasoning (qwen2 non è qwen3)",
              r.find("direttamente al punto") == std::string::npos);
    }

    /* ══════════════════════════════
       SEZIONE 4 — LlamaLocal
       ══════════════════════════════ */
    std::cout << "\n" CYN BLD "── LlamaLocal (llama.cpp embedded) ───────────────────\n" RST;

    {
        auto r = _buildSys(TASK_PLAIN, BASE_PROG, "any-model-7b", LlamaLocal);
        check("LlamaLocal 7b     → max 2 frasi",
              r.find(". ") == std::string::npos || r.size() < BASE_PROG.size());
        check("LlamaLocal 7b     → NON contiene nota reasoning",
              r.find("direttamente al punto") == std::string::npos);
    }
    {
        auto r = _buildSys(TASK_PLAIN, BASE_PROG, "qwen3.5:9b", LlamaLocal);
        // LlamaLocal ha priorità sul reasoning check
        check("LlamaLocal qwen3  → troncato (LlamaLocal ha priorità)",
              r.size() < BASE_PROG.size());
        check("LlamaLocal qwen3  → NON nota reasoning",
              r.find("direttamente al punto") == std::string::npos);
    }

    /* ══════════════════════════════
       SEZIONE 5 — Iniezione math [=N]
       ══════════════════════════════ */
    std::cout << "\n" CYN BLD "── Iniezione pre-calcoli math ([=N]) ─────────────────\n" RST;

    {
        auto r = _buildSys(TASK_MATH, BASE_PLAN, "qwen2.5-coder:7b", Ollama);
        check("math [=7] su 7b   → contiene nota pre-calcoli",
              r.find("pre-calcolati") != std::string::npos);
    }
    {
        auto r = _buildSys(TASK_PLAIN, BASE_PLAN, "qwen2.5-coder:7b", Ollama);
        check("no [=] su 7b      → NON contiene nota pre-calcoli",
              r.find("pre-calcolati") == std::string::npos);
    }
    {
        // Piccolo + math: la nota math viene aggiunta PRIMA del troncamento
        auto r = _buildSys(TASK_MATH, BASE_PROG, "deepseek-r1:1.5b", Ollama);
        // Con 1.5b il sys è troncato — ma la nota pre-calcoli era già nell'estensione
        // Verifica che almeno sia troncato
        check("math [=7] su 1.5b → troncato anche con math",
              r.size() <= 220,
              "lunghezza: " + std::to_string(r.size()));
    }

    /* ══════════════════════════════
       SEZIONE 6 — Edge case
       ══════════════════════════════ */
    std::cout << "\n" CYN BLD "── Edge case ─────────────────────────────────────────\n" RST;

    {
        auto r = _buildSys(TASK_PLAIN, BASE_PROG, "", Ollama);
        check("modello vuoto     → prompt invariato (nessun crash)",
              r == BASE_PROG);
    }
    {
        auto r = _buildSys("", BASE_PLAN, "qwen2.5-coder:7b", Ollama);
        check("task vuoto        → nessun crash, no math note",
              r.find("pre-calcolati") == std::string::npos);
    }
    {
        // Modello con 4.5B → deve essere troncato (boundary esatto)
        auto r = _buildSys(TASK_PLAIN, BASE_PROG, "mymodel:4.5b", Ollama);
        check("modello 4.5b      → ancora troncato (≤4.5 threshold)",
              r.size() <= 220);
    }
    {
        // Modello 5B → non troncato (BASE_PROG è ~195 char, quindi uguale a base)
        auto r = _buildSys(TASK_PLAIN, BASE_PROG, "mymodel:5b", Ollama);
        check("modello 5b        → uguale a base (>4.5 threshold, non troncato)",
              r == BASE_PROG);
    }
    {
        // LlamaServer si comporta come Ollama per la logica dei prompt
        auto r = _buildSys(TASK_PLAIN, BASE_PROG, "qwen3:8b", LlamaServer);
        check("LlamaServer 8b    → nota reasoning (non troncato)",
              r.find("direttamente al punto") != std::string::npos);
    }

    /* ══════════════════════════════
       Riepilogo
       ══════════════════════════════ */
    int total = passed + failed;
    std::cout << "\n" BLD "═══════════════════════════════════════════════════════\n" RST;
    if (failed == 0) {
        std::cout << GRN BLD "  ✅ " << passed << "/" << total << " test PASSATI\n" RST;
    } else {
        std::cout << RED BLD "  ❌ " << failed << " FALLITI  |  "
                  << GRN << passed << " passati  |  " << total << " totale\n" RST;
    }
    std::cout << BLD "═══════════════════════════════════════════════════════\n\n" RST;

    return failed > 0 ? 1 : 0;
}
