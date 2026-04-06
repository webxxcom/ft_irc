#include "CommandHandler.hpp"
#include "Server.hpp"
#include <cstdlib>
#include <iostream>

using namespace irc;

void CommandHandler::setupCommands()
{
	_commandMap["PASS"]     = &CommandHandler::handlePass;
	_commandMap["NICK"]     = &CommandHandler::handleNick;
	_commandMap["USER"]     = &CommandHandler::handleUser;
	_commandMap["CAP"]      = &CommandHandler::handleCap;
	_commandMap["JOIN"]     = &CommandHandler::handleJoin;
	_commandMap["INVITE"]   = &CommandHandler::handleInvite;
	_commandMap["TOPIC"]    = &CommandHandler::handleTopic;
	_commandMap["MODE"]     = &CommandHandler::handleMode;
	_commandMap["PRIVMSG"]  = &CommandHandler::handlePrivmsg;
}

CommandHandler::CommandHandler(Server& server, ReplyHandler &rh) : _server(server), _replyHandler(rh)
{
	setupCommands();
}

void CommandHandler::handle(Client *cl)
{
	std::vector<std::string> &mssgs = cl->getReceivedMessages();

	while (!mssgs.empty())
	{
		// Get the whole line
		std::stringstream line;
		line << mssgs.front();
		std::vector<std::string> cpy(mssgs.begin() + 1, mssgs.end());
		mssgs = cpy;

		// Extract single command
		std::string command;
		std::getline(line, command, ' ');

		// Find the command in the map
		std::map<std::string, CommandFn>::iterator it = _commandMap.find(command);
		if (it != _commandMap.end())
			(this->*(it->second))(cl, line);
		else
			_replyHandler.unknownCommand(cl, command);
	}
}

void CommandHandler::handlePass(Client *client, std::stringstream &command)
{
	std::string word;

	std::getline(command, word, ' ');
	if (word != _server._password)
		_replyHandler.passwdMismatch(client);
	else
		client->setPassword(word);
}

void CommandHandler::handleUser(Client *client, std::stringstream& command)
{
	std::string word;
	
	// Username
	std::getline(command, word, ' ');
	client->setUsername(word);

	// Mode, Asterix
	std::getline(command, word, ' ');
	std::getline(command, word, ' ');

	// Real name
	std::getline(command, word, ' ');
	client->setRealname(word);
}

void CommandHandler::handleNick(Client *client, std::stringstream& command)
{
	std::string word;
   
	std::getline(command, word, ' ');
	client->setNickname(word);
	_server._clientsByName.insert(std::pair<std::string, Client *>(word, client));
}

void CommandHandler::handleCap(Client *client, std::stringstream& command)
{
	std::string word;
	
	std::getline(command, word, ' ');
	if (word == "LS")
		client->receiveMsg(":server CAP * LS :\r\n"); // No capabilities
	else // Do not support any other than LS
		_replyHandler.unknownCommand(client, word);
}

// JOIN <channels> [<keys>]
void CommandHandler::handleJoin(Client *client, std::stringstream &command)
{
	std::string channel;
	std::getline(command, channel, ' ');

	// ! The keys to channels are not yet implemented
	std::stringstream channelList;
	channelList << channel;
	while (!channelList.eof())
	{
		std::getline(channelList, channel, ',');
		if (channel[0] != '#' || channel.size() < 2)
		{
			_replyHandler.noSuchChannel(client, channel);
			continue;
		}

		Channel *ch = _server._channelsByName.find(channel);
		if (!ch)
			ch = _server.createChannel(client, channel);
		else
		{
			if ((ch->getModes() & Channel::E_INVITE_ONLY) && !client->isInvitedTo(ch))
				return _replyHandler.inviteOnlyChannel(client, channel);
			if ((ch->getModes() & Channel::E_USER_LIMIT) && ch->getMembers().size() >= ch->getUserLimit())
				return _replyHandler.channelIsFull(client, channel);

			// ! IMPLEMENT KEY
			//if (ch->getKey() != key)
			//	return _replyHandler.badChannelKey(client, channel);
		}
		
		std::string msg = 
			":" + client->getFullUserPrefix() + " JOIN " + ":" + channel;
		ch->broadcast(msg);
		ch->addMember(client);
		_replyHandler.nameReply(client, channel, "NAMES"); // !
		_replyHandler.endOfNames(client, channel);
	}
}

void CommandHandler::handlePrivmsg(Client *client, std::stringstream &command)
{
}

// KICK <channel> <client> :[<message>]
void CommandHandler::handleKick(Client *client, std::stringstream &command)
{
	std::string channel, member, message;
	std::getline(command, channel, ' ');
	std::getline(command, member, ' ');
	std::getline(command, message, ' ');

	if (channel[0] != '#' || channel.size() < 2)
		return ;
	Channel * ch = _server._channelsByName.find(channel);
	if (!ch)
		return _replyHandler.noSuchChannel(client, channel);

	if (!ch->hasMember(client))
		return _replyHandler.notOnChannel(client, channel);
	if (!ch->hasOperator(client))
		return _replyHandler.chanOpPrivsNeeded(client, channel);
		
	Client *target = ch->hasMember(member);
	if (!target)
		return _replyHandler.userNotInChannel(client, member, channel);

	// :<source> KICK <channel> <target> :<reason>
	std::string msg = 
		":" + client->getNickname() + "!" + client->getUsername() + "@host" +
		" KICK " + channel + " " + member + " :" + message + "\r\n";

	ch->broadcast(msg);
	ch->removeMember(target);
}

