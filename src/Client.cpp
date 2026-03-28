#include "../include/irc.hpp"
#include "Client.hpp"

Client::Client() {}

Client::Client(int fd) : _clientfd(fd) {
    std::cout << "Client connected" << std::endl;
}

//todo
Client::Client(const Client &orig) {
    (void)orig;
}

Client &Client::operator=(const Client &orig) {
    (void)orig;
    return *this;
}
Client::~Client() {
    // std::cout << "Client disconnected" << std::endl;
}

std::stack<std::string> &Client::getReceivedMessages() { return this->_out; }
std::string &Client::getRecvBuffer() { return this->_buffer; }
const std::string &Client::getNickname() const { return _nickName; }
const std::string &Client::getUsername() const { return _username; }
const std::string &Client::getRealname() const { return _realname; }

bool Client::isRegistered() const
{
    return state.has_nick && state.has_user && state.pass_ok;
}

void Client::setPassword(std::string const&)
{
    state.pass_ok = true;
}

void Client::setUsername(std::string const& realname)
{
    _username = realname;
    state.has_user = true;
}

void Client::setRealname(std::string const& realname)
{
    _realname = realname;
}

void Client::setNickname(std::string const& nickname)
{
    _nickName = nickname;
    state.has_nick = true;
}
