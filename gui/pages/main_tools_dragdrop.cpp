/* ══════════════════════════════════════════════════════════════
   main_tools_dragdrop.cpp — StrumentiPage: Drag&Drop + hook esterni
   =========================================================================
   Drag&drop file su RAG, hook cron panel/tab esterne, helper Office
   bridge. Split da main_tools.cpp (TODO D-8).
   ══════════════════════════════════════════════════════════════ */
#include "main_tools.h"
#include "main_maintenance.h"
#include "../prismalux_paths.h"

#include <QVBoxLayout>
#include <QDragEnterEvent>
#include <QDropEvent>
#include <QMimeData>
#include <QDir>
#include <QFile>
#include <QCoreApplication>

namespace P = PrismaluxPaths;

/* ══════════════════════════════════════════════════════════════
   Drag & Drop — rilascia PDF/TXT/MD/CSV direttamente sulla pagina
   per aggiungerli all'indice RAG senza passare dal file dialog.
   ══════════════════════════════════════════════════════════════ */
void StrumentiPage::dragEnterEvent(QDragEnterEvent* e)
{
    if (!e->mimeData()->hasUrls()) { e->ignore(); return; }

    static const QStringList kExt = { "pdf", "txt", "md", "csv", "rst" };
    for (const QUrl& u : e->mimeData()->urls()) {
        if (!u.isLocalFile()) continue;
        const QString ext = QFileInfo(u.toLocalFile()).suffix().toLower();
        if (kExt.contains(ext)) { e->acceptProposedAction(); return; }
    }
    e->ignore();
}

void StrumentiPage::dropEvent(QDropEvent* e)
{
    if (!e->mimeData()->hasUrls()) { e->ignore(); return; }

    static const QStringList kExt = { "pdf", "txt", "md", "csv", "rst" };
    bool added = false;
    for (const QUrl& u : e->mimeData()->urls()) {
        if (!u.isLocalFile()) continue;
        const QString path = u.toLocalFile();
        if (kExt.contains(QFileInfo(path).suffix().toLower())) {
            ragAddFile(path);
            added = true;
        }
    }
    if (added) {
        e->acceptProposedAction();
        /* Attiva automaticamente il checkbox RAG dopo il drop */
        if (m_ragCheck && !m_ragCheck->isChecked())
            m_ragCheck->setChecked(true);
    } else {
        e->ignore();
    }
}



/* ══════════════════════════════════════════════════════════════
   installCronPanel — sostituisce il pannello redirect con il
   vero buildCronTab() di ManutenzioneePage.
   Chiamato da MainWindow dopo la creazione di ImpostazioniPage.
   ══════════════════════════════════════════════════════════════ */
void StrumentiPage::installCronPanel(ManutenzioneePage* man)
{
    if (!man || !m_cronPanel) return;
    if (m_cronPanel->layout() && m_cronPanel->layout()->count() > 0
        && m_cronPanel->layout()->itemAt(0)
        && m_cronPanel->layout()->itemAt(0)->widget() != nullptr
        && qobject_cast<QLabel*>(m_cronPanel->layout()->itemAt(0)->widget()) == nullptr)
        return; /* già installato con widget reale — non reinstallare */

    /* Rimuovi il contenuto redirect (label + stretch) */
    QLayout* old = m_cronPanel->layout();
    if (old) {
        while (QLayoutItem* item = old->takeAt(0)) {
            if (QWidget* w = item->widget()) w->deleteLater();
            delete item;
        }
        delete old;
    }

    /* Installa il vero pannello Cron nel contenitore esistente */
    QWidget* cronWidget = man->buildCronTab();
    if (!cronWidget) return;
    auto* lay = new QVBoxLayout(m_cronPanel);
    lay->setContentsMargins(0, 0, 0, 0);
    lay->addWidget(cronWidget);
}

void StrumentiPage::addExternalTab(QWidget* w, const QString& label)
{
    if (m_tabs && w) m_tabs->addTab(w, label);
}

/* ══════════════════════════════════════════════════════════════
   Helper privati
   ══════════════════════════════════════════════════════════════ */
void StrumentiPage::_readOfficeBridgeToken()
{
    const QString tokenFile = QDir::homePath() + "/.prismalux_office_token";
    QFile f(tokenFile);
    if (!f.open(QIODevice::ReadOnly | QIODevice::Text)) return;
    m_officeBridgeToken = QString::fromUtf8(f.readAll()).trimmed();
}

QString StrumentiPage::_officeBridgePath() const
{
    QDir d(QCoreApplication::applicationDirPath());
    for (int i = 0; i < 4; i++) {
        QString p = d.filePath("MCPs/office_bridge/prismalux_office_bridge.py");
        if (QFile::exists(p)) return p;
        d.cdUp();
    }
    return {};
}
