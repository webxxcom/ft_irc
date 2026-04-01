#ifndef SERVER_HPP
#define SERVER_HPP

#include "irc.hpp"

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

class Channel;

class Server {
	private:
		typedef void (Server::*CommandHandler)(Client*, std::stringstream&);

		std::map<std::string, CommandHandler>   _command_map; // map of commands
		std::string     			            _password;
		int             			            _port;
		int           				            _serverSocketfd;
		std::vector<struct pollfd>              _pollfds;
		std::map<int, size_t>                   _fd_index_map; // get the client index in the pollfds by its fd

		// Channels storage
		std::vector<Channel *>					_channels; // the pointers owner
		AdvancedMap<std::string, Channel *>     _channelsByName;

		// Clients storage
		std::vector<Client *>					_clients; // the pointers owner
		std::map<int, Client *>					_clientsByFd;
		AdvancedMap<std::string, Client *>		_clientsByName;

		void setupCommands();
		int parseArgs(int ac, char *av[]);

		void setupServer();
		void acceptClient();
		void receiveClientData(Client *client);
		void messageClient(Client &client);
		void disconnectClient(Client *client); // Renamed removeClient to disconnectClient
		void handleClientCommands(Client *client);

		void handlePolls();
		Channel *createChannel(Client *cl, std::string const& name);

		// Commands
		void handlePass(Client* client, std::stringstream& command);
		void handleUser(Client* client, std::stringstream& command);
		void handleNick(Client* client, std::stringstream& command);
		void handleCap(Client* client, std::stringstream& command);
		void handleJoin(Client* client, std::stringstream& command);
		void handleKick(Client* client, std::stringstream& command);
		void handleInvite(Client* client, std::stringstream& command);
		void handleTopic(Client* client, std::stringstream& command);
		void handleMode(Client* client, std::stringstream& command);

		void notifyClient(Client *client, ServerNotifyCodes error_code, std::string const& extra = "");
		Server(const Server &);
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

#endif