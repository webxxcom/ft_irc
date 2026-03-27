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

// Changed Client::buffer to static buffer inside of the method.
// Now each command is stored individually inside of `Client::receivedMsgs'
void Client::parseCommands(std::string msg) {
    
}

std::vector<std::string> &Client::getReceivedMessages()
{
    return this->_out;
}
