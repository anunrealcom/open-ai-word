#include "mainwindow.h"
#include "core/ziputil.h"
#include <QApplication>
#include <QDebug>

int main(int argc, char* argv[]) {
    QApplication app(argc, argv);
    QApplication::setOrganizationName("OpenOffice");
    QApplication::setApplicationName("OpenOffice");
    QApplication::setApplicationDisplayName("OpenOffice");

    // Verify the embedded DEFLATE inflater against Qt's zlib compressor.
    if (!ZipUtil::selfTest())
        qWarning() << "[OpenOffice] ZipUtil::selfTest FAILED — docx reading may be broken!";
    else
        qDebug() << "[OpenOffice] ZipUtil::selfTest OK";

    MainWindow w;
    w.show();
    return app.exec();
}
