#include <QtNetwork>
#include <QCoreApplication>
#include <QDebug>
#include <QSettings>
#include <QJsonDocument>
#include <QJsonObject>
#include <QJsonArray>
#include <QMap>

struct NodeInfo {
    QString nodeType;
    QString ip;
};

QTcpSocket clientSocket;
QMap<QString, NodeInfo> nodeMap; // Map to store node information by IP address

QString getLocalIpAddress() {
    QList<QHostAddress> list = QNetworkInterface::allAddresses();
    for (int nIter = 0; nIter < list.count(); nIter++) {
        if (!list[nIter].isLoopback() && list[nIter].protocol() == QAbstractSocket::IPv4Protocol)
             qDebug() << "IPV4";
        return list[nIter].toString();
    }
    qDebug() << "IPV6";
    return "127.0.0.1";  // Default to localhost if no suitable IP found
}

void readData() {
    while (clientSocket.bytesAvailable() > 0) {
        QByteArray requestData = clientSocket.readAll();
        qDebug() << "Received data from server:" << requestData;

        QJsonDocument doc = QJsonDocument::fromJson(requestData);
        if (doc.isNull()) {
            qDebug() << "Failed to parse JSON data";
            return;
        }

        QJsonObject obj = doc.object();
        QString requestType = obj["requestType"].toString();

        if (requestType == "Heartbeat") {
            QJsonObject responseObj;
            responseObj["requestType"] = "Heartbeat Response";
            responseObj["message"] = "I am alive";
            responseObj["status"] = "OK";

            QJsonDocument responseDoc(responseObj);
            QByteArray responseData = responseDoc.toJson();

            clientSocket.write(responseData);
            clientSocket.waitForBytesWritten();
            qDebug() << "Heartbeat response sent to server.";
        } else if (requestType == "Node Discovery") {
            QJsonArray nodes = obj["nodes"].toArray();
            nodeMap.clear(); // Clear existing data before updating
            for (const QJsonValue &value : nodes) {
                QJsonObject nodeObj = value.toObject();
                NodeInfo info;
                info.nodeType = nodeObj["nodeType"].toString();
                info.ip = nodeObj["IP"].toString();
                nodeMap[info.ip] = info;
                qDebug() << "Discovered Node IP:" << info.ip
                         << "Type:" << info.nodeType;
            }
        }
    }
}

int main(int argc, char *argv[]) {
    QCoreApplication app(argc, argv);

    // Read settings from configuration file
    QSettings settings("config.env", QSettings::IniFormat);
    QString serverIp = settings.value("SERVER_IP", "localhost").toString();
    int port = settings.value("PORT", 12345).toInt(); // Default port if not specified

    clientSocket.connectToHost(serverIp, port);
    if (!clientSocket.waitForConnected(3000)) {
        qDebug() << "Failed to connect to server at" << serverIp << "on port" << port;
        return -1;
    }

    QString localIP = getLocalIpAddress();  // Get the local IP address

    qDebug() <<localIP;
    // Registration data sent to server upon connection
    QJsonObject registrationObject;
    registrationObject["requestType"] = "registering";
    registrationObject["ip"] = localIP;  // Send the actual IP address discovered
    registrationObject["nodeType"] = "Analytics Node";
    QJsonDocument registrationDoc(registrationObject);
    QByteArray registrationData = registrationDoc.toJson();

    clientSocket.write(registrationData);
    clientSocket.waitForBytesWritten();

    QObject::connect(&clientSocket, &QTcpSocket::readyRead, &readData);

    return app.exec();
}
