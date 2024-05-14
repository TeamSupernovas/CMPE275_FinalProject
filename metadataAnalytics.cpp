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

QString getLocalIpAddress() {
    // QList<QHostAddress> list = QNetworkInterface::allAddresses();
    // for (int nIter = 0; nIter < list.count(); nIter++) {
    //     if (list[nIter].protocol() == QAbstractSocket::IPv4Protocol && !list[nIter].isLoopback()) {
    //         qDebug() << "Local IPv4 Address found:" << list[nIter].toString();
    //         return list[nIter].toString();
    //     }
    // }
    // qDebug() << "No non-loopback IPv4 address found, defaulting to localhost.";
   // return "localhost";
    return "192.168.1.102";  // Default to localhost if no suitable IP found
}
metadataAnalytics::metadataAnalytics(QObject *parent) : QTcpServer(parent) {
    QTimer* timer = new QTimer(this);
    // connect(timer, &QTimer::timeout, this, &metadataAnalytics::monitorHeartbeat);
   // timer->start(5000);
    qDebug() << "metadataAnalytics created.";

    // Set up election timer
    electionTimer = new QTimer(this);
    connect(electionTimer, &QTimer::timeout, this, &metadataAnalytics::conductElection);
    electionTimer->start(3000); // 30 seconds

    // Generate number and get IP address
    myNumber = generateNumber();
    myIP = getLocalIpAddress();

    // Initialize initAnalyticsTimer
    initAnalyticsTimer = new QTimer(this);
    connect(initAnalyticsTimer, &QTimer::timeout, this, &metadataAnalytics::initAnalytics);
    //electionTimer->start(1500); // 30 seconds
    // Start the initAnalyticsTimer with a single-shot
    initAnalyticsTimer->singleShot(8000, this, &metadataAnalytics::initAnalytics);
}


void metadataAnalytics::initAnalytics() {
    // Collect IPs of all analytics nodes
    std::vector<std::string> analyticsNodeIPs;
    // qDebug() << "here1111111" << "\n";
    for(const auto& reg : registrations) {

      //  qDebug() << "here222" << "\n";
        if (QString::fromStdString(reg.second.getType()) == "analytics") {

            analyticsNodeIPs.push_back((reg.second.getIpAddress()));
        }
    }

    // Iterate over analytics nodes and distribute replicas
    for(const auto& reg : registrations) {
        if (reg.second.getType() == "analytics") {
            // Find replicas for this analytics node
           // qDebug() << "" << "\n";
            std::vector<std::string> replicas = findReplicas(reg.second.getIpAddress(), analyticsNodeIPs);

            // Convert replicas to QStringList
            QStringList replicasStringList;
            for (const auto& replica : replicas) {
                replicasStringList << QString::fromStdString(replica);
            }

            // Construct API request
            QJsonObject requestData;
            requestData["requestType"] = "Init Analytics";
            requestData["replicas"] = QJsonArray::fromStringList(replicasStringList);
            qDebug() << requestData << "\n";
            // Convert JSON data to QByteArray
            QJsonDocument doc(requestData);
            QByteArray requestDataBytes = doc.toJson();

            // Send API request to the analytics node
            QTcpSocket* nodeSocket = analyticNodes[reg.second.getIpAddress()];
            if (nodeSocket && nodeSocket->state() == QAbstractSocket::ConnectedState) {
                nodeSocket->write(requestDataBytes);
                nodeSocket->flush();
            }
        }
    }
}

std::vector<std::string> metadataAnalytics::findReplicas(const std::string& nodeIP, const std::vector<std::string>& analyticsNodeIPs) {
    std::vector<std::string> replicas;

    // If there are fewer than 3 analytics nodes, return empty list
    if (analyticsNodeIPs.size() <= 2) {
        return replicas;
    }

    // Calculate the index of the current node's IP in the sorted list of analytics node IPs
    auto it = std::lower_bound(analyticsNodeIPs.begin(), analyticsNodeIPs.end(), nodeIP);
    int currentIndex = std::distance(analyticsNodeIPs.begin(), it);

    // Calculate the total number of analytics nodes
    int numAnalyticsNodes = analyticsNodeIPs.size();

    // Calculate the indices of the two replicas using a fair hashing mechanism
    int firstReplicaIndex = (currentIndex + 1) % numAnalyticsNodes;
    int secondReplicaIndex = (currentIndex + 2) % numAnalyticsNodes;

    // Add the IPs of the replicas to the list
    replicas.push_back(analyticsNodeIPs[firstReplicaIndex]);
    replicas.push_back(analyticsNodeIPs[secondReplicaIndex]);

    return replicas;
}


