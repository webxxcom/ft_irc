#pragma once
#include <string>
#include <map>
#include <vector>

/**
 * Every bot is like a normal user
 * They must always exist - so create at the startup
 * Bot's invited to a channel - it automatically joins it
 * 
 * 
 * 
 */
class RespectBot {
public:
    RespectBot(std::string const& host, std::string const& port, std::string const& pass);
    ~RespectBot();

    void listen();
private:
    int _sock;
    std::string _nickname;

    // Channel name -> pair of client's name and their respect counter
    typedef std::map<std::string, std::map<std::string, int> > respectMap;
    respectMap respectCounter;

    void processRegistration(std::string const& pass);

    void processInvite(std::string const& line);
    void processPrivmsg(std::stringstream &ss);

    void processLine(std::string const &line);
    void sendDataToServer(std::string const& data) const;

    std::string getIrcNickname() const;
};
