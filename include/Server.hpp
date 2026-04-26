#pragma once

#include <poll.h>
#include <sys/types.h>
#include <csignal>

#include "AdvancedMap.hpp"
#include "Client.hpp"
#include "CommandHandler.hpp"
#include "Channel.hpp"
#include "Exceptions.hpp"
#include "ServerNotifyCodes.hpp"
#include "ReplyHandler.hpp"
#include "FileSendHandler.hpp"
#include <fstream>

extern volatile sig_atomic_t g_serverRunning; // ! maybe move to Server class

struct TransferSession
{
	enum states
	{
		WAITING_RESPONSE,
		ACCEPTED,
		TRANSFERRING,
		DONE,
		REJECTED,
		FAILED
	};
	std::string token;
	Client *to;
	Client *from;
	std::string file;
	long size;
	states state;
	int listenerFd;
	int socketFd;

	std::ifstream ifs;
};

class Server {
private:
	struct CompareByFd
	{   
		int _fd;
		CompareByFd(int fd) : _fd(fd) {}
		bool operator()(pollfd const& o) { return (o.fd == _fd); }
	};

	friend class CommandHandler;
	friend class FileSendHandler;
	int           				            _serverSocketfd;
	FileSendHandler							_fileSendHandler;
	ReplyHandler							_replyHandler;
	CommandHandler 							_commandHandler;
	std::string     			            _password;
	int             			            _port;
	std::vector<struct pollfd>              _pollfds;
	AdvancedMap<std::string, TransferSession *>	_pendingTransfers;

	// Channels storage
	std::vector<Channel *>					_channels; // the pointers owner
	AdvancedMap<std::string, Channel *>		_channelsByName;

	// Clients storage
	std::vector<Client *>					_clients; // the pointers owner
	std::map<int, Client *>					_clientsByFd;
	AdvancedMap<std::string, Client *>		_clientsByName;

	int parseArgs(int ac, char *av[]);

	void setupServer();
	void acceptClient();
	bool receiveClientData(Client *client);
	bool messageClient(Client *client);
	void disconnectClient(Client *client);

	void handlePolls();
	void handlePendingTransfers();
	Channel *createChannel(Client *cl, std::string const& name);
	void deleteChannel(Channel *ch);
	void addTransferSession(TransferSession *ts);

	// Commands
	Server(const Server &);
public:
	Server(int ac, char *av[]);
	~Server();
	void startServer();
	void finishServer();

	// TEST FUNCTION
	friend class Tester;

	AdvancedMap<std::string, Channel *> const& 	getChannelsByName() const;
	std::vector<Channel *> const&				getChannels() 		const;
};

enum returned {
	ARGS_NUM_INVALID,
	PORT_NUM_INVALID,
	OK,
} ;
