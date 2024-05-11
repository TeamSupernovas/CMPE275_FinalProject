#include "metadataAnalytics.h"
#include <QDebug>

std::map<std::string, Register> metadataAnalytics::registrations; // Initialize registrations map
std::map<std::string, QTcpSocket*> metadataAnalytics::analyticNodes; // Initialize analyticNodes map

metadataAnalytics::metadataAnalytics(QObject *parent) : QTcpServer(parent) {
    // Start heartbeat monitoring timer
    QTimer* timer = new QTimer(this);
    connect(timer, &QTimer::timeout, this, &metadataAnalytics::monitorHeartbeat);
    timer->start(5000); // Adjust the interval as needed (e.g., 5000 ms = 5 seconds)

    qDebug() << "metadataAnalytics created.";
}

metadataAnalytics::~metadataAnalytics() {}

void metadataAnalytics::incomingConnection(qintptr socketDescriptor) {
    QTcpSocket *clientSocket = new QTcpSocket(this);
    clientSocket->setSocketDescriptor(socketDescriptor);
    connect(clientSocket, &QTcpSocket::readyRead, this, &metadataAnalytics::readData);
    connect(clientSocket, &QTcpSocket::disconnected, this, &metadataAnalytics::analyticsNodeDisconnected);

    qDebug() << "Incoming connection established.";
}

void metadataAnalytics::readData() {
    QTcpSocket *clientSocket = qobject_cast<QTcpSocket*>(sender());
    if (!clientSocket)
        return;

    QByteArray data = clientSocket->readAll();
    qDebug() << "Received data:" << data;

    if (data.startsWith("Registration:")) {
        QString ipAddress = clientSocket->peerAddress().toString();
        QString type = data.mid(13);

        qDebug() << "Registration request from:" << ipAddress << "Type:" << type;

        // Store registration data
        registrations[ipAddress.toStdString()] = Register(ipAddress.toStdString(), type.toStdString());

        // Add client socket to analyticNodes map
        analyticNodes[ipAddress.toStdString()] = clientSocket;
    }
}


void metadataAnalytics::analyticsNodeDisconnected() {
    QTcpSocket *clientSocket = qobject_cast<QTcpSocket*>(sender());
    if (!clientSocket)
        return;

    QString ipAddress = clientSocket->peerAddress().toString();
    qDebug() << "Client disconnected:" << ipAddress;

    // Remove client socket from clients map
    if (analyticNodes.find(ipAddress.toStdString()) != analyticNodes.end()) {
 
        delete analyticNodes[ipAddress.toStdString()];
        analyticNodes.erase(ipAddress.toStdString());
    }
}

void metadataAnalytics::monitorHeartbeat() {
    qDebug() << "Monitoring heartbeat for registered analyticNodes.";

    // Iterate through registered analyticNodes
    for (auto it = registrations.begin(); it != registrations.end();) {
        // Get client's IP address
        std::string ipAddress = it->first;

        // Check if the client is connected
        if (analyticNodes.find(ipAddress) != analyticNodes.end()) {
            // Get the client's socket
            QTcpSocket* clientSocket = analyticNodes[ipAddress];

            // Send heartbeat message
            qDebug() << "Sending heartbeat to:" << QString::fromStdString(ipAddress);
            clientSocket->write("Heartbeat");
            clientSocket->waitForBytesWritten();

            // Wait for response within timeout period
            if (clientSocket->waitForReadyRead(10000)) {
                // Received response, client is alive
                qDebug() << "Heartbeat response received from:" << QString::fromStdString(ipAddress);
                ++it; // Move to the next client
            } else {
                // No response received within timeout period, client may be disconnected
                qDebug() << "Heartbeat response not received from:" << QString::fromStdString(ipAddress);
                // Remove client from registrations and analyticNodes
                registrations.erase(it++);
                delete analyticNodes[ipAddress];
                analyticNodes.erase(ipAddress);
            }
        } else {
            // Client not connected, remove it from registrations
            qDebug() << "Client not connected:" << QString::fromStdString(ipAddress);
            registrations.erase(it++);
        }
    }
}
