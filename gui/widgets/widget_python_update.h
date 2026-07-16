#pragma once
/*
 * widget_python_update.h — Pannello aggiornamento librerie Python di Prismalux.
 *
 * Scope: solo i pacchetti elencati in requirements.txt (non l'intero
 * ambiente Python di sistema). Per ciascuno:
 *   1. "Controlla aggiornamenti" legge versione installata (pip list) e
 *      ultima disponibile (pip list --outdated) in due sole chiamate pip.
 *   2. L'utente può accettare l'ultima versione o scrivere una versione
 *      precedente nel campo "Versione target" (downgrade a scelta).
 *   3. pip install pkg==target; se l'installazione riesce, viene subito
 *      verificato "import <modulo>" — se fallisce (incompatibilità), il
 *      pacchetto viene reinstallato alla versione precedente (backup preso
 *      prima di ogni update) e la coda "Aggiorna tutti" si ferma lì,
 *      lasciando gli altri pacchetti alla versione di default già in uso.
 */

#include <QFrame>
#include <QLabel>
#include <QLineEdit>
#include <QProcess>
#include <QPushButton>
#include <QScrollArea>
#include <QTextBrowser>
#include <QVBoxLayout>
#include <QHBoxLayout>
#include <QJsonDocument>
#include <QJsonArray>
#include <QJsonObject>
#include "../prismalux_paths.h"
#include "../dpi_utils.h"

namespace P = PrismaluxPaths;

class PythonUpdatePanel : public QFrame {
    Q_OBJECT

public:
    struct PyPkg { const char* pip; const char* importName; };

    explicit PythonUpdatePanel(QWidget* parent = nullptr) : QFrame(parent)
    {
        setObjectName("actionCard");
        auto* lay = new QVBoxLayout(this);
        lay->setContentsMargins(14, 10, 14, 10);
        lay->setSpacing(8);

        auto* header = new QWidget(this);
        auto* headerLay = new QHBoxLayout(header);
        headerLay->setContentsMargins(0, 0, 0, 0);
        headerLay->setSpacing(8);

        auto* title = new QLabel(
            tr("\xf0\x9f\x90\x8d  <b>Librerie Python (Prismalux)</b>"), header);
        title->setObjectName("cardTitle");
        title->setTextFormat(Qt::RichText);
        headerLay->addWidget(title, 1);

        m_btnCheck = new QPushButton(tr("\xf0\x9f\x94\x84  Controlla aggiornamenti"), header);
        m_btnCheck->setObjectName("actionBtn");
        m_btnCheck->setFixedHeight(dpiScale(28));
        headerLay->addWidget(m_btnCheck);

        m_btnUpdateAll = new QPushButton(tr("\xe2\xac\x87  Aggiorna tutti"), header);
        m_btnUpdateAll->setObjectName("dangerBtn");
        m_btnUpdateAll->setFixedHeight(dpiScale(28));
        m_btnUpdateAll->setEnabled(false);
        headerLay->addWidget(m_btnUpdateAll);

        lay->addWidget(header);

        auto* descLbl = new QLabel(
            tr("Solo i pacchetti richiesti da Prismalux (requirements.txt). "
            "Puoi modificare \xe2\x80\x9cVersione target\xe2\x80\x9d per un downgrade: "
            "prima di ogni modifica la versione attuale viene salvata e "
            "ripristinata automaticamente se il modulo non si importa più "
            "dopo l'installazione."), this);
        descLbl->setObjectName("cardDesc");
        descLbl->setWordWrap(true);
        lay->addWidget(descLbl);

        auto* scroll = new QScrollArea(this);
        scroll->setWidgetResizable(true);
        scroll->setFrameShape(QFrame::NoFrame);
        scroll->setMinimumHeight(dpiScale(260));

        m_grid = new QWidget;
        m_gridLay = new QVBoxLayout(m_grid);
        m_gridLay->setContentsMargins(0, 4, 0, 0);
        m_gridLay->setSpacing(3);
        buildRows();
        scroll->setWidget(m_grid);
        lay->addWidget(scroll, 1);

        m_log = new QTextBrowser(this);
        m_log->setMaximumHeight(dpiScale(130));
        m_log->setObjectName("logBrowser");
        m_log->setOpenExternalLinks(false);
        m_log->hide();
        lay->addWidget(m_log);

        connect(m_btnCheck,     &QPushButton::clicked, this, &PythonUpdatePanel::onCheckClicked);
        connect(m_btnUpdateAll, &QPushButton::clicked, this, &PythonUpdatePanel::onUpdateAllClicked);
    }

