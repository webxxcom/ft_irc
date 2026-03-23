
#include "../include/irc.hpp"
#include "../include/Server.hpp"

void signal_handler(int sig) {
    g_serverRunning = 0;
}

int main(int ac, char *av[])
{
    signal(SIGINT, signal_handler);
    signal(SIGQUIT, signal_handler);
    try {
        Server s(ac, av);
        s.startServer();
    }
    catch (ServerExceptions &e) {
        std::cout << e.what() << std::endl;
    }
    return 0;
}