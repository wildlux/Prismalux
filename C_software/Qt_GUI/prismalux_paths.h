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
#include <QWidget>
#include <QStyle>

namespace PrismaluxPaths {

/* ══════════════════════════════════════════════════════════════
   Costanti di configurazione
   ══════════════════════════════════════════════════════════════ */

/** Porta default Ollama (documentata: https://ollama.ai) */
constexpr int kOllamaPort      = 11434;

/** Porta default llama-server quando avviato dalla GUI */
constexpr int kLlamaServerPort = 8081;

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
 *   (build/Prismalux_GUI → Qt_GUI/build → Qt_GUI → Prismalux)
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
 *   1. C_software/models   ← cartella canonica del progetto C
 *   2. llama_cpp_studio/models
 *   3. models/             ← fallback generico
 *
 * Se nessuna esiste restituisce la prima (C_software/models) che potrà
 * essere creata con QDir::mkpath() dal chiamante.
 */
inline QString modelsDir()
{
    const QStringList candidates = {
        root() + "/C_software/models",
        root() + "/llama_cpp_studio/models",
        root() + "/models",
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
 * Priorità: posizione standard dopo la compilazione con build.sh,
 * poi varianti alternative note, poi fallback.
 */
inline QString llamaServerBin()
{
    const QString ext = exeExt();
    const QStringList candidates = {
        root() + "/llama_cpp_studio/llama.cpp/build/bin/llama-server" + ext,
        root() + "/C_software/llama_cpp_studio/llama.cpp/build/bin/llama-server" + ext,
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
        root() + "/C_software/llama_cpp_studio/llama.cpp/build/bin/llama-cli" + ext,
        root() + "/llama.cpp/build/bin/llama-cli" + ext,
    };
    for (const QString& b : candidates)
        if (QFileInfo::exists(b)) return b;
    return candidates.first();
}

/**
 * llamaStudioDir() — Cartella llama_cpp_studio (con CMakeLists, etc.).
 *
 * Cerca prima nella posizione canonica (root del progetto), poi nel
 * vecchio percorso dentro Python_Software per compatibilità.
 */
inline QString llamaStudioDir()
{
    const QStringList candidates = {
        root() + "/llama_cpp_studio",
        root() + "/Python_Software/Ottimizzazioni_Avanzate/llama_cpp_studio",
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
 * vramBenchBin() — Binario vram_bench (C_software/vram_bench).
 * vramProfilePath() — JSON prodotto dal benchmark (C_software/vram_profile.json).
 */
inline QString vramBenchBin()
{
    return root() + "/C_software/vram_bench" + exeExt();
}
inline QString vramProfilePath()
{
    return root() + "/C_software/vram_profile.json";
}

/* ══════════════════════════════════════════════════════════════
   Utility UI — helper Qt riutilizzabili
   ══════════════════════════════════════════════════════════════ */

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
   Percorsi whisper.cpp — tutti DENTRO il progetto (C_software/)
   ══════════════════════════════════════════════════════════════ */

/**
 * whisperDir() — Cartella sorgente whisper.cpp dentro il progetto.
 *   C_software/whisper.cpp/   (clonato automaticamente al primo avvio)
 */
inline QString whisperDir()
{
    return root() + "/C_software/whisper.cpp";
}

/**
 * whisperBin() — Binario whisper-cli dentro il progetto.
 * Restituisce stringa vuota se non ancora compilato.
 */
inline QString whisperBin()
{
    const QString ext = exeExt();
    const QStringList candidates = {
        whisperDir() + "/build/bin/whisper-cli" + ext,
        whisperDir() + "/build/whisper-cli"     + ext,
        whisperDir() + "/build/main"            + ext, /* vecchio nome */
    };
    for (const QString& p : candidates)
        if (QFileInfo::exists(p)) return p;
    return {};
}

/**
 * whisperModelsDir() — Cartella modelli GGML dentro il progetto.
 *   C_software/whisper/models/
 */
inline QString whisperModelsDir()
{
    return root() + "/C_software/whisper/models";
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

} // namespace PrismaluxPaths
