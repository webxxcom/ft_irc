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

extern volatile sig_atomic_t g_serverRunning; // ! maybe move to Server class

class Server {
	private:
		struct CompareByFd
		{   
			int _fd;
			CompareByFd(int fd) : _fd(fd) {}
			bool operator()(pollfd const& o) { return (o.fd == _fd); }
		};

		friend class CommandHandler;
		ReplyHandler							_replyHandler;
		CommandHandler 							_commandHandler;
		std::string     			            _password;
		int             			            _port;
		int           				            _serverSocketfd;
		std::vector<struct pollfd>              _pollfds;

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
		Channel *createChannel(Client *cl, std::string const& name);

		// Commands
		Server(const Server &);
	public:
		Server(int ac, char *av[]);
		~Server();
		void startServer();
		void finishServer();
		void clientReadyReceive(Client *client);

		// TEST FUNCTION
		friend class Tester;

		AdvancedMap<std::string, Channel *> const& 	getChannelsByName() const;
		std::vector<Channel *> const&				getChannels() 		const;
} ;

enum returned {
	ARGS_NUM_INVALID,
	PORT_NUM_INVALID,
	OK,
} ;
