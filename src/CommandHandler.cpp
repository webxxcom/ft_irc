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

bool isValidNick(const std::string& nick)
{
    if (nick.empty() || nick.length() > 9)
        return false;

    char first = nick[0];
    if (!(std::isalpha(first) ||
          first == '[' || first == ']' || first == '\\' ||
          first == '`' || first == '^' || first == '{' || first == '}'
         ))
    {
        return false;
    }

    for (size_t i = 1; i < nick.length(); ++i)
    {
        char c = nick[i];
        if (!(std::isalnum(c) ||
              c == '-' ||
              c == '[' || c == ']' || c == '\\' ||
              c == '`' || c == '^' || c == '{' || c == '}'
             ))
        {
            return false;
        }
    }

    return true;
}

void CommandHandler::handleNick(Client *client, std::stringstream& command)
{
	std::string nick;
   
	std::getline(command, nick, ' ');
	if (!isValidNick(nick))
		_replyHandler.erroneusNick(client, nick);
	else if (_server._clientsByName.find(nick) == NULL)
	{
		client->setNickname(nick);
		_server._clientsByName.insert(std::pair<std::string, Client *>(nick, client));
	}
	else
		_replyHandler.nicknameAlreadyInUse(client, nick);
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
	if (!client->isRegistered())
		return _replyHandler.notRegistered(client);

	std::string channelName, key;
	std::getline(command, channelName, ' ');
	std::getline(command, key, ' ');

	std::stringstream channelList;
	channelList << channelName;
	std::stringstream keyList;
	keyList << key;
	while (!channelList.eof())
	{
		std::getline(channelList, channelName, ',');
		if (channelName[0] != '#' || channelName.size() < 2)
		{
			_replyHandler.badChannelMask(client, channelName);
			continue;
		}

		Channel *ch = _server._channelsByName.find(channelName);
		if (!ch)
			ch = _server.createChannel(client, channelName);
		else
		{
			if ((ch->getModes() & Channel::E_INVITE_ONLY) && !client->isInvitedTo(ch))
			{
				_replyHandler.inviteOnlyChannel(client, channelName);
				continue;
			}
			else if ((ch->getModes() & Channel::E_USER_LIMIT) && ch->getMembers().size() >= ch->getUserLimit())
			{
				_replyHandler.channelIsFull(client, channelName);
				continue;
			}
			else
			{
				std::getline(keyList, key, ',');
				if (ch->getKey() != key)
				{
					_replyHandler.badChannelKey(client, channelName);
					continue;
				}
			}
		}
		std::string msg = 
			":" + client->getFullUserPrefix() + " JOIN " + channelName;
		ch->addMember(client);
		ch->broadcast(msg);
		_replyHandler.nameReply(client, ch);
		_replyHandler.endOfNames(client, channelName);
	}
}

void CommandHandler::handlePrivmsg(Client *client, std::stringstream &command)
{
}

// KICK <channel> <client> :[<message>]
void CommandHandler::handleKick(Client *client, std::stringstream &command)
{
	if (!client->isRegistered())
		return _replyHandler.notRegistered(client);

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
	if (!client->isRegistered())
		return _replyHandler.notRegistered(client);

	std::string nickname, channelName;

	std::getline(command, nickname, ' ');
	std::getline(command, channelName, ' ');

	if (channelName[0] != '#' || channelName.size() < 2)
		return ;

	Channel * ch = _server._channelsByName.find(channelName);
	if (!ch)
		return _replyHandler.noSuchChannel(client, channelName);

	if (!ch->hasMember(client))
		return _replyHandler.notOnChannel(client, channelName);
	if ((ch->getModes() & Channel::E_INVITE_ONLY) && !ch->hasOperator(client))
		return _replyHandler.chanOpPrivsNeeded(client, channelName);

	Client *invitee = _server._clientsByName.find(nickname);
	if (!invitee)
		return _replyHandler.noSuchNick(client, nickname);

	if (ch->hasMember(invitee))
		return _replyHandler.alreadyOnChannel(client, nickname, channelName);
	_replyHandler.inviting(client, invitee, channelName);
	invitee->getsInvitedTo(ch);
}

