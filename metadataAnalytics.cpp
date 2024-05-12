#include "metadataAnalytics.h"
#include <QDebug>
#include <QJsonDocument>
#include <QJsonObject>
#include <QJsonArray>
#include <QTimer>
#include <sys/sysinfo.h> // Include for sysinfo struct
#include <QThread> // Include for QThread

struct NodeInfo {
    QString nodeType;
    QString ip;
    double computingCapacity; // Include computing capacity in NodeInfo
};
std::map<std::string, Register> metadataAnalytics::registrations;
std::map<std::string, QTcpSocket*> metadataAnalytics::analyticNodes;
std::map<std::string, NodeInfo> nodeMap; // New map to store node information

metadataAnalytics::metadataAnalytics(QObject *parent) : QTcpServer(parent) {
    QTimer* timer = new QTimer(this);
    connect(timer, &QTimer::timeout, this, &metadataAnalytics::monitorHeartbeat);
    timer->start(5000);
    qDebug() << "metadataAnalytics created.";

    // Set up election timer
    electionTimer = new QTimer(this);
    connect(electionTimer, &QTimer::timeout, this, &metadataAnalytics::conductElection);
    electionTimer->start(3000); // 30 seconds

    // Generate number and get IP address
    myNumber = generateNumber();
    myIP = QHostAddress(QHostAddress::LocalHost).toString();
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

        // Self-register current node only once at start
        if (!registeredSelf) {
            registerNode(actualIP, typeNode, computeCapacityHeuristic());
            registeredSelf = true;
        }
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

    QString ipAddress = clientSocket->peerAddress().toString(); // Current node's IP address

    if (obj.contains("requestType")) {
        QString requestType = obj["requestType"].toString();

        if (requestType == "Heartbeat") {
            QJsonObject responseObj;
            responseObj["requestType"] = "Heartbeat Response";
            responseObj["message"] = "I am alive";
            responseObj["status"] = "OK";

            QJsonDocument responseDoc(responseObj);
            QByteArray responseData = responseDoc.toJson();

            clientSocket->write(responseData);
            clientSocket->waitForBytesWritten();
            qDebug() << "Heartbeat response sent to server.";
        } else if (requestType == "Node Discovery") {
            QJsonArray nodes = obj["nodes"].toArray();
            nodeMap.clear(); // Clear existing data before updating
            for (const QJsonValue &value : nodes) {
                QJsonObject nodeObj = value.toObject();
                NodeInfo info;
                info.nodeType = nodeObj["nodeType"].toString();
                info.ip = nodeObj["IP"].toString();
                info.computingCapacity = nodeObj["computingCapacity"].toDouble(); // Store computing capacity in NodeInfo
                nodeMap[info.ip.toStdString()] = info; // Store node info in nodeMap
                qDebug() << "Discovered Node IP:" << info.ip
                         << "Type:" << info.nodeType
                         << "Computing Capacity:" << info.computingCapacity; // Print computing capacity
            }
        } else if (requestType == "registering") {
            QString type = obj["nodeType"].toString();
            double computingCapacity = obj["computingCapacity"].toDouble(); // Extract computing capacity from JSON
            qDebug() << "Registration request from IP:" << ipAddress << "Type:" << type << "Computing Capacity:" << computingCapacity;
            registerNode(ipAddress, type.toStdString(), computingCapacity);
            broadcastNodeInfo();
        } else if (requestType == "Election Message") {
            int receivedNumber = obj["Current Number"].toInt();
            QString receivedIP = obj["IP"].toString(); // Received IP address
            processElectionMessage(receivedNumber, receivedIP.toStdString());
        }
        else{
            qDebug() << requestType << "   btyihbhvgkyulhjk\n";
            leader = obj["IP"].toString();
            qDebug() << leader << "\n";
        }
    }

    // Handle self-heartbeat response and node discovery events
    if (obj.contains("requestType")) {
        QString requestType = obj["requestType"].toString();

        if (requestType == "Heartbeat Response") {
            qDebug() << "Received heartbeat response from self.";
        } else if (requestType == "Node Discovery") {
            QJsonArray nodes = obj["nodes"].toArray();
            nodeMap.clear(); // Clear existing data before updating
            for (const QJsonValue &value : nodes) {
                QJsonObject nodeObj = value.toObject();
                NodeInfo info;
                info.nodeType = nodeObj["nodeType"].toString();
                info.ip = nodeObj["IP"].toString();
                info.computingCapacity = nodeObj["computingCapacity"].toDouble(); // Store computing capacity in NodeInfo
                nodeMap[info.ip.toStdString()] = info; // Store node info in nodeMap
                qDebug() << "Discovered Node IP:" << info.ip
                         << "Type:" << info.nodeType
                         << "Computing Capacity:" << info.computingCapacity; // Print computing capacity
            }
        }
    }
}

