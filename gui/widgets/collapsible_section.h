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
#include <QSettings>
#include <QRegularExpression>

class CollapsibleSection : public QWidget {
    Q_OBJECT
public:
    explicit CollapsibleSection(const QString& title, QWidget* content,
                                bool startOpen = true, QWidget* parent = nullptr)
        : QWidget(parent), m_content(content)
    {
        /* D-61: stato aperto/chiuso ricordato tra sessioni. startOpen del
           chiamante resta il default alla prima apparizione; poi vince
           l'ultima scelta dell'utente. La chiave deriva dal titolo tr():
           cambiando lingua lo stato riparte dai default (accettato). */
        m_persistKey = persistKeyFromTitle(title);
        const bool open = m_persistKey.isEmpty()
            ? startOpen
            : QSettings("Prismalux", "GUI")
                  .value(m_persistKey, startOpen).toBool();

        auto* lay = new QVBoxLayout(this);
        lay->setContentsMargins(0, 0, 0, 0);
        lay->setSpacing(2);

        m_btn = new QToolButton(this);
        m_btn->setText(title);
        m_btn->setCheckable(true);
        m_btn->setChecked(open);
        m_btn->setToolButtonStyle(Qt::ToolButtonTextBesideIcon);
        m_btn->setArrowType(open ? Qt::DownArrow : Qt::RightArrow);
        m_btn->setAutoRaise(true);
        m_btn->setSizePolicy(QSizePolicy::Expanding, QSizePolicy::Fixed);
        m_btn->setStyleSheet(
            "QToolButton{border:none;font-weight:bold;text-align:left;padding:2px 4px;}");
        m_btn->setToolTip(tr("Mostra/nascondi la sezione"));
        lay->addWidget(m_btn);

        m_content->setParent(this);
        m_content->setVisible(open);
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
        if (!m_persistKey.isEmpty())
            QSettings("Prismalux", "GUI").setValue(m_persistKey, open);
        emit openChanged(open);
    }

private:
    /* Chiave QSettings derivata dal titolo (solo lettere/numeri).
       Vuota = niente persistenza: titolo senza caratteri utili oppure
       PRISMALUX_NO_UI_PERSIST impostata (i test la usano per restare
       deterministici e non scrivere nelle QSettings reali). */
    static QString persistKeyFromTitle(const QString& title)
    {
        if (qEnvironmentVariableIsSet("PRISMALUX_NO_UI_PERSIST"))
            return {};
        static const QRegularExpression kNonAlnum("[^\\p{L}\\p{N}]+");
        QString slug = title;
        slug.remove(kNonAlnum);
        return slug.isEmpty() ? QString()
                              : QString("ui/collapsedOpen/") + slug;
    }

    QToolButton* m_btn     = nullptr;
    QWidget*     m_content = nullptr;
    QString      m_persistKey;
};
