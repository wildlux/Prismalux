#pragma once
/* ══════════════════════════════════════════════════════════════
   PipInstaller — widget riutilizzabile per pip install.

   Astrae il pattern (label pacchetti + pulsante installa + log)
   presente in oltre 10 pagine (voice_cloner, mcp_manager, lora,
   lan_wan, sci_compute, ecc.).

   Uso minimo:
       auto* pip = new PipInstaller(this);
       pip->setPackages({"xtts", "torch"});
       connect(pip, &PipInstaller::installed, this, [](bool ok){ ... });
       layout->addWidget(pip);

   Con titolo e descrizione:
       auto* pip = new PipInstaller("Dipendenze TTS", this);
       pip->setPackages({"chatterbox-tts"});
       pip->setDescription("Necessario per la sintesi vocale offline.");
   ══════════════════════════════════════════════════════════════ */
#include <QWidget>
#include <QLabel>
#include <QPushButton>
#include <QTextEdit>
#include <QVBoxLayout>
#include <QHBoxLayout>
#include <QProcess>
#include <QStringList>
#include "../prismalux_paths.h"
#include "dpi_utils.h"

class PipInstaller : public QWidget
{
    Q_OBJECT

public:
    explicit PipInstaller(const QString& title = QString(),
                          QWidget*       parent = nullptr)
        : QWidget(parent)
    {
        auto* lay = new QVBoxLayout(this);
        lay->setContentsMargins(0, 0, 0, 0);
        lay->setSpacing(dpiScale(4));

        if (!title.isEmpty()) {
            auto* lbl = new QLabel("<b>" + title + "</b>", this);
            lay->addWidget(lbl);
        }

        m_descLabel = new QLabel(this);
        m_descLabel->setWordWrap(true);
        m_descLabel->setVisible(false);
        lay->addWidget(m_descLabel);

        auto* row = new QHBoxLayout;
        m_pkgLabel = new QLabel(this);
        m_pkgLabel->setWordWrap(true);
        row->addWidget(m_pkgLabel, 1);

        m_btn = new QPushButton(tr("\xe2\xac\x87  Installa"), this);
        m_btn->setFixedWidth(dpiScale(110));
        connect(m_btn, &QPushButton::clicked, this, &PipInstaller::onInstall);
        row->addWidget(m_btn);
        lay->addLayout(row);

        m_log = new QTextEdit(this);
        m_log->setReadOnly(true);
        m_log->setFixedHeight(dpiScale(120));
        m_log->setStyleSheet(
            "font-family:monospace;font-size:11px;"
            "background:#0f172a;color:#94a3b8;");
        lay->addWidget(m_log);
    }

    /** Imposta i pacchetti da installare. */
    void setPackages(const QStringList& pkgs)
    {
        m_packages = pkgs;
        m_pkgLabel->setText(tr("Pacchetti: ") +
                            "<code>" + pkgs.join(", ") + "</code>");
    }

    /** Testo descrittivo opzionale sopra il pulsante. */
    void setDescription(const QString& text)
    {
        m_descLabel->setText(text);
        m_descLabel->setVisible(!text.isEmpty());
    }

    /** Abilita/disabilita il pulsante. */
    void setInstallEnabled(bool on) { m_btn->setEnabled(on); }

signals:
    /** Emesso al termine. @param success true se pip è uscito con codice 0. */
    void installed(bool success);

    /** Emesso per ogni riga di output (per append in log esterno). */
    void logLine(const QString& line);

private slots:
    void onInstall()
    {
        if (m_packages.isEmpty()) return;
        m_btn->setEnabled(false);
        m_log->clear();
        appendLog(tr("pip install %1 ...").arg(m_packages.join(" ")));

        m_proc = new QProcess(this);
        m_proc->setProcessChannelMode(QProcess::MergedChannels);

        QStringList args = {"install", "--break-system-packages"};
        args << m_packages;

        connect(m_proc, &QProcess::readyRead, this, [this] {
            for (const QByteArray& raw : m_proc->readAll().split('\n')) {
                const QString line = QString::fromUtf8(raw).trimmed();
                if (!line.isEmpty()) { appendLog(line); emit logLine(line); }
            }
        });

        connect(m_proc,
                QOverload<int, QProcess::ExitStatus>::of(&QProcess::finished),
                this,
                [this](int code, QProcess::ExitStatus) {
                    const bool ok = (code == 0);
                    appendLog(ok
                        ? tr("\xe2\x9c\x85 Installazione completata.")
                        : tr("\xe2\x9d\x8c Installazione fallita (exit %1).").arg(code));
                    m_btn->setEnabled(true);
                    emit installed(ok);
                    m_proc = nullptr;
                });

        m_proc->start(PrismaluxPaths::findPython3(), args);
    }

private:
    void appendLog(const QString& msg)
    {
        m_log->append(msg);
        m_log->ensureCursorVisible();
    }

    static QString findPython3()
    {
        /* Preferisce python3, fallback su python */
        for (const QString& py : {"python3", "python"}) {
            QProcess test;
            test.start(py, {"--version"});
            if (test.waitForFinished(1000)) return py;
        }
        return "python3";
    }

    QLabel*      m_descLabel = nullptr;
    QLabel*      m_pkgLabel  = nullptr;
    QPushButton* m_btn       = nullptr;
    QTextEdit*   m_log       = nullptr;
    QStringList  m_packages;
    QProcess*    m_proc      = nullptr;
};
