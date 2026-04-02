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

Server::Server(int ac, char *av[]) : _serverSocketfd(-1), _commandHandler(*this) {
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

    Client *cl = new Client(clientfd.fd);
    _clients.push_back(cl);
    _clientsByFd.insert(std::pair<int, Client *>(cl->getFd(), cl));
    _clientsByName.insert(std::pair<std::string, Client *>(cl->getNickname(), cl));
}

// before disconnecting the client we want to send all of the messages to them
void Server::disconnectClient(Client *client)
{
    shutdown(client->getFd(), SHUT_WR);
    close(client->getFd());
    std::remove_if(_pollfds.begin(), _pollfds.end(), CompareByFd(client->getFd()));

    _clientsByFd.erase(client->getFd());
    _clientsByName.erase(client->getNickname());
    _clients.erase(std::find(_clients.begin(), _clients.end(), client));
    delete client;
}

// Errors are divided into two types: the ones which disconnect the client 
//  and the ones which just send them the error occured to the client.
void Server::notifyClient(Client *target, ServerNotifyCodes error_code, std::string const& extra)
{
    std::stringstream msg;
    msg << ":server " << (int)error_code << " " + target->getIrcNickname() + " ";
    switch (error_code)
    {
        case ERR_PASSWDMISMATCH:
            msg << ":Password incorrect\r\n";
            throw ClientException(msg.str());
        case ERR_ALREADYREGISTERED:
            msg << ":already registered\r\n";
            throw ClientException(msg.str());
        case ERR_NOTREGISTERED:
            msg << ":complete registration first\r\n";
            throw ClientException(msg.str());
        case ERR_UNKNOWN_COMMAND:
            msg << extra << " :Unknown command";
            break ;
        case ERR_NOSUCHNICK:
            msg << extra << " :No such nick/channel";
            break;
        case ERR_NOSUCHCHANNEL:
            msg << extra << " :No such channel";
            break;
        case ERR_NOTONCHANNEL:
            msg << extra << " :You're not on that channel";
            break;
        case ERR_NOPRIVILEGES:
            msg << extra << " :Permission Denied- You're not an IRC operator";
            break;
        case ERR_USERNOTINCHANNEL:
            msg << extra << " :They aren't on that channel";
            break;
        case ERR_CHANOPRIVSNEEDED:
            msg << extra << " :You're not channel operator";
            break;
        case RPL_INVITING:
            msg << extra;
            break;
        default:
            msg << ":Unknown error";
            break;
    }
    msg << "\r\n";
    target->receiveMsg(msg.str());
}

void Server::receiveClientData(Client *client)
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

            client->getReceivedMessages().push_back(singleMsg);

            std::cout << "msg: " << singleMsg << '\n';
            buffer.erase(0, endMsg + 2);
        }
        _commandHandler.handle(client);

        // Client expects message `001' about successfull connection
        if (client->isRegistered())
            client->receiveMsg(":server 001 " + client->getNickname() + " :Welcome to the IRC server");
    }
    else
    {
        if (bytesread == 0)
            std::cout << "Client disconnected" << std::endl;
        else
            std::cerr << "recv() error" << std::endl;
        disconnectClient(client); // ! Should we throw an exception?
    }
}

void Server::messageClient(Client &client) {
    ssize_t bytessend;
    std::vector<std::string> msgtoSend = client.getinMsg();
    if (msgtoSend[0].empty()) { //or could do bool msgready
        return ;
    } 
    //static int, probably cant empty
    for (size_t i = 0; i < msgtoSend.size(); i++) {
        do { // replaced with do while because `bytessend' can't be initialized
            bytessend = send(this->_pollfds[i].fd, &msgtoSend[i], sizeof(msgtoSend[i]), 0); // ! std::string::size()?
        } while (bytessend);
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
            throw ServerErrorException("\nsignal catched");
        if (status == -1)
            throw ServerErrorException("poll() error");

        // Separate responsibilites to have less function lines of code
        handlePolls();
    }
}

void Server::handlePolls()
{
    std::size_t i = 0;

    // The `i' index is now not increased constantly as it was before with the for loop
    //  now the `receiveClientData' member function can throw an exception ClientException meaning
    //  that there was an error with some client data and if
    //  that exception is thrown the user must be diconnected (see the catch block)

    // Now the `messageClient' doesn't take `i' as a parameter but rather a client
    //  and if client's fd is needed then we have Client::_fd field. Passing the index down the function calls
    //  can lead to really bad mistakes when it's an index to iterate over an array. Better to have it ONLY inside
    //  the loop block with no non-const exposing outside (as it was before).
    while (i < this->_pollfds.size())
    {
        // If the exception is caught the `i' index is not increased so no need to pass it
        //  to all the function calls and decrement after disconnecting the client
        try
        {
            if (this->_pollfds[i].revents != 0)
            {
                if (this->_pollfds[i].fd == this->_serverSocketfd 
                        && this->_pollfds[i].revents & POLLIN) {
                    acceptClient();
                }
                else if (this->_pollfds[i].revents & POLLIN) {
                    receiveClientData(_clientsByFd.at(_pollfds[i].fd));
                }
                else if (this->_pollfds[i].revents & POLLOUT) {
                    messageClient(*_clientsByFd.at(_pollfds[i].fd));
                }
                else if (this->_pollfds[i].revents & (POLLERR | POLLHUP | POLLNVAL)) {
                    //issue with client, need to remove him, free fd, erase from all lists
                }
            }
            ++i;
        }
        catch(const ClientException& e)
        {
            //to avoid `irc: sending data to server: error 32 Broken pipe'
            // in the client we may receive all the messages from recv? 
            disconnectClient(_clientsByFd[_pollfds[i].fd]);
        }
    }
}

Channel *Server::createChannel(Client *creator, std::string const &name)
{
    Channel *ch = new Channel(creator, name);

    _channels.push_back(ch);
    _channelsByName.insert(std::pair<std::string, Channel *>(name, ch));
    return ch;
}
