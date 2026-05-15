#include "tablet_nav_rail.h"
#include <QLabel>

TabletNavRail::TabletNavRail(QWidget* parent)
    : QWidget(parent)
    , m_layout(new QVBoxLayout(this))
    , m_group(new QButtonGroup(this))
{
    setFixedWidth(80);
    m_layout->setContentsMargins(4, 12, 4, 12);
    m_layout->setSpacing(4);
    m_layout->addStretch();

    m_group->setExclusive(true);
    connect(m_group, &QButtonGroup::idClicked, this, &TabletNavRail::tabSelected);

    setStyleSheet(R"(
        TabletNavRail {
            background: #12151f;
            border-right: 1px solid #2a2d3a;
        }
        QPushButton {
            background: transparent;
            border: none;
            border-radius: 16px;
            color: #8890a8;
            font-size: 22px;
            padding: 8px;
            min-width: 48px;
            min-height: 48px;
        }
        QPushButton:checked {
            background: #1e2236;
            color: #00c8ff;
        }
        QPushButton:hover:!checked {
            background: #1a1d27;
        }
    )");
}

void TabletNavRail::addTab(const QString& icon, const QString& label, int index)
{
    auto* btn = new QPushButton(icon, this);
    btn->setCheckable(true);
    btn->setToolTip(label);
    btn->setChecked(index == 0);

    m_group->addButton(btn, index);
    m_layout->insertWidget(m_layout->count() - 1, btn);
}
