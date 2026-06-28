#include "astro_calc.h"
#include <cmath>

/* ── Costanti ─────────────────────────────────────────────────── */
static constexpr double kPI  = M_PI;
static constexpr double kD2R = kPI / 180.0;
static constexpr double kR2D = 180.0 / kPI;

static double mod360(double x) {
    x = std::fmod(x, 360.0);
    return x < 0 ? x + 360.0 : x;
}
static double sin_d(double d) { return std::sin(d * kD2R); }
static double cos_d(double d) { return std::cos(d * kD2R); }
static double tan_d(double d) { return std::tan(d * kD2R); }
static double asin_d(double x) { return std::asin(std::max(-1.0, std::min(1.0, x))) * kR2D; }
static double acos_d(double x) { return std::acos(std::max(-1.0, std::min(1.0, x))) * kR2D; }
static double atan2_d(double y, double x) { return std::atan2(y, x) * kR2D; }

/* ── Julian Day Number (Meeus cap.7) ──────────────────────────── */
static double julianDay(int year, int month, int day, double ut)
{
    if (month <= 2) { year -= 1; month += 12; }
    const int A = year / 100;
    const int B = 2 - A + A / 4;
    return std::floor(365.25 * (year + 4716))
         + std::floor(30.6001 * (month + 1))
         + day + B - 1524.5 + ut / 24.0;
}

/* ── Obliquità eclittica (Meeus p.147) ──────────────────────── */
static double obliquity(double T)
{
    return 23.4392911111
         - (46.8150 / 3600.0) * T
         - (0.00059 / 3600.0) * T * T
         + (0.001813 / 3600.0) * T * T * T;
}

/* ── GMST in gradi (Meeus p.88) ──────────────────────────────── */
static double gmst(double JD)
{
    const double T = (JD - 2451545.0) / 36525.0;
    double theta = 280.46061837
                 + 360.98564736629 * (JD - 2451545.0)
                 + 0.000387933 * T * T
                 - T * T * T / 38710000.0;
    return mod360(theta);
}

/* ── Kepler solver (Newton-Raphson) ─────────────────────────── */
static double solveKepler(double M_deg, double e)
{
    double M = mod360(M_deg) * kD2R;
    double E = M;
    for (int i = 0; i < 50; ++i) {
        const double dE = (M - E + e * std::sin(E)) / (1.0 - e * std::cos(E));
        E += dE;
        if (std::fabs(dE) < 1e-10) break;
    }
    return E;
}

/* ── Elementi orbitali JPL J2000.0 (Meeus cap.31) ─────────── */
struct OrbEl {
    double a;
    double e, erate;
    double i, irate;
    double Om, Omrate;
    double w, wrate;
    double L, Lrate;
};

static const OrbEl kPlanets[] = {
    /* Mercurio */
    { 0.38709927, 0.20563593, 0.00001906,  7.00497902,-0.00594749,
      48.33076593,-0.12534081, 77.45779628, 0.16047689, 252.25032350, 149472.67411175 },
    /* Venere */
    { 0.72333566, 0.00677672,-0.00004107,  3.39467605,-0.00078890,
      76.67984255,-0.27769418,131.60246718, 0.00268329, 181.97909950,  58517.81538729 },
    /* Terra */
    { 1.00000261, 0.01671123,-0.00004392, -0.00001531,-0.01294668,
       0.0,        0.0,       102.93768193, 0.32327364, 100.46457166,  35999.37244981 },
    /* Marte */
    { 1.52371034, 0.09339410, 0.00007882,  1.84969142,-0.00813131,
      49.55953891,-0.29257343,-23.94362959, 0.44441088,  -4.55343205,  19140.30268499 },
    /* Giove */
    { 5.20288700, 0.04838624,-0.00013253,  1.30439695,-0.00183714,
     100.47390909, 0.20469106, 14.72847983, 0.21252668,  34.39644051,   3034.74612775 },
    /* Saturno */
    { 9.53667594, 0.05386179,-0.00050991,  2.48599187, 0.00193609,
     113.66242448,-0.28867794, 92.59887831,-0.41897216,  49.95424423,   1222.49362201 },
    /* Urano */
    { 19.18916464, 0.04725744,-0.00004397, 0.77263783,-0.00242939,
      74.01692503, 0.04240589,170.95427630, 0.40805281, 313.23810451,    428.48202785 },
    /* Nettuno */
    { 30.06992276, 0.00859048, 0.00005105, 1.77004347, 0.00035372,
     131.78422574,-0.00508664, 44.96476227,-0.32241464, -55.12002969,    218.45945325 },
    /* Plutone */
    { 39.48211675, 0.24882730, 0.00005170,17.14001206, 0.00004170,
     110.30393684,-0.01183482,224.06891629,-0.04062942, 238.92903833,    145.20780515 },
};

