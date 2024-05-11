// register.h
#ifndef REGISTER_H
#define REGISTER_H

#include <string>

class Register {
public:
    Register() = default; // Default constructor
    Register(const std::string& ipAddress, const std::string& type)
        : ipAddress(ipAddress), type(type) {}

    std::string getIpAddress() const { return ipAddress; }
    std::string getType() const { return type; }

private:
    std::string ipAddress;
    std::string type;
};

#endif // REGISTER_H
