#include "../include/irc.hpp"

Client::Client() {}

Client::Client(int fd) : _clientfd(fd) {
    std::cout << "Client created" << std::endl;
}

//todo
Client::Client(const Client &orig) {
    (void)orig;
}

Client &Client::operator=(const Client &orig) {
    (void)orig;
    return *this;
}
Client::~Client() {}
//

void Client::addtoBuffer(std::string msg) {
    this->_buffer.append(msg);
    if (this->_buffer.compare((this->_buffer.length() - 5), 4, "\r\n") == 0) {
        this->_receivedMsg = this->_buffer;
        this->_buffer.clear();
    }
}
