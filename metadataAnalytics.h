#ifndef SERVER_H
#define SERVER_H

#include <QtNetwork>
#include <QObject>
#include <map>
#include <string>
#include "register.h" // Assuming this class correctly defines a Register struct or class

class metadataAnalytics : public QTcpServer {
    Q_OBJECT
public:
    explicit metadataAnalytics(QObject *parent = nullptr);
    virtual ~metadataAnalytics();

protected:
    void incomingConnection(qintptr socketDescriptor) override;

private slots:
    void readData();
    void analyticsNodeDisconnected();
    void monitorHeartbeat();
    void broadcastNodeInfo(); // New function for broadcasting node info

private:
    static std::map<std::string, Register> registrations; // Store registrations
    static std::map<std::string, QTcpSocket*> analyticNodes; // Store client sockets
};

#endif // SERVER_H
