#pragma once
#include <QFrame>
#include <QWidget>
#include <QHBoxLayout>
#include <QVBoxLayout>
#include <QLineEdit>
#include <QLabel>
#include <QPushButton>
#include <QScrollArea>
#include <QList>
#include <QVector>
#include <QDragEnterEvent>
#include <QDropEvent>
#include "dpi_utils.h"

/* ─── Singolo blocco formula con slot editabili per ogni {} ─── */
class FormulaBlock : public QFrame {
    Q_OBJECT
public:
    explicit FormulaBlock(const QString& tpl, QWidget* parent = nullptr);
    QString toLaTeX() const;

signals:
    void changed();
    void removeRequested();

private slots:
    void onRemove() { emit removeRequested(); }

private:
    QString          m_tpl;
    bool             m_autoAppendC = false;
    QVector<QLineEdit*> m_slots;
};

/* ─── Area dove si trascinano i blocchi per costruire la formula ─── */
class FormulaBuilderWidget : public QWidget {
    Q_OBJECT
public:
    explicit FormulaBuilderWidget(QWidget* parent = nullptr);
    QString toLaTeX() const;
    int     blockCount() const { return m_blocks.size(); }
    void clearAll();
    void addBlock(const QString& tpl);

signals:
    void formulaChanged(const QString& latex);

protected:
    void dragEnterEvent(QDragEnterEvent* e) override;
    void dropEvent(QDropEvent* e) override;

private slots:
    void onBlockChanged();
    void onBlockRemoveRequested();

private:
    void updatePlaceholder();

    QHBoxLayout*        m_blockLay   = nullptr;
    QLabel*             m_placeholder = nullptr;
    QScrollArea*        m_scroll     = nullptr;
    QList<FormulaBlock*> m_blocks;
};
