#pragma once
/* ══════════════════════════════════════════════════════════════
   PathGuard — validazione path per tool AI (leggi_file, lista_file,
   search_rag, scrivi_file, Dev Agent, ecc.)

   Problema ricorrente: ogni volta che un tool AI accetta un percorso
   di file dall'utente (o dal modello LLM) rischia di accedere a file
   sensibili via prompt injection o input malevolo.

   Uso:
       #include "widgets/path_guard.h"

       QString safe = PathGuard::checkRead(rawInput);
       if (safe.isEmpty()) { onDone("accesso negato: path non consentito"); return; }
       QFile f(safe);   // usa il path canonico validato

   Funzioni pubbliche:
       checkRead(raw)   → path canonico se consentito per lettura, "" se negato
       checkWrite(raw)  → path canonico se consentito per scrittura (più restrittivo)
       checkDir(raw)    → path canonico se consentito per listare directory
       isDenied(canon)  → true se il path canonico è nella blacklist
       expandHome(raw)  → espande "~/" con QDir::homePath()
   ══════════════════════════════════════════════════════════════ */

#include <QString>
#include <QStringList>
#include <QDir>
#include <QFileInfo>
#include <QStandardPaths>

namespace PrismaluxPaths { QString root(); }   /* prismalux_paths.h — forward */

class PathGuard {
public:
    /* ── Lettura ─────────────────────────────────────────────────
       Consentito: home utente, root progetto, RAG docs, /tmp.
       Bloccato: directory di sistema, credenziali, certificati.
       Ritorna il path CANONICO (symlink risolti) o "" se negato.   */
    static QString checkRead(const QString& raw)
    {
        const QString canon = _resolve(raw);
        if (canon.isEmpty())         return {};
        if (!_inReadAllowed(canon))  return {};
        if (_isDenied(canon))        return {};
        if (_hasSensitiveExt(canon)) return {};
        return canon;
    }

    /* ── Scrittura ───────────────────────────────────────────────
       Solo root progetto e RAG docs — mai altrove.                  */
    static QString checkWrite(const QString& raw)
    {
        const QString canon = _resolve(raw);
        if (canon.isEmpty())         return {};
        if (!_inWriteAllowed(canon)) return {};
        if (_isDenied(canon))        return {};
        return canon;
    }

    /* ── Directory listing ───────────────────────────────────────
       Stesse regole di lettura.                                      */
    static QString checkDir(const QString& raw)
    {
        const QString canon = _resolveDir(raw);
        if (canon.isEmpty())        return {};
        if (!_inReadAllowed(canon)) return {};
        if (_isDenied(canon))       return {};
        return canon;
    }

    /* ── Espande "~/" con il path home dell'utente ───────────────  */
    static QString expandHome(const QString& raw)
    {
        QString s = raw.trimmed();
        if (s.startsWith("~/"))
            s = QDir::homePath() + s.mid(1);
        else if (s == "~")
            s = QDir::homePath();
        return s;
    }

    /* ── Messaggio di errore standard (per onDone / log) ─────────  */
    static QString deniedMsg(const QString& raw)
    {
        return QString(
            "accesso negato: il path '%1' non rientra nelle directory "
            "consentite per i tool AI (home, progetto, RAG docs)."
        ).arg(raw.trimmed().left(80));
    }

private:
    /* Risolve symlink e normalizza — file */
    static QString _resolve(const QString& raw)
    {
        const QString expanded = expandHome(raw.trimmed());
        if (expanded.isEmpty()) return {};
        const QFileInfo fi(expanded);
        /* Per file non ancora esistenti usa il parent canonico */
        if (fi.exists())
            return fi.canonicalFilePath();
        const QString parent = fi.absoluteDir().canonicalPath();
        if (parent.isEmpty()) return {};
        return parent + "/" + fi.fileName();
    }

    /* Risolve symlink — directory */
    static QString _resolveDir(const QString& raw)
    {
        const QString expanded = expandHome(raw.trimmed());
        if (expanded.isEmpty()) return {};
        const QFileInfo fi(expanded);
        if (fi.exists() && fi.isDir())
            return fi.canonicalFilePath();
        return fi.absoluteDir().canonicalPath();
    }

    /* Directory CONSENTITE per lettura (prefisso) */
    static bool _inReadAllowed(const QString& canon)
    {
        static const QStringList kAllow = _buildReadAllow();
        for (const QString& d : kAllow)
            if (canon.startsWith(d))
                return true;
        return false;
    }

    static QStringList _buildReadAllow()
    {
        return {
            QDir::homePath() + "/",
            QDir::homePath(),               /* home stesso */
            QDir::tempPath() + "/",
            QDir::tempPath(),
            /* /proc/cpuinfo, /proc/meminfo — utili e non sensibili */
            "/proc/cpuinfo",
            "/proc/meminfo",
            "/proc/version",
        };
    }

    /* Directory CONSENTITE per scrittura (sottoinsieme più stretto) */
    static bool _inWriteAllowed(const QString& canon)
    {
        const QString proj = PrismaluxPaths::root();
        const QString rag1 = QDir::homePath() + "/prismalux_rag_docs/";
        const QString rag2 = proj + "/RAG/";
        const QString tmp  = QDir::tempPath() + "/";
        return canon.startsWith(proj + "/")
            || canon.startsWith(rag1)
            || canon.startsWith(rag2)
            || canon.startsWith(tmp);
    }

    /* Blacklist assoluta — mai accessibili anche se dentro home */
    static bool _isDenied(const QString& canon)
    {
        static const QStringList kDeny = _buildDeny();
        for (const QString& d : kDeny)
            if (canon.startsWith(d))
                return true;
        return false;
    }

    static QStringList _buildDeny()
    {
        const QString home = QDir::homePath();
        return {
            /* Credenziali SSH e PGP */
            home + "/.ssh/",
            home + "/.gnupg/",
            home + "/.gpg/",
            /* Token e configurazioni browser */
            home + "/.config/chromium/",
            home + "/.config/google-chrome/",
            home + "/.config/BraveSoftware/",
            home + "/.mozilla/",
            home + "/.thunderbird/",
            /* Keychain e wallet */
            home + "/.local/share/keyrings/",
            home + "/.aws/",
            home + "/.azure/",
            home + "/.kube/",
            home + "/.docker/",
            /* Directory di sistema */
            "/etc/",
            "/proc/",
            "/sys/",
            "/dev/",
            "/boot/",
            "/root/",
            "/var/",
            "/run/",
        };
    }

    /* Blocca estensioni con materiale crittografico o credenziali */
    static bool _hasSensitiveExt(const QString& canon)
    {
        static const QStringList kBadExt = {
            "pem", "key", "p12", "pfx", "crt", "cer", "der",
            "p8", "ppk", "asc", "gpg",
            "env",        /* .env con segreti */
            "keystore",   /* Android keystore */
            "jks",        /* Java KeyStore */
        };
        const QString ext = QFileInfo(canon).suffix().toLower();
        return kBadExt.contains(ext);
    }
};
