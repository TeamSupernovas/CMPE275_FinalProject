#include <QtNetwork>
#include <QCoreApplication>
#include <QDebug>
#include <QSettings> // Include QSettings for reading environment variables

QTcpSocket clientSocket;

void readData() {
    if (clientSocket.state() == QAbstractSocket::ConnectedState) {
        while (clientSocket.bytesAvailable() > 0) {
            QByteArray requestData = clientSocket.readAll();
            qDebug() << "Received data from server:" << requestData;

            if (requestData == "Heartbeat") {
                // Received heartbeat from server, send response
                clientSocket.write("I am alive: OK");
                clientSocket.waitForBytesWritten();
                qDebug() << "Response sent to server";
            }
        }
    }
}

int main(int argc, char *argv[]) {
    QCoreApplication a(argc, argv);

    // Read port number from environment file
    QSettings settings("config.env", QSettings::IniFormat);
    int port = settings.value("PORT").toInt();

    //qDebug() << "Port number read from config.env:" << port;

    clientSocket.connectToHost("localhost", port); // Use the port number read from environment

    if (!clientSocket.waitForConnected(3000)) {
        qDebug() << "Failed to connect to server!";
        return -1;
    }

    // Send registration data
    QByteArray registrationData = "Registration: Analytics Node"; // Change type as needed
    clientSocket.write(registrationData);
    clientSocket.waitForBytesWritten();

    // Connect signal for reading data from server
    QObject::connect(&clientSocket, &QTcpSocket::readyRead, &readData);

    return a.exec();
}
