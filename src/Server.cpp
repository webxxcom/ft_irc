#include "Server.hpp"
#include "irc.hpp"

Server::Server(int ac, char *av[]) {
    int status = this->parseArgs(ac, av);
    if (status == 1)
        throw ServerExceptions("Arguments invalid\nRun with: ./irc <port> <password>");
    else if (status == 2)
        throw ServerExceptions("Port number is invalid");
    std::cout << "Server created" << std::endl;
}

//
//todo
Server::Server(const Server &orig) {
    (void)orig;
}
Server &Server::operator=(const Server &orig) {
    (void)orig;
    return *this;
}
Server::~Server(){}
//

int Server::parseArgs(int ac, char *av[]) {
    if (ac != 3)
        return 1;
    int p = std::atoi(av[1]);
    std::string strPort(av[1]);
    if (p == 0 && !strPort.compare("0"))
        return 2;
    if (p < 1024 || p > 65536)
        return 2;
    this->_port = p;
    this->_password = av[2];
    return 0;
}

// void Server::startServer(void) {
//     struct sockaddr_in s;
//     s.sin_family = AF_INET;
//     s.sin_addr.s_addr = INADDR_ANY;
//     s.sin_port = htons(this->_port);
//     this->_socketfd = socket(AF_INET, SOCK_STREAM, 0);
//     if (this->_socketfd == -1)
//         throw ServerExceptions("Socket not created");
//     setsockopt(this->_socketfd, )
    
    
// }

