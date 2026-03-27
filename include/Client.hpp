#ifndef CLIENT_HPP
#define CLIENT_HPP

#include "irc.hpp"

class Client {
    private:
        int             _clientfd;
        std::string     _nickName;
        std::string     _userName;
        std::string     _address;
        std::vector<std::string>     _out;
    public:
        Client();
        Client(int fd);
        Client(const Client &orig);
        Client&operator=(const Client &orig);
        ~Client();
        void parseCommands(std::string msg);

        std::vector<std::string>& getReceivedMessages();
} ;


#endif