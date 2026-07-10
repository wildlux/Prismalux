#pragma once
#include <QWidget>
#include "../ai_client.h"
#include "../widgets/ai_error_widget.h"

class QTabWidget;
class QLineEdit;
class QTextEdit;
class QComboBox;
class QPushButton;
class QLabel;
class QProgressBar;

/* ══════════════════════════════════════════════════════════════
   ModGiochiWidget — Godot + Game Modding

   Spostati da AppControllerPage/TeleComanda (dove stavano insieme a
   tool professionali come Blender/FreeCAD/KiCAD) a UtilityPage: sono
   entrambi generazione AI per hobby videoludico, non strumenti
   professionali — appartengono semanticamente altrove.
   ══════════════════════════════════════════════════════════════ */
class ModGiochiWidget : public QWidget {
    Q_OBJECT
public:
    explicit ModGiochiWidget(AiClient* ai, QWidget* parent = nullptr);

    /** Estrae il primo blocco ```...``` dall'output AI. Public per testabilità. */
    static QString extractCode(const QString& text);
    /** Indovina l'estensione file del mod generato dal contenuto del codice. */
    static QString detectModExtension(const QString& code);

private:
    AiClient*      m_ai           = nullptr;
    QObject*       m_tokenHolder  = nullptr;
    int            m_activeTab    = -1;   ///< 0 = Godot, 1 = Game Modding
    AiErrorWidget* m_aiErrorPanel = nullptr;
    QProgressBar*  m_aiProgress   = nullptr;

    /* ── Godot (sotto-tab interna 0) ── */
    QLabel*      m_godotStatusLbl = nullptr;
    QPushButton* m_godotExecBtn   = nullptr;
    QComboBox*   m_godotAction    = nullptr;
    QComboBox*   m_godotModel     = nullptr;
    QTextEdit*   m_godotInput     = nullptr;
    QTextEdit*   m_godotOutput    = nullptr;
    QPushButton* m_godotRunBtn    = nullptr;
    QPushButton* m_godotStopBtn   = nullptr;
    QString      m_godotCode;

    /* ── Game Modding (sotto-tab interna 1) ── */
    QComboBox*   m_moddingGameCombo  = nullptr;
    QComboBox*   m_moddingTypeCombo  = nullptr;
    QComboBox*   m_moddingModel      = nullptr;
    QTextEdit*   m_moddingInput      = nullptr;
    QTextEdit*   m_moddingOutput     = nullptr;
    QPushButton* m_moddingRunBtn     = nullptr;
    QPushButton* m_moddingStopBtn    = nullptr;
    QPushButton* m_moddingSaveBtn    = nullptr;
    QLineEdit*   m_moddingFolderEdit = nullptr;
    QLabel*      m_moddingStatusLbl  = nullptr;
    QString      m_moddingCode;

    /* ── runAi session state (salvato per gli slot nominati) ── */
    QTextEdit*   m_runAiOutput     = nullptr;
    QPushButton* m_runAiRunBtn     = nullptr;
    QPushButton* m_runAiStopBtn    = nullptr;
    QComboBox*   m_runAiModelCombo = nullptr;
    int          m_runAiTabIdx     = -1;
    QString      m_runAiSys;
    QString      m_runAiUserMsg;
    /* Connessioni runAi (disconnesse prima di ogni nuova chiamata) */
    QMetaObject::Connection m_connToken;
    QMetaObject::Connection m_connFinished;
    QMetaObject::Connection m_connError;

    QWidget* buildGodotTab();
    QWidget* buildGameModdingTab();

    void runAi(int tabIdx, const QString& sys, const QString& userMsg,
               QTextEdit* output, QPushButton* runBtn, QPushButton* stopBtn,
               QComboBox* modelCombo);

    /* ── Slot estratti da lambda — runAi() ── */
    void onRunAiToken(const QString& t);
    void onRunAiFinished(const QString& full);
    void onRunAiError(const QString& msg);

    /* ── Slot estratti da lambda — Godot ── */
    void onGodotExecClicked();
    void onGodotRunClicked();
    void onGodotStopClicked();

    /* ── Slot Game Modding ── */
    void onModdingGameChanged(int idx);
    void onModdingRunClicked();
    void onModdingStopClicked();
    void onModdingSaveClicked();
    void onModdingBrowseClicked();
    void onModdingOpenFolderClicked();
};
