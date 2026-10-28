#include "docxreader.h"
#include "core/ziputil.h"
#include <QXmlStreamReader>
#include <QMap>
#include <QDebug>

namespace {

void parseStyles(const QByteArray& xml, OOXML::Document& doc);
bool parseDocument(const QByteArray& xml, OOXML::Document& doc);

// Read an attribute by local name (ignoring namespace prefix).
QString attrVal(QXmlStreamReader& r, const QString& local) {
    const auto& attrs = r.attributes();
    for (const auto& a : attrs)
        if (a.name() == local)
            return a.value().toString();
    return QString();
}

// Parse w:val of a boolean-ish property ("true"/"1"/"single" => true,
// "false"/"0"/"none"/"off" => false). Absent => defaultArg.
bool boolAttr(QXmlStreamReader& r, bool defaultArg) {
    QString v = attrVal(r, "val").toLower();
    if (v.isEmpty()) return defaultArg;
    if (v == "false" || v == "0" || v == "none" || v == "off") return false;
    if (v == "true" || v == "1" || v == "single") return true;
    return defaultArg;
}

QColor parseHexColor(const QString& hex) {
    if (hex.isEmpty() || hex == "auto") return QColor();
    QString h = hex;
    if (h.startsWith('#')) h = h.mid(1);
    if (h.size() == 6)
        return QColor(h);
    return QColor();
}

OOXML::Align parseAlign(const QString& v) {
    if (v == "center") return OOXML::Align::Center;
    if (v == "right") return OOXML::Align::Right;
    if (v == "both" || v == "justify") return OOXML::Align::Justify;
    return OOXML::Align::Left;
}
} // namespace

bool DocxReader::read(const QString& path, OOXML::Document& doc) {
    doc.clear();
    QMap<QString, QByteArray> entries;
    if (!ZipUtil::readZip(path, entries)) {
        qWarning() << "DocxReader: failed to read zip" << path;
        return false;
    }

    // --- styles.xml ---
    if (entries.contains("word/styles.xml")) {
        parseStyles(entries.value("word/styles.xml"), doc);
    }

    // --- document.xml ---
    QByteArray docXml = entries.value("word/document.xml");
    if (docXml.isEmpty()) {
        qWarning() << "DocxReader: word/document.xml missing";
        return false;
    }
    return parseDocument(docXml, doc);
}

