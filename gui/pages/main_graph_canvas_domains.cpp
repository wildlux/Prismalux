/*
 * main_graph_canvas_domains.cpp — GraficoCanvas: domini specialistici
 * ========================================================================
 * MathConst (pi/e/primi su diagramma di Smith) e Smith Prime (RF).
 * Split da main_graph_canvas.cpp (TODO D-8).
 */
#include "main_graph.h"
#include "main_graph_canvas_p.h"

#include <QPainter>
#include <QPainterPath>
#include <QFont>
#include <cmath>
#include <algorithm>

/* ══════════════════════════════════════════════════════════════
   Math Const — rendering
   ══════════════════════════════════════════════════════════════ */
void GraficoCanvas::drawMathConst(QPainter& p, const QRectF& area) {
    p.setRenderHint(QPainter::Antialiasing);

    const double cx = area.center().x() + m_panNC.x();
    const double cy = area.center().y() + m_panNC.y();
    const double R  = std::min(area.width(), area.height()) * 0.44 * m_zoomNC;

    auto toSc = [&](double re, double im) -> QPointF {
        return QPointF(cx + re * R, cy - im * R);
    };

    /* ── Sfondo disco ── */
    p.setBrush(QColor(0x0d, 0x1a, 0x26));
    p.setPen(Qt::NoPen);
    p.drawEllipse(QPointF(cx, cy), R, R);

    /* ── Griglia Smith (clip al disco) ── */
    {
        QPainterPath clip;
        clip.addEllipse(QPointF(cx, cy), R + 1.5, R + 1.5);
        p.setClipPath(clip);

        QPen gridPen(QColor(0x1a, 0x42, 0x58, 160), 0.7);
        p.setPen(gridPen);
        p.setBrush(Qt::NoBrush);

        static const double kR[] = {0.2, 0.5, 1.0, 2.0, 5.0};
        for (double r : kR) {
            double cn = r / (r + 1.0), rn = 1.0 / (r + 1.0);
            p.drawEllipse(QPointF(cx + cn * R, cy), rn * R, rn * R);
        }
        static const double kX[] = {0.5, 1.0, 2.0, 5.0};
        for (double x : kX) {
            for (int sg : {1, -1}) {
                double xs = x * sg, cyG = 1.0 / xs, rxG = std::abs(cyG);
                p.drawEllipse(QPointF(cx + R, cy - cyG * R), rxG * R, rxG * R);
            }
        }
        p.setPen(QPen(QColor(0x22, 0x66, 0x55, 160), 0.7));
        p.drawLine(toSc(-1.0, 0.0), toSc(1.0, 0.0));
        p.setClipping(false);
    }

    /* ── Cerchio unitario ── */
    p.setPen(QPen(QColor(0x33, 0x99, 0xcc), 1.5));
    p.setBrush(Qt::NoBrush);
    p.drawEllipse(QPointF(cx, cy), R, R);

    /* ── Titolo ── */
    p.setPen(QColor(0xbb, 0xdd, 0xff, 190));
    p.setFont(QFont("Inter,Ubuntu,sans-serif", 9, QFont::Bold));
    p.drawText(QRectF(area.left(), area.top(), area.width(), 22),
               Qt::AlignHCenter | Qt::AlignVCenter,
               QString::fromUtf8("Diagramma di Smith \xe2\x80\x94 \xcf\x80 \xc2\xb7 e \xc2\xb7 Primi"));

    if (m_mathPts.isEmpty()) {
        p.setPen(QColor(0x66, 0x88, 0xaa));
        p.setFont(QFont("Inter,Ubuntu,sans-serif", 10));
        p.drawText(QRectF(cx - 140, cy - 14, 280, 28),
                   Qt::AlignCenter,
                   "Clicca \xe2\x80\x9cTraccia\xe2\x80\x9d per generare");
        return;
    }

    /* ── Linee verticali target Γ(π) e Γ(e) ── */
    const double kGammaPi = (M_PI - 1.0) / (M_PI + 1.0);   /* ≈ 0.5168 */
    const double kGammaE  = (M_E  - 1.0) / (M_E  + 1.0);   /* ≈ 0.4621 */

    p.setPen(QPen(QColor(0xff, 0xa5, 0x00, 90), 1.0, Qt::DashLine));
    p.drawLine(toSc(kGammaPi, -0.22), toSc(kGammaPi,  0.22));
    p.setPen(QPen(QColor(0x40, 0xe0, 0x70, 90), 1.0, Qt::DashLine));
    p.drawLine(toSc(kGammaE,  -0.22), toSc(kGammaE,   0.22));

    /* Etichette target */
    p.setFont(QFont("Inter,Ubuntu,sans-serif", 8, QFont::Bold));
    p.setPen(QColor(0xff, 0xa5, 0x00, 200));
    p.drawText(toSc(kGammaPi + 0.02,  0.24), QString::fromUtf8("\xcf\x80"));
    p.setPen(QColor(0x40, 0xe0, 0x70, 200));
    p.drawText(toSc(kGammaE  + 0.02, -0.26), "e");

    /* ── Separa per serie ── */
    QVector<MathPt> piPts, ePts, primePts;
    for (const MathPt& pt : m_mathPts) {
        if      (pt.seriesId == 0) piPts    << pt;
        else if (pt.seriesId == 1) ePts     << pt;
        else                       primePts << pt;
    }

    /* Offset verticale fisso (coordinate Γ) per separare visivamente le tre serie */
    const double yPi    = +0.10;   /* π sopra l'asse reale */
    const double yE     = -0.10;   /* e sotto l'asse reale */

    /* ── Serie π (arancio) ── */
    if (!piPts.isEmpty()) {
        const QColor col(0xff, 0xa5, 0x00, 220);
        /* Linea di convergenza */
        p.setPen(QPen(col, 1.2));
        p.setBrush(Qt::NoBrush);
        QPainterPath path;
        path.moveTo(toSc(piPts[0].re, yPi));
        for (int i = 1; i < piPts.size(); ++i) path.lineTo(toSc(piPts[i].re, yPi));
        p.drawPath(path);
        /* Punti */
        p.setPen(Qt::NoPen);
        p.setBrush(col);
        for (int i = 0; i < piPts.size(); ++i) {
            double r = (i == piPts.size() - 1) ? 4.5 : 2.4;
            p.drawEllipse(toSc(piPts[i].re, yPi), r, r);
        }
        /* Freccia tratteggiata verso target */
        p.setPen(QPen(col.darker(150), 0.8, Qt::DotLine));
        p.drawLine(toSc(piPts.back().re, yPi), toSc(kGammaPi, yPi));
    }

    /* ── Serie e (verde smeraldo) ── */
    if (!ePts.isEmpty()) {
        const QColor col(0x40, 0xe0, 0x70, 220);
        p.setPen(QPen(col, 1.2));
        p.setBrush(Qt::NoBrush);
        QPainterPath path;
        path.moveTo(toSc(ePts[0].re, yE));
        for (int i = 1; i < ePts.size(); ++i) path.lineTo(toSc(ePts[i].re, yE));
        p.drawPath(path);
        p.setPen(Qt::NoPen);
        p.setBrush(col);
        for (int i = 0; i < ePts.size(); ++i) {
            double r = (i == ePts.size() - 1) ? 4.5 : 2.4;
            p.drawEllipse(toSc(ePts[i].re, yE), r, r);
        }
        p.setPen(QPen(col.darker(150), 0.8, Qt::DotLine));
        p.drawLine(toSc(ePts.back().re, yE), toSc(kGammaE, yE));
    }

    /* ── Primi (rosso, sull'asse reale) ── */
    if (!primePts.isEmpty()) {
        const QColor col(0xff, 0x55, 0x44, 220);
        p.setPen(Qt::NoPen);
        p.setBrush(col);
        for (const MathPt& pt : primePts)
            p.drawEllipse(toSc(pt.re, 0.0), 2.8, 2.8);
    }

    /* ── Punti notevoli ── */
    p.setFont(QFont("Inter,Ubuntu,sans-serif", 7, QFont::Bold));
    p.setPen(QColor(0x77, 0xbb, 0xdd));
    p.drawText(toSc(-1.02, 0.04), "SC");
    p.drawText(toSc( 0.86, 0.04), "OC");
    p.drawText(toSc( 0.02, 0.06), "Z\xe2\x82\x80");

    /* ── Legenda ── */
    {
        double lx = area.right() - 168;
        double ly = area.top() + 28;
        p.setFont(QFont("Inter,Ubuntu,sans-serif", 8));

        auto drawItem = [&](QColor col, const QString& text) {
            p.setBrush(col); p.setPen(Qt::NoPen);
            p.drawEllipse(QPointF(lx + 4, ly + 4), 3.5, 3.5);
            p.setPen(QColor(0xcc, 0xcc, 0xcc));
            p.drawText(QPointF(lx + 14, ly + 8), text);
            ly += 17;
        };

        if (!piPts.isEmpty())
            drawItem(QColor(0xff,0xa5,0x00),
                     QString::fromUtf8("\xcf\x80 Leibniz (%1 termini)").arg(piPts.size()));
        if (!ePts.isEmpty())
            drawItem(QColor(0x40,0xe0,0x70),
                     QString("e Taylor (%1 termini)").arg(ePts.size()));
        if (!primePts.isEmpty())
            drawItem(QColor(0xff,0x55,0x44),
                     QString("Primi (%1)").arg(primePts.size()));
    }

    /* ── Hint ── */
    p.setPen(QColor(0x44, 0x55, 0x55));
    p.setFont(QFont("Inter,Ubuntu,sans-serif", 8));
    p.drawText(QRectF(area.left(), area.bottom() - 16, area.width(), 14),
               Qt::AlignRight,
               "\xce\x93 = (z\xe2\x88\x92" "1)/(z+1)  \xe2\x80\x94  Rotella per zoom \xc2\xb7 Trascina");
}

