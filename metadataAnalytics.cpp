#include "metadataAnalytics.h"
#include <QDebug>
#include <QJsonDocument>
#include <QJsonObject>
#include <QJsonArray>

std::map<std::string, Register> metadataAnalytics::registrations;
std::map<std::string, QTcpSocket*> metadataAnalytics::analyticNodes;

metadataAnalytics::metadataAnalytics(QObject *parent) : QTcpServer(parent) {
    QTimer* timer = new QTimer(this);
    connect(timer, &QTimer::timeout, this, &metadataAnalytics::monitorHeartbeat);
    timer->start(5000);
    qDebug() << "metadataAnalytics created.";
}

metadataAnalytics::~metadataAnalytics() {
    for (auto& node : analyticNodes) {
        node.second->deleteLater();
    }
}

void metadataAnalytics::incomingConnection(qintptr socketDescriptor) {
    QTcpSocket *clientSocket = new QTcpSocket(this);
    if (clientSocket->setSocketDescriptor(socketDescriptor)) {
        QString actualIP = clientSocket->peerAddress().toString();
        connect(clientSocket, &QTcpSocket::readyRead, this, &metadataAnalytics::readData);
        connect(clientSocket, &QTcpSocket::disconnected, this, &metadataAnalytics::analyticsNodeDisconnected);
        qDebug() << "Incoming connection established from IP:" << actualIP;
        analyticNodes[actualIP.toStdString()] = clientSocket;
    } else {
        delete clientSocket;
    }
}

void metadataAnalytics::readData() {
    QTcpSocket *clientSocket = qobject_cast<QTcpSocket*>(sender());
    if (!clientSocket) return;

    QByteArray data = clientSocket->readAll();
    qDebug() << "Received data:" << data;

    QJsonDocument doc = QJsonDocument::fromJson(data);
    QJsonObject obj = doc.object();

    if (obj.contains("requestType") && obj["requestType"].toString() == "registering") {
        QString ipAddress = obj["ip"].toString();//clientSocket->peerAddress().toString();
        QString type = obj["nodeType"].toString();
        qDebug() << "Registration request from IP:" << ipAddress << "Type:" << type;
        registrations[ipAddress.toStdString()] = Register(ipAddress.toStdString(), type.toStdString());
        broadcastNodeInfo();
    }
}

void metadataAnalytics::analyticsNodeDisconnected() {
    QTcpSocket *clientSocket = qobject_cast<QTcpSocket*>(sender());
    if (!clientSocket) return;

    QString ipAddress = clientSocket->peerAddress().toString();
    qDebug() << "Client disconnected, IP:" << ipAddress;
    registrations.erase(ipAddress.toStdString());
    analyticNodes.erase(ipAddress.toStdString());
    clientSocket->deleteLater();
    broadcastNodeInfo();
}

void metadataAnalytics::monitorHeartbeat() {
    for (auto it = analyticNodes.begin(); it != analyticNodes.end(); ++it) {
        QTcpSocket* clientSocket = it->second;
        if (clientSocket && clientSocket->state() == QAbstractSocket::ConnectedState) {
            QJsonObject heartbeatObj;
            heartbeatObj["requestType"] = "Heartbeat";
            QJsonDocument doc(heartbeatObj);
            clientSocket->write(doc.toJson());
        }
    }
}

void metadataAnalytics::broadcastNodeInfo() {
    qDebug() << "Broadcasting Node Discovery Information";

    QJsonArray nodesArray;
    for (const auto& reg : registrations) {
        QJsonObject nodeObject;
        nodeObject["nodeType"] = QString::fromStdString(reg.second.type);
        nodeObject["IP"] = QString::fromStdString(reg.first);
        nodesArray.append(nodeObject);
    }

    QJsonObject parentNode;
    parentNode["requestType"] = "Node Discovery";
    parentNode["nodes"] = nodesArray;

    QJsonDocument doc(parentNode);
    QByteArray nodeData = doc.toJson();

    for (const auto& node : analyticNodes) {
        QTcpSocket* clientSocket = node.second;
        if (clientSocket && clientSocket->state() == QAbstractSocket::ConnectedState) {
            clientSocket->write(nodeData);
            clientSocket->flush();
        }
    }
}
