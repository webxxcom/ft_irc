#include "Server.hpp"

#include <fstream>
#include <sys/stat.h>
#include <iostream>
#include <sys/types.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <cstring>
#include <unistd.h>
#include <sstream>
#include "FileSendHandler.hpp"
#include "TransferSession.hpp"

static long verifyAndGetSize(std::string const& path)
{
    struct stat st;

    if (stat(path.c_str(), &st) != 0)
        return -1;
    if (!S_ISREG(st.st_mode))
        return -1;
    return (long)(st.st_size);
}

int createListener(int port, int &realPort)
{
    int fd = ::socket(AF_INET, SOCK_STREAM, 0);
    if (fd < 0)
        return -1;

    int yes = 1;
    ::setsockopt(fd, SOL_SOCKET, SO_REUSEADDR, &yes, sizeof(yes));

    sockaddr_in addr;
    std::memset(&addr, 0, sizeof(addr));

    addr.sin_family = AF_INET;
    addr.sin_addr.s_addr = INADDR_ANY;
    addr.sin_port = ::htons(port);

    if (bind(fd, (sockaddr*)&addr, sizeof(addr)) < 0)
    {
        ::close(fd);
        return -1;
    }

    if (listen(fd, 1) < 0)
    {
        ::close(fd);
        return -1;
    }

    sockaddr_in bound;
    socklen_t len = sizeof(bound);

    if (getsockname(fd, (sockaddr*)&bound, &len) < 0)
    {
        close(fd);
        return -1;
    }

    realPort = ntohs(bound.sin_port);
    return fd;
}

FileSendHandler::FileSendHandler(ServerState &server) : _serverState(server) { }

std::string makeToken()
{
    static int i = 0;
    std::ostringstream oss;
    oss << ++i;
    return oss.str();
}

void FileSendHandler::request(Client *sender, Client *recevier, std::string const &filename)
{
    TransferSession *ts = new TransferSession();
    ts->file = filename;
    ts->size = verifyAndGetSize(filename);
    int port;
    ts->listenerFd = createListener(0, port);
    ts->state = ts->WAITING_RESPONSE;
    ts->token = makeToken();
    ts->to = recevier;
    ts->from = sender;

    std::ostringstream oss;
    oss << ":" << sender->getFullUserPrefix() << " FILE REQ "
        << ts->token << " " << ts->file << " " << ts->size << " " << port << "\r\n";
    recevier->receiveMsg(oss.str());
    _serverState.addTransferSession(ts);
}

bool openTransferSocket(TransferSession *ts)
{
    sockaddr_in peer;
    socklen_t len = sizeof(peer);

    int fd = ::accept(ts->listenerFd, (sockaddr*)&peer, &len);
    if (fd < 0)
        return false;

    ts->socketFd = fd;
    ::close(ts->listenerFd);
    ts->listenerFd = -1;

    return true;
}

void FileSendHandler::accept(Client *, TransferSession *ts)
{
    ts->state = ts->ACCEPTED;

    ts->state = ts->TRANSFERRING;
}

void FileSendHandler::reject(Client *, TransferSession *ts)
{
    ts->state = ts->REJECTED;
}

void FileSendHandler::sendChunk(TransferSession *ts)
{
    if (ts->state != TransferSession::TRANSFERRING)
        return ;

    char buf[4096];

    ts->ifs.read(buf, sizeof(buf));
    std::streamsize n = ts->ifs.gcount();

    if (n <= 0)
    {
        ts->state = TransferSession::DONE;
        return;
    }

    std::streamsize off = 0;

    while (off < n)
    {
        ssize_t sent = ::send(ts->socketFd, buf + off, n - off, 0);

        if (sent <= 0)
        {
            ts->state = TransferSession::FAILED;
            return;
        }

        off += sent;
    }
}

