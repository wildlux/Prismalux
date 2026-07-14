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
   Il taglio amputa però anche il bordo 1px della bolla (le linee
   dritte terminavano di colpo e la curva restava senza contorno —
   segnalato da Paolo 2026-07-13 con screenshot): dopo il taglio si
   ridisegna quindi l'intero contorno con drawRoundedRect, usando
   colore/spessore letti dal QTextTableCellFormat della cella
   (verificato in standalone: lo stroke coincide coi lati dritti
   già disegnati dalla base, nessuna doppia linea visibile).

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
#include <QTextTableCell>
#include <QTextTableFormat>
#include <QTextLength>
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

                /* Geometria REALE della cella dai block rect interni:
                   contenuto (primo ∪ ultimo blocco) + padding + bordo.
                   frameBoundingRect() ± larghezza spacer (versione
                   precedente) includeva i margini del frame tabella
                   (~16px): il taglio cadeva fuori dalla cella e lo
                   stroke "fluttuava" accanto al bordo vero — doppio
                   contorno visibile (screenshot Paolo 2026-07-14).
                   Verificato in standalone: bordo reale a x=565,
                   blocchi+padding+bordo → 566; frameBoundingRect → 581. */
                /* Bordi per lato: la riga inferiore della striscia
                   "Ragionamento" ha border-top:none — usare solo
                   topBorder() lì azzerava box e stroke. */
                const QTextTableCellFormat cf =
                    cell.format().toTableCellFormat();
                const qreal bwT = cf.topBorder(),  bwB = cf.bottomBorder();
                const qreal bwL = cf.leftBorder(), bwR = cf.rightBorder();
                const QRectF firstBlk = lay->blockBoundingRect(
                    cell.firstCursorPosition().block());
                const QRectF lastBlk = lay->blockBoundingRect(
                    cell.lastCursorPosition().block());
                QRectF box = firstBlk.united(lastBlk).adjusted(
                    -cf.leftPadding() - bwL, -cf.topPadding()  - bwT,
                     cf.rightPadding() + bwR, cf.bottomPadding() + bwB);
                box.translate(off);
                if (box.isEmpty() || !box.intersects(vpRect))
                    continue;                                  /* fuori schermo */

                /* Nelle tabelle multi-riga (striscia "Ragionamento":
                   header + corpo) si arrotondano solo gli angoli sul
                   perimetro esterno — mai la giuntura tra le righe. */
                const bool roundTop    = (r == 0);
                const bool roundBottom = (r == t->rows() - 1);
                const qreal rad = qMin<qreal>(radius,
                    qMin(box.width(), box.height()) / 2.0);

                cutRoundedCorners(p, box, rad, roundTop, roundBottom);

                /* Contorno con gli stessi angoli arrotondati al posto del
                   bordo dritto amputato dal taglio (se la cella ne ha uno).
                   Spessore/colore dal primo lato con bordo reale. */
                const qreal bw = qMax(qMax(bwT, bwB), qMax(bwL, bwR));
                const QColor bc = (bwT > 0 ? cf.topBorderBrush()
                                 : bwB > 0 ? cf.bottomBorderBrush()
                                 : bwL > 0 ? cf.leftBorderBrush()
                                           : cf.rightBorderBrush()).color();
                if (bw > 0 && bc.isValid() && bc.alpha() > 0) {
                    p.setPen(QPen(bc, bw));
                    p.setBrush(Qt::NoBrush);
                    p.drawPath(roundedCellPath(
                        box.adjusted(bw/2, bw/2, -bw/2, -bw/2),
                        rad, roundTop, roundBottom));
                    p.setPen(Qt::NoPen);   /* ripristina per i cut successivi */
                    p.setBrush(pageBg);
                }
            }
        }
    }

private:
    /* Taglia gli angoli richiesti del riquadro col colore di sfondo: la
       parte del quadratino d'angolo che sta FUORI dal quarto di cerchio
       viene coperta, lasciando l'angolo arrotondato. Copre anche i
       segmenti di bordo che cadono negli angoli — il contorno viene
       ridisegnato intero dal chiamante con roundedCellPath(). */
    static void cutRoundedCorners(QPainter& p, const QRectF& box,
                                  qreal rad, bool top, bool bottom)
    {
        if (rad <= 0.5) return;

        auto cut = [&](const QRectF& corner, const QPointF& center) {
            QPainterPath sq;   sq.addRect(corner);
            QPainterPath disc; disc.addEllipse(center, rad, rad);
            p.drawPath(sq.subtracted(disc));
        };
        const qreal L = box.left(),  R = box.right();
        const qreal T = box.top(),   B = box.bottom();
        if (top) {
            cut(QRectF(L,       T, rad, rad), QPointF(L + rad, T + rad)); /* alto-sx */
            cut(QRectF(R - rad, T, rad, rad), QPointF(R - rad, T + rad)); /* alto-dx */
        }
        if (bottom) {
            cut(QRectF(L,       B - rad, rad, rad), QPointF(L + rad, B - rad)); /* basso-sx */
            cut(QRectF(R - rad, B - rad, rad, rad), QPointF(R - rad, B - rad)); /* basso-dx */
        }
    }

    /* Rettangolo con i soli angoli alto/basso arrotondati a scelta —
       serve alle strisce multi-riga dove la giuntura resta dritta. */
    static QPainterPath roundedCellPath(const QRectF& r, qreal rad,
                                        bool top, bool bottom)
    {
        QPainterPath path;
        const qreal tl = top ? rad : 0, tr = top ? rad : 0;
        const qreal bl = bottom ? rad : 0, br = bottom ? rad : 0;
        path.moveTo(r.left() + tl, r.top());
        path.lineTo(r.right() - tr, r.top());
        if (tr > 0) path.arcTo(r.right() - 2*tr, r.top(), 2*tr, 2*tr, 90, -90);
        path.lineTo(r.right(), r.bottom() - br);
        if (br > 0) path.arcTo(r.right() - 2*br, r.bottom() - 2*br, 2*br, 2*br, 0, -90);
        path.lineTo(r.left() + bl, r.bottom());
        if (bl > 0) path.arcTo(r.left(), r.bottom() - 2*bl, 2*bl, 2*bl, 270, -90);
        path.lineTo(r.left(), r.top() + tl);
        if (tl > 0) path.arcTo(r.left(), r.top(), 2*tl, 2*tl, 180, -90);
        path.closeSubpath();
        return path;
    }
};
