#pragma once
#include <QWidget>
#include "../model_processor.h"

class QListWidget;
class QListWidgetItem;
class QLineEdit;
class QTextEdit;
class QCheckBox;
class QComboBox;
class QLabel;
class QPushButton;
class QGroupBox;

/* ══════════════════════════════════════════════════════════════
   ModelProfilesTab — tab "Profili Modello" nelle Impostazioni.

   Lista profili (QListWidget) + form di editing inline.
   Le modifiche sono salvate su QSettings via ModelProcessor::saveProfiles().
   ══════════════════════════════════════════════════════════════ */
class ModelProfilesTab : public QWidget {
    Q_OBJECT
public:
    explicit ModelProfilesTab(QWidget* parent = nullptr);

private slots:
    void onListSelectionChanged();
    void onAddClicked();
    void onDeleteClicked();
    void onLoadDefaultsClicked();
    void onSaveClicked();
    void onToggleEnabledClicked();

private:
    void buildUi();
    void refreshList();
    void loadProfileIntoForm(const ModelProfile& p);
    ModelProfile profileFromForm() const;
    int currentRow() const;

    /* Lista */
    QListWidget*  m_list = nullptr;
    QPushButton*  m_btnAdd    = nullptr;
    QPushButton*  m_btnDel    = nullptr;
    QPushButton*  m_btnDefs   = nullptr;
    QPushButton*  m_btnToggle = nullptr;

    /* Form editing */
    QLineEdit*  m_edLabel    = nullptr;
    QLineEdit*  m_edPattern  = nullptr;
    QCheckBox*  m_chkEnabled = nullptr;

    /* Pre-processing */
    QComboBox*  m_cmbLang    = nullptr;   // Forza lingua (indice → PreAction)
    QCheckBox*  m_chkPrepSys  = nullptr;
    QTextEdit*  m_edPrepSys   = nullptr;
    QCheckBox*  m_chkPrepUser = nullptr;
    QLineEdit*  m_edPrepUser  = nullptr;
    QCheckBox*  m_chkReplSys  = nullptr;
    QTextEdit*  m_edReplSys   = nullptr;

    /* Post-processing */
    QCheckBox*  m_chkPostThink = nullptr;
    QCheckBox*  m_chkPostMd    = nullptr;
    QCheckBox*  m_chkPostRx    = nullptr;
    QLineEdit*  m_edRxFrom     = nullptr;
    QLineEdit*  m_edRxTo       = nullptr;
    QComboBox*  m_cmbTranslate    = nullptr;  // Traduzione LLM (lingua)
    QComboBox*  m_cmbTransModel   = nullptr;  // Modello dedicato traduzione (combo)
    QLineEdit*  m_edTransModelCustom = nullptr; // campo libero per modello personalizzato

    QPushButton* m_btnSave = nullptr;

    bool m_loading = false; // blocca onListSelectionChanged durante refreshList
};
