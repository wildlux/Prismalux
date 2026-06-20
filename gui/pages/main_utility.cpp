#include "main_utility.h"
#include "widget_solar_calc.h"
#include "widget_idro.h"
#include "main_jobs.h"
#include "main_finance.h"
#include <QVBoxLayout>
#include <QTabWidget>
#include <QLabel>

UtilityPage::UtilityPage(AiClient* ai, QWidget* parent)
    : QWidget(parent)
{
    auto* lay  = new QVBoxLayout(this);
    lay->setContentsMargins(0, 0, 0, 0);
    lay->setSpacing(0);

    auto* hdr = new QLabel(
        "  \xf0\x9f\x94\xa7  <b>Utility</b>"
        "  <span style='color:gray;font-size:11px;'>"
        "Fotovoltaico \xc2\xb7 Idroponica \xc2\xb7 Lavoro \xc2\xb7 Finanza \xc2\xb7 LAN & WAN"
        "</span>", this);
    hdr->setTextFormat(Qt::RichText);
    hdr->setObjectName("pageHeader");
    hdr->setFixedHeight(36);
    lay->addWidget(hdr);

    m_tabs = new QTabWidget(this);
    m_tabs->setDocumentMode(true);
    lay->addWidget(m_tabs, 1);

    m_tabs->addTab(new SolarCalcWidget(m_tabs),
                   "\xe2\x98\x80\xef\xb8\x8f  Fotovoltaico");

    m_tabs->addTab(new IdroWidget(m_tabs),
                   "\xf0\x9f\x8c\xbf  Idroponica");

    m_tabs->addTab(new LavoroPage(ai, m_tabs),
                   "\xf0\x9f\x92\xbc  Lavoro");

    m_tabs->addTab(new PraticoPage(ai, m_tabs),
                   "\xf0\x9f\x92\xb0  Finanza");
}

void UtilityPage::addTab(QWidget* w, const QString& label)
{
    if (m_tabs && w) m_tabs->addTab(w, label);
}
