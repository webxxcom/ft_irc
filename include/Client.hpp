#pragma once

#include <string>
#include <vector>
#include "ServerNotifyCodes.hpp"

struct ClientState
{
    bool pass_ok;
    bool has_nick;
    bool has_user;

    ClientState();
};

class Server;
class Channel;

class Client {
private:
    ClientState                     _state; // state to check the state
    std::string                     _realname; // added realname just because
    int                             _fd;
    std::string                     _nickname;
    std::string                     _username;
    std::string                     _address;
    std::string                     _buffer;
    std::vector<std::string>        _outMsg; // changed vector to stack to efficiently pop the elems
    std::vector<std::string>        _inMsg;
    std::vector<Channel *>          _invitedTo;
public:

    struct NickEquals {
        NickEquals(std::string const& target) {_target = target; };
        bool operator()(Client const* cl) {return cl->getNickname() == _target; }
    private:
        std::string _target;
    };

    // Constructors
    Client();
    Client(int fd);
    Client(const Client &orig);
    Client&operator=(const Client &orig);
    ~Client();

    bool operator==(Client const& other) const;

    // Getters
    std::vector<std::string>& getReceivedMessages();
    std::string &getRecvBuffer();
    const std::string& getNickname() const;
    std::string getIrcNickname() const;
    const std::string& getUsername() const;
    const std::string& getRealname() const;
    int getFd() const;
    std::string getFullUserPrefix() const;
    bool isRegistered() const;
    bool hasNickname() const;
    bool isInvitedTo(Channel *ch) const;

    // Setters
    void setNickname(std::string const& nickname);
    void setPassword(std::string const& password);
    void setUsername(std::string const& username);
    void setRealname(std::string const& username);
    void getsInvitedTo(Channel *ch);
    
    std::vector<std::string> getInMsg(void);
    void addtoBuffer(std::string msg);

    void receiveMsg(std::string const& msg);

    friend class Tester;
} ;