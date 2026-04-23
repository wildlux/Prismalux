#pragma once
#include <QObject>
#include <QString>
#include <QStringList>

/* ══════════════════════════════════════════════════════════════
   ThemeManager — singleton per il cambio tema a runtime
   ══════════════════════════════════════════════════════════════ */
class ThemeManager : public QObject {
    Q_OBJECT
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

signals:
    void changed(const QString& id);

private:
    explicit ThemeManager(QObject* parent = nullptr);
    static ThemeManager* s_instance;

    QList<Theme> m_themes;
    QString      m_currentId;
};