metadataAnalytics::~metadataAnalytics() {
    for (auto& node : analyticNodes) {
        node.second->deleteLater();
    }
}

void metadataAnalytics::incomingConnection(qintptr socketDescriptor) {
    QTcpSocket *clientSocket = new QTcpSocket(this);
    // Self-register current node only once at start
    if (!registeredSelf) {
        registerNode(myIP, typeNode, computeCapacityHeuristic());
        registeredSelf = true;
    }
    if (clientSocket->setSocketDescriptor(socketDescriptor)) {
        QString actualIP = clientSocket->peerAddress().toString();

        connect(clientSocket, &QTcpSocket::readyRead, this, &metadataAnalytics::readData);
        connect(clientSocket, &QTcpSocket::disconnected, this, &metadataAnalytics::analyticsNodeDisconnected);
        qDebug() << "Incoming connection established from IP:" << actualIP;
        analyticNodes[actualIP.toStdString()] = clientSocket;


    } else {
        delete clientSocket;
    }
    qDebug() << registrations.size() << "\n";
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
            //qDebug() << "Heartbeat response sent to server.";
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
            qDebug() << receivedIP << "::" << receivedNumber << "\n";
            processElectionMessage(receivedNumber, receivedIP.toStdString());
        }
        else if (requestType == "Leader Announcement") {

            // QJsonObject obj = value.toObject();
            std::string receivedNumber = obj["leaderIP"].toString().toStdString();
            std::string nodeType=obj["nodeType"].toString().toStdString();
            leader =obj["leaderIP"].toString();
            qDebug()<<"Wining Node"<<receivedNumber<<"...Type...."<<nodeType;
        }
        else{
            qDebug()<<requestType<<"\n";
        }
    }

    // Handle self-heartbeat response and node discovery events
    if (obj.contains("requestType")) {
        QString requestType = obj["requestType"].toString();

        if (requestType == "Heartbeat Response") {
           // qDebug() << "Received heartbeat response from self.";
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
        // Forward the election message to the next node of type "metadata Analytics"
        auto currentNode = registrations.find(myIP.toStdString());
        if (currentNode == registrations.end()) {
            qDebug() << "Current node not found in registrations.";
            return;
        }

        auto nextNode = currentNode;
        do {
            ++nextNode; // Move to the next node
            if (nextNode == registrations.end()) {
                // Reached the end, loop back to the beginning
                nextNode = registrations.begin();
            }
        } while (nextNode->second.getType() != "metadata Analytics" && nextNode != currentNode);

        if (nextNode != currentNode && nextNode != registrations.end()) {
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
            // No next node of type "metadata Analytics" to forward the message, declare this node as the leader
            sendLeaderInfo(ipAddress);
        }
    } else if (receivedNumber < myNumber) {
        // Send the received number to the next node of type "metadata Analytics"
        auto currentNode = registrations.find(myIP.toStdString());
        if (currentNode == registrations.end()) {
            qDebug() << "Current node not found in registrations.";
            return;
        }

        auto nextNode = currentNode;
        do {
            ++nextNode; // Move to the next node
            if (nextNode == registrations.end()) {
                // Reached the end, loop back to the beginning
                nextNode = registrations.begin();
            }
        } while (nextNode->second.getType() != "metadata Analytics" && nextNode != currentNode);

        if (nextNode != currentNode && nextNode != registrations.end()) {
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
        qDebug() << "TIE" << "\n";
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
    qDebug() << registrations.size() << " ================>\n";
    for (const auto& node : analyticNodes) {
        QTcpSocket* clientSocket = node.second;
        if (clientSocket && clientSocket->state() == QAbstractSocket::ConnectedState) {
            clientSocket->write(nodeData);
            clientSocket->flush();
        }
    }
}

void metadataAnalytics::broadcastLeaderNodeInfo(const std::string& ipAddress) {
    qDebug() << "Broadcasting Leader Information";


    QJsonObject nodeObject;
    nodeObject["requestType"] = "Leader Announcement";
    nodeObject["leaderIP"] = QString::fromStdString(ipAddress);
    nodeObject["nodeType"] = QString::fromStdString(typeNode);
    //nodesArray.append(nodeObject);
    //}

    //QJsonObject parentNode;
    //parentNode["requestType"] = "Node Discovery";
    //parentNode["nodes"] = nodesArray;

    QJsonDocument doc(nodeObject);
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
        //qDebug() << "No nodes registered for election.";
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
    if (currentNode == registrations.end()) {
        qDebug() << "Current node not found in registrations.";
        return;
    }

    // Find the next eligible node of type "metadata Analytics"
    auto nextNode = currentNode;
    do {
        ++nextNode; // Move to the next node
        if (nextNode == registrations.end()) {
            // Reached the end, loop back to the beginning
            nextNode = registrations.begin();
        }
    } while (nextNode->second.getType() != "metadata Analytics" && nextNode != currentNode);

    // Send election message to the next eligible node
    if (nextNode != currentNode && nextNode != registrations.end()) {
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
        qDebug() << "No next node of type metadata Analytics to send election message to.";
        sendLeaderInfo(myIP.toStdString());
    }
}


void metadataAnalytics::sendLeaderInfo(const std::string& ipAddress) {
    // Send leader information to the node
    // QTcpSocket* leaderSocket = analyticNodes[ipAddress];
    // qDebug() << "here..." << "\n";
    // if(ipAddress == myIP.toStdString()){
    leader = QString::fromStdString(ipAddress);
    metadataAnalytics::broadcastLeaderNodeInfo(ipAddress);

    // }
    // if (leaderSocket && leaderSocket->state() == QAbstractSocket::ConnectedState) {
    //     QJsonObject leaderInfo;
    //     qDebug() << "here1..." << "\n";
    //     leaderInfo["requestType"] = "Leader Announcement";
    //     leaderInfo["message"] = "I am the leader.";
    //     leaderInfo["leaderIP"] = QString::fromStdString(ipAddress);
    //     leader = QString::fromStdString(ipAddress);
    //     QJsonDocument doc(leaderInfo);
    //     leaderSocket->write(doc.toJson());
    //     leaderSocket->flush();
    //     qDebug() << "Leader information sent to" << QString::fromStdString(ipAddress);
    // }
}

void metadataAnalytics::analyticsNodeDisconnected() {
    QTcpSocket *clientSocket = qobject_cast<QTcpSocket*>(sender());

    if (!clientSocket) return;

   // QString ipAddress = QHostAddress(clientSocket->peerAddress()).toString();
    QString ipAddress = clientSocket->peerAddress().toString();
    std::string addr= ipAddress.toStdString().substr(7);
    //if (addr.protocol() == QAbstractSocket::IPv6Protocol && addr.isIPv4Mapped()) {
       // addr =  // Skip the '::ffff:' part
    //}
    if(addr == leader.toStdString()){
        leader = "";
        qDebug() << "Insider info .... " << "\n";
        connect(electionTimer, &QTimer::timeout, this, &metadataAnalytics::conductElection);
        //electionTimer->start(3000); // 30 seconds
    }
    qDebug() << "Client disconnected, IP:" << ipAddress;
    deregisterNode(ipAddress); // Remove the disconnected node from registrations
    qDebug() << ipAddress.toStdString() << " klnon\n";
    analyticNodes.erase(ipAddress.toStdString()); // Erase the disconnected node from the analyticNodes map
    registrations.erase(ipAddress.toStdString());
    clientSocket->deleteLater();
    broadcastNodeInfo(); // Broadcast updated node information
}

int metadataAnalytics::generateNumber() {
    // Generate and return a random number
    return 42.0; // Placeholder, replace with actual random number generation logic
}
