#pragma once
#include <string>
#include <fstream>

class Client;

struct TransferSession
{
	enum states
	{
		WAITING_RESPONSE,
		ACCEPTED,
		TRANSFERRING,
		DONE,
		REJECTED,
		FAILED
	};

	TransferSession() : to(NULL), from(NULL), size(0), state(WAITING_RESPONSE), listenerFd(-1), socketFd(-1) { }

	std::string		token;
	Client*			to;
	Client*			from;
	std::string		file;
	std::string		remainder;
	long			size;
	states			state;
	int				listenerFd;
	int				socketFd;

	std::ifstream	ifs;
};