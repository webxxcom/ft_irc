#include "ServerState.hpp"
#include "Channel.hpp"
#include "Client.hpp"
#include "TransferSession.hpp"
#include "FileSendHandler.hpp"

#include <algorithm>
#include <unistd.h>

ServerState::ServerState() : _serverSocketfd(-1) { }

ServerState::~ServerState()
{
	close(_serverSocketfd);
	for(size_t i = 0; i < _clients.size(); ++i)
		delete _clients[i];
	for(size_t i = 0; i < _channels.size(); ++i)
		delete _channels[i];
}

void ServerState::pollfdAdd(struct pollfd fd)
{
    this->_pollfds.push_back(fd);
}

void ServerState::pollfdRemove(int fd)
{
	for(std::vector<pollfd>::iterator it = _pollfds.begin(); it != _pollfds.end(); ++it)
	{
		if (it->fd == fd)
		{
			_pollfds.erase(it);
			return;
		}
	}
}

struct pollfd& ServerState::pollfdFindByFd(int fd)
{
	return *std::find_if(_pollfds.begin(), _pollfds.end(), CompareByFd(fd));
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
	_transferSession.push_back(ts);

	struct pollfd tsPollFd;
	tsPollFd.fd = ts->listenerFd;
	tsPollFd.events = POLLIN;
	pollfdAdd(tsPollFd);
}

void ServerState::removeTransferSession(TransferSession *ts)
{
	pollfdRemove(ts->listenerFd);
	// ! _transferSession.erase(std::find(_transferSession.begin(), _transferSession.end(), ts));
	// ! delete ts;
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

	pollfdRemove(cl->getFd());
	_clientsByFd.erase(cl->getFd());
	_clientsByName.erase(cl->getNickname());
	_clients.erase(std::find(_clients.begin(), _clients.end(), cl));
    delete cl;
}

int							ServerState::getPort() 				const 	{ return _port; }
std::string const&			ServerState::getPassword() 			const 	{ return _password; }
int 						ServerState::getServerSockerFd() 	const 	{ return _serverSocketfd; }
std::vector<struct pollfd>& ServerState::getPollFds() 					{ return _pollfds; }

void						ServerState::setPort(int port) { _port = port; }
void						ServerState::setPassword(std::string const &password) { _password = password; }
void 						ServerState::setServerSockerFd(int fd) { _serverSocketfd = fd; }

TransferSession *ServerState::transferSessionFindByToken(std::string const &token) const
{
	for(size_t i = 0; i < _transferSession.size(); ++i)
		if (_transferSession[i]->token == token)
			return _transferSession[i];
	return NULL;
}

TransferSession *ServerState::transferSessionFindByFd(int fd) const
{
	if (fd == -1)
		return NULL;

	for(size_t i = 0; i < _transferSession.size(); ++i)
		if (_transferSession[i]->listenerFd == fd || _transferSession[i]->socketFd == fd)
			return _transferSession[i];
	return NULL;
}

bool ServerState::isTransferFd(int fd) const
{
    return transferSessionFindByFd(fd) != NULL;
}
