#ifndef METADATA_ANALYTICS_H
#define METADATA_ANALYTICS_H

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
    void registerNode(const QString& ipAddress, const std::string& type, double computingCapacity); // New function for registering nodes
    void deregisterNode(const QString& ipAddress); // New function for deregistering nodes
    void conductElection(); // New function for conducting elections
    void sendLeaderInfo(const std::string& ipAddress); // New function for sending leader information
    double computeCapacityHeuristic();
    int generateNumber(); // Declaration for computeCapacityHeuristic
    void processElectionMessage(int receivedNumber, const std::string& ipAddress);
    void broadcastLeaderNodeInfo(const std::string& ipAddress);

private:
    static std::map<std::string, Register> registrations; // Store registrations
    static std::map<std::string, QTcpSocket*> analyticNodes; // Store client sockets
    bool registeredSelf = false; // Flag to track self-registration
    std::string typeNode = "metadata Analytics"; // Type of the node
    QString myIP; // Store current node's IP address
    int myNumber; // Store generated number for the current node
    QString leader = "";
    QTimer* electionTimer;
};

#endif // METADATA_ANALYTICS_H
