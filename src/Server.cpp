#include "Server.hpp"
#include "irc.hpp"
#include <cstring>
#include <sstream>

void Server::setupCommands()
{
    _command_map["PASS"] = &Server::handlePass;
    _command_map["NICK"] = &Server::handleNick;
    _command_map["USER"] = &Server::handleUser;
    _command_map["CAP"] = &Server::handleCap;
    _command_map["JOIN"] = &Server::handleJoin;
    _command_map["INVITE"] = &Server::handleInvite;
    _command_map["TOPIC"] = &Server::handleTopic;
    _command_map["MODE"] = &Server::handleMode;
}

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

Server::Server(int ac, char *av[]) : _serverSocketfd(-1) {
    int status = this->parseArgs(ac, av);
    if (status == ARGS_NUM_INVALID)
        throw ServerErrorException("Arguments invalid\nRun with: ./irc <port> <password>");
    else if (status == PORT_NUM_INVALID)
        throw ServerErrorException("Port number is invalid");
    std::cout << "Server created" << std::endl;
    setupCommands();
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
    _fd_index_map[clientfd.fd] = _pollfds.size() - 1;
    _clients.insert(std::pair<int, Client>(clientfd.fd, Client(clientfd.fd)));
}

void Server::updateIndex(Client &client) {
    size_t const i =_fd_index_map[client.getFd()];

    for (size_t k = i + 1; k < this->_pollfds.size(); ++k) {
        _fd_index_map[this->_pollfds[k].fd] = k - 1;
    }
    _fd_index_map.erase(client.getFd());
}

void Server::disconnectClient(Client &client) {
    size_t const i = _fd_index_map[client.getFd()];

    shutdown(client.getFd(), SHUT_WR); 
    close(client.getFd());
    updateIndex(client);
    this->_clients.erase(client.getFd());
    this->_pollfds.erase(this->_pollfds.begin() + i);
    // + remove from channels
}

void Server::handleClientCommands(Client &client)
{
    std::queue<std::string> &mssgs = client.getReceivedMessages();

    while (!mssgs.empty())
    {
        // Get the whole line
        std::stringstream line;
        line << mssgs.front();
        mssgs.pop();

        // Extract single command
        std::string command;
        std::getline(line, command, ' ');

        // Find the command in the map
        std::map<std::string, CommandHandler>::iterator it = _command_map.find(command);
        if (it != _command_map.end())
            (this->*(it->second))(client, line);
        else {
            client.receiveMsg(":server 421 " + client.getIrcNickname() + " " + command + ":unknown command\r\n");
            size_t i = _fd_index_map[client.getFd()];
            this->_pollfds[i].events = POLLIN | POLLOUT;
        }
            
    }
}

// Pass the error handling to a separate function with error_codes to send to the client
// Exception we throw so far has the message to send to the client but the design is under the question
//  because exception's messages for us to get not for us to send to some application.
//  Bad practice here but i guess we've got no other choice? debatable
void Server::terminateClient(ServerErrorCodes error_code, Client &client)
{
    std::string msg;
    switch (error_code)
    {
        case ERR_PASSWDMISMATCH:
            msg = ":server 464 " + client.getIrcNickname() + " :Password incorrect";
            break ;
        case ERR_ALREADYREGISTERED:
            msg = ":server 462 " + client.getIrcNickname() + " :already registered";
            break ;
        case ERR_NOTREGISTERED:
            msg = ":server 451 " + client.getIrcNickname() + " :complete registration first";
            break ;
    }
    msg += "\r\n";
    throw ClientException(msg); //talk to roman> not okay, breaks client completely
}

// !! talk wit roman about the references, if better change??
// after renaming receivedmsg to inmsg >> better .getreceivedmsg to .getinmsg, makes more sense
// maybe confused< why roman>static, then uses buffer

