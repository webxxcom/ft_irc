#ifndef EXCEPTIONS_HPP
#define EXCEPTIONS_HPP

#include "irc.hpp"
#include <exception>

//does excaption class need orthodox canonical form also? 
//separate definition needed?
// ! ServerException, without `s'
class ServerExceptions : public std::exception {
    private:
        std::string _msg;
    public:
        ServerExceptions(std::string msg) : _msg(msg) {};
        virtual ~ServerExceptions() throw() {};
        virtual const char *what() const throw() {
            return _msg.c_str();
        };
} ;

#endif