#pragma once

#include "irc.hpp"

class Channel {
private:
    std::string                 _name;
    std::vector<std::string>    _messages;
    std::set<Client *>          _members;
    std::set<Client *>          _operators;
    unsigned int                _modes;

public:
    enum ChannelModes {
        E_INVITE_ONLY = 0x1,
        E_TOPIC_RESTRICT = 0x2,
        E_CHANNEL_KEY = 0x4,
        E_USER_LIMIT = 0x8
    };

    Channel(std::string const& name);
    Channel(Client &creator, std::string const& name);

    // Getters
    std::string const& getName() const;
    std::set<Client *> const& getMembers() const;
    std::set<Client *> const& getOperators() const;
    std::vector<std::string> const& getMessages() const;
    unsigned int getModes() const;

    // ??
    std::pair<Client *, bool> hasMember(std::string)

    // Modifiers
    void setModes(unsigned int modes);
    void addModes(unsigned int mode);
    void removeModes(unsigned int mode);
    void addMember(Client &user);
    void addOperator(Client &user);
};
