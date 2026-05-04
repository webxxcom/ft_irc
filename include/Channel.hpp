#pragma once

#include <string>
#include <vector>
#include <set>
#include "Client.hpp"

class Client;

struct ChannelTopic{
	std::string					_text;
	std::string					_setby;
	std::string					_time;
} ;

class Channel {
private:
	std::string                 _name;
	std::vector<std::string>    _messages;
	std::set<Client *>          _members;
	std::set<Client *>          _operators;
	ChannelTopic 				_topic;


	struct Mode{
		unsigned int            _modes;
		size_t               	_userLimit;
		std::string             _key;

		Mode() : _modes(0), _userLimit(0) { }
	} _modes;
		
public:
	enum ChannelModes {
		E_INVITE_ONLY = 0x1,
		E_TOPIC_RESTRICT = 0x2,
		E_CHANNEL_KEY = 0x4,
		E_USER_LIMIT = 0x8
	};

	class NameEquals{
		std::string _target;
	public:
		explicit NameEquals(std::string const& name) : _target(name) { }
		bool operator()(Channel const* ch) const;
	};

	explicit Channel(std::string const& name);
	Channel(Client *creator, std::string const& name);

	// Getters
	std::string const&				getName() 			const;
	std::set<Client *> const&		getMembers() 		const;
	std::set<Client *> const&		getOperators() 		const;
	std::vector<std::string> const&	getMessages() 		const;
	unsigned int					getModes() 			const;
	std::string						getIrcModes() 		const;
	size_t							getUserLimit() 		const;
	bool							isInviteOnly()		const;
	std::string const&				getKey()			const;
	const ChannelTopic&				getTopic()			const;
	bool							isTopicRestricted()	const;
	bool							isEmpty()			const;
	bool							hasUserLimit()		const;
	bool							hasKey()			const;
	bool							isFull()		const;

	Client *   						hasMember(std::string const& nick)    const;
	bool                        	hasMember(Client *cl)                 const;
	Client *   						hasOperator(std::string const& nick)  const;
	bool                        	hasOperator(Client *cl)               const;

	// Modifiers
	
	void addMember(Client *cl);
	void addOperator(Client *cl);
	void removeMember(Client *cl);
	void removeOperator(Client *cl);
	void makeInviteOnly();
	void makeTopicRestricted();
	void makeUserLimit(size_t l);
	void makeKey(std::string const& key);
    void removeMode(unsigned int mode);

    void setTopic(std::string const& topic, Client* cl);

    void broadcast(std::string const &msg, ServerState const &registry, Client *cl = NULL);
};
