#include "resource_gauge.h"
#include "../dpi_utils.h"
#include "../prismalux_paths.h"

#include <QHBoxLayout>
#include <QLabel>
#include <QProgressBar>

namespace P = PrismaluxPaths;

ResourceGauge::ResourceGauge(const QString& label, QWidget* parent)
    : QWidget(parent)
{
    auto* lay = new QHBoxLayout(this);
    lay->setContentsMargins(0,0,0,0);
    lay->setSpacing(5);

    m_lbl = new QLabel(label, this);
    m_lbl->setObjectName("gaugeLabel");
    m_lbl->setFixedWidth(dpiScale(34));

    m_bar = new QProgressBar(this);
    m_bar->setObjectName("resBar");
    m_bar->setRange(0,100);
    m_bar->setValue(0);
    m_bar->setTextVisible(false);
    m_bar->setFixedSize(dpiScale(70), dpiScale(8));

    m_pct = new QLabel("0.0%", this);
    m_pct->setObjectName("gaugePct");
    m_pct->setFixedWidth(dpiScale(42));
    /* Allineato a sinistra: il valore resta attaccato alla sua barra (chiara
       associazione barra→valore). Lo spazio di 12px tra un gauge e l'altro
       separa i gruppi. Vale per tutti (CPU/RAM/GPU e conteggio token CTX). */
    m_pct->setAlignment(Qt::AlignLeft | Qt::AlignVCenter);

    lay->addWidget(m_lbl);
    lay->addWidget(m_bar);
    lay->addWidget(m_pct);
}

void ResourceGauge::update(double pct, const QString& detail) {
    m_bar->setValue(static_cast<int>(pct));
    m_pct->setText(QString("%1%").arg(pct, 0, 'f', 1));
    setLevel(pct);
    if (!detail.isEmpty())
        setToolTip(detail);
}

void ResourceGauge::updateWithText(double pct, const QString& valueText,
                                    const QString& detail) {
    m_bar->setValue(static_cast<int>(pct));
    m_pct->setText(valueText);
    setLevel(pct);
    if (!detail.isEmpty())
        setToolTip(detail);
}

void ResourceGauge::setLevel(double pct) {
    /* Soglie QSS: < 70% verde (default), 70-90% giallo, > 90% rosso */
    const QString lvl = (pct >= 90) ? "crit" : (pct >= 70) ? "warn" : "";
    m_bar->setProperty("level", lvl);
    P::repolish(m_bar);  /* forza ricalcolo stile dopo cambio property */
}
