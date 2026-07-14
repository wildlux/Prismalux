#pragma once
/*
 * widget_dep_check.h — Pannello check + install dipendenze Python/sistema.
 *
 * Uso:
 *   QList<DepCheckPanel::Dep> deps = {
 *       { "faster-whisper", "faster_whisper", "faster-whisper",  "",       "STT 2-4× più veloce" },
 *       { "webrtcvad",      "webrtcvad",       "webrtcvad",       "",       "Rilevamento voce VAD" },
 *       { "ffmpeg",         "",                "",                "ffmpeg", "Mux/demux audio/video" },
 *   };
 *   auto* panel = new DepCheckPanel(deps, parent);
 *   panel->runAllChecks();  // lancia i check in sequenza, non blocca la UI
 */

#include <QFrame>
#include <QLabel>
#include <QProcess>
#include <QPushButton>
#include <QScrollArea>
#include <QStandardPaths>
#include <QTextBrowser>
#include <QVBoxLayout>
#include <QHBoxLayout>
#include <QTimer>
#include "../prismalux_paths.h"
#include "../dpi_utils.h"

namespace P = PrismaluxPaths;

class DepCheckPanel : public QFrame {
    Q_OBJECT

public:
    struct Dep {
        QString label;      /* nome visualizzato            */
        QString module;     /* "import X" Python check      */
        QString pipPkg;     /* pip install <pkg>            */
        QString binName;    /* "which X" se module è vuoto  */
        QString desc;       /* una riga di descrizione      */
    };

    explicit DepCheckPanel(const QList<Dep>& deps, QWidget* parent = nullptr)
        : QFrame(parent), m_deps(deps)
    {
        setObjectName("actionCard");
        auto* lay = new QVBoxLayout(this);
        lay->setContentsMargins(14, 10, 14, 10);
        lay->setSpacing(8);

        /* Titolo + pulsanti azione */
        auto* header = new QWidget(this);
        auto* headerLay = new QHBoxLayout(header);
        headerLay->setContentsMargins(0, 0, 0, 0);
        headerLay->setSpacing(8);

        auto* title = new QLabel(
            "\xf0\x9f\x93\xa6  <b>Dipendenze software</b>", header);  /* 📦 */
        title->setObjectName("cardTitle");
        title->setTextFormat(Qt::RichText);
        headerLay->addWidget(title, 1);

        m_btnCheckAll = new QPushButton(
            "\xf0\x9f\x94\x84  Controlla tutto", header);  /* 🔄 */
        m_btnCheckAll->setObjectName("actionBtn");
        m_btnCheckAll->setToolTip(tr("Verifica lo stato di ogni dipendenza"));
        m_btnCheckAll->setFixedHeight(dpiScale(28));
        headerLay->addWidget(m_btnCheckAll);

        m_btnInstallAll = new QPushButton(
            "\xe2\xac\x87  Installa mancanti", header);  /* ⬇ */
        m_btnInstallAll->setObjectName("dangerBtn");
        m_btnInstallAll->setToolTip(tr("Installa via pip le dipendenze mancanti"));
        m_btnInstallAll->setFixedHeight(dpiScale(28));
        m_btnInstallAll->setEnabled(false);
        headerLay->addWidget(m_btnInstallAll);

        lay->addWidget(header);

        /* Griglia righe dipendenze */
        m_grid = new QWidget(this);
        m_gridLay = new QVBoxLayout(m_grid);
        m_gridLay->setContentsMargins(0, 4, 0, 0);
        m_gridLay->setSpacing(4);

        for (int i = 0; i < m_deps.size(); ++i) {
            const Dep& d = m_deps[i];

            auto* row = new QWidget(m_grid);
            auto* rowLay = new QHBoxLayout(row);
            rowLay->setContentsMargins(0, 0, 0, 0);
            rowLay->setSpacing(8);

            /* Nome + descrizione */
            auto* nameCol = new QWidget(row);
            auto* nameColLay = new QVBoxLayout(nameCol);
            nameColLay->setContentsMargins(0, 0, 0, 0);
            nameColLay->setSpacing(0);

            auto* lblName = new QLabel(
                "<code style='font-size:12px;'>" + d.label.toHtmlEscaped() + "</code>",
                nameCol);
            lblName->setTextFormat(Qt::RichText);
            nameColLay->addWidget(lblName);

            if (!d.desc.isEmpty()) {
                auto* lblDesc = new QLabel(
                    "<span style='color:#888;font-size:10px;'>" +
                    d.desc.toHtmlEscaped() + "</span>",
                    nameCol);
                lblDesc->setTextFormat(Qt::RichText);
                nameColLay->addWidget(lblDesc);
            }
            rowLay->addWidget(nameCol, 1);

            /* Status */
            auto* lblStatus = new QLabel(tr("\xe2\x8f\xb3 ..."), row);  /* ⏳ */
            lblStatus->setFixedWidth(dpiScale(90));
            lblStatus->setAlignment(Qt::AlignRight | Qt::AlignVCenter);
            m_statusLabels.append(lblStatus);
            rowLay->addWidget(lblStatus);

            /* Pulsante install individuale */
            auto* btnInst = new QPushButton("\xe2\xac\x87", row);  /* ⬇ */
            btnInst->setFixedSize(dpiScale(28), dpiScale(24));
            btnInst->setToolTip(tr("Installa ") + d.label);
            btnInst->setEnabled(false);
            btnInst->setProperty("depIdx", i);
            m_installBtns.append(btnInst);
            rowLay->addWidget(btnInst);

            m_gridLay->addWidget(row);

            connect(btnInst, &QPushButton::clicked,
                    this, &DepCheckPanel::onInstallSingleClicked);
        }
        lay->addWidget(m_grid);

        /* Log output installazione (nascosto di default) */
        m_log = new QTextBrowser(this);
        m_log->setMaximumHeight(dpiScale(120));
        m_log->setObjectName("logBrowser");
        m_log->setOpenExternalLinks(false);
        m_log->hide();
        lay->addWidget(m_log);

        connect(m_btnCheckAll,   &QPushButton::clicked,
                this, &DepCheckPanel::onCheckAllClicked);
        connect(m_btnInstallAll, &QPushButton::clicked,
                this, &DepCheckPanel::onInstallAllClicked);
    }

