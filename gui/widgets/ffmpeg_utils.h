#pragma once
#include <QString>
#include <QStringList>
#include <QStandardPaths>
#include <QFileInfo>
#include "../prismalux_paths.h"
namespace P = PrismaluxPaths;

/**
 * FfmpegUtils — discovery e argomenti ottimali per ffmpeg.
 *
 * Ordine di ricerca in findFfmpeg():
 *   1. Frameworks/ffmpeg/bin/ffmpeg[.exe]  (build locale del progetto)
 *   2. PATH di sistema
 *
 * whisperArgs() produce WAV 16kHz mono con:
 *   - highpass=f=100  rimuove il rumore a bassa frequenza (ventole, HVAC)
 *   - loudnorm         normalizza il volume (EBU R128 single-pass)
 * Questo migliora sensibilmente l'accuratezza di Whisper su audio
 * registrato troppo piano o con rumore di fondo.
 */
namespace FfmpegUtils {

inline QString findFfmpeg()
{
#ifdef Q_OS_WIN
    const QString local = P::ffmpegDir() + "/bin_windows/ffmpeg.exe";
#else
    const QString local = P::ffmpegDir() + "/bin/ffmpeg";
#endif
    if (QFileInfo::exists(local))
        return local;
    return QStandardPaths::findExecutable("ffmpeg");
}

inline bool isAvailable()
{
    return !findFfmpeg().isEmpty();
}

/* Converte src → dst WAV 16kHz mono ottimizzato per Whisper STT. */
inline QStringList whisperArgs(const QString& src, const QString& dst)
{
    return {
        "-y", "-i", src,
        "-ar", "16000", "-ac", "1",
        "-c:a", "pcm_s16le",
        "-af", "highpass=f=100,loudnorm",
        dst
    };
}

/* Estrae/converte audio da src (video o audio) → dst WAV, senza filtri STT.
   Usare per video quando si vuole preservare l'audio originale invariato. */
inline QStringList extractArgs(const QString& src, const QString& dst)
{
    return {
        "-y", "-i", src,
        "-ar", "16000", "-ac", "1",
        "-c:a", "pcm_s16le",
        dst
    };
}

} // namespace FfmpegUtils
