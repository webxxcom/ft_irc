
#include "Server.hpp"
#include <iostream>

volatile sig_atomic_t g_serverRunning = 1; // Can move it to Server class?

void signal_handler(int sig) {
    (void)sig;
    g_serverRunning = 0;
}

int main(int ac, char *av[])
{
    signal(SIGINT, signal_handler);
    signal(SIGQUIT, signal_handler);
    signal(SIGPIPE, SIG_IGN);
    try {
        Server s(ac, av);
        s.startServer();
    }
    catch (ServerErrorException &e) {
        std::cout << e.what() << std::endl;
    }
    return 0;
}