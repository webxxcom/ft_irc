/* --- ReplyHandler.cpp --- */

/* ------------------------------------------
author: Webxxcom
date: 4/6/2026
------------------------------------------ */

#include "ReplyHandler.hpp"

#include <sstream>
#include "Exceptions.hpp"
#include "Channel.hpp"

ReplyHandler::ReplyHandler() { }
ReplyHandler::~ReplyHandler() { }

using namespace irc;

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

void ReplyHandler::welcome(Client *client) const
{
    handle(RPL_WELCOME, client);
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

// Errors are divided into two types: the ones which disconnect the client 
//  and the ones which just send them the error occured to the client.
void ReplyHandler::handle(irc::ServerNotifyCodes code, Client *client, std::string const &extra) const
{
    std::stringstream msg;
    msg << ":server " << (int)code << " " + client->getIrcNickname();
    if (!extra.empty())
        msg << " ";

    using namespace irc;
    switch (code)
    {
        case ERR_PASSWDMISMATCH:
            msg << ":Password incorrect\r\n";
            throw ClientException(msg.str());
        case ERR_ALREADYREGISTERED:
            msg << ":already registered\r\n";
            throw ClientException(msg.str());
        case ERR_NOTREGISTERED:
            msg << ":You have not registered\r\n";
            throw ClientException(msg.str());
        case ERR_UNKNOWN_COMMAND:
            msg << extra << " :Unknown command";
            break ;
        case ERR_NOSUCHNICK:
            msg << extra << " :No such nick/channel";
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
        default:
            msg << ":Unknown error";
            break;
    }
    msg << "\r\n";
    client->receiveMsg(msg.str());
}
