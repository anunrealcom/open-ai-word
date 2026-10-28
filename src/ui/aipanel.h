#pragma once
#include <QDockWidget>
#include <QList>
#include "ai/aiprovider.h"
#include "ui/editorwidget.h"

class AIManager;
class QLineEdit;
class QTextEdit;
class QPushButton;
class QLabel;

// AI editing panel: configure provider, run text operations on the current
// selection, and chat with the whole document.
class AIPanel : public QDockWidget {
    Q_OBJECT
public:
    explicit AIPanel(EditorWidget* editor, QWidget* parent = nullptr);

private slots:
    void onConfigureChanged();
    void runOperation(const QString& op);
    void onAIFinished(const QString& content);
    void onAIError(const QString& message);
    void onReplaceClicked();
    void onInsertClicked();
    void onSendChat();

private:
    QList<AIMessage> buildMessages(const QString& system, const QString& user) const;

    EditorWidget* m_editor;
    QLineEdit* m_baseUrl;
    QLineEdit* m_apiKey;
    QLineEdit* m_model;
    QTextEdit* m_result;
    QTextEdit* m_chatLog;
    QLineEdit* m_chatInput;
    QLabel* m_status;
    QString m_pendingResult;
    bool m_busy = false;
};
