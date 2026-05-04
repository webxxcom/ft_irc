#include "Channel.hpp"
#include <algorithm>
#include <limits>
#include <sstream>
#include <ctime>

Channel::Channel(std::string const &name) : _name(name) { }
Channel::Channel(Client *creator, std::string const &name) : _name(name) { addOperator(creator); }

// Getters
std::string const&					Channel::getName() 					const 	{ return _name; }
std::set<Client *> const&			Channel::getMembers() 				const 	{ return _members; }
std::set<Client *> const&			Channel::getOperators() 			const 	{ return _operators; }
std::vector<std::string> const&		Channel::getMessages() 				const 	{ return _messages; }
unsigned int 						Channel::getModes() 				const 	{ return _modes._modes; }
const ChannelTopic&					Channel::getTopic()					const 	{ return _topic; }
size_t								Channel::getUserLimit()				const 	{ return _modes._userLimit; }
std::string const&					Channel::getKey() 					const 	{ return _modes._key; }
bool								Channel::isInviteOnly()				const 	{ return _modes._modes & E_INVITE_ONLY; }
bool 								Channel::isTopicRestricted()		const 	{ return _modes._modes & E_TOPIC_RESTRICT; }
bool 								Channel::hasUserLimit() 			const 	{ return _modes._modes & E_USER_LIMIT; }
bool 								Channel::hasKey() 					const 	{ return _modes._modes & E_CHANNEL_KEY; }
bool 								Channel::isFull() 					const 	{ return hasUserLimit() && _members.size() >= _modes._userLimit; }
bool								Channel::isEmpty() 					const 	{ return _members.empty(); }
bool 								Channel::hasMember(Client *cl) 		const 	{ return _members.find(cl) != _members.end(); }
bool 								Channel::hasOperator(Client *cl) 	const	{ return _operators.find(cl) != _operators.end(); }

std::string Channel::getIrcModes() const
{
    std::string flags;
	std::stringstream params;

	if (_modes._modes == 0)
		return "";
	flags.push_back('+');
	if (isInviteOnly())
		flags.push_back('i');
	if (isTopicRestricted())
		flags.push_back('t');
	if (hasKey())
	{
		flags.push_back('k');
		params << _modes._key;
	}
	if (hasUserLimit())
	{
		flags.push_back('l');
		if (!params.str().empty())
			params << " ";
		params << _modes._userLimit;
	}
	if (!params.str().empty())
		flags += " ";
	return flags + params.str();
}

Client *Channel::hasMember(std::string const& nick) const
{
	std::set<Client *>::iterator it = std::find_if(_members.begin(), _members.end(), Client::NickEquals(nick));
    return it != _members.end() ? *it : NULL;
}

Client *Channel::hasOperator(std::string const &nick) const
{
	std::set<Client *>::iterator it = std::find_if(_operators.begin(), _operators.end(), Client::NickEquals(nick));
    return it != _operators.end() ? *it : NULL;
}

// Modifiers
void Channel::removeMode(unsigned int mode)
{
	_modes._modes &= ~mode;

	if (mode == E_CHANNEL_KEY)
		_modes._key.clear();
	else if (mode == E_USER_LIMIT)
		_modes._userLimit = 0;
}

void Channel::addMember(Client *user) { _members.insert(user); }
void Channel::addOperator(Client *user)
{
	if (!hasMember(user))
		addMember(user);
	_operators.insert(user);
}

void Channel::removeMember(Client *cl)
{
	_members.erase(cl);
	_operators.erase(cl);
}

void Channel::removeOperator(Client *cl)
{
	_operators.erase(cl);
}

void Channel::makeInviteOnly() { _modes._modes |= E_INVITE_ONLY; }
void Channel::makeTopicRestricted() { _modes._modes |= E_TOPIC_RESTRICT; }

void Channel::makeUserLimit(size_t l)
{
	_modes._modes |= E_USER_LIMIT;
	_modes._userLimit = l;
}

void Channel::makeKey(std::string const &key)
{
	_modes._modes |= E_CHANNEL_KEY;
	_modes._key = key;
}

void Channel::broadcast(std::string const &msg, ServerState const& registry)
{
	for (std::set<Client *>::iterator it = _members.begin(); it != _members.end(); ++it)
		(*it)->receiveMsg(msg, registry);
}

void Channel::setTopic(std::string const& topic, Client* cl) {
	this->_topic._text = topic;
	this->_topic._setby = cl->getNickname();
	std::time_t currentTime = std::time(NULL);
	this->_topic._time = currentTime;
}
