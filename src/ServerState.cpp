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

int ServerState::poll()
{
	return ::poll(&_pollfds[0], _pollfds.size(), -1);
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

bool ServerState::pollfdFindByFd(int fd, pollfd& out) const
{
	for (std::vector<pollfd>::const_iterator it = _pollfds.begin(); it != _pollfds.end(); ++it)
	{
		if (it->fd == fd)
		{
			out = *it;
			return true;
		}
	}
	return false;
}

Channel *ServerState::createChannel(Client *creator, std::string const &name)
{
	Channel *ch = new Channel(creator, name);

	_channels.push_back(ch);
	return ch;
}

Channel *ServerState::channelFindByName(std::string const &name) const
{
	std::vector<Channel *>::const_iterator it = std::find_if(_channels.begin(), _channels.end(), Channel::NameEquals(name));
	return it != _channels.end() ? *it : NULL;
}

void ServerState::removeChannel(Channel *ch)
{
	_channels.erase(std::find(_channels.begin(), _channels.end(), ch));
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
	_transferSession.erase(std::find(_transferSession.begin(), _transferSession.end(), ts));
	delete ts;
}

std::set<Client *> ServerState::getUsersClientKnows(Client *cl) const
{
    std::set<Client *> res;

	for(size_t i = 0; i < _channels.size(); ++i)
	{
		if (_channels[i]->hasMember(cl))
		{
			std::set<Client *> const& users = _channels[i]->getMembers();
			for(std::set<Client *>::iterator it = users.begin(); it != users.end(); ++it)
				res.insert(*it);
		}
	}
	res.insert(cl);
	return res;
}

Client *ServerState::clientFindByFd(int fd) const
{
	for(size_t i = 0; i < _clients.size(); ++i)
		if (_clients[i]->getFd() == fd)
			return _clients[i];
	return (NULL);
}

Client *ServerState::clientFindConnectedByNickname(std::string const &name) const
{
    std::vector<Client *>::const_iterator it = std::find_if(_clients.begin(), _clients.end(), Client::NickEquals(name));
	return (it != _clients.end() && !(*it)->isPendingDisconnect()) ? *it : NULL;
}

Client *ServerState::clientFindByNickname(std::string const &name) const
{
	std::vector<Client *>::const_iterator it = std::find_if(_clients.begin(), _clients.end(), Client::NickEquals(name));
	return it != _clients.end() ? *it : NULL;
}

void ServerState::clientChangesName(Client *cl, std::string const &newName) const
{
	if (cl->hasNickname())
	{
		std::set<Client *> recipients = getUsersClientKnows(cl);

		std::string msg = ":" + cl->getIrcNickname() + " NICK :" + newName + "\r\n";
		for(std::set<Client *>::iterator it = recipients.begin(); it != recipients.end(); ++it)
			(*it)->receiveMsg(msg, *this);
	}

	cl->setNickname(newName);
}

void ServerState::addClient(Client *cl)
{
	_clients.push_back(cl);
}

void ServerState::removeClient(Client *cl)
{
	if (!cl)
		return ;

	pollfdRemove(cl->getFd());
	_clients.erase(std::find(_clients.begin(), _clients.end(), cl));
	removeClientFromAllChannels(cl);
    delete cl;
}

void ServerState::removeClientFromAllChannels(Client *cl)
{
	for(size_t i = 0; i < _channels.size(); ++i)
	{
		_channels[i]->removeMember(cl);
		if (_channels[i]->isEmpty())
			removeChannel(_channels[i]);
	}
}

void ServerState::clientIsReadyToReceiveMessage(Client const* cl) const
{
	pollfd clientPollFd;
	if (!pollfdFindByFd(cl->getFd(), clientPollFd))
		return ;

	clientPollFd.events = POLLIN | POLLOUT;
}

void ServerState::clientDisconnects(Client *cl) const
{
	if (!cl)
		return;

	cl->setPendingDisconnect(true);
}

std::vector<struct pollfd> const& 	ServerState::getPollFds() 			const	{ return _pollfds; }
std::string const&					ServerState::getPassword() 			const 	{ return _password; }
int									ServerState::getPort() 				const 	{ return _port; }
int 								ServerState::getServerSocketFd() 	const 	{ return _serverSocketfd; }
bool 								ServerState::isTransferFd(int fd) 	const	{ return transferSessionFindByFd(fd) != NULL; }

// Modifiers
void								ServerState::setPort(int port) { _port = port; }
void								ServerState::setPassword(std::string const &password) { _password = password; }
void 								ServerState::setServerSockerFd(int fd) { _serverSocketfd = fd; }

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


