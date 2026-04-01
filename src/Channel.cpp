/* --- Channel.cpp --- */

/* ------------------------------------------
author: Webxxcom
date: 3/31/2026
------------------------------------------ */

#include "Channel.hpp"

Channel::Channel(std::string const &name) : _name(name), _modes(0) { }

Channel::Channel(Client &creator, std::string const &name) : _name(name), _modes(0)
{
	addOperator(creator);
}

// Getters
std::string const&					Channel::getName() 		const { return _name; }
std::set<Client *> const&			Channel::getMembers() 	const { return _members; }
std::set<Client *> const&			Channel::getOperators() const { return _operators; }
std::vector<std::string> const&		Channel::getMessages() 	const { return _messages; }
unsigned int 						Channel::getModes() 	const { return _modes; }

// Modifiers
void Channel::setModes(unsigned int modes) 		{ _modes = modes; }
void Channel::addModes(unsigned int mode) 		{ _modes &= mode; }
void Channel::removeModes(unsigned int mode) 	{ _modes &= ~mode; }

void Channel::addMember(Client &user)
{
	_members.insert(std::pair<std::string, Client *>(user.getNickname(), &user));
}

void Channel::addOperator(Client &user)
{
	if (!user.isMemberOf(*this))
		addMember(user);
	_operators.insert(std::pair<std::string, Client *>(user.getNickname(), &user));
}
