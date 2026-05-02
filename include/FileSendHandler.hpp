#pragma once

#include <string>

class Client;
class ServerState;
struct TransferSession;

class FileSendHandler {
public:
    explicit FileSendHandler(ServerState &server);

    void request(Client *sender, Client *recevier, std::string const& filename);
    void accept(Client *client, TransferSession *ts) const;
    void reject(Client *client, TransferSession *ts) const;
    void sendChunk(TransferSession* ts);
private:
   ServerState &_serverState;

};
