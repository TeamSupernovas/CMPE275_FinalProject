#include <QCoreApplication>
#include "metadataAnalytics.h"
#include <QSettings>
#include <QDebug>



int main(int argc, char *argv[]) {
    QCoreApplication a(argc, argv);

    // Read port number from environment file
    QSettings settings("config.env", QSettings::IniFormat);
    int port = settings.value("PORT").toInt();
    //qDebug()  << port << "\n";
    metadataAnalytics metadataAnalytics_1;
    if (!metadataAnalytics_1.listen(QHostAddress::Any, port)) {
        qDebug() << "metadataAnalytics could not start!";
        return -1;
    }
    qDebug() << "metadataAnalytics started on port " << port;

    return a.exec();
}
