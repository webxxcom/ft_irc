#pragma once

#include <string>
#include <vector>
#include "ServerNotifyCodes.hpp"
#include <queue>


struct ClientState
{
    bool pass_ok;
    bool has_nick;
    bool has_user;

    ClientState();
};

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
    std::queue<std::string>         _outMsg;
    std::queue<std::string>         _inMsg;
    std::vector<Channel *>          _invitedTo;
public:
    struct NickEquals {
        NickEquals(std::string const& target) {_target = target; };
        bool operator()(Client const* cl) { return cl->getNickname() == _target; }
    public:
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
    std::queue<std::string>& getReceivedMessages();
    std::string &getRecvBuffer();
    const std::string& getNickname() const;
    std::string getIrcNickname() const;
    std::string getFullUserPrefix() const;
    const std::string& getUsername() const;
    const std::string& getRealname() const;
    int getFd() const;
    bool isRegistered() const;
    bool hasNickname() const;

    std::queue<std::string> getInMsg(void) const;
    void clearinMsg();
    void clearOutMsg(void);
    void addinMsg(std::string remainder);

    bool isInvitedTo(Channel *ch) const;
    void addtoBuffer(std::string msg);

    // Setters
    void setNickname(std::string const& nickname);
    void setPassword(std::string const& password);
    void setUsername(std::string const& username);
    void setRealname(std::string const& username);
    void getsInvitedTo(Channel *ch);
    
    void receiveMsg(std::string const& msg);

    friend class Tester;
} ;