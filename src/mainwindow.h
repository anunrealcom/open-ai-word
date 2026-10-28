#pragma once
#include <QMainWindow>
class EditorWidget;
class AIPanel;

class MainWindow : public QMainWindow {
    Q_OBJECT
public:
    explicit MainWindow(QWidget* parent = nullptr);

private slots:
    void onNew();
    void onOpen();
    bool onSave();
    bool onSaveAs();
    void onAbout();

private:
    void loadFile(const QString& path);
    bool saveFile(const QString& path);

    EditorWidget* m_editor;
    AIPanel* m_aiPanel;
    QString m_currentFile;
};
