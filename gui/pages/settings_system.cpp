#include "settings_main.h"
#include "../widgets/toggle_switch.h"
#include "../widgets/stt_whisper.h"
#include "main_customize.h"
#include "main_maintenance.h"
#include "main_graph.h"
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

QWidget* ImpostazioniPage::buildPuliziaTab()
{
    auto* w   = new QWidget(this);
    auto* lay = new QVBoxLayout(w);
    lay->setContentsMargins(16, 16, 16, 16);
    lay->setSpacing(10);

    auto* ttl = new QLabel("\xf0\x9f\xa7\xb9  Pulizia spazio su disco", w);
    ttl->setObjectName("cardTitle");
    lay->addWidget(ttl);

    auto* intro = new QLabel(
        "Rimuovi file temporanei, output vecchi e artefatti di build "
        "che non sono pi\xc3\xb9 necessari. "
        "Ogni sezione mostra la dimensione stimata prima di procedere.", w);
    intro->setObjectName("cardDesc");
    intro->setWordWrap(true);
    lay->addWidget(intro);

    /* ── Helper: dimensione totale file in una cartella (ricorsiva) ── */
    auto calcFilesSize = [](const QString& dirPath,
                            const QStringList& nameFilters = {}) -> QPair<qint64, int> {
        qint64 bytes = 0;
        int    count = 0;
        QDirIterator it(dirPath,
                        nameFilters.isEmpty() ? QStringList{"*"} : nameFilters,
                        QDir::Files | QDir::Hidden,
                        QDirIterator::Subdirectories);
        while (it.hasNext()) {
            it.next();
            bytes += it.fileInfo().size();
            ++count;
        }
        return {bytes, count};
    };

    auto fmtBytes = [](qint64 b) -> QString {
        if (b <= 0)    return "0 B";
        if (b < 1024)  return QString("%1 B").arg(b);
        if (b < 1<<20) return QString("%1 KB").arg(b>>10);
        if (b < 1<<30) return QString("%1 MB").arg(b>>20);
        return QString("%1 GB").arg(b>>30);
    };

    /* ── Helper: crea una card di pulizia ── */
    auto makeCard = [&](const QString& icon,
                        const QString& heading,
                        const QString& desc,
                        const QString& pathHint,
                        const QString& sizeText) -> QGroupBox* {
        auto* grp = new QGroupBox(w);
        grp->setObjectName("cardGroup");
        auto* gl  = new QVBoxLayout(grp);
        gl->setContentsMargins(12, 12, 12, 8);
        gl->setSpacing(4);

        auto* hdr = new QLabel(icon + "  " + heading, grp);
        hdr->setObjectName("cardTitle");
        gl->addWidget(hdr);

        auto* dsc = new QLabel(desc, grp);
        dsc->setObjectName("cardDesc");
        dsc->setWordWrap(true);
        gl->addWidget(dsc);

        auto* pth = new QLabel(pathHint, grp);
        pth->setObjectName("hintLabel");
        pth->setWordWrap(true);
        gl->addWidget(pth);

        auto* row = new QWidget(grp);
        auto* rl  = new QHBoxLayout(row);
        rl->setContentsMargins(0, 4, 0, 0);
        rl->setSpacing(8);

        auto* sizeLbl   = new QLabel(sizeText, row);
        sizeLbl->setObjectName("cardDesc");
        rl->addWidget(sizeLbl, 1);

        auto* statusLbl = new QLabel("", row);
        statusLbl->setObjectName("hintLabel");
        rl->addWidget(statusLbl, 1);

        auto* btn = new QPushButton("\xf0\x9f\x97\x91  Pulisci", row);
        btn->setObjectName("actionBtn");
        btn->setProperty("danger", "true");
        btn->setFixedWidth(110);
        rl->addWidget(btn);

        gl->addWidget(row);

        /* Salva i puntatori come proprietà della card per accesso nei lambda */
        grp->setProperty("_sizeLbl",   QVariant::fromValue<QObject*>(sizeLbl));
        grp->setProperty("_statusLbl", QVariant::fromValue<QObject*>(statusLbl));
        grp->setProperty("_btn",       QVariant::fromValue<QObject*>(btn));
        return grp;
    };

    /* ──────────────────────────────────────────────────────────
       SEZIONE 1: Esportazioni
       ────────────────────────────────────────────────────────── */
    const QString esportDir = QDir::cleanPath(P::root() + "/../esportazioni");
    {
        auto [bytes, cnt] = calcFilesSize(esportDir);
        auto* grp = makeCard(
            "\xf0\x9f\x93\xa6",
            "Esportazioni",
            "Output del multi-agente TUI: file .txt, .py, .pyx, .html, .json generati nelle sessioni precedenti.",
            QString("Percorso: %1").arg(esportDir),
            QString("%1 file \xc2\xb7 %2").arg(cnt).arg(fmtBytes(bytes)));
        lay->addWidget(grp);

        auto* btn       = qobject_cast<QPushButton*>(grp->property("_btn").value<QObject*>());
        auto* statusLbl = qobject_cast<QLabel*>(grp->property("_statusLbl").value<QObject*>());
        auto* sizeLbl   = qobject_cast<QLabel*>(grp->property("_sizeLbl").value<QObject*>());

        connect(btn, &QPushButton::clicked, grp,
                [btn, statusLbl, sizeLbl, esportDir, calcFilesSize, fmtBytes]() {
            int removed = 0;
            QDirIterator it(esportDir, QDir::Files | QDir::Hidden);
            while (it.hasNext()) {
                it.next();
                if (QFile::remove(it.filePath())) ++removed;
            }
            statusLbl->setText(QString("\xe2\x9c\x85  %1 file rimossi").arg(removed));
            btn->setEnabled(false);
            auto [b2, c2] = calcFilesSize(esportDir);
            sizeLbl->setText(QString("%1 file \xc2\xb7 %2").arg(c2).arg(fmtBytes(b2)));
        });
    }

    /* ──────────────────────────────────────────────────────────
       SEZIONE 2: Artefatti build Qt
       ────────────────────────────────────────────────────────── */
    {
        const QString qtGuiDir = QDir::cleanPath(P::root() + "/Qt_GUI");
        const QStringList buildDirs = {
            qtGuiDir + "/build",
            qtGuiDir + "/build_tests",
            qtGuiDir + "/build_debug",
            qtGuiDir + "/build_check",
            qtGuiDir + "/build_dbg",
        };
        qint64 totalBytes = 0;
        int    existCount = 0;
        for (const QString& d : buildDirs) {
            if (!QDir(d).exists()) continue;
            ++existCount;
            auto [b, _c] = calcFilesSize(d);
            totalBytes += b;
        }
        auto* grp = makeCard(
            "\xf0\x9f\x94\xa8",
            "Artefatti build Qt",
            "Cartelle generate da cmake: build/, build_tests/, build_debug/, build_check/, build_dbg/. "
            "Ricrea con: cmake -B build && cmake --build build",
            QString("Percorso: %1/build*/").arg(qtGuiDir),
            QString("%1 cartell%2 \xc2\xb7 %3")
                .arg(existCount)
                .arg(existCount == 1 ? "a" : "e")
                .arg(fmtBytes(totalBytes)));
        lay->addWidget(grp);

        auto* btn       = qobject_cast<QPushButton*>(grp->property("_btn").value<QObject*>());
        auto* statusLbl = qobject_cast<QLabel*>(grp->property("_statusLbl").value<QObject*>());
        auto* sizeLbl   = qobject_cast<QLabel*>(grp->property("_sizeLbl").value<QObject*>());

        connect(btn, &QPushButton::clicked, grp,
                [btn, statusLbl, sizeLbl, buildDirs, fmtBytes]() {
            int removed = 0;
            for (const QString& d : buildDirs)
                if (QDir(d).exists() && QDir(d).removeRecursively()) ++removed;
            statusLbl->setText(QString("\xe2\x9c\x85  %1 cartell%2 rimosse")
                               .arg(removed).arg(removed == 1 ? "a" : "e"));
            btn->setEnabled(false);
            sizeLbl->setText("0 cartelle \xc2\xb7 0 B");
        });
    }

    /* ──────────────────────────────────────────────────────────
       SEZIONE 3: Cache Python (__pycache__)
       ────────────────────────────────────────────────────────── */
    {
        const QString pyDir = QDir::cleanPath(P::root() + "/../Python_Software");
        int    cacheCount = 0;
        qint64 cacheBytes = 0;
        {
            QDirIterator it(pyDir, QStringList{"__pycache__"},
                            QDir::Dirs | QDir::NoDotAndDotDot,
                            QDirIterator::Subdirectories);
            while (it.hasNext()) {
                it.next();
                ++cacheCount;
                auto [b, _c] = calcFilesSize(it.filePath());
                cacheBytes += b;
            }
        }
        auto* grp = makeCard(
            "\xf0\x9f\x90\x8d",
            "Cache Python",
            "Cartelle __pycache__ generate dall'interprete Python in Python_Software/. "
            "Vengono ricreate automaticamente alla prossima esecuzione.",
            QString("Percorso: %1/*/__pycache__/").arg(pyDir),
            QString("%1 cartell%2 \xc2\xb7 %3")
                .arg(cacheCount)
                .arg(cacheCount == 1 ? "a" : "e")
                .arg(fmtBytes(cacheBytes)));
        lay->addWidget(grp);

        auto* btn       = qobject_cast<QPushButton*>(grp->property("_btn").value<QObject*>());
        auto* statusLbl = qobject_cast<QLabel*>(grp->property("_statusLbl").value<QObject*>());
        auto* sizeLbl   = qobject_cast<QLabel*>(grp->property("_sizeLbl").value<QObject*>());

        connect(btn, &QPushButton::clicked, grp,
                [btn, statusLbl, sizeLbl, pyDir]() {
            int removed = 0;
            QStringList toRemove;
            QDirIterator it(pyDir, QStringList{"__pycache__"},
                            QDir::Dirs | QDir::NoDotAndDotDot,
                            QDirIterator::Subdirectories);
            while (it.hasNext()) { it.next(); toRemove << it.filePath(); }
            for (const QString& d : toRemove)
                if (QDir(d).removeRecursively()) ++removed;
            statusLbl->setText(QString("\xe2\x9c\x85  %1 cartell%2 rimosse")
                               .arg(removed).arg(removed == 1 ? "a" : "e"));
            btn->setEnabled(false);
            sizeLbl->setText("0 cartelle \xc2\xb7 0 B");
        });
    }

    /* ──────────────────────────────────────────────────────────
       SEZIONE 4: File temporanei editor (prismalux_code.*)
       ────────────────────────────────────────────────────────── */
    {
        const QString tmpDir = QStandardPaths::writableLocation(QStandardPaths::TempLocation);
        auto [bytes, cnt] = calcFilesSize(tmpDir, {"prismalux_code*"});
        auto* grp = makeCard(
            "\xf0\x9f\x93\x84",
            "File temporanei editor",
            "File prismalux_code.* e prismalux_code_bin lasciati dalla pagina Programmazione "
            "dopo ogni esecuzione di codice.",
            QString("Percorso: %1/prismalux_code*").arg(tmpDir),
            QString("%1 file \xc2\xb7 %2").arg(cnt).arg(fmtBytes(bytes)));
        lay->addWidget(grp);

        auto* btn       = qobject_cast<QPushButton*>(grp->property("_btn").value<QObject*>());
        auto* statusLbl = qobject_cast<QLabel*>(grp->property("_statusLbl").value<QObject*>());
        auto* sizeLbl   = qobject_cast<QLabel*>(grp->property("_sizeLbl").value<QObject*>());

        connect(btn, &QPushButton::clicked, grp,
                [btn, statusLbl, sizeLbl, tmpDir]() {
            int removed = 0;
            QDirIterator it(tmpDir, QStringList{"prismalux_code*"}, QDir::Files);
            while (it.hasNext()) {
                it.next();
                if (QFile::remove(it.filePath())) ++removed;
            }
            statusLbl->setText(QString("\xe2\x9c\x85  %1 file rimossi").arg(removed));
            btn->setEnabled(false);
            sizeLbl->setText("0 file \xc2\xb7 0 B");
        });
    }

    /* ──────────────────────────────────────────────────────────
       SEZIONE 5: Cartelle pesanti (solo informazione)
       ────────────────────────────────────────────────────────── */
    {
        auto* grp = new QGroupBox(w);
        grp->setObjectName("cardGroup");
        auto* gl  = new QVBoxLayout(grp);
        gl->setContentsMargins(12, 12, 12, 12);
        gl->setSpacing(6);

        auto* hdr = new QLabel(
            "\xe2\x9a\xa0\xef\xb8\x8f  Cartelle pesanti (rimozione manuale)", grp);
        hdr->setObjectName("cardTitle");
        gl->addWidget(hdr);

        const QList<QPair<QString,QString>> heavy = {
            { "C_software/llama_cpp_studio/",
              QDir::cleanPath(P::llamaStudioDir()) },
        };
        bool anyFound = false;
        for (const auto& [label, path] : heavy) {
            if (!QDir(path).exists()) continue;
            anyFound = true;
            auto [b, _c] = calcFilesSize(path);
            auto* lbl = new QLabel(
                QString("\xf0\x9f\x93\x81  <b>%1</b>  \xe2\x80\x94  %2  \xe2\x80\x94  sorgenti llama.cpp (git clone)")
                    .arg(label).arg(fmtBytes(b)),
                grp);
            lbl->setObjectName("cardDesc");
            lbl->setTextFormat(Qt::RichText);
            lbl->setWordWrap(true);
            gl->addWidget(lbl);
        }
        if (!anyFound) {
            auto* lbl = new QLabel("\xe2\x9c\x85  Nessuna cartella llama_cpp_studio trovata.", grp);
            lbl->setObjectName("hintLabel");
            gl->addWidget(lbl);
        }
        auto* note = new QLabel(
            "Necessarie per compilare llama-server localmente (tab Personalizza \xe2\x86\x92 llama.cpp Studio). "
            "Rimuovi manualmente solo se non usi pi\xc3\xb9 llama.cpp locale.",
            grp);
        note->setObjectName("hintLabel");
        note->setWordWrap(true);
        gl->addWidget(note);
        lay->addWidget(grp);
    }

    /* ── Backup dati ── */
    {
        auto* grp = new QGroupBox(
            "\xf0\x9f\x92\xbe  Backup dati Prismalux", w);
        auto* gl  = new QVBoxLayout(grp);
        gl->setContentsMargins(12, 12, 12, 8);
        gl->setSpacing(6);

        auto* desc = new QLabel(
            "Crea un backup ZIP dei database SQLite (<b>graph_memory.db</b>, "
            "<b>rag_graph.db</b>, <b>chat_history.db</b>) e del log di accesso. "
            "I file vengono copiati nella cartella scelta con timestamp nel nome.",
            grp);
        desc->setObjectName("cardDesc");
        desc->setTextFormat(Qt::RichText);
        desc->setWordWrap(true);
        gl->addWidget(desc);

        const QString prismaDir = QDir::homePath() + "/.prismalux";
        const QStringList backupFiles = {
            prismaDir + "/graph_memory.db",
            prismaDir + "/rag_graph.db",
            prismaDir + "/chat_history.db",
            prismaDir + "/access.log",
        };
        qint64 totalBytes = 0;
        for (const QString& f : backupFiles) {
            QFileInfo fi(f);
            if (fi.exists()) totalBytes += fi.size();
        }
        auto* sizeInfo = new QLabel(
            QString("Dimensione stimata: <b>%1</b>  —  Cartella: <code>~/.prismalux/</code>")
                .arg(fmtBytes(totalBytes)),
            grp);
        sizeInfo->setObjectName("hintLabel");
        sizeInfo->setTextFormat(Qt::RichText);
        gl->addWidget(sizeInfo);

        auto* btnRow  = new QHBoxLayout;
        auto* backupBtn = new QPushButton(
            "\xf0\x9f\x92\xbe  Crea backup ZIP...", grp);
        backupBtn->setObjectName("actionBtn");
        backupBtn->setFixedWidth(180);
        auto* openDirBtn = new QPushButton(
            "\xf0\x9f\x93\x82  Apri cartella dati", grp);
        openDirBtn->setObjectName("actionBtn");
        openDirBtn->setFixedWidth(160);
        auto* statusLbl = new QLabel("", grp);
        statusLbl->setObjectName("statusLabel");
        btnRow->addWidget(backupBtn);
        btnRow->addWidget(openDirBtn);
        btnRow->addWidget(statusLbl, 1);
        gl->addLayout(btnRow);

        connect(backupBtn, &QPushButton::clicked, grp, [=]() mutable {
            const QString ts = QDateTime::currentDateTime()
                                   .toString("yyyy-MM-dd_HH-mm-ss");
            const QString defName =
                QString("prismalux_backup_%1").arg(ts);
            const QString destDir = QFileDialog::getExistingDirectory(
                nullptr, "Scegli cartella di destinazione backup",
                QDir::homePath());
            if (destDir.isEmpty()) return;

            int copied = 0;
            QStringList failed;
            for (const QString& src : backupFiles) {
                if (!QFile::exists(src)) continue;
                const QFileInfo fi(src);
                const QString dst =
                    destDir + "/" + defName + "_" + fi.fileName();
                if (QFile::copy(src, dst))
                    ++copied;
                else
                    failed << fi.fileName();
            }
            if (copied > 0 && failed.isEmpty())
                statusLbl->setText(
                    QString("\xe2\x9c\x85  %1 file copiati in %2/")
                        .arg(copied).arg(QDir(destDir).dirName()));
            else if (!failed.isEmpty())
                statusLbl->setText(
                    QString("\xe2\x9a\xa0\xef\xb8\x8f  %1 copiati, errori: %2")
                        .arg(copied)
                        .arg(failed.join(", ")));
            else
                statusLbl->setText("\xe2\x9a\xa0\xef\xb8\x8f  Nessun file trovato da copiare.");
        });

        connect(openDirBtn, &QPushButton::clicked, grp, [prismaDir]() {
            QDesktopServices::openUrl(QUrl::fromLocalFile(prismaDir));
        });

        lay->addWidget(grp);
    }

    /* ── Audit segreti ── */
    {
        auto* grp = new QGroupBox(
            "\xf0\x9f\x94\x90  Audit segreti & permessi", w);
        auto* gl  = new QVBoxLayout(grp);
        gl->setContentsMargins(12, 12, 12, 8);
        gl->setSpacing(6);

        auto* desc = new QLabel(
            "Verifica che file sensibili non siano esposti: "
            "<code>.env</code> in <code>.gitignore</code>, "
            "permessi <code>0600</code> su token LAN, "
            "nessun segreto in chiaro nei settings Qt.",
            grp);
        desc->setObjectName("cardDesc");
        desc->setTextFormat(Qt::RichText);
        desc->setWordWrap(true);
        gl->addWidget(desc);

        auto* auditBtn = new QPushButton(
            "\xf0\x9f\x94\x8d  Esegui audit ora", grp);
        auditBtn->setObjectName("actionBtn");
        auditBtn->setFixedWidth(160);
        auto* auditLog = new QTextEdit(grp);
        auditLog->setReadOnly(true);
        auditLog->setFixedHeight(150);
        auditLog->setPlaceholderText("Risultati audit...");
        gl->addWidget(auditBtn);
        gl->addWidget(auditLog);

        connect(auditBtn, &QPushButton::clicked, grp, [=]() {
            auditLog->clear();
            int pass = 0, warn = 0;

            auto ok  = [&](const QString& msg){ auditLog->append("\xe2\x9c\x85 " + msg); ++pass; };
            auto bad = [&](const QString& msg){ auditLog->append("\xe2\x9d\x8c " + msg); ++warn; };
            auto info= [&](const QString& msg){ auditLog->append("\xe2\x84\xb9\xef\xb8\x8f " + msg); };

            const QString root = QString(PRISMALUX_ROOT);

            /* 1. .env in .gitignore */
            const QString gitignore = root + "/.gitignore";
            if (QFile::exists(gitignore)) {
                QFile f(gitignore);
                if (f.open(QIODevice::ReadOnly)) {
                    const QString content = QString::fromUtf8(f.readAll());
                    if (content.contains(".env"))
                        ok(".env presente in .gitignore");
                    else
                        bad(".env NON trovato in .gitignore — aggiungilo!");
                    if (content.contains("*.key") || content.contains("lan_token"))
                        ok("File *.key / token LAN in .gitignore");
                    else
                        bad("*.key non in .gitignore — aggiungere '*.key'");
                }
            } else {
                bad(".gitignore non trovato in " + root);
            }

            /* 2. Permessi file sensibili (solo Linux) */
#ifndef Q_OS_WIN
            const QStringList sensitiveFiles = {
                QDir::homePath() + "/.prismalux/lan_token.key",
                QDir::homePath() + "/.prismalux/graph_memory.db",
                QDir::homePath() + "/.prismalux/rag_graph.db",
            };
            for (const QString& path : sensitiveFiles) {
                if (!QFile::exists(path)) continue;
                QFileInfo fi(path);
                const QFile::Permissions p = fi.permissions();
                const bool groupRead  = p & (QFile::ReadGroup  | QFile::WriteGroup  | QFile::ExeGroup);
                const bool otherRead  = p & (QFile::ReadOther  | QFile::WriteOther  | QFile::ExeOther);
                if (!groupRead && !otherRead)
                    ok("Permessi 0600 su " + fi.fileName());
                else
                    bad("Permessi troppo aperti su " + fi.fileName() + " — esegui: chmod 600 " + path);
            }
#else
            info("Verifica permessi file non disponibile su Windows");
#endif

            /* 3. Nessun token LAN in QSettings in chiaro */
            QSettings s("Prismalux", "GUI");
            if (s.contains("lan/token") || s.contains("lan_token") || s.contains("token")) {
                const QString val = s.value("lan/token",
                    s.value("lan_token", s.value("token"))).toString();
                if (!val.isEmpty() && val.length() > 8)
                    bad("Token LAN trovato in QSettings — usa QKeychain per sicurezza");
                else
                    ok("Nessun token LAN in chiaro in QSettings");
            } else {
                ok("Nessun token LAN in chiaro in QSettings");
            }

            /* 4. KNOWLEDGE_USER non in git */
            const QString kuDir = root + "/KNOWLEDGE_USER";
            if (QDir(kuDir).exists()) {
                if (QFile::exists(gitignore)) {
                    QFile f(gitignore);
                    if (f.open(QIODevice::ReadOnly)) {
                        if (QString::fromUtf8(f.readAll()).contains("KNOWLEDGE_USER"))
                            ok("KNOWLEDGE_USER in .gitignore");
                        else
                            bad("KNOWLEDGE_USER NON in .gitignore — dati privati esposti!");
                    }
                }
            } else {
                info("KNOWLEDGE_USER non trovata — skip");
            }

            /* 5. RAG/ non in git */
            const QString ragDir = root + "/RAG";
            if (QDir(ragDir).exists()) {
                if (QFile::exists(gitignore)) {
                    QFile f(gitignore);
                    if (f.open(QIODevice::ReadOnly)) {
                        if (QString::fromUtf8(f.readAll()).contains("RAG/") ||
                            QString::fromUtf8(f.readAll()).contains("/RAG"))
                            ok("RAG/ in .gitignore");
                        else
                            bad("RAG/ NON in .gitignore — documenti privati esposti!");
                    }
                }
            }

            auditLog->append(QString("\n\xe2\x80\x94\xe2\x80\x94 %1 controlli OK, %2 avvisi \xe2\x80\x94\xe2\x80\x94")
                .arg(pass).arg(warn));
        });

        lay->addWidget(grp);
    }

    lay->addStretch();
    return w;
}

/* ══════════════════════════════════════════════════════════════
   buildRingraziamentiTab — licenza MIT + crediti autore, librerie e MCPs.
   Widget Qt nativi (nessun colore hardcodato): palette tema automatica.
   Link cliccabili via setCellWidget + QLabel HTML (QPalette::Link).
   ══════════════════════════════════════════════════════════════ */

