#pragma once
#include <QWidget>
#include <QString>

class QComboBox;
class QTextBrowser;

/* Palette adattiva chiaro/scuro — calcolata in widget_bike.cpp::themeColors() */
struct BkColors {
    QString bg, card, bdr, accent, green, amb, red, txt, mut;
};

/* ════════════════════════════════════════════════════════════════════════════
   BikeWidget — Assistente manutenzione bicicletta
   Problemi comuni · Regolazione cambio/freni · Manopole · Manutenzione
   Tipi: Pieghevole · MTB · Cross Country · Gravel · City / Trekking
   ════════════════════════════════════════════════════════════════════════════ */
class BikeWidget : public QWidget {
    Q_OBJECT
public:
    explicit BikeWidget(QWidget* parent = nullptr);

private:
    QComboBox*    m_tipoCombo = nullptr;
    QComboBox*    m_catCombo  = nullptr;
    QTextBrowser* m_info      = nullptr;

    void buildUi();
    void updateContent();

    struct Section { QString titolo; QString html; };
    static QList<Section> sectionsProblemi(int tipo, const BkColors& c);
    static QString htmlCambio(const BkColors& c);
    static QString htmlFreni(const BkColors& c);
    static QString htmlManopole(const BkColors& c);
    static QString htmlManutenzione(const BkColors& c);

private slots:
    void onSelectionChanged();
};
