#pragma once
/*
 * widget_docker_update.h — Pannello aggiornamento container Docker installati.
 *
 * Elenca tutti i container (docker ps -a), permette di scaricare l'ultima
 * immagine (docker pull) per uno o tutti. Il pull NON ricrea da solo un
 * container avviato con un semplice `docker run`: per evitare di perdere
 * dati/configurazione (volumi anonimi, flag custom), la ricreazione
 * automatica avviene SOLO per container gestiti da docker-compose (rilevati
 * dalle label com.docker.compose.*), dove `docker compose up -d
 * --force-recreate` è l'operazione prevista e sicura. Per gli altri
 * container viene mostrato solo un promemoria per la ricreazione manuale.
 */

#include <QFrame>
#include <QLabel>
#include <QProcess>
#include <QPushButton>
#include <QScrollArea>
#include <QTextBrowser>
#include <QVBoxLayout>
#include <QHBoxLayout>
#include <QSet>
#include <QVariant>
#include "../prismalux_paths.h"
#include "../dpi_utils.h"

namespace P = PrismaluxPaths;

class DockerUpdatePanel : public QFrame {
    Q_OBJECT

public:
    explicit DockerUpdatePanel(QWidget* parent = nullptr) : QFrame(parent)
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
            "\xf0\x9f\x90\xb3  <b>Container Docker</b>", header);
        title->setObjectName("cardTitle");
        title->setTextFormat(Qt::RichText);
        headerLay->addWidget(title, 1);

        m_btnRefresh = new QPushButton("\xf0\x9f\x94\x84  Elenca container", header);
        m_btnRefresh->setObjectName("actionBtn");
        m_btnRefresh->setFixedHeight(dpiScale(28));
        headerLay->addWidget(m_btnRefresh);

        m_btnUpdateAll = new QPushButton("\xe2\xac\x87  Aggiorna tutte le immagini", header);
        m_btnUpdateAll->setObjectName("dangerBtn");
        m_btnUpdateAll->setFixedHeight(dpiScale(28));
        m_btnUpdateAll->setEnabled(false);
        headerLay->addWidget(m_btnUpdateAll);

        lay->addWidget(header);

        m_statusLbl = new QLabel(this);
        m_statusLbl->setObjectName("cardDesc");
        m_statusLbl->setWordWrap(true);
        lay->addWidget(m_statusLbl);

        m_grid = new QWidget(this);
        m_gridLay = new QVBoxLayout(m_grid);
        m_gridLay->setContentsMargins(0, 4, 0, 0);
        m_gridLay->setSpacing(4);
        lay->addWidget(m_grid);

        m_log = new QTextBrowser(this);
        m_log->setMaximumHeight(dpiScale(130));
        m_log->setObjectName("logBrowser");
        m_log->setOpenExternalLinks(false);
        m_log->hide();
        lay->addWidget(m_log);

        connect(m_btnRefresh,   &QPushButton::clicked, this, &DockerUpdatePanel::onRefreshClicked);
        connect(m_btnUpdateAll, &QPushButton::clicked, this, &DockerUpdatePanel::onUpdateAllClicked);

        const QString docker = P::findDocker();
        if (docker.isEmpty()) {
            m_statusLbl->setText(
                "\xf0\x9f\x94\xb4  Docker non disponibile su questo PC "
                "(binario assente o daemon non raggiungibile).");
            m_btnRefresh->setEnabled(false);
        } else {
            m_statusLbl->setText("Premi \xe2\x80\x9c" "Elenca container\xe2\x80\x9d per vedere lo stato.");
        }
    }

    ~DockerUpdatePanel() override
    {
        const auto procs = findChildren<QProcess*>(QString(), Qt::FindDirectChildrenOnly);
        for (auto* proc : procs) {
            proc->blockSignals(true);
            if (proc->state() != QProcess::NotRunning)
                proc->kill();
        }
    }

