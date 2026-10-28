#include "aipanel.h"
#include "ai/aimanager.h"
#include <QVBoxLayout>
#include <QHBoxLayout>
#include <QFormLayout>
#include <QLineEdit>
#include <QTextEdit>
#include <QPushButton>
#include <QLabel>
#include <QGroupBox>
#include <QScrollArea>

AIPanel::AIPanel(EditorWidget* editor, QWidget* parent)
    : QDockWidget("AI 助手", parent), m_editor(editor) {
    setAllowedAreas(Qt::LeftDockWidgetArea | Qt::RightDockWidgetArea);

    auto* root = new QWidget(this);
    auto* mainLayout = new QVBoxLayout(root);

    // ---- configuration ----
    auto* cfgBox = new QGroupBox("模型配置（OpenAI 兼容）");
    auto* cfgForm = new QFormLayout(cfgBox);
    m_baseUrl = new QLineEdit(AIManager::instance().baseUrl());
    m_apiKey = new QLineEdit(AIManager::instance().apiKey());
    m_apiKey->setEchoMode(QLineEdit::Password);
    m_model = new QLineEdit(AIManager::instance().model());
    cfgForm->addRow("Base URL", m_baseUrl);
    cfgForm->addRow("API Key", m_apiKey);
    cfgForm->addRow("模型", m_model);
    connect(m_baseUrl, &QLineEdit::editingFinished, this, &AIPanel::onConfigureChanged);
    connect(m_apiKey, &QLineEdit::editingFinished, this, &AIPanel::onConfigureChanged);
    connect(m_model, &QLineEdit::editingFinished, this, &AIPanel::onConfigureChanged);
    mainLayout->addWidget(cfgBox);

    // ---- text operations on selection ----
    auto* opBox = new QGroupBox("选中文本操作");
    auto* opLayout = new QVBoxLayout(opBox);
    QStringList ops = {"改写", "润色", "总结", "翻译(中→英)", "扩写"};
    auto* btnRow = new QHBoxLayout();
    for (const QString& op : ops) {
        auto* b = new QPushButton(op);
        connect(b, &QPushButton::clicked, this, [this, op]() { runOperation(op); });
        btnRow->addWidget(b);
    }
    opLayout->addLayout(btnRow);

    m_result = new QTextEdit();
    m_result->setPlaceholderText("AI 结果将显示在这里…");
    m_result->setMinimumHeight(120);
    opLayout->addWidget(m_result);

    auto* actRow = new QHBoxLayout();
    auto* replaceBtn = new QPushButton("替换选中");
    auto* insertBtn = new QPushButton("插入到光标");
    connect(replaceBtn, &QPushButton::clicked, this, &AIPanel::onReplaceClicked);
    connect(insertBtn, &QPushButton::clicked, this, &AIPanel::onInsertClicked);
    actRow->addWidget(replaceBtn);
    actRow->addWidget(insertBtn);
    opLayout->addLayout(actRow);
    mainLayout->addWidget(opBox);

    // ---- document chat ----
    auto* chatBox = new QGroupBox("文档对话");
    auto* chatLayout = new QVBoxLayout(chatBox);
    m_chatLog = new QTextEdit();
    m_chatLog->setReadOnly(true);
    m_chatLog->setPlaceholderText("基于整篇文档提问，例如：这篇文档的要点是什么？");
    m_chatLog->setMinimumHeight(120);
    chatLayout->addWidget(m_chatLog);
    m_chatInput = new QLineEdit();
    m_chatInput->setPlaceholderText("输入问题后回车…");
    connect(m_chatInput, &QLineEdit::returnPressed, this, &AIPanel::onSendChat);
    auto* sendBtn = new QPushButton("发送");
    connect(sendBtn, &QPushButton::clicked, this, &AIPanel::onSendChat);
    auto* chatRow = new QHBoxLayout();
    chatRow->addWidget(m_chatInput);
    chatRow->addWidget(sendBtn);
    chatLayout->addLayout(chatRow);
    mainLayout->addWidget(chatBox);

    m_status = new QLabel("就绪");
    mainLayout->addWidget(m_status);
    mainLayout->addStretch(1);

    setWidget(root);

    connect(&AIManager::instance(), &AIManager::finished, this, &AIPanel::onAIFinished);
    connect(&AIManager::instance(), &AIManager::errorOccurred, this, &AIPanel::onAIError);
}

