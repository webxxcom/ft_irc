#pragma once

#include <map>
#include <sstream>

class ServerState;
class Client;
class Channel;
class ReplyHandler;
class FileSendHandler;

class CommandHandler {
public:
	CommandHandler(ServerState &registry, ReplyHandler& rh, FileSendHandler &fsh);

	void handle(Client *cl);

private:
	typedef void (CommandHandler::*CommandFn)(Client *, std::stringstream&);

	std::map<std::string, CommandFn>	_commandMap;
	ServerState& 						_registry;
	ReplyHandler& 						_replyHandler;
	FileSendHandler&					_fileSendHandler;

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
	void handlePing(Client* client, std::stringstream& command);
	void handleQuit(Client* client, std::stringstream& command);
	void handlePart(Client* client, std::stringstream& command);
	void handleFile(Client* client, std::stringstream& command);

	Client *clientLooksForUserInChannel(Client *client, std::string const& nick, Channel const* ch = NULL) const;
};
