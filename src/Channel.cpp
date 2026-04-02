#include "Channel.hpp"
#include <algorithm>

Channel::Channel(std::string const &name) : _name(name), _modes(0) { }
Channel::Channel(Client *creator, std::string const &name) : _name(name), _modes(0) { addOperator(creator); }

// Getters
std::string const&					Channel::getName() 		const { return _name; }
std::set<Client *> const&			Channel::getMembers() 	const { return _members; }
std::set<Client *> const&			Channel::getOperators() const { return _operators; }
std::vector<std::string> const&		Channel::getMessages() 	const { return _messages; }
unsigned int 						Channel::getModes() 	const { return _modes; }

std::pair<Client *, bool> Channel::hasMember(std::string const& nick) const
{
	std::set<Client *>::iterator it = std::find_if(_members.begin(), _members.end(), Client::NickEquals(nick));
    return it != _members.end() ?
		std::pair<Client *, bool>(*it, true) 
		: std::pair<Client *, bool>(NULL, false);
}

bool Channel::hasMember(Client *cl) const
{
    return _members.find(cl) != _members.end();
}

std::pair<Client *, bool> Channel::hasOperator(std::string const &nick) const
{
	std::set<Client *>::iterator it = std::find_if(_operators.begin(), _operators.end(), Client::NickEquals(nick));
    return it != _operators.end() ?
		std::pair<Client *, bool>(*it, true) 
		: std::pair<Client *, bool>(NULL, false);
}

bool Channel::hasOperator(Client *cl) const
{
    return _operators.find(cl) != _operators.end();
}

// Modifiers
void Channel::setModes(unsigned int modes) 		{ _modes = modes; }
void Channel::addModes(unsigned int mode) 		{ _modes &= mode; }
void Channel::removeModes(unsigned int mode) 	{ _modes &= ~mode; }

void Channel::addMember(Client *user) { _members.insert(user); }
void Channel::addOperator(Client *user)
{
	if (!hasMember(user))
		addMember(user);
	_operators.insert(user);
}
