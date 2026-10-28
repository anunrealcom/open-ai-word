#pragma once
// Zero-dependency ZIP reader/writer + RFC1951 DEFLATE inflater.
// Used to read/write .docx (which is a ZIP of XML). Writer uses STORE
// (method 0) which is fully valid and opens correctly in Word/WPS.
// MIT licensed, no third-party dependencies.
#include <QByteArray>
#include <QMap>
#include <QString>

namespace ZipUtil {

// Inflate a raw DEFLATE stream (no zlib header). Returns uncompressed data,
// or an empty QByteArray on error.
QByteArray inflateRaw(const QByteArray& deflate);

// Read all entries of a zip file into out (entry path -> data). Returns success.
bool readZip(const QString& path, QMap<QString, QByteArray>& out);

// Write entries as a zip using STORE (method 0). Returns success.
bool writeZip(const QString& path, const QMap<QString, QByteArray>& files);

// Self-test: verifies inflateRaw against Qt's qCompress output.
// Returns true if all vectors inflate correctly.
bool selfTest();

} // namespace ZipUtil
