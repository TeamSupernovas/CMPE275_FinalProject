#include <QtNetwork>
#include <QCoreApplication>
#include <QDebug>
#include <QSettings>
#include <QJsonDocument>
#include <QJsonObject>
#include <fstream>
#include <sstream>
#include <chrono> // For time-related functions
#include <thread> // For sleep_for

struct NodeInfo {
    QString nodeType;
    QString ip;
    double computingCapacity;
};

std::vector<std::string> replicasVector;

QTcpSocket clientSocket;
std::string leaderIP;
QMap<QString, NodeInfo> nodeMap;
int myNumber = 2;
QString myIp;

QString getLocalIpAddress() {
    return "192.168.1.105"; // Default to localhost if no suitable IP found
}

void sendQueryRequest() {
    QJsonObject queryRequest;
    queryRequest["requestType"] = "query";
    queryRequest["param"] = QJsonArray::fromVariantList(QVariantList() << QVariantList() << 0);

    QJsonDocument doc(queryRequest);
    QByteArray requestData = doc.toJson();

    clientSocket.write(requestData);
    clientSocket.flush();
}
void sendIngestionRequest(const std::vector<std::vector<std::string>>& records) {
    for (const auto& record : records) {
        QJsonObject ingestionRequest;
        ingestionRequest["requestType"] = "ingestion";

        // Create a nested QJsonArray for the record
        QJsonArray recordArray;
        for (const auto& value : record) {
            recordArray.append(QString::fromStdString(value));
        }

        // Add the record array to the data array
        QJsonArray dataArray;
        dataArray.append(recordArray);

        // Set the data array in the ingestion request
        ingestionRequest["data"] = dataArray;

        QJsonDocument doc(ingestionRequest);
        QByteArray requestData = doc.toJson();

        // Assuming clientSocket is your QWebSocket object
        clientSocket.write(requestData);
        clientSocket.flush();

        // Optionally, add a delay between each request
        // std::this_thread::sleep_for(std::chrono::seconds(2));
        // Sleep for 2 seconds before sending the next request
        std::this_thread::sleep_for(std::chrono::seconds(2));
    }
}


// Function to read data from the CSV file and send ingestion requests
void readAndSendCSVData(const std::string& filename) {
    std::ifstream file(filename);
    std::string line;

    // Read each line from the CSV file
    std::vector<std::vector<std::string>> allRecords;
    int rowIndex = 0;
    while (std::getline(file, line)) {
        // Parse the line into individual data points
        std::istringstream iss(line);
        std::string data;

        // Assuming each line contains comma-separated values
        std::vector<std::string> rowData;

        // Add the third column value with row index at the beginning of each row
        std::string thirdColumn;
        for (int i = 0; i < 3; ++i) {
            std::getline(iss, data, ',');
            if (i == 2) {
                thirdColumn = data + "@" + std::to_string(rowIndex);
            } else {
                rowData.push_back(data);
            }
        }
        rowData.insert(rowData.begin(), thirdColumn);

        while (std::getline(iss, data, ',')) {
            rowData.push_back(data);
        }

        allRecords.push_back(rowData);
        rowIndex++;
    }

    // Send ingestion request for all records
    sendIngestionRequest(allRecords);
}




int main(int argc, char *argv[]) {
    QCoreApplication app(argc, argv);

    // Read settings from configuration file
    QSettings settings("config.env", QSettings::IniFormat);
    QString serverIp = settings.value("SERVER_IP", "localhost").toString();
    int port = settings.value("PORT", 12351).toInt();

    clientSocket.connectToHost(serverIp, port);
    if (!clientSocket.waitForConnected(3000)) {
        qDebug() << "Failed to connect to server at" << serverIp << "on port" << port;
        return -1;
    }

    QString localIP = getLocalIpAddress();
    myIp = getLocalIpAddress();

    // Registration data sent to server upon connection
    QJsonObject registrationObject;
    registrationObject["requestType"] = "registering";
    registrationObject["ip"] = myIp;
    registrationObject["nodeType"] = "ingestion";
    registrationObject["computingCapacity"] = 0.4;
    QJsonDocument registrationDoc(registrationObject);
    QByteArray registrationData = registrationDoc.toJson();

    clientSocket.write(registrationData);
    clientSocket.waitForBytesWritten();
    // Start a timer to send query requests every 10 seconds
    //QTimer timer;
    //QObject::connect(&timer, &QTimer::timeout, sendQueryRequest);
    //timer.start(5000); // 10000 milliseconds = 10 seconds
    QTimer::singleShot(5000, sendQueryRequest);

    // Read CSV file and send ingestion requests
    readAndSendCSVData("test.csv");

    return app.exec();
}



