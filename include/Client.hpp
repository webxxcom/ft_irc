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
        ClientState     state; // state to check the state
        int             _clientfd;
        std::string     _nickName;
        std::string     _username;
        std::string     _realname; // added realname just because
        std::string     _address;
        std::string     _buffer;
        std::stack<std::string>     _out; // changed vector to stack to efficiently pop the elems

    public:

        // Constructors
        Client();
        Client(int fd);
        Client(const Client &orig);
        Client&operator=(const Client &orig);
        ~Client();

        // Getters
        std::stack<std::string>& getReceivedMessages();
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
        


} ;

#endif