#pragma once
#include <QWidget>
#include <QDialog>
#include "../ai_client.h"
#include "../hardware_monitor.h"
#include "../monitor_panel.h"

class PersonalizzaPage;
class ManutenzioneePage;
class GraficoCanvas;

/* ══════════════════════════════════════════════════════════════
   ImpostazioniPage — Personalizzazione + Manutenzione
   Layout flat: QTabWidget con 2 tab, nessun menu intermedio.
   ══════════════════════════════════════════════════════════════ */
class ImpostazioniPage : public QWidget {
    Q_OBJECT
public:
    explicit ImpostazioniPage(AiClient* ai, HardwareMonitor* hw, QWidget* parent = nullptr);
    void onHWReady(HWInfo hw);

    /** Porta in primo piano il tab il cui titolo contiene @p name (case-insensitive) */
    void switchToTab(const QString& name);

    /** Aggiunge il tab "Grafico" con i controlli di posizione degli assi.
     *  Deve essere chiamato DOPO la costruzione, quando il canvas è disponibile. */
    void setGraficoCanvas(GraficoCanvas* canvas);

    /** Percorso del binario Piper installato localmente (o stringa vuota se assente). */
    static QString piperBinPath();
    /** Cartella voci ~/.prismalux/piper/voices/ */
    static QString piperVoicesDir();
    /** Voce attiva salvata dall'utente (nome file ONNX senza estensione). */
    static QString piperActiveVoice();
    /** Salva la voce attiva. */
    static void    savePiperActiveVoice(const QString& name);

signals:
    /** Emesso quando l'utente cambia la modalità etichette tab (in tempo reale). */
    void tabModeChanged(const QString& mode);
    /** Emesso quando l'utente cambia lo stile di navigazione (in tempo reale). */
    void navStyleChanged(const QString& style);
    /** Emesso quando l'utente cambia la modalità dei pulsanti di esecuzione. */
    void execBtnModeChanged(const QString& mode);

private:
    QWidget* buildTemaTab();
    QWidget* buildTestTab();
    QWidget* buildVoceTab();
    QWidget* buildTrascriviTab();
    QWidget* buildGraficoTab(GraficoCanvas* canvas);
    QWidget* buildAiLocaleTab();
    QWidget* buildRagTab();
    QWidget* buildDipendenzeTab();
    QWidget* buildLlmConsigliatiTab();

    PersonalizzaPage*  m_personalizza  = nullptr;
    ManutenzioneePage* m_manutenzione  = nullptr;
    QTabWidget*        m_tabs          = nullptr;
    AiClient*          m_ai            = nullptr;
    QDialog*           m_studioWidget  = nullptr;  ///< llama.cpp Studio dialog (lazy)
};
