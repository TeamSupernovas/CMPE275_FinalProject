#include "metadataAnalytics.h"
#include <QDebug>
#include <QJsonDocument>
#include <QJsonObject>
#include <QJsonArray>
#include <QTimer>
#include <sys/sysinfo.h> // Include for sysinfo struct
#include <QThread> // Include for QThread


/* TODO FLOW:
 *      - FIX ANOTHER TIME AGNOSTIC QUERY
 *      - CREATE ANOTHER FILE FOR METADATA ANALYTICS(KEEP THIS ONE FOR REGISTRATION)
 *          -   HAVE ALL THE THINGS UPDATED, I UPDATE ONLY SOME AT A TIME
 *          -   PREFER TO USE NODE MAP OVER REGISTRATION
 *      - LOAD BALANCING
 *      - INDEXING
 *
 *      NOTE: ANALYTIC MAP NAME IS MIS LEADING IT CONTAINS ALL THE INFO OF ALL THE INCOMING CONNECTIONS
 * /


/*
    TODO-1: THIS FILE IS FOR REGISTRATION NODE, SEPERATE THE CODE FOR METADATA ANALYTICS

    ---- FOR METADATA ANALYTICS ALL IT KNOWS FROM NODE DISCOVERY PHASE - TRY LEVERAGING NnodeMap
    --- FOR REGISTRATION NODE YOU CAN KEEP AS IT IS, FOR METADATA ANALYTICS CHANGE IT(CREATE DIFFERENT FILE)
*/


struct NodeInfo {
    QString nodeType;
    QString ip;
    double computingCapacity; // Include computing capacity in NodeInfo
};

struct ReplicaInfo {
    std::string replicaIP;
    bool isTrue;
};

int requestId = 0;
int requestIdQuery = 0;

// Define a new map to store replica information for each IP address
std::map<std::string, std::vector<ReplicaInfo>> replicaMap;
std::map<std::string, Register> metadataAnalytics::registrations;
std::map<std::string, QTcpSocket*> metadataAnalytics::analyticNodes;
std::map<std::string, NodeInfo> nodeMap; // New map to store node information
// Define a map to store data for each requestId
std::map<int, QByteArray> requestDataMap;
std::map<int, std::pair<std::string,double>> requestQuery;
int totalAnalyticsServers = 0;

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



    /*
    TODO-2: GOTTA CHANGE THIS TO STATIC IP FOR YOUR LAPTOP/NODE
    ---- WILL DO WHEN WE DO, THIS IS LAST STEP
    */

    return "192.168.1.102";  // Default to localhost if no suitable IP found
}
metadataAnalytics::metadataAnalytics(QObject *parent) : QTcpServer(parent) {

    /*
    TODO-3: ALL THE TIMES HERE NEEDED TO BE CHANGED, WE HAVE TO GET REALSISTIC ESTIMATES

    ---- THIS IS LAST STEP AGAIN
    */


    QTimer* timer = new QTimer(this);
    // connect(timer, &QTimer::timeout, this, &metadataAnalytics::monitorHeartbeat);
   // timer->start(5000);
    qDebug() << "metadataAnalytics created.";

    // Generate number and get IP address
    myNumber = generateNumber();
    myIP = getLocalIpAddress();
    // Set up election timer
    if(myIP.toStdString() == "192.168.1.102"){
        electionTimer = new QTimer(this);
        connect(electionTimer, &QTimer::timeout, this, &metadataAnalytics::conductElection);
        electionTimer->start(10000); // 30 seconds
    }

    // Initialize initAnalyticsTimer
    initAnalyticsTimer = new QTimer(this);
    connect(initAnalyticsTimer, &QTimer::timeout, this, &metadataAnalytics::initAnalytics);
    //electionTimer->start(1500); // 30 seconds
    // Start the initAnalyticsTimer with a single-shot
    initAnalyticsTimer->singleShot(7000, this, &metadataAnalytics::initAnalytics);
}


/*TODO-8: FOR ALL RELATED FUNCTION USE NODE MAP FOR BELOW THREE INSTEAD OF OTHERS FOR METADATA*/

