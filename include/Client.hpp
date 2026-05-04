#pragma once

#include <string>
#include <vector>
#include <poll.h>
#include "ServerNotifyCodes.hpp"
#include <queue>

struct ClientState
{
	bool pass_ok;
	bool has_nick;
	bool has_user;
	bool cap_negotiating;
	bool was_welcomed;
    bool pendingDisconnect;

	ClientState();
};

class Channel;

class Client {
private:
	int                             _fd;
	std::string                     _realname;
	std::string                     _nickname;
	std::string                     _username;
	std::string                     _address;
	std::string                     _buffer;
	ClientState                     _state;
	std::queue<std::string>         _outMsg;
	std::queue<std::string>         _inMsg;
	std::vector<Channel *>          _invitedTo;
	struct pollfd					_pollInfo;
	
	Client();

public:
	struct NickEquals {
		explicit NickEquals(std::string const& target) : _target(target) { };
		bool operator()(Client const* cl) const { return cl->getNickname() == _target; }
	public:
		std::string _target;
	};

	// Constructors
	explicit Client(int fd);
	Client(const Client &orig);
	Client&operator=(const Client &orig);
	~Client();

	bool operator==(Client const& other) const;

	// Getters
	std::queue<std::string>&		getOutMssgs()			     ; //
	std::queue<std::string>&		getInMssgs(void)		  	 ; //
	std::string&					getRecvBuffer()				 ; //ask roman why changed, need to be modifiable
	const std::string&              getNickname()			const;
	std::string                     getIrcNickname()		const;
	std::string                     getFullUserPrefix()		const;
	const std::string&              getUsername()			const;
	const std::string&              getRealname()			const;
	int                             getFd()					const;
	bool                            isRegistered()			const;
	bool                            wasWelcomed()			const;
	bool                            hasNickname()			const;
	bool                            isCapNegotiating()		const;
	bool                            hasPassword()			const;

	void clearInMssgs();
	void clearOutMssgs(void);
	void addInMsg(std::string remainder);

	bool isInvitedTo(Channel *ch) const;
	void addtoBuffer(std::string msg);
	
	bool isPendingDisconnect();

	// Modifiers
	void setNickname(std::string const& nickname);
	void setPassword(std::string const& password);
	void setUsername(std::string const& username);
	void setRealname(std::string const& username);
	void setIsCapNegotiating(bool flag);
	void setWasWelcomed(bool flag);
	void getsInvitedTo(Channel *ch);
	void putIntoRecvBuffer(std::string const& data);
	void setPendingDisconnect(bool status);
	void addClientPollInfo(struct pollfd fd);
	void setReceiving(bool mode);
	
	void receiveMsg(std::string const& msg);

	friend class Tester;
} ;