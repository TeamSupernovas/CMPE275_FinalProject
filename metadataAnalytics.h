#ifndef SERVER_H
#define SERVER_H

#include <QtNetwork>
#include <QObject>
#include <map>
#include <string>
#include "register.h" // Include the Register class header

class metadataAnalytics : public QTcpServer {
    Q_OBJECT
public:
    explicit metadataAnalytics(QObject *parent = nullptr);
    virtual ~metadataAnalytics();

protected:
    void incomingConnection(qintptr socketDescriptor) override;

private slots:
    void readData();
    void analyticsNodeDisconnected(); // New slot declaration for handling client disconnections

private:
    static std::map<std::string, Register> registrations; // Store registrations
    static std::map<std::string, QTcpSocket*> analyticNodes; // Store client sockets
    void monitorHeartbeat(); // Function declaration for monitoring heartbeats
};

#endif // metadataAnalytics_H
