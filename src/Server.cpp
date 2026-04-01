#include "Server.hpp"
#include "irc.hpp"
#include <cstring>
#include <sstream>
#include <algorithm>

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
    _fd_index_map[clientfd.fd] = _pollfds.size() - 1;

    Client *cl = new Client(clientfd.fd);
    _clients.push_back(cl);
    _clientsByFd.insert(std::pair<int, Client *>(cl->getFd(), cl));
    _clientsByName.insert(std::pair<std::string, Client *>(cl->getNickname(), cl));
}

// before disconnecting the client we want to send all of the messages to them
void Server::disconnectClient(Client *client) {
    // Now this method takes no `i' so we must get it from somewhere to clean the Server::_pollfds
    // Created a map to store fd -> index to be able to get the client's index in the Server::_pollfds by client's fd
    //  which it has inside ( Client::_fd ) 
    int const i = _fd_index_map[client->getFd()];

    shutdown(client->getFd(), SHUT_WR);
    close(this->_pollfds[i].fd);
    _fd_index_map.erase(this->_pollfds[i].fd);

    _clientsByFd.erase(client->getFd());
    _clientsByName.erase(client->getNickname());
    _clients.erase(std::find(_clients.begin(), _clients.end(), client));

    this->_pollfds.erase(this->_pollfds.begin() + i);
}

void Server::handleClientCommands(Client *client)
{
    std::vector<std::string> &mssgs = client->getReceivedMessages();

    while (!mssgs.empty())
    {
        // Get the whole line
        std::stringstream line;
        line << mssgs.back();
        mssgs.pop_back();

        // Extract single command
        std::string command;
        std::getline(line, command, ' ');

        // Find the command in the map
        std::map<std::string, CommandHandler>::iterator it = _command_map.find(command);
        if (it != _command_map.end())
            (this->*(it->second))(client, line);
        else
            notifyClient(client, ERR_UNKNOWN_COMMAND, command);
    }
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
        handleClientCommands(client);

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
        disconnectClient(client); // Should we throw an exception?
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

//////////////////////// COMMANDS /////////////////////////////

void Server::handlePass(Client *client, std::stringstream& command)
{
    std::string word;

    // Client sent the password they used to connect to the server
    std::getline(command, word, ' ');
    if (word != _password)
        // The passwords do not match -- send the message it's incorrect
        notifyClient(client, ERR_PASSWDMISMATCH);
    else
        client->setPassword(word); // set password if correct
}

void Server::handleUser(Client *client, std::stringstream& command)
{
    std::string word;
    
    // Username
    std::getline(command, word, ' ');
    client->setUsername(word);

    // Mode, Asterix
    std::getline(command, word, ' ');
    std::getline(command, word, ' ');

    // Real name
    std::getline(command, word, ' ');
    client->setRealname(word);
}

void Server::handleNick(Client *client, std::stringstream& command)
{
    std::string word;
   
    std::getline(command, word, ' ');
    client->setNickname(word);
}

void Server::handleCap(Client *client, std::stringstream& command)
{
    std::string word;
    
    std::getline(command, word, ' ');
    if (word == "LS") // 
        client->receiveMsg(":server CAP * LS :\r\n"); // No capabilities
    else // Do not support any other than LS
        notifyClient(client, ERR_UNKNOWN_COMMAND, word);
}

// JOIN <channels> [<keys>]
void Server::handleJoin(Client *client, std::stringstream &command)
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
        if (word[0] != '#' || word.size() < 2)
            continue ;

        std::pair<Channel *, bool> optChannel = _channelsByName.find(word);
        if (optChannel.second)
            optChannel.first->addMember(client);
        else
            createChannel(client, word);
    }
}

