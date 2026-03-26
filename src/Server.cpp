#include "Server.hpp"
#include "irc.hpp"

//for properly closing fds later, probably have to redo because of try/catch
Server::Server(int ac, char *av[]) : _serverSocketfd(-1) {
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

void Server::setupServer(void) {
    struct sockaddr_in s;
    memset(&s, 0, sizeof(s));
    s.sin_family = AF_INET;
    s.sin_addr.s_addr = INADDR_ANY;
    s.sin_port = htons(this->_port);
    struct pollfd serverfd;
    this->_serverSocketfd = socket(AF_INET, SOCK_STREAM, 0);
    if (this->_serverSocketfd == -1)
        throw ServerExceptions("socket() error");
    int on = 1;
    if (setsockopt(this->_serverSocketfd, SOL_SOCKET, SO_REUSEADDR, &on, sizeof(on)) == -1) {
        close(this->_serverSocketfd);
        throw ServerExceptions("setsockopt() error");
    }
    //if fcntl can be used
    if (fcntl(this->_serverSocketfd, F_SETFL, O_NONBLOCK) == -1) {
        close(this->_serverSocketfd);
        throw ServerExceptions("fcntl() error");
    }
    if (bind(this->_serverSocketfd, (struct sockaddr*)&s, sizeof(s)) == -1) {
        close(this->_serverSocketfd);
        throw ServerExceptions("bind() error");
    }
    if (listen(this->_serverSocketfd, SOMAXCONN) == -1) {
        close(this->_serverSocketfd);
        throw ServerExceptions("listen() error");
    }
    serverfd.fd = this->_serverSocketfd;
    serverfd.events = POLLIN;
    serverfd.revents = 0;
    this->_pollfds.push_back(serverfd);
}

void Server::acceptClient(void) {
    struct sockaddr_in c;
    memset(&c, 0, sizeof(c));
    socklen_t lenc = sizeof(c);
    int clientSocketfd = accept(this->_serverSocketfd, (struct sockaddr*)&c, &lenc);
    if (clientSocketfd == -1) {
        std::cerr << "accept() error" << std::endl;
        return ;
    }
    //fcntl if could be used
    if (fcntl(clientSocketfd, F_SETFL, O_NONBLOCK) == -1) {
        std::cerr << "fcntl() error" << std::endl;
        close(clientSocketfd);
        return ;
    }
    struct pollfd clientfd;
    clientfd.fd = clientSocketfd;
    clientfd.events = POLLIN;
    clientfd.revents = 0;
    this->_pollfds.push_back(clientfd);
    Client newClient(clientSocketfd);
    this->_clients[clientSocketfd] = newClient;
}

void Server::removeClient(int &i) {
    close(this->_pollfds[i].fd);
    this->_clients.erase(this->_pollfds[i].fd);
    this->_pollfds.erase(this->_pollfds.begin() + i);
    i--;
}

void Server::receiveClientData(int &i) {
    char buffer[1024];
    memset(buffer, 0, sizeof(buffer));
    int bytesread;
    bytesread = recv(this->_pollfds[i].fd, buffer, (sizeof(buffer) - 1), 0);
    if (bytesread <= 0) {
        std::cout << bytesread << std::endl;
        if (bytesread == 0)
            std::cout << "Client disconnected" << std::endl;
        else
            std::cerr << "recv() error" << std::endl;
        removeClient(i);
        return ;
    }
    this->_clients[this->_pollfds[i].fd].addtoBuffer(buffer);
    //receive data
    //wait for \r\n, add to command to parse (probably Roman)
    //also check here when client disconnects, clean up
}

void Server::messageClient(void) {
    //to sent data to client, need to frist msg the buffer
}

void Server::startServer(void) {
    setupServer();
    while (g_serverRunning) {
        int status = poll(&this->_pollfds[0], this->_pollfds.size(), -1);
        if (status == -1 && g_serverRunning == 0)
            throw ServerExceptions("\nsignal catched");
        if (status == -1)
            throw ServerExceptions("poll() error");
        for (int i = 0; i < static_cast<int>(this->_pollfds.size()); i++) {
            if (this->_pollfds[i].revents == 0)
                continue;
            else if (this->_pollfds[i].fd == this->_serverSocketfd 
                && this->_pollfds[i].revents & POLLIN) {
                    acceptClient();
            }
            else if (this->_pollfds[i].revents & POLLIN) {
                receiveClientData(i);
            }
            else if (this->_pollfds[i].revents & POLLOUT) {
                messageClient();
            }
            else if (this->_pollfds[i].revents & (POLLERR | POLLHUP | POLLNVAL)) {
                //issue with client, need to remove him, free fd, erase from all lists
            }
        }
    }
}

