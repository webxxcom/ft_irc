#pragma once

#include <vector>
#include <map>
#include <string>
#include <sys/poll.h>
#include <iostream>

class TransferSession;
class Client;
class Channel;
class FileSendHandler;

class ServerState {
public:
	ServerState();
	~ServerState();

	void pollfdAdd(struct pollfd fd);
	void pollfdRemove(int fd);
	struct pollfd& pollfdFindByFd(int fd);

	Channel *createChannel(Client *cl, std::string const& name);
	Channel *channelFindByName(std::string const& name) const;
	void deleteChannel(Channel *ch);

	void addTransferSession(TransferSession *ts);
	void removeTransferSession(TransferSession *ts);
	void transferSessionReplaceKey(int oldfd, int newfd, pollfd newPollfd);

	Client*	clientFindByFd(int fd) const;
	Client*	clientFindByNickname(std::string const& name) const;
	void clientChangeName(Client *cl, std::string const& newName);
	void addClient(Client *cl);
	void removeClient(Client *cl);

	int							getPort() const;
	std::string const&			getPassword() const;
	int							getServerSockerFd() const;
	std::vector<struct pollfd>& getPollFds();

	void 						setPort(int port);
	void						setPassword(std::string const& password);
	void						setServerSockerFd(int fd);

	TransferSession *transferSessionFindByToken(std::string const& token) const;
	TransferSession *transferSessionFindByFd(int fd) const;
	bool			isTransferFd(int fd) const;

private:
	struct CompareByFd
	{   
		int _fd;
		CompareByFd(int fd) : _fd(fd) {}
		bool operator()(pollfd const& o) { return (o.fd == _fd); }
	};
	
	
	// State
	std::vector<struct pollfd>              	_pollfds;
	int           				            	_serverSocketfd;
	std::string     			            	_password;
	int											_port;

	// Channels storage
	std::vector<Channel *>						_channels; // the pointers owner
	std::map<std::string, Channel *>			_channelsByName;

	// Clients storage
	std::vector<Client *>						_clients; // the pointers owner
	std::map<int, Client *>						_clientsByFd;
	std::map<std::string, Client *>				_clientsByName;

	// TransferSession
	std::vector<TransferSession *>				_transferSession; // the pointers owner

	// TEST
	friend class Tester;
};
