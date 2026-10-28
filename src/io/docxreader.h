#pragma once
// Reads .docx (Office Open XML / WordprocessingML) into the OOXML::Document model.
#include "core/ooxmlmodel.h"
#include <QString>

class DocxReader {
public:
    // Returns true on success. Populates `doc`.
    static bool read(const QString& path, OOXML::Document& doc);

    // Convenience: extract plain text (used for AI document-level chat).
    static QString toPlainText(const OOXML::Document& doc);
};
