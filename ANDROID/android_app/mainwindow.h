#pragma once
#include <QMainWindow>
#include <QStackedWidget>
#include <QToolBar>
#include <QActionGroup>

class AiClient;
class RagEngineSimple;
class ChatPage;
class CameraPage;
class BlePage;
class SettingsPage;

/* ══════════════════════════════════════════════════════════════
   MainWindow — finestra principale con bottom navigation bar.

   Layout fisso (verticale):
     ┌────────────────────────────────┐
     │  QStackedWidget (pagine)        │  ← flex, occupa tutto lo spazio
     ├────────────────────────────────┤
     │  BottomBar: Chat│Camera│BT│⚙   │  ← 56px fissi
     └────────────────────────────────┘

   Le pagine condividono un'unica istanza di AiClient e RagEngineSimple
   passate come puntatori nel costruttore di ciascuna pagina.
   ══════════════════════════════════════════════════════════════ */
class MainWindow : public QMainWindow {
    Q_OBJECT
public:
    explicit MainWindow(QWidget* parent = nullptr);
    ~MainWindow() override = default;

private slots:
    void onTabChanged(int index);

private:
    void buildBottomBar();
    void applyPermissions();   // richiede permessi Android a runtime

    QStackedWidget* m_stack       = nullptr;
    QToolBar*       m_bottomBar   = nullptr;
    QActionGroup*   m_tabGroup    = nullptr;

    /* Risorse condivise tra le pagine */
    AiClient*         m_ai  = nullptr;
    RagEngineSimple*  m_rag = nullptr;

    /* Pagine */
    ChatPage*     m_chatPage     = nullptr;
    CameraPage*   m_cameraPage   = nullptr;
    BlePage*      m_blePage      = nullptr;
    SettingsPage* m_settingsPage = nullptr;
};
