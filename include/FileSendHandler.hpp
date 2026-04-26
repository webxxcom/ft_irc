#pragma once

#include <string>

class Client;
class Server;
class TransferSession;

class FileSendHandler {
public:
    FileSendHandler(Server &server);

    void request(Client *sender, Client *recevier, std::string const& filename);
    void accept(Client *client, TransferSession *ts);
    void reject(Client *client, TransferSession *ts);
    void sendInChunks(TransferSession* ts);
private:
   Server &_server;

};
