#pragma once

#include <poll.h>
#include <sys/types.h>
#include <csignal>
#include <fstream>

#include "Client.hpp"
#include "CommandHandler.hpp"
#include "Channel.hpp"
#include "Exceptions.hpp"
#include "ServerNotifyCodes.hpp"
#include "ReplyHandler.hpp"
#include "FileSendHandler.hpp"
#include "ServerState.hpp"
#include "TransferSession.hpp"

extern volatile sig_atomic_t g_serverRunning; // ! maybe move to Server class

class Server {
private:
	struct CompareByFd
	{   
		int _fd;
		CompareByFd(int fd) : _fd(fd) {}
		bool operator()(pollfd const& o) { return (o.fd == _fd); }
	};

	int           				            	_serverSocketfd;
	ServerState									_state;
	FileSendHandler								_fileSendHandler;
	ReplyHandler								_replyHandler;
	CommandHandler 								_commandHandler;
	std::vector<struct pollfd>              	_pollfds;

	int parseArgs(int ac, char *av[]);

	void setupServer();
	void acceptClient();
	bool receiveClientData(Client *client);
	bool messageClient(Client *client);
	void disconnectClient(Client *client);
	void handlePendingTransfers();
	void handlePolls();
	void finishServer();

	Server(const Server &);
public:
	Server(int ac, char *av[]);
	~Server();
	
	void startServer();

	// TEST FUNCTION
	friend class Tester;
};

enum returned {
	ARGS_NUM_INVALID,
	PORT_NUM_INVALID,
	OK,
} ;
