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

FileSendHandler::FileSendHandler(ServerState &server) : _registry(server) { }

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
    recevier->receiveMsg(oss.str(), _registry);
    _registry.addTransferSession(ts);
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

void FileSendHandler::accept(Client *, TransferSession *ts) const
{
    if (ts->state != TransferSession::TRANSFERRING)
        ts->state = ts->ACCEPTED;
}

void FileSendHandler::reject(Client *, TransferSession *ts) const
{
    _registry.removeTransferSession(ts);
}

void FileSendHandler::sendChunk(TransferSession *ts) const
{
    if (ts->state != TransferSession::TRANSFERRING)
        return;

    const size_t    bufSize = 4096;
    char            buf[bufSize];

    size_t offset = ts->remainder.size();

    if (!ts->remainder.empty())
        std::memcpy(buf, ts->remainder.data(), offset);

    ts->ifs.read(buf + offset, bufSize - offset);
    std::streamsize n = ts->ifs.gcount();

    size_t total = offset + n;

    if (total == 0)
    {
        ts->state = TransferSession::DONE;
        return;
    }

    ssize_t sent = ::send(ts->socketFd, buf, total, 0);

    if (sent < 0)
    {
        ts->state = TransferSession::FAILED;
        return;
    }

    if ((size_t)sent < total)
        ts->remainder.assign(buf + sent, total - sent);
    else
        ts->remainder.clear();
}

