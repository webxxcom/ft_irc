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
	ServerState			_state;
	FileSendHandler		_fileSendHandler;
	ReplyHandler		_replyHandler;
	CommandHandler 		_commandHandler;

	int parseArgs(int ac, char *av[]);

	void setupServer();
	void acceptClient();
	bool receiveClientData(Client *client);
	bool messageClient(Client *client);
	void disconnectClient(Client *client);
	bool handleTransferFd(int fd, int ev);
	void handlePolls(std::vector<struct pollfd> &pollfds);
	void finishServer();

	Server(const Server &);
public:
	Server(int ac, char *av[]);
	
	void startServer();

	// TEST FUNCTION
	friend class Tester;
};

enum returned {
	ARGS_NUM_INVALID,
	PORT_NUM_INVALID,
	OK,
} ;
