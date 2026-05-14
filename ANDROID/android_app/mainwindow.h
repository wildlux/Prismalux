#pragma once
#include <QMainWindow>
#include <QStackedWidget>
#include <QToolBar>
#include <QActionGroup>

class AiClient;
class RagEngineSimple;
class ChatPage;
class StudioPage;
class LavoroPage;
class ObsPage;
class MisurePage;
class SettingsPage;

#ifdef HAVE_MULTIMEDIA
class CameraPage;
#endif
#ifdef HAVE_BLE
class BlePage;
#endif

/* ══════════════════════════════════════════════════════════════
   MainWindow — finestra principale con bottom navigation bar.

   Layout fisso (verticale):
     ┌────────────────────────────────┐
     │  QStackedWidget (pagine)        │
     ├────────────────────────────────┤
     │  BottomBar: Chat│Lavoro│OBS│    │
     │             Misure│Camera│BT│⚙  │
     └────────────────────────────────┘
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
    void applyPermissions();

    QStackedWidget* m_stack     = nullptr;
    QToolBar*       m_bottomBar = nullptr;
    QActionGroup*   m_tabGroup  = nullptr;

    AiClient*        m_ai  = nullptr;
    RagEngineSimple* m_rag = nullptr;

    ChatPage*     m_chatPage     = nullptr;
    StudioPage*   m_studioPage   = nullptr;
    LavoroPage*   m_lavoroPage   = nullptr;
    ObsPage*      m_obsPage      = nullptr;
    MisurePage*   m_misurePage   = nullptr;
    SettingsPage* m_settingsPage = nullptr;

#ifdef HAVE_MULTIMEDIA
    CameraPage*   m_cameraPage   = nullptr;
#else
    QWidget*      m_cameraPage   = nullptr;
#endif

#ifdef HAVE_BLE
    BlePage*      m_blePage      = nullptr;
#else
    QWidget*      m_blePage      = nullptr;
#endif

    /* indici degli stack per tab switching */
    int m_idxChat     = 0;
    int m_idxStudio   = 1;
    int m_idxLavoro   = 2;
    int m_idxObs      = 3;
    int m_idxMisure   = 4;
    int m_idxCamera   = 5;
    int m_idxBle      = 6;
    int m_idxSettings = 7;
};