struct Vec3 { double x, y, z; };

static Vec3 planetXYZ(const OrbEl& el, double T)
{
    const double a  = el.a;
    const double e  = el.e   + el.erate  * T;
    const double i  = el.i   + el.irate  * T;
    const double Om = mod360(el.Om  + el.Omrate * T);
    const double w  = mod360(el.w   + el.wrate  * T);
    const double L  = mod360(el.L   + el.Lrate  * T);

    const double M = mod360(L - w);
    const double E = solveKepler(M, e);

    const double xp = a * (std::cos(E) - e);
    const double yp = a * std::sqrt(1.0 - e * e) * std::sin(E);

    const double cosOm = cos_d(Om), sinOm = sin_d(Om);
    const double cosw  = cos_d(w),  sinw  = sin_d(w);
    const double cosi  = cos_d(i),  sini  = sin_d(i);

    const double Px =  cosOm * cosw - sinOm * sinw * cosi;
    const double Py =  sinOm * cosw + cosOm * sinw * cosi;
    const double Pz =  sinw  * sini;
    const double Qx = -cosOm * sinw - sinOm * cosw * cosi;
    const double Qy = -sinOm * sinw + cosOm * cosw * cosi;
    const double Qz =  cosw  * sini;

    return { Px * xp + Qx * yp,
             Py * xp + Qy * yp,
             Pz * xp + Qz * yp };
}

static double xyzToEclLon(const Vec3& v)
{
    return mod360(atan2_d(v.y, v.x));
}

/* ── Luna ELP2000 — 30 termini principali (Meeus cap.47) ──── */
static double moonLongitude(double T)
{
    const double D  = mod360(297.85036 + 445267.111480 * T - 0.0019142 * T*T + T*T*T/189474.0);
    const double M  = mod360(357.52772 +  35999.050340 * T - 0.0001603 * T*T - T*T*T/300000.0);
    const double Mp = mod360(134.96298 + 477198.867398 * T + 0.0086972 * T*T + T*T*T/ 56250.0);
    const double F  = mod360( 93.27191 + 483202.017538 * T - 0.0036825 * T*T + T*T*T/327270.0);

    struct LTerm { int D, M, Mp, F; double coef; };
    static const LTerm kL[] = {
        { 0, 0, 1, 0,  6288774.0}, { 2, 0,-1, 0,  1274027.0},
        { 2, 0, 0, 0,   658314.0}, { 0, 0, 2, 0,   213618.0},
        { 0, 1, 0, 0,  -185116.0}, { 0, 0, 0, 2,  -114332.0},
        { 2, 0,-2, 0,    58793.0}, { 2,-1,-1, 0,    57066.0},
        { 2, 0, 1, 0,    53322.0}, { 2,-1, 0, 0,    45758.0},
        { 0, 1,-1, 0,   -40923.0}, { 1, 0, 0, 0,   -34720.0},
        { 0, 1, 1, 0,   -30383.0}, { 2, 0, 0,-2,    15327.0},
        { 0, 0, 1,-2,    10980.0}, { 4, 0,-1, 0,    10675.0},
        { 0, 0, 3, 0,    10034.0}, { 4, 0,-2, 0,     8548.0},
        { 2, 1,-1, 0,    -7888.0}, { 2, 1, 0, 0,    -6766.0},
        { 1, 0,-1, 0,    -5163.0}, { 1, 1, 0, 0,     4987.0},
        { 2,-1, 1, 0,     4036.0}, { 2, 0, 2, 0,     3994.0},
        { 4, 0, 0, 0,     3861.0}, { 2, 0,-3, 0,     3665.0},
        { 0, 1,-2, 0,    -2689.0}, { 2,-1,-2, 0,     2390.0},
        { 0, 2, 0, 0,     2236.0}, { 2, 0,-1, 2,    -2602.0},
    };

    const double E = 1.0 - 0.002516 * T - 0.0000074 * T * T;
    double sumL = 0.0;
    for (auto& t : kL) {
        double arg = t.D * D + t.M * M + t.Mp * Mp + t.F * F;
        double c = t.coef;
        if (std::abs(t.M) == 1) c *= E;
        else if (std::abs(t.M) == 2) c *= E * E;
        sumL += c * sin_d(arg);
    }

    const double Lp = mod360(218.3164477 + 481267.88123421 * T);
    return mod360(Lp + sumL / 1000000.0);
}

