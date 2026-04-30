#include "CommandHandler.hpp"
#include "ServerNotifyCodes.hpp"
#include "ServerState.hpp"
#include "FileSendHandler.hpp"
#include "Client.hpp"
#include "Channel.hpp"
#include "ReplyHandler.hpp"

#include <cstdlib>
#include <iostream>
#include <cstdio>
#include <sys/stat.h>

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
	_commandMap["PING"]  	= &CommandHandler::handlePing;
	_commandMap["QUIT"]  	= &CommandHandler::handleQuit;
	_commandMap["PART"]  	= &CommandHandler::handlePart;
	_commandMap["FILE"]  	= &CommandHandler::handleFile;
}

CommandHandler::CommandHandler(ServerState& server, ReplyHandler &rh, FileSendHandler &fsh)
	: _registry(server), _replyHandler(rh), _fileSendHandler(fsh)
{
	setupCommands();
}

void CommandHandler::handle(Client *cl)
{
	std::queue<std::string> mssgs = cl->getOutMssgs();
	cl->clearOutMssgs();

	while (!mssgs.empty())
	{
		// Get the whole line
		std::stringstream line;
		line << mssgs.front();
		mssgs.pop();

		// Extract single command
		std::string command;
		line >> command;

		if (command.empty())
			continue;

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
	std::string token;

	command >> token;
	if (token != _registry.getPassword())
		_replyHandler.passwdMismatch(client);
	else
		client->setPassword(token);
}

void CommandHandler::handleUser(Client *client, std::stringstream& command)
{
	std::string token;
	
	// Username
	command >> token;
	client->setUsername(token);

	// Mode, Asterix
	command >> token;
	command >> token;

	// Real name
	std::getline(command, token);
	client->setRealname(token);
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
   
	if (!(command >> nick))
		return _replyHandler.noNickSupplied(client);
	if (!isValidNick(nick))
		return _replyHandler.erroneusNick(client, nick);

	if (_registry.clientFindByNickname(nick))
		_replyHandler.nicknameAlreadyInUse(client, nick);
	else
		_registry.clientChangeName(client, nick);
}

void CommandHandler::handleCap(Client *client, std::stringstream& command)
{
	std::string word;
	
	command >> word;
	if (word == "LS")
		client->receiveMsg(":server CAP * LS :\r\n"); // No capabilities
	else if (word == "REQ")
		client->setIsCapNegotiating(true);
	else if (word == "END")
		client->setIsCapNegotiating(false);
	else
		_replyHandler.unknownCommand(client, word);
}

// JOIN <channels> [<keys>]
void CommandHandler::handleJoin(Client *client, std::stringstream &command)
{
	if (!client->isRegistered())
		return _replyHandler.notRegistered(client);

	std::string channelName, key;
	command >> channelName >> key;

	if (channelName.empty())
		return _replyHandler.needMoreParams(client, "JOIN");
		
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

		Channel *ch = _registry.channelFindByName(channelName);
		if (!ch)
			ch = _registry.createChannel(client, channelName);
		else
		{
			if (ch->isInviteOnly() && !client->isInvitedTo(ch))
			{
				_replyHandler.inviteOnlyChannel(client, channelName);
				continue;
			}
			else if (ch->isFull())
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
			":" + client->getFullUserPrefix() + " JOIN " + channelName + "\r\n";
		ch->addMember(client);
		ch->broadcast(msg);
		_replyHandler.nameReply(client, ch);
		_replyHandler.endOfNames(client, channelName);
	}
}

//PRIVMSG <target> :<message>
void CommandHandler::handlePrivmsg(Client *client, std::stringstream &command)
{
	if (!client->isRegistered())
		return _replyHandler.notRegistered(client);

	std::string target, message;

	command >> target;
	std::getline(command, message);

	if (message.empty())
		return _replyHandler.noTextToSend(client);

	if (target.empty())
		return _replyHandler.noRecipient(client, "PRIVMSG");

	if (message[0] == ' ')
		message.erase(0, 1);

	if (target[0] == '#')
	{
		Channel *ch = _registry.channelFindByName(target);
		if (!ch)
			return _replyHandler.noSuchChannel(client, target);
		if (!ch->hasMember(client))
			return _replyHandler.notOnChannel(client, ch->getName());
		ch->broadcast(client->getFullUserPrefix() + " PRIVMSG " + target + " " + message + "\r\n");
	}
	else
	{
		Client *cl = _registry.clientFindByNickname(target);
		if (!cl)
			return _replyHandler.noSuchNick(client, target);
		client->receiveMsg(client->getFullUserPrefix() + " PRIVMSG " + target + " " + message + "\r\n");
	}
}

// KICK <channel> <client> :[<message>]
void CommandHandler::handleKick(Client *client, std::stringstream &command)
{
	if (!client->isRegistered())
		return _replyHandler.notRegistered(client);

	std::string channel, member, message;
	command >> channel >> member >> message;

	if (channel[0] != '#' || channel.size() < 2)
		return ;

	Channel * ch = _registry.channelFindByName(channel);
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
	command >> nickname >> channelName;

	if (channelName[0] != '#' || channelName.size() < 2)
		return ;

	Channel * ch = _registry.channelFindByName(channelName);
	if (!ch)
		return _replyHandler.noSuchChannel(client, channelName);

	if (!ch->hasMember(client))
		return _replyHandler.notOnChannel(client, channelName);
	if ((ch->getModes() & Channel::E_INVITE_ONLY) && !ch->hasOperator(client))
		return _replyHandler.chanOpPrivsNeeded(client, channelName);

	Client *invitee = _registry.clientFindByNickname(nickname);
	if (!invitee)
		return _replyHandler.noSuchNick(client, nickname);

	if (ch->hasMember(invitee))
		return _replyHandler.alreadyOnChannel(client, nickname, channelName);
	_replyHandler.inviting(client, invitee, channelName);
	invitee->getsInvitedTo(ch);
}

void CommandHandler::handleTopic(Client *client, std::stringstream &command)
{
	std::string channelName, newTopic;
	if (command.peek() == EOF)
		return _replyHandler.needMoreParams(client, "TOPIC");
	std::getline(command, channelName, ' ');
	if (channelName[0] != '#')
		return _replyHandler.needMoreParams(client, "TOPIC");

	Channel *ch = _registry.channelFindByName(channelName);
	if (!ch)
		return _replyHandler.noSuchChannel(client, channelName);
	if (!ch->hasMember(client))
		return _replyHandler.notOnChannel(client, channelName);

	std::getline(command, newTopic);
	if (!newTopic.empty() && newTopic[newTopic.length() - 1] == '\r')
		newTopic.erase(newTopic.length() - 1);
	if (!newTopic.empty() && newTopic[0] == ' ') {
        size_t pos = newTopic.find_first_not_of(" ");
		newTopic.erase(0, pos);
	}

	const ChannelTopic& currentTopic = ch->getTopic();
	if (newTopic.empty()) {
		if (currentTopic._text.empty())
			return _replyHandler.topicEmpty(client, channelName);
		_replyHandler.currentTopic(client, channelName, currentTopic._text);
		return _replyHandler.currentTopicInfo(client, channelName, currentTopic);
	}
	else if (newTopic[0] == ':') {
		if (ch->isTopicRestricted() && !ch->hasOperator(client))
			return _replyHandler.chanOpPrivsNeeded(client, channelName);
		newTopic.erase(0,1);

		ch->setTopic(newTopic, client);
		std::string msg;
		msg = ":" + client->getFullUserPrefix() + " TOPIC " + channelName + " :" + newTopic + "\r\n"; 
		ch->broadcast(msg);
	}
	else { //maybe irc allows wihtout : if topic is set to one word
		
	}
}

void CommandHandler::handleMode(Client *client, std::stringstream &command)
{
	if (!client->isRegistered())
		return _replyHandler.notRegistered(client);

	std::string first, flags, param;
	command >> first >> flags;

	// Setting modes for a channel
	if (first[0] == '#')
	{
		Channel *ch = _registry.channelFindByName(first);
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
						command >> param;
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
						command >> param;
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
						command >> param;
						if (!param.empty())
						{
							Client *target = clientLooksForUserInChannel(client, param, ch);
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
}

void CommandHandler::handlePing(Client *client, std::stringstream &command)
{
	std::string token;

	command >> token;
	_replyHandler.pong(client, token);
}

void CommandHandler::handleQuit(Client *client, std::stringstream &command)
{
	client->getFd();
	(void)command;
}

//PART <channel>{,<channel>} [<reason>]
void CommandHandler::handlePart(Client *client, std::stringstream &command)
{
	if (!client->isRegistered())
		return _replyHandler.notRegistered(client);

	std::string channels, reason;
	if (!(command >> channels))
		return _replyHandler.needMoreParams(client, "PART");
	std::getline(command, reason);
	if (!reason.empty() && reason[0] == ' ')
		reason.erase(0, 1);

	std::stringstream channelsStream(channels);
	std::string chanName;
	while (std::getline(channelsStream, chanName, ','))
	{
		Channel *ch = _registry.channelFindByName(chanName);
		if (!ch)
		{
			_replyHandler.noSuchChannel(client, chanName);
			continue;
		}
		if (!ch->hasMember(client))
		{
			_replyHandler.notOnChannel(client, ch->getName());
			continue;
		}
		std::string msg = ":" + client->getFullUserPrefix() + " PART " + chanName + (reason.empty() ? "" : (" " + reason)) + "\r\n";
		ch->broadcast(msg);
		ch->removeMember(client);
		if (ch->isEmpty())
			_registry.deleteChannel(ch);
	}
}

bool fileExists(const std::string& path)
{
    struct stat st;
    return ::stat(path.c_str(), &st) == 0;
}

//FILE SN <receiver nickname> <filename>
//:<full prefix> FILE REQ <token> <filename> <filesize> <port>
//FILE ACC <token>
//FILE REJ <token>
void CommandHandler::handleFile(Client *client, std::stringstream &command)
{
	std::string cmd;
	command >> cmd;

	if (cmd == "SN")
	{
		std::string nick, filename;
		command >> nick >> filename;

		if (nick.empty() || filename.empty())
			return _replyHandler.needMoreParams(client, "SEND");
		
		Client *cl = _registry.clientFindByNickname(nick);
		if (!cl)
			return _replyHandler.noSuchNick(client, nick);
		if (!fileExists(filename))
			return _replyHandler.fileIsAbsent(client, filename);
		return _fileSendHandler.request(client, cl, filename);
	}

	std::string token;
	command >> token;
	try
	{
		TransferSession *ts = _registry.transferSessionFindByToken(token);
		if (cmd == "REQ")
		{
			// Do nothing client must reply
		}
		else if (cmd == "ACC")
			_fileSendHandler.accept(client, ts);
		else if (cmd == "REJ")
			_fileSendHandler.reject(client, ts);
		else
			return _replyHandler.unknownCommand(client, token);
	}
	catch(const std::out_of_range& e)
	{
		return _replyHandler.badFileSessionToken(client, token);
	}
}

Client *CommandHandler::clientLooksForUserInChannel(Client *client, std::string const &nick, Channel *ch) const
{
	Client * target = _registry.clientFindByNickname(nick);
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
