#include "widget_rab0l.h"
#include "../dpi_utils.h"
#include "rab0l_canvas.h"
#include <QVBoxLayout>
#include <QHBoxLayout>
#include <QLabel>
#include <QPushButton>
#include <QLineEdit>
#include <QRegularExpression>

Rab0lWidget::Rab0lWidget(QWidget* parent)
    : QWidget(parent)
{
    auto* vlay = new QVBoxLayout(this);
    vlay->setContentsMargins(10, 8, 10, 8);
    vlay->setSpacing(6);

    auto* bar  = new QWidget(this);
    auto* hlay = new QHBoxLayout(bar);
    hlay->setContentsMargins(0, 0, 0, 0);
    hlay->setSpacing(8);

    m_seq1 = new QLineEdit(this);
    m_seq1->setPlaceholderText(tr("Sequenza 1  (es. ATCGATCGGCTA)"));
    m_seq2 = new QLineEdit(this);
    m_seq2->setPlaceholderText(tr("Sequenza 2  (opzionale \xe2\x80\x94 calcola SIM)"));

    auto* btnAn = new QPushButton("\xe2\x96\xb6  Analizza", this);
    btnAn->setObjectName("primaryBtn");
    auto* btnCl = new QPushButton("\xf0\x9f\x97\x91", this);
    btnCl->setToolTip(tr("Pulisci"));
    btnCl->setFixedWidth(dpiScale(32));

    m_simLbl = new QLabel(this);
    m_simLbl->setTextFormat(Qt::RichText);

    hlay->addWidget(new QLabel("Seq 1:", this));
    hlay->addWidget(m_seq1, 3);
    hlay->addWidget(new QLabel("Seq 2:", this));
    hlay->addWidget(m_seq2, 3);
    hlay->addWidget(btnAn);
    hlay->addWidget(btnCl);
    hlay->addWidget(m_simLbl, 2);

    m_canvas = new Rab0lCanvas(this);

    vlay->addWidget(bar);
    vlay->addWidget(m_canvas, 1);

    connect(btnAn,  &QPushButton::clicked,     this, &Rab0lWidget::onAnalyzeClicked);
    connect(m_seq1, &QLineEdit::returnPressed, this, &Rab0lWidget::onAnalyzeClicked);
    connect(btnCl,  &QPushButton::clicked,     this, &Rab0lWidget::onClearClicked);
}

void Rab0lWidget::onAnalyzeClicked()
{
    static const QRegularExpression kNonDna("[^ATCGatcg]");
    QString s1 = m_seq1->text().trimmed().toUpper();
    QString s2 = m_seq2->text().trimmed().toUpper();
    s1.remove(kNonDna);
    s2.remove(kNonDna);
    if (s1.isEmpty()) return;
    if (s2.isEmpty()) {
        m_canvas->setSeq1Only(s1);
        m_simLbl->setText(
            QString("<b>N=%1</b>  <span style='color:gray'>inserisci Seq 2 per confronto SIM</span>")
            .arg(s1.size()));
    } else {
        m_canvas->setSequences(s1, s2);
        const auto& r = m_canvas->result();
        const QString col = (r.sim >= 0.8) ? "#4CAF50" : (r.sim >= 0.4) ? "#FFC107" : "#F44336";
        m_simLbl->setText(
            QString("<b>SIM = <span style='color:%1'>%2</span></b>  |  N=%3  |  SNP=%4")
            .arg(col).arg(r.sim, 0, 'f', 3).arg(r.len).arg(r.snp));
    }
}

void Rab0lWidget::onClearClicked()
{
    m_seq1->clear();
    m_seq2->clear();
    m_simLbl->clear();
    m_canvas->clearAll();
}
