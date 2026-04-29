#include "Server.hpp"

#include <iostream>
#include <algorithm>
#include <cstring>
#include <fcntl.h>
#include <cerrno>
#include <sys/socket.h>
#include <netinet/in.h>

int Server::parseArgs(int ac, char *av[])
{
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
    _state.setPort(p);
    _state.setPassword(av[2]);
    return OK;
}

Server::Server(int ac, char *av[])
    : _serverSocketfd(-1)
    , _state()
    , _fileSendHandler(_state)
    , _replyHandler(*this)
    , _commandHandler(_state, _replyHandler, _fileSendHandler)
{
    int status = this->parseArgs(ac, av);
    if (status == ARGS_NUM_INVALID)
        throw ServerErrorException("Arguments invalid\nRun with: ./irc <port> <password>");
    else if (status == PORT_NUM_INVALID)
        throw ServerErrorException("Port number is invalid");
    std::cout << "Server created" << std::endl;
}   

Server::~Server() {
    if (this->_serverSocketfd != -1)
        close(this->_serverSocketfd);
}

void Server::setupServer(void) {
    struct sockaddr_in s;
    memset(&s, 0, sizeof(s));
    s.sin_family = AF_INET;
    s.sin_addr.s_addr = INADDR_ANY;
    s.sin_port = htons(_state.getPort());
    struct pollfd serverfd;
    this->_serverSocketfd = socket(AF_INET, SOCK_STREAM, 0);
    if (this->_serverSocketfd == -1)
        throw ServerErrorException("socket() error");
    int on = 1;
    if (setsockopt(this->_serverSocketfd, SOL_SOCKET, SO_REUSEADDR, &on, sizeof(on)) == -1)
        throw ServerErrorException("setsockopt() error");
    if (fcntl(this->_serverSocketfd, F_SETFL, O_NONBLOCK) == -1)
        throw ServerErrorException("fcntl() error");
    if (bind(this->_serverSocketfd, (struct sockaddr*)&s, sizeof(s)) == -1)
        throw ServerErrorException("bind() error");
    if (listen(this->_serverSocketfd, SOMAXCONN) == -1)
        throw ServerErrorException("listen() error");
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

    Client *cl = new Client(clientfd.fd);
    _state.addClient(cl);
}

void Server::disconnectClient(Client *client) {
    std::remove_if(_pollfds.begin(), _pollfds.end(), CompareByFd(client->getFd()));
    _state.removeClient(client);
}

bool Server::receiveClientData(Client *client)
{
    char    temp[512];
    ssize_t bytesread;

    bytesread = recv(client->getFd(), temp, sizeof(temp), 0);
    if (bytesread > 0)
    {
        temp[bytesread] = '\0';
        client->putIntoRecvBuffer(temp);
        
        _commandHandler.handle(client);

        if (client->isRegistered()) {
            if (!client->wasWelcomed())
                _replyHandler.welcome(client);
            std::find_if(_pollfds.begin(), _pollfds.end(), CompareByFd(client->getFd()))->events = POLLIN | POLLOUT;
        }
    }
    else
    {
        if (bytesread == 0)
            std::cout << "Client disconnected" << std::endl;
        else
            std::cerr << "recv() error" << std::endl;
        disconnectClient(client);
        return true;
    }
    return false;
}

bool Server::messageClient(Client *client) {
    std::vector<struct pollfd>::iterator it = std::find_if(_pollfds.begin(), _pollfds.end(), CompareByFd(client->getFd()));

    std::queue<std::string> mssgsToSend = client->getInMssgs();
    if (mssgsToSend.empty()) {
        it->events = POLLIN;
        return false;
    }
    std::string longMsg = "";
    while(!mssgsToSend.empty()) {
        longMsg += mssgsToSend.front();
        std::cout << "Client receives: " << mssgsToSend.front();
        mssgsToSend.pop();
    }
    client->clearInMssgs();
    ssize_t bytessend = send(client->getFd(), longMsg.c_str(), longMsg.length(), MSG_NOSIGNAL);
    if (bytessend > 0) {
        if (static_cast<size_t>(bytessend) < longMsg.length()) {
            std::string remainder = longMsg.substr(bytessend);
            client->addInMsg(remainder);
            it->events = POLLIN | POLLOUT;
        }
    }
    else if (bytessend == -1) {
        if (errno == EAGAIN || errno == EWOULDBLOCK) {
            client->addInMsg(longMsg);
            it->events = POLLIN | POLLOUT;
        }
        else {
            std::cerr << "send() error" << std::endl;
            disconnectClient(client);
            return true; 
        }
    }
    return false;
}

void Server::finishServer(void)
{

}

void Server::startServer(void) {
    setupServer();
    while (g_serverRunning) {
        int status = poll(&this->_pollfds[0], this->_pollfds.size(), -1);
        if (status == -1 && g_serverRunning == 0)
            throw ServerErrorException("\nsignal catched");
        if (status == -1)
            throw ServerErrorException("poll() error");
        handlePolls();
    }
}

void Server::handlePolls()
{
    std::size_t i = 0;
    bool iDisconnected = false;

    while (i < this->_pollfds.size())
    {
        try
        {
            if (this->_pollfds[i].revents != 0)
            {
                if (this->_pollfds[i].revents & (POLLERR | POLLHUP | POLLNVAL))
                {
                    if (this->_pollfds[i].fd == this->_serverSocketfd)
                    {
                        finishServer(); // ! Shouldn't throw to exit?
                        return ;
                    }
                    else
                    {
                        disconnectClient(_state.clientFindByFd(_pollfds[i].fd));
                        iDisconnected = true;
                    }
                }
                if (!iDisconnected && this->_pollfds[i].revents & POLLIN)
                {
                    if (this->_pollfds[i].fd == this->_serverSocketfd)
                        acceptClient();
                    else
                        iDisconnected = receiveClientData(_state.clientFindByFd(this->_pollfds[i].fd));
                }
                if (!iDisconnected && this->_pollfds[i].revents & POLLOUT)
                    iDisconnected = messageClient(_state.clientFindByFd(this->_pollfds[i].fd));
            }
        }
        catch(const ClientException& e) // ! leave it for QUIT?
        {
            std::string errMsg = std::string("Error :") + e.what() + "\r\n";
            std::cout<< "disconnecting fd.." << std::endl;
            send(this->_pollfds[i].fd, errMsg.c_str(), errMsg.length(), MSG_NOSIGNAL);
            disconnectClient(_state.clientFindByFd(this->_pollfds[i].fd));
            iDisconnected = true;
        }
        if (!iDisconnected)
            ++i;
    }
}

void Server::handlePendingTransfers()
{
    for(std::map<std::string, TransferSession *>::iterator it = _state.getPendingTransfers().begin(); it != _state.getPendingTransfers().end(); ++it)
        if (it->second->state == TransferSession::TRANSFERRING)    
            _fileSendHandler.sendInChunks(it->second);
}