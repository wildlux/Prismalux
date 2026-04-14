#pragma once
#include <QWidget>
#include <QTextEdit>
#include <QLineEdit>
#include <QPushButton>
#include <QComboBox>
#include <QListWidget>
#include <QStackedWidget>
#include <QLabel>
#include <QString>
#include <QNetworkAccessManager>
#include "../ai_client.h"

/* ══════════════════════════════════════════════════════════════
   StrumentiPage — Assistente AI multi-dominio
   Layout a due colonne: nav sinistra (200px) + stack destra.
   Categorie: Studio, Scrittura Creativa, Ricerca, Libri, Produttività
   ══════════════════════════════════════════════════════════════ */
class StrumentiPage : public QWidget {
    Q_OBJECT
public:
    explicit StrumentiPage(AiClient* ai, QWidget* parent = nullptr);

private:
    AiClient*       m_ai        = nullptr;
    QListWidget*    m_navList   = nullptr;
    QStackedWidget* m_stack     = nullptr;
    QTextEdit*      m_output    = nullptr;
    QTextEdit*      m_inputArea = nullptr;
    QPushButton*    m_btnRun    = nullptr;
    QPushButton*    m_btnStop   = nullptr;
    QComboBox*      m_cmbSub    = nullptr;  ///< Combo sotto-azione corrente
    QLabel*         m_waitLbl   = nullptr;
    bool            m_active    = false;

    /* ── Documenti PDF (cat = 5) ── */
    QWidget*        m_pdfRow    = nullptr;  ///< riga picker PDF (visibile solo per cat 5)
    QLabel*         m_pdfPathLbl = nullptr; ///< mostra il nome del PDF caricato
    QString         m_pdfPath;              ///< percorso assoluto del PDF selezionato

    /* ── Blender MCP (cat = 6) ── */
    int             m_currentCat   = 0;       ///< categoria selezionata corrente
    QWidget*        m_blenderRow   = nullptr; ///< barra connessione Blender
    QLineEdit*      m_blenderHostEdit = nullptr; ///< host:porta del bridge Blender
    QLabel*         m_blenderStatusLbl = nullptr;
    QPushButton*    m_blenderExecBtn = nullptr;  ///< esegui codice in Blender
    QNetworkAccessManager* m_blenderNam = nullptr;
    QString         m_blenderCode;             ///< codice bpy estratto dall'ultima risposta AI

    /* ── Office/LibreOffice MCP (cat = 7) ── */
    QWidget*        m_officeRow    = nullptr; ///< barra connessione Office bridge
    QLabel*         m_officeStatusLbl = nullptr;
    QPushButton*    m_officeExecBtn = nullptr;  ///< esegui codice office
    QPushButton*    m_officeStartBtn = nullptr; ///< avvia/ferma bridge Python
    QNetworkAccessManager* m_officeNam = nullptr;
    QProcess*       m_officeBridgeProc = nullptr; ///< processo bridge Python locale
    QString         m_officeCode;              ///< codice office estratto dall'ultima risposta AI
    QString         m_officeBridgeToken;       ///< token Bearer letto da ~/.prismalux_office_token

    /* ── Costruttori sotto-pagine ── */
    QWidget* buildSubPage(const QStringList& actions, const QString& placeholder);
    QWidget* buildStudio();
    QWidget* buildScritturaCreativa();
    QWidget* buildRicerca();
    QWidget* buildLibri();
    QWidget* buildProduttivita();

    /* ── Tabella system prompt per sotto-azione corrente ── */
    static QString sysPromptForAction(int navIdx, int subIdx);

    void runTool(const QString& sysPrompt, const QString& userMsg);

private slots:
    void onToken(const QString& t);
    void onFinished(const QString& full);
    void onError(const QString& msg);
};
