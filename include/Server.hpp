#ifndef SERVER_HPP
#define SERVER_HPP

#include "irc.hpp"

class Server {
    private:
        std::string _password;
        int         _port;
        // pid_t       _socketfd;
        //vetors or maps to save clients fd;
        //methods for setting server;

        int parseArgs(int ac, char *av[]);
    public:
        Server(int ac, char *av[]);
        Server(const Server &orig);
        Server&operator=(const Server &orig);
        ~Server();
        // void startServer();
        // void finishServer();
} ;

#endif