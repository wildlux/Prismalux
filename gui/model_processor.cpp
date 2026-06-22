#include "model_processor.h"
#include <QSettings>
#include <QUuid>
#include <QRegularExpression>
#include <QJsonDocument>
#include <QJsonObject>

/* ── Singleton ─────────────────────────────────────────────── */
ModelProcessor& ModelProcessor::instance() {
    static ModelProcessor inst(nullptr);
    return inst;
}

ModelProcessor::ModelProcessor(QObject* parent) : QObject(parent) {
    loadProfiles();
}

/* ── Persistenza ────────────────────────────────────────────── */
void ModelProcessor::loadProfiles() {
    QSettings s;
    const int n = s.beginReadArray("ModelProfiles");
    m_profiles.clear();
    for (int i = 0; i < n; ++i) {
        s.setArrayIndex(i);
        ModelProfile p;
        p.id          = s.value("id").toString();
        p.label       = s.value("label").toString();
        p.namePattern = s.value("namePattern").toString();
        p.enabled     = s.value("enabled", true).toBool();
        for (const auto& v : s.value("preActions").toList())
            p.preActions.append(static_cast<PreAction>(v.toInt()));
        for (const auto& v : s.value("postActions").toList())
            p.postActions.append(static_cast<PostAction>(v.toInt()));
        const QByteArray pj = s.value("params").toByteArray();
        if (!pj.isEmpty()) {
            const QJsonObject obj = QJsonDocument::fromJson(pj).object();
            for (auto it = obj.begin(); it != obj.end(); ++it)
                p.params[it.key()] = it.value().toString();
        }
        p.translateModel = s.value("translateModel").toString();
        if (!p.id.isEmpty() && !p.namePattern.isEmpty())
            m_profiles.append(p);
    }
    s.endArray();
}

void ModelProcessor::saveProfiles() const {
    QSettings s;
    s.beginWriteArray("ModelProfiles", m_profiles.size());
    for (int i = 0; i < m_profiles.size(); ++i) {
        s.setArrayIndex(i);
        const ModelProfile& p = m_profiles[i];
        s.setValue("id",          p.id);
        s.setValue("label",       p.label);
        s.setValue("namePattern", p.namePattern);
        s.setValue("enabled",     p.enabled);
        QVariantList pre, post;
        for (auto a : p.preActions)  pre.append(static_cast<int>(a));
        for (auto a : p.postActions) post.append(static_cast<int>(a));
        s.setValue("preActions",  pre);
        s.setValue("postActions", post);
        QJsonObject obj;
        for (auto it = p.params.constBegin(); it != p.params.constEnd(); ++it)
            obj[it.key()] = it.value();
        s.setValue("params", QJsonDocument(obj).toJson(QJsonDocument::Compact));
        s.setValue("translateModel", p.translateModel);
    }
    s.endArray();
}

void ModelProcessor::setProfiles(const QList<ModelProfile>& p) {
    m_profiles = p;
    saveProfiles();
}

/* ── Match ──────────────────────────────────────────────────── */
const ModelProfile* ModelProcessor::matchProfile(const QString& modelName) const {
    for (const auto& p : m_profiles) {
        if (!p.enabled || p.namePattern.isEmpty()) continue;
        QRegularExpression re(p.namePattern, QRegularExpression::CaseInsensitiveOption);
        if (re.isValid() && re.match(modelName).hasMatch())
            return &p;
    }
    return nullptr;
}

/* ── Pre-processing ─────────────────────────────────────────── */
static QString forceLangText(PreAction a) {
    switch (a) {
    case PreAction::ForceLangIt:
        return "Rispondi SEMPRE in italiano, qualunque sia la lingua del tuo training.\n";
    case PreAction::ForceLangEn:
        return "Always respond in English, regardless of your training language.\n";
    case PreAction::ForceLangFr:
        return "R\xc3\xa9ponds toujours en fran\xc3\xa7" "ais, quelle que soit ta langue d'entra\xc3\xaenement.\n";
    case PreAction::ForceLangZh:
        /* 请始终用中文回答。*/
        return "\xe8\xaf\xb7\xe5\xa7\x8b\xe7\xbb\x88\xe7\x94\xa8\xe4\xb8\xad\xe6\x96\x87\xe5\x9b\x9e\xe7\xad\x94\xe3\x80\x82\n";
    case PreAction::ForceLangDe:
        return "Antworte immer auf Deutsch, unabh\xc3\xa4ngig von deiner Trainingssprache.\n";
    default: return {};
    }
}

