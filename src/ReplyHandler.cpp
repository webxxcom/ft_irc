/* --- ReplyHandler.cpp --- */

/* ------------------------------------------
author: Webxxcom
date: 4/6/2026
------------------------------------------ */

#include "ReplyHandler.hpp"

#include <sstream>
#include "Exceptions.hpp"
#include "Channel.hpp"
#include "Server.hpp"
#include <iostream>
#include <iomanip>

ReplyHandler::ReplyHandler(Server &server) : _server(server) { }
ReplyHandler::~ReplyHandler() { }

using namespace irc;

void ReplyHandler::noNickSupplied(Client *client) const
{
    handle(ERR_NONICKNAMEGIVEN, client);
}

void ReplyHandler::erroneusNick(Client *client, const std::string &nick) const
{
    handle(ERR_ERRONEUSNICKNAME, client, nick);
}

void ReplyHandler::nicknameAlreadyInUse(Client *client, const std::string &nick) const
{
    handle(ERR_NICKNAMEINUSE, client, nick);
}

void ReplyHandler::badChannelMask(Client *client, const std::string &channelName) const
{
    handle(ERR_BADCHANMASK, client, channelName);
}

void ReplyHandler::noSuchChannel(Client *client, std::string const &channelName) const
{
    handle(ERR_NOSUCHCHANNEL, client, channelName);
}

void ReplyHandler::noSuchNick(Client *client, const std::string &nick) const
{
    handle(ERR_NOSUCHNICK, client, nick);
}

void ReplyHandler::notOnChannel(Client *client, const std::string &channelName) const
{
    handle(ERR_NOTONCHANNEL, client, channelName);
}

void ReplyHandler::alreadyOnChannel(Client *client, const std::string &inviteeName, const std::string &channelName) const
{
    handle(ERR_USERONCHANNEL, client, inviteeName + " " + channelName);
}

void ReplyHandler::userNotInChannel(Client *client, const std::string &nick, const std::string &channelName) const
{
    handle(ERR_USERNOTINCHANNEL, client, nick + " " + channelName);
}

void ReplyHandler::noRecipient(Client *client, std::string const &command) const
{
    handle(ERR_NORECIPIENT, client, command);
}

void ReplyHandler::noTextToSend(Client *client)
{
    handle(ERR_NOTEXTTOSEND, client);
}

void ReplyHandler::inviteOnlyChannel(Client *client, std::string const &channelName) const
{
    handle(ERR_INVITEONLYCHAN, client, channelName);
}

void ReplyHandler::badChannelKey(Client *client, const std::string &channelName) const
{
    handle(ERR_BADCHANNELKEY, client, channelName);
}

void ReplyHandler::keySet(Client *client, const std::string &channelName) const
{
    handle(ERR_KEYSET, client, channelName);
}

void ReplyHandler::chanOpPrivsNeeded(Client *client, const std::string &channelName) const
{
    handle(ERR_CHANOPRIVSNEEDED, client, channelName);
}

void ReplyHandler::noPrivileges(Client *client) const
{
    handle(ERR_NOPRIVILEGES, client);
}

void ReplyHandler::unknownMode(Client *client, const std::string &channelName, char mode) const
{
    handle(ERR_UNKNOWNMODE, client, channelName + " " + std::string(1, mode));
}

void ReplyHandler::unknownCommand(Client *client, const std::string &command) const
{
    handle(ERR_UNKNOWN_COMMAND, client, command);
}

void ReplyHandler::needMoreParams(Client *client, const std::string &command) const
{
    handle(ERR_NEEDMOREPARAMS, client, command);
}

void ReplyHandler::notRegistered(Client *client) const
{
    handle(ERR_NOTREGISTERED, client);
}

void ReplyHandler::alreadyRegistered(Client *client) const
{
    handle(ERR_ALREADYREGISTERED, client);
}

void ReplyHandler::passwdMismatch(Client *client) const
{
    handle(ERR_PASSWDMISMATCH, client);
}

void ReplyHandler::badFileSessionToken(Client *client, std::string const &token) const
{
    handle(ERR_BADFILESESSTOKEN, client, token);
}

void ReplyHandler::fileIsAbsent(Client *client, std::string const &filename) const
{
    handle(ERR_FILEISABSENT, client, filename);
}

void ReplyHandler::pong(Client* client, std::string const &token) const
{
    handle(RPL_PONG, client, token);
}

void ReplyHandler::welcome(Client *client) const
{
    handle(RPL_WELCOME, client);
    client->setWasWelcomed(true);
}

void ReplyHandler::channelModeIs(Client *client, const std::string &channelName, const std::string &modes) const
{
    handle(RPL_CHANNELMODEIS, client, channelName + " " + modes);
}

void ReplyHandler::inviting(Client *inviter, Client *invitee, const std::string &channelName) const
{
	// :<inviter>!<user>@<host> INVITE <invitee> :<channel>
    invitee->receiveMsg(":" + inviter->getFullUserPrefix() + " INVITE " + invitee->getNickname() + " :" + channelName + "\r\n");
    // ! Not sure about username

	// :server 341 <inviter> <invitee> <channel>
    handle(RPL_INVITING, inviter, invitee->getNickname() + " " + channelName);
}

