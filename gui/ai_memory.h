#pragma once
#include <QObject>
#include <QString>
#include <QStringList>

/*
 * AIMemory — memoria versionata con Git
 *
 * Gestisce ~/.ai-memory/ come repository Git locale.
 * Ogni preferenza appresa, feedback e interazione diventa un commit →
 * storia completa, revertibile, analizzabile con `git log`.
 *
 * Struttura:
 *   ~/.ai-memory/
 *   ├── profile/
 *   │   ├── preferences.yaml   ← preferenze apprese
 *   │   └── habits.yaml        ← abitudini
 *   ├── interactions/
 *   │   └── YYYY-MM/
 *   │       ├── DD.md          ← conversazioni del giorno
 *   │       └── feedback.yaml  ← 👍/👎 con motivo
 *   ├── context/
 *   │   └── active_context.yaml
 *   └── .git/
 */
class AIMemory : public QObject {
    Q_OBJECT
public:
    explicit AIMemory(QObject* parent = nullptr);

    /* Inizializza il repo git (idempotente) */
    bool initialize();
    bool isInitialized() const { return m_initialized; }
    QString rootPath()   const { return m_root; }

    /* Salva coppia query/response nel diario giornaliero e committa */
    void saveInteraction(const QString& query, const QString& response);

    /* Registra feedback 👍/👎 con motivo opzionale */
    void logFeedback(const QString& query, const QString& response,
                     bool thumbsUp, const QString& reason = {});

    /* Carica preferences.yaml + ultimi maxFeedback feedback come stringa di contesto */
    QString getRelevantContext(const QString& query = {}, int maxFeedback = 5) const;

    /* Aggiorna una chiave YAML in profile/preferences.yaml */
    void updatePreference(const QString& key, const QString& value);

    /* Ultimi n commit (sha short + messaggio) */
    QStringList gitLog(int n = 20) const;

    /* Ripristina un file a un commit specifico (relativePath dentro m_root) */
    bool revertFile(const QString& commitHash, const QString& relativePath);

signals:
    void committed(const QString& message);
    void error(const QString& message);

private:
    QString m_root;
    bool    m_initialized = false;

    int     gitRun(const QStringList& args, QString* out = nullptr) const;
    bool    gitCommitAll(const QString& message);
    void    ensureDirs() const;

    QString preferencesPath()      const;
    QString todayFeedbackPath()    const;
    QString todayInteractionPath() const;

    static QString readFile(const QString& path);
    static void    writeFile(const QString& path, const QString& content);
    static void    appendFile(const QString& path, const QString& content);
    static void    setYamlKey(const QString& path,
                               const QString& key, const QString& value);
};
