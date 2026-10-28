#include "docxwriter.h"
#include "core/ziputil.h"
#include <QMap>
#include <QDateTime>

namespace {
QString xmlEscape(const QString& s) {
    QString o;
    o.reserve(s.size() + 16);
    for (QChar c : s) {
        switch (c.unicode()) {
        case '&': o += "&amp;"; break;
        case '<': o += "&lt;"; break;
        case '>': o += "&gt;"; break;
        case '"': o += "&quot;"; break;
        case '\'': o += "&apos;"; break;
        default:
            if (c.unicode() < 0x20 && c.unicode() != '\t' && c.unicode() != '\n' && c.unicode() != '\r')
                o += QChar(0xFFFD);
            else
                o += c;
        }
    }
    return o;
}

QString alignToW(OOXML::Align a) {
    switch (a) {
    case OOXML::Align::Center: return "center";
    case OOXML::Align::Right: return "right";
    case OOXML::Align::Justify: return "both";
    default: return "left";
    }
}

QString colorToHex(const QColor& c) {
    if (!c.isValid()) return QString();
    return QString("%1%2%3")
        .arg(c.red(), 2, 16, QChar('0'))
        .arg(c.green(), 2, 16, QChar('0'))
        .arg(c.blue(), 2, 16, QChar('0'));
}

QString runRPr(const OOXML::RunFormat& f) {
    QString s;
    if (f.bold) s += "<w:b/>";
    if (f.italic) s += "<w:i/>";
    if (f.underline) s += "<w:u w:val=\"single\"/>";
    if (f.strike) s += "<w:strike/>";
    if (f.fontSizePt > 0)
        s += QString("<w:sz w:val=\"%1\"/>").arg(qRound(f.fontSizePt * 2));
    if (!f.fontFamily.isEmpty())
        s += QString("<w:rFonts w:ascii=\"%1\" w:hAnsi=\"%1\"/>").arg(xmlEscape(f.fontFamily));
    QString hex = colorToHex(f.color);
    if (!hex.isEmpty())
        s += QString("<w:color w:val=\"%1\"/>").arg(hex);
    if (s.isEmpty()) return QString();
    return "<w:rPr>" + s + "</w:rPr>";
}

QString paragraphRPr(const OOXML::ParagraphFormat& pf) {
    QString s;
    if (!pf.styleId.isEmpty())
        s += QString("<w:pStyle w:val=\"%1\"/>").arg(xmlEscape(pf.styleId));
    s += QString("<w:jc w:val=\"%1\"/>").arg(alignToW(pf.align));
    if (pf.spaceBeforePt > 0 || pf.spaceAfterPt > 0)
        s += QString("<w:spacing w:before=\"%1\" w:after=\"%2\"/>")
                 .arg(qRound(pf.spaceBeforePt * 20))
                 .arg(qRound(pf.spaceAfterPt * 20));
    if (s.isEmpty()) return QString();
    return "<w:pPr>" + s + "</w:pPr>";
}

QString paragraphToXml(const OOXML::Paragraph& p) {
    QString body = paragraphRPr(p.fmt);
    for (const auto& run : p.runs) {
        body += "<w:r>";
        body += runRPr(run.fmt);
        body += QString("<w:t xml:space=\"preserve\">%1</w:t>").arg(xmlEscape(run.text));
        body += "</w:r>";
    }
    if (p.runs.isEmpty())
        body += "<w:r><w:t></w:t></w:r>";
    return "<w:p>" + body + "</w:p>";
}

QString cellToXml(const OOXML::TableCell& cell) {
    QString s = "<w:tc><w:tcPr>";
    if (cell.colSpan > 1)
        s += QString("<w:gridSpan w:val=\"%1\"/>").arg(cell.colSpan);
    s += "</w:tcPr>";
    for (const auto& p : cell.paragraphs)
        s += paragraphToXml(p);
    if (cell.paragraphs.isEmpty())
        s += paragraphToXml(OOXML::Paragraph());
    s += "</w:tc>";
    return s;
}

QString tableToXml(const OOXML::Table& t) {
    QString s = "<w:tbl><w:tblPr><w:tblStyle w:val=\"TableNormal\"/>"
                "<w:tblW w:w=\"0\" w:type=\"auto\"/>"
                "<w:tblBorders>"
                "<w:top w:val=\"single\" w:sz=\"4\" w:space=\"0\" w:color=\"auto\"/>"
                "<w:left w:val=\"single\" w:sz=\"4\" w:space=\"0\" w:color=\"auto\"/>"
                "<w:bottom w:val=\"single\" w:sz=\"4\" w:space=\"0\" w:color=\"auto\"/>"
                "<w:right w:val=\"single\" w:sz=\"4\" w:space=\"0\" w:color=\"auto\"/>"
                "<w:insideH w:val=\"single\" w:sz=\"4\" w:space=\"0\" w:color=\"auto\"/>"
                "<w:insideV w:val=\"single\" w:sz=\"4\" w:space=\"0\" w:color=\"auto\"/>"
                "</w:tblBorders></w:tblPr><w:tblGrid>";
    int cols = 0;
    for (const auto& row : t.rows)
        cols = qMax(cols, row.cells.size());
    for (int i = 0; i < qMax(cols, 1); i++)
        s += QString("<w:gridCol w:w=\"%1\"/>").arg(9072 / qMax(cols, 1));
    s += "</w:tblGrid>";
    for (const auto& row : t.rows) {
        s += "<w:tr>";
        for (const auto& cell : row.cells)
            s += cellToXml(cell);
        s += "</w:tr>";
    }
    s += "</w:tbl>";
    return s;
}

QString documentBody(const OOXML::Document& doc) {
    QString s = "<?xml version=\"1.0\" encoding=\"UTF-8\" standalone=\"yes\"?>"
                "<w:document xmlns:w=\"http://schemas.openxmlformats.org/wordprocessingml/2006/main\">"
                "<w:body>";
    for (const auto& b : doc.blocks) {
        if (b.type == OOXML::Block::ParagraphBlock)
            s += paragraphToXml(b.paragraph);
        else if (b.type == OOXML::Block::TableBlock)
            s += tableToXml(b.table);
    }
    s += "<w:sectPr><w:pgSz w:w=\"12240\" w:h=\"15840\"/>"
         "<w:pgMar w:top=\"1440\" w:right=\"1440\" w:bottom=\"1440\" w:left=\"1440\" "
         "w:header=\"720\" w:footer=\"720\" w:gutter=\"0\"/></w:sectPr>";
    s += "</w:body></w:document>";
    return s;
}

QString stylesXml() {
    QString s = "<?xml version=\"1.0\" encoding=\"UTF-8\" standalone=\"yes\"?>"
        "<w:styles xmlns:w=\"http://schemas.openxmlformats.org/wordprocessingml/2006/main\">"
        "<w:docDefaults><w:rPrDefault><w:rPr>"
        "<w:rFonts w:ascii=\"Calibri\" w:hAnsi=\"Calibri\" w:eastAsia=\"宋体\"/>"
        "<w:sz w:val=\"22\"/></w:rPr></w:rPrDefault></w:docDefaults>"
        "<w:style w:type=\"paragraph\" w:default=\"1\" w:styleId=\"Normal\">"
        "<w:name w:val=\"Normal\"/></w:style>";
    // Heading styles 1..6
    double sizes[6] = {32, 28, 26, 24, 22, 22};
    for (int i = 0; i < 6; i++) {
        s += QString("<w:style w:type=\"paragraph\" w:styleId=\"Heading%1\">"
                     "<w:name w:val=\"Heading%2\"/>"
                     "<w:basedOn w:val=\"Normal\"/>"
                     "<w:pPr><w:keepNext/><w:spacing w:before=\"240\" w:after=\"60\"/>"
                     "<w:outlineLvl w:val=\"%3\"/></w:pPr>"
                     "<w:rPr><w:b/><w:sz w:val=\"%4\"/></w:rPr></w:style>")
                 .arg(i + 1).arg(i + 1).arg(i).arg(qRound(sizes[i] * 2));
    }
    s += "</w:styles>";
    return s;
}

QString contentTypesXml() {
    return "<?xml version=\"1.0\" encoding=\"UTF-8\" standalone=\"yes\"?>"
           "<Types xmlns=\"http://schemas.openxmlformats.org/package/2006/content-types\">"
           "<Default Extension=\"rels\" ContentType=\"application/vnd.openxmlformats-package.relationships+xml\"/>"
           "<Default Extension=\"xml\" ContentType=\"application/xml\"/>"
           "<Override PartName=\"/word/document.xml\" ContentType=\"application/vnd.openxmlformats-officedocument.wordprocessingml.document.main+xml\"/>"
           "<Override PartName=\"/word/styles.xml\" ContentType=\"application/vnd.openxmlformats-officedocument.wordprocessingml.styles+xml\"/>"
           "<Override PartName=\"/docProps/core.xml\" ContentType=\"application/vnd.openxmlformats-package.core-properties+xml\"/>"
           "<Override PartName=\"/docProps/app.xml\" ContentType=\"application/vnd.openxmlformats-officedocument.extended-properties+xml\"/>"
           "</Types>";
}

QString relsXml() {
    return "<?xml version=\"1.0\" encoding=\"UTF-8\" standalone=\"yes\"?>"
           "<Relationships xmlns=\"http://schemas.openxmlformats.org/package/2006/relationships\">"
           "<Relationship Id=\"rId1\" Type=\"http://schemas.openxmlformats.org/officeDocument/2006/relationships/officeDocument\" Target=\"word/document.xml\"/>"
           "<Relationship Id=\"rId2\" Type=\"http://schemas.openxmlformats.org/package/2006/relationships/metadata/core-properties\" Target=\"docProps/core.xml\"/>"
           "<Relationship Id=\"rId3\" Type=\"http://schemas.openxmlformats.org/officeDocument/2006/relationships/extended-properties\" Target=\"docProps/app.xml\"/>"
           "</Relationships>";
}

QString docRelsXml() {
    return "<?xml version=\"1.0\" encoding=\"UTF-8\" standalone=\"yes\"?>"
           "<Relationships xmlns=\"http://schemas.openxmlformats.org/package/2006/relationships\">"
           "<Relationship Id=\"rId1\" Type=\"http://schemas.openxmlformats.org/officeDocument/2006/relationships/styles\" Target=\"styles.xml\"/>"
           "</Relationships>";
}

QString coreXml(const OOXML::Document& doc) {
    QString now = QDateTime::currentDateTimeUtc().toString("yyyy-MM-ddThh:mm:ssZ");
    return "<?xml version=\"1.0\" encoding=\"UTF-8\" standalone=\"yes\"?>"
           "<cp:coreProperties xmlns:cp=\"http://schemas.openxmlformats.org/package/2006/metadata/core-properties\" "
           "xmlns:dc=\"http://purl.org/dc/elements/1.1/\" "
           "xmlns:dcterms=\"http://purl.org/dc/terms/\" "
           "xmlns:xsi=\"http://www.w3.org/2001/XMLSchema-instance\">"
           "<dc:title>" + xmlEscape(doc.title) + "</dc:title>"
           "<dc:creator>" + xmlEscape(doc.creator) + "</dc:creator>"
           "<cp:lastModifiedBy>" + xmlEscape(doc.creator) + "</cp:lastModifiedBy>"
           "<dcterms:created xsi:type=\"dcterms:W3CDTF\">" + now + "</dcterms:created>"
           "<dcterms:modified xsi:type=\"dcterms:W3CDTF\">" + now + "</dcterms:modified>"
           "</cp:coreProperties>";
}

QString appXml() {
    return "<?xml version=\"1.0\" encoding=\"UTF-8\" standalone=\"yes\"?>"
           "<Properties xmlns=\"http://schemas.openxmlformats.org/officeDocument/2006/extended-properties\" "
           "xmlns:vt=\"http://schemas.openxmlformats.org/officeDocument/2006/docPropsVTypes\">"
           "<Application>OpenOffice (Qt)</Application>"
           "<Company>OpenOffice Open Source</Company>"
           "</Properties>";
}
} // namespace

bool DocxWriter::write(const QString& path, const OOXML::Document& doc) {
    QMap<QString, QByteArray> files;
    files["[Content_Types].xml"] = contentTypesXml().toUtf8();
    files["_rels/.rels"] = relsXml().toUtf8();
    files["word/document.xml"] = documentBody(doc).toUtf8();
    files["word/_rels/document.xml.rels"] = docRelsXml().toUtf8();
    files["word/styles.xml"] = stylesXml().toUtf8();
    files["docProps/core.xml"] = coreXml(doc).toUtf8();
    files["docProps/app.xml"] = appXml().toUtf8();
    return ZipUtil::writeZip(path, files);
}