QPair<QString,QString> ModelProcessor::preProcess(const QString& sys,
                                                   const QString& user,
                                                   const QString& modelName) const {
    const ModelProfile* p = matchProfile(modelName);
    if (!p) return {sys, user};

    QString sysOut  = sys;
    QString userOut = user;

    for (PreAction a : p->preActions) {
        switch (a) {
        case PreAction::None: break;
        case PreAction::ForceLangIt:
        case PreAction::ForceLangEn:
        case PreAction::ForceLangFr:
        case PreAction::ForceLangZh:
        case PreAction::ForceLangDe:
            sysOut = forceLangText(a) + sysOut;
            break;
        case PreAction::PrependSystem:
            if (p->params.contains("pre_sys"))
                sysOut = p->params["pre_sys"] + "\n" + sysOut;
            break;
        case PreAction::PrependUser:
            if (p->params.contains("pre_user"))
                userOut = p->params["pre_user"] + "\n" + userOut;
            break;
        case PreAction::ReplaceSystem:
            if (p->params.contains("repl_sys"))
                sysOut = p->params["repl_sys"];
            break;
        }
    }
    return {sysOut, userOut};
}

/* ── Post-processing sincrono ───────────────────────────────── */
QString ModelProcessor::postProcess(const QString& response, const QString& modelName) const {
    const ModelProfile* p = matchProfile(modelName);
    if (!p) return response;

    QString out = response;
    for (PostAction a : p->postActions) {
        switch (a) {
        case PostAction::None: break;
        case PostAction::RemoveThinkTags:
            out.remove(QRegularExpression("<think>[\\s\\S]*?</think>",
                QRegularExpression::CaseInsensitiveOption));
            out = out.trimmed();
            break;
        case PostAction::StripMarkdown:
            out.remove(QRegularExpression("\\*{1,3}([^*\\n]+)\\*{1,3}"));
            out.remove(QRegularExpression("^#{1,6}\\s*", QRegularExpression::MultilineOption));
            out.remove(QRegularExpression("`{1,3}[^`\\n]*`{1,3}"));
            break;
        case PostAction::RegexReplace:
            if (p->params.contains("rx_from") && p->params.contains("rx_to")) {
                QRegularExpression re(p->params["rx_from"]);
                if (re.isValid())
                    out.replace(re, p->params["rx_to"]);
            }
            break;
        case PostAction::TranslateLLM_It:
        case PostAction::TranslateLLM_En:
            // Gestito esternamente tramite needsLLMTranslation() + llmTranslateLang()
            break;
        }
    }
    return out;
}

/* ── Traduzione LLM ─────────────────────────────────────────── */
bool ModelProcessor::needsLLMTranslation(const QString& modelName) const {
    const ModelProfile* p = matchProfile(modelName);
    if (!p) return false;
    for (PostAction a : p->postActions)
        if (a == PostAction::TranslateLLM_It || a == PostAction::TranslateLLM_En)
            return true;
    return false;
}

QString ModelProcessor::llmTranslateLang(const QString& modelName) const {
    const ModelProfile* p = matchProfile(modelName);
    if (!p) return "it";
    for (PostAction a : p->postActions) {
        if (a == PostAction::TranslateLLM_It) return "it";
        if (a == PostAction::TranslateLLM_En) return "en";
    }
    return "it";
}

QString ModelProcessor::llmTranslateModel(const QString& modelName) const {
    const ModelProfile* p = matchProfile(modelName);
    if (!p || p->translateModel.trimmed().isEmpty()) return {};
    return p->translateModel.trimmed();
}

/* ── Profili predefiniti ────────────────────────────────────── */
QList<ModelProfile> ModelProcessor::defaultProfiles() {
    QList<ModelProfile> list;
    auto mkId = [] { return QUuid::createUuid().toString(QUuid::WithoutBraces); };

    {   ModelProfile p;
        p.id = mkId(); p.label = "DeepSeek \xe2\x86\x92 italiano";
        p.namePattern = "deepseek.*";
        p.preActions  = { PreAction::ForceLangIt };
        p.postActions = { PostAction::RemoveThinkTags };
        list.append(p); }

    {   ModelProfile p;
        p.id = mkId(); p.label = "Qwen \xe2\x86\x92 italiano";
        p.namePattern = "qwen.*";
        p.preActions  = { PreAction::ForceLangIt };
        p.postActions = { PostAction::RemoveThinkTags };
        list.append(p); }

    {   ModelProfile p;
        p.id = mkId(); p.label = "Mistral \xe2\x86\x92 italiano";
        p.namePattern = "mi(?:stral|xtral).*";
        p.preActions  = { PreAction::ForceLangIt };
        list.append(p); }

    {   ModelProfile p;
        p.id = mkId(); p.label = "Phi \xe2\x86\x92 italiano";
        p.namePattern = "phi.*";
        p.preActions  = { PreAction::ForceLangIt };
        list.append(p); }

    {   ModelProfile p;
        p.id = mkId(); p.label = "GLM (Zhipu) \xe2\x86\x92 italiano";
        p.namePattern = "glm.*";
        p.preActions  = { PreAction::ForceLangIt };
        p.postActions = { PostAction::RemoveThinkTags };
        list.append(p); }

    return list;
}
