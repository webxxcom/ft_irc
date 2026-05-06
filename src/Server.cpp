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
    long p = strtol(av[1], &rest, 10);
    const bool range_error = errno == ERANGE;
    if (range_error)
        return PORT_NUM_INVALID;
    if (av[1] == rest)
        return PORT_NUM_INVALID;
    if (*rest != '\0')
        return PORT_NUM_INVALID;
    if (p < 1024 || p > 65536)
        return PORT_NUM_INVALID;
    _state.setPort((int)p);
    _state.setPassword(av[2]);
    return OK;
}

Server::Server(int ac, char *av[])
    : _state()
    , _fileSendHandler(_state)
    , _replyHandler(_state)
    , _commandHandler(_state, _replyHandler, _fileSendHandler)
{
    int status = this->parseArgs(ac, av);
    if (status == ARGS_NUM_INVALID)
        throw ServerErrorException("Arguments invalid\nRun with: ./irc <port> <password>");
    else if (status == PORT_NUM_INVALID)
        throw ServerErrorException("Port number is invalid");
    std::cout << "Server created" << std::endl;
}   

void Server::setupServer(void) {
    struct sockaddr_in s;
    memset(&s, 0, sizeof(s));
    s.sin_family = AF_INET;
    s.sin_addr.s_addr = INADDR_ANY;
    s.sin_port = htons(_state.getPort());
    struct pollfd serverfd;
    _state.setServerSockerFd(socket(AF_INET, SOCK_STREAM, 0));
    if (_state.getServerSocketFd() == -1)
        throw ServerErrorException("socket() error");
    int on = 1;
    if (setsockopt(_state.getServerSocketFd(), SOL_SOCKET, SO_REUSEADDR, &on, sizeof(on)) == -1)
        throw ServerErrorException("setsockopt() error");
    if (fcntl(_state.getServerSocketFd(), F_SETFL, O_NONBLOCK) == -1)
        throw ServerErrorException("fcntl() error");
    if (bind(_state.getServerSocketFd(), (struct sockaddr*)&s, sizeof(s)) == -1)
        throw ServerErrorException("bind() error");
    if (listen(_state.getServerSocketFd(), SOMAXCONN) == -1)
        throw ServerErrorException("listen() error");
    serverfd.fd = _state.getServerSocketFd();
    serverfd.events = POLLIN;
    serverfd.revents = 0;
    _state.pollfdAdd(serverfd);
}

void Server::acceptClient(void) {
    struct sockaddr_in c;
    memset(&c, 0, sizeof(c));
    socklen_t lenc = sizeof(c);
    int clientSocketfd = accept(_state.getServerSocketFd(), (struct sockaddr*)&c, &lenc);
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
    clientfd.events = POLLIN | POLLOUT;
    clientfd.revents = 0;
    _state.pollfdAdd(clientfd);

    _state.addClient(new Client(clientfd.fd));
}

void Server::disconnectClient(Client *client)
{
    // empty the buffer
    _state.removeClient(client);
}

void Server::handleTransferFd(int fd, int ev)
{
    TransferSession *ts = _state.transferSessionFindByFd(fd);
    if (!ts)
        return ;

    if (ev & (POLLERR | POLLHUP | POLLNVAL))
    {
        ts->state = TransferSession::FAILED;
        _state.removeTransferSession(ts);
    }
    else if ((ev & POLLIN) && fd == ts->listenerFd)
    {
        sockaddr_in peer;
        socklen_t len = sizeof(peer);

        int newfd = ::accept(ts->listenerFd, (sockaddr*)&peer, &len);
        if (newfd < 0)
        {
            ts->state = TransferSession::FAILED;
            _state.removeTransferSession(ts);
            return ;
        }

        ::close(ts->listenerFd);
        _state.pollfdRemove(ts->listenerFd);

        ts->listenerFd = -1;
        ts->socketFd   = newfd;
        ts->ifs.open(ts->file.c_str(), std::ios_base::binary);
        ts->state      = TransferSession::TRANSFERRING;

        _state.pollfdAdd((pollfd){.fd=newfd, .events=POLLOUT, .revents=0});
    }
    else if ((ev & POLLOUT) && fd == ts->socketFd)
    {
        _fileSendHandler.sendChunk(ts);

        if (ts->state == TransferSession::DONE || ts->state == TransferSession::FAILED)
        {
            ::close(ts->socketFd);
            _state.pollfdRemove(ts->socketFd);
            _state.removeTransferSession(ts);
        }
    }
}

