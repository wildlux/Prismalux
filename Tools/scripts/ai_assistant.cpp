/*
 * ai_assistant.cpp — Prototipo standalone AIOrchestrator
 *
 * Unisce: AIMemory (git repo locale) + SmartRouter (LOCAL/CLOUD) + feedback 👍/👎.
 * Zero dipendenze oltre a `git` e `curl` sulla PATH.
 *
 * Compilazione:
 *   g++ -std=c++17 -O2 ai_assistant.cpp -o ai_assistant
 *   # oppure
 *   clang++ -std=c++17 -O2 ai_assistant.cpp -o ai_assistant
 *
 * Utilizzo:
 *   ./ai_assistant                          # interattivo
 *   ./ai_assistant --cloud-url URL --key K  # usa endpoint cloud OpenAI-compatible
 *   ./ai_assistant --ollama-url URL         # es. http://localhost:11434
 *
 * Struttura ~/.ai-memory/ (creata automaticamente):
 *   profile/preferences.yaml               # preferenze apprese
 *   interactions/YYYY-MM/DD-feedback.yaml  # feedback giornaliero
 *   .git/                                  # storia completa versionata
 *
 * Routing LOCAL/CLOUD (5 regole):
 *   1. Ollama non raggiungibile → CLOUD
 *   2. Keyword sensibili (password, iban, cf) → LOCAL
 *   3. Query > 1500 char → CLOUD (contesto ampio)
 *   4. Keyword complessità (analizza, dimostra, confronta) → CLOUD
 *   5. Default → LOCAL
 */

#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <ctime>
#include <iostream>
#include <sstream>
#include <string>
#include <vector>
#include <algorithm>
#include <filesystem>
#include <fstream>

namespace fs = std::filesystem;

/* ════════════════════════════════════════════════════════════════════
 *  Utility — shell/process helpers (popen)
 * ════════════════════════════════════════════════════════════════════ */

static std::string runCmd(const std::string& cmd) {
    FILE* f = popen(cmd.c_str(), "r");
    if (!f) return {};
    std::string out;
    char buf[256];
    while (fgets(buf, sizeof(buf), f))
        out += buf;
    pclose(f);
    return out;
}

/* Esegue cmd; restituisce true se exit code == 0. */
static bool runCmdOk(const std::string& cmd) {
    return system(cmd.c_str()) == 0;
}

/* ════════════════════════════════════════════════════════════════════
 *  AIMemory — repository git locale in ~/.ai-memory/
 * ════════════════════════════════════════════════════════════════════ */

class AIMemory {
public:
    explicit AIMemory(const std::string& base = "")
        : m_base(base.empty() ? (homeDir() + "/.ai-memory") : base) {}

    /* Crea la struttura se non esiste e inizializza git. */
    void initialize() {
        fs::create_directories(m_base + "/profile");
        fs::create_directories(m_base + "/interactions");

        if (!fs::exists(m_base + "/.git"))
            runCmdOk("git -C " + shellQuote(m_base) + " init -q");

        const std::string prefs = m_base + "/profile/preferences.yaml";
        if (!fs::exists(prefs)) {
            writeFile(prefs, "# Preferenze apprese da AIOrchestrator\n"
                             "response_length: medium\n"
                             "interface: cli\n");
            gitCommit("init: struttura memoria AI");
        }
    }

    /* Salva feedback 👍/👎 per query+risposta. */
    void logFeedback(const std::string& query,
                     const std::string& response,
                     bool positive,
                     const std::string& reason = "") {
        const auto [year, month, day] = today();
        const std::string dir  = m_base + "/interactions/" + year + "-" + month;
        fs::create_directories(dir);
        const std::string path = dir + "/" + day + "-feedback.yaml";

        std::ofstream f(path, std::ios::app);
        f << "---\n";
        f << "timestamp: " << isoNow() << "\n";
        f << "rating: " << (positive ? "\xf0\x9f\x91\x8d" : "\xf0\x9f\x91\x8e") << "\n";
        f << "query: |2\n  " << indent(query, 2) << "\n";
        f << "response_snippet: |2\n  " << indent(response.substr(0, 200), 2) << "\n";
        if (!reason.empty())
            f << "reason: " << reason << "\n";
        f.close();

        gitCommit(std::string("feedback: ") + (positive ? "👍" : "👎") + " — " + query.substr(0, 60));
    }

