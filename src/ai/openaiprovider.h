#pragma once
#include "aiprovider.h"
#include <QNetworkAccessManager>

// OpenAI-compatible chat provider. Works with OpenAI, DeepSeek, Qwen,
// Moonshot, Ollama (with the openai extension), and any endpoint that
// implements POST {baseUrl}/chat/completions.
class OpenAIProvider : public AIProvider {
    Q_OBJECT
public:
    explicit OpenAIProvider(QObject* parent = nullptr);
    void setBaseUrl(const QString& url);   // e.g. https://api.openai.com/v1
    void setApiKey(const QString& key);

    QString id() const override { return "openai-compatible"; }
    QString name() const override { return "OpenAI 兼容"; }

    void chat(const QList<AIMessage>& messages, const QString& model) override;

private slots:
    void onFinished(QNetworkReply* reply);

private:
    QNetworkAccessManager* m_nam;
    QString m_baseUrl;
    QString m_apiKey;
};
