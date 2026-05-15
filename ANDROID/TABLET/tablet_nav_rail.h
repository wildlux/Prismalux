#pragma once
#include <QWidget>
#include <QVBoxLayout>
#include <QPushButton>
#include <QButtonGroup>

/* NavRail verticale stile Material 3 per tablet.
   Sostituisce il bottom bar dello smartphone. */
class TabletNavRail : public QWidget {
    Q_OBJECT
public:
    explicit TabletNavRail(QWidget* parent = nullptr);
    void addTab(const QString& icon, const QString& label, int index);

signals:
    void tabSelected(int index);

private:
    QVBoxLayout*  m_layout = nullptr;
    QButtonGroup* m_group  = nullptr;
};
