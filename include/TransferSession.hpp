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
	std::string token;
	Client *to;
	Client *from;
	std::string file;
	long size;
	states state;
	int listenerFd;
	int socketFd;

	std::ifstream ifs;
};