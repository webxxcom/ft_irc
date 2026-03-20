
#include "../include/irc.hpp"
#include "../include/Server.hpp"

int main(int ac, char *av[])
{
    try {
        Server s(ac, av);
        // s.startServer();
    }
    catch (ServerExceptions &e) {
        std::cout << e.what() << std::endl;
    }
    return 0;
}