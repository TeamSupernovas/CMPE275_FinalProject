#include <QtNetwork>
#include <QCoreApplication>
#include <QDebug>
#include <QSettings>
#include <QJsonDocument>
#include <QJsonObject>
#include <QJsonArray>
#include <QMap>
#include <QThread>
#include <vector>
#include <map>

struct NodeInfo
{
    QString nodeType;
    QString ip;
    double computingCapacity;
};

std::vector<std::string> replicasVector;
std::map<std::string, QTcpSocket *> replicaSockets;
QTcpSocket clientSocket;
std::string leaderIP;
QMap<QString, NodeInfo> nodeMap;
int myNumber = 2;
QString myIp;
std::map<QString, double> areaToAverageMap;
std::map<QString, double> countMap;
std::map<QString, std::map<QString, double>> receivedAnalyticsDataMap;
std::string siteName = "No Analytics to Calculate Max AQI";
double maxAQI = -1.0;

QString getLocalIpAddress()
{
    return "192.168.1.105";
}

double computeCapacityHeuristic()
{
    return 0.4;
}

void sendElectionMessage(int number, const QString &ip)
{
    QJsonObject electionMessage;
    electionMessage["requestType"] = "Election Message";
    electionMessage["Current Number"] = number;
    electionMessage["IP"] = ip;
    QJsonDocument doc(electionMessage);
    QByteArray messageData = doc.toJson();

    clientSocket.write(messageData);
    clientSocket.flush();
}

void broadcastLeaderInfo(const QString &leaderIP)
{
    QJsonObject leaderInfo;
    leaderInfo["requestType"] = "Leader Announcement";
    leaderInfo["leaderIP"] = leaderIP;
    leaderInfo["nodeType"] = "metadata Analytics";
    QJsonDocument doc(leaderInfo);
    QByteArray leaderData = doc.toJson();
    qDebug() << nodeMap.size() << "********\n";

    for (const auto &node : nodeMap)
    {
        if (node.ip != getLocalIpAddress())
        {
            qDebug() << node.ip << "  -----\n";
            clientSocket.write(leaderData);
            clientSocket.waitForBytesWritten();
            qDebug() << "Leader info broadcasted to:" << node.ip;
        }
    }
}

void sendAnalyticsCompletedMessage(int requestId)
{
    QJsonObject completionObj;
    completionObj["requestType"] = "analytics acknowledgment";
    completionObj["requestId"] = requestId;

    QJsonDocument doc(completionObj);
    QByteArray completionData = doc.toJson();

    clientSocket.write(completionData);
    clientSocket.flush();
    qDebug() << "Analytics completion message sent to leader IP:" << leaderIP;
}

void broadcastAnalyticsData()
{
    QJsonObject dataObject;
    QJsonObject countMapJson;
    QJsonObject areaToAverageMapJson;

    for (const auto &entry : countMap)
    {
        countMapJson[entry.first] = entry.second;
    }

    for (const auto &entry : areaToAverageMap)
    {
        areaToAverageMapJson[entry.first] = entry.second;
    }

    dataObject["requestType"] = "Analytics Data";
    dataObject["countMap"] = countMapJson;
    dataObject["areaToAverageMap"] = areaToAverageMapJson;

    QJsonDocument doc(dataObject);
    QByteArray data = doc.toJson();

    for (const auto &replica : replicaSockets)
    {
        QTcpSocket *socket = replica.second;
        if (socket->state() == QAbstractSocket::ConnectedState)
        {
            socket->write(data);
            socket->waitForBytesWritten();
            qDebug() << "Analytics data broadcasted to:" << QString::fromStdString(replica.first);
        }
        else
        {
            qDebug() << "Failed to connect to replica:" << QString::fromStdString(replica.first);
        }
    }
}

