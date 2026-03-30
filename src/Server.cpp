#include "Server.hpp"
#include "irc.hpp"
#include <cstring>
#include <sstream>

Server::Server(int ac, char *av[]) : _serverSocketfd(-1) {
    int status = this->parseArgs(ac, av);
    if (status == ARGS_NUM_INVALID)
        throw ServerException("Arguments invalid\nRun with: ./irc <port> <password>");
    else if (status == PORT_NUM_INVALID)
        throw ServerException("Port number is invalid");
    std::cout << "Server created" << std::endl;
}

//
//todo
Server::Server(const Server &orig) {
    (void)orig;
}

Server::~Server() {
    if (this->_serverSocketfd != -1)
        close(this->_serverSocketfd);
}
//

int Server::parseArgs(int ac, char *av[]) {
    if (ac != 3)
        return ARGS_NUM_INVALID;
    char *rest;
    int p = strtol(av[1], &rest, 10);
    const bool range_error = errno == ERANGE;
    if (range_error)
        return PORT_NUM_INVALID;
    if (av[1] == rest)
        return PORT_NUM_INVALID;
    if (*rest != '\0')
        return PORT_NUM_INVALID;
    if (p < 1024 || p > 65536)
        return PORT_NUM_INVALID;
    this->_port = p;
    this->_password = av[2];
    return OK;
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
        throw ServerException("socket() error");
    int on = 1;
    if (setsockopt(this->_serverSocketfd, SOL_SOCKET, SO_REUSEADDR, &on, sizeof(on)) == -1)
        throw ServerException("setsockopt() error");
    if (fcntl(this->_serverSocketfd, F_SETFL, O_NONBLOCK) == -1)
        throw ServerException("fcntl() error");
    if (bind(this->_serverSocketfd, (struct sockaddr*)&s, sizeof(s)) == -1)
        throw ServerException("bind() error");
    if (listen(this->_serverSocketfd, SOMAXCONN) == -1)
        throw ServerException("listen() error");
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
        std::cerr << "accept() error" << std::endl; //implement ClientException?
        return ;
    }
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

void Server::handleClientCommands(Client &client)
{
    std::vector<std::string> &mssgs = client.getReceivedMessages();

    while (!mssgs.empty())
    {
        std::stringstream ss;
        ss << mssgs.back();
        mssgs.pop_back();

        std::string line;
        std::getline(ss, line, ' ');
        if (line == "PASS")
        {
            // Client sent the password they used to connect to the server
            std::getline(ss, line, ' ');
            if (line != this->_password)
            {
                client.setPassword(line);
                // need some password logic
            }
        }
        else if (line == "NICK")
        {
            // Client sent their nickname
            std::getline(ss, line, ' ');
            client.setNickname(line);
        }
        else if (line == "CAP")
        {
            // Client sent a request to get the server's capabilities

            // CAP LS 302

            // some capabilities logic
            // need to answer to the server?

            // Answer like CAP * LS :<capabilities>
        }
        else if (line == "USER")
        {
            // Client sent their username, mode. `*' and real name

            // Username
            std::getline(ss, line, ' ');
            client.setUsername(line);

            // Mode
            std::getline(ss, line, ' ');

            // Asterix
            std::getline(ss, line, ' ');

            // Real name
            std::getline(ss, line, ' ');
            client.setRealname(line);
        }
    }
}

void Server::receiveClientData(int &i, Client &client)
{
    std::string &buffer = client.getRecvBuffer();
    char temp[512];

    // Getting the data from the socket
    ssize_t bytesread = recv(this->_pollfds[i].fd, temp, sizeof(temp), 0);
    if (bytesread > 0)
    {
        buffer.append(temp, bytesread);

        std::size_t endMsg;
        while ((endMsg = buffer.find("\r\n")) != std::string::npos)
        {
            std::string singleMsg = buffer.substr(0, endMsg);

            client.getReceivedMessages().push_back(singleMsg);

            std::cout << "msg: " << singleMsg << '\n';
            buffer.erase(0, endMsg + 2);
        }
        handleClientCommands(client);
    }
    else
    {
        if (bytesread == 0)
            std::cout << "Client disconnected" << std::endl;
        else
            std::cerr << "recv() error" << std::endl;
        removeClient(i);
    }
}

void Server::messageClient(int &i) {
    ssize_t bytessend;
    std::vector<std::string> msgtoSend = this->_clients[this->_pollfds[i].fd].getinMsg();
    if (msgtoSend[0].empty()) { //or could do bool msgready
        return ;
    } 
    //static int, probably cant empty
    for (int i = 0; i < msgtoSend.size(); i++) {
        while (bytessend) {
            bytessend = send(this->_pollfds[i].fd, &msgtoSend[i], sizeof(msgtoSend[i]), 0);
        }
        if (bytessend == -1)
            std::cerr << "send() error" << std::endl;
    }
    //have to empty all / could fix bool

}

void Server::finishServer(void) {
    //cleanup all fds
}

void Server::startServer(void) {
    setupServer();
    while (g_serverRunning) {
        int status = poll(&this->_pollfds[0], this->_pollfds.size(), -1);
        if (status == -1 && g_serverRunning == 0)
            throw ServerException("\nsignal catched");
        if (status == -1)
            throw ServerException("poll() error");
        for (int i = 0; i < static_cast<int>(this->_pollfds.size()); i++) {
            if (this->_pollfds[i].revents == 0)
                continue;
            else if (this->_pollfds[i].fd == this->_serverSocketfd 
                && this->_pollfds[i].revents & POLLIN) {
                    acceptClient();
            }
            else if (this->_pollfds[i].revents & POLLIN) {
                receiveClientData(i, this->_clients[this->_pollfds[i].fd]);
            }
            else if (this->_pollfds[i].revents & POLLOUT) {
                messageClient(i);
            }
            else if (this->_pollfds[i].revents & (POLLERR | POLLHUP | POLLNVAL)) {
                //issue with client, need to remove him, free fd, erase from all lists
            }
        }
    }
}

