#include "Client.hpp"

#include <iostream>
#include <sstream>
#include "Exceptions.hpp"
#include "Server.hpp"
#include "Channel.hpp"

ClientState::ClientState() : pass_ok(false), has_nick(false), has_user(false) { }

Client::Client() : _state() {}
Client::Client(int fd) : _state(), _fd(fd)
{
    std::cout << "Client connected" << std::endl;
}

Client::Client(const Client &other) { *this = other; }

Client &Client::operator=(const Client &other)
{
    _state = other._state;
    _realname = other._realname;
    _fd = other._fd;
    _nickname = other._nickname;
    _username = other._username;
    _address = other._address;
    _buffer = other._buffer;
    _outMsg = other._outMsg;
    _inMsg = other._inMsg;
    
    return *this;
}

Client::~Client() {
    // std::cout << "Client disconnected" << std::endl;
}

bool Client::operator==(Client const &other) const { return _fd == other._fd; }

std::vector<std::string>    &Client::getReceivedMessages()      { return this->_outMsg; }
std::string                 &Client::getRecvBuffer()            { return this->_buffer; }
const std::string           &Client::getNickname()      const   { return _nickname; }
std::string                 Client::getIrcNickname()    const   { return _state.has_nick ? _nickname : "*"; }
const std::string           &Client::getUsername()      const   { return _username; }
const std::string           &Client::getRealname()      const   { return _realname; }
int                         Client::getFd()             const   { return _fd; }
std::string                 Client::getFullUserPrefix() const   { return _nickname + "!" + _username + "@" + "host"; }

bool Client::isRegistered() const
{
    return _state.has_nick && _state.has_user && _state.pass_ok;
}

bool Client::hasNickname() const
{
    return _state.has_nick;
}

void Client::setPassword(std::string const&)
{
    _state.pass_ok = true;
}

void Client::setUsername(std::string const& realname)
{
    _username = realname;
    _state.has_user = true;
}

void Client::setRealname(std::string const& realname)
{
    _realname = realname;
}

void Client::setNickname(std::string const& nickname)
{
    _nickname = nickname;
    _state.has_nick = true;
}

std::vector<std::string> Client::getInMsg(void) {
    return this->_inMsg;
}

void Client::addtoBuffer(std::string msg) {
    this->_buffer.append(msg);
    // std::cout << "buffer: " << this->_buffer << std::endl;
    // std::cout << "msg: " << msg << std::endl;
    // std::cout << "pass: " << this->_outMsg << std::endl;
    size_t endMsg;
    while ((endMsg = this->_buffer.find("\r\n")) != std::string::npos) {
        std::string singleMsg = this->_buffer.substr(0, endMsg);
        this->_outMsg.push_back(singleMsg);
        this->_buffer.erase(0, endMsg + 2);
    }
}

// Errors are divided into two types: the ones which disconnect the client 
//  and the ones which just send them the error occured to the client.
void Client::receiveMsg(irc::ServerNotifyCodes error_code, std::string const& extra)
{
    std::stringstream msg;
    msg << ":server " << (int)error_code << " " + getIrcNickname();
    if (!extra.empty())
        msg << " ";

    using namespace irc;
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
        case ERR_UNKNOWNMODE:
            msg << extra << " :is unknown mode char to me";
            break;
        case ERR_INVITEONLYCHAN:
            msg << extra << " :Cannot join channel (+i)";
            break;
        case ERR_BADCHANNELKEY:
            msg << extra << " :Cannot join channel (+k)";
            break;
        case ERR_KEYSET:
            msg << extra << " :"; // ! provide message for the error
            break;
        case ERR_NEEDMOREPARAMS:
            msg << extra << " :Not enough parameters";
            break;
        case RPL_INVITING:
            msg << extra;
            break;
        case RPL_NAMREPLY:
            msg << extra << " :LIST OF NAME"; // ! must implement the NAMES
            break;
        case RPL_ENDOFNAMES:
            msg << extra << " :End of NAMES"; // ! must implement the end of names
            break;
        case RPL_CHANNELMODEIS:
            msg << extra;
            break;
        case RPL_WELCOME:
            msg << extra << " :Welcome to the IRC server";
            break;
        default:
            msg << ":Unknown error";
            break;
    }
    msg << "\r\n";
    receiveMsg(msg.str());
}

void Client::receiveMsg(std::string const &msg)
{
    _inMsg.push_back(msg);
}