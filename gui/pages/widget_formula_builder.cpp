#include "widget_formula_builder.h"
#include <QMimeData>
#include <QRegularExpression>

/* ══ FormulaBlock ══════════════════════════════════════════════════════════ */

FormulaBlock::FormulaBlock(const QString& tpl, QWidget* parent)
    : QFrame(parent), m_tpl(tpl)
{
    setFrameShape(QFrame::StyledPanel);
    setStyleSheet("QFrame{"
                  "background:#1e293b;"
                  "border:1px solid #334155;"
                  "border-radius:5px;}");

    auto* hbox = new QHBoxLayout(this);
    hbox->setContentsMargins(5, 2, 5, 2);
    hbox->setSpacing(3);

    /* integrali → aggiungi +C automaticamente */
    static const QStringList kIntSymbols = {"\\int", "\\oint", "\\iint"};
    for (const auto& sym : kIntSymbols)
        if (tpl.contains(sym)) { m_autoAppendC = true; break; }

    /* parse del template: ogni {} diventa un QLineEdit con placeholder "N°" */
    QString part;
    for (int i = 0; i < tpl.size(); ) {
        if (i + 1 < tpl.size() && tpl[i] == '{' && tpl[i+1] == '}') {
            if (!part.isEmpty()) {
                auto* lbl = new QLabel(part, this);
                lbl->setStyleSheet("color:#93c5fd;font-family:monospace;font-size:11px;");
                hbox->addWidget(lbl);
                part.clear();
            }
            auto* le = new QLineEdit(this);
            le->setPlaceholderText("N\xc2\xb0");   /* "N°" */
            le->setFixedWidth(dpiScale(36));
            le->setFixedHeight(dpiScale(20));
            le->setStyleSheet("background:#0f172a;"
                              "color:#f1f5f9;"
                              "border:1px solid #475569;"
                              "border-radius:3px;"
                              "padding:1px 4px;"
                              "font-size:11px;");
            m_slots.append(le);
            hbox->addWidget(le);
            connect(le, &QLineEdit::textChanged, this, &FormulaBlock::changed);
            i += 2;
        } else {
            part += tpl[i++];
        }
    }
    if (!part.isEmpty()) {
        auto* lbl = new QLabel(part, this);
        lbl->setStyleSheet("color:#93c5fd;font-family:monospace;font-size:11px;");
        hbox->addWidget(lbl);
    }

    if (m_autoAppendC) {
        auto* cLbl = new QLabel("+C", this);
        cLbl->setStyleSheet("color:#a78bfa;font-family:monospace;font-size:11px;"
                            "padding-left:2px;");
        hbox->addWidget(cLbl);
    }

    /* pulsante × per rimuovere il blocco */
    auto* del = new QPushButton("\xc3\x97", this);   /* × */
    del->setFixedSize(dpiScale(14), dpiScale(14));
    del->setToolTip(tr("Rimuovi blocco"));
    del->setStyleSheet("QPushButton{"
                       "background:transparent;"
                       "color:#ef4444;"
                       "border:none;"
                       "font-weight:bold;}"
                       "QPushButton:hover{color:#fca5a5;}");
    hbox->addWidget(del);
    connect(del, &QPushButton::clicked, this, &FormulaBlock::onRemove);
}

QString FormulaBlock::toLaTeX() const
{
    QString result = m_tpl;
    for (auto* le : m_slots) {
        const int pos = result.indexOf("{}");
        if (pos >= 0)
            result.replace(pos, 2, "{" + le->text() + "}");
    }
    if (m_autoAppendC) result += " + C";
    return result;
}

/* ══ FormulaBuilderWidget ══════════════════════════════════════════════════ */

