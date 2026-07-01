#pragma once
/*
 * lazy_tab_loader.h — LazyTabLoader: popola un QTabWidget on-demand.
 *
 * addEager() per la prima tab (QTabWidget non emette currentChanged per lo
 * stato iniziale, quindi l'indice 0 deve arrivare già costruito), addLazy()
 * per le altre — restano un placeholder vuoto finché l'utente non ci clicca
 * sopra (o una ricerca esterna non le seleziona via setCurrentIndex()).
 * Stesso pattern di ensureTabBuilt() in mainwindow.cpp, generalizzato per
 * essere riusabile su qualunque QTabWidget (usato da ImpostazioniPage sia
 * a livello di tab esterne che di sotto-tab interne).
 *
 * Nessun Q_OBJECT necessario: onCurrentChanged è un metodo pubblico
 * normale, il connect per puntatore-a-membro non richiede moc.
 */
#include <QObject>
#include <QTabWidget>
#include <QVector>
#include <functional>

class LazyTabLoader : public QObject {
public:
    explicit LazyTabLoader(QTabWidget* tabs, QObject* parent)
        : QObject(parent), m_tabs(tabs)
    {
        connect(m_tabs, &QTabWidget::currentChanged, this, &LazyTabLoader::onCurrentChanged);
    }

    void addEager(const QString& label, QWidget* widget)
    {
        m_tabs->addTab(widget, label);
        m_built.append(true);
        m_factories.append(nullptr);
    }

    void addLazy(const QString& label, std::function<QWidget*()> factory)
    {
        m_tabs->addTab(new QWidget(m_tabs), label);
        m_built.append(false);
        m_factories.append(std::move(factory));
    }

    void onCurrentChanged(int idx)
    {
        if (idx < 0 || idx >= m_built.size() || m_built[idx]) return;
        m_built[idx] = true;
        const QString text = m_tabs->tabText(idx);
        QWidget* placeholder = m_tabs->widget(idx);
        QWidget* real = m_factories[idx]();
        m_tabs->blockSignals(true);
        m_tabs->removeTab(idx);
        m_tabs->insertTab(idx, real, text);
        /* removeTab() sulla tab corrente può spostare la selezione su un
         * vicino: idx va sempre ripristinato esplicitamente, è quello che
         * l'utente (o la ricerca tab) ha appena richiesto. */
        m_tabs->setCurrentIndex(idx);
        m_tabs->blockSignals(false);
        placeholder->deleteLater();
    }

private:
    QTabWidget*                         m_tabs;
    QVector<bool>                       m_built;
    QVector<std::function<QWidget*()>>  m_factories;
};
