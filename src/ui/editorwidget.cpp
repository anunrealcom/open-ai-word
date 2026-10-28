#include "editorwidget.h"
#include <QTextBlock>
#include <QTextBlockFormat>
#include <QTextCharFormat>
#include <QTextCursor>
#include <QTextTable>
#include <QTextDocument>

EditorWidget::EditorWidget(QWidget* parent) : QTextEdit(parent) {
    setAcceptRichText(true);
    setLineWrapMode(QTextEdit::WidgetWidth);
}

// ---------------- character format mapping ----------------
QTextCharFormat EditorWidget::runToCharFormat(const OOXML::RunFormat& f) const {
    QTextCharFormat cf;
    cf.setFontWeight(f.bold ? QFont::Bold : QFont::Normal);
    cf.setFontItalic(f.italic);
    cf.setFontUnderline(f.underline);
    cf.setFontStrikeOut(f.strike);
    if (f.fontSizePt > 0) cf.setFontPointSize(f.fontSizePt);
    if (!f.fontFamily.isEmpty()) cf.setFontFamily(f.fontFamily);
    if (f.color.isValid()) cf.setForeground(f.color);
    return cf;
}

OOXML::RunFormat EditorWidget::charFormatToRun(const QTextCharFormat& cf) const {
    OOXML::RunFormat f;
    f.bold = (cf.fontWeight() == QFont::Bold);
    f.italic = cf.fontItalic();
    f.underline = cf.fontUnderline();
    f.strike = cf.fontStrikeOut();
    f.fontSizePt = cf.fontPointSize();
    if (f.fontSizePt <= 0) f.fontSizePt = 11.0;
    f.fontFamily = cf.fontFamily();
    if (cf.hasProperty(QTextFormat::ForegroundBrush) && cf.foreground().color().isValid())
        f.color = cf.foreground().color();
    return f;
}

// ---------------- model -> editor ----------------
void EditorWidget::setModel(const OOXML::Document& doc) {
    QTextDocument* d = new QTextDocument(this);
    d->setDocumentMargin(24);
    QTextCursor c(d);
    bool first = true;

    for (const auto& b : doc.blocks) {
        if (!first) c.insertBlock();
        first = false;

        if (b.type == OOXML::Block::ParagraphBlock) {
            const auto& p = b.paragraph;
            for (const auto& run : p.runs) {
                c.setCharFormat(runToCharFormat(run.fmt));
                c.insertText(run.text);
            }
            QTextBlockFormat bf;
            switch (p.fmt.align) {
            case OOXML::Align::Center: bf.setAlignment(Qt::AlignHCenter); break;
            case OOXML::Align::Right: bf.setAlignment(Qt::AlignRight); break;
            case OOXML::Align::Justify: bf.setAlignment(Qt::AlignJustify); break;
            default: bf.setAlignment(Qt::AlignLeft); break;
            }
            bf.setTopMargin(p.fmt.spaceBeforePt * 1.0);
            bf.setBottomMargin(p.fmt.spaceAfterPt * 1.0);
            if (p.fmt.isHeading && p.fmt.headingLevel > 0) {
                bf.setProperty(HeadingProp, p.fmt.headingLevel);
                QTextCharFormat hcf;
                double size = 22.0 - p.fmt.headingLevel * 2.0;
                hcf.setFontPointSize(size);
                hcf.setFontWeight(QFont::Bold);
                bf.setTopMargin(12);
                c.setBlockFormat(bf);
                c.setBlockCharFormat(hcf);
            } else {
                c.setBlockFormat(bf);
            }
        } else if (b.type == OOXML::Block::TableBlock) {
            const auto& t = b.table;
            int rows = t.rows.size();
            int cols = 0;
            for (const auto& row : t.rows) cols = qMax(cols, row.cells.size());
            if (rows > 0 && cols > 0) {
                QTextTable* qt = c.insertTable(rows, cols);
                qt->setFormat(QTextTableFormat());
                for (int r = 0; r < rows; r++) {
                    for (int col = 0; col < t.rows[r].cells.size(); col++) {
                        const auto& cell = t.rows[r].cells[col];
                        QTextTableCell qc = qt->cellAt(r, col);
                        QTextCursor cc = qc.firstCursorPosition();
                        QString cellText;
                        QTextCharFormat firstFmt;
                        for (const auto& p : cell.paragraphs)
                            for (const auto& run : p.runs) {
                                cellText += run.text;
                                if (!run.text.isEmpty() && firstFmt.fontPointSize() <= 0)
                                    firstFmt = runToCharFormat(run.fmt);
                            }
                        cc.insertText(cellText, firstFmt);
                    }
                }
            }
        }
    }

    setDocument(d);
}

