#pragma once

#include <vector>
#include <map>
#include <string>
#include <sys/poll.h>
#include <iostream>
#include <set>

struct TransferSession;
class Client;
class Channel;
class FileSendHandler;

class ServerState {
public:
	ServerState();
	~ServerState();

	int				poll();
	void			pollfdAdd(struct pollfd fd);
	void 			pollfdRemove(int fd);
	bool			pollfdFindByFd(int fd, pollfd &out) const;

	Channel*		createChannel(Client *cl, std::string const& name);
	Channel*		channelFindByName(std::string const& name) const;
	void			removeChannel(Channel *ch);

	void addTransferSession(TransferSession *ts);
	void removeTransferSession(TransferSession *ts);

	std::set<Client *>	getUsersClientKnows(Client *cl)								const;
	Client*				clientFindByFd(int fd)										const;
	Client*				clientFindByNickname(std::string const& name)				const;
	void				clientChangesName(Client *cl, std::string const& newName)	const;
	void				clientIsReadyToReceiveMessage(Client const* cl)					const;
	void				clientDisconnects(Client *cl)								const;
	void				addClient(Client *cl);
	void				removeClient(Client *cl);
	void				removeClientFromAllChannels(Client *cl);

	int							getPort() 			const;
	int							getServerSocketFd()	const;
	std::string const&			getPassword()		const;
	std::vector<pollfd> const&	getPollFds()		const;

	void 						setPort(int port);
	void						setPassword(std::string const& password);
	void						setServerSockerFd(int fd);

	TransferSession *transferSessionFindByToken(std::string const& token) const;
	TransferSession *transferSessionFindByFd(int fd) const;
	bool			isTransferFd(int fd) const;

private:
	// State
	std::vector<struct pollfd>              	_pollfds;
	int           				            	_serverSocketfd;
	std::string     			            	_password;
	int											_port;

	// Registry
	std::vector<Channel *>						_channels;
	std::vector<Client *>						_clients;
	std::vector<TransferSession *>				_transferSession;

	// TEST
	friend class Tester;
};
