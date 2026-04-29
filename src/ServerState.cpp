#include "ServerState.hpp"
#include "Channel.hpp"
#include "Client.hpp"
#include "TransferSession.hpp"
#include "FileSendHandler.hpp"

#include <algorithm>

ServerState::ServerState() { }

ServerState::~ServerState()
{
	for(size_t i = 0; i < _clients.size(); ++i)
		delete _clients[i];
	for(size_t i = 0; i < _channels.size(); ++i)
		delete _channels[i];
}

Channel *ServerState::createChannel(Client *creator, std::string const &name)
{
	Channel *ch = new Channel(creator, name);

	_channels.push_back(ch);
	_channelsByName.insert(std::pair<std::string, Channel *>(name, ch));
	return ch;
}

Channel *ServerState::channelFindByName(std::string const &name) const
{
	std::map<std::string, Channel *>::const_iterator it = _channelsByName.find(name);
	return it == _channelsByName.end() ? NULL : it->second;
}

void ServerState::deleteChannel(Channel *ch)
{
	_channelsByName.erase(ch->getName());
	std::remove(_channels.begin(), _channels.end(), ch);
	delete ch;
}

void ServerState::addTransferSession(TransferSession *ts)
{
	_pendingTransfers.insert(std::pair<std::string, TransferSession *>(ts->token, ts));
}

Client *ServerState::clientFindByFd(int fd) const
{
	std::map<int, Client *>::const_iterator it = _clientsByFd.find(fd);
	return it == _clientsByFd.end() ? NULL : it->second;
}

Client *ServerState::clientFindByNickname(std::string const &name) const
{
	std::map<std::string, Client *>::const_iterator it = _clientsByName.find(name);
	return it == _clientsByName.end() ? NULL : it->second;
}

void ServerState::clientChangeName(Client *cl, std::string const &newName)
{
	if (cl->hasNickname())
		_clientsByName.erase(cl->getNickname());
	cl->setNickname(newName);
	_clientsByName.insert(std::pair<std::string, Client *>(newName, cl));
}

void ServerState::addClient(Client *cl)
{
	_clients.push_back(cl);
	_clientsByFd.insert(std::pair<int, Client *>(cl->getFd(), cl));
}

void ServerState::removeClient(Client *cl)
{
	if (!cl)
		return ;

	_clientsByFd.erase(cl->getFd());
	_clientsByName.erase(cl->getNickname());
	_clients.erase(std::find(_clients.begin(), _clients.end(), cl));
    delete cl;
}

int						ServerState::getPort() const { return _port; }
std::string const&		ServerState::getPassword() const { return _password; }

void					ServerState::setPort(int port) { _port = port; }
void					ServerState::setPassword(std::string const &password) { _password = password; }

TransferSession *ServerState::transferSessionFindByToken(std::string const &token) const
{
	std::map<std::string, TransferSession *>::const_iterator it = _pendingTransfers.find(token);
	return it == _pendingTransfers.end() ? NULL : it->second;
}

std::map<std::string, TransferSession *> ServerState::getPendingTransfers() const
{
	return _pendingTransfers;
}
