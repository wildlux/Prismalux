#pragma once
#include <QString>
#include <QStringList>
#include <QFileInfo>
#include <QProcess>
#include "../prismalux_paths.h"
#include "ffmpeg_utils.h"
namespace P = PrismaluxPaths;

/**
 * OpencvUtils — discovery Python+cv2 e hook per OpenCV C++ nativo.
 *
 * Ordine di ricerca in findCvPython():
 *   1. Frameworks/opencv/venv/  (venv dedicato con cv2+pytesseract isolati)
 *   2. P::findPython()           (Python di sistema / venv generico Prismalux)
 *
 * Per creare il venv dedicato usa setupVenvScript() o il file
 * Frameworks/opencv/setup_venv.sh generato automaticamente.
 *
 * Hook C++ nativo (futuro):
 *   hasNativeLibs() — true se Frameworks/opencv/build/lib/libopencv_core.so esiste
 *   nativeLibDir()  — path ai .so compilati localmente
 */
namespace OpencvUtils {

/* ── Venv dedicato OpenCV ─────────────────────────────────────── */

inline QString venvDir()
{
    return P::opencvDir() + "/venv";
}

inline QString findCvPython()
{
#ifdef Q_OS_WIN
    const QStringList venvCandidates = {
        venvDir() + "/Scripts/python.exe",
        venvDir() + "/Scripts/python3.exe",
    };
#else
    const QStringList venvCandidates = {
        venvDir() + "/bin/python3",
        venvDir() + "/bin/python",
    };
#endif
    for (const QString& p : venvCandidates)
        if (QFileInfo::exists(p)) return p;
    return P::findPython();
}

/* Verifica sincrona (max 3s): cv2 è importabile nel Python trovato? */
inline bool isCv2Available()
{
    const QString py = findCvPython();
    if (py.isEmpty()) return false;
    QProcess p;
    p.start(py, {"-c", "import cv2"});
    return p.waitForFinished(3000) && p.exitCode() == 0;
}

/* Verifica sincrona: pytesseract è importabile? */
inline bool isPytesseractAvailable()
{
    const QString py = findCvPython();
    if (py.isEmpty()) return false;
    QProcess p;
    p.start(py, {"-c", "import pytesseract"});
    return p.waitForFinished(3000) && p.exitCode() == 0;
}

/* True se entrambe le dipendenze OCR sono pronte */
inline bool isOcrReady()
{
    return isCv2Available() && isPytesseractAvailable();
}

/* ── Hook C++ nativo (futuro) ─────────────────────────────────── */

inline QString nativeLibDir()
{
    return P::opencvDir() + "/build/lib";
}

inline bool hasNativeLibs()
{
#ifdef Q_OS_WIN
    return QFileInfo::exists(P::opencvDir() + "/build/bin/opencv_world490.dll")
        || QFileInfo::exists(P::opencvDir() + "/build/lib/libopencv_core.dll.a");
#else
    return QFileInfo::exists(nativeLibDir() + "/libopencv_core.so")
        || QFileInfo::exists(nativeLibDir() + "/libopencv_core.so.4.10")
        || QFileInfo::exists(nativeLibDir() + "/libopencv_core.so.4.9");
#endif
}

/* ── Script di setup venv ─────────────────────────────────────── */

/* Comandi shell per creare il venv e installare cv2+pytesseract.
   Restituisce una stringa multi-riga eseguibile come script bash/sh. */
inline QString setupVenvScript()
{
    const QString venv = venvDir();
    return QString(
        "#!/bin/bash\n"
        "set -e\n"
        "VENV=\"%1\"\n"
        "echo \"[OpenCV] Creo venv in: $VENV\"\n"
        "python3 -m venv \"$VENV\"\n"
        "\"$VENV/bin/pip\" install --upgrade pip\n"
        "\"$VENV/bin/pip\" install opencv-python-headless pytesseract\n"
        "echo \"[OpenCV] Venv pronto. cv2 + pytesseract installati.\"\n"
    ).arg(venv);
}

#ifdef Q_OS_WIN
inline QString setupVenvScriptWin()
{
    const QString venv = venvDir();
    return QString(
        "@echo off\n"
        "set VENV=%1\n"
        "echo [OpenCV] Creo venv in: !VENV!\n"
        "python -m venv \"!VENV!\"\n"
        "\"!VENV!\\Scripts\\pip\" install --upgrade pip\n"
        "\"!VENV!\\Scripts\\pip\" install opencv-python-headless pytesseract\n"
        "echo [OpenCV] Venv pronto.\n"
    ).arg(venv);
}
#endif

} // namespace OpencvUtils