    /* Aggiorna una preferenza in profile/preferences.yaml. */
    void updatePreference(const std::string& key, const std::string& value) {
        const std::string path = m_base + "/profile/preferences.yaml";
        std::string content = readFile(path);
        /* Cerca e sostituisce la riga key: */
        const std::string pat = key + ": ";
        auto pos = content.find(pat);
        if (pos != std::string::npos) {
            auto eol = content.find('\n', pos);
            content.replace(pos, (eol == std::string::npos ? content.size() : eol) - pos,
                            pat + value);
        } else {
            content += key + ": " + value + "\n";
        }
        writeFile(path, content);
        gitCommit("learn: " + key + " = " + value);
    }

    /* Restituisce le ultime N righe di preferenze come contesto. */
    std::string getRelevantContext(const std::string& /*query*/, int lines = 20) const {
        const std::string path = m_base + "/profile/preferences.yaml";
        if (!fs::exists(path)) return {};
        const std::string content = readFile(path);
        std::istringstream ss(content);
        std::vector<std::string> all;
        std::string line;
        while (std::getline(ss, line))
            if (!line.empty() && line[0] != '#')
                all.push_back(line);
        const int start = std::max(0, (int)all.size() - lines);
        std::string out;
        for (int i = start; i < (int)all.size(); ++i)
            out += all[i] + "\n";
        return out;
    }

    /* Ultimi N commit del repo memoria. */
    std::string gitLog(int n = 10) const {
        return runCmd("git -C " + shellQuote(m_base) + " log --oneline -" + std::to_string(n) + " 2>/dev/null");
    }

private:
    std::string m_base;

    static std::string homeDir() {
        const char* h = std::getenv("HOME");
        return h ? h : ".";
    }

    static std::string shellQuote(const std::string& s) {
        return "'" + s + "'";
    }

    static std::string isoNow() {
        char buf[32];
        std::time_t t = std::time(nullptr);
        std::strftime(buf, sizeof(buf), "%Y-%m-%dT%H:%M:%S", std::localtime(&t));
        return buf;
    }

    /* Restituisce {year, month, day} come stringhe "YYYY","MM","DD". */
    static std::tuple<std::string,std::string,std::string> today() {
        char y[8], m[4], d[4];
        std::time_t t = std::time(nullptr);
        std::tm* tm = std::localtime(&t);
        std::strftime(y, sizeof(y), "%Y", tm);
        std::strftime(m, sizeof(m), "%m", tm);
        std::strftime(d, sizeof(d), "%d", tm);
        return {y, m, d};
    }

    static std::string indent(const std::string& s, int n) {
        const std::string pad(n, ' ');
        std::string out;
        for (char c : s) {
            out += c;
            if (c == '\n') out += pad;
        }
        return out;
    }

    static void writeFile(const std::string& path, const std::string& content) {
        std::ofstream f(path);
        f << content;
    }

    static std::string readFile(const std::string& path) {
        std::ifstream f(path);
        return {std::istreambuf_iterator<char>(f), {}};
    }

    void gitCommit(const std::string& msg) const {
        const std::string base = shellQuote(m_base);
        runCmdOk("git -C " + base + " add -A");
        runCmdOk("git -C " + base + " -c user.name=AIOrchestrator "
                 "-c user.email=ai@prismalux commit -q -m " + shellQuote(msg) + " 2>/dev/null");
    }
};

/* ════════════════════════════════════════════════════════════════════
 *  SmartRouter — decide LOCAL vs CLOUD
 * ════════════════════════════════════════════════════════════════════ */

enum class Route { LOCAL, CLOUD };

class SmartRouter {
public:
    SmartRouter(const std::string& ollamaUrl  = "http://localhost:11434",
                const std::string& cloudUrl   = "",
                const std::string& cloudModel = "gpt-4o-mini",
                const std::string& apiKey     = "")
        : m_ollama(ollamaUrl), m_cloud(cloudUrl),
          m_cloudModel(cloudModel), m_apiKey(apiKey) {}

    Route decide(const std::string& query) const {
        /* Regola 1: Ollama non raggiungibile → CLOUD */
        if (!ollamaAlive()) return Route::CLOUD;

        /* Regola 2: Keyword sensibili → LOCAL (privacy) */
        const std::string q = lower(query);
        for (const char* kw : kPrivacyKeywords)
            if (q.find(kw) != std::string::npos) return Route::LOCAL;

        /* Regola 3: Query lunga → CLOUD (contesto ampio) */
        if (query.size() > 1500) return Route::CLOUD;

        /* Regola 4: Keyword complessità → CLOUD */
        for (const char* kw : kComplexKeywords)
            if (q.find(kw) != std::string::npos) return Route::CLOUD;

        /* Regola 5: Default → LOCAL */
        return Route::LOCAL;
    }

