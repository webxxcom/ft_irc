
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
    try {
        Server s(ac, av);
        s.startServer();
    }
    catch (ServerErrorException &e) {
        std::cout << e.what() << std::endl;
        //need to close all fds, remove clients atd.
    }
    return 0;
}