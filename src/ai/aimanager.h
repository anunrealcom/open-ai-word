#pragma once
#include "aiprovider.h"
#include "openaiprovider.h"
#include <QObject>
#include <QString>

// Centralizes provider + configuration (persisted via QSettings) and forwards
// chat requests/responses. Other UI only talks to AIManager.
class AIManager : public QObject {
    Q_OBJECT
public:
    static AIManager& instance();

    void configure(const QString& baseUrl, const QString& apiKey, const QString& model);
    QString baseUrl() const { return m_baseUrl; }
    QString apiKey() const { return m_apiKey; }
    QString model() const { return m_model; }

    void chat(const QList<AIMessage>& messages);

signals:
    void finished(const QString& content);
    void errorOccurred(const QString& message);

private:
    explicit AIManager(QObject* parent = nullptr);
    OpenAIProvider* m_provider;
    QString m_baseUrl;
    QString m_apiKey;
    QString m_model;
};
