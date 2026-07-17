#pragma once
/* ══════════════════════════════════════════════════════════════
   CollapsibleSection — pannello arrotolabile stile Blender (D-48).
   Header cliccabile con freccia ▾/▸ che mostra/nasconde il widget
   contenuto. Header-only con Q_OBJECT: va elencato in CPP_SRCS
   (CMakeLists) per AUTOMOC, come ai_error_widget.h.

   Uso:
     auto* sec = new CollapsibleSection(tr("Titolo"), contenuto, true, parent);
     lay->addWidget(sec);
   ══════════════════════════════════════════════════════════════ */
#include <QWidget>
#include <QToolButton>
#include <QVBoxLayout>
#include <QGroupBox>

class CollapsibleSection : public QWidget {
    Q_OBJECT
public:
    explicit CollapsibleSection(const QString& title, QWidget* content,
                                bool startOpen = true, QWidget* parent = nullptr)
        : QWidget(parent), m_content(content)
    {
        auto* lay = new QVBoxLayout(this);
        lay->setContentsMargins(0, 0, 0, 0);
        lay->setSpacing(2);

        m_btn = new QToolButton(this);
        m_btn->setText(title);
        m_btn->setCheckable(true);
        m_btn->setChecked(startOpen);
        m_btn->setToolButtonStyle(Qt::ToolButtonTextBesideIcon);
        m_btn->setArrowType(startOpen ? Qt::DownArrow : Qt::RightArrow);
        m_btn->setAutoRaise(true);
        m_btn->setSizePolicy(QSizePolicy::Expanding, QSizePolicy::Fixed);
        m_btn->setStyleSheet(
            "QToolButton{border:none;font-weight:bold;text-align:left;padding:2px 4px;}");
        m_btn->setToolTip(tr("Mostra/nascondi la sezione"));
        lay->addWidget(m_btn);

        m_content->setParent(this);
        m_content->setVisible(startOpen);
        lay->addWidget(m_content);

        connect(m_btn, &QToolButton::toggled,
                this, &CollapsibleSection::onToggled);
    }

    /* Adozione a 1 riga nelle colonne dense già scritte con QGroupBox:
       il titolo migra nell'header cliccabile, il box resta come cornice-card.
         lay->addWidget(CollapsibleSection::fromGroupBox(box)); */
    static CollapsibleSection* fromGroupBox(QGroupBox* box, bool startOpen = true,
                                            QWidget* parent = nullptr)
    {
        const QString title = box->title();
        box->setTitle(QString());
        return new CollapsibleSection(title, box, startOpen, parent);
    }

    bool isOpen() const     { return m_btn->isChecked(); }
    void setOpen(bool open) { m_btn->setChecked(open); }
    QWidget* content() const { return m_content; }

signals:
    void openChanged(bool open);

private slots:
    void onToggled(bool open)
    {
        m_content->setVisible(open);
        m_btn->setArrowType(open ? Qt::DownArrow : Qt::RightArrow);
        emit openChanged(open);
    }

private:
    QToolButton* m_btn     = nullptr;
    QWidget*     m_content = nullptr;
};
