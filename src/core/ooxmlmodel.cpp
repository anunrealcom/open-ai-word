#include "ooxmlmodel.h"

namespace OOXML {

void Document::clear() {
    blocks.clear();
    styles.clear();
    title.clear();
    creator.clear();
    description.clear();
}

void Document::addParagraph(const Paragraph& p) {
    Block b;
    b.type = Block::ParagraphBlock;
    b.paragraph = p;
    blocks.append(b);
}

void Document::addTable(const Table& t) {
    Block b;
    b.type = Block::TableBlock;
    b.table = t;
    blocks.append(b);
}

ParagraphFormat Document::resolveParagraphFormat(const ParagraphFormat& pf) const {
    ParagraphFormat out = pf;
    if (!pf.styleId.isEmpty()) {
        auto it = styles.find(pf.styleId);
        if (it != styles.end()) {
            const StyleDef& s = it.value();
            if (s.isParagraphStyle) {
                // inherit alignment / spacing only if not explicitly set
                if (out.align == Align::Left && s.parFmt.align != Align::Left)
                    out.align = s.parFmt.align;
                if (out.spaceBeforePt == 0) out.spaceBeforePt = s.parFmt.spaceBeforePt;
                if (out.spaceAfterPt == 0) out.spaceAfterPt = s.parFmt.spaceAfterPt;
                if (out.lineSpacingPt == 0) out.lineSpacingPt = s.parFmt.lineSpacingPt;
                if (!out.isHeading) {
                    out.isHeading = s.parFmt.isHeading;
                    out.headingLevel = s.parFmt.headingLevel;
                }
            }
        }
    }
    return out;
}

RunFormat Document::resolveRunFormat(const RunFormat& rf, const QString& paragraphStyleId) const {
    RunFormat out = rf;
    if (!paragraphStyleId.isEmpty()) {
        auto it = styles.find(paragraphStyleId);
        if (it != styles.end()) {
            const StyleDef& s = it.value();
            if (out.fontSizePt == 0) out.fontSizePt = s.runFmt.fontSizePt;
            if (out.fontFamily.isEmpty()) out.fontFamily = s.runFmt.fontFamily;
            if (!out.color.isValid()) out.color = s.runFmt.color;
            // Bold/italic/underline are usually explicitly set in runs; only
            // inherit when the run did not specify them. We use font size 0 as
            // the "unspecified" sentinel; for bools we cannot distinguish, so
            // we do not override explicit run bools here.
        }
    }
    if (out.fontSizePt == 0) out.fontSizePt = 11.0;
    return out;
}

} // namespace OOXML
