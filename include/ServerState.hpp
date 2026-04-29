#pragma once

#include <vector>
#include <map>
#include <string>

class TransferSession;
class Client;
class Channel;
class FileSendHandler;

class ServerState {
public:
	ServerState();
	~ServerState();

	Channel *createChannel(Client *cl, std::string const& name);
	Channel *channelFindByName(std::string const& name) const;
	void deleteChannel(Channel *ch);

	void addTransferSession(TransferSession *ts);

	Client*	clientFindByFd(int fd) const;
	Client*	clientFindByNickname(std::string const& name) const;
	void clientChangeName(Client *cl, std::string const& newName);
	void addClient(Client *cl);
	void removeClient(Client *cl);

	int					getPort() const;
	std::string const&	getPassword() const;

	void 				setPort(int port);
	void				setPassword(std::string const& password);

	TransferSession *transferSessionFindByToken(std::string const& token) const;
	std::map<std::string, TransferSession *> getPendingTransfers() const;

private:
	// State
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
	std::map<std::string, TransferSession *>	_pendingTransfers;

	// TEST
	friend class Tester;
};
