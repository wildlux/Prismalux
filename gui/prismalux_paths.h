#pragma once
/*
 * prismalux_paths.h — Risoluzione percorsi e costanti globali Prismalux
 * ======================================================================
 * SCOPO: unico punto di verità per tutti i path e le costanti di
 *        configurazione. Nessun hardcode nei file sorgente.
 *
 * FUNZIONAMENTO: ogni funzione tenta i candidati in ordine di priorità
 *   e restituisce il primo che esiste sul filesystem. Se nessuno esiste,
 *   restituisce il candidato principale (che potrà essere creato dopo).
 *   Questo rende il codice resiliente a riorganizzazioni della cartella.
 *
 * UTILIZZO:
 *   #include "prismalux_paths.h"
 *   namespace P = PrismaluxPaths;
 *   QString dir = P::modelsDir();
 *
 * NOTE CROSS-PLATFORM:
 *   - Windows: i binari hanno estensione .exe (aggiunta automaticamente)
 *   - Linux/macOS: estensione vuota
 *   - PRISMALUX_ROOT è iniettato da CMake (vedi CMakeLists.txt)
 */

#include <QString>
#include <QDir>
#include <QFile>
#include <QFileInfo>
#include <QFileInfoList>
#include <QCoreApplication>
#include <QProcess>
#include <QStandardPaths>
#include <QWidget>
#include <QStyle>
#include <QDateTime>
#include <QSettings>

