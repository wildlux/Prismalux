#pragma once
/*
 * monitor_panel.h — Pannello temporaneo di diagnostica AI
 * =========================================================
 * Finestra non-modale che intercetta tutti i segnali di AiClient e
 * HardwareMonitor per costruire un registro dettagliato di ogni
 * sessione di inferenza.
 *
 * Per ogni richiesta chat() misura e visualizza:
 *   - Modello e backend usati
 *   - TTFT  — Time To First Token (ms)
 *   - Durata totale (ms)
 *   - Token contati in uscita
 *   - Throughput stimato (tok/s)
 *   - RAM prima e dopo (Δ%)
 *   - Score sintetico 0-100
 *
 * NOTA: questo pannello è temporaneo (uso diagnostico).
 * Aprirlo non modifica il comportamento del motore AI.
 */

#include <QDialog>
#include <QElapsedTimer>
#include <QTableWidget>
#include <QTextEdit>
#include <QLabel>
#include <QPainter>
#include <QVector>

#include "ai_client.h"
#include "hardware_monitor.h"

/* ── PerfChartWidget — bar chart storico sessioni AI ─────────────── */
class PerfChartWidget : public QWidget {
    Q_OBJECT
public:
    explicit PerfChartWidget(QWidget* parent = nullptr) : QWidget(parent) {
        setMinimumHeight(110);
        setSizePolicy(QSizePolicy::Expanding, QSizePolicy::Fixed);
    }

    struct Bar { double tokps; int score; bool aborted; };

    void addBar(const Bar& b) {
        m_bars.append(b);
        if (m_bars.size() > 30) m_bars.removeFirst();
        update();
    }

    void clear() { m_bars.clear(); update(); }

protected:
    void paintEvent(QPaintEvent*) override {
        QPainter p(this);
        p.setRenderHint(QPainter::Antialiasing);
        p.fillRect(rect(), QColor(14, 14, 26));

        if (m_bars.isEmpty()) {
            p.setPen(QColor(80, 80, 110));
            p.setFont(QFont("sans", 10));
            p.drawText(rect(), Qt::AlignCenter, "Nessuna sessione registrata");
            return;
        }

        const int n   = m_bars.size();
        const int ml  = 42, mr = 8, mt = 8, mb = 22;
        const int pw  = width()  - ml - mr;
        const int ph  = height() - mt - mb;
        const int gap = 3;
        const int bw  = std::max(6, (pw - gap * (n + 1)) / n);

        /* titoli assi */
        QFont fAx; fAx.setPixelSize(9); p.setFont(fAx);
        p.setPen(QColor(80, 90, 120));
        p.drawText(2, mt, ml - 4, ph / 2, Qt::AlignRight | Qt::AlignVCenter, "tok/s");
        p.drawText(2, mt + ph / 2, ml - 4, ph / 2, Qt::AlignRight | Qt::AlignVCenter, "score");

        /* linea divisoria orizzontale al centro */
        const int midY = mt + ph / 2;
        p.setPen(QPen(QColor(40, 40, 60), 1, Qt::DotLine));
        p.drawLine(ml, midY, ml + pw, midY);

        /* assi */
        p.setPen(QPen(QColor(60, 60, 90), 1));
        p.drawLine(ml, mt, ml, mt + ph);
        p.drawLine(ml, mt + ph, ml + pw, mt + ph);

        /* massimo tok/s per scala */
        double maxTok = 1.0;
        for (auto& b : m_bars) if (b.tokps > maxTok) maxTok = b.tokps;

        QFont fLbl; fLbl.setPixelSize(8); p.setFont(fLbl);

        for (int i = 0; i < n; i++) {
            const auto& b  = m_bars[i];
            const int   x  = ml + gap + i * (bw + gap);

            /* colore score: verde/giallo/arancio/rosso */
            QColor scoreCol = b.aborted ? QColor(100,100,100) :
                              b.score >= 90 ? QColor(105, 240, 174) :
                              b.score >= 70 ? QColor(255, 214, 0)   :
                              b.score >= 50 ? QColor(255, 152, 0)   :
                                              QColor(255, 23, 68);

            /* barra tok/s (metà superiore, altezza proporzionale) */
            const int tokH = b.tokps > 0
                ? (int)(b.tokps / maxTok * (ph / 2 - 4))
                : 0;
            if (tokH > 0) {
                p.fillRect(x, midY - tokH, bw, tokH, QColor(0, 176, 255, 200));
            }

            /* barra score (metà inferiore, altezza proporzionale a score/100) */
            const int scoreH = b.aborted ? 2
                : (int)(b.score / 100.0 * (ph / 2 - 4));
            p.fillRect(x, midY + 1, bw, scoreH, scoreCol);

            /* label indice sotto */
            p.setPen(QColor(70, 80, 110));
            p.drawText(x - 2, mt + ph + 3, bw + 4, 14,
                       Qt::AlignCenter, QString::number(i + 1));
        }

        /* legenda */
        p.setPen(QColor(100, 110, 140));
        p.fillRect(ml + pw - 90, mt + 2, 10, 8, QColor(0, 176, 255, 200));
        p.drawText(ml + pw - 77, mt, 80, 12, Qt::AlignLeft | Qt::AlignVCenter, "tok/s");
        p.fillRect(ml + pw - 90, mt + 14, 10, 8, QColor(105, 240, 174));
        p.drawText(ml + pw - 77, mt + 12, 80, 12, Qt::AlignLeft | Qt::AlignVCenter, "score");
    }

private:
    QVector<Bar> m_bars;
};

/* ══════════════════════════════════════════════════════════════
   MonitorPanel — finestra diagnostica inferenza AI
   ══════════════════════════════════════════════════════════════ */
class MonitorPanel : public QDialog {
    Q_OBJECT

public:
    /**
     * Costruttore.
     * @param ai  Puntatore all'AiClient condiviso (non-owning)
     * @param hw  Puntatore all'HardwareMonitor condiviso (non-owning)
     */
    explicit MonitorPanel(AiClient* ai, HardwareMonitor* hw,
                          QWidget* parent = nullptr);

public slots:
    /** Riceve aggiornamenti periodici RAM da HardwareMonitor. */
    void onHWUpdated(SysSnapshot snap);

private slots:
    void onRequestStarted(const QString& model, const QString& backend);
    void onFirstToken(const QString& chunk);
    void onFinished(const QString& full);
    void onError(const QString& msg);
    void onAborted();

private:
    /* ── Helpers ── */
    void appendLog(const QString& line);
    void flushSession();                     ///< chiude la sessione corrente e aggiunge riga tabella
    int  computeScore(qint64 ttftMs, qint64 durMs, int tokens, double ramDelta) const;

    /* ── UI ── */
    QTableWidget*    m_table   = nullptr;    ///< tabella sessioni completate
    QTextEdit*       m_log     = nullptr;    ///< log live
    QLabel*          m_liveInfo = nullptr;   ///< riga di stato in tempo reale
    PerfChartWidget* m_chart   = nullptr;    ///< chart storico performance sessioni

    /* ── Stato sessione corrente ── */
    struct Session {
        QString  model;
        QString  backend;
        qint64   ttftMs   = -1;
        qint64   durMs    = -1;
        int      tokens   = 0;
        double   ramBefore= 0.0;
        double   ramAfter = 0.0;
        bool     active   = false;
        bool     aborted  = false;
        QElapsedTimer timer;
    } m_cur;

    double m_lastRamUsed = 0.0;    ///< % RAM usata all'ultimo snapshot HW
    bool   m_firstToken  = false;  ///< true = primo token già ricevuto in questa sessione
};
