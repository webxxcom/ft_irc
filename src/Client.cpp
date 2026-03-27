#include "../include/irc.hpp"

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
//

std::vector<std::string> Client::getinMsg(void) {
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
