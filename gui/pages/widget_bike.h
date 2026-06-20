#pragma once
#include <QWidget>

class QComboBox;
class QTextBrowser;
class QTabWidget;
class QLineEdit;
class QPushButton;
class QLabel;

/* ════════════════════════════════════════════════════════════════════════════
   BikeWidget — Assistente manutenzione bicicletta
   Problemi comuni · Regolazione cambio · Manopole / attacchi
   Tipi: Pieghevole · MTB · Cross Country · Gravel · City / Trekking
   ════════════════════════════════════════════════════════════════════════════ */
class BikeWidget : public QWidget {
    Q_OBJECT
public:
    explicit BikeWidget(QWidget* parent = nullptr);

private:
    QComboBox*   m_tipoCombo    = nullptr;
    QComboBox*   m_catCombo     = nullptr;
    QTextBrowser* m_info        = nullptr;

    void buildUi();
    void updateContent();

    /* Contenuto per categoria (indipendente dal tipo) */
    struct Section { QString titolo; QString html; };
    static QList<Section> sectionsProblemi(const QString& tipo);
    static QString htmlCambio();
    static QString htmlFreni();
    static QString htmlManopole();
    static QString htmlManutenzione();

private slots:
    void onSelectionChanged();
};
