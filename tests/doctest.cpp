// Engine round-trip test: build an OOXML model (heading, formatted paragraph,
// table), write to .docx, read it back, and verify content survived.
#include "core/ooxmlmodel.h"
#include "io/docxreader.h"
#include "io/docxwriter.h"
#include <QCoreApplication>
#include <QDebug>
#include <QTemporaryDir>

int main(int argc, char* argv[]) {
    QCoreApplication app(argc, argv);

    OOXML::Document doc;

    OOXML::Paragraph h;
    h.fmt.isHeading = true; h.fmt.headingLevel = 1; h.fmt.styleId = "Heading1";
    OOXML::Run hr; hr.text = "第一章 引言"; hr.fmt.bold = true;
    h.runs.append(hr);
    doc.addParagraph(h);

    OOXML::Paragraph p;
    p.fmt.align = OOXML::Align::Center;
    OOXML::Run r1; r1.text = "这是"; r1.fmt.italic = true; p.runs.append(r1);
    OOXML::Run r2; r2.text = "加粗文本"; r2.fmt.bold = true; r2.fmt.fontSizePt = 14; p.runs.append(r2);
    doc.addParagraph(p);

    OOXML::Table t;
    OOXML::TableRow row;
    OOXML::TableCell c1, c2;
    OOXML::Paragraph cp1, cp2;
    OOXML::Run cr1; cr1.text = "姓名"; cp1.runs.append(cr1); c1.paragraphs.append(cp1);
    OOXML::Run cr2; cr2.text = "年龄"; cp2.runs.append(cr2); c2.paragraphs.append(cp2);
    row.cells.append(c1); row.cells.append(c2);
    t.rows.append(row);
    doc.addTable(t);

    QTemporaryDir dir;
    QString path = dir.filePath("test.docx");
    if (!DocxWriter::write(path, doc)) { qDebug() << "WRITE FAIL"; return 1; }

    OOXML::Document doc2;
    if (!DocxReader::read(path, doc2)) { qDebug() << "READ FAIL"; return 1; }

    bool ok = true;
    int paraCount = 0, tableCount = 0;
    for (const auto& b : doc2.blocks) {
        if (b.type == OOXML::Block::ParagraphBlock) paraCount++;
        else if (b.type == OOXML::Block::TableBlock) tableCount++;
    }
    if (paraCount != 2) { qDebug() << "paraCount =" << paraCount; ok = false; }
    if (tableCount != 1) { qDebug() << "tableCount =" << tableCount; ok = false; }

    QString allText;
    for (const auto& b : doc2.blocks)
        if (b.type == OOXML::Block::ParagraphBlock)
            for (const auto& r : b.paragraph.runs) allText += r.text;
    if (!allText.contains("第一章")) { qDebug() << "missing heading"; ok = false; }
    if (!allText.contains("加粗文本")) { qDebug() << "missing bold run"; ok = false; }

    bool hasName = false, hasAge = false;
    for (const auto& b : doc2.blocks)
        if (b.type == OOXML::Block::TableBlock)
            for (const auto& row2 : b.table.rows)
                for (const auto& cell : row2.cells)
                    for (const auto& p2 : cell.paragraphs)
                        for (const auto& r : p2.runs) {
                            if (r.text == "姓名") hasName = true;
                            if (r.text == "年龄") hasAge = true;
                        }
    if (!hasName || !hasAge) { qDebug() << "table cell missing"; ok = false; }

    qDebug() << "doc round-trip =>" << (ok ? "PASS" : "FAIL");
    return ok ? 0 : 1;
}