// --- styles parsing ---
namespace {
void parseStyles(const QByteArray& xml, OOXML::Document& doc) {
    QXmlStreamReader r(xml);
    OOXML::StyleDef cur;
    bool inStyle = false;
    bool inRPr = false;
    bool inPPr = false;

    while (!r.atEnd()) {
        r.readNext();
        if (r.isStartElement()) {
            QString n = r.name().toString();
            if (n == "style") {
                cur = OOXML::StyleDef();
                cur.id = attrVal(r, "styleId");
                cur.isParagraphStyle = (attrVal(r, "type") != "character");
                inStyle = true;
            } else if (inStyle && n == "name") {
                cur.name = attrVal(r, "val");
                QString ln = cur.name.toLower();
                if (ln.startsWith("heading") || ln.startsWith("标题")) {
                    cur.parFmt.isHeading = true;
                    // Heading1..Heading6
                    bool ok = false;
                    int lvl = cur.name.mid(7).toInt(&ok);
                    if (ok) cur.parFmt.headingLevel = lvl;
                }
            } else if (inStyle && n == "pPr") {
                inPPr = true;
            } else if (inStyle && n == "rPr") {
                inRPr = true;
            } else if (inPPr && n == "jc") {
                cur.parFmt.align = parseAlign(attrVal(r, "val"));
            } else if (inPPr && n == "spacing") {
                QString b = attrVal(r, "before");
                QString a = attrVal(r, "after");
                QString l = attrVal(r, "line");
                if (!b.isEmpty()) cur.parFmt.spaceBeforePt = b.toDouble() / 20.0;
                if (!a.isEmpty()) cur.parFmt.spaceAfterPt = a.toDouble() / 20.0;
                if (!l.isEmpty()) cur.parFmt.lineSpacingPt = l.toDouble() / 240.0; // 240 = single
            } else if (inRPr && n == "b") {
                cur.runFmt.bold = boolAttr(r, true);
            } else if (inRPr && n == "i") {
                cur.runFmt.italic = boolAttr(r, true);
            } else if (inRPr && n == "u") {
                cur.runFmt.underline = boolAttr(r, true);
            } else if (inRPr && n == "strike") {
                cur.runFmt.strike = boolAttr(r, true);
            } else if (inRPr && n == "sz") {
                QString v = attrVal(r, "val");
                if (!v.isEmpty()) cur.runFmt.fontSizePt = v.toDouble() / 2.0;
            } else if (inRPr && n == "rFonts") {
                QString f = attrVal(r, "ascii");
                if (f.isEmpty()) f = attrVal(r, "hAnsi");
                if (!f.isEmpty()) cur.runFmt.fontFamily = f;
            } else if (inRPr && n == "color") {
                cur.runFmt.color = parseHexColor(attrVal(r, "val"));
            }
        } else if (r.isEndElement()) {
            QString n = r.name().toString();
            if (n == "style" && inStyle) {
                doc.styles.insert(cur.id, cur);
                inStyle = false;
            } else if (n == "pPr") {
                inPPr = false;
            } else if (n == "rPr") {
                inRPr = false;
            }
        }
    }
}

// --- document parsing ---
bool parseDocument(const QByteArray& xml, OOXML::Document& doc) {
    QXmlStreamReader r(xml);
    OOXML::Paragraph* curPar = nullptr;
    OOXML::Run* curRun = nullptr;
    OOXML::Table* curTable = nullptr;
    OOXML::TableRow* curRow = nullptr;
    OOXML::TableCell* curCell = nullptr;
    bool inRPr = false;
    bool inPPr = false;
    bool inRun = false;
    bool inPar = false;

    auto flushParagraph = [&]() {
        if (curPar) {
            doc.addParagraph(*curPar);
            delete curPar;
            curPar = nullptr;
        }
    };
    auto flushTable = [&]() {
        if (curTable) {
            doc.addTable(*curTable);
            delete curTable;
            curTable = nullptr;
            curRow = nullptr;
            curCell = nullptr;
        }
    };

    while (!r.atEnd()) {
        r.readNext();
        if (r.isStartElement()) {
            QString n = r.name().toString();
            if (n == "p" && !curCell) {
                flushTable(); // paragraphs and tables are siblings in body
                curPar = new OOXML::Paragraph();
                inPar = true;
            } else if (n == "pPr" && curPar) {
                inPPr = true;
            } else if (n == "rPr" && (curRun || curPar)) {
                inRPr = true;
                if (!curRun) curRun = new OOXML::Run();
            } else if (n == "r" && curPar) {
                curRun = new OOXML::Run();
                inRun = true;
            } else if (n == "t" && curRun) {
                curRun->text.append(r.readElementText());
            } else if (n == "tab" && curRun) {
                curRun->text.append("\t");
            } else if (n == "br" && curRun) {
                curRun->text.append("\n");
            } else if (n == "jc" && inPPr && curPar) {
                curPar->fmt.align = parseAlign(attrVal(r, "val"));
            } else if (n == "pStyle" && inPPr && curPar) {
                curPar->fmt.styleId = attrVal(r, "val");
            } else if (n == "spacing" && inPPr && curPar) {
                QString b = attrVal(r, "before");
                QString a = attrVal(r, "after");
                if (!b.isEmpty()) curPar->fmt.spaceBeforePt = b.toDouble() / 20.0;
                if (!a.isEmpty()) curPar->fmt.spaceAfterPt = a.toDouble() / 20.0;
            } else if (inRPr && n == "b") {
                curRun->fmt.bold = boolAttr(r, true);
            } else if (inRPr && n == "i") {
                curRun->fmt.italic = boolAttr(r, true);
            } else if (inRPr && n == "u") {
                curRun->fmt.underline = boolAttr(r, true);
            } else if (inRPr && n == "strike") {
                curRun->fmt.strike = boolAttr(r, true);
            } else if (inRPr && n == "sz") {
                QString v = attrVal(r, "val");
                if (!v.isEmpty()) curRun->fmt.fontSizePt = v.toDouble() / 2.0;
            } else if (inRPr && n == "rFonts") {
                QString f = attrVal(r, "ascii");
                if (f.isEmpty()) f = attrVal(r, "hAnsi");
                if (!f.isEmpty()) curRun->fmt.fontFamily = f;
            } else if (inRPr && n == "color") {
                curRun->fmt.color = parseHexColor(attrVal(r, "val"));
            }
            // --- tables ---
            else if (n == "tbl") {
                flushParagraph();
                curTable = new OOXML::Table();
            } else if (n == "tr" && curTable) {
                curTable->rows.append(OOXML::TableRow());
                curRow = &curTable->rows.last();
            } else if (n == "tc" && curRow) {
                curRow->cells.append(OOXML::TableCell());
                curCell = &curRow->cells.last();
            } else if (n == "gridSpan" && curCell) {
                QString v = attrVal(r, "val");
                if (!v.isEmpty()) curCell->colSpan = v.toInt();
            } else if (n == "p" && curCell) {
                // paragraph inside a cell
                curPar = new OOXML::Paragraph();
                inPar = true;
            }
        } else if (r.isEndElement()) {
            QString n = r.name().toString();
            if (n == "rPr") {
                inRPr = false;
            } else if (n == "pPr") {
                inPPr = false;
            } else if (n == "r" && inRun) {
                if (curPar && curRun) curPar->runs.append(*curRun);
                delete curRun;
                curRun = nullptr;
                inRun = false;
            } else if (n == "p") {
                if (curCell && curPar) {
                    curCell->paragraphs.append(*curPar);
                    delete curPar;
                    curPar = nullptr;
                } else if (curPar && !curCell) {
                    flushParagraph();
                }
                inPar = false;
            } else if (n == "tc") {
                curCell = nullptr;
            } else if (n == "tr") {
                curRow = nullptr;
            } else if (n == "tbl") {
                flushTable();
            }
        }
    }

    if (r.hasError()) {
        qWarning() << "DocxReader: XML parse error:" << r.errorString();
    }
    return true;
}

} // anonymous namespace

QString DocxReader::toPlainText(const OOXML::Document& doc) {
    QString out;
    for (const auto& b : doc.blocks) {
        if (b.type == OOXML::Block::ParagraphBlock) {
            for (const auto& run : b.paragraph.runs)
                out.append(run.text);
            out.append("\n");
        } else if (b.type == OOXML::Block::TableBlock) {
            for (const auto& row : b.table.rows) {
                QStringList cells;
                for (const auto& cell : row.cells) {
                    QString c;
                    for (const auto& p : cell.paragraphs)
                        for (const auto& run : p.runs) c.append(run.text);
                    cells.append(c);
                }
                out.append(cells.join(" | ")).append("\n");
            }
        }
    }
    return out;
}
