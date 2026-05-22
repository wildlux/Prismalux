#pragma once
#include <QVector>
#include "natal_chart_widget.h"

/* ══════════════════════════════════════════════════════════════
   AstroCalc — calcolo astronomico reale (Meeus + JPL simplified)
   Algoritmi da: "Astronomical Algorithms" Jean Meeus (2nd ed.)
   ══════════════════════════════════════════════════════════════ */
namespace AstroCalc {

struct Result {
    QVector<NatalChartWidget::Planet> planets;
    double ascLon  = 0.0;
    double mcLon   = 0.0;
    double cusps[12] = {};   /* Placidus house cusps 1-12 */
    bool   ok      = false;
    QString error;
};

/* year=anno civile, month=1-12, day=1-31
   hour=0-23, minute=0-59 (ora locale solare, UT+0 per default)
   latDeg/lonDeg = latitudine/longitudine geografica (° decimali, N+/S-/E+/W-) */
Result compute(int year, int month, int day,
               int hour, int minute,
               double latDeg, double lonDeg);

} // namespace AstroCalc
