
#include "RespectBot.hpp"
#include <cstring>
#include <sys/poll.h>
#include <netdb.h>
#include <unistd.h>
#include <iostream>
#include <cerrno>
#include <sstream>

static int connectToServer(const std::string& host, const std::string& port)
{
    struct addrinfo hints;
    struct addrinfo *res;

    std::memset(&hints, 0, sizeof(hints));
    hints.ai_family   = AF_UNSPEC;
    hints.ai_socktype = SOCK_STREAM;

    if (getaddrinfo(host.c_str(), port.c_str(), &hints, &res) != 0)
        return -1;

    int sock = -1;

    for (struct addrinfo *p = res; p; p = p->ai_next)
    {
        sock = socket(p->ai_family, p->ai_socktype, p->ai_protocol);
        if (sock < 0)
            continue;

        if (connect(sock, p->ai_addr, p->ai_addrlen) == 0)
            break;
        else
            std::cout << "connect failed: " << strerror(errno) << "\n";

        close(sock);
        sock = -1;
    }

    freeaddrinfo(res);
    return sock;
}

void RespectBot::processRegistration(std::string const& password)
{
    std::string const pass = "PASS " + password + "\r\n";
    std::string const nick = "NICK " + _nickname + "\r\n";
    std::string const user = "USER " + _nickname + " 0 * :" + _nickname + "\r\n";

    sendDataToServer(pass + nick + user);
}

// :webxxcom!webxxcom@server INVITE webxxcom :#test
void RespectBot::processInvite(std::string const& line)
{
    std::string inviter;
    std::string channelName;

    // Check that exclamation mark really exists
    std::size_t pos = line.find('!');
    if (pos == std::string::npos)
        return ;

    // extract the inviter
    inviter = line.substr(1, pos - 1);

    // Now find the trailing param which is the channel's name
    pos = line.find(':', 1);
    if (pos == std::string::npos)
        return;

    // Extract the channel's name
    channelName = line.substr(pos + 1);
    if (channelName[0] != '#')
        return ;

    // If INVITE reached me then I'm for sure not on this channel (443 error)
    respectCounter[channelName] = std::map<std::string, int>();
    std::string joinMsg = "JOIN " + channelName + "\r\n";

    // Send inviter the message
    std::string successMsg = "PRIVMSG " + inviter + " :I attempted to join `" + channelName + "' As you asked me\r\n";

    sendDataToServer(joinMsg + successMsg);
}

// :webxxcom!webxxcom@server PRIVMSG #test :+rep webxxcom
// :webxxcom!webxxcom@server PRIVMSG #test :display respect webxxcom
void RespectBot::processPrivmsg(std::stringstream &ss)
{
    std::string token;
    std::string channelName;

    if (!(ss >> channelName))
        return ;

    respectMap::iterator it = respectCounter.find(channelName);
    if (it == respectCounter.end())
    {
        std::cerr << ("For some reason i was not able to find channel `" + channelName + "'\n");
        return ;
    }

    std::getline(ss, token, ':');
    if (!(ss >> token))
        return ;

    if (token == "display")
    {
        if (!(ss >> token))
            return;

        if (token != "respect")
            return ;
        
        if (!(ss >> token))
            return;

        std::stringstream out;
        int val = respectCounter[channelName][token];
        out << "PRIVMSG " + channelName + " :User `" + token + "' has " << val << " points of respect\r\n";
        sendDataToServer(out.str());
    }
    else if (token == "+rep" || token == "-rep")
    {
        int sign = (token[0] == '-') ? -1 : 1;

        if (!(ss >> token))
            return;
        it->second[token] += sign;
    }
}

void RespectBot::processLine(std::string const &line)
{
    std::stringstream ss(line);
    std::string token;

    ss >> token >> token;
    if (token == "PRIVMSG")
        processPrivmsg(ss);
    else if (token == "INVITE")
        processInvite(line);
}

RespectBot::RespectBot(std::string const &host, std::string const &port, std::string const &pass)
    : _nickname("RespectBot")
{
    _sock = connectToServer(host, port);
    if (_sock != -1)
        processRegistration(pass);
        
}
RespectBot::~RespectBot() { close(_sock); }

void RespectBot::listen()
{
    if (_sock == -1)
        return ;

    char        buf[1024];
    std::string recvBuffer;

    while (true)
    {
        int received = recv(_sock, buf, sizeof (buf), 0);
        if (received <= 0)
        {
            std::cout << "Server closed connection" << std::endl;
            return ;
        }
        buf[received] = '\0';
        recvBuffer.append(buf);

        size_t pos;
        while ((pos = recvBuffer.find("\r\n")) != std::string::npos)
        {
            std::string line = recvBuffer.substr(0, pos);
            recvBuffer = recvBuffer.substr(pos + 2);

            processLine(line);
        }
    }
}

void RespectBot::sendDataToServer(std::string const& data) const
{
    ssize_t total = 0;
    while (total < data.size())
    {
        ssize_t sent = ::send(_sock, data.data(), data.size(), 0);

        if (sent == -1)
        {
            std::cout << "Server closed connection" << std::endl;
            return ;
        }

        total += sent;
    }
}

std::string RespectBot::getIrcNickname() const
{
    return _nickname + "!" + _nickname + "@server";
}
