#pragma once
#include <QMainWindow>
#include <QStackedWidget>
#include <QPushButton>
#include <QLabel>
#include <QToolButton>
#include <QPainter>
#include <QPropertyAnimation>
#include <QNetworkAccessManager>
#include <QNetworkReply>
#include <QNetworkRequest>
#include "thermal_monitor.h"
#ifdef PRISMALUX_FORM_FACTOR_TABLET
#include <QVBoxLayout>
#endif

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
class ImparaPage;
class RicercaPage;
class MatematicaPage;
class SintetizzatorePage;
class AssistentePage;
class OraclePage;
class HermesPage;
class FileAiPage;
class FinanzaPage;
class SimulatorePage;
class WanClientPage;
class SecurityPage;
class MultiAgentPage;
class ChartPage;

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
    void onDrawerAnimFinished();
    void onQuizFullscreen(bool on);
    void checkForUpdates();
    void onUpdateReply();
    void onThermalTemp(float celsius);
#ifdef PRISMALUX_FORM_FACTOR_TABLET
    void onNavTablet_0();
    void onNavTablet_1();
    void onNavTablet_2();
    void onNavTablet_3();
    void onNavTablet_4();
    void onNavTablet_5();
    void onNavTablet_6();
    void onNavTablet_7();
    void onNavTablet_8();
    void onNavTablet_9();
    void onNavTablet_10();
    void onNavTablet_11();
    void onNavTablet_12();
    void onNavTablet_13();
    void onNavTablet_14();
#endif

private:
    void buildHeaderBar();
    void buildDrawer();
    void updateDrawerGeometry();
#ifdef PRISMALUX_FORM_FACTOR_TABLET
    void buildNavRail();
#endif

    QStackedWidget*     m_stack       = nullptr;
    QWidget*            m_headerBar   = nullptr;
    QWidget*            m_drawer      = nullptr;
    QWidget*            m_overlay     = nullptr;
    QLabel*             m_titleLbl    = nullptr;
    QLabel*             m_thermalLbl  = nullptr;
    QLabel*             m_updateLbl   = nullptr;
    bool                m_drawerOpen  = false;
    QPropertyAnimation* m_drawerAnim  = nullptr;
    ThermalMonitor*     m_thermal     = nullptr;
    QNetworkAccessManager* m_netMgr   = nullptr;
    QNetworkReply*         m_updateReply = nullptr;

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
    AudioPage*      m_audioPage      = nullptr;
    ImparaPage*          m_imparaPage          = nullptr;
    RicercaPage*         m_ricercaPage         = nullptr;
    MatematicaPage*      m_matematicaPage      = nullptr;
    SintetizzatorePage*  m_sintetizzatorePage  = nullptr;
    AssistentePage*      m_assistentePage      = nullptr;
    OraclePage*          m_oraclePage          = nullptr;
    HermesPage*          m_hermesPage          = nullptr;
    FileAiPage*          m_fileAiPage          = nullptr;
    FinanzaPage*         m_finanzaPage         = nullptr;
    SimulatorePage*      m_simulatorePage      = nullptr;
    WanClientPage*       m_wanClientPage       = nullptr;
    SecurityPage*        m_securityPage        = nullptr;
    MultiAgentPage*      m_multiAgentPage      = nullptr;
    ChartPage*           m_chartPage           = nullptr;

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

    int m_idxChat            =  0;
    int m_idxStudio          =  1;
    int m_idxLavoro          =  2;
    int m_idxObs             =  3;
    int m_idxMisure          =  4;
    int m_idxCamera          =  5;
    int m_idxMcp             =  6;
    int m_idxBle             =  7;
    int m_idxAudio           =  8;
    int m_idxSettings        =  9;
    int m_idxInfo            = 10;
    int m_idxImpara          = 11;
    int m_idxRicerca         = 12;
    int m_idxMatematica      = 13;
    int m_idxSintetizzatore  = 14;
    int m_idxAssistente      = 15;
    int m_idxOracle          = 16;
    int m_idxHermes          = 17;
    int m_idxFileAi          = 18;
    int m_idxFinanza         = 19;
    int m_idxSimulatore      = 20;
    int m_idxWanClient       = 21;
    int m_idxSecurity        = 22;
    int m_idxMultiAgent      = 23;
    int m_idxChart           = 24;

#ifdef PRISMALUX_FORM_FACTOR_TABLET
    QWidget*     m_navRail    = nullptr;
    QVBoxLayout* m_navLayout  = nullptr;
#endif
};
