#pragma once

#include <map>
#include <sstream>

class Server;
class Client;
class Channel;

class CommandHandler {
public:
	CommandHandler(Server &server);

	void handle(Client *cl);

private:
	typedef void (CommandHandler::*CommandFn)(Client *, std::stringstream&);

	std::map<std::string, CommandFn>	_commandMap;
	Server& 							_server;

	void setupCommands();
	void handlePass(Client* client, std::stringstream& command);
	void handleUser(Client* client, std::stringstream& command);
	void handleNick(Client* client, std::stringstream& command);
	void handleCap(Client* client, std::stringstream& command);
	void handleJoin(Client* client, std::stringstream& command);
	void handlePrivmsg(Client *client, std::stringstream &command);
	void handleKick(Client* client, std::stringstream& command);
	void handleInvite(Client* client, std::stringstream& command);
	void handleTopic(Client* client, std::stringstream& command);
	void handleMode(Client* client, std::stringstream& command);

	Client *clientLooksFor(Client *client, std::string const& nick) const;
	Client *clientLooksFor(Client *client, std::string const& nick, Channel *ch) const;
};