private slots:
    void onRefreshClicked()
    {
        qDeleteAll(m_rows);
        m_rows.clear();
        m_containers.clear();
        m_btnUpdateAll->setEnabled(false);
        m_statusLbl->setText("\xe2\x8f\xb3  Elenco container in corso...");

        auto* proc = new QProcess(this);
        connect(proc, QOverload<int, QProcess::ExitStatus>::of(&QProcess::finished),
                this, &DockerUpdatePanel::onListFinished);
        proc->start(P::findDocker(),
                    {"ps", "-a", "--format", "{{.ID}}\t{{.Names}}\t{{.Image}}\t{{.Status}}"});
    }

    void onListFinished(int code, QProcess::ExitStatus)
    {
        auto* proc = qobject_cast<QProcess*>(sender());
        if (!proc) return;
        const QString out = QString::fromUtf8(proc->readAllStandardOutput());
        proc->deleteLater();

        if (code != 0) {
            m_statusLbl->setText("\xe2\x9d\x8c  Impossibile interrogare Docker.");
            return;
        }

        const QStringList lines = out.split('\n', Qt::SkipEmptyParts);
        for (const QString& line : lines) {
            const QStringList f = line.split('\t');
            if (f.size() < 4) continue;
            Container c;
            c.id     = f[0];
            c.name   = f[1];
            c.image  = f[2];
            c.status = f[3];
            m_containers.append(c);
        }

        if (m_containers.isEmpty()) {
            m_statusLbl->setText("Nessun container trovato (docker ps -a vuoto).");
            return;
        }

        m_statusLbl->setText(QString("%1 container trovati.").arg(m_containers.size()));
        buildRows();
        m_btnUpdateAll->setEnabled(true);
    }

    void onUpdateAllClicked()
    {
        m_pullQueue.clear();
        QSet<QString> seen;
        for (const Container& c : m_containers) {
            if (seen.contains(c.image)) continue;
            seen.insert(c.image);
            m_pullQueue.append(c.image);
        }
        if (m_pullQueue.isEmpty()) return;
        m_log->show();
        m_log->clear();
        m_btnUpdateAll->setEnabled(false);
        m_btnRefresh->setEnabled(false);
        doNextPull();
    }

    void onPullSingleClicked()
    {
        auto* btn = qobject_cast<QPushButton*>(sender());
        if (!btn) return;
        const QString image = btn->property("dockerImage").toString();
        if (image.isEmpty()) return;
        m_pullQueue.clear();
        m_pullQueue.append(image);
        m_log->show();
        m_log->clear();
        m_btnUpdateAll->setEnabled(false);
        m_btnRefresh->setEnabled(false);
        doNextPull();
    }

    void doNextPull()
    {
        if (m_pullQueue.isEmpty()) {
            m_log->append("\n<b>\xe2\x9c\x85  Pull completato.</b> Le immagini aggiornate "
                          "verranno usate al prossimo avvio/ricreazione del container.");
            m_btnUpdateAll->setEnabled(true);
            m_btnRefresh->setEnabled(true);
            return;
        }
        const QString image = m_pullQueue.takeFirst();
        m_log->append("<b>\xe2\xac\x87  docker pull " + image.toHtmlEscaped() + "...</b>");

        auto* proc = new QProcess(this);
        proc->setProperty("dockerImage", image);
        connect(proc, &QProcess::readyReadStandardOutput,
                this, &DockerUpdatePanel::onPullProcReadyRead);
        connect(proc, &QProcess::readyReadStandardError,
                this, &DockerUpdatePanel::onPullProcReadyRead);
        connect(proc,
                QOverload<int, QProcess::ExitStatus>::of(&QProcess::finished),
                this, &DockerUpdatePanel::onPullProcFinished);
        proc->start(P::findDocker(), {"pull", image});
    }

    void onPullProcReadyRead()
    {
        auto* proc = qobject_cast<QProcess*>(sender());
        if (!proc) return;
        const QByteArray out = proc->readAllStandardOutput() + proc->readAllStandardError();
        if (!out.isEmpty())
            m_log->append(QString::fromLocal8Bit(out).toHtmlEscaped());
    }

    void onPullProcFinished(int code, QProcess::ExitStatus)
    {
        auto* proc = qobject_cast<QProcess*>(sender());
        if (!proc) return;
        const QString image = proc->property("dockerImage").toString();
        proc->deleteLater();
        if (code == 0) {
            markImageUpdated(image);
        } else {
            m_log->append(
                "<span style='color:#ef4444;'>\xe2\x9c\x96  Errore durante il pull di "
                + image.toHtmlEscaped() + "</span>");
        }
        doNextPull();
    }

    void onRecreateClicked()
    {
        auto* btn = qobject_cast<QPushButton*>(sender());
        if (!btn) return;
        const QString name = btn->property("containerName").toString();
        const QString dir  = btn->property("composeDir").toString();
        const QString svc  = btn->property("composeService").toString();
        if (name.isEmpty() || dir.isEmpty()) return;

        m_log->show();
        m_log->append("<b>\xf0\x9f\x94\x84  Ricreo " + name.toHtmlEscaped()
                      + " via docker compose...</b>");
        btn->setEnabled(false);

        auto* proc = new QProcess(this);
        proc->setWorkingDirectory(dir);
        proc->setProperty("containerName", name);
        proc->setProperty("recreateBtn", QVariant::fromValue<QObject*>(btn));
        connect(proc, &QProcess::readyReadStandardOutput,
                this, &DockerUpdatePanel::onPullProcReadyRead);
        connect(proc, &QProcess::readyReadStandardError,
                this, &DockerUpdatePanel::onPullProcReadyRead);
        connect(proc, QOverload<int, QProcess::ExitStatus>::of(&QProcess::finished),
                this, &DockerUpdatePanel::onRecreateProcFinished);
        QStringList args = {"compose"};
        if (!svc.isEmpty())
            args << "up" << "-d" << "--no-deps" << "--force-recreate" << svc;
        else
            args << "up" << "-d" << "--force-recreate";
        proc->start(P::findDocker(), args);
    }

    void onRecreateProcFinished(int code, QProcess::ExitStatus)
    {
        auto* proc = qobject_cast<QProcess*>(sender());
        if (!proc) return;
        auto* btn = qobject_cast<QPushButton*>(
            qvariant_cast<QObject*>(proc->property("recreateBtn")));
        proc->deleteLater();
        if (code == 0) {
            m_log->append("<span style='color:#22c55e;'>\xe2\x9c\x85  Container "
                          "ricreato con la nuova immagine.</span>");
        } else {
            m_log->append("<span style='color:#ef4444;'>\xe2\x9c\x96  Ricreazione "
                          "fallita \xe2\x80\x94 controlla il log sopra.</span>");
            if (btn) btn->setEnabled(true);
        }
    }

