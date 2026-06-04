#pragma once
/* ══════════════════════════════════════════════════════════════
   SciProteinWidget — Strumenti interattivi per proteine

   1. ESMFold API (Meta): predizione 3D da sequenza amminoacidica
      POST https://api.esmatlas.com/foldSequence/v1/pdb/
      Limite: ~400 aa. Gratuito, nessuna chiave.

   2. AlphaFold DB (EBI/Google DeepMind): strutture pre-calcolate
      GET https://alphafold.ebi.ac.uk/api/prediction/{UniProt_ID}
      Copre ~200 M proteine. Gratuito, nessuna chiave.

   3. RCSB PDB: ricerca strutture sperimentali (X-ray, cryo-EM, NMR)
      POST https://search.rcsb.org/rcsbsearch/v2/query
      Download: https://files.rcsb.org/download/{ID}.pdb

   4. UniProt: ricerca proteine per nome, malattia, organismo
      GET https://rest.uniprot.org/uniprotkb/search?query=...

   5. Viewer 3D: 3Dmol.js in QWebEngineView (dark theme, cartoon spectrum)
      Fallback: QTextEdit con testo PDB grezzo
   ══════════════════════════════════════════════════════════════ */
#include <QWidget>
#include <QNetworkAccessManager>

class QTextEdit;
class QLabel;
class QPushButton;
class QLineEdit;
class QListWidget;
class QSplitter;
class QTabWidget;
class QComboBox;

class SciProteinWidget : public QWidget {
    Q_OBJECT
public:
    explicit SciProteinWidget(QWidget* parent = nullptr);

    QString lastPdbData()  const { return m_lastPdb; }
    QString lastPdbLabel() const { return m_lastLabel; }

signals:
    /* Emesso quando una struttura PDB è pronta — utente può inviarla al docking */
    void pdbReady(const QString& pdbData, const QString& label);

private slots:
    void onEsmFoldClicked();
    void onAlphaFoldClicked();
    void onPdbSearchClicked();
    void onUniprotSearchClicked();
    void onSearchItemClicked(int row);
    void onDownloadPdbClicked();
    void onSendToDockClicked();

private:
    void onEsmFoldReply(QNetworkReply* reply);
    void onAlphaFoldReply(QNetworkReply* reply);
    void onPdbReply(QNetworkReply* reply);
    void onUniprotReply(QNetworkReply* reply);
    void loadInViewer(const QString& pdbData, const QString& label = {});
    void appendLog(const QString& msg);
    static QString cleanSequence(const QString& raw);

    QNetworkAccessManager* m_nam     = nullptr;
    QString                m_lastPdb;
    QString                m_lastLabel;

    /* Sequenza + predizione */
    QTextEdit*   m_seqEdit      = nullptr;
    QLineEdit*   m_uniprotEdit  = nullptr;  /* per AlphaFold DB */
    QPushButton* m_btnFold      = nullptr;
    QPushButton* m_btnAlpha     = nullptr;
    QLabel*      m_foldStatus   = nullptr;

    /* Ricerca database */
    QLineEdit*   m_pdbQuery     = nullptr;
    QLineEdit*   m_upQuery      = nullptr;
    QListWidget* m_searchList   = nullptr;

    /* Azioni sul PDB caricato */
    QPushButton* m_btnDownload  = nullptr;
    QPushButton* m_btnDock      = nullptr;

    /* Log */
    QTextEdit*   m_logView      = nullptr;

    /* Viewer 3D */
#ifdef HAVE_WEBENGINE_WIDGETS
    class QWebEngineView* m_viewer3d = nullptr;
    bool m_viewerReady = false;
#else
    QTextEdit* m_viewer3d = nullptr;
#endif
};
