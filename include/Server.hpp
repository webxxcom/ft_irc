#ifndef SERVER_HPP
#define SERVER_HPP

#include "irc.hpp"

enum ServerErrorCodes
{
    BAD_SERVER_PASSWORD = 464
};

class Server {
    private:
        std::string     			_password;
        int             			_port;
        int           				_serverSocketfd;
        std::vector<struct pollfd>  _pollfds;
        std::map<int, size_t>       _fd_index_map; // get the client index in the pollfds by its fd
		std::map<int, Client>		_clients;

        int parseArgs(int ac, char *av[]);
		void setupServer();
		void acceptClient();
        void receiveClientData(Client &client);
        void handleClientCommand(Client &client, std::stringstream &command);
        void messageClient(Client &client);
        void disconnectClient(Client &client); // Renamed removeClient to disconnectClient
        void handleClientCommands(Client &client);

        void handlePolls();
        void serverError(int error_code, Client &client);
    public:
        Server(int ac, char *av[]);
        Server(const Server &orig);
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