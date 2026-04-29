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
    : _state()
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

void Server::setupServer(void) {
    struct sockaddr_in s;
    memset(&s, 0, sizeof(s));
    s.sin_family = AF_INET;
    s.sin_addr.s_addr = INADDR_ANY;
    s.sin_port = htons(_state.getPort());
    struct pollfd serverfd;
    _state.setServerSockerFd(socket(AF_INET, SOCK_STREAM, 0));
    if (_state.getServerSockerFd() == -1)
        throw ServerErrorException("socket() error");
    int on = 1;
    if (setsockopt(_state.getServerSockerFd(), SOL_SOCKET, SO_REUSEADDR, &on, sizeof(on)) == -1)
        throw ServerErrorException("setsockopt() error");
    if (fcntl(_state.getServerSockerFd(), F_SETFL, O_NONBLOCK) == -1)
        throw ServerErrorException("fcntl() error");
    if (bind(_state.getServerSockerFd(), (struct sockaddr*)&s, sizeof(s)) == -1)
        throw ServerErrorException("bind() error");
    if (listen(_state.getServerSockerFd(), SOMAXCONN) == -1)
        throw ServerErrorException("listen() error");
    serverfd.fd = _state.getServerSockerFd();
    serverfd.events = POLLIN;
    serverfd.revents = 0;
    _state.pollfdAdd(serverfd);
}

void Server::acceptClient(void) {
    struct sockaddr_in c;
    memset(&c, 0, sizeof(c));
    socklen_t lenc = sizeof(c);
    int clientSocketfd = accept(_state.getServerSockerFd(), (struct sockaddr*)&c, &lenc);
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
    _state.pollfdAdd(clientfd);

    _state.addClient(new Client(clientfd.fd));
}

void Server::disconnectClient(Client *client)
{
    _state.removeClient(client);
}

bool Server::handleTransferFd(int fd, int ev)
{
    TransferSession *ts = _state.transferSessionFindByFd(fd);
    if (!ts)
        return false;

    if (ev & (POLLERR | POLLHUP | POLLNVAL))
    {
        ts->state = TransferSession::FAILED;
        _state.removeTransferSession(ts);
        return true;
    }

    if ((ev & POLLIN) && fd == ts->listenerFd)
    {
        sockaddr_in peer;
        socklen_t len = sizeof(peer);

        int newfd = ::accept(ts->listenerFd, (sockaddr*)&peer, &len);
        if (newfd < 0)
        {
            ts->state = TransferSession::FAILED;
            _state.removeTransferSession(ts);
            return true;
        }

        ::close(ts->listenerFd);
        _state.pollfdRemove(ts->listenerFd);

        ts->listenerFd = -1;
        ts->socketFd   = newfd;
        ts->ifs.open(ts->file.c_str(), std::ios_base::binary);
        ts->state      = TransferSession::TRANSFERRING;

        _state.pollfdAdd((pollfd){.fd=newfd, .events=POLLOUT, .revents=0});
        return false;
    }

    // ready to send next file chunk
    if ((ev & POLLOUT) && fd == ts->socketFd)
    {
        _fileSendHandler.sendChunk(ts);

        if (ts->state == TransferSession::DONE)
        {
            ::close(ts->socketFd);
            _state.pollfdRemove(ts->socketFd);
            _state.removeTransferSession(ts);
            return true;
        }

        if (ts->state == TransferSession::FAILED)
        {
            ::close(ts->socketFd);
            _state.pollfdRemove(ts->socketFd);
            _state.removeTransferSession(ts);
            return true;
        }
    }

    return false;
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
            _state.pollfdFindByFd(client->getFd()).events = POLLIN | POLLOUT;
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
    struct pollfd &clientPollfd = _state.pollfdFindByFd(client->getFd());

    std::queue<std::string> mssgsToSend = client->getInMssgs();
    if (mssgsToSend.empty()) {
        clientPollfd.events = POLLIN;
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
            clientPollfd.events = POLLIN | POLLOUT;
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

    std::vector<struct pollfd>& pollfds = _state.getPollFds();
    while (g_serverRunning) {
        int status = poll(&pollfds[0], pollfds.size(), -1);
        if (status == -1 && g_serverRunning == 0)
            throw ServerErrorException("\nsignal catched");
        if (status == -1)
            throw ServerErrorException("poll() error");
        handlePolls(pollfds);
    }
}

void Server::handlePolls(std::vector<struct pollfd> &pollfds)
{
    std::size_t i = 0;
    bool iDisconnected = false;

    while (i < pollfds.size())
    {
        try
        {
            if (pollfds[i].revents == 0)
            {
                ++i;
                continue;
            }

            // My file transfer addition
            int fd = pollfds[i].fd;
            if (_state.isTransferFd(fd))
                iDisconnected = handleTransferFd(fd, pollfds[i].revents);
            else 
            {
                if (pollfds[i].revents & (POLLERR | POLLHUP | POLLNVAL))
                {
                    if (pollfds[i].fd == _state.getServerSockerFd())
                    {
                        finishServer(); // ! Shouldn't throw to exit?
                        return ;
                    }
                    else
                    {
                        disconnectClient(_state.clientFindByFd(pollfds[i].fd));
                        iDisconnected = true;
                    }
                }
                if (!iDisconnected && pollfds[i].revents & POLLIN)
                {
                    if (pollfds[i].fd == _state.getServerSockerFd())
                        acceptClient();
                    else
                        iDisconnected = receiveClientData(_state.clientFindByFd(pollfds[i].fd));
                }
                if (!iDisconnected && pollfds[i].revents & POLLOUT)
                    iDisconnected = messageClient(_state.clientFindByFd(pollfds[i].fd));
            }
        }
        catch(const ClientException& e) // ! leave it for QUIT?
        {
            std::string errMsg = std::string("Error :") + e.what() + "\r\n";
            std::cout<< "disconnecting fd.." << std::endl;
            send(pollfds[i].fd, errMsg.c_str(), errMsg.length(), MSG_NOSIGNAL);
            disconnectClient(_state.clientFindByFd(pollfds[i].fd));
            iDisconnected = true;
        }
        if (!iDisconnected)
            ++i;
    }
}