void AIPanel::onConfigureChanged() {
    AIManager::instance().configure(m_baseUrl->text(), m_apiKey->text(), m_model->text());
    m_status->setText("配置已保存");
}

QList<AIMessage> AIPanel::buildMessages(const QString& system, const QString& user) const {
    QList<AIMessage> msgs;
    AIMessage s; s.role = "system"; s.content = system;
    AIMessage u; u.role = "user"; u.content = user;
    msgs.append(s); msgs.append(u);
    return msgs;
}

void AIPanel::runOperation(const QString& op) {
    if (m_busy) return;
    QString text = m_editor->selectedText();
    if (text.trimmed().isEmpty()) {
        m_status->setText("请先在编辑区选中一段文字");
        return;
    }
    QString system;
    if (op == "改写") system = "你是一个中文写作助手。请在不改变原意的前提下重写下面的文本，使其更通顺、自然、连贯。只输出改写后的文本。";
    else if (op == "润色") system = "请润色下面的文本，修正错别字和语法问题，提升表达质量，保持原意和长度大致不变。只输出润色后的文本。";
    else if (op == "总结") system = "请用简洁的中文总结下面文本的核心要点，用分条列表呈现。";
    else if (op == "翻译(中→英)") system = "请将下面的中文文本翻译成英文，只输出英文译文，不要解释。";
    else if (op == "扩写") system = "请在不改变原意的前提下扩写下面的文本，增加细节、例子和论据，使内容更充实。只输出扩写后的文本。";

    m_busy = true;
    m_status->setText(QString("正在调用 %1 …").arg(op));
    AIManager::instance().chat(buildMessages(system, text));
}

void AIPanel::onAIFinished(const QString& content) {
    m_busy = false;
    m_pendingResult = content.trimmed();
    m_result->setPlainText(m_pendingResult);
    m_status->setText("完成");
}

void AIPanel::onAIError(const QString& message) {
    m_busy = false;
    m_status->setText(message);
}

void AIPanel::onReplaceClicked() {
    if (m_pendingResult.isEmpty()) return;
    m_editor->replaceSelection(m_pendingResult);
    m_status->setText("已替换选中内容");
}

void AIPanel::onInsertClicked() {
    if (m_pendingResult.isEmpty()) return;
    m_editor->replaceSelection(m_pendingResult); // inserts at cursor if no selection
    m_status->setText("已插入到光标处");
}

void AIPanel::onSendChat() {
    if (m_busy) return;
    QString question = m_chatInput->text().trimmed();
    if (question.isEmpty()) return;
    QString docText = m_editor->documentText();
    if (docText.trimmed().isEmpty()) {
        m_status->setText("文档为空，无法对话");
        return;
    }
    QString system = "你是文档助手。请基于用户提供的文档内容回答问题，不要编造文档之外的信息。如果文档中没有相关信息，请明确说明。";
    QString user = "文档内容如下：\n\"\"\"\n" + docText.left(12000) + "\n\"\"\"\n\n问题：" + question;

    m_chatLog->append("你：" + question);
    m_busy = true;
    m_status->setText("正在思考…");
    m_chatInput->clear();
    // use a connection that routes this answer into the chat log
    auto* mgr = &AIManager::instance();
    auto* conn = new QMetaObject::Connection;
    *conn = connect(mgr, &AIManager::finished, this, [this, conn](const QString& content) {
        m_chatLog->append("AI：" + content);
        m_status->setText("就绪");
        m_busy = false;
        disconnect(*conn);
        delete conn;
    });
    auto* errConn = new QMetaObject::Connection;
    *errConn = connect(mgr, &AIManager::errorOccurred, this, [this, errConn](const QString& msg) {
        m_chatLog->append("AI：错误 - " + msg);
        m_status->setText(msg);
        m_busy = false;
        disconnect(*errConn);
        delete errConn;
    });
    mgr->chat(buildMessages(system, user));
}