    const std::string& ollamaUrl()  const { return m_ollama; }
    const std::string& cloudUrl()   const { return m_cloud; }
    const std::string& cloudModel() const { return m_cloudModel; }
    const std::string& apiKey()     const { return m_apiKey; }

private:
    std::string m_ollama, m_cloud, m_cloudModel, m_apiKey;

    static constexpr const char* kPrivacyKeywords[] = {
        "password", "iban", "codice fiscale", "carta di credito",
        "token", "api key", "chiave privata", "certificato", nullptr
    };
    static constexpr const char* kComplexKeywords[] = {
        "analizza profondamente", "dimostra che", "confronta e spiega",
        "prova formale", "ragionamento passo", nullptr
    };

    bool ollamaAlive() const {
        const std::string cmd = "curl -sf --max-time 1 " + m_ollama + "/api/tags > /dev/null 2>&1";
        return system(cmd.c_str()) == 0;
    }

    static std::string lower(const std::string& s) {
        std::string out = s;
        std::transform(out.begin(), out.end(), out.begin(), ::tolower);
        return out;
    }
};

/* ════════════════════════════════════════════════════════════════════
 *  HTTP helpers — curl per chiamate LLM
 * ════════════════════════════════════════════════════════════════════ */

static std::string escapeJson(const std::string& s) {
    std::string out;
    for (char c : s) {
        switch (c) {
            case '"':  out += "\\\""; break;
            case '\\': out += "\\\\"; break;
            case '\n': out += "\\n";  break;
            case '\r': out += "\\r";  break;
            case '\t': out += "\\t";  break;
            default:   out += c;
        }
    }
    return out;
}

/* Estrae il valore di "content" o "response" dalla risposta JSON Ollama/OpenAI. */
static std::string extractContent(const std::string& json) {
    /* Cerca "content":"..." o "response":"..." */
    for (const char* key : {"\"content\":\"", "\"response\":\""}) {
        auto pos = json.find(key);
        if (pos == std::string::npos) continue;
        pos += strlen(key);
        std::string val;
        bool esc = false;
        for (; pos < json.size(); ++pos) {
            if (esc) { val += json[pos]; esc = false; continue; }
            if (json[pos] == '\\') { esc = true; continue; }
            if (json[pos] == '"') break;
            val += json[pos];
        }
        return val;
    }
    return {};
}

static std::string callLocal(const std::string& ollamaUrl,
                             const std::string& model,
                             const std::string& system,
                             const std::string& query) {
    const std::string body =
        "{\"model\":\"" + escapeJson(model) + "\","
        "\"stream\":false,"
        "\"system\":\"" + escapeJson(system) + "\","
        "\"prompt\":\"" + escapeJson(query) + "\"}";
    const std::string cmd =
        "curl -sf --max-time 60 -X POST " + ollamaUrl + "/api/generate "
        "-H 'Content-Type: application/json' "
        "-d '" + body + "' 2>/dev/null";
    return extractContent(runCmd(cmd));
}

static std::string callCloud(const std::string& cloudUrl,
                             const std::string& model,
                             const std::string& apiKey,
                             const std::string& system,
                             const std::string& query) {
    if (cloudUrl.empty() || apiKey.empty()) return "[Cloud non configurato]";
    const std::string body =
        "{\"model\":\"" + escapeJson(model) + "\","
        "\"messages\":["
        "{\"role\":\"system\",\"content\":\"" + escapeJson(system) + "\"},"
        "{\"role\":\"user\",\"content\":\"" + escapeJson(query) + "\"}"
        "]}";
    const std::string cmd =
        "curl -sf --max-time 60 -X POST " + cloudUrl + " "
        "-H 'Content-Type: application/json' "
        "-H 'Authorization: Bearer " + apiKey + "' "
        "-d '" + body + "' 2>/dev/null";
    return extractContent(runCmd(cmd));
}

/* ════════════════════════════════════════════════════════════════════
 *  AIOrchestrator — main loop
 * ════════════════════════════════════════════════════════════════════ */

