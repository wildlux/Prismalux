#pragma once
/*
 * grafico_canvas_p.h — Helpers privati di GraficoCanvas
 * ======================================================
 * Inline helpers condivisi tra tutti i translation unit di GraficoCanvas.
 * NON va incluso dai consumatori esterni: solo dai grafico_canvas_*.cpp.
 */
#include "pages/grafico_page.h"

#include <QColor>
#include <QRectF>
#include <QPointF>
#include <QPainter>
#include <cmath>

/* ── Palette colori globale ──────────────────────────────────────── */
static const QColor kPal[] = {
    {0x00,0xbf,0xd8}, {0xff,0x79,0x5a}, {0x73,0xe2,0x73},
    {0xff,0xd7,0x6e}, {0xb0,0x90,0xff}, {0xff,0x82,0xc2},
    {0x5a,0xd7,0xff}, {0xff,0xa0,0x50}
};

/* ── Area di disegno (margini fissi) ─────────────────────────────── */
inline QRectF plotArea(const QWidget* w) {
    return QRectF(60, 18, w->width() - 80, w->height() - 52);
}

/* ── Origine gizmo assi in base alla posizione scelta ────────────── */
inline QPointF gizmoOrigin(int pos, const QRectF& a) {
    constexpr double M = 52;  /* margine dal bordo */
    switch (pos) {
        case GraficoCanvas::BottomLeft:  return { a.left()  + M, a.bottom() - M };
        case GraficoCanvas::TopLeft:     return { a.left()  + M, a.top()    + M };
        case GraficoCanvas::TopRight:    return { a.right() - M, a.top()    + M };
        default: /* BottomRight */       return { a.right() - M, a.bottom() - M };
    }
}
