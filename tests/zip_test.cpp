// Console tool: verifies the embedded DEFLATE inflater (ZipUtil::inflateRaw)
// by compressing known vectors with Qt's zlib (qCompress) and checking the
// round-trip. Build with BUILD_ZIP_TEST=ON (default).
#include "core/ziputil.h"
#include <QCoreApplication>
#include <QDebug>

int main(int argc, char* argv[]) {
    QCoreApplication app(argc, argv);
    bool ok = ZipUtil::selfTest();
    qDebug() << "ZipUtil::selfTest =>" << (ok ? "PASS" : "FAIL");
    return ok ? 0 : 1;
}