/* ── Nodo Nord (Meeus p.144) ─────────────────────────────── */
static double northNodeLon(double T)
{
    return mod360(125.04452 - 1934.136261 * T
                 + 0.0020708 * T * T + T * T * T / 450000.0);
}

/* ── ASC e MC (Meeus cap.15) ─────────────────────────────── */
static double calcMC(double RAMC, double eps)
{
    double mc = atan2_d(sin_d(RAMC), cos_d(RAMC) * cos_d(eps));
    if (mc < 0) mc += 360.0;
    double diff = mod360(RAMC - mc);
    if (diff > 90.0 && diff < 270.0)
        mc = mod360(mc + 180.0);
    return mc;
}

/* Applica correzione di quadrante: cos(RAMC)<=0 → ASC∈[180°,360°), altrimenti [0°,180°). */
static double ascQuadrant(double asc, double RAMC)
{
    if ((cos_d(RAMC) <= 0.0) != (asc >= 180.0))
        asc = mod360(asc + 180.0);
    return asc;
}

static double calcASC(double RAMC, double eps, double lat)
{
    const double num = -cos_d(RAMC);
    const double den =  sin_d(eps) * tan_d(lat) + cos_d(eps) * sin_d(RAMC);
    double asc = atan2_d(num, den);
    if (asc < 0) asc += 360.0;
    /* Correzione quadrante: cos(RAMC)<=0 → ASC∈[180°,360°), cos(RAMC)>0 → ASC∈[0°,180°).
       Verificato su 10 carte (Astrodienst + Astro-Seek). */
    asc = ascQuadrant(asc, RAMC);

    /* Zona singolare (|den| < 0.02): atan2 perde precisione quando il denominatore
       si annulla. Avviene quando RAMC ≈ arcsin(-sin(ε)·tan(φ)/cos(ε)).
       Esempio: lat=45.5°, eps=23.4° → singolare a RAMC≈205.7° (vicino a Celentano).
       Rifiniamo con iterazione di Newton sull'equazione dell'orizzonte
         f(λ) = sin(φ)·sin(δ(λ)) + cos(φ)·cos(δ(λ))·cos(RAMC − α(λ)) = 0
       dove δ = arcsin(sin(ε)·sin(λ)) e α = atan2(sin(λ)·cos(ε), cos(λ)). */
    if (std::abs(den) < 0.02) {
        double lam = asc;
        for (int i = 0; i < 20; ++i) {
            const double sinDec  = sin_d(eps) * sin_d(lam);
            const double cosDec  = std::sqrt(std::max(0.0, 1.0 - sinDec * sinDec));
            const double ra      = mod360(atan2_d(sin_d(lam) * cos_d(eps), cos_d(lam)));
            const double H       = mod360(RAMC - ra);
            const double f       = sin_d(lat)*sinDec + cos_d(lat)*cosDec*cos_d(H);
            if (std::abs(f) < 1e-9) break;
            // derivata numerica con passo 0.001°
            const double dL       = 0.001;
            const double sinDec2  = sin_d(eps) * sin_d(lam + dL);
            const double cosDec2  = std::sqrt(std::max(0.0, 1.0 - sinDec2 * sinDec2));
            const double ra2      = mod360(atan2_d(sin_d(lam+dL)*cos_d(eps), cos_d(lam+dL)));
            const double H2       = mod360(RAMC - ra2);
            const double f2       = sin_d(lat)*sinDec2 + cos_d(lat)*cosDec2*cos_d(H2);
            const double df       = (f2 - f) / dL;
            if (std::abs(df) < 1e-12) break;
            lam = mod360(lam - f / df);
        }
        asc = ascQuadrant(lam, RAMC);
    }
    return asc;
}

