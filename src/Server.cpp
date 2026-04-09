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
    this->_port = p;
    this->_password = av[2];
    return OK;
}

Server::Server(int ac, char *av[]) : _serverSocketfd(-1), _commandHandler(*this, _replyHandler) {
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
    
    for(size_t i = 0; i < _clients.size(); ++i)
        delete _clients[i];
    for(size_t i = 0; i < _channels.size(); ++i)
        delete _channels[i];
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
    _clients.push_back(cl);
    _clientsByFd.insert(std::pair<int, Client *>(cl->getFd(), cl));
    _clientsByName.insert(std::pair<std::string, Client *>(cl->getNickname(), cl));
}

void Server::disconnectClient(Client &client) {
    shutdown(client.getFd(), SHUT_WR); 
    close(client.getFd());
    std::remove_if(_pollfds.begin(), _pollfds.end(), CompareByFd(client.getFd()));
    _clientsByFd.erase(client.getFd());
    _clientsByName.erase(client.getNickname());
    _clients.erase(std::find(_clients.begin(), _clients.end(), &client));
    // delete client; // !!!
}

bool Server::receiveClientData(Client *client)
{
    std::string &buffer = client->getRecvBuffer();
    char temp[512];

    ssize_t bytesread = recv(client->getFd(), temp, sizeof(temp), 0);
    if (bytesread > 0)
    {
        buffer.append(temp, bytesread);

        std::size_t endMsg;
        while ((endMsg = buffer.find("\r\n")) != std::string::npos)
        {
            std::string singleMsg = buffer.substr(0, endMsg);

            client->getReceivedMessages().push(singleMsg);

            // std::cout << "msg: " << singleMsg << '\n';
            buffer.erase(0, endMsg + 2);
        }
        _commandHandler.handle(client);

        if (client->isRegistered()) {
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
        disconnectClient(*client);
        return true;
    }
    return false;
}

bool Server::messageClient(Client &client) {
    std::vector<struct pollfd>::iterator it = _pollfds.begin();

    std::queue<std::string> msgtoSend = client.getInMsg();
    if (msgtoSend.empty()) {
        it->events = POLLIN;
        return false;
    }
    std::string longMsg = "";
    while(!msgtoSend.empty()) {
        longMsg += msgtoSend.front();
        msgtoSend.pop();
    }
    client.clearinMsg();
    ssize_t bytessend = send(client.getFd(), longMsg.c_str(), longMsg.length(), MSG_NOSIGNAL);
    if (bytessend > 0) {
        if (static_cast<size_t>(bytessend) < longMsg.length()) {
            std::string remainder = longMsg.substr(bytessend);
            client.addinMsg(remainder);
            it->events = POLLIN | POLLOUT;
        }
    }
    else if (bytessend == -1) {
        if (errno == EAGAIN || errno == EWOULDBLOCK) {
            client.addinMsg(longMsg);
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

void Server::finishServer(void) {

    // or maybe close in clients destructor, would probably be better
    for (size_t k = 0; k < this->_pollfds.size(); ++k) {
        if (this->_pollfds[k].fd >= 0)
            close(this->_pollfds[k].fd);
    }
    this->_pollfds.clear();
    this->_clients.clear();
    this->_channels.clear();
    //if roman uses pointers, first need to free memory (close, delete, clear)
}

AdvancedMap<std::string, Channel *> const &Server::getChannelsByName() const
{
    return _channelsByName;
}

std::vector<Channel *> const &Server::getChannels() const
{
    return _channels;
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

    //discuss with roman: since moved outside start server, diffrent ctrl+c behaviour
    //is it ok according to subject?

    while (i < this->_pollfds.size())
    {
        try
        {
            if (this->_pollfds[i].revents != 0)
            {
                if (this->_pollfds[i].revents & (POLLERR | POLLHUP | POLLNVAL)) {
                    if (this->_pollfds[i].fd == this->_serverSocketfd) {
                        finishServer();
                        return ;
                    }
                    else
                        disconnectClient(*this->_clientsByFd[this->_pollfds[i].fd]);
                        iDisconnected = true;
                }
                if (!iDisconnected && this->_pollfds[i].revents & POLLIN) {
                    if (this->_pollfds[i].fd == this->_serverSocketfd)
                        acceptClient();
                    else
                        iDisconnected = receiveClientData(this->_clientsByFd[this->_pollfds[i].fd]);
                }
                if (!iDisconnected && this->_pollfds[i].revents & POLLOUT)
                    iDisconnected = messageClient(*this->_clientsByFd.at(this->_pollfds[i].fd));
            }
            // if (!iDisconnected)
            //     ++i;
        }
        // ! talk to roman: dont think can use while loop here to not block server
        //just send and disconnect?
        //but also not after poll() so subject incorrect >>buffer instead
        //also discuss purpose, would change ++i placement
        catch(const ClientException& e)
        {
            //roman thingie
            // ssize_t total = 0, len = strlen(e.what());



            // while (total < len)
            // {
            //     std::cout << "Sending " << e.what() + total << std::endl;
            //     // TEST: `messageClient' didn't work
            //     ssize_t n = ::send(this->_pollfds[i].fd, e.what() + total, strlen(e.what()) - total, 0);
            //     if (n <= 0)
            //         break ;
            //     total += n;
            // }
            // //A>by the previously commented logic, isnt it risky to use this->_pollfds[i].fd? 
            // //why change it everywhere else and leave here??

            // disconnectClient(this->_clients[this->_pollfds[i].fd]);
            // // iDisconnected = true;
            // // if specific purpose (roman) add iDisconnected bool

            std::string errMsg = std::string("Error :") + e.what() + "\r\n";
            std::cout<< "disconnecting fd.." << std::endl;
            send(this->_pollfds[i].fd, errMsg.c_str(), errMsg.length(), MSG_NOSIGNAL);
            disconnectClient(*this->_clients[this->_pollfds[i].fd]);
            iDisconnected = true;

            //or could do bool pending disconnect
            //this->_clients[this->_pollfds[i].fd].addinMsg(errMsg);
        }
        if (!iDisconnected)
            ++i;
    }
}

Channel *Server::createChannel(Client *creator, std::string const &name)
{
    Channel *ch = new Channel(creator, name);

    _channels.push_back(ch);
    _channelsByName.insert(std::pair<std::string, Channel *>(name, ch));
    return ch;
}
