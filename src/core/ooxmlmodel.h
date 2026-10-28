#pragma once
// In-memory OOXML (WordprocessingML) document model.
// This is the engine's own representation of a .docx document. It is intentionally
// a "common subset" model: paragraphs, runs (with basic character formatting),
// simple tables, and section/style resolution. Fidelity will grow over time.
#include <QString>
#include <QList>
#include <QColor>
#include <QMap>
#include <QtMath>

namespace OOXML {

struct RunFormat {
    bool bold = false;
    bool italic = false;
    bool underline = false;
    double fontSizePt = 11.0;      // point size; 0 means "inherit"
    QString fontFamily;
    QColor color;                  // invalid => default color
    bool strike = false;

    bool operator==(const RunFormat& o) const {
        return bold == o.bold && italic == o.italic && underline == o.underline &&
               qAbs(fontSizePt - o.fontSizePt) < 0.01 && fontFamily == o.fontFamily &&
               color == o.color && strike == o.strike;
    }
    bool operator!=(const RunFormat& o) const { return !(*this == o); }
};

struct Run {
    QString text;
    RunFormat fmt;
};

enum class Align { Left, Center, Right, Justify };

struct ParagraphFormat {
    Align align = Align::Left;
    double spaceBeforePt = 0;
    double spaceAfterPt = 0;
    double lineSpacingPt = 0; // 0 = default (single)
    QString styleId;          // paragraph style reference (e.g. "Heading1")
    bool isHeading = false;
    int headingLevel = 0;     // 1..6 if isHeading
};

struct Paragraph {
    QList<Run> runs;
    ParagraphFormat fmt;
};

struct TableCell {
    QList<Paragraph> paragraphs; // a cell contains one or more paragraphs
    int colSpan = 1;
    int rowSpan = 1;
};

struct TableRow {
    QList<TableCell> cells;
};

struct Table {
    QList<TableRow> rows;
    QList<int> gridCols; // relative column widths (twips), informational
};

struct Block {
    enum Type { ParagraphBlock, TableBlock } type = ParagraphBlock;
    Paragraph paragraph;
    Table table;
};

// Style definitions resolved from styles.xml
struct StyleDef {
    QString id;
    QString name;
    bool isParagraphStyle = true;
    RunFormat runFmt;
    ParagraphFormat parFmt;
};

class Document {
public:
    QList<Block> blocks;
    QMap<QString, StyleDef> styles; // keyed by styleId
    QString title;
    QString creator;
    QString description;

    void clear();
    void addParagraph(const Paragraph& p);
    void addTable(const Table& t);

    // Resolve a paragraph's effective formatting by merging any referenced style.
    ParagraphFormat resolveParagraphFormat(const ParagraphFormat& pf) const;
    RunFormat resolveRunFormat(const RunFormat& rf, const QString& paragraphStyleId) const;
};

} // namespace OOXML