    ~PythonUpdatePanel() override
    {
        const auto procs = findChildren<QProcess*>(QString(), Qt::FindDirectChildrenOnly);
        for (auto* proc : procs) {
            proc->blockSignals(true);
            if (proc->state() != QProcess::NotRunning)
                proc->kill();
        }
    }

private slots:
    void onCheckClicked()
    {
        m_btnCheck->setEnabled(false);
        m_btnUpdateAll->setEnabled(false);
        for (auto& r : m_rows) {
            r.statusLbl->setText(tr("\xe2\x8f\xb3 ..."));
            r.updateBtn->setEnabled(false);
        }

        auto* proc = new QProcess(this);
        proc->setProperty("stage", "list");
        connect(proc, QOverload<int, QProcess::ExitStatus>::of(&QProcess::finished),
                this, &PythonUpdatePanel::onPipQueryFinished);
        proc->start(P::findPython(), {"-m", "pip", "list", "--format=json"});
    }

    void onPipQueryFinished(int code, QProcess::ExitStatus)
    {
        auto* proc = qobject_cast<QProcess*>(sender());
        if (!proc) return;
        const QString stage = proc->property("stage").toString();
        const QByteArray out = proc->readAllStandardOutput();
        proc->deleteLater();

        if (code != 0) {
            m_btnCheck->setEnabled(true);
            for (auto& r : m_rows) r.statusLbl->setText(tr("\xe2\x9d\x8c pip non raggiungibile"));
            return;
        }

        const QJsonArray arr = QJsonDocument::fromJson(out).array();
        if (stage == "list") {
            m_installed.clear();
            for (const QJsonValue& v : arr) {
                const QJsonObject o = v.toObject();
                m_installed[normalize(o.value("name").toString())] = o.value("version").toString();
            }
            /* Seconda chiamata: solo i pacchetti con update disponibile */
            auto* p2 = new QProcess(this);
            p2->setProperty("stage", "outdated");
            connect(p2, QOverload<int, QProcess::ExitStatus>::of(&QProcess::finished),
                    this, &PythonUpdatePanel::onPipQueryFinished);
            p2->start(P::findPython(), {"-m", "pip", "list", "--outdated", "--format=json"});
            return;
        }

        /* stage == "outdated" */
        QMap<QString, QString> latest;
        for (const QJsonValue& v : arr) {
            const QJsonObject o = v.toObject();
            latest[normalize(o.value("name").toString())] = o.value("latest_version").toString();
        }
        applyCheckResults(latest);
        m_btnCheck->setEnabled(true);
    }

    void onUpdateSingleClicked()
    {
        auto* btn = qobject_cast<QPushButton*>(sender());
        if (!btn) return;
        const int idx = btn->property("pkgIdx").toInt();
        if (idx < 0 || idx >= m_rows.size()) return;
        m_updateQueue.clear();
        m_updateQueue.append(idx);
        m_log->show();
        m_log->clear();
        m_btnCheck->setEnabled(false);
        m_btnUpdateAll->setEnabled(false);
        doNextUpdate();
    }

    void onUpdateAllClicked()
    {
        m_updateQueue.clear();
        for (int i = 0; i < m_rows.size(); ++i) {
            const Row& r = m_rows[i];
            if (!r.updateBtn->isEnabled()) continue;
            if (r.targetEdit->text().trimmed() == r.installedVersion) continue;
            m_updateQueue.append(i);
        }
        if (m_updateQueue.isEmpty()) return;
        m_log->show();
        m_log->clear();
        m_btnCheck->setEnabled(false);
        m_btnUpdateAll->setEnabled(false);
        doNextUpdate();
    }

    void doNextUpdate()
    {
        if (m_updateQueue.isEmpty()) {
            m_log->append("\n<b>\xe2\x9c\x85  Aggiornamento completato.</b>");
            m_btnCheck->setEnabled(true);
            m_btnUpdateAll->setEnabled(true);
            return;
        }
        const int idx = m_updateQueue.takeFirst();
        Row& r = m_rows[idx];
        const QString target = r.targetEdit->text().trimmed();
        if (target.isEmpty()) { doNextUpdate(); return; }

        r.backupVersion = r.installedVersion;   /* "quelle di default" da ripristinare */
        r.statusLbl->setText(tr("\xe2\x8f\xb3 installo ") + target + "...");
        m_log->append("<b>\xe2\xac\x87  " + QString::fromUtf8(r.pkg.pip)
                      + "==" + target.toHtmlEscaped() + "...</b>");

        auto* proc = new QProcess(this);
        proc->setProperty("pkgIdx", idx);
        proc->setProperty("targetVersion", target);
        connect(proc, &QProcess::readyReadStandardOutput,
                this, &PythonUpdatePanel::onInstallProcReadyRead);
        connect(proc, &QProcess::readyReadStandardError,
                this, &PythonUpdatePanel::onInstallProcReadyRead);
        connect(proc, QOverload<int, QProcess::ExitStatus>::of(&QProcess::finished),
                this, &PythonUpdatePanel::onInstallProcFinished);
        proc->start(P::findPython(), {"-m", "pip", "install", "--break-system-packages",
                    QString::fromUtf8(r.pkg.pip) + "==" + target});
    }

