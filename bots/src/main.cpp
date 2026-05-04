#include "RespectBot.hpp"
#include <iostream>

int main(int ac, char **argv)
{
#if 1
    (void)ac;
    (void)argv;
    RespectBot rb("localhost", "6667", "a");
#else
    if (ac < 3)
    {
        std::cerr << "Usage: ./bin <host> <port> <password>" << std::endl;
        return 1;
    }
    RespectBot rb(argv[1], argv[2], argv[3]);
#endif

    rb.listen();
}