private:
    struct Container { QString id, name, image, status; };

    void buildRows()
    {
        for (int i = 0; i < m_containers.size(); ++i) {
            const Container& c = m_containers[i];

            auto* row = new QWidget(m_grid);
            auto* rowLay = new QVBoxLayout(row);
            rowLay->setContentsMargins(0, 2, 0, 2);
            rowLay->setSpacing(2);

            auto* topLine = new QWidget(row);
            auto* topLay  = new QHBoxLayout(topLine);
            topLay->setContentsMargins(0, 0, 0, 0);
            topLay->setSpacing(8);

            auto* nameLbl = new QLabel(
                "<code style='font-size:12px;'><b>" + c.name.toHtmlEscaped() + "</b></code>"
                " <span style='color:#888;font-size:11px;'>" + c.image.toHtmlEscaped() + "</span>",
                topLine);
            nameLbl->setTextFormat(Qt::RichText);
            topLay->addWidget(nameLbl, 1);

            auto* statusLbl = new QLabel(c.status, topLine);
            statusLbl->setObjectName("cardDesc");
            statusLbl->setFixedWidth(dpiScale(150));
            topLay->addWidget(statusLbl);

            auto* pullBtn = new QPushButton("\xe2\xac\x87", topLine);
            pullBtn->setFixedSize(dpiScale(28), dpiScale(24));
            pullBtn->setToolTip("docker pull " + c.image);
            pullBtn->setProperty("dockerImage", c.image);
            topLay->addWidget(pullBtn);
            connect(pullBtn, &QPushButton::clicked, this, &DockerUpdatePanel::onPullSingleClicked);

            rowLay->addWidget(topLine);

            /* Riga stato immagine + ricreazione (nascosta finché non si aggiorna) */
            auto* noteLbl = new QLabel(row);
            noteLbl->setObjectName("cardDesc");
            noteLbl->setWordWrap(true);
            noteLbl->hide();
            rowLay->addWidget(noteLbl);

            m_gridLay->addWidget(row);
            m_rows.append(row);
            m_noteForContainer[c.name] = noteLbl;
            m_containerNames[c.image].append(c.name);
        }
    }

    /** Dopo un pull riuscito, avvia "docker inspect" per ogni container che usa
     *  quell'immagine — onInspectFinished() decide come applicare l'update. */
    void markImageUpdated(const QString& image)
    {
        for (const QString& name : m_containerNames.value(image)) {
            if (!m_noteForContainer.contains(name)) continue;
            auto* proc = new QProcess(this);
            proc->setProperty("containerName", name);
            proc->setProperty("image", image);
            connect(proc, QOverload<int, QProcess::ExitStatus>::of(&QProcess::finished),
                    this, &DockerUpdatePanel::onInspectFinished);
            proc->start(P::findDocker(), {"inspect", name, "--format",
                "com.docker.compose.project.working_dir={{index .Config.Labels "
                "\"com.docker.compose.project.working_dir\"}}\n"
                "com.docker.compose.service={{index .Config.Labels "
                "\"com.docker.compose.service\"}}"});
        }
    }