    void onInstallProcReadyRead()
    {
        auto* proc = qobject_cast<QProcess*>(sender());
        if (!proc) return;
        const QByteArray out = proc->readAllStandardOutput() + proc->readAllStandardError();
        if (!out.isEmpty())
            m_log->append(QString::fromLocal8Bit(out).toHtmlEscaped());
    }

    void onInstallProcFinished(int code, QProcess::ExitStatus)
    {
        auto* proc = qobject_cast<QProcess*>(sender());
        if (!proc) return;
        const int idx = proc->property("pkgIdx").toInt();
        const QString target = proc->property("targetVersion").toString();
        proc->deleteLater();
        if (idx < 0 || idx >= m_rows.size()) { doNextUpdate(); return; }
        Row& r = m_rows[idx];

        if (code != 0) {
            r.statusLbl->setText(tr("<span style='color:#ef4444;'>\xe2\x9c\x96 errore pip install</span>"));
            r.statusLbl->setTextFormat(Qt::RichText);
            m_log->append("<span style='color:#ef4444;'>\xe2\x9c\x96  pip install fallito per "
                          + QString::fromUtf8(r.pkg.pip) + "</span>");
            stopQueueOnFailure();
            return;
        }

        /* Installato — ora verifica compatibilità con un import diretto */
        auto* checkProc = new QProcess(this);
        checkProc->setProperty("pkgIdx", idx);
        checkProc->setProperty("targetVersion", target);
        connect(checkProc, QOverload<int, QProcess::ExitStatus>::of(&QProcess::finished),
                this, &PythonUpdatePanel::onImportCheckFinished);
        checkProc->start(P::findPython(), {"-c", "import " + QString::fromUtf8(r.pkg.importName)});
    }

    void onImportCheckFinished(int code, QProcess::ExitStatus)
    {
        auto* proc = qobject_cast<QProcess*>(sender());
        if (!proc) return;
        const int idx = proc->property("pkgIdx").toInt();
        const QString target = proc->property("targetVersion").toString();
        proc->deleteLater();
        if (idx < 0 || idx >= m_rows.size()) { doNextUpdate(); return; }
        Row& r = m_rows[idx];

        if (code == 0) {
            r.installedVersion = target;
            r.statusLbl->setText(tr("<span style='color:#22c55e;'>\xe2\x9c\x85 ") + target + "</span>");
            r.statusLbl->setTextFormat(Qt::RichText);
            m_log->append("<span style='color:#22c55e;'>\xe2\x9c\x85  "
                          + QString::fromUtf8(r.pkg.pip) + " aggiornato a " + target + "</span>");
            doNextUpdate();
            return;
        }

        /* Import fallito: incompatibilità — ripristina la versione di default */
        m_log->append("<span style='color:#fbbf24;'>\xe2\x9a\xa0  " + QString::fromUtf8(r.pkg.pip)
                      + "==" + target + " non \xc3\xa8 compatibile (import fallito). "
                      "Ripristino " + r.backupVersion + "...</span>");
        r.statusLbl->setText(tr("\xe2\x8f\xb3 rollback..."));

        auto* rbProc = new QProcess(this);
        rbProc->setProperty("pkgIdx", idx);
        connect(rbProc, &QProcess::readyReadStandardOutput,
                this, &PythonUpdatePanel::onInstallProcReadyRead);
        connect(rbProc, &QProcess::readyReadStandardError,
                this, &PythonUpdatePanel::onInstallProcReadyRead);
        connect(rbProc, QOverload<int, QProcess::ExitStatus>::of(&QProcess::finished),
                this, &PythonUpdatePanel::onRollbackFinished);
        rbProc->start(P::findPython(), {"-m", "pip", "install", "--break-system-packages",
                      QString::fromUtf8(r.pkg.pip) + "==" + r.backupVersion});
    }

