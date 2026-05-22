#pragma once
#include <QObject>
#include <QString>
#include <QStringList>
#include <QHash>
#include <QGuiApplication>
#include <QStyleHints>

/* ══════════════════════════════════════════════════════════════
   ThemeManager — singleton per il cambio tema a runtime
   ══════════════════════════════════════════════════════════════ */
class ThemeManager : public QObject {
    Q_OBJECT
    Q_DISABLE_COPY(ThemeManager)
public:
    struct Theme {
        QString id;          /* chiave interna  */
        QString label;       /* nome nel menu   */
        QString resource;    /* path Qt resource: ":/themes/..." */
    };

    static ThemeManager* instance();

    /* Lista temi disponibili */
    const QList<Theme>& themes() const { return m_themes; }

    /* Applica tema e salva preferenza */
    void apply(const QString& id);

    /* Carica e applica il tema salvato (fallback: dark_cyan) */
    void loadSaved();

    /* Scansiona themes/ accanto all'exe — aggiunge temi .qss custom */
    void scanExternalThemes();

    /* ID tema attivo */
    QString currentId() const { return m_currentId; }

    /* QSS completo del tema attivo (per parsing colori a runtime) */
    QString currentQss() const { return m_cssCache.value(m_currentId); }

    /* Applica il tema chiaro/scuro in base al tema di sistema corrente */
    void applySystemTheme();

    /* Attiva/disattiva il follow del tema di sistema */
    void setFollowSystem(bool follow);

    bool followSystem() const { return m_followSystem; }

signals:
    void changed(const QString& id);

public slots:
    void onColorSchemeChanged();

private:
    explicit ThemeManager(QObject* parent = nullptr);
    static ThemeManager* s_instance;

    QList<Theme>         m_themes;
    QString              m_currentId;
    QHash<QString,QString> m_cssCache;   ///< id → CSS già letto da disco
    bool                 m_followSystem = false;
};
