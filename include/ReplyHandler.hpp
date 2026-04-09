/* --- ReplyHandler.h --- */

/* ------------------------------------------
Author: Webxxcom
Date: 4/6/2026
------------------------------------------ */

#pragma once

#include "ServerNotifyCodes.hpp"
#include "Client.hpp"

struct ChannelTopic;

class ReplyHandler {
public:
    ReplyHandler();
    ~ReplyHandler();

    void erroneusNick(Client *client, const std::string &nick) const;
    void nicknameAlreadyInUse(Client *client, const std::string& nick) const;
    void badChannelMask(Client *client, const std::string& channelName) const;
    void noSuchChannel(Client* client, const std::string& channelName) const;
    void noSuchNick(Client* client, const std::string& nick) const;
    void notOnChannel(Client* client, const std::string& channelName) const;
    void alreadyOnChannel(Client* client, const std::string &inviteeName, const std::string& channelName) const;
    void userNotInChannel(Client* client, const std::string& nick, const std::string& channelName) const;

    void channelIsFull(Client* client, const std::string& channelName) const;
    void inviteOnlyChannel(Client* client, const std::string& channelName) const;
    void badChannelKey(Client* client, const std::string& channelName) const;
    void keySet(Client* client, const std::string& channelName) const;

    void chanOpPrivsNeeded(Client* client, const std::string& channelName) const;
    void noPrivileges(Client* client) const;

    void unknownMode(Client* client, const std::string &channelName, char mode) const;
    void unknownCommand(Client* client, const std::string& command) const;

    void needMoreParams(Client* client, const std::string& command) const;
    void notRegistered(Client* client) const;
    void alreadyRegistered(Client* client) const;

    void passwdMismatch(Client* client) const;

    // Replies
    void welcome(Client* client) const;
    void channelModeIs(Client* client, const std::string& channelName, const std::string& modes) const;

    void inviting(Client *inviter, Client *invitee, const std::string& channelName) const;

    void nameReply(Client* client, Channel *channel) const;
    void endOfNames(Client* client, const std::string& channelName) const;

    void topicEmpty(Client* client,  const std::string& channelName) const;
    void currentTopic(Client* client,const std::string& channelName, std::string const& topic) const;
    void currentTopicInfo(Client* client, const std::string& channelName, ChannelTopic const& topic) const;

private:
    void handle(irc::ServerNotifyCodes code, Client *client, std::string const& extra = "") const;

};