/* ── Case: Placidus approssimato (ASC/MC reali, intermedie per tricotomia) */
static void placidusHouses(double RAMC, double eps, double lat, double* cusps)
{
    (void)acos_d; (void)asin_d; /* usate solo se si implementa Placidus iterativo */

    const double asc = calcASC(RAMC, eps, lat);
    const double mc  = calcMC(RAMC, eps);
    const double ic  = mod360(mc  + 180.0);
    const double dc  = mod360(asc + 180.0);

    auto interp = [](double a, double b, double frac) -> double {
        double span = mod360(b - a);
        return mod360(a + span * frac);
    };

    cusps[0]  = asc;
    cusps[1]  = interp(asc, dc,  1.0/3.0);
    cusps[2]  = interp(asc, dc,  2.0/3.0);
    cusps[3]  = ic;
    cusps[4]  = interp(ic,  asc, 1.0/3.0);
    cusps[5]  = interp(ic,  asc, 2.0/3.0);
    cusps[6]  = dc;
    cusps[7]  = interp(dc,  mc,  1.0/3.0);
    cusps[8]  = interp(dc,  mc,  2.0/3.0);
    cusps[9]  = mc;
    cusps[10] = interp(mc,  asc, 1.0/3.0);
    cusps[11] = interp(mc,  asc, 2.0/3.0);
}

/* ══════════════════════════════════════════════════════════════ */

AstroCalc::Result AstroCalc::compute(int year, int month, int day,
                                     int hour, int minute,
                                     double latDeg, double lonDeg)
{
    Result res;

    const double ut  = hour + minute / 60.0;
    const double JD  = julianDay(year, month, day, ut);
    const double T   = (JD - 2451545.0) / 36525.0;
    const double eps = obliquity(T);

    const double LMST_deg = mod360(gmst(JD) + lonDeg);

    const Vec3 earthXYZ = planetXYZ(kPlanets[2], T);
    const Vec3 sunGeo   = { -earthXYZ.x, -earthXYZ.y, -earthXYZ.z };
    const double sunLon = xyzToEclLon(sunGeo);

    auto geoLon = [&](int idx) -> double {
        Vec3 pXYZ = planetXYZ(kPlanets[idx], T);
        Vec3 geo  = { pXYZ.x - earthXYZ.x,
                      pXYZ.y - earthXYZ.y,
                      pXYZ.z - earthXYZ.z };
        return xyzToEclLon(geo);
    };

    res.ascLon = calcASC(LMST_deg, eps, latDeg);
    res.mcLon  = calcMC(LMST_deg, eps);
    placidusHouses(LMST_deg, eps, latDeg, res.cusps);

    struct PM { const char* name; const char* sym; double lon; QColor col; };
    const PM pms[] = {
        { "SOLE",     "\xe2\x98\x89", sunLon,           QColor(220,140,  0) },
        { "LUNA",     "\xe2\x98\xbd", moonLongitude(T), QColor( 80,120,200) },
        { "MERCURIO", "\xe2\x98\xbf", geoLon(0),        QColor(180,130, 40) },
        { "VENERE",   "\xe2\x99\x80", geoLon(1),        QColor( 40,160, 80) },
        { "MARTE",    "\xe2\x99\x82", geoLon(3),        QColor(200, 40, 40) },
        { "GIOVE",    "\xe2\x99\x83", geoLon(4),        QColor(200,120, 30) },
        { "SATURNO",  "\xe2\x99\x84", geoLon(5),        QColor( 60, 60,160) },
        { "URANO",    "\xe2\x9b\xa2", geoLon(6),        QColor( 20,170,180) },
        { "NETTUNO",  "\xe2\x99\x86", geoLon(7),        QColor( 20, 80,180) },
        { "PLUTONE",  "\xe2\x99\x87", geoLon(8),        QColor(140, 30, 80) },
        { "NODO",     "\xe2\x98\x8a", northNodeLon(T),  QColor(120, 80,140) },
    };

    for (auto& pm : pms) {
        NatalChartWidget::Planet pl;
        pl.name   = QString::fromUtf8(pm.name);
        pl.symbol = QString::fromUtf8(pm.sym);
        pl.lon    = pm.lon;
        pl.color  = pm.col;
        res.planets << pl;
    }

    res.ok = true;
    return res;
}