namespace PrismaluxPaths {

/* ══════════════════════════════════════════════════════════════
   Costanti di configurazione
   ══════════════════════════════════════════════════════════════ */

/** Porta default Ollama (documentata: https://ollama.ai) */
constexpr int kOllamaPort      = 11434;

/** Porta default llama-server quando avviato dalla GUI */
constexpr int kLlamaServerPort = 8081;

/** Porta default opencode serve quando avviato dalla GUI */
constexpr int kOpenCodePort = 8092;

/** Host loopback — unico valore accettato per sicurezza */
constexpr const char* kLocalHost = "127.0.0.1";


/* ══════════════════════════════════════════════════════════════
   Risoluzione percorsi
   ══════════════════════════════════════════════════════════════ */

/**
 * root() — Radice del progetto Prismalux.
 *
 * Usa la macro PRISMALUX_ROOT iniettata da CMake quando disponibile.
 * Fallback: risale di due livelli dall'eseguibile
 *   (build/Prismalux_GUI → gui/build → gui → Prismalux)
 */
inline QString root()
{
#ifdef PRISMALUX_ROOT
    return QDir::cleanPath(QString(PRISMALUX_ROOT));
#else
    return QDir::cleanPath(
        QDir(QCoreApplication::applicationDirPath()).absoluteFilePath("../.."));
#endif
}

/**
 * modelsDir() — Cartella contenente i modelli .gguf.
 *
 * Priorità di ricerca:
 *   1. models/                  ← cartella canonica (Prismalux/models)
 *   2. llama_cpp_studio/models  ← dentro lo studio
 *
 * Se nessuna esiste restituisce la prima che potrà essere creata
 * con QDir::mkpath() dal chiamante.
 */
inline QString modelsDir()
{
    const QStringList candidates = {
        root() + "/models",
        root() + "/llama_cpp_studio/models",
    };
    for (const QString& d : candidates)
        if (QDir(d).exists()) return d;
    return candidates.first();
}

/**
 * scanGgufFiles() — Scansiona le cartelle modelli e restituisce i path
 *                   assoluti di tutti i file .gguf trovati.
 *
 * Cerca in modelsDir() e llamaStudioDir()/models, rimuove i duplicati.
 * Usare questa funzione in tutti i punti dove si costruisce una lista
 * di modelli per evitare logiche di scansione divergenti.
 */
inline QStringList scanGgufFiles()
{
    /* Cartelle da ispezionare in ordine */
    const QStringList dirs = {
        modelsDir(),
        root() + "/llama_cpp_studio/models",
    };

    QStringList result;
    for (const QString& dirPath : dirs) {
        const QDir d(dirPath);
        if (!d.exists()) continue;
        for (const QFileInfo& fi : d.entryInfoList({"*.gguf", "*.bin"}, QDir::Files))
            result << fi.absoluteFilePath();
    }
    result.removeDuplicates();
    return result;
}

/* ── Estensione binari ─────────────────────────────────────────
   Inline per evitare la ripetizione del blocco #ifdef nei file
   che cercano i binari di llama.cpp.                           */
inline QString exeExt()
{
#ifdef Q_OS_WIN
    return QStringLiteral(".exe");
#else
    return QString();
#endif
}

/**
 * llamaServerBin() — Path al binario llama-server.
 *
 * Cerca prima dentro llama_cpp_studio/ (compilazione tramite aggiorna.sh --llama-studio),
 * poi in llama.cpp/ alla radice.
 */
inline QString llamaServerBin()
{
    const QString ext = exeExt();
    const QStringList candidates = {
        root() + "/llama_cpp_studio/llama.cpp/build/bin/llama-server" + ext,
        root() + "/llama.cpp/build/bin/llama-server" + ext,
    };
    for (const QString& b : candidates)
        if (QFileInfo::exists(b)) return b;
    return candidates.first();
}

/**
 * llamaCliBin() — Path al binario llama-cli (modalità interattiva).
 */
inline QString llamaCliBin()
{
    const QString ext = exeExt();
    const QStringList candidates = {
        root() + "/llama_cpp_studio/llama.cpp/build/bin/llama-cli" + ext,
        root() + "/llama.cpp/build/bin/llama-cli" + ext,
    };
    for (const QString& b : candidates)
        if (QFileInfo::exists(b)) return b;
    return candidates.first();
}

/**
 * llamaStudioDir() — Cartella llama_cpp_studio (con config.json, llama.cpp/, etc.).
 */
inline QString llamaStudioDir()
{
    const QStringList candidates = {
        root() + "/llama_cpp_studio",
    };
    for (const QString& d : candidates)
        if (QDir(d).exists()) return d;
    return candidates.first();
}

/**
 * llamaLibDir() — Directory delle librerie condivise (.so/.dll) di llama.cpp.
 *
 * Corrisponde alla directory del binario llama-server: le .so vengono
 * prodotte nella stessa cartella da cmake --build.
 * Usata per impostare LD_LIBRARY_PATH prima di avviare il server.
 */
inline QString llamaLibDir()
{
    return QFileInfo(llamaServerBin()).absolutePath();
}


/**
 * vramBenchBin() — Binario vram_bench (Prismalux/vram_bench).
 * vramProfilePath() — JSON prodotto dal benchmark (Prismalux/vram_profile.json).
 */
inline QString vramBenchBin()
{
    return root() + "/vram_bench" + exeExt();
}
inline QString vramProfilePath()
{
    return root() + "/vram_profile.json";
}

/**
 * openCodeBin() — Path al binario opencode.
 * Cerca nel PATH di sistema, poi nelle installazioni di default note.
 */
inline QString openCodeBin()
{
    const QString inPath = QStandardPaths::findExecutable("opencode");
    if (!inPath.isEmpty()) return inPath;
    const QString ext = exeExt();
    const QStringList candidates = {
        QDir::homePath() + "/.opencode/bin/opencode" + ext,
        QDir::homePath() + "/.local/bin/opencode"    + ext,
        "/usr/local/bin/opencode"                    + ext,
    };
    for (const QString& b : candidates)
        if (QFileInfo::exists(b)) return b;
    return "opencode" + ext;
}

/* ══════════════════════════════════════════════════════════════
   Utility UI — helper Qt riutilizzabili
   ══════════════════════════════════════════════════════════════ */

/**
 * safeTempPath() — Cartella temporanea di sistema, SEMPRE assoluta.
 *
 * BUG WINDOWS: quando l'app viene estratta/lanciata da dentro
 *   C:\Users\...\AppData\Local\Temp\Prismalux_Windows_full\
 *   QDir::tempPath() restituisce il percorso RELATIVO "Temp" invece
 *   dell'assoluto C:\Users\...\AppData\Local\Temp.
 *   QStandardPaths::TempLocation è immune a questo problema.
 *
 * Uso:  P::safeTempPath() + "/miofile.tmp"
 *       (sostituisce QDir::tempPath() ovunque si scrivono file temporanei)
 */
inline QString safeTempPath()
{
    QString tmp = QStandardPaths::writableLocation(QStandardPaths::TempLocation);
    if (tmp.isEmpty() || QDir(tmp).isRelative())
        tmp = QDir::homePath();   /* fallback estremo: home è sempre assoluta */
    return tmp;
}

/**
 * sanitizeErrorOutput(raw, maxLines) — Limita stack trace eccessivamente lunghi.
 *
 * MOTIVAZIONE:
 *   stderr di Python/llama-server può contenere centinaia di righe di traceback
 *   con path assoluti interni, indirizzi di memoria e flag di avvio del processo.
 *   Mostrare tutto all'utente è: 1) inutile 2) rivela la struttura interna del sistema.
 *
 *   Questa funzione mantiene le prime e ultime righe (le più informative) e
 *   rimuove il centro del traceback (solitamente call stack interno alle librerie).
 *
 * COSA NON FILTRA:
 *   - Errori dell'utente (SyntaxError, NameError, ValueError, ecc.)
 *   - Numero di riga del codice sorgente dell'utente
 *   - Messaggio finale dell'eccezione
 *
 * Non usare per log interni di debug — solo per output mostrato all'utente in UI.
 */
inline QString sanitizeErrorOutput(const QString& raw, int maxLines = 30)
{
    const QStringList lines = raw.split('\n');
    if (lines.size() <= maxLines) return raw;

    /* Mantieni le prime 8 righe (contesto iniziale) e le ultime 12 (errore finale) */
    constexpr int kHead = 8;
    constexpr int kTail = 12;
    const int skipped = lines.size() - kHead - kTail;

    QStringList result;
    result.reserve(kHead + 2 + kTail);
    result << lines.mid(0, kHead);
    result << QString("... [%1 righe omesse] ...").arg(skipped);
    result << lines.mid(lines.size() - kTail);
    return result.join('\n');
}

/**
 * isKnownBrokenModel(name) — true se il modello richiede una versione minima
 * di Ollama per funzionare correttamente.
 *
 * qwen3.5 (tutte le varianti): causa thinking loop infinito con Ollama < 0.21.1
 * — message.content resta vuoto perché i token del blocco <think> esauriscono
 * il budget prima della risposta. Risolto in Ollama 0.21.1.
 */
inline bool isKnownBrokenModel(const QString& name)
{
    Q_UNUSED(name)
    return false;   /* bug thinking loop risolto in Ollama 0.21.1 */
}

/**
 * knownBrokenModelTooltip() — testo del tooltip per i modelli con requisiti
 * di versione Ollama. Unico punto di verità: modificare qui per aggiornare
 * tutti i combo box dell'applicazione.
 */
inline QString knownBrokenModelTooltip()
{
    return "\xe2\x9a\xa0  Richiede Ollama \xe2\x89\xa5 0.21.1\n"
           "Versioni precedenti causano thinking loop infinito:\n"
           "il modello ragiona ma non produce risposta visibile.\n\n"
           "\xf0\x9f\x94\xa7  Workaround attivo: Prismalux aggiunge think:false\n"
           "automaticamente per forzare la risposta diretta.\n"
           "Soluzione definitiva: aggiorna Ollama con 'ollama update'.";
}

/**
 * modelIcon(sizeBytes, name) — prefisso emoji ☁️/🌍📍 per combo modelli.
 * cloud : sizeBytes == 0 (Ollama /api/tags) oppure nome termina con "cloud"
 * locale: sizeBytes  > 0 (file GGUF presente sul disco)
 */
inline QString modelIcon(qint64 sizeBytes, const QString& name)
{
    const bool isCloud = (sizeBytes == 0) || name.endsWith("cloud", Qt::CaseInsensitive);
    return isCloud
        ? QString::fromUtf8("\xe2\x98\x81\xef\xb8\x8f  ")          /* ☁️  */
        : QString::fromUtf8("\xf0\x9f\x8c\x8d\xf0\x9f\x93\x8d  "); /* 🌍📍  */
}

/**
 * totalRamBytes() — Memoria fisica totale del sistema in byte.
 *
 * Usata per verificare se un modello LLM è caricabile senza esaurire la RAM.
 * Regola euristica (ispirata al teorema di Nyquist-Shannon):
 *   il modello è caricabile comodamente se modelSizeBytes × 2 ≤ totalRamBytes.
 * Restituisce 0 se il dato non è determinabile (es. piattaforma non supportata).
 */
inline qint64 totalRamBytes()
{
#if defined(Q_OS_LINUX)
    QFile f("/proc/meminfo");
    if (f.open(QIODevice::ReadOnly)) {
        while (!f.atEnd()) {
            const QString line = QString::fromLatin1(f.readLine()).trimmed();
            if (line.startsWith("MemTotal:")) {
                /* formato: "MemTotal:    16327788 kB" */
                const QStringList parts = line.split(QChar(' '), Qt::SkipEmptyParts);
                if (parts.size() >= 2) return parts.at(1).toLongLong() * 1024LL;
            }
        }
    }
#elif defined(Q_OS_WIN)
#  ifndef WIN32_LEAN_AND_MEAN
#    define WIN32_LEAN_AND_MEAN
#  endif
#  ifndef NOMINMAX
#    define NOMINMAX
#  endif
#  include <windows.h>
    MEMORYSTATUSEX ms;
    ms.dwLength = sizeof(ms);
    if (GlobalMemoryStatusEx(&ms))
        return static_cast<qint64>(ms.ullTotalPhys);
#endif
    return 0;   /* non determinabile: nessun avviso */
}

/**
 * modelParamsPath() — JSON con override parametri per modello specifico.
 *
 * Formato: { "qwen3.5:4b": { "num_predict": 2048, "num_ctx": 4096 }, ... }
 * Scritto dal Bug Tracker quando l'utente applica un fix suggerito dall'AI.
 * Letto da AiClient::chat() che applica gli override sulle opzioni Ollama.
 */
inline QString modelParamsPath()
{
    return QDir::homePath() + "/.prismalux/model_params.json";
}

/** cronFile() — File JSON dei job pianificati Cron */
inline QString cronFile()
{
    return QDir::homePath() + "/.prismalux/cron_jobs.json";
}

/** feedbackPath() — JSONL feedback 👍/👎 per risposta (base per DPO futuro). */
inline QString feedbackPath()
{
    return QDir::homePath() + "/.prismalux/feedback.jsonl";
}

/** lanTokenPath() — Token Bearer LAN salvato con permessi 0600 (non in QSettings). */
inline QString lanTokenPath()
{
    return QDir::homePath() + "/.prismalux/lan_token.key";
}

/** userKnowledgePath() — Percorso del file memoria persistente utente. */
inline QString userKnowledgePath()
{
    return root() + "/KNOWLEDGE_USER/user_knowledge.md";
}

/* Stato cache per readUserKnowledge() — esposto per invalidateKnowledgeCache(). */
namespace detail {
inline qint64& knowledgeCacheMs() { static qint64 v = 0;  return v; }
inline QString& knowledgeCache()  { static QString v;      return v; }
}

/**
 * readUserKnowledge() — Legge user_knowledge.md con cache 30s.
 * La cache evita I/O ripetuto durante lo streaming token.
 * Restituisce stringa vuota se il file non esiste.
 */
inline QString readUserKnowledge()
{
    constexpr qint64 kTtlMs = 30'000;
    const qint64 now = QDateTime::currentMSecsSinceEpoch();

    if (!detail::knowledgeCache().isNull() &&
        (now - detail::knowledgeCacheMs()) < kTtlMs)
        return detail::knowledgeCache();

    const QString path = userKnowledgePath();
    if (!QFile::exists(path)) {
        detail::knowledgeCache()   = QString("");
        detail::knowledgeCacheMs() = now;
        return detail::knowledgeCache();
    }
    QFile f(path);
    if (!f.open(QIODevice::ReadOnly | QIODevice::Text))
        return {};
    detail::knowledgeCache()   = QString::fromUtf8(f.readAll());
    detail::knowledgeCacheMs() = now;
    return detail::knowledgeCache();
}

/** invalidateKnowledgeCache() — Forza rilettura del file alla prossima chiamata. */
inline void invalidateKnowledgeCache()
{
    detail::knowledgeCacheMs() = 0;
    detail::knowledgeCache().clear();
}

/**
 * prependKnowledge(systemPrompt) — Prepende user_knowledge.md al system prompt.
 * Se kInjectUserKnowledge è false o il file è vuoto, restituisce il prompt invariato.
 */
inline QString prependKnowledge(const QString& systemPrompt)
{
    QSettings s("Prismalux", "GUI");
    if (!s.value("ai/injectUserKnowledge", true).toBool())
        return systemPrompt;
    const QString knowledge = readUserKnowledge();
    if (knowledge.trimmed().isEmpty())
        return systemPrompt;
    return "# Contesto utente (memoria persistente)\n"
           + knowledge
           + "\n\n---\n\n"
           + systemPrompt;
}

/**
 * repolish(widget) — Forza il ricalcolo dello stile Qt su un widget.
 *
 * Necessario dopo aver modificato una proprietà Qt (setProperty) che
 * influenza il foglio di stile QSS: Qt non rileva automaticamente il
 * cambiamento dei custom property, quindi bisogna chiamare
 * unpolish + polish per applicare il nuovo stile.
 *
 * Uso: P::repolish(m_btnBackend);  // dopo setProperty("backendActive",...)
 */
inline void repolish(QWidget* w)
{
    if (!w) return;
    w->style()->unpolish(w);
    w->style()->polish(w);
}

/* ══════════════════════════════════════════════════════════════
   Percorsi whisper.cpp — tutti DENTRO il progetto (Prismalux/)
   ══════════════════════════════════════════════════════════════ */

/**
 * whisperDir() — Cartella sorgente whisper.cpp dentro il progetto.
 *   root() = Prismalux/  →  Prismalux/whisper.cpp/
 */
inline QString whisperDir()
{
    return root() + "/whisper.cpp";
}

/**
 * whisperBin() — Binario whisper-cli.
 * Cerca in ordine:
 *   1. Posizione progetto (root/whisper.cpp/build/...)
 *   2. Vecchia posizione (~/.prismalux/whisper.cpp/build/...)
 *   3. PATH di sistema (sudo apt install whisper-cpp)
 * Restituisce stringa vuota se non trovato.
 */
inline QString whisperBin()
{
    const QString ext  = exeExt();
    const QString home = QDir::homePath();
    const QStringList candidates = {
        /* posizione corrente del progetto */
        whisperDir() + "/build/bin/whisper-cli" + ext,
        whisperDir() + "/build/whisper-cli"     + ext,
        whisperDir() + "/build/main"            + ext,
        /* vecchia posizione usata nelle versioni precedenti */
        home + "/.prismalux/whisper.cpp/build/bin/whisper-cli" + ext,
        home + "/.prismalux/whisper.cpp/build/bin/main"        + ext,
        home + "/.prismalux/whisper.cpp/build/whisper-cli"     + ext,
    };
    for (const QString& p : candidates)
        if (QFileInfo::exists(p)) return p;
    /* fallback: binario nel PATH di sistema (es. apt install whisper-cpp) */
    return QStandardPaths::findExecutable("whisper-cli");
}

/**
 * whisperModelsDir() — Cartella modelli GGML dentro il progetto.
 *   Prismalux/whisper.cpp/models/
 */
inline QString whisperModelsDir()
{
    return root() + "/whisper.cpp/models";
}

/**
 * findPython() — Restituisce il percorso ASSOLUTO dell'eseguibile Python.
 *
 * PROBLEMA WINDOWS (double-quoting bug):
 *   Se findPython() restituisce un nome bare come "python3", QProcess può
 *   avviare uno shim .bat tramite cmd.exe. cmd.exe aggiunge doppio quoting
 *   agli argomenti, corrompendo il path:
 *     atteso:  C:\Temp\prisma_code.py
 *     ricevuto: C:\...\C_software\"C:\Temp\prisma_code.py"
 *   Soluzione: su Windows usare SEMPRE il path completo al .exe reale.
 *
 * Ordine di ricerca:
 *   1. py_env/Scripts/python.exe  — venv Windows creato da Avvia_Prismalux.bat
 *   2. py_env/bin/python3         — venv Linux/macOS
 *   3. Windows: where.exe → primo match .exe (esclude .bat/.cmd shim)
 *   4. Linux/macOS: probe bare name python3 / python
 */
inline QString findPython()
{
    const QString appDir = QCoreApplication::applicationDirPath();

    /* 0. PRISMALUX_PYTHON: impostato da Avvia_Prismalux.bat / Avvia_Prismalux.sh
       Punta direttamente al python.exe del venv con tutte le dipendenze. */
    const QString envPy = qEnvironmentVariable("PRISMALUX_PYTHON");
    if (!envPy.isEmpty() && QFile::exists(envPy)) return envPy;

    /* 1-2. Venv accanto all'eseguibile (priorità massima) */
    const QStringList venvCandidates = {
        appDir + "/py_env/Scripts/python.exe",  /* Windows venv */
        appDir + "/py_env/bin/python3",          /* Linux/macOS venv */
        appDir + "/py_env/bin/python",
    };
    for (const QString& p : venvCandidates)
        if (QFile::exists(p)) return p;

#ifdef Q_OS_WIN
    /* 3. Windows: trova il path COMPLETO via where.exe, solo .exe, niente .bat
       where.exe lista tutti i match in ordine PATH; prendiamo il primo .exe.
       Verifica anche che non sia il Microsoft Store stub (esce senza "Python"). */
    auto findExeViaWhere = [](const QString& name) -> QString {
        QProcess w;
        w.start("where", {name});
        if (!w.waitForStarted(1500) || !w.waitForFinished(2000)) return {};
        const QStringList lines =
            QString::fromLocal8Bit(w.readAllStandardOutput())
            .split('\n', Qt::SkipEmptyParts);
        for (const QString& line : lines) {
            const QString path = line.trimmed();
            if (path.toLower().endsWith(".exe") && QFile::exists(path))
                return path;
        }
        return {};
    };

    for (const QString& name : {"python", "py", "python3"}) {
        const QString exe = findExeViaWhere(name);
        if (exe.isEmpty()) continue;
        /* Verifica che sia Python reale (non Store stub che apre il Market) */
        QProcess verify;
        verify.start(exe, {"--version"});
        if (verify.waitForStarted(1500) && verify.waitForFinished(2000)) {
            const QString ver = QString::fromLocal8Bit(
                verify.readAllStandardOutput() +
                verify.readAllStandardError()).trimmed();
            if (ver.contains("Python", Qt::CaseInsensitive)) return exe;
        }
    }
    return "python.exe";   /* fallback: almeno .exe, non .bat */
#else
    /* 3. Linux/macOS: bare name, nessun problema di .bat routing */
    const QStringList sysCandidates = { "python3", "python" };
    for (const QString& c : sysCandidates) {
        QProcess probe;
        probe.start(c, QStringList{"--version"});
        if (probe.waitForStarted(1500) && probe.waitForFinished(2000))
            return c;
    }
    return "python3";
#endif
}

/**
 * findDocker() — Restituisce il percorso del binario docker, o QString() se assente.
 *
 * Tenta: /usr/bin/docker · /usr/local/bin/docker · docker (PATH).
 * Verifica con "docker info --format '{{.ServerVersion}}'" che il daemon sia
 * raggiungibile (non solo che il binario esista).
 * Il risultato è memorizzato staticamente: la prima chiamata è bloccante
 * (~100–300 ms), le successive sono istantanee.
 */
inline QString findDocker()
{
    static QString cached;
    static bool    checked = false;
    if (checked) return cached;
    checked = true;

    const QStringList candidates = {
        "/usr/bin/docker",
        "/usr/local/bin/docker",
        "docker",
    };
    QString bin;
    for (const QString& c : candidates) {
        if (c == "docker") {
            /* bare name: verifica via PATH */
            bin = QStandardPaths::findExecutable("docker");
            if (!bin.isEmpty()) break;
        } else if (QFile::exists(c)) {
            bin = c;
            break;
        }
    }
    if (bin.isEmpty()) return cached; /* docker non installato */

    /* Daemon check: "docker info" restituisce 0 solo se il daemon risponde */
    QProcess probe;
    probe.setProcessChannelMode(QProcess::MergedChannels);
    probe.start(bin, {"info", "--format", "{{.ServerVersion}}"});
    if (!probe.waitForStarted(1000) || !probe.waitForFinished(3000))
        return cached; /* daemon non raggiungibile */

    if (probe.exitCode() != 0) return cached;

    cached = bin;
    return cached;
}

/**
 * isSandboxReady() — true se Docker disponibile E sandbox abilitato nelle impostazioni.
 */
inline bool isSandboxReady()
{
    if (findDocker().isEmpty()) return false;
    return QSettings("Prismalux", "GUI").value("sandbox/enabled", true).toBool();
}

/* ══════════════════════════════════════════════════════════════
   SK — Settings Keys (unico punto di verità per le chiavi QSettings)

   MOTIVAZIONE: 43 istanze di QSettings("Prismalux","GUI") sparse in
   8 file con chiavi letterali. Una singola typo (es. "rag/noSave" vs
   "rag/no_save") silenziosamente produce una chiave mai letta.
   Centralizzare qui garantisce che ogni chiave abbia un solo nome
   nel codebase, trovabile con grep/IDE e rinominabile in un posto.

   USO:
     #include "prismalux_paths.h"
     QSettings ss("Prismalux", "GUI");
     ss.setValue(P::SK::kTheme, "dark_cyan");
     QString t = ss.value(P::SK::kTheme, "dark_cyan").toString();
   ══════════════════════════════════════════════════════════════ */
namespace SK {

/* ── Aspetto ─────────────────────────────────────── */
constexpr const char* kTheme           = "theme";
constexpr const char* kBubbleRadius    = "bubble_radius";
constexpr const char* kTabsTop         = "tabs_top";
constexpr const char* kIconsText       = "icons_text";

/* ── Navigazione ─────────────────────────────────── */
constexpr const char* kNavStyle        = "nav/navStyle";
constexpr const char* kNavTabMode      = "nav/tabMode";
constexpr const char* kNavExecBtnMode  = "nav/execBtnMode";

/* ── RAG ─────────────────────────────────────────── */
constexpr const char* kRagMaxResults   = "rag/maxResults";
constexpr const char* kRagNoSave       = "rag/noSave";
constexpr const char* kRagDocCount     = "rag/docCount";
constexpr const char* kRagDocsDir      = "rag/docsDir";
constexpr const char* kRagJlTransform  = "rag/jlTransform";
constexpr const char* kRagLastIndexed  = "rag/lastIndexed";
constexpr const char* kRagEmbedModel   = "rag/embedModel";   ///< modello Ollama per embedding (default: nomic-embed-text)

/* ── STT / TTS ───────────────────────────────────── */
constexpr const char* kSttModelPath    = "stt/model_path";

/* ── AI — modello/backend preferiti dall'utente ──── */
constexpr const char* kActiveModel     = "ai/activeModel";    ///< ultimo modello selezionato manualmente
constexpr const char* kActiveBackend   = "ai/activeBackend";  ///< 0=Ollama, 1=LlamaServer, 2=LlamaLocal
constexpr const char* kActiveHost      = "ai/activeHost";     ///< host AI (default: 127.0.0.1)
constexpr const char* kActivePort      = "ai/activePort";     ///< porta AI (default: dipende dal backend)

/* ── Varie ───────────────────────────────────────── */
constexpr const char* kLoopFixWarning  = "loop_fix_warning_shown";
constexpr const char* kDefaultTheme    = "dark_ocean";   ///< valore default tema
constexpr const char* kCavemanMode          = "ai/cavemanMode";          ///< modalità risposte dirette (Caveman)
constexpr const char* kComputeMode          = "ai/computeMode";          ///< "auto"|"gpu"|"cpu"|"misto"
constexpr const char* kInjectUserKnowledge  = "ai/injectUserKnowledge";  ///< inietta user_knowledge.md nel system prompt (default: true)
constexpr const char* kMlockModel           = "ai/mlockModel";           ///< --mlock llama-server: blocca pagine modello in RAM (default: false)
constexpr const char* kAutoZramDoppia       = "ai/autoZramDoppia";       ///< avvia zRAM Doppia (zstd, 75% RAM) all'avvio — Linux (default: true)
constexpr const char* kAutoOptApplied       = "ai/autoOptApplied";       ///< preset RAM automatico già applicato al primo avvio (default: false)
constexpr const char* kAiPersonality        = "ai/personality";           ///< personalità AI: "nessuna"|"jarvis"|"kitt"|"yoda"|"snake"|"sonic"|"mario"

/* ── Onboarding ─────────────────────────────────── */
constexpr const char* kSetupDone      = "setup/done";   ///< wizard primo avvio già mostrato

/* ── Sandbox Docker ──────────────────────────────── */
constexpr const char* kSandboxEnabled = "sandbox/enabled";        ///< esegui codice AI in container Docker (default: true se Docker disponibile)
constexpr const char* kSandboxImage   = "sandbox/image";          ///< immagine Docker da usare (default: "python:3.11-slim")
constexpr const char* kSandboxMemory  = "sandbox/memoryMb";       ///< limite RAM container MB (default: 256)

/* ── LAN Server ──────────────────────────────────── */
constexpr const char* kLanToken       = "lan/accessToken";         ///< token Bearer opzionale per proteggere le API LAN

}  // namespace SK

/* ── Personalità AI — nome visualizzato e suffisso system prompt ─── */
inline QString personalityName() {
    QSettings ss("Prismalux", "GUI");
    const QString p = ss.value(SK::kAiPersonality, "nessuna").toString();
    if (p == "jarvis") return "Jarvis";
    if (p == "kitt")   return "KITT";
    if (p == "yoda")   return "Yoda";
    if (p == "snake")  return "Snake";
    if (p == "sonic")  return "Sonic";
    if (p == "mario")  return "Mario";
    return {};
}

inline QString personalityPrompt() {
    QSettings ss("Prismalux", "GUI");
    const QString p = ss.value(SK::kAiPersonality, "nessuna").toString();
    if (p == "jarvis") return "Rispondi come JARVIS, l'AI di Tony Stark: professionale, preciso, con sottile ironia britannica. Chiama l'utente \"signore\".";
    if (p == "kitt")   return "Rispondi come KITT, il sistema di bordo di Knight Rider: sofisticato, calmo, formale, con occasionali riferimenti alla guida e alla sicurezza stradale.";
    if (p == "yoda")   return "Rispondi come Yoda di Star Wars. Sintassi invertita (complemento-soggetto-verbo). Breve, saggio, sereno.";
    if (p == "snake")  return "Rispondi come Solid Snake di Metal Gear Solid: diretto, tattico, cinismo controllato, frasi brevi e incisive.";
    if (p == "sonic")  return "Rispondi come Sonic the Hedgehog: rapido, energico, spiritoso, leggermente impaziente con le cose lente.";
    if (p == "mario")  return "Rispondi come Super Mario: entusiasta, positivo, usa esclamazioni come \"Wahoo!\" e \"Mamma mia!\", sempre incoraggiante.";
    return {};
}

} // namespace PrismaluxPaths