// INVITE <nickname> <channel>
void CommandHandler::handleInvite(Client *client, std::stringstream &command)
{
	std::string nickname, channel;

	std::getline(command, nickname, ' ');
	std::getline(command, channel, ' ');

	if (channel[0] != '#' || channel.size() < 2)
		return ;

	Channel * ch = _server._channelsByName.find(channel);
	if (!ch)
		return _replyHandler.noSuchChannel(client, channel);

	if (!ch->hasMember(client))
		return _replyHandler.notOnChannel(client, channel);
	if ((ch->getModes() & Channel::E_INVITE_ONLY) && !ch->hasOperator(client))
		return _replyHandler.chanOpPrivsNeeded(client, channel);

	Client *invitee = _server._clientsByName.find(nickname);
	if (!invitee)
		return _replyHandler.noSuchNick(client, nickname);

	_replyHandler.inviting(client, invitee, channel);
	invitee->getsInvitedTo(ch);
}

void CommandHandler::handleTopic(Client *client, std::stringstream &command)
{

}


/*
	401 ERR_NOSUCHNICK 			- user does not exist						+
	403 ERR_NOSUCHCHANNEL 		- channel does not exist					+
	442 ERR_NOTONCHANNEL 		- you're not on that channel				+
	482 ERR_CHANOPRIVSNEEDED 	- change mode with no operator privilege	+
	461 ERR_NEEDMOREPARAMS		- missing required params
	472 ERR_UNKNOWNMODE			- unknown mode character(channel)			+
	501 ERR_UMODEUNKNOWNFLAG	- unknown user mode flag(user)
	502 ERR_USERSDONTMATCH		- trying to change other users flag	
	441 ERR_USERNOTINCHANNEL	- for modes like +o
	467 ERR_KEYSET				- setting +k when already set
*/
void CommandHandler::handleMode(Client *client, std::stringstream &command)
{
	std::string first, flags, param;

	std::getline(command, first, ' ');
	std::getline(command, flags, ' ');

	// Setting modes for a channel
	if (first[0] == '#')
	{
		Channel *ch = _server._channelsByName.find(first);
		if (!ch)
			return _replyHandler.noSuchChannel(client, first);

		if (!ch->hasMember(client))
			return _replyHandler.notOnChannel(client, first);
			
		if (!flags.empty())
		{
			if (!ch->hasOperator(client))
				return _replyHandler.chanOpPrivsNeeded(client, first);

			if (flags[0] != '+' && flags[0] != '-')
				return _replyHandler.needMoreParams(client, "MODE");

			for (size_t i = 1; i < flags.size(); ++i)
			{
				switch (flags[i])
				{
					case 'i':
					{	
						if (flags[0] == '+') ch->makeInviteOnly();
						else ch->removeMode(Channel::E_INVITE_ONLY);
						break;
					}
					case 't':
					{
						if (flags[0] == '+') ch->makeTopicRestricted();
						else ch->removeMode(Channel::E_TOPIC_RESTRICT);
						break;
					}
					case 'k':
					{
						if (ch->getModes() & Channel::E_CHANNEL_KEY)
						{
							_replyHandler.keySet(client, ch->getName()); 
							break;
						}
						std::getline(command, param, ' ');
						if (flags[0] == '+') ch->makeKey(param);
						else ch->removeMode(Channel::E_CHANNEL_KEY);
						break;
					}
					case 'l':
					{
						std::getline(command, param, ' ');
						if (flags[0] == '+') ch->makeUserLimit(std::atoi(param.c_str()));
						else ch->removeMode(Channel::E_USER_LIMIT);
						break;
					}
					case 'o':
					{
						std::getline(command, param, ' ');
						Client *target = clientLooksFor(client, param, ch);
						if (target)
							ch->addOperator(target);
						break;
					}
					default:
						_replyHandler.unknownMode(client, flags[i]);
				}
			}
		}
		else
			_replyHandler.channelModeIs(client, first, ch->getIrcModes());
	}
	else
	{
		Client *target = clientLooksFor(client, first);
		if (!target)
			return ;
	}
}

Client *CommandHandler::clientLooksFor(Client *client, std::string const &nick) const
{
    return clientLooksFor(client, nick, NULL);
}

Client *CommandHandler::clientLooksFor(Client *client, std::string const &nick, Channel *ch) const
{
	Client * target = _server._clientsByName.find(nick);
	if (!target)
	{
		_replyHandler.noSuchNick(client, nick);
		return NULL;
	}
	
	if (ch && !ch->hasMember(target))
	{
		_replyHandler.notOnChannel(client, ch->getName());
		return NULL;
	}
    return target;
}