void CommandHandler::handleTopic(Client *client, std::stringstream &command)
{
	std::string channelName;
	if (command.peek() == EOF)
		//invalid request (use 461 ERR_NEEDMOREPARAMS)
		//:your.server.name 461 <client_nickname> TOPIC :Not enough parameters
	std::getline(command, channelName, ' ');
	if (channelName[0] != '#') {
		//invalid request (use 461 ERR_NEEDMOREPARAMS) probably too
	}
	std::string topicMsg = command.str();
	Channel *ch = _server._channelsByName.find(channelName);
	if (!ch)
		return _replyHandler.noSuchChannel(client, channelName);
	if (!ch->hasMember(client))
		return _replyHandler.notOnChannel(client, channelName);
	//check the channel if topic existing
	if (topicMsg.empty()) {
		//just display topic
	}
	else //or set a new one
	{
		if (ch->isTopicRestricted()) {
			//check if client is an operator or 482 ERR_CHANOPRIVSNEEDED
			//:your.server.name 482 <client_nickname> <channel> :You're not channel operator
		}
	}

}

void CommandHandler::handleMode(Client *client, std::stringstream &command)
{
	if (!client->isRegistered())
		return _replyHandler.notRegistered(client);

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

			std::string replyFlags = std::string(1, flags[0]), replyParams;
			for (size_t i = 1; i < flags.size(); ++i)
			{
				switch (flags[i])
				{
					case 'i':
					{	
						if (flags[0] == '+') ch->makeInviteOnly();
						else ch->removeMode(Channel::E_INVITE_ONLY);
						replyFlags += flags[i];
						break;
					}
					case 't':
					{
						if (flags[0] == '+') ch->makeTopicRestricted();
						else ch->removeMode(Channel::E_TOPIC_RESTRICT);
						replyFlags += flags[i];
						break;
					}
					case 'k':
					{
						if ((ch->getModes() & Channel::E_CHANNEL_KEY) && flags[0] != '-')
						{
							_replyHandler.keySet(client, ch->getName()); 
							break;
						}
						std::getline(command, param, ' ');
						if (flags[0] == '+' && param.empty()) _replyHandler.needMoreParams(client, "MODE");
						else if (flags[0] == '+') ch->makeKey(param);
						else ch->removeMode(Channel::E_CHANNEL_KEY);

						replyFlags += flags[i];
						if (!replyParams.empty())
							replyParams += " ";
						replyParams += param;
						break;
					}
					case 'l':
					{
						std::getline(command, param, ' ');
						int limit = std::atoi(param.c_str());
						if ((limit == 0 && flags[0] == '+') || (flags[0] == '+' && param.empty())) _replyHandler.needMoreParams(client, "MODE");
						else if (flags[0] == '+' && !param.empty()) ch->makeUserLimit(limit);
						else ch->removeMode(Channel::E_USER_LIMIT);

						replyFlags += flags[i];
						if (!replyParams.empty())
							replyParams += " ";
						replyParams += param;
						break;
					}
					case 'o':
					{
						std::getline(command, param, ' ');
						if (!param.empty())
						{
							Client *target = clientLooksFor(client, param, ch);
							if (target)
							{
								if (flags[0] == '+') ch->addOperator(target);
								else ch->removeOperator(target);

								replyFlags += flags[i];
								if (!replyParams.empty())
									replyParams += " ";
								replyParams += param;
							}
						}
						else
							_replyHandler.needMoreParams(client, "MODE");
						break;
					}
					default:
						_replyHandler.unknownMode(client, first, flags[i]);
				}
			}
			std::string msg = ":" + client->getFullUserPrefix() + " MODE "
				+ first + " " + replyFlags;
			if (!replyParams.empty())
				msg += " " + replyParams;
			ch->broadcast(msg);
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
		_replyHandler.userNotInChannel(client, nick, ch->getName());
		return NULL;
	}
    return target;
}