    ~DepCheckPanel() override
    {
        /* I QProcess di check/install sono figli locali (parent=this), non membri.
         * Se ancora in esecuzione alla chiusura dell'app, il loro distruttore
         * chiama waitForFinished() e riemette finished() in modo sincrono
         * mentre DepCheckPanel è già in fase di distruzione (le QLabel/QPushButton
         * figlie, create prima nel costruttore, possono già essere deallocate) —
         * blockSignals+kill evita il use-after-free, come per LanServer::stop(). */
        const auto procs = findChildren<QProcess*>(QString(), Qt::FindDirectChildrenOnly);
        for (auto* proc : procs) {
            proc->blockSignals(true);
            if (proc->state() != QProcess::NotRunning)
                proc->kill();
        }
    }

    void runAllChecks() { onCheckAllClicked(); }

signals:
    void allOk();                  /* emesso quando tutte le dep sono ok */
    void someMissing(int count);   /* quante dep mancano dopo il check   */

private slots:
    void onCheckAllClicked()
    {
        m_checkIdx = 0;
        m_btnCheckAll->setEnabled(false);
        m_btnInstallAll->setEnabled(false);
        for (auto* s : m_statusLabels)
            s->setText(tr("\xe2\x8f\xb3 ..."));
        for (auto* b : m_installBtns)
            b->setEnabled(false);
        m_checkedOk.clear();
        m_checkedOk.resize(m_deps.size(), false);
        doNextCheck();
    }

    void doNextCheck()
    {
        if (m_checkIdx >= m_deps.size()) {
            finishChecks();
            return;
        }
        const Dep& d = m_deps[m_checkIdx];
        const QString py = P::findPython();

        QStringList args;
        QString prog;
        if (!d.module.isEmpty() && !py.isEmpty()) {
            prog = py;
            args << "-c" << "import " + d.module;
        } else if (!d.binName.isEmpty()) {
            /* "which X" come check sistema */
            prog = "which";
            args << d.binName;
        } else {
            /* nessun check possibile */
            updateStatus(m_checkIdx, false);
            ++m_checkIdx;
            doNextCheck();
            return;
        }

        auto* proc = new QProcess(this);
        proc->setProperty("depIdx", m_checkIdx);
        connect(proc,
                QOverload<int, QProcess::ExitStatus>::of(&QProcess::finished),
                this, &DepCheckPanel::onCheckProcFinished);
        proc->start(prog, args);
    }

    void onCheckProcFinished(int code, QProcess::ExitStatus)
    {
        auto* proc = qobject_cast<QProcess*>(sender());
        if (!proc) return;
        const int idx = proc->property("depIdx").toInt();
        proc->deleteLater();
        const bool ok = (code == 0);
        m_checkedOk[idx] = ok;
        updateStatus(idx, ok);
        m_installBtns[idx]->setEnabled(!ok && !m_deps[idx].pipPkg.isEmpty());
        ++m_checkIdx;
        doNextCheck();
    }

