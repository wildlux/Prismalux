#pragma once
#include <QMainWindow>
#include <QStackedWidget>
#include <QPushButton>
#include <QLabel>
#include <QToolButton>
#include <QPainter>

/* ── HamburgerButton: disegna 3 linee via QPainter (evita font gap su Android) ── */
class HamburgerButton : public QToolButton {
    Q_OBJECT
public:
    explicit HamburgerButton(QWidget* parent = nullptr) : QToolButton(parent) {
        setObjectName("HamburgerBtn");
        setAutoRaise(true);
    }
protected:
    void paintEvent(QPaintEvent*) override {
        QPainter p(this);
        p.setRenderHint(QPainter::Antialiasing);
        const int w = width(), h = height();
        const int lw = w * 56 / 100;
        const int lh = 3;
        const int lx = (w - lw) / 2;
        QColor c = palette().buttonText().color();
        p.fillRect(lx,   h/4 - lh/2, lw, lh, c);
        p.fillRect(lx,   h/2 - lh/2, lw, lh, c);
        p.fillRect(lx, 3*h/4 - lh/2, lw, lh, c);
    }
};

class AiClient;
class LocalLlmClient;
class RagEngineSimple;
class ChatPage;
class StudioPage;
class LavoroPage;
class ObsPage;
class MisurePage;
class SettingsPage;
class InfoPage;
class McpAddonsPage;
class AudioPage;

#ifdef HAVE_MULTIMEDIA
class CameraPage;
#endif
#ifdef HAVE_BLE
class BlePage;
#endif

/* --------------------------------------------------------------
   MainWindow -- finestra principale con hamburger menu laterale.

   Layout (verticale):
     ----------------------------------
     -  TopBar: ☰ + Titolo            -
     ----------------------------------
     -  QStackedWidget (pagine)        -
     ----------------------------------

   Cassetto laterale (sovrapposto, mosso con ☰):
     -  Lista voci di navigazione      -
   -------------------------------------------------------------- */
class MainWindow : public QMainWindow {
    Q_OBJECT
public:
    explicit MainWindow(QWidget* parent = nullptr);
    ~MainWindow() override = default;

protected:
    void resizeEvent(QResizeEvent* e) override;
    bool eventFilter(QObject* obj, QEvent* ev) override;

private slots:
    void onTabChanged(int index);
    void onToggleDrawer();
    void onDrawerNavClicked();
    void onQuizFullscreen(bool on);

private:
    void buildHeaderBar();
    void buildDrawer();
    void updateDrawerGeometry();

    QStackedWidget* m_stack      = nullptr;
    QWidget*        m_headerBar  = nullptr;
    QWidget*        m_drawer     = nullptr;
    QWidget*        m_overlay    = nullptr;
    QLabel*         m_titleLbl   = nullptr;
    bool            m_drawerOpen = false;

    AiClient*        m_ai       = nullptr;
    LocalLlmClient*  m_localLlm = nullptr;
    RagEngineSimple* m_rag      = nullptr;

    ChatPage*     m_chatPage     = nullptr;
    StudioPage*   m_studioPage   = nullptr;
    LavoroPage*   m_lavoroPage   = nullptr;
    ObsPage*      m_obsPage      = nullptr;
    MisurePage*    m_misurePage   = nullptr;
    McpAddonsPage* m_mcpPage      = nullptr;
    SettingsPage*  m_settingsPage = nullptr;
    InfoPage*      m_infoPage     = nullptr;
    AudioPage*     m_audioPage    = nullptr;

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

    int m_idxChat     = 0;
    int m_idxStudio   = 1;
    int m_idxLavoro   = 2;
    int m_idxObs      = 3;
    int m_idxMisure   = 4;
    int m_idxCamera   = 5;
    int m_idxMcp      = 6;
    int m_idxBle      = 7;
    int m_idxAudio    = 8;
    int m_idxSettings = 9;
    int m_idxInfo     = 10;
};
