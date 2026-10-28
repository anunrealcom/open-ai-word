#pragma once
// Plugin-friendly LLM provider interface. The app ships an OpenAI-compatible
// provider, but any LLM (DeepSeek, Qwen, local Ollama, etc.) that speaks the
// OpenAI /chat/completions protocol can be added as a new AIProvider subclass.
#include <QObject>
#include <QString>
#include <QList>

struct AIMessage {
    QString role;    // "system" | "user" | "assistant"
    QString content;
};

class AIProvider : public QObject {
    Q_OBJECT
public:
    explicit AIProvider(QObject* parent = nullptr) : QObject(parent) {}
    virtual ~AIProvider() override = default;

    virtual QString id() const = 0;
    virtual QString name() const = 0;

    // Async chat completion. Results arrive via signals.
    virtual void chat(const QList<AIMessage>& messages, const QString& model) = 0;

signals:
    void finished(const QString& content);
    void errorOccurred(const QString& message);
};
