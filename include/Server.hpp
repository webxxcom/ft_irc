#ifndef SERVER_HPP
#define SERVER_HPP

#include "irc.hpp"

class Server {
    private:
        std::string     			_password;
        int             			_port;
        int           				_serverSocketfd;
        std::vector<struct pollfd>  _pollfds;
		std::map<int, Client>		_clients;
        std::string                 _msgforClient;

        int parseArgs(int ac, char *av[]);
		void setupServer();
		void acceptClient();
        void receiveClientData(int &i, Client &client);
        void messageClient(int &i);
        void removeClient(int &i);
        void handleClientCommands(Client &client);
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