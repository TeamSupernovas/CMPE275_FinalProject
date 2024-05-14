#include <QtNetwork>
#include <QCoreApplication>
#include <QDebug>
#include <QSettings>
#include <QJsonDocument>//#include <QtNetwork>
#include <QCoreApplication>
#include <QDebug>
#include <QSettings>
#include <QJsonDocument>
#include <QJsonObject>
#include <QJsonArray>
#include <QMap>
// #include <sys/sysinfo.h> // Include for sysinfo struct
#include <QThread> // Include for QThread

struct NodeInfo {
    QString nodeType;
    QString ip;
    double computingCapacity; // Include computing capacity in NodeInfo
};

std::vector<std::string> replicasVector;

QTcpSocket clientSocket;
std::string leaderIP;
QMap<QString, NodeInfo> nodeMap; // Map to store node information by IP address
int myNumber = 2; // Store the generated number for this node
QString myIp;

QString getLocalIpAddress() {
    // QList<QHostAddress> list = QNetworkInterface::allAddresses();
    // for (int nIter = 0; nIter < list.count(); nIter++) {
    //     if (list[nIter].protocol() == QAbstractSocket::IPv4Protocol && !list[nIter].isLoopback()) {
    //         qDebug() << "Local IPv4 Address found:" << list[nIter].toString();
    //         return list[nIter].toString();
    //     }
    // }
    // qDebug() << "No non-loopback IPv4 address found, defaulting to localhost.";
    return "192.168.1.101";  // Default to localhost if no suitable IP found
}

double computeCapacityHeuristic() {
    // // RAM
    // struct sysinfo info;
    // sysinfo(&info); // Get system information

    // // Normalize RAM size to range between 0 and 1
    // double ramCapacity = static_cast<double>(info.totalram) / (1024 * 1024 * 1024); // Convert to GB
    // double normalizedRAM = ramCapacity / 16.0; // Assuming a maximum RAM capacity of 16 GB

    // // CPU Cores
    // int numCores = QThread::idealThreadCount(); // Get the number of CPU cores
    // double normalizedCores = static_cast<double>(numCores) / QThread::idealThreadCount();

    // // Assume weights for each factor (adjust as needed)
    // const double weightRAM = 0.5;
    // const double weightCores = 0.5;

    // // Compute weighted average
    // double weightedAverage = (normalizedRAM * weightRAM) + (normalizedCores * weightCores);

    // return weightedAverage;
    return 0.4;
}

void sendElectionMessage(int number, const QString& ip) {
    QJsonObject electionMessage;
    electionMessage["requestType"] = "Election Message";
    electionMessage["Current Number"] = number;
    electionMessage["IP"] = ip;
    QJsonDocument doc(electionMessage);
    QByteArray messageData = doc.toJson();

    clientSocket.write(messageData);
    clientSocket.flush();
}

void broadcastLeaderInfo(const QString& leaderIP) {
    // Broadcast leader information to all nodes
    QJsonObject leaderInfo;
    leaderInfo["requestType"] = "Leader Announcement";
    leaderInfo["leaderIP"] = leaderIP;
    leaderInfo["nodeType"]="metadata Analytics";
    QJsonDocument doc(leaderInfo);
    QByteArray leaderData = doc.toJson();
    qDebug()<<nodeMap.size()<<"********\n";

    for (const auto& node : nodeMap) {
        if(node.ip!=getLocalIpAddress()){
            qDebug() << node.ip << "  -----\n";
            clientSocket.write(leaderData);
            clientSocket.waitForBytesWritten(); // Wait for data to be written to the socket
            qDebug() << "Leader info broadcasted to:" << node.ip;
        }
    }
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
        qDebug()<< requestType<<"-------\n";
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
                info.computingCapacity = nodeObj["computingCapacity"].toDouble(); // Store computing capacity in NodeInfo
                nodeMap[info.ip] = info;
                qDebug() << "Discovered Node IP:" << info.ip
                         << "Type:" << info.nodeType
                         << "Computing Capacity:" << info.computingCapacity; // Print computing capacity
            }
        } else if (requestType == "Election Message") {
            int receivedNumber = obj["Current Number"].toInt();
            QString receivedIP = obj["IP"].toString(); // Received IP address
            if (receivedNumber < myNumber) {
                // Ignore the message, as the received number is smaller than my number
                // In this case, send the next node the received node and received IP
                sendElectionMessage(receivedNumber, receivedIP);
            } else if (receivedNumber > myNumber) {
                // Forward the message to the next node
                sendElectionMessage(myNumber, myIp);
            } else {
                // Election is over, broadcast leader information
                qDebug() << receivedIP << "  =====\n";
                broadcastLeaderInfo(receivedIP);
            }
        }
        else if (requestType == "Leader Announcement") {

            // QJsonObject obj = value.toObject();
            std::string receivedNumber = obj["leaderIP"].toString().toStdString();
            std::string nodeType=obj["nodeType"].toString().toStdString();
            leaderIP=receivedNumber;
            qDebug()<<"Wining Node"<<receivedNumber<<"...Type...."<<nodeType;
        }
        else if (requestType == "Init Analytics") {
            qDebug() << "Init Analytics request received";
            qDebug() << obj << "\n";
            // Extract replicas from the received JSON data
            QJsonArray replicasArray = obj["replicas"].toArray();

            for (const auto& replica : replicasArray) {
                replicasVector.push_back(replica.toString().toStdString());
            }

            for (const auto& replica : replicasVector) {
                qDebug() << QString::fromStdString(replica);
            }
        }

    }
}





int main(int argc, char *argv[]) {
    QCoreApplication app(argc, argv);

    // Read settings from configuration file
    QSettings settings("config.env", QSettings::IniFormat);
    //QString serverIp = settings.value("SERVER_IP", "192.168.1.102").toString();
    QString serverIp = settings.value("SERVER_IP", "localhost").toString();
    qDebug() << serverIp << "\n";
    int port = settings.value("PORT", 12351).toInt(); // Default port if not specified

    clientSocket.connectToHost(serverIp, port);
    if (!clientSocket.waitForConnected(3000)) {
        qDebug() << "Failed to connect to server at" << serverIp << "on port" << port;
        return -1;
    }

    QString localIP = getLocalIpAddress();  // Get the local IP address
    myIp = getLocalIpAddress();
    qDebug() << myIp << " niokmn vkguyijkn\n";

    qDebug() << localIP;
    // Registration data sent to server upon connection
    QJsonObject registrationObject;
    registrationObject["requestType"] = "registering";
    registrationObject["ip"] =myIp;  // Send the actual IP address discovered
    registrationObject["nodeType"] = "metadata Analytics";
    registrationObject["computingCapacity"] = computeCapacityHeuristic(); // Send computing capacity heuristic
    QJsonDocument registrationDoc(registrationObject);
    QByteArray registrationData = registrationDoc.toJson();

    clientSocket.write(registrationData);
    clientSocket.waitForBytesWritten();

    QObject::connect(&clientSocket, &QTcpSocket::readyRead, &readData);

    // Generate a number for this node (you need to implement this function)
    myNumber = 2; // Assuming a function 'generateNumber()' is implemented

    return app.exec();
}