// Function to initialize analytics and find replicas
void metadataAnalytics::initAnalytics() {
    // Collect IPs of all analytics nodes
    qDebug() << "===============" << "\n";

      /*
        TODO-8: BIG NO IMPLEMENTATION, TRY USING NODE INFO INSTEAD FOR METADATA ANALYTICS
         */


    std::vector<std::string> analyticsNodeIPs;
    for(const auto& reg : registrations) {
        if (QString::fromStdString(reg.second.getType()) == "analytics") {
            analyticsNodeIPs.push_back((reg.second.getIpAddress()));
        }
    }

    // Iterate over analytics nodes and distribute replicas
    for(const auto& reg : registrations) {
        if (reg.second.getType() == "analytics") {
            // Find replicas for this analytics node
            std::vector<std::string> replicas = findReplicas(reg.second.getIpAddress(), analyticsNodeIPs);

            // Store the replica information in the replicaMap
            std::vector<ReplicaInfo> replicaInfoList;
            for (const auto& replica : replicas) {
                // Assuming all replicas are initially marked as true
                replicaInfoList.push_back({replica, true});
            }
            replicaMap[reg.second.getIpAddress()] = replicaInfoList;

            // Construct API request
            QJsonObject requestData;
            requestData["requestType"] = "Init Analytics";
            QJsonArray replicasArray;
            for (const auto& replica : replicas) {
                replicasArray.append(QString::fromStdString(replica));
            }
            requestData["replicas"] = replicasArray;
            qDebug() << requestData;

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
// Function to update replica status
void metadataAnalytics::updateReplicaStatus(const std::string& ipAddress, const std::string& replicaIP, bool status) {
    auto it = replicaMap.find(ipAddress);
    if (it != replicaMap.end()) {
        // Search for the replica IP in the vector and update its status
        for (auto& replicaInfo : it->second) {
            if (replicaInfo.replicaIP == replicaIP) {
                replicaInfo.isTrue = status;
                qDebug() << "Updated status of replica" << QString::fromStdString(replicaIP)
                         << "for IP" << QString::fromStdString(ipAddress) << "to" << status;
                break;
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

    /*
    TODO-3: UPDATE IN OTHERS AS WELL NODE MAP, ONLY REGISTRATION

    ---- BASICALLY UPDATE EVERYTHING AT EVERY PLACE, AT SOME PLACES I AM UPDATING ONLY FEW PARTS
    ---- FOR EXAMPLE I MIGHT BE FORGETING TO UPDATE NODE MAP
    */
    if (!registeredSelf) {
        registerNode(myIP, typeNode, computeCapacityHeuristic());
        registeredSelf = true;
    }
    if (clientSocket->setSocketDescriptor(socketDescriptor)) {
        QString actualIP = clientSocket->peerAddress().toString();

        connect(clientSocket, &QTcpSocket::readyRead, this, &metadataAnalytics::readData);
        connect(clientSocket, &QTcpSocket::disconnected, this, &metadataAnalytics::analyticsNodeDisconnected);
        qDebug() << "Incoming connection established from IP:" << actualIP;
        //analyticNodes[actualIP.toStdString()] = clientSocket;


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

            /*
        TODO-4: CONSISTENT UPDATES FOR ALL CORRESSPONDING AND USE SOMETHING COMMON TO RETRIEVE

        ---- BASICALLY UPDATE EVERYTHING AT EVERY PLACE, AT SOME PLACES I AM UPDATING ONLY FEW PARTS
        ---- HERE I AM CLEARING NODE MAP, DON'T FORGET THE SELF REGISTRATION OR SELF DETAILS
         */
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
            /*
        TODO-4: CONSISTENT UPDATES FOR ALL CORRESSPONDING AND USE SOMETHING COMMON TO RETRIEVE

        ---- BASICALLY UPDATE EVERYTHING AT EVERY PLACE, AT SOME PLACES I AM UPDATING ONLY FEW PARTS
        ---- FOR EX: I AM NOT UPDATING MY NODE MAP HERE
        ---- USE PARAMETERS RECEIVED
        ---- FOR EX: I AM USING IPADDRESS FROM PEER ADDRESS BUT USE ONE YOU RECEIVED IN PAYLOAD
         */
            QString type = obj["nodeType"].toString();
            double computingCapacity = obj["computingCapacity"].toDouble(); // Extract computing capacity from JSON
            qDebug() << "Registration request from IP:" << ipAddress << "Type:" << type << "Computing Capacity:" << computingCapacity;
            analyticNodes[obj["ip"].toString().toStdString()] = clientSocket; /*TODO: CHANGE IP*/
            if(type.toStdString() == "analytics"){
                totalAnalyticsServers++;
            }
            registerNode(obj["ip"].toString(), type.toStdString(), computingCapacity);
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
            if(nodeType == "metadata Analytics"){
                leader =obj["leaderIP"].toString();
                if(leader != myIP){
                    //disconnect(electionTimer, &QTimer::timeout, this, &metadataAnalytics::conductElection);
                    disconnect(initAnalyticsTimer, &QTimer::timeout, this, &metadataAnalytics::initAnalytics);
                }
            }
            else{
                leaderIngestion = obj["leaderIP"].toString();
            }
            qDebug()<<"Wining Node"<<receivedNumber<<"...Type...."<<nodeType;
            broadcastNodeInfo();
        }
        else if (requestType == "ingestion") {
            qDebug() << "Received ingestion request.";
            // Increment request ID

            // Get the data from the ingestion request
            QJsonArray dataArray = obj["data"].toArray();
            std::vector<std::string> data;
            for (const auto& value : dataArray) {
                data.push_back(value.toString().toStdString());
            }
            // Attach request ID to the data
            QJsonObject requestData;
            //requestData["requestID"] = requestId;
            requestData["data"] = dataArray;

            // Convert JSON data to QByteArray
            QJsonDocument doc(requestData);
            QByteArray requestDataBytes = doc.toJson();

            // Send the data to one of the nodes of type "analytics"
            distributeDataToAnalyticsServers(requestDataBytes);
             qDebug() << "Sent out analytics request.";
        }
        else if (requestType == "analytics acknowledgment"){
            qDebug() << "Received analytics acknowledgment.";

            // Retrieve request ID from the JSON object
            int requestId = obj["requestId"].toInt();

            // Remove the request data from the map
            if (requestDataMap.find(requestId) != requestDataMap.end()) {
                requestDataMap.erase(requestId);
                qDebug() << "Request data removed from requestDataMap for request ID:" << requestId;
            } else {
                qDebug() << "Request ID not found in requestDataMap:" << requestId;
            }
        }
        else if(requestType == "query"){
            // Find nodes of type "analytics" and send the JSON payload to each of them
          //  qDebug() << "ytugijkbvgyuhjk\n";
            for (const auto& node : nodeMap) {
              //  qDebug() << nodeMap.size() << "\n";
                if (node.second.nodeType == "analytics") {
                    // Create a copy of the JSON object to send to each node
                   // qDebug() << node.second.nodeType << "\n";
                    QJsonObject queryObj = obj;

                    // Add the request ID to the JSON object
                    queryObj["requestId"] = requestIdQuery; // Replace 12345 with the actual request ID

                    // Convert the modified JSON object to a JSON document
                    QJsonDocument queryDoc(queryObj);

                    // Convert the JSON document to a byte array
                    QByteArray queryData = queryDoc.toJson();

                    // Send the JSON payload to the node
                    QTcpSocket *analyticsSocket = analyticNodes[node.second.ip.toStdString()];
                    if (analyticsSocket) {
                        analyticsSocket->write(queryData);
                        analyticsSocket->flush();
                        qDebug() << "Query sent to analytics node at IP:" << node.second.ip;
                    } else {
                        qDebug() << "Analytics node at IP:" << node.second.ip << "not found in analyticNodes.";
                    }
                }
            }
            requestIdQuery++;
        }

        else if(requestType == "query response"){
            // Retrieve information from the JSON object
            int requestId = obj["requestId"].toInt();
            QString maxArea = obj["maxArea"].toString();
            double maxAverage = obj["maxAverage"].toDouble();

            // Check if the new maximum average is greater than the current value
            if (requestQuery.find(requestId) == requestQuery.end() || maxAverage > requestQuery[requestId].second) {
                // Update the requestQuery map only if the new maximum average is greater
                requestQuery[requestId] = std::make_pair(maxArea.toStdString(), maxAverage);

            }
            qDebug() << "Updated requestQuery map - Request ID:" << requestId << ", Max Area:" << maxArea << ", Max Average:" << maxAverage;
        }




        else if(requestType == "analytics"){

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


/*TODO-8: FOR ALL RELATED FUNCTION USE NODE MAP INSTEAD OF REGISTARTIONS FOR BELOW NSTEAD OF OTHERS FOR METADATA*/
void metadataAnalytics::distributeDataToAnalyticsServers(const QByteArray& data) {
    // Get the data from the ingestion request
    QJsonDocument doc = QJsonDocument::fromJson(data);
    QJsonObject requestData = doc.object();
    QJsonArray dataArray = requestData["data"].toArray();

    // Determine the total number of analytics servers
   //  = analyticNodes.size();

    // Initialize requestId
    //int requestId = 0;

    // Iterate over each data point
    for (const auto& value : dataArray) {
        QJsonArray dataPoint = value.toArray();

        // Extract timestamp from data point (assuming it's the first element)
        //QString timestamp = dataPoint[0].toString();

        // Extract hour from the timestamp
        // Extract timestamp from data point (assuming it's the first element)
        //QString timestamp = dataPoint[0].toString();

        // Split the timestamp at the "@" symbol to get the time part
        // Extract timestamp from data point (assuming it's the first element)
        // Extract timestamp from data point (assuming it's the first element)
        QString timestamp = dataPoint[0].toString();
        int hour = 0;
        // Split the timestamp at the "@" symbol to get the time part
        QStringList dateTimeParts = timestamp.split('@');
       // qDebug() << dateTimeParts << "\n";
        if (dateTimeParts.size() >= 2) {
            QString timePart = dateTimeParts[0];

            // Split the time part at the ":" symbol to get hours and minutes
            QStringList timeParts = timePart.split(':');
            if (timeParts.size() >= 2) {
                QString hourString = timeParts[0];
                QString minuteString = timeParts[1];
                QStringList hourParts = hourString.split('T');
                // Convert hour and minute strings to integers
         //       qDebug() << hourString << "==================>\n";
                hour = hourParts[1].toInt();
                int minute = minuteString.toInt();

                // Now you have the hour and minute values
           //     qDebug() << "Hour:" << hour << ", Minute:" << minute;
            } else {
                qDebug() << "Invalid time format in timestamp:" << timestamp;
            }
        } else {
            qDebug() << "Invalid timestamp format:" << timestamp;
        }



        // Calculate which analytics server should receive the data based on the hour
        int serverIndex = (hour * totalAnalyticsServers) / 24;
        ++requestId;
        // Get the IP address of the analytics server
        std::string analyticsNodeIP;
        int currentIndex = 0;
        for (const auto& node : registrations) {
            if (node.second.getType() == "analytics") {
                if (currentIndex == serverIndex) {
                    analyticsNodeIP = node.first;
                    break;
                }
                currentIndex++;
            }
        }
        qDebug() << hour << "," << serverIndex << "," << analyticsNodeIP << "\n";
        // Send the data to the corresponding analytics server
        if (!analyticsNodeIP.empty()) {
            QTcpSocket* nodeSocket = analyticNodes[analyticsNodeIP];
            if (nodeSocket && nodeSocket->state() == QAbstractSocket::ConnectedState) {
                // Attach requestId and requestType to the data
                QJsonObject requestData;
                QJsonArray dataPointArray;
                dataPointArray.append(dataPoint);

                requestData["requestID"] = requestId;
                requestData["requestType"] = "analytics";
                requestData["data"] = dataPointArray;;
                // Convert JSON data to QByteArray
                QJsonDocument doc(requestData);
                QByteArray requestDataBytes = doc.toJson();
                requestDataMap[requestId] = requestDataBytes;
                // Send the data
                nodeSocket->write(requestDataBytes);
                nodeSocket->flush();
                qDebug() << "DONE------------------1" << "\n";
            } else {
                qDebug() << "Failed to send data to analytics node. Analytics node is not connected.";
            }
        } else {
            qDebug() << "No analytics node found to send data to.";
        }

        // Store the data in the requestDataMap

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

    /*
        TODO-6: BIG NO IMPLEMENTATION, TRY USING NODE INFO INSTEAD FOR METADATA ANALYTICS
        FOR REGISTRATION NODE, KEEP AS IT IS

         */

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
    parentNode["metadataAnalyticsLeader"] = leader;
    parentNode["metadataIngestionLeader"] = leaderIngestion;
    parentNode["initElectionIngestion"] = "192.168.1.109";
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
    NodeInfo info;
    info.nodeType = QString::fromStdString(type);
    info.ip = (ipAddress);
    info.computingCapacity = computingCapacity;

    // Add the new node info to the nodeMap
    nodeMap[ipAddress.toStdString()] = info;
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

    /*
        TODO-7: BIG NO IMPLEMENTATION, TRY USING BROADCASTNODEINFO INSTEAD LEADERINFO
         */
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
