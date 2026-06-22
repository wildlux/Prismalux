#pragma once
#include <QObject>
#include <QString>
#include <QList>
#include <QMap>

/* ══════════════════════════════════════════════════════════════
   ModelProcessor — pre/post processing per-modello configurabile.

   Pre-processing: trasforma sys+user PRIMA di inviare all'LLM.
   Post-processing: trasforma la risposta DOPO averla ricevuta.

   Profili salvati in QSettings → gruppo "ModelProfiles".
   Abbinamento: primo profilo il cui namePattern (regex) matcha il nome modello.
   ══════════════════════════════════════════════════════════════ */

enum class PreAction {
    None        = 0,
    ForceLangIt = 1,   // Aggiunge "Rispondi SEMPRE in italiano." al sys prompt
    ForceLangEn = 2,   // "Always respond in English."
    ForceLangFr = 3,   // "Réponds toujours en français."
    ForceLangZh = 4,   // "请始终用中文回答。"
    ForceLangDe = 5,   // "Antworte immer auf Deutsch."
    PrependSystem = 6, // params["pre_sys"]  → anteposto al sys prompt
    PrependUser   = 7, // params["pre_user"] → anteposto al messaggio utente
    ReplaceSystem = 8, // params["repl_sys"] → sostituisce il sys prompt
};

enum class PostAction {
    None            = 0,
    RemoveThinkTags = 1, // rimuove <think>…</think>
    StripMarkdown   = 2, // rimuove **bold**, ##header, *italic*, `code`
    RegexReplace    = 3, // params["rx_from"] → params["rx_to"]
    TranslateLLM_It = 4, // seconda chiamata LLM: traduce in italiano
    TranslateLLM_En = 5, // seconda chiamata LLM: traduce in inglese
};

struct ModelProfile {
    QString id;             ///< UUID
    QString label;          ///< nome leggibile (es. "DeepSeek → italiano")
    QString namePattern;    ///< regex case-insensitive sul nome modello
    bool    enabled = true;
    QList<PreAction>  preActions;
    QList<PostAction> postActions;
    QMap<QString, QString> params;  ///< parametri extra (pre_sys, repl_sys, pre_user, rx_from, rx_to)
    QString translateModel; ///< modello dedicato per la traduzione LLM (es. "aya:8b");
                            ///< se vuoto, usa il modello principale del profilo
};

class ModelProcessor : public QObject {
    Q_OBJECT
public:
    /** Singleton — unica istanza per processo. */
    static ModelProcessor& instance();

    QList<ModelProfile> profiles() const { return m_profiles; }
    void setProfiles(const QList<ModelProfile>& p);
    void loadProfiles();
    void saveProfiles() const;

    /** Pre-processing: restituisce {sys', user'} con azioni applicate. */
    QPair<QString,QString> preProcess(const QString& sys,
                                      const QString& user,
                                      const QString& modelName) const;

    /** Post-processing sincrono (nessuna chiamata LLM): restituisce risposta modificata. */
    QString postProcess(const QString& response, const QString& modelName) const;

    /** True se il profilo attivo richiede una seconda chiamata LLM per tradurre. */
    bool    needsLLMTranslation(const QString& modelName) const;

    /** "it" o "en" — la lingua target per la traduzione LLM. */
    QString llmTranslateLang(const QString& modelName) const;

    /** Modello dedicato per la traduzione (vuoto = usa il modello principale). */
    QString llmTranslateModel(const QString& modelName) const;

    /** Profili predefiniti pronti da caricare (DeepSeek, Qwen, Mistral, Phi). */
    static QList<ModelProfile> defaultProfiles();

private:
    explicit ModelProcessor(QObject* parent = nullptr);
    const ModelProfile* matchProfile(const QString& modelName) const;

    QList<ModelProfile> m_profiles;
};
