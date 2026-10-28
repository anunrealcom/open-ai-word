#pragma once
// Writes the OOXML::Document model back to a valid .docx package.
#include "core/ooxmlmodel.h"
#include <QString>

class DocxWriter {
public:
    // Returns true on success.
    static bool write(const QString& path, const OOXML::Document& doc);
};