void Server::receiveClientData(Client *client)
{
    char    temp[512];
    ssize_t bytesread;

    bytesread = recv(client->getFd(), temp, sizeof(temp), 0);
    if (bytesread > 0)
    {
        temp[bytesread] = '\0';
        client->putIntoRecvBuffer(temp);
        
        _commandHandler.handle(client);

        if (client->isRegistered() && !client->wasWelcomed())
            _replyHandler.welcome(client);
    }
    else
    {
        if (bytesread == 0)
            std::cout << "Client disconnected" << std::endl;
        else
            std::cerr << "recv() error" << std::endl;
        _state.clientDisconnects(client);
    }
}

void Server::messageClient(Client *client) {
    struct pollfd clientPollfd;
    if (!client || !_state.pollfdFindByFd(client->getFd(), clientPollfd))
        return ;

    std::queue<std::string> mssgsToSend = client->getInMssgs();
    if (mssgsToSend.empty())
    {
        clientPollfd.events = POLLIN;
        if (client->isPendingDisconnect())
            disconnectClient(client);
        return;
    }

    std::string longMsg = "";
    while(!mssgsToSend.empty())
    {
        longMsg += mssgsToSend.front();
        mssgsToSend.pop();
    }
    client->clearInMssgs();

    ssize_t bytessend = ::send(client->getFd(), longMsg.c_str(), longMsg.length(), MSG_NOSIGNAL);
    std::cout << "Client " << client->getNickname() + " receives: " << longMsg.substr(0, bytessend);
    if (bytessend > 0) {
        if (static_cast<size_t>(bytessend) < longMsg.length()) {
            client->addInMsg(longMsg.substr(bytessend));
            clientPollfd.events = POLLIN | POLLOUT;
        }
        else {
            clientPollfd.events = POLLIN;
            if (client->isPendingDisconnect())
                disconnectClient(client);
        }
    }
    else if (bytessend == -1) {
        if (errno == EAGAIN || errno == EWOULDBLOCK) {
            client->addInMsg(longMsg);
            clientPollfd.events = POLLIN | POLLOUT;
        }
        else {
            std::cerr << "send() error" << std::endl;
            disconnectClient(client);
        }
    }
}

void Server::startServer(void) {
    setupServer();

    while (g_serverRunning) {
        int status = _state.poll();

        if (status == -1 && g_serverRunning == 0)
            throw ServerErrorException("\nsignal catched");
        if (status == -1)
            throw ServerErrorException("poll() error");
        handlePolls(_state.getPollFds());
    }
}

void Server::handlePolls(std::vector<pollfd> const pollfds)
{
    for (std::size_t i = 0; i < pollfds.size(); ++i)
    {
        if (pollfds[i].revents == 0)
            continue;

        int fd = pollfds[i].fd;
        if (_state.isTransferFd(fd))
            handleTransferFd(fd, pollfds[i].revents);
        else 
        {
            if (pollfds[i].revents & (POLLERR | POLLHUP | POLLNVAL))
            {
                if (pollfds[i].fd == _state.getServerSocketFd())
                {
                    _state.clientDisconnects(_state.clientFindByFd(pollfds[i].fd));
                    continue ;
                }
                else
                    return ;
            }
            if (pollfds[i].revents & POLLIN)
            {
                if (pollfds[i].fd == _state.getServerSocketFd())
                    acceptClient();
                else
                    receiveClientData(_state.clientFindByFd(pollfds[i].fd));
            }
            if (pollfds[i].revents & POLLOUT)
                messageClient(_state.clientFindByFd(pollfds[i].fd));
        }
    }
}