    void onRollbackFinished(int code, QProcess::ExitStatus)
    {
        auto* proc = qobject_cast<QProcess*>(sender());
        if (!proc) return;
        const int idx = proc->property("pkgIdx").toInt();
        proc->deleteLater();
        if (idx < 0 || idx >= m_rows.size()) { stopQueueOnFailure(); return; }
        Row& r = m_rows[idx];

        if (code == 0) {
            r.statusLbl->setText(tr("<span style='color:#fbbf24;'>\xe2\x86\xa9 ripristinato ")
                                 + r.backupVersion + "</span>");
            m_log->append("<span style='color:#fbbf24;'>\xe2\x86\xa9  " + QString::fromUtf8(r.pkg.pip)
                          + " ripristinato alla versione di default " + r.backupVersion + "</span>");
        } else {
            r.statusLbl->setText(tr("<span style='color:#ef4444;'>\xe2\x9c\x96 rollback fallito!</span>"));
            m_log->append("<span style='color:#ef4444;'>\xe2\x9c\x96  Rollback di "
                          + QString::fromUtf8(r.pkg.pip) + " fallito \xe2\x80\x94 verifica manualmente "
                          "lo stato del pacchetto.</span>");
        }
        r.statusLbl->setTextFormat(Qt::RichText);
        /* Un'incompatibilità ferma la coda: gli altri pacchetti restano
         * alla versione già in uso, come richiesto. */
        stopQueueOnFailure();
    }

private:
    struct Row {
        PyPkg        pkg{};
        QLabel*      installedLbl = nullptr;
        QLabel*      latestLbl    = nullptr;
        QLineEdit*   targetEdit   = nullptr;
        QPushButton* updateBtn    = nullptr;
        QLabel*      statusLbl    = nullptr;
        QString      installedVersion;
        QString      backupVersion;
    };

    static const QList<PyPkg>& packages()
    {
        static const QList<PyPkg> kPkgs = {
            { "requests",           "requests" },
            { "numpy",              "numpy" },
            { "scipy",              "scipy" },
            { "sympy",              "sympy" },
            { "pandas",             "pandas" },
            { "matplotlib",         "matplotlib" },
            { "seaborn",            "seaborn" },
            { "plotly",             "plotly" },
            { "opencv-python",      "cv2" },
            { "Pillow",             "PIL" },
            { "scikit-learn",       "sklearn" },
            { "openpyxl",           "openpyxl" },
            { "python-docx",        "docx" },
            { "python-pptx",        "pptx" },
            { "pdfminer.six",       "pdfminer" },
            { "pypdf",              "pypdf" },
            { "psutil",             "psutil" },
            { "python-dotenv",      "dotenv" },
            { "watchdog",           "watchdog" },
            { "beautifulsoup4",     "bs4" },
            { "lxml",               "lxml" },
            { "cryptography",       "cryptography" },
            { "PyJWT",              "jwt" },
            { "faster-whisper",     "faster_whisper" },
            { "webrtcvad",          "webrtcvad" },
            { "simple-diarizer",    "simple_diarizer" },
            { "torchcodec",         "torchcodec" },
            { "resemblyzer",        "resemblyzer" },
            { "yt-dlp",             "yt_dlp" },
            { "gns3fy",             "gns3fy" },
            { "graphviz",           "graphviz" },
            { "obsws-python",       "obsws_python" },
            { "pyserial",           "serial" },
        };
        return kPkgs;
    }

    static QString normalize(const QString& name)
    {
        return name.toLower().replace('_', '-');
    }

