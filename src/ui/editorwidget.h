#pragma once
// Editing surface. Uses QTextDocument as the rich-text editing buffer and
// converts to/from the OOXML::Document model. (The OOXML engine owns the model;
// QTextEdit owns the view. This class bridges them.)
#include "core/ooxmlmodel.h"
#include <QTextEdit>

class EditorWidget : public QTextEdit {
    Q_OBJECT
public:
    explicit EditorWidget(QWidget* parent = nullptr);

    // Load the OOXML model into the editor.
    void setModel(const OOXML::Document& doc);
    // Serialize the current editor content back into an OOXML model.
    OOXML::Document toModel() const;

    // --- AI helpers ---
    QString selectedText() const;
    void replaceSelection(const QString& text);
    QString documentText() const;

private:
    static constexpr int HeadingProp = QTextFormat::UserProperty + 1;

    QTextCharFormat runToCharFormat(const OOXML::RunFormat& f) const;
    OOXML::RunFormat charFormatToRun(const QTextCharFormat& cf) const;
};
