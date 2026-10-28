#include "openaiprovider.h"
#include <QNetworkRequest>
#include <QNetworkReply>
#include <QJsonDocument>
#include <QJsonObject>
#include <QJsonArray>
#include <QUrl>
#include <QUrlQuery>

OpenAIProvider::OpenAIProvider(QObject* parent)
    : AIProvider(parent), m_nam(new QNetworkAccessManager(this)) {
    m_baseUrl = "https://api.openai.com/v1";
    connect(m_nam, &QNetworkAccessManager::finished, this, &OpenAIProvider::onFinished);
}

void OpenAIProvider::setBaseUrl(const QString& url) {
    m_baseUrl = url.trimmed();
    if (!m_baseUrl.endsWith('/')) m_baseUrl += '/';
}

void OpenAIProvider::setApiKey(const QString& key) {
    m_apiKey = key.trimmed();
}

void OpenAIProvider::chat(const QList<AIMessage>& messages, const QString& model) {
    if (m_apiKey.isEmpty()) {
        emit errorOccurred("未配置 API Key。请在右侧 AI 面板填写后重试。");
        return;
    }
    QUrl url(m_baseUrl + "chat/completions");
    QNetworkRequest req(url);
    req.setHeader(QNetworkRequest::ContentTypeHeader, "application/json");
    req.setRawHeader("Authorization", ("Bearer " + m_apiKey).toUtf8());

    QJsonArray msgArr;
    for (const auto& m : messages) {
        QJsonObject o;
        o["role"] = m.role;
        o["content"] = m.content;
        msgArr.append(o);
    }
    QJsonObject body;
    body["model"] = model.isEmpty() ? "gpt-4o-mini" : model;
    body["messages"] = msgArr;
    body["temperature"] = 0.7;
    body["stream"] = false;

    QJsonDocument doc(body);
    QByteArray payload = doc.toJson(QJsonDocument::Compact);
    m_nam->post(req, payload);
}

void OpenAIProvider::onFinished(QNetworkReply* reply) {
    reply->deleteLater();
    if (reply->error() != QNetworkReply::NoError) {
        emit errorOccurred(QString("网络错误：%1").arg(reply->errorString()));
        return;
    }
    QByteArray data = reply->readAll();
    QJsonDocument doc = QJsonDocument::fromJson(data);
    if (doc.isNull() || !doc.isObject()) {
        emit errorOccurred("返回数据解析失败（非 JSON）。");
        return;
    }
    QJsonObject root = doc.object();
    if (root.contains("error")) {
        QJsonObject err = root["error"].toObject();
        emit errorOccurred("API 错误：" + err["message"].toString());
        return;
    }
    QJsonArray choices = root["choices"].toArray();
    if (choices.isEmpty()) {
        emit errorOccurred("返回为空（无 choices）。");
        return;
    }
    QJsonObject msg = choices[0].toObject()["message"].toObject();
    QString content = msg["content"].toString();
    emit finished(content);
}
