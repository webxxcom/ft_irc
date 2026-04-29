#pragma once

#include <string>

class Client;
class ServerState;
class TransferSession;

class FileSendHandler {
public:
    FileSendHandler(ServerState &server);

    void request(Client *sender, Client *recevier, std::string const& filename);
    void accept(Client *client, TransferSession *ts);
    void reject(Client *client, TransferSession *ts);
    void sendChunk(TransferSession* ts);
private:
   ServerState &_serverState;

};