    void buildRows()
    {
        {
            auto* hdr = new QWidget(m_grid);
            auto* hdrLay = new QHBoxLayout(hdr);
            hdrLay->setContentsMargins(4, 0, 4, 2);
            hdrLay->setSpacing(8);
            auto mk = [](const QString& t, int w) {
                auto* l = new QLabel(tr("<b>") + t + "</b>");
                l->setObjectName("hintLabel");
                if (w > 0) l->setFixedWidth(w);
                return l;
            };
            hdrLay->addWidget(mk("Pacchetto",          150), 0);
            hdrLay->addWidget(mk("Installata",          90), 0);
            hdrLay->addWidget(mk("Disponibile",         90), 0);
            hdrLay->addWidget(mk("Versione target",     110), 0);
            hdrLay->addWidget(mk("",                     70), 0);
            hdrLay->addWidget(mk("Stato", 0), 1);
            m_gridLay->addWidget(hdr);
        }

        for (const PyPkg& pkg : packages()) {
            Row r;
            r.pkg = pkg;

            auto* row = new QWidget(m_grid);
            auto* rowLay = new QHBoxLayout(row);
            rowLay->setContentsMargins(4, 2, 4, 2);
            rowLay->setSpacing(8);

            auto* nameLbl = new QLabel(
                "<code style='font-size:12px;'>" + QString::fromUtf8(pkg.pip) + "</code>", row);
            nameLbl->setTextFormat(Qt::RichText);
            nameLbl->setFixedWidth(dpiScale(150));
            rowLay->addWidget(nameLbl);

            r.installedLbl = new QLabel("\xe2\x80\x94", row);
            r.installedLbl->setFixedWidth(dpiScale(90));
            rowLay->addWidget(r.installedLbl);

            r.latestLbl = new QLabel("\xe2\x80\x94", row);
            r.latestLbl->setFixedWidth(dpiScale(90));
            rowLay->addWidget(r.latestLbl);

            r.targetEdit = new QLineEdit(row);
            r.targetEdit->setFixedWidth(dpiScale(110));
            r.targetEdit->setPlaceholderText(tr("es. 1.2.3"));
            r.targetEdit->setToolTip(tr("Modifica per scegliere una versione diversa (anche precedente)"));
            rowLay->addWidget(r.targetEdit);

            r.updateBtn = new QPushButton(tr("\xe2\xac\x87  Aggiorna"), row);
            r.updateBtn->setFixedWidth(dpiScale(70));
            r.updateBtn->setEnabled(false);
            r.updateBtn->setProperty("pkgIdx", m_rows.size());
            rowLay->addWidget(r.updateBtn);
            connect(r.updateBtn, &QPushButton::clicked, this, &PythonUpdatePanel::onUpdateSingleClicked);

            r.statusLbl = new QLabel(tr("non verificato"), row);
            r.statusLbl->setObjectName("cardDesc");
            rowLay->addWidget(r.statusLbl, 1);

            m_gridLay->addWidget(row);
            m_rows.append(r);
        }
    }

    void applyCheckResults(const QMap<QString, QString>& latestMap)
    {
        for (int i = 0; i < m_rows.size(); ++i) {
            Row& r = m_rows[i];
            const QString key = normalize(QString::fromUtf8(r.pkg.pip));
            const QString installed = m_installed.value(key);

            if (installed.isEmpty()) {
                r.installedVersion.clear();
                r.installedLbl->setText(tr("non installato"));
                r.latestLbl->setText("\xe2\x80\x94");
                r.targetEdit->clear();
                r.updateBtn->setEnabled(false);
                r.statusLbl->setText("\xe2\x80\x94");
                continue;
            }

            r.installedVersion = installed;
            r.installedLbl->setText(installed);

            const QString latest = latestMap.value(key);
            if (!latest.isEmpty() && latest != installed) {
                r.latestLbl->setText(
                    "<span style='color:#fbbf24;'>" + latest + "</span>");
                r.latestLbl->setTextFormat(Qt::RichText);
                r.targetEdit->setText(latest);
                r.statusLbl->setText(tr("aggiornamento disponibile"));
            } else {
                r.latestLbl->setText(
                    tr("<span style='color:#22c55e;'>aggiornata</span>"));
                r.latestLbl->setTextFormat(Qt::RichText);
                r.targetEdit->setText(installed);
                r.statusLbl->setText("aggiornata");
            }
            r.updateBtn->setEnabled(true);
        }
        m_btnUpdateAll->setEnabled(true);
    }

    /** Un'incompatibilità (o un errore pip) interrompe la coda "Aggiorna
     *  tutti": i pacchetti non ancora processati restano alla versione
     *  già installata, come richiesto — nessuna cascata di aggiornamenti
     *  su un ambiente già segnalato come a rischio. */
    void stopQueueOnFailure()
    {
        if (!m_updateQueue.isEmpty()) {
            m_log->append(QString("<b>\xe2\x8f\xb9  Aggiornamento interrotto \xe2\x80\x94 "
                          "%1 pacchetti rimasti alla versione attuale.</b>")
                          .arg(m_updateQueue.size()));
            m_updateQueue.clear();
        }
        m_btnCheck->setEnabled(true);
        m_btnUpdateAll->setEnabled(true);
    }

    QWidget*      m_grid    = nullptr;
    QVBoxLayout*  m_gridLay = nullptr;
    QTextBrowser* m_log     = nullptr;
    QPushButton*  m_btnCheck     = nullptr;
    QPushButton*  m_btnUpdateAll = nullptr;

    QList<Row>          m_rows;
    QMap<QString,QString> m_installed;   ///< nome normalizzato → versione installata
    QList<int>           m_updateQueue;
};
