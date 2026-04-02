#pragma once

#include <stdexcept>
#include <string>

// Base class for any server exception
class ServerException : public std::runtime_error {
public:
    ServerException(std::string const& msg);
};

// Exception after which server must terminate
class ServerErrorException : public ServerException {
public:
    ServerErrorException(std::string const& msg);
};

// Exception caused by some client bad behavior
class ClientException : public ServerException {
public:
    ClientException(std::string const& msg);
};