#ifndef REGISTER_H
#define REGISTER_H

#include <string>

class Register {
public:
    Register() = default; // Default constructor
    Register(const std::string& ipAddress, const std::string& type, double computingCapacity)
        : ipAddress(ipAddress), type(type), computingCapacity(computingCapacity) {}

    std::string getIpAddress() const { return ipAddress; }
    std::string getType() const { return type; }
    double getComputingCapacity() const { return computingCapacity; }

private:
    std::string ipAddress;
    std::string type;
    double computingCapacity;
};

#endif // REGISTER_H
