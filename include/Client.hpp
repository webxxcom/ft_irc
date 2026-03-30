#ifndef CLIENT_HPP
#define CLIENT_HPP

#include "irc.hpp"
#include <stack>

struct ClientState
{
    bool pass_ok;
    bool has_nick;
    bool has_user;
};

class Client {
    private:
        ClientState                     state; // state to check the state
        std::string                     _realname; // added realname just because
        int                             _clientfd;
        std::string                     _nickname;
        std::string                     _username;
        std::string                     _address;
        std::string                     _buffer;
        std::vector<std::string>        _outMsg; // changed vector to stack to efficiently pop the elems
        std::vector<std::string>        _inMsg;
    public:

        // Constructors
        Client();
        Client(int fd);
        Client(const Client &orig);
        Client&operator=(const Client &orig);
        ~Client();

        // Getters
        std::vector<std::string>& getReceivedMessages();
        std::string &getRecvBuffer();
        const std::string& getNickname() const;
        const std::string& getUsername() const;
        const std::string& getRealname() const;
        bool isRegistered() const;

        // Setters
        void setNickname(std::string const& nickname);
        void setPassword(std::string const& password);
        void setUsername(std::string const& username);
        void setRealname(std::string const& username);
        
        std::vector<std::string> getinMsg(void);
        void addtoBuffer(std::string msg);
} ;

#endif