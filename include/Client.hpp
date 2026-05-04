#pragma once

#include <string>
#include <vector>
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
	std::queue<std::string> const&	getOutMssgs()				const;
	std::queue<std::string> const&	getInMssgs(void)			const;
	std::string const&				getRecvBuffer()				const;
	std::string const&              getNickname()				const;
	std::string const&              getUsername()				const;
	std::string const&              getRealname()				const;
	std::string                     getIrcNickname()			const;
	std::string                     getFullUserPrefix()			const;
	int                             getFd()						const;
	bool                            isRegistered()				const;
	bool                            wasWelcomed()				const;
	bool                            hasNickname()				const;
	bool                            isCapNegotiating()			const;
	bool                            hasPassword()				const;
	bool							isInvitedTo(Channel *ch)	const;

	void clearInMssgs();
	void clearOutMssgs(void);
	void addInMsg(std::string remainder);

	void addtoBuffer(std::string msg);

	// Modifiers
	void setNickname(std::string const& nickname);
	void setPassword(std::string const& password);
	void setUsername(std::string const& username);
	void setRealname(std::string const& username);
	void setIsCapNegotiating(bool flag);
	void setWasWelcomed(bool flag);
	void setPendingDisconnect(bool flag);
	void getsInvitedTo(Channel *ch);

	void putIntoRecvBuffer(std::string const& data);
	void receiveMsg(std::string const& msg);
} ;