FormulaBuilderWidget::FormulaBuilderWidget(QWidget* parent)
    : QWidget(parent)
{
    setAcceptDrops(true);
    setMinimumHeight(dpiScale(50));
    setStyleSheet("FormulaBuilderWidget{"
                  "background:#0f172a;"
                  "border:2px dashed #334155;"
                  "border-radius:6px;}");

    auto* outer = new QVBoxLayout(this);
    outer->setContentsMargins(4, 4, 4, 4);
    outer->setSpacing(0);

    m_placeholder = new QLabel(tr("Trascina i simboli qui  \xe2\x86\x93"), this);   /* ↓ */
    m_placeholder->setStyleSheet("color:#475569;font-size:11px;");
    m_placeholder->setAlignment(Qt::AlignCenter);
    outer->addWidget(m_placeholder);

    auto* scrollContent = new QWidget(this);
    m_blockLay = new QHBoxLayout(scrollContent);
    m_blockLay->setContentsMargins(2, 2, 2, 2);
    m_blockLay->setSpacing(4);
    m_blockLay->addStretch();

    m_scroll = new QScrollArea(this);
    m_scroll->setWidget(scrollContent);
    m_scroll->setWidgetResizable(true);
    m_scroll->setHorizontalScrollBarPolicy(Qt::ScrollBarAsNeeded);
    m_scroll->setVerticalScrollBarPolicy(Qt::ScrollBarAlwaysOff);
    m_scroll->setFixedHeight(dpiScale(40));
    m_scroll->setStyleSheet("QScrollArea{border:none;background:transparent;}");
    m_scroll->hide();
    outer->addWidget(m_scroll);
}

void FormulaBuilderWidget::addBlock(const QString& tpl)
{
    auto* block = new FormulaBlock(tpl, m_scroll->widget());
    m_blocks.append(block);
    m_blockLay->insertWidget(m_blocks.size() - 1, block);
    connect(block, &FormulaBlock::changed,
            this,  &FormulaBuilderWidget::onBlockChanged);
    connect(block, &FormulaBlock::removeRequested,
            this,  &FormulaBuilderWidget::onBlockRemoveRequested);
    updatePlaceholder();
    onBlockChanged();
}

QString FormulaBuilderWidget::toLaTeX() const
{
    QStringList parts;
    for (auto* b : m_blocks) parts << b->toLaTeX();
    return parts.join(" ");
}

void FormulaBuilderWidget::clearAll()
{
    for (auto* b : m_blocks) {
        m_blockLay->removeWidget(b);
        b->deleteLater();
    }
    m_blocks.clear();
    updatePlaceholder();
    emit formulaChanged("");
}

void FormulaBuilderWidget::dragEnterEvent(QDragEnterEvent* e)
{
    if (e->mimeData()->hasFormat("application/x-prismalux-math"))
        e->acceptProposedAction();
    else
        e->ignore();
}

void FormulaBuilderWidget::dropEvent(QDropEvent* e)
{
    if (e->mimeData()->hasFormat("application/x-prismalux-math")) {
        const QByteArray raw = e->mimeData()->data("application/x-prismalux-math");
        /* Limita dimensione e caratteri validi LaTeX — previene iniezione da app esterne */
        static const QRegularExpression kSafe(
            R"(^[\\a-zA-Z0-9{}_^+\-*/=()|\[\] .,:%!]+$)");
        if (raw.size() > 512 || !kSafe.match(QString::fromUtf8(raw)).hasMatch()) {
            e->ignore();
            return;
        }
        addBlock(QString::fromUtf8(raw));
        e->acceptProposedAction();
    }
}

void FormulaBuilderWidget::onBlockChanged()
{
    emit formulaChanged(toLaTeX());
}

void FormulaBuilderWidget::onBlockRemoveRequested()
{
    auto* block = qobject_cast<FormulaBlock*>(sender());
    if (!block) return;
    m_blocks.removeOne(block);
    m_blockLay->removeWidget(block);
    block->deleteLater();
    updatePlaceholder();
    onBlockChanged();
}

void FormulaBuilderWidget::updatePlaceholder()
{
    const bool empty = m_blocks.isEmpty();
    m_placeholder->setVisible(empty);
    m_scroll->setVisible(!empty);
}
