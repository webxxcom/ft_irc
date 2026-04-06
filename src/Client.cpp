#include "Client.hpp"

#include <iostream>
#include <sstream>
#include "Exceptions.hpp"
#include "Server.hpp"
#include "Channel.hpp"
#include <algorithm>

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
std::string                 Client::getFullUserPrefix() const   { return _nickname + "!" + _username + "@" + "server"; }

bool Client::isRegistered() const
{
    return _state.has_nick && _state.has_user && _state.pass_ok;
}

bool Client::hasNickname() const
{
    return _state.has_nick;
}

bool Client::isInvitedTo(Channel *ch) const
{
    return std::find(_invitedTo.begin(), _invitedTo.end(), ch) != _invitedTo.end();
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

void Client::getsInvitedTo(Channel *ch) { _invitedTo.push_back(ch); }

void Client::setNickname(std::string const& nickname)
{
    _nickname = nickname;
    _state.has_nick = true;
}

std::vector<std::string> &Client::getInMsg(void) {
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

void Client::receiveMsg(std::string const &msg)
{
    std::string toSend = msg;

    if (msg[msg.size() - 1] != '\n' && msg[msg.size() - 2] != '\r')
        toSend.append("\r\n");
    
    _inMsg.push_back(toSend);
}