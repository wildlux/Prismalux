#include "main_bioinformatica.h"
#include "widget_cytoscape.h"
#include "widget_rdkit.h"
#include "widget_bioconda.h"
#include "widget_avogadro.h"
#include "widget_rab0l.h"
#include "widget_blhm.h"
#include <QTabWidget>
#include <QVBoxLayout>

BioinformaticaPage::BioinformaticaPage(AiClient* ai, QWidget* parent)
    : QWidget(parent)
{
    auto* lay  = new QVBoxLayout(this);
    lay->setContentsMargins(0, 0, 0, 0);
    auto* tabs = new QTabWidget(this);
    tabs->setDocumentMode(true);
    tabs->addTab(new CytoscapeWidget(ai, tabs), "\xf0\x9f\x95\xb8  Cytoscape");
    tabs->addTab(new RDKitWidget(ai, tabs),     "\xf0\x9f\xa7\xaa  RDKit");
    tabs->addTab(new BiocondaWidget(ai, tabs),  "\xf0\x9f\x8c\xbf  Bioconda");
    tabs->addTab(new AvogadroWidget(ai, tabs),  "\xf0\x9f\xa7\xaa  Avogadro");
    tabs->addTab(new Rab0lWidget(tabs),         "RAB\xe2\x82\x80-L");
    tabs->addTab(new BlhmWidget(tabs),          "\xf0\x9f\xa7\xa0  BLHM");
    lay->addWidget(tabs);
}