class AIOrchestrator {
public:
    AIOrchestrator(const SmartRouter& router,
                   const std::string& ollamaModel = "llama3.2",
                   const std::string& memoryBase  = "")
        : m_router(router), m_model(ollamaModel), m_memory(memoryBase) {
        m_memory.initialize();
    }

    void run() {
        std::cout << "\n=== AIOrchestrator Prismalux ===\n";
        std::cout << "Memoria: ~/.ai-memory/  |  Modello locale: " << m_model << "\n";
        std::cout << "Digita una domanda (vuoto = esci, /log = storia, /prefs = preferenze)\n\n";

        std::string line;
        while (true) {
            std::cout << "\033[1;36mTu>\033[0m ";
            if (!std::getline(std::cin, line)) break;
            if (line.empty()) break;

            if (line == "/log") {
                std::cout << m_memory.gitLog(15) << "\n"; continue;
            }
            if (line == "/prefs") {
                std::cout << m_memory.getRelevantContext(line) << "\n"; continue;
            }

            processQuery(line);
        }

        std::cout << "\nA presto!\n";
    }

private:
    SmartRouter m_router;
    std::string m_model;
    AIMemory    m_memory;

    void processQuery(const std::string& query) {
        /* Contesto personalizzato dalla memoria */
        const std::string ctx = m_memory.getRelevantContext(query);
        std::string sysPrompt = "Sei un assistente AI locale Prismalux. Rispondi in italiano.";
        if (!ctx.empty())
            sysPrompt += "\n\nPreferenze utente:\n" + ctx;

        /* Routing */
        const Route route = m_router.decide(query);
        const bool isCloud = (route == Route::CLOUD);
        std::cout << (isCloud ? "\033[33m[☁️  CLOUD]\033[0m" : "\033[32m[🏠 LOCAL]\033[0m") << " ";
        std::cout.flush();

        std::string response;
        if (!isCloud) {
            response = callLocal(m_router.ollamaUrl(), m_model, sysPrompt, query);
        } else {
            response = callCloud(m_router.cloudUrl(), m_router.cloudModel(),
                                 m_router.apiKey(), sysPrompt, query);
        }

        if (response.empty())
            response = "[Nessuna risposta ricevuta]";

        std::cout << "\n\033[1;32mAI>\033[0m " << response << "\n\n";

        /* Feedback 👍/👎 */
        std::cout << "Questa risposta è stata utile? [y/n/skip]: ";
        std::string fb;
        if (!std::getline(std::cin, fb)) return;
        if (fb == "y" || fb == "Y") {
            m_memory.logFeedback(query, response, true);
            std::cout << "  \xf0\x9f\x91\x8d registrato\n\n";
        } else if (fb == "n" || fb == "N") {
            std::cout << "Motivo (opzionale): ";
            std::string reason;
            std::getline(std::cin, reason);
            m_memory.logFeedback(query, response, false, reason);
            std::cout << "  \xf0\x9f\x91\x8e registrato\n\n";
        }
    }
};

/* ════════════════════════════════════════════════════════════════════
 *  main
 * ════════════════════════════════════════════════════════════════════ */

int main(int argc, char* argv[]) {
    std::string ollamaUrl  = "http://localhost:11434";
    std::string cloudUrl;
    std::string cloudModel = "gpt-4o-mini";
    std::string apiKey;
    std::string localModel = "llama3.2";

    for (int i = 1; i < argc; ++i) {
        const std::string a = argv[i];
        if ((a == "--ollama-url" || a == "--ollama") && i + 1 < argc)
            ollamaUrl = argv[++i];
        else if ((a == "--cloud-url" || a == "--cloud") && i + 1 < argc)
            cloudUrl = argv[++i];
        else if ((a == "--cloud-model") && i + 1 < argc)
            cloudModel = argv[++i];
        else if ((a == "--key" || a == "--api-key") && i + 1 < argc)
            apiKey = argv[++i];
        else if ((a == "--model" || a == "-m") && i + 1 < argc)
            localModel = argv[++i];
        else if (a == "--help" || a == "-h") {
            std::cout << "Utilizzo: ai_assistant [--ollama-url URL] [--cloud-url URL]\n"
                         "          [--cloud-model MODEL] [--key APIKEY] [--model MODEL]\n";
            return 0;
        }
    }

    const SmartRouter router(ollamaUrl, cloudUrl, cloudModel, apiKey);
    AIOrchestrator orchestrator(router, localModel);
    orchestrator.run();
    return 0;
}
