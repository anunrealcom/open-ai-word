#include "aimanager.h"
#include <QSettings>

AIManager::AIManager(QObject* parent) : QObject(parent) {
    m_provider = new OpenAIProvider(this);
    connect(m_provider, &OpenAIProvider::finished, this, &AIManager::finished);
    connect(m_provider, &OpenAIProvider::errorOccurred, this, &AIManager::errorOccurred);

    QSettings s;
    m_baseUrl = s.value("ai/baseUrl", "https://api.openai.com/v1").toString();
    m_apiKey = s.value("ai/apiKey", "").toString();
    m_model = s.value("ai/model", "gpt-4o-mini").toString();

    m_provider->setBaseUrl(m_baseUrl);
    m_provider->setApiKey(m_apiKey);
}

AIManager& AIManager::instance() {
    static AIManager m;
    return m;
}

void AIManager::configure(const QString& baseUrl, const QString& apiKey, const QString& model) {
    m_baseUrl = baseUrl;
    m_apiKey = apiKey;
    m_model = model;
    m_provider->setBaseUrl(baseUrl);
    m_provider->setApiKey(apiKey);

    QSettings s;
    s.setValue("ai/baseUrl", baseUrl);
    s.setValue("ai/apiKey", apiKey);
    s.setValue("ai/model", model);
}

void AIManager::chat(const QList<AIMessage>& messages) {
    m_provider->chat(messages, m_model);
}
