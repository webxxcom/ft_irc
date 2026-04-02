#pragma once

#include <poll.h>
#include <sys/types.h>
#include <csignal>

#include "AdvancedMap.hpp"
#include "Client.hpp"
#include "CommandHandler.hpp"
#include "Channel.hpp"
#include "Exceptions.hpp"

extern volatile sig_atomic_t g_serverRunning; // ! maybe move to Server class

enum ServerNotifyCodes
{
	ERR_PASSWDMISMATCH = 464,
	ERR_NOTREGISTERED = 451,
	ERR_ALREADYREGISTERED = 462,
	ERR_UNKNOWN_COMMAND = 421,
	ERR_NOSUCHNICK = 401,
	ERR_NOSUCHCHANNEL = 403,
	ERR_NOTONCHANNEL = 442,
	ERR_USERNOTINCHANNEL = 441,
	ERR_NOPRIVILEGES = 481,
	ERR_CHANOPRIVSNEEDED = 482,
	RPL_INVITING = 341
};

class Server {
	private:
		struct CompareByFd
		{   
			int _fd;
			CompareByFd(int fd) : _fd(fd) {}
			bool operator()(pollfd const& o) { return (o.fd == _fd); }
		};

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
		void receiveClientData(Client *client);
		void messageClient(Client &client);
		void disconnectClient(Client *client); // Renamed removeClient to disconnectClient

		void handlePolls();
		Channel *createChannel(Client *cl, std::string const& name);

		// Commands
		void notifyClient(Client *client, ServerNotifyCodes error_code, std::string const& extra = "");
		Server(const Server &);
		friend class CommandHandler;
	public:
		Server(int ac, char *av[]);
		~Server();
		void startServer();
		void finishServer();
} ;

enum returned {
	ARGS_NUM_INVALID,
	PORT_NUM_INVALID,
	OK,
} ;
