#pragma once
#include <QWidget>
class AiClient;
class QTabWidget;

/* ══════════════════════════════════════════════════════════════
   UtilityPage — tab [5]
   Raccoglie: Fotovoltaico · Idroponica · Lavoro · Finanza · LAN & WAN
   ══════════════════════════════════════════════════════════════ */
class UtilityPage : public QWidget {
    Q_OBJECT
public:
    explicit UtilityPage(AiClient* ai, QWidget* parent = nullptr);
    void addTab(QWidget* w, const QString& label);

private:
    QTabWidget* m_tabs = nullptr;
};
