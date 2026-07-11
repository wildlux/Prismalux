#pragma once
/* ══════════════════════════════════════════════════════════════
   ChatLogBrowser — QTextBrowser con bolle dagli angoli arrotondati.

   PERCHÉ ESISTE: il motore rich-text di Qt (QTextDocument) NON
   implementa la proprietà CSS `border-radius` — verificato
   empiricamente su Qt 6.10.2: una cella con `border-radius:40px`
   viene comunque disegnata quadrata. Il parametro "arrotondamento
   bolle" in Impostazioni scriveva il valore nell'HTML ma il
   renderer lo ignorava, quindi le bolle restavano squadrate.

   COME FUNZIONA: dopo che la classe base ha disegnato le bolle
   (quadrate) + il testo, `paintEvent` ridisegna i 4 angoli di ogni
   bolla "tagliandoli" col colore di sfondo del log — l'effetto è un
   angolo arrotondato vero, col raggio scelto dall'utente.

   Riconoscimento bolle: si iterano SOLO le tabelle di primo livello
   (figlie del root frame) e, di queste, le celle con uno sfondo
   pieno (`cell.format().background()`). Le tabelle annidate (barre
   azioni) non sono figlie del root → ignorate. Nessuna modifica
   all'HTML delle bolle: il colore e la geometria si leggono dal
   documento già costruito.

   Q_OBJECT non serve: si ridefinisce solo paintEvent (dispatch
   virtuale), nessun nuovo segnale/slot.
   ══════════════════════════════════════════════════════════════ */

#include <QTextBrowser>
#include <QTextTable>
#include <QTextFrame>
#include <QTextBlock>
#include <QAbstractTextDocumentLayout>
#include <QScrollBar>
#include <QPainter>
#include <QPainterPath>
#include <QPaintEvent>

#include "../app_config.h"
#include "../prismalux_paths.h"

class ChatLogBrowser : public QTextBrowser {
public:
    explicit ChatLogBrowser(QWidget* parent = nullptr)
        : QTextBrowser(parent) {}

protected:
    void paintEvent(QPaintEvent* e) override
    {
        /* 1) La base disegna bolle quadrate + testo. */
        QTextBrowser::paintEvent(e);

        /* 2) Raggio scelto dall'utente (px). 0 = nessun arrotondamento. */
        const int radius =
            AppConfig::s().value(PrismaluxPaths::SK::kBubbleRadius, 10).toInt();
        if (radius <= 0)
            return;

        QTextDocument* doc = document();
        if (!doc) return;
        auto* lay = doc->documentLayout();
        QTextFrame* root = doc->rootFrame();
        if (!lay || !root) return;

        /* Colore di sfondo del log: col QSS applicato, la palette del
           viewport lo riflette (verificato). Con questo "tagliamo" gli
           angoli quadrati per ottenere quelli arrotondati. */
        const QColor pageBg =
            viewport()->palette().color(QPalette::Base);

        /* Doc-coords → viewport-coords: sottrai lo scroll. */
        const QPointF off(-horizontalScrollBar()->value(),
                          -verticalScrollBar()->value());
        const QRectF vpRect(viewport()->rect());

        QPainter p(viewport());
        p.setRenderHint(QPainter::Antialiasing, true);
        p.setPen(Qt::NoPen);
        p.setBrush(pageBg);

        for (QTextFrame* f : root->childFrames()) {
            auto* t = qobject_cast<QTextTable*>(f);
            if (!t) continue;

            for (int r = 0; r < t->rows(); ++r)
            for (int col = 0; col < t->columns(); ++col) {
                QTextTableCell cell = t->cellAt(r, col);
                if (!cell.isValid()) continue;
                const QBrush bg = cell.format().background();
                if (bg.style() == Qt::NoBrush || bg.color().alpha() == 0)
                    continue;

                /* Rettangolo del contenuto della cella (senza padding). */
                QRectF text;
                for (auto it = cell.begin(); !it.atEnd(); ++it) {
                    const QTextBlock b = it.currentBlock();
                    if (b.isValid())
                        text = text.united(lay->blockBoundingRect(b));
                }
                if (text.isEmpty()) continue;

                /* Espandi al riquadro colorato: padding bolla 10px v / 14px h.
                   (valori dei letterali HTML in main_ai_bubbles.cpp) */
                QRectF box = text.adjusted(-14, -10, 14, 10).translated(off);
                if (!box.intersects(vpRect)) continue;   /* fuori schermo */

                cutRoundedCorners(p, box, radius, pageBg);
            }
        }
    }

private:
    /* Taglia i 4 angoli del riquadro col colore di sfondo: la parte del
       quadratino d'angolo che sta FUORI dal quarto di cerchio viene
       coperta, lasciando l'angolo arrotondato. Non tocca i lati dritti,
       così il bordo originale della bolla resta del suo colore. */
    static void cutRoundedCorners(QPainter& p, const QRectF& box,
                                  int radius, const QColor& bg)
    {
        const qreal rad = qMin<qreal>(radius,
            qMin(box.width(), box.height()) / 2.0);
        if (rad <= 0.5) return;

        auto cut = [&](const QRectF& corner, const QPointF& center) {
            QPainterPath sq;   sq.addRect(corner);
            QPainterPath disc; disc.addEllipse(center, rad, rad);
            p.drawPath(sq.subtracted(disc));
        };
        const qreal L = box.left(),  R = box.right();
        const qreal T = box.top(),   B = box.bottom();
        cut(QRectF(L,        T,        rad, rad), QPointF(L + rad, T + rad)); /* alto-sx */
        cut(QRectF(R - rad,  T,        rad, rad), QPointF(R - rad, T + rad)); /* alto-dx */
        cut(QRectF(L,        B - rad,  rad, rad), QPointF(L + rad, B - rad)); /* basso-sx */
        cut(QRectF(R - rad,  B - rad,  rad, rad), QPointF(R - rad, B - rad)); /* basso-dx */
        Q_UNUSED(bg);
    }
};