// ---------------- editor -> model ----------------
OOXML::Document EditorWidget::toModel() const {
    OOXML::Document doc;
    const QTextDocument* d = document();
    if (!d) return doc;
    QTextFrame* root = d->rootFrame();

    auto blockToParagraph = [](QTextBlock blk) -> OOXML::Paragraph {
        OOXML::Paragraph p;
        QTextBlockFormat bf = blk.blockFormat();
        Qt::Alignment al = bf.alignment();
        if (al & Qt::AlignHCenter) p.fmt.align = OOXML::Align::Center;
        else if (al & Qt::AlignRight) p.fmt.align = OOXML::Align::Right;
        else if (al & Qt::AlignJustify) p.fmt.align = OOXML::Align::Justify;
        else p.fmt.align = OOXML::Align::Left;
        p.fmt.spaceBeforePt = bf.topMargin();
        p.fmt.spaceAfterPt = bf.bottomMargin();
        QVariant h = bf.property(HeadingProp);
        if (h.isValid()) {
            p.fmt.isHeading = true;
            p.fmt.headingLevel = h.toInt();
            p.fmt.styleId = QString("Heading%1").arg(p.fmt.headingLevel);
        }
        for (QTextBlock::iterator it = blk.begin(); !it.atEnd(); ++it) {
            QTextFragment frag = it.fragment();
            if (!frag.isValid() || frag.text().isEmpty()) continue;
            OOXML::Run run;
            run.text = frag.text();
            QTextCharFormat cf = frag.charFormat();
            run.fmt.bold = (cf.fontWeight() == QFont::Bold);
            run.fmt.italic = cf.fontItalic();
            run.fmt.underline = cf.fontUnderline();
            run.fmt.strike = cf.fontStrikeOut();
            run.fmt.fontSizePt = cf.fontPointSize() > 0 ? cf.fontPointSize() : 11.0;
            run.fmt.fontFamily = cf.fontFamily();
            if (cf.hasProperty(QTextFormat::ForegroundBrush))
                run.fmt.color = cf.foreground().color();
            p.runs.append(run);
        }
        if (p.runs.isEmpty()) {
            OOXML::Run run; run.text.clear();
            p.runs.append(run);
        }
        return p;
    };

    for (QTextFrame::iterator it = root->begin(); !it.atEnd(); ++it) {
        QTextFrame* f = it.currentFrame();
        if (f && f != root) {
            QTextTable* t = qobject_cast<QTextTable*>(f);
            if (t) {
                OOXML::Table tbl;
                for (int r = 0; r < t->rows(); r++) {
                    tbl.rows.append(OOXML::TableRow());
                    for (int col = 0; col < t->columns(); col++) {
                        QTextTableCell qc = t->cellAt(r, col);
                        OOXML::TableCell cell;
                        cell.colSpan = qc.columnSpan();
                        QTextCursor cc = qc.firstCursorPosition();
                        QTextBlock blk = cc.block();
                        cell.paragraphs.append(blockToParagraph(blk));
                        tbl.rows.last().cells.append(cell);
                    }
                }
                doc.addTable(tbl);
            }
        } else {
            QTextBlock blk = it.currentBlock();
            if (blk.isValid() && !blk.text().isNull()) {
                // skip a stray empty block that may precede a leading table
                if (blk.text().isEmpty()) {
                    bool hasContent = false;
                    for (QTextBlock::iterator fi = blk.begin(); !fi.atEnd(); ++fi)
                        if (fi.fragment().isValid() && !fi.fragment().text().isEmpty())
                            hasContent = true;
                    if (!hasContent) continue;
                }
                doc.addParagraph(blockToParagraph(blk));
            }
        }
    }
    return doc;
}

// ---------------- AI helpers ----------------
QString EditorWidget::selectedText() const {
    QTextCursor c = textCursor();
    return c.selectedText();
}

void EditorWidget::replaceSelection(const QString& text) {
    QTextCursor c = textCursor();
    if (c.hasSelection())
        c.insertText(text);
    else
        c.insertText(text);
    setTextCursor(c);
}

QString EditorWidget::documentText() const {
    return document() ? document()->toPlainText() : QString();
}
