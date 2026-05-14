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

    lay->addStretch();
    return w;
}

/* ══════════════════════════════════════════════════════════════
   buildRingraziamentiTab — licenza MIT + crediti autore, librerie e MCPs.
   Widget Qt nativi (nessun colore hardcodato): palette tema automatica.
   Link cliccabili via setCellWidget + QLabel HTML (QPalette::Link).
   ══════════════════════════════════════════════════════════════ */