// KICK <channel> <client> :[<message>]
void Server::handleKick(Client *client, std::stringstream &command)
{
    std::string channel, member, message;
    std::getline(command, channel, ' ');
    std::getline(command, member, ' ');
    std::getline(command, message, ' ');

    if (channel[0] != '#')
        return ;
    std::pair<Channel *, bool> optCh = _channelsByName.find(channel);
    if (optCh.second)
    {
        Channel *ch = optCh.first;

        // are YOU a channel's operator first of all??
        if (ch->hasOperator(client))
        {
            std::pair<Client *, bool> optMember = ch->hasMember(member);
            if (optMember.second)
            {
                // :<source> KICK <channel> <target> :<reason>
                optMember.first->receiveMsg(
                    ":" + client->getNickname() + "!" + client->getUsername() + "@host" +
                    "KICK " + channel + " " +
                    member + " " + message
                );
            }
            else
                notifyClient(client, ERR_USERNOTINCHANNEL, member + " " + channel);
        }
        else
            notifyClient(client, ERR_NOPRIVILEGES);
    }
    else
        notifyClient(client, ERR_NOSUCHCHANNEL, channel);
}

// INVITE <nickname> <channel>
void Server::handleInvite(Client *client, std::stringstream &command)
{
    std::string nickname, channel;

    std::getline(command, nickname, ' ');
    std::getline(command, channel, ' ');

    if (channel[0] != '#' || channel.size() < 2)
        return ;

    std::pair<Channel *, bool> optChannel = _channelsByName.find(channel);
    if (optChannel.second)
    {
        Channel *ch = optChannel.first;

        if (ch->hasMember(client))
        {
            // if channel is invite-only only operators can invite
            if (ch->getModes() & Channel::E_INVITE_ONLY)
            {
                if (!ch->hasOperator(client))
                {
                    notifyClient(client, ERR_CHANOPRIVSNEEDED, client->getIrcNickname() + " " + channel);
                    return ;
                }
            }

            std::pair<Client *, bool> optInvitee = _clientsByName.find(nickname);
            if (optInvitee.second)
            {
                Client *invitee = optInvitee.first;

                // :<inviter>!<user>@<host> INVITE <invitee> :<channel>
                invitee->receiveMsg(client->getFullUserPrefix() + " INVITE " + invitee->getNickname() + ":" + channel); // ! Not sure about username

                // :server 341 <inviter> <invitee> <channel>
                notifyClient(client, RPL_INVITING, client->getNickname() + " " + invitee->getNickname() + " " + channel);
            }
            else
                notifyClient(client, ERR_NOSUCHNICK);
        }
        else
            notifyClient(client, ERR_NOTONCHANNEL, client->getIrcNickname() + " " + channel);
    }
    else
        notifyClient(client, ERR_NOSUCHCHANNEL, channel);
}

void Server::handleTopic(Client *client, std::stringstream &command)
{

}

void Server::handleMode(Client *client, std::stringstream &command)
{
    // std::string first, flags;

    // std::getline(command, first, ' ');
    // std::getline(command, flags, ' ');

    // // Setting modes for a channel
    // if (first[0] == '#')
    // {
    //     std::map<std::string, Channel>::iterator it = _channels.find(first);
    //     if (it == _channels.end())
    //         return ; // No such channel :(

    //     Channel &ch = it->second;
    //     if (!client.isOperatorOf(ch))
    //         return ;
            
    //     if (!flags.empty())
    //     {
    //         unsigned int modes = 0;
    //         for (size_t i = 1; i < flags.size(); ++i)
    //         {
    //             switch (flags[i])
    //             {
    //             case 'i':
    //                 modes |= modes & Channel::E_INVITE_ONLY;
    //                 break;
    //             case 't':
    //                 modes |= modes & Channel::E_TOPIC_RESTRICT;
    //                 break;
    //             case 'k':
    //                 modes |= modes & Channel::E_CHANNEL_KEY;
    //                 break;
    //             case 'l':
    //                 modes |= modes & Channel::E_USER_LIMIT;
    //                 break;
    //             }
    //         }
    //         if (flags[0] == '+')
    //             ch.addModes(modes);
    //         else
    //             ch.removeModes(modes);
    //     }
    //     else
    //     {
    //         // Wanna know the current channel's modes
    //         // ? Do we want to implement?
    //     }
    // }
    // else // no '#'? then it's a user hahaaaaa
    // {

    // }
}