void handleAnalyticsRequest(const QJsonObject &obj)
{
    qDebug() << "Received analytics request.";

    // Extract data from the received JSON
    if (obj.contains("data") && obj["data"].isArray())
    {
        QJsonArray dataArray = obj["data"].toArray();

        // qDebug() << "Received analytics request. Data array size:" << dataArray.size();
        // qDebug() << dataArray << "\n";
        // Iterate through the data array to calculate average for each area
        for (const auto &value : dataArray)
        {
            if (value.isArray())
            {
                QJsonArray dataPoint = value.toArray();
                // qDebug() << "Data point array:" << dataPoint.at(8);

                if (dataPoint.size() >= 11)
                {
                    QString area = dataPoint.at(9).toString();                   // Assuming 11th column is the area
                    int value1 = stoi(dataPoint.at(8).toString().toStdString()); // Assuming 10th column contains the value to average
                    qDebug() << "#####Area:" << area << ", Value:" << value1 << "######";

                    if (value1 > maxAQI)
                    {
                        maxAQI = value1;
                        siteName = area.toStdString();
                    }

                    // Check if the area already exists in the map
                    if (areaToAverageMap.find(area) != areaToAverageMap.end())
                    {
                        // Update the average for the area
                        areaToAverageMap[area] += value1;
                        countMap[area] += 1;
                        areaToAverageMap[area] /= countMap[area];
                    }
                    else
                    {
                        // Initialize the average for the area
                        areaToAverageMap[area] = value1;
                        countMap[area] = 1;
                        areaToAverageMap[area] /= countMap[area];
                    }
                }
                else
                {
                    qDebug() << "Invalid data point format: " << dataPoint;
                }
            }
            else
            {
                qDebug() << "Invalid data value format: " << value;
            }
        }

        // Calculate the average for each area
        for (auto &entry : areaToAverageMap)
        {
            entry.second /= dataArray.size(); // Divide the sum by the number of data points
        }

        // Print the calculated averages
        for (const auto &entry : areaToAverageMap)
        {
            qDebug() << "Area:" << entry.first << ", Average:" << entry.second;
        }

        // Send completion message to leaderIP
        int requestId = (obj["requestID"].toInt());
        broadcastAnalyticsData();
        sendAnalyticsCompletedMessage(requestId);
    }
    else
    {
        qDebug() << "Data array not found or is not an array in the received analytics request.";
    }
}

void handleReceivedAnalyticsData(const QJsonObject &obj, const QString &senderIp)
{
    if (obj.contains("countMap") && obj.contains("areaToAverageMap"))
    {
        QJsonObject countMapJson = obj["countMap"].toObject();
        QJsonObject areaToAverageMapJson = obj["areaToAverageMap"].toObject();

        std::map<QString, double> analyticsData;

        for (const QString &key : areaToAverageMapJson.keys())
        {
            analyticsData[key] = areaToAverageMapJson[key].toDouble();
        }

        receivedAnalyticsDataMap[senderIp] = analyticsData;
        qDebug() << "Received analytics data stored from IP:" << senderIp;
    }
    else
    {
        qDebug() << "Invalid analytics data format received from IP:" << senderIp;
    }
}

