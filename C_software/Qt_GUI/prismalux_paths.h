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
#include <QFileInfo>
#include <QFileInfoList>
#include <QCoreApplication>
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

} // namespace PrismaluxPaths
