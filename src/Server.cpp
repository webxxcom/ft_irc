#include "Server.hpp"
#include "irc.hpp"
#include <cstring>
#include <sstream>

Server::Server(int ac, char *av[]) : _serverSocketfd(-1) {
    int status = this->parseArgs(ac, av);
    if (status == ARGS_NUM_INVALID)
        throw ServerErrorException("Arguments invalid\nRun with: ./irc <port> <password>");
    else if (status == PORT_NUM_INVALID)
        throw ServerErrorException("Port number is invalid");
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
    _clients.insert(std::pair<int, Client>(clientfd.fd, Client(clientfd.fd)));
}

void Server::disconnectClient(Client &client) {
    // Now this method takes no `i' so we must get it from somewhere to clean the Server::_pollfds
    // Created a map to store fd -> index to be able to get the client's index in the Server::_pollfds by client's fd
    //  which it has inside ( Client::_fd ) 
    int const i = _fd_index_map[client.getFd()];

    close(this->_pollfds[i].fd);
    this->_clients.erase(this->_pollfds[i].fd);
    this->_pollfds.erase(this->_pollfds.begin() + i);
}

void Server::handleClientCommands(Client &client)
{
    std::vector<std::string> &mssgs = client.getReceivedMessages();

    while (!mssgs.empty())
    {
        std::stringstream line;
        line << mssgs.back();
        mssgs.pop_back();

        handleClientCommand(client, line);
    }
}

// Pass the error handling to a separate function with error_codes to send to the client
// Exception we throw so far has the message to send to the client but the design is under the question
//  because exception's messages for us to get not for us to send to some application.
//  Bad practice here but i guess we've got no other choice? debatable
void Server::serverError(int error_code, Client &client)
{
    std::string msg;
    if (error_code == BAD_SERVER_PASSWORD)
    {
        msg = ":server 464 " +
            (client.hasNickname() ? client.getNickname() : "*")
            + " :Password incorrect";
    }
    msg += "\r\n";
    throw ClientException(msg);
}

void Server::handleClientCommand(Client &client, std::stringstream &command)
{
    std::string word;
    std::getline(command, word, ' ');

    if (word == "PASS")
    {
        // Client sent the password they used to connect to the server
        std::getline(command, word, ' ');
        if (word != _password)
            // The passwords do not match -- send the message it's incorrect
            serverError(464, client);
        else
            client.setPassword(word); // set password if correct
    }
    else if (word == "NICK")
    {
        // Client sent their nickname
        std::getline(command, word, ' ');
        client.setNickname(word);
    }
    else if (word == "CAP")
    {
        // Client sent a request to get the server's capabilities

        // CAP LS 302

        // some capabilities logic
        // need to answer to the server?

        // Answer like CAP * LS :<capabilities>
    }
    else if (word == "USER")
    {
        // Client sent their username, mode. `*' and real name

        // Username
        std::getline(command, word, ' ');
        client.setUsername(word);

        // Mode
        std::getline(command, word, ' ');

        // Asterix
        std::getline(command, word, ' ');

        // Real name
        std::getline(command, word, ' ');
        client.setRealname(word);
    }
}

void Server::receiveClientData(Client &client)
{
    std::string &buffer = client.getRecvBuffer();
    char temp[512];

    // Getting the data from the socket
    ssize_t bytesread = recv(client.getFd(), temp, sizeof(temp), 0);
    if (bytesread > 0) // The condition which we want to happen most of the times should be the first to increase readability
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
    for (int i = 0; i < msgtoSend.size(); i++) {
        do { // replaced with do while because `bytessend' can't be initialized
            bytessend = send(this->_pollfds[i].fd, &msgtoSend[i], sizeof(msgtoSend[i]), 0); // ! std::string::size()?
        } while (bytessend);
        if (bytessend == -1);
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
                    receiveClientData(this->_clients[this->_pollfds[i].fd]);
                }
                else if (this->_pollfds[i].revents & POLLOUT) {
                    messageClient(this->_clients.at(this->_pollfds[i].fd));
                }
                else if (this->_pollfds[i].revents & (POLLERR | POLLHUP | POLLNVAL)) {
                    //issue with client, need to remove him, free fd, erase from all lists
                }
            }
            ++i;
        }
        // For now let us assume that if client disconnects due to an error we always have to send the respond
        // Except for the case when client disconnects themselves : exception is not thrown obviously
        catch(const ClientException& e)
        {
            ::send(this->_pollfds[i].fd, e.what(), strlen(e.what()), 0); // TEST: `messageClient' didn't work
            disconnectClient(this->_clients[this->_pollfds[i].fd]);
        }
    }
}