    void finishChecks()
    {
        m_btnCheckAll->setEnabled(true);
        int missing = 0;
        for (bool ok : m_checkedOk) if (!ok) ++missing;
        m_btnInstallAll->setEnabled(missing > 0);
        if (missing == 0)
            emit allOk();
        else
            emit someMissing(missing);
    }

    void onInstallAllClicked()
    {
        m_installQueue.clear();
        for (int i = 0; i < m_deps.size(); ++i)
            if (!m_checkedOk.value(i, false) && !m_deps[i].pipPkg.isEmpty())
                m_installQueue.append(i);
        if (m_installQueue.isEmpty()) return;
        m_log->show();
        m_log->clear();
        m_btnInstallAll->setEnabled(false);
        m_btnCheckAll->setEnabled(false);
        doNextInstall();
    }

    void onInstallSingleClicked()
    {
        auto* btn = qobject_cast<QPushButton*>(sender());
        if (!btn) return;
        const int idx = btn->property("depIdx").toInt();
        if (idx < 0 || idx >= m_deps.size()) return;
        m_installQueue.clear();
        m_installQueue.append(idx);
        m_log->show();
        m_log->clear();
        m_btnInstallAll->setEnabled(false);
        m_btnCheckAll->setEnabled(false);
        doNextInstall();
    }

    void doNextInstall()
    {
        if (m_installQueue.isEmpty()) {
            m_log->append("\n<b>\xe2\x9c\x85  Installazione completata. "
                          "Premi \xe2\x80\x9c" "Controlla tutto\xe2\x80\x9d per aggiornare lo stato.</b>");
            m_btnCheckAll->setEnabled(true);
            return;
        }
        const int idx = m_installQueue.takeFirst();
        const Dep& d = m_deps[idx];
        const QString py = P::findPython();

        m_log->append("<b>\xe2\xac\x87  Installo " + d.label.toHtmlEscaped() + "...</b>");
        m_statusLabels[idx]->setText(tr("\xe2\x8f\xb3 installo..."));

        auto* proc = new QProcess(this);
        proc->setProperty("depIdx", idx);
        connect(proc, &QProcess::readyReadStandardOutput,
                this, &DepCheckPanel::onInstallProcReadyRead);
        connect(proc, &QProcess::readyReadStandardError,
                this, &DepCheckPanel::onInstallProcReadyRead);
        connect(proc,
                QOverload<int, QProcess::ExitStatus>::of(&QProcess::finished),
                this, &DepCheckPanel::onInstallProcFinished);

        QStringList args = { "-m", "pip", "install", "--break-system-packages", d.pipPkg };
        proc->start(py.isEmpty() ? "python3" : py, args);
    }

    void onInstallProcReadyRead()
    {
        auto* proc = qobject_cast<QProcess*>(sender());
        if (!proc) return;
        const QByteArray out = proc->readAllStandardOutput()
                             + proc->readAllStandardError();
        if (!out.isEmpty())
            m_log->append(QString::fromLocal8Bit(out).toHtmlEscaped());
    }

    void onInstallProcFinished(int code, QProcess::ExitStatus)
    {
        auto* proc = qobject_cast<QProcess*>(sender());
        if (!proc) return;
        const int idx = proc->property("depIdx").toInt();
        proc->deleteLater();
        if (code == 0) {
            m_checkedOk[idx] = true;
            updateStatus(idx, true);
            m_installBtns[idx]->setEnabled(false);
        } else {
            m_log->append(
                "<span style='color:#ef4444;'>\xe2\x9c\x96  Errore durante l&apos;installazione di "
                + m_deps[idx].label.toHtmlEscaped() + "</span>");
        }
        doNextInstall();
    }

private:
    void updateStatus(int idx, bool ok)
    {
        if (idx < 0 || idx >= m_statusLabels.size()) return;
        if (ok) {
            m_statusLabels[idx]->setText(
                "<span style='color:#22c55e;'>\xe2\x9c\x85  OK</span>");
        } else {
            m_statusLabels[idx]->setText(
                "<span style='color:#ef4444;'>\xe2\x9c\x96  Mancante</span>");
        }
        m_statusLabels[idx]->setTextFormat(Qt::RichText);
    }

    QList<Dep>        m_deps;
    QVBoxLayout*      m_gridLay   = nullptr;
    QWidget*          m_grid      = nullptr;
    QTextBrowser*     m_log       = nullptr;
    QPushButton*      m_btnCheckAll   = nullptr;
    QPushButton*      m_btnInstallAll = nullptr;
    QList<QLabel*>    m_statusLabels;
    QList<QPushButton*> m_installBtns;
    QVector<bool>     m_checkedOk;
    QList<int>        m_installQueue;
    int               m_checkIdx = 0;
};
