#ifndef IRC_HPP
#define IRC_HPP

//circular dependencies check later
#include <netinet/in.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <fcntl.h>
#include <poll.h>
#include <iostream>
#include <string>
#include <cstdlib>
#include <unistd.h>
#include <vector>
#include <map>
#include <set>
#include <csignal>
#include <string.h>
#include <cerrno>
#include <algorithm>

#include "AdvancedMap.hpp"
#include "Client.hpp"
#include "Channel.hpp"
#include "Server.hpp"
#include "Exceptions.hpp"

extern volatile sig_atomic_t g_serverRunning;

#endif