private slots:
    /** Continuazione di markImageUpdated(): decide ricreazione automatica
     *  (compose) o promemoria manuale, in base alle label ispezionate. */
    void onInspectFinished(int, QProcess::ExitStatus)
    {
        auto* proc = qobject_cast<QProcess*>(sender());
        if (!proc) return;
        const QString name  = proc->property("containerName").toString();
        const QString image = proc->property("image").toString();
        const QString labels = QString::fromUtf8(proc->readAllStandardOutput());
        proc->deleteLater();

        QLabel* note = m_noteForContainer.value(name, nullptr);
        if (!note) return;

        const QString dir = labelValue(labels, "com.docker.compose.project.working_dir");
        const QString svc = labelValue(labels, "com.docker.compose.service");

        note->setText("\xe2\x9c\x85  Immagine <code>" + image.toHtmlEscaped() + "</code> aggiornata.");
        note->show();

        if (!dir.isEmpty()) {
            auto* recreateBtn = new QPushButton(
                "\xf0\x9f\x94\x84  Ricrea ora (docker compose)", note->parentWidget());
            recreateBtn->setObjectName("actionBtn");
            recreateBtn->setProperty("containerName", name);
            recreateBtn->setProperty("composeDir", dir);
            recreateBtn->setProperty("composeService", svc);
            static_cast<QVBoxLayout*>(note->parentWidget()->layout())->addWidget(recreateBtn);
            connect(recreateBtn, &QPushButton::clicked, this, &DockerUpdatePanel::onRecreateClicked);
        } else {
            note->setText(note->text() +
                "<br><span style='color:#fbbf24;'>\xe2\x9a\xa0 "
                "\xe2\x80\x9c" + name.toHtmlEscaped() + "\xe2\x80\x9d non \xc3\xa8 "
                "gestito da docker-compose: per usare la nuova immagine ricrealo "
                "manualmente (<code>docker stop " + name.toHtmlEscaped()
                + " &amp;&amp; docker rm " + name.toHtmlEscaped()
                + "</code> + il comando <code>docker run</code> originale) \xe2\x80\x94 "
                "non lo facciamo in automatico per non perdere dati nei volumi.</span>");
        }
    }

private:
    static QString labelValue(const QString& blob, const QString& key)
    {
        for (const QString& line : blob.split('\n', Qt::SkipEmptyParts)) {
            const QString prefix = key + "=";
            if (line.startsWith(prefix)) {
                const QString v = line.mid(prefix.size()).trimmed();
                if (v != "<no value>") return v;
            }
        }
        return QString();
    }

    QWidget*      m_grid    = nullptr;
    QVBoxLayout*  m_gridLay = nullptr;
    QTextBrowser* m_log     = nullptr;
    QLabel*       m_statusLbl    = nullptr;
    QPushButton*  m_btnRefresh   = nullptr;
    QPushButton*  m_btnUpdateAll = nullptr;

    QList<Container>   m_containers;
    QList<QWidget*>    m_rows;
    QStringList        m_pullQueue;
    QMap<QString, QLabel*>     m_noteForContainer; ///< nome container → riga nota
    QMap<QString, QStringList> m_containerNames;   ///< image → nomi container che la usano
};
