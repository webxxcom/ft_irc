#ifndef EXCEPTIONS_HPP
#define EXCEPTIONS_HPP

#include "irc.hpp"
#include <exception>

class ServerException : public std::exception {
    private:
        std::string _msg;
    public:
        ServerException(std::string msg) : _msg(msg) {};
        virtual ~ServerException() throw() {};
        virtual const char *what() const throw() {
            return _msg.c_str();
        };
} ;

#endif