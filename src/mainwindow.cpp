#include "mainwindow.h"
#include "ui/editorwidget.h"
#include "ui/aipanel.h"
#include "io/docxreader.h"
#include "io/docxwriter.h"
#include "core/ziputil.h"
#include <QMenuBar>
#include <QMenu>
#include <QFileDialog>
#include <QMessageBox>
#include <QFileInfo>
#include <QTextStream>
#include <QApplication>

MainWindow::MainWindow(QWidget* parent) : QMainWindow(parent) {
    setWindowTitle("OpenOffice — 开源 Word 编辑器");
    resize(1100, 760);

    m_editor = new EditorWidget(this);
    setCentralWidget(m_editor);

    m_aiPanel = new AIPanel(m_editor, this);
    addDockWidget(Qt::RightDockWidgetArea, m_aiPanel);

    // ---- menus ----
    auto* fileMenu = menuBar()->addMenu("文件(&F)");
    auto* actNew = fileMenu->addAction("新建(&N)");
    connect(actNew, &QAction::triggered, this, &MainWindow::onNew);
    auto* actOpen = fileMenu->addAction("打开…(&O)");
    connect(actOpen, &QAction::triggered, this, &MainWindow::onOpen);
    auto* actSave = fileMenu->addAction("保存(&S)");
    connect(actSave, &QAction::triggered, this, &MainWindow::onSave);
    auto* actSaveAs = fileMenu->addAction("另存为…(&A)");
    connect(actSaveAs, &QAction::triggered, this, &MainWindow::onSaveAs);
    fileMenu->addSeparator();
    auto* actQuit = fileMenu->addAction("退出(&X)");
    connect(actQuit, &QAction::triggered, this, []() { QApplication::quit(); });

    auto* viewMenu = menuBar()->addMenu("视图(&V)");
    viewMenu->addAction(m_aiPanel->toggleViewAction());

    auto* helpMenu = menuBar()->addMenu("帮助(&H)");
    auto* actAbout = helpMenu->addAction("关于");
    connect(actAbout, &QAction::triggered, this, &MainWindow::onAbout);

    // a default empty document
    OOXML::Document d;
    OOXML::Paragraph p;
    OOXML::Run r;
    r.text = "欢迎使用 OpenOffice——一个基于 Qt 的开源 Word 文档编辑器。\n"
             "打开 .docx 即可编辑，保存为合法 OOXML。右侧面板可接入 OpenAI 兼容大模型对选中文本进行改写、润色、总结、翻译与扩写。";
    p.runs.append(r);
    d.addParagraph(p);
    m_editor->setModel(d);
}

void MainWindow::onNew() {
    OOXML::Document d;
    m_editor->setModel(d);
    m_currentFile.clear();
}

void MainWindow::onOpen() {
    QString path = QFileDialog::getOpenFileName(this, "打开 Word 文档",
                                               QString(), "Word 文档 (*.docx);;所有文件 (*.*)");
    if (path.isEmpty()) return;
    loadFile(path);
}

void MainWindow::loadFile(const QString& path) {
    OOXML::Document doc;
    if (!DocxReader::read(path, doc)) {
        QMessageBox::warning(this, "打开失败", "无法读取该文件，可能不是有效的 .docx 文档。");
        return;
    }
    m_editor->setModel(doc);
    m_currentFile = path;
    setWindowTitle(QString("OpenOffice — %1").arg(QFileInfo(path).fileName()));
}

bool MainWindow::onSave() {
    if (m_currentFile.isEmpty())
        return onSaveAs();
    return saveFile(m_currentFile);
}

bool MainWindow::onSaveAs() {
    QString path = QFileDialog::getSaveFileName(this, "保存 Word 文档",
                                               QString(), "Word 文档 (*.docx)");
    if (path.isEmpty()) return false;
    if (!path.endsWith(".docx", Qt::CaseInsensitive)) path += ".docx";
    return saveFile(path);
}

bool MainWindow::saveFile(const QString& path) {
    OOXML::Document doc = m_editor->toModel();
    doc.creator = "OpenOffice";
    if (!DocxWriter::write(path, doc)) {
        QMessageBox::warning(this, "保存失败", "写入文件时出错。");
        return false;
    }
    m_currentFile = path;
    setWindowTitle(QString("OpenOffice — %1").arg(QFileInfo(path).fileName()));
    return true;
}

void MainWindow::onAbout() {
    QMessageBox::about(this, "关于 OpenOffice",
        "OpenOffice — 开源 Word 文档编辑器\n\n"
        "基于 Qt 6 与自研 OOXML 引擎，可打开/编辑/保存 .docx，\n"
        "并接入 OpenAI 兼容大模型实现 AI 自动编辑。\n\n"
        "许可证：MIT");
}