bool Server::receiveClientData(Client &client)
{
    std::string &buffer = client.getRecvBuffer();
    char temp[512];

    ssize_t bytesread = recv(client.getFd(), temp, sizeof(temp), 0);
    if (bytesread > 0)
    {
        buffer.append(temp, bytesread);

        std::size_t endMsg;
        while ((endMsg = buffer.find("\r\n")) != std::string::npos)
        {
            std::string singleMsg = buffer.substr(0, endMsg);

            client.getReceivedMessages().push(singleMsg);

            // std::cout << "msg: " << singleMsg << '\n';
            buffer.erase(0, endMsg + 2);
        }
        handleClientCommands(client);

        // Client expects message `001' about successfull connection
        if (client.isRegistered()) {
            client.receiveMsg(":server 001 " + client.getNickname() + " :Welcome to the IRC server");
            size_t i = _fd_index_map[client.getFd()];
            this->_pollfds[i].events = POLLIN | POLLOUT;
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

bool Server::messageClient(Client &client) {
    size_t i = _fd_index_map[client.getFd()];
    std::queue<std::string> msgtoSend = client.getinMsg();
    if (msgtoSend.empty()) {
        this->_pollfds[i].events = POLLIN;
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
            this->_pollfds[i].events = POLLIN | POLLOUT;
        }
    }
    else if (bytessend == -1) {
        if (errno == EAGAIN || errno == EWOULDBLOCK) {
            client.addinMsg(longMsg);
            this->_pollfds[i].events = POLLIN | POLLOUT;
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
    this->_fd_index_map.clear();
    //if roman uses pointers, first need to free memory (close, delete, clear)
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
                        disconnectClient(this->_clients[this->_pollfds[i].fd]);
                        iDisconnected = true;
                }
                if (!iDisconnected && this->_pollfds[i].revents & POLLIN) {
                    if (this->_pollfds[i].fd == this->_serverSocketfd)
                        acceptClient();
                    else
                        iDisconnected = receiveClientData(this->_clients[this->_pollfds[i].fd]);
                }
                if (!iDisconnected && this->_pollfds[i].revents & POLLOUT)
                    iDisconnected = messageClient(this->_clients.at(this->_pollfds[i].fd));
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
            disconnectClient(this->_clients[this->_pollfds[i].fd]);
            iDisconnected = true;

            //or could do bool pending disconnect
            //this->_clients[this->_pollfds[i].fd].addinMsg(errMsg);
        }
        if (!iDisconnected)
            ++i;
    }
}

//////////////////////// COMMANDS /////////////////////////////

void Server::handlePass(Client& client, std::stringstream& command)
{
    std::string word;

    // Client sent the password they used to connect to the server
    std::getline(command, word, ' ');
    if (word != _password)
        // The passwords do not match -- send the message it's incorrect
        terminateClient(ERR_PASSWDMISMATCH, client);
    else
        client.setPassword(word); // set password if correct
}

void Server::handleUser(Client& client, std::stringstream& command)
{
    std::string word;
    
    // Username
    std::getline(command, word, ' ');
    client.setUsername(word);

    // Mode, Asterix
    std::getline(command, word, ' ');
    std::getline(command, word, ' ');

    // Real name
    std::getline(command, word, ' ');
    client.setRealname(word);
}

void Server::handleNick(Client& client, std::stringstream& command)
{
    std::string word;
   
    std::getline(command, word, ' ');
    std::cout << "NICKNAME: " << word << std::endl;
    client.setNickname(word);
}

void Server::handleCap(Client& client, std::stringstream& command)
{
    size_t i = _fd_index_map[client.getFd()];
    std::string word;
    
    std::getline(command, word, ' ');
    if (word == "LS") //
    {
        client.receiveMsg(":server CAP * LS :\r\n"); // No capabilities
        this->_pollfds[i].events = POLLIN | POLLOUT;
    }
    else // Do not support any other than LS {
    {
        client.receiveMsg(":server 421 " + client.getIrcNickname() + " " + word + ":unknown command for CAP\r\n");
        this->_pollfds[i].events = POLLIN | POLLOUT;
    }
}

void Server::handleJoin(Client &client, std::stringstream &command)
{
    std::string word;
    std::getline(command, word, ' ');

    // ! The keys to channels are not yet implemented
    std::stringstream channelList;
    channelList << word;
    while (1)
    {
        std::getline(channelList, word, ',');
        if (channelList.eof())
            break ;
        if (word[0] != '#')
            continue ;

        std::map<std::string, Channel>::iterator it = _channels.find(word); 
        if (it == _channels.end()) // channel with that name was not found - create it
        {
            Channel ch(word);
            ch.addOperator(client); // The creator is automatically an operator
            _channels.insert(std::pair<std::string, Channel>(word, ch));
        }
        _channels.at(word).addMember(client);
    }
}

void Server::handleKick(Client &client, std::stringstream &command)
{
    std::string channel, who, message;
    std::getline(command, channel, ' ');
    std::getline(command, who, ' ');
    std::getline(command, message, ' ');

    if (channel[0] != '#')
        return ;
    std::map<std::string, Channel>::iterator it = _channels.find(channel); 
    if (it != _channels.end())
    {
        Channel &ch = it->second;

        // are YOU a channel's operator first of all??
        if (!client.isChannelOperator(ch))
            return ;

        std::map<std::string, Client *>::const_iterator it = ch.getMembers().find(who);
        if (it != ch.getMembers().end()) {
            it->second->receiveMsg("ERROR :" + message);
            size_t i = _fd_index_map[it->second->getFd()];
            this->_pollfds[i].events = POLLIN | POLLOUT;
        }
    }
    else {
        client.receiveMsg(":server :channel with that name does not exist\r\n");
        size_t i = _fd_index_map[client.getFd()];
        this->_pollfds[i].events = POLLIN | POLLOUT;
    }
}

void Server::handleInvite(Client &client, std::stringstream &command)
{
    std::string nickname, channel;

    std::getline(command, nickname, ' ');
    std::getline(command, channel, ' ');

    if (channel[0] != '#')
        return ;
    std::map<std::string, Channel>::iterator itCh = _channels.find(channel);
    if (itCh != _channels.end())
    {
        Channel &ch = itCh->second;

        // if channel is invite-only only operators can invite
        if (ch.getModes() & Channel::E_INVITE_ONLY)
        {
            std::map<std::string, Client *>::const_iterator it = ch.getOperators().find(client.getNickname());
            if (it == ch.getOperators().end()) // Client is not an operator
            {
                client.receiveMsg("ERROR :you are not channel's operator\r\n");
                size_t i = _fd_index_map[client.getFd()];
                this->_pollfds[i].events = POLLIN | POLLOUT;
                return ;
            }
        }
        
        // Invite client with the following name
        for (std::map<int, Client>::iterator it = _clients.begin(); it != _clients.end(); ++it)
        {
            // Send the invitation in format: :<inviter> INVITE <invitee> :<channel>
            if (it->second.getNickname() == nickname)
                it->second.receiveMsg(":" + client.getNickname() + " INVITE " + nickname + " :" + channel + "\r\n");
                size_t i = _fd_index_map[it->second.getFd()];
                this->_pollfds[i].events = POLLIN | POLLOUT;
        }
    }
}

void Server::handleTopic(Client &client, std::stringstream &command)
{

}

void Server::handleMode(Client &client, std::stringstream &command)
{
    std::string first, flags;

    std::getline(command, first, ' ');
    std::getline(command, flags, ' ');

    // Setting modes for a channel
    if (first[0] == '#')
    {
        std::map<std::string, Channel>::iterator it = _channels.find(first);
        if (it == _channels.end())
            return ; // No such channel :(

        Channel &ch = it->second;
        if (!client.isChannelOperator(ch))
            return ;
            
        if (!flags.empty())
        {
            unsigned int modes = 0;
            for (size_t i = 1; i < flags.size(); ++i)
            {
                switch (flags[i])
                {
                case 'i':
                    modes |= modes & Channel::E_INVITE_ONLY;
                    break;
                case 't':
                    modes |= modes & Channel::E_TOPIC_RESTRICT;
                    break;
                case 'k':
                    modes |= modes & Channel::E_CHANNEL_KEY;
                    break;
                case 'l':
                    modes |= modes & Channel::E_USER_LIMIT;
                    break;
                }
            }
            if (flags[0] == '+')
                ch.addModes(modes);
            else
                ch.removeModes(modes);
        }
        else
        {
            // Wanna know the current channel's modes
            // ? Do we want to implement?
        }
    }
    else // no '#'? then it's a user hahaaaaa
    {

    }
}