void readData()
{
    while (clientSocket.bytesAvailable() > 0)
    {
        QByteArray requestData = clientSocket.readAll();
        qDebug() << "Received data from server:" << requestData;

        QJsonDocument doc = QJsonDocument::fromJson(requestData);
        if (doc.isNull())
        {
            qDebug() << "Failed to parse JSON data";
            return;
        }

        QJsonObject obj = doc.object();
        QString requestType = obj["requestType"].toString();
        qDebug() << requestType << "-------\n";
        if (requestType == "Heartbeat")
        {
            QJsonObject responseObj;
            responseObj["requestType"] = "Heartbeat Response";
            responseObj["message"] = "I am alive";
            responseObj["status"] = "OK";

            QJsonDocument responseDoc(responseObj);
            QByteArray responseData = responseDoc.toJson();

            clientSocket.write(responseData);
            clientSocket.waitForBytesWritten();
            qDebug() << "Heartbeat response sent to server.";
        }
        else if (requestType == "Node Discovery")
        {
            QJsonArray nodes = obj["nodes"].toArray();
            nodeMap.clear();
            for (const QJsonValue &value : nodes)
            {
                QJsonObject nodeObj = value.toObject();
                NodeInfo info;
                info.nodeType = nodeObj["nodeType"].toString();
                info.ip = nodeObj["IP"].toString();
                info.computingCapacity = nodeObj["computingCapacity"].toDouble();
                nodeMap[info.ip] = info;
                qDebug() << "Discovered Node IP:" << info.ip
                         << "Type:" << info.nodeType
                         << "Computing Capacity:" << info.computingCapacity;
            }
        }
        else if (requestType == "Election Message")
        {
            int receivedNumber = obj["Current Number"].toInt();
            QString receivedIP = obj["IP"].toString();
            if (receivedNumber < myNumber)
            {
                sendElectionMessage(receivedNumber, receivedIP);
            }
            else if (receivedNumber > myNumber)
            {
                sendElectionMessage(myNumber, myIp);
            }
            else
            {
                qDebug() << receivedIP << "  =====\n";
                broadcastLeaderInfo(receivedIP);
            }
        }
        else if (requestType == "Leader Announcement")
        {
            std::string receivedNumber = obj["leaderIP"].toString().toStdString();
            std::string nodeType = obj["nodeType"].toString().toStdString();
            leaderIP = receivedNumber;
            qDebug() << "Winning Node" << receivedNumber << "...Type...." << nodeType;
        }
        else if (requestType == "Init Analytics")
        {
            qDebug() << "Init Analytics request received";
            qDebug() << obj << "\n";
            QJsonArray replicasArray = obj["replicas"].toArray();

            for (const auto &replica : replicasArray)
            {
                std::string replicaIP = replica.toString().toStdString();
                replicasVector.push_back(replicaIP);

                QTcpSocket *socket = new QTcpSocket();
                socket->connectToHost(QString::fromStdString(replicaIP), 12351); // Assuming the port is 12351
                if (socket->waitForConnected(3000))
                {
                    replicaSockets[replicaIP] = socket;
                    qDebug() << "Connected to replica:" << QString::fromStdString(replicaIP);
                }
                else
                {
                    qDebug() << "Failed to connect to replica:" << QString::fromStdString(replicaIP);
                    delete socket;
                }
            }

            for (const auto &replica : replicasVector)
            {
                qDebug() << QString::fromStdString(replica);
            }
        }
        else if (requestType == "analytics")
        {
            handleAnalyticsRequest(obj);
        }
        else if (requestType == "Analytics Data")
        {
            QString senderIp = clientSocket.peerAddress().toString();
            handleReceivedAnalyticsData(obj, senderIp);
        }
        else if (requestType == "query")
        {
            // Find the area with the maximum AQI average
            qDebug() << "###############################################" << "\n";
            QString maxArea;
            double maxAverage = -1;
            QString queryType = obj["query"].toString();

            if (queryType == "0")
            {
                // Sitename with max average AQI
                for (const auto &entry : areaToAverageMap)
                {
                    if (entry.second > maxAverage)
                    {
                        maxArea = entry.first;
                        maxAverage = entry.second;
                    }
                }

                // Get the request ID from the received JSON object
                int requestId = obj["requestId"].toInt();

                // Construct the response JSON object
                QJsonObject responseObject;
                responseObject["requestType"] = "query response";
                responseObject["requestId"] = requestId; // Include the request ID in the response
                responseObject["maxArea"] = maxArea;
                responseObject["maxAverage"] = maxAverage;

                // Convert JSON object to JSON document
                QJsonDocument responseDoc(responseObject);
                QByteArray responseData = responseDoc.toJson();

                // Send the response data back to the client
                clientSocket.write(responseData);
                clientSocket.flush();
            }
            else if (queryType == "1")
            {
                // Sitename with max AQI
                QString siteName;
                double maxAQI = -1;

                for (const auto &entry : areaToAverageMap)
                {
                    if (entry.second > maxAQI)
                    {
                        siteName = entry.first;
                        maxAQI = entry.second;
                    }
                }

                // Get the request ID from the received JSON object
                int requestId = obj["requestId"].toInt();

                // Construct the response JSON object
                QJsonObject responseObject;
                responseObject["requestType"] = "query response";
                responseObject["requestId"] = requestId; // Include the request ID in the response
                responseObject["maxArea"] = siteName;
                responseObject["maxAqi"] = maxAQI;

                qDebug() << "max Aqi is" << maxAQI << "for this site" << siteName << "\n";

                // Convert JSON object to JSON document
                QJsonDocument responseDoc(responseObject);
                QByteArray responseData = responseDoc.toJson();

                // Send the response data back to the client
                clientSocket.write(responseData);
                clientSocket.flush();
            }
            else
            {
                qDebug() << requestType << "\n";
            }
        }
    }
}

int main(int argc, char *argv[])
{
    QCoreApplication app(argc, argv);

    QSettings settings("config.env", QSettings::IniFormat);
    QString serverIp = settings.value("SERVER_IP", "localhost").toString();
    qDebug() << serverIp << "\n";
    int port = settings.value("PORT", 12351).toInt();

    clientSocket.connectToHost(serverIp, port);
    if (!clientSocket.waitForConnected(3000))
    {
        qDebug() << "Failed to connect to server at" << serverIp << "on port" << port;
        return -1;
    }

    QString localIP = getLocalIpAddress();
    myIp = getLocalIpAddress();
    qDebug() << myIp << " niokmn vkguyijkn\n";

    qDebug() << localIP;
    QJsonObject registrationObject;
    registrationObject["requestType"] = "registering";
    registrationObject["ip"] = myIp;
    registrationObject["nodeType"] = "analytics";
    registrationObject["computingCapacity"] = computeCapacityHeuristic();
    QJsonDocument registrationDoc(registrationObject);
    QByteArray registrationData = registrationDoc.toJson();

    clientSocket.write(registrationData);
    clientSocket.waitForBytesWritten();

    QObject::connect(&clientSocket, &QTcpSocket::readyRead, &readData);

    myNumber = 3;

    return app.exec();
}
