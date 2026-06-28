#include "astro_calc.h"
#include "vsop87b_data.h"
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

/* ── Elementi orbitali JPL J2000.0 — solo Plutone (non coperto da VSOP87B) ─ */
struct OrbEl {
    double a;
    double e, erate;
    double i, irate;
    double Om, Omrate;
    double w, wrate;
    double L, Lrate;
};

static const OrbEl kOrbElPluto = {
    39.48211675, 0.24882730, 0.00005170, 17.14001206, 0.00004170,
   110.30393684,-0.01183482,224.06891629,-0.04062942, 238.92903833, 145.20780515
};

struct Vec3 { double x, y, z; };

/* ── VSOP87B (Bretagnon & Francou 1988) — precisione < 0.02° ──── */
static double vsop87b_eval(const VsopSeries (&s)[6], double tau)
{
    double total = 0.0, tau_k = 1.0;
    for (int k = 0; k < 6; ++k, tau_k *= tau) {
        if (!s[k].terms) continue;
        double sum = 0.0;
        for (int i = 0; i < s[k].n; ++i)
            sum += s[k].terms[i].amp * std::cos(s[k].terms[i].phi + s[k].terms[i].freq * tau);
        total += sum * tau_k;
    }
    return total;
}

static Vec3 vsop87b_xyz(const VsopPlanet& p, double tau)
{
    const double L = vsop87b_eval(p.series[0], tau);
    const double B = vsop87b_eval(p.series[1], tau);
    const double R = vsop87b_eval(p.series[2], tau);
    return { R * std::cos(B) * std::cos(L),
             R * std::cos(B) * std::sin(L),
             R * std::sin(B) };
}

/* Precessione longitudine eclittica J2000→data (Meeus cap.21), T in secoli */
static double precessionLon(double T)
{
    return (5029.0966 * T + 1.5623 * T * T) / 3600.0;
}

/* ── Elementi orbitali (solo per Plutone, non coperto da VSOP87B) ──────── */
static Vec3 planetXYZ(const OrbEl& el, double T)
{
    const double a      = el.a;
    const double e      = el.e   + el.erate  * T;
    const double i      = el.i   + el.irate  * T;
    const double Om     = mod360(el.Om + el.Omrate * T);
    const double wtilde = mod360(el.w  + el.wrate  * T); /* longitudine del perielio ω̃ */
    const double w      = mod360(wtilde - Om);            /* argomento del perielio ω = ω̃−Ω */
    const double L      = mod360(el.L  + el.Lrate  * T);

    const double M = mod360(L - wtilde); /* anomalia media M = L − ω̃ */
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
    const double tau = T / 10.0;   /* VSOP87B usa millenni giuliani */
    const double eps = obliquity(T);

    /* VSOP87B e gli elementi JPL sono riferiti all'eclittica fissa J2000.
       Per ottenere longitudine eclittica tropicale (equinozio della data),
       si aggiunge la precessione da J2000 alla data (~50.3"/anno). */
    const double prec = precessionLon(T);

    const double LMST_deg = mod360(gmst(JD) + lonDeg);

    const Vec3 earthXYZ = vsop87b_xyz(kVsopEar, tau);
    const Vec3 sunGeo   = { -earthXYZ.x, -earthXYZ.y, -earthXYZ.z };
    const double sunLon = mod360(xyzToEclLon(sunGeo) + prec);

    /* Tabella VSOP87B per indice pianeta (0=Mer,1=Ven,3=Mar,4=Jup,5=Sat,6=Ura,7=Nep).
       Indice 2 (Terra) non usato qui; indice 8 (Plutone) gestito sotto. */
    static const VsopPlanet* const kVsopByIdx[] = {
        &kVsopMer, &kVsopVen, nullptr, &kVsopMar,
        &kVsopJup, &kVsopSat, &kVsopUra, &kVsopNep
    };

    auto geoLon = [&](int idx) -> double {
        Vec3 pXYZ = (idx < 8) ? vsop87b_xyz(*kVsopByIdx[idx], tau)
                               : planetXYZ(kOrbElPluto, T);
        Vec3 geo  = { pXYZ.x - earthXYZ.x,
                      pXYZ.y - earthXYZ.y,
                      pXYZ.z - earthXYZ.z };
        return mod360(xyzToEclLon(geo) + prec);
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