void ReplyHandler::nameReply(Client *client, Channel *channel) const
{
    std::string names, symbol = channel->isInviteOnly() ? "*" : "=";

    for (std::set<Client*>::iterator it = channel->getMembers().begin(); it != channel->getMembers().end(); ++it)
    {
        if (channel->hasOperator(*it))
        {
            if (!names.empty()) names += " ";
            names += "@" + (*it)->getIrcNickname();
        }
    }

    for (std::set<Client*>::iterator it = channel->getMembers().begin(); it != channel->getMembers().end(); ++it)
    {
        if (!channel->hasOperator(*it))
        {
            if (!names.empty()) names += " ";
            names += (*it)->getIrcNickname();
        }
    }

    handle(RPL_NAMREPLY, client, symbol + " " + channel->getName() + " :" + names);
}

void ReplyHandler::endOfNames(Client *client, const std::string &channelName) const
{
    handle(RPL_ENDOFNAMES, client, channelName);
}

void ReplyHandler::channelIsFull(Client *client, std::string const &channelName) const
{
    handle(ERR_CHANNELISFULL, client, channelName);
}

void ReplyHandler::topicEmpty(Client *client, std::string const &channelName) const {
    handle(RPL_NOTOPIC, client, channelName);
}

void ReplyHandler::currentTopic(Client* client, std::string const &channelName, std::string const &topic) const {
    std::string extra = channelName + " :" + topic;
    handle(RPL_TOPIC, client, extra);
}

void ReplyHandler::currentTopicInfo(Client* client, std::string const& channelName, ChannelTopic const& topic) const {
    std::string extra = channelName + " " + topic._setby + " " + topic._time;
    handle(RPL_TOPICWHOTIME, client, extra);
}

// Errors are divided into two types: the ones which disconnect the client 
//  and the ones which just send them the error occured to the client.
void ReplyHandler::handle(irc::ServerNotifyCodes code, Client *client, std::string const &extra) const
{
    std::stringstream msg;
    msg << ":server " << std::setw(3) << std::setfill('0') << (int)code << " " + client->getIrcNickname();
    if (!extra.empty())
        msg << " ";

    using namespace irc;
    switch (code)
    {
        case ERR_PASSWDMISMATCH:
            msg << ":Password incorrect\r\n";
            break;
        case ERR_ALREADYREGISTERED:
            msg << ":already registered\r\n";
            break;
        case ERR_NOTREGISTERED:
            msg << ":You have not registered\r\n";
            break;
        case ERR_UNKNOWN_COMMAND:
            msg << extra << " :Unknown command";
            break;
        case ERR_NONICKNAMEGIVEN:
            msg << " :No nickname given";
            break;
        case ERR_NOSUCHNICK:
            msg << extra << " :No such nick/channel";
            break;
        case ERR_NICKNAMEINUSE:
            msg << extra << " :Nickname is already in use";
            break;
        case ERR_NORECIPIENT:
            msg << " :No recipient given " << extra;
            break;
        case ERR_NOTEXTTOSEND:
            msg << " :No text to send";
            break;
        case ERR_ERRONEUSNICKNAME:
            msg << extra << " :Erroneous nickname";
            break;
        case ERR_NOSUCHCHANNEL:
            msg << extra << " :No such channel";
            break;
        case ERR_USERONCHANNEL:
            msg << extra << " :is already on channel";
            break;
        case ERR_NOTONCHANNEL:
            msg << extra << " :You're not on that channel";
            break;
        case ERR_NOPRIVILEGES:
            msg << extra << " :Permission Denied- You're not an IRC operator";
            break;
        case ERR_USERNOTINCHANNEL:
            msg << extra << " :They aren't on that channel";
            break;
        case ERR_CHANOPRIVSNEEDED:
            msg << extra << " :You're not channel operator";
            break;
        case ERR_UNKNOWNMODE:
            msg << extra << " :is unknown mode char";
            break;
        case ERR_INVITEONLYCHAN:
            msg << extra << " :Cannot join channel (+i)";
            break;
        case ERR_BADCHANNELKEY:
            msg << extra << " :Cannot join channel (+k)";
            break;
        case ERR_CHANNELISFULL:
            msg << extra << " :Cannot join channel (+l)";
            break;
        case ERR_BADCHANMASK:
            msg << extra << " :Bad Channel Mask";
            break;
        case ERR_KEYSET:
            msg << extra << " :Channel key already set";
            break;
        case ERR_NEEDMOREPARAMS:
            msg << extra << " :Not enough parameters";
            break;
        case ERR_BADFILESESSTOKEN:
            msg << extra << " :Bad file transfer session token";
            break;
        case ERR_FILEISABSENT:
            msg << extra << " :File is not present";
            break;

        case RPL_PONG:
            msg.str(""); msg.clear();
            msg << ":server PONG server :" << extra;
            break;
        case RPL_INVITING:
            msg << extra;
            break;
        case RPL_NAMREPLY:
            msg << extra;
            break;
        case RPL_ENDOFNAMES:
            msg << extra << " :End of /NAMES list";
            break;
        case RPL_CHANNELMODEIS:
            msg << extra;
            break;
        case RPL_WELCOME:
            msg << extra << " :Welcome to the IRC server";
            break;
        case RPL_NOTOPIC:
            msg << extra << " :No topic is set";
            break;
        case RPL_TOPIC:
            msg << extra;
            break;
        case RPL_TOPICWHOTIME:
            msg << extra;
            break;
        default:
            msg << ":ServerNotifyCode was not implemented for this code";
            break;
    }
    msg << "\r\n";
    client->receiveMsg(msg.str());
    //_server.clientIsReady(client);
}