/* ══════════════════════════════════════════════════════════════
   Smith Prime — rendering
   ══════════════════════════════════════════════════════════════ */
void GraficoCanvas::drawSmithPrime(QPainter& p, const QRectF& area) {
    p.setRenderHint(QPainter::Antialiasing);

    /* ── Centro e raggio del disco unitario (con zoom/pan) ── */
    const double cx = area.center().x() + m_panNC.x();
    const double cy = area.center().y() + m_panNC.y();
    const double R  = std::min(area.width(), area.height()) * 0.44 * m_zoomNC;

    /* Conversione da coordinate Γ (re,im) a schermo */
    auto toSc = [&](double re, double im) -> QPointF {
        return QPointF(cx + re * R, cy - im * R);
    };

    /* ── Sfondo del disco ── */
    p.setBrush(QColor(0x0d, 0x1a, 0x26));
    p.setPen(Qt::NoPen);
    p.drawEllipse(QPointF(cx, cy), R, R);

    /* ── Griglia Smith (clip al disco) ── */
    {
        QPainterPath clip;
        clip.addEllipse(QPointF(cx, cy), R + 1.5, R + 1.5);
        p.setClipPath(clip);

        QPen gridPen(QColor(0x1a, 0x42, 0x58, 160), 0.7);
        p.setPen(gridPen);
        p.setBrush(Qt::NoBrush);

        /* Cerchi a resistenza normalizzata costante r:
         * centro Γ = (r/(r+1), 0), raggio = 1/(r+1) */
        static const double kR[] = {0.2, 0.5, 1.0, 2.0, 5.0};
        for (double r : kR) {
            double cn = r / (r + 1.0);
            double rn = 1.0 / (r + 1.0);
            p.drawEllipse(QPointF(cx + cn * R, cy), rn * R, rn * R);
        }

        /* Archi a reattanza normalizzata costante x:
         * centro Γ = (1, 1/x), raggio = 1/|x|  — clippati al disco */
        static const double kX[] = {0.5, 1.0, 2.0, 5.0};
        for (double x : kX) {
            for (int sg : {1, -1}) {
                double xs   = x * sg;
                double cyG  = 1.0 / xs;
                double rxG  = std::abs(cyG);
                double scx  = cx + 1.0 * R;
                double scy  = cy - cyG * R;
                p.drawEllipse(QPointF(scx, scy), rxG * R, rxG * R);
            }
        }

        /* Asse reale (x = 0) */
        p.setPen(QPen(QColor(0x22, 0x66, 0x55, 160), 0.7));
        p.drawLine(toSc(-1.0, 0.0), toSc(1.0, 0.0));

        p.setClipping(false);
    }

    /* ── Cerchio unitario (bordo) ── */
    p.setPen(QPen(QColor(0x33, 0x99, 0xcc), 1.5));
    p.setBrush(Qt::NoBrush);
    p.drawEllipse(QPointF(cx, cy), R, R);

    /* ── Etichette resistenza ── */
    p.setFont(QFont("Inter,Ubuntu,sans-serif", 7));
    p.setPen(QColor(0x44, 0x88, 0xaa, 190));
    {
        static const double kR[] = {0.2, 0.5, 1.0, 2.0, 5.0};
        static const char*  kL[] = {"0.2", "0.5", "1", "2", "5"};
        for (int i = 0; i < 5; i++) {
            double cn = kR[i] / (kR[i] + 1.0);
            double rn = 1.0 / (kR[i] + 1.0);
            /* Etichetta sul punto dell'asse reale più a sinistra del cerchio */
            QPointF lp = toSc(cn - rn + 0.01, 0.06);
            p.drawText(lp, kL[i]);
        }
    }
    /* Etichetta r=0 vicino al bordo sinistro */
    p.drawText(toSc(-0.97, 0.05), "0");

    /* ── Punti: nessun dato disponibile ── */
    if (m_smithPts.isEmpty()) {
        p.setPen(QColor(0x66, 0x88, 0xaa));
        p.setFont(QFont("Inter,Ubuntu,sans-serif", 10));
        p.drawText(QRectF(cx - 140, cy - 14, 280, 28),
                   Qt::AlignCenter,
                   "Clicca \xe2\x80\x9cTraccia\xe2\x80\x9d per generare i punti");
        /* Titolo e return */
        p.setPen(QColor(0xaa, 0xcc, 0xee, 160));
        p.setFont(QFont("Inter,Ubuntu,sans-serif", 9, QFont::Bold));
        p.drawText(QRectF(area.left(), area.top(), area.width(), 24),
                   Qt::AlignHCenter | Qt::AlignVCenter,
                   "Diagramma di Smith \xe2\x80\x94 Numeri Primi");
        return;
    }

    /* ── Colore per seriesId ── */
    auto seriesColor = [](int id, bool gaussBorder) -> QColor {
        if (id == 4) return gaussBorder
                         ? QColor(0xff, 0xdd, 0x33, 230)   /* gaussiano bordo — giallo */
                         : QColor(0x44, 0x88, 0xff, 200);  /* gaussiano complesso — blu */
        if (id == 5) return QColor(0xff, 0x44, 0xdd, 230);  /* personalizzato — magenta */
        static const QColor kC[] = {
            QColor(0xff, 0x66, 0x44, 230),   /* 0 = primi  — rosso-arancio */
            QColor(0xff, 0xdd, 0x00, 220),   /* 1 = Fib    — giallo */
            QColor(0x00, 0xe0, 0xcc, 220),   /* 2 = tri    — ciano */
            QColor(0x44, 0xdd, 0x44, 220),   /* 3 = sqr    — verde */
        };
        return (id >= 0 && id < 4) ? kC[id] : QColor(0xaa, 0xaa, 0xaa);
    };

    /* ── Raggruppa punti reali per valore intero (collision detection) ── */
    /* I gaussiani vengono tenuti separati (hanno coordinate im≠0) */
    const double limSq = m_smithExpanded ? 4.0 : 1.04;

    /* offset verticale (in coord Γ) per ogni seriesId (0..5) */
    static const double kYOff[] = { 0.0, 0.06, 0.12, -0.06, 0.0, 0.18 };

    /* Mappa value → lista (seriesId, re, im_base) per serie reali */
    QMap<int, QVector<QPair<int,double>>> groups;  /* value → [(sid, re)] */
    for (const SmithPt& pt : m_smithPts) {
        if (pt.gaussian) continue;
        double mod2 = pt.re * pt.re + pt.im * pt.im;
        if (mod2 > limSq) continue;
        groups[pt.value].append({pt.seriesId, pt.re});
    }

    /* ── Disegna linee di collisione (dashed) ── */
    {
        QPen dashPen(QColor(0x88, 0x88, 0x88, 130), 0.8, Qt::DashLine);
        p.setPen(dashPen);
        p.setBrush(Qt::NoBrush);
        for (auto it = groups.cbegin(); it != groups.cend(); ++it) {
            const auto& items = it.value();
            if (items.size() < 2) continue;
            double re = items[0].second;
            /* y range: da min offset a max offset */
            double yMin = 1e9, yMax = -1e9;
            for (auto& [sid, r2] : items) {
                double yo = (sid >= 0 && sid < 6) ? kYOff[sid] : 0.0;
                yMin = std::min(yMin, yo);
                yMax = std::max(yMax, yo);
            }
            if (yMax > yMin)
                p.drawLine(toSc(re, yMin), toSc(re, yMax));
        }
    }

    /* ── Disegna punti reali (con offset per collisioni) ── */
    p.setPen(Qt::NoPen);
    for (auto it = groups.cbegin(); it != groups.cend(); ++it) {
        for (auto& [sid, re] : it.value()) {
            double yo = (sid >= 0 && sid < 6) ? kYOff[sid] : 0.0;
            QPointF sp = toSc(re, yo);
            QColor col = seriesColor(sid, false);
            p.setBrush(col);
            p.drawEllipse(sp, 2.5, 2.5);
        }
    }

    /* ── Disegna punti gaussiani ── */
    p.setPen(Qt::NoPen);
    for (const SmithPt& pt : m_smithPts) {
        if (!pt.gaussian) continue;
        double mod2 = pt.re * pt.re + pt.im * pt.im;
        if (mod2 > limSq) continue;
        bool border = (pt.ga == 0);
        QColor col = seriesColor(4, border);
        p.setBrush(col);
        p.drawEllipse(toSc(pt.re, pt.im), border ? 3.0 : 2.8, border ? 3.0 : 2.8);
    }

    /* ── Etichette numeriche sui gruppi (se abilitate) ── */
    if (m_smithShowLabels) {
        p.setFont(QFont("Inter,Ubuntu,sans-serif", 6));
        for (auto it = groups.cbegin(); it != groups.cend(); ++it) {
            int val = it.key();
            if (val > 9999) continue;  /* evita sovraffollamento */
            const auto& items = it.value();
            double re = items[0].second;
            /* posiziona l'etichetta sopra il gruppo */
            double yTop = -1e9;
            for (auto& [sid, r2] : items) {
                double yo = (sid >= 0 && sid < 6) ? kYOff[sid] : 0.0;
                yTop = std::max(yTop, yo);
            }
            QPointF lp = toSc(re, yTop + 0.04);
            p.setPen(QColor(0xee, 0xee, 0xee, 200));
            p.drawText(lp, QString::number(val));
        }
    }

    /* ── Punti notevoli ── */
    p.setFont(QFont("Inter,Ubuntu,sans-serif", 7, QFont::Bold));
    p.setPen(QColor(0x77, 0xbb, 0xdd));
    p.drawText(toSc(-0.97, 0.05), "SC");
    p.drawText(toSc( 0.92, 0.05), "OC");
    p.drawText(toSc(-0.02, 0.06), "Z\xe2\x82\x80");

    /* ── Titolo ── */
    p.setPen(QColor(0xbb, 0xdd, 0xff, 190));
    p.setFont(QFont("Inter,Ubuntu,sans-serif", 9, QFont::Bold));
    p.drawText(QRectF(area.left(), area.top(), area.width(), 22),
               Qt::AlignHCenter | Qt::AlignVCenter,
               "Diagramma di Smith \xe2\x80\x94 Numeri Primi");

    /* ── Legenda ── */
    {
        /* Conta punti per serie */
        int nSeries[6] = {0, 0, 0, 0, 0, 0};
        for (const SmithPt& sp : m_smithPts) {
            if (sp.gaussian) { ++nSeries[4]; continue; }
            int sid = sp.seriesId;
            if (sid >= 0 && sid < 6) ++nSeries[sid];
        }

        static const char* kNames[] = {
            "Primi reali", "Fibonacci", "Triangolari",
            "Quadrati perfetti", "Primi gaussiani", "Personalizzata"
        };

        double lx = area.right() - 162;
        double ly = area.top() + 28;
        p.setFont(QFont("Inter,Ubuntu,sans-serif", 8));

        for (int sid = 0; sid < 6; ++sid) {
            if (nSeries[sid] == 0) continue;
            QColor col = seriesColor(sid, sid == 4);
            p.setBrush(col);
            p.setPen(Qt::NoPen);
            p.drawEllipse(QPointF(lx + 4, ly + 4), 3.0, 3.0);
            p.setPen(QColor(0xcc, 0xcc, 0xcc));
            p.drawText(QPointF(lx + 12, ly + 8),
                       QString("%1 (%2)").arg(kNames[sid]).arg(nSeries[sid]));
            ly += 16;
        }
    }

    /* ── Hint in basso ── */
    p.setPen(QColor(0x44, 0x55, 0x55));
    p.setFont(QFont("Inter,Ubuntu,sans-serif", 8));
    p.drawText(QRectF(area.left(), area.bottom() - 16, area.width(), 14),
               Qt::AlignRight,
               "\xce\x93 = (z\xe2\x88\x92""1)/(z+1)  \xe2\x80\x94  Rotella per zoom \xc2\xb7 Trascina per spostare");
}
