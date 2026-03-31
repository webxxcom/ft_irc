#ifndef SERVER_HPP
#define SERVER_HPP

#include "irc.hpp"

enum ServerErrorCodes
{
    ERR_PASSWDMISMATCH = 464,
    ERR_NOTREGISTERED = 451,
    ERR_ALREADYREGISTERED = 462
};

class Server {
    private:
        typedef void (Server::*CommandHandler)(Client&, std::stringstream&);

        std::map<std::string, CommandHandler>   _command_map; // map of commands
        std::string     			            _password;
        int             			            _port;
        int           				            _serverSocketfd;
        std::vector<struct pollfd>              _pollfds;
        std::map<int, size_t>                   _fd_index_map; // get the client index in the pollfds by its fd
		std::map<int, Client>		            _clients;

        void setupCommands();
        int parseArgs(int ac, char *av[]);

		void setupServer();
		void acceptClient();
        void receiveClientData(Client &client);
        void messageClient(Client &client);
        void disconnectClient(Client &client); // Renamed removeClient to disconnectClient
        void handleClientCommands(Client &client);

        void handlePolls();

        // Commands
        void handlePass(Client& client, std::stringstream& command);
        void handleUser(Client& client, std::stringstream& command);
        void handleNick(Client& client, std::stringstream& command);
        void handleCap(Client& client, std::stringstream& command);

        void serverError(ServerErrorCodes error_code, Client &client);
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