void metadataAnalytics::processElectionMessage(int receivedNumber, const std::string& ipAddress) {
    // Compare received number with myNumber
    if (receivedNumber > myNumber) {
        // Forward the election message to the next node
        auto currentNode = registrations.find(myIP.toStdString());
        auto nextNode = currentNode;
        ++nextNode;

        if (nextNode != registrations.end()) {
            QJsonObject electionMessage;
            electionMessage["requestType"] = "Election Message";
            electionMessage["Current Number"] = receivedNumber; // Forward the received number
            electionMessage["IP"] = QString::fromStdString(ipAddress);
            QJsonDocument doc(electionMessage);
            QByteArray messageData = doc.toJson();

            QTcpSocket* nextSocket = analyticNodes[nextNode->first];
            if (nextSocket && nextSocket->state() == QAbstractSocket::ConnectedState) {
                nextSocket->write(messageData);
                nextSocket->flush();
            }
        } else {
            // No next node to forward the message, declare this node as the leader
            sendLeaderInfo(ipAddress);
        }
    } else if (receivedNumber < myNumber) {
        // Send the received number to the next node
        auto currentNode = registrations.find(myIP.toStdString());
        auto nextNode = currentNode;
        ++nextNode;

        if (nextNode != registrations.end()) {
            QJsonObject electionMessage;
            electionMessage["requestType"] = "Election Message";
            electionMessage["Current Number"] = receivedNumber; // Forward the received number
            electionMessage["IP"] = QString::fromStdString(ipAddress);
            QJsonDocument doc(electionMessage);
            QByteArray messageData = doc.toJson();

            QTcpSocket* nextSocket = analyticNodes[nextNode->first];
            if (nextSocket && nextSocket->state() == QAbstractSocket::ConnectedState) {
                nextSocket->write(messageData);
                nextSocket->flush();
            }
        }
    } else {
        // Election tie, declare this node as the leader
        sendLeaderInfo(ipAddress);
    }
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
        nodeObject["nodeType"] = QString::fromStdString(reg.second.getType());
        nodeObject["IP"] = QString::fromStdString(reg.second.getIpAddress());
        nodeObject["computingCapacity"] = reg.second.getComputingCapacity(); // Include computing capacity in the broadcast
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

void metadataAnalytics::registerNode(const QString& ipAddress, const std::string& type, double computingCapacity) {
    registrations[ipAddress.toStdString()] = Register(ipAddress.toStdString(), type, computingCapacity);
}

void metadataAnalytics::deregisterNode(const QString& ipAddress) {
    registrations.erase(ipAddress.toStdString());
}

double metadataAnalytics::computeCapacityHeuristic() {
    // RAM
    struct sysinfo info;
    sysinfo(&info); // Get system information

    // Normalize RAM size to range between 0 and 1
    double ramCapacity = static_cast<double>(info.totalram) / (1024 * 1024 * 1024); // Convert to GB
    double normalizedRAM = ramCapacity / 16.0; // Assuming a maximum RAM capacity of 16 GB

    // CPU Cores
    int numCores = QThread::idealThreadCount(); // Get the number of CPU cores
    double normalizedCores = static_cast<double>(numCores) / QThread::idealThreadCount();

    // Assume weights for each factor (adjust as needed)
    const double weightRAM = 0.5;
    const double weightCores = 0.5;

    // Compute weighted average
    double weightedAverage = (normalizedRAM * weightRAM) + (normalizedCores * weightCores);

    return weightedAverage;
}

void metadataAnalytics::conductElection() {
    if (registrations.empty()) {
        qDebug() << "No nodes registered for election.";
        return;
    }
    if (!leader.isEmpty()) {
        qDebug() << "A leader has been elected. Stopping further elections.";
        // Disconnect the timer's timeout signal to stop further elections
        disconnect(electionTimer, &QTimer::timeout, this, &metadataAnalytics::conductElection);
        return;
    }

    qDebug() << "Conducting election...";

    auto currentNode = registrations.find(myIP.toStdString()); // Start from the current node
    auto nextNode = currentNode;
    ++nextNode; // Move to the next node

    // Send election message to the next node
    if (nextNode != registrations.end()) {
        QJsonObject electionMessage;
        electionMessage["requestType"] = "Election Message";
        electionMessage["Current Number"] = myNumber;
        electionMessage["IP"] = myIP;
        QJsonDocument doc(electionMessage);
        QByteArray messageData = doc.toJson();

        QTcpSocket* nextSocket = analyticNodes[nextNode->first];
        if (nextSocket && nextSocket->state() == QAbstractSocket::ConnectedState) {
            nextSocket->write(messageData);
            nextSocket->flush();
        }
    } else {
        qDebug() << "No next node to send election message to.";
        sendLeaderInfo(myIP.toStdString());
    }
}

void metadataAnalytics::sendLeaderInfo(const std::string& ipAddress) {
    // Send leader information to the node
    QTcpSocket* leaderSocket = analyticNodes[ipAddress];
    if (leaderSocket && leaderSocket->state() == QAbstractSocket::ConnectedState) {
        QJsonObject leaderInfo;
        leaderInfo["requestType"] = "Leader Announcement";
        leaderInfo["message"] = "I am the leader.";
        leaderInfo["leaderIP"] = QString::fromStdString(ipAddress);
        QJsonDocument doc(leaderInfo);
        leaderSocket->write(doc.toJson());
        leaderSocket->flush();
        qDebug() << "Leader information sent to" << QString::fromStdString(ipAddress);
    }
}

void metadataAnalytics::analyticsNodeDisconnected() {
    QTcpSocket *clientSocket = qobject_cast<QTcpSocket*>(sender());
    if (!clientSocket) return;

    QString ipAddress = clientSocket->peerAddress().toString();
    qDebug() << "Client disconnected, IP:" << ipAddress;
    deregisterNode(ipAddress); // Remove the disconnected node from registrations
    analyticNodes.erase(ipAddress.toStdString()); // Erase the disconnected node from the analyticNodes map
    clientSocket->deleteLater();
    broadcastNodeInfo(); // Broadcast updated node information
}

int metadataAnalytics::generateNumber() {
    // Generate and return a random number
    return 42.0; // Placeholder, replace with actual random number generation logic
}
