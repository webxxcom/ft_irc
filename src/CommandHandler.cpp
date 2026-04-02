#include "CommandHandler.hpp"
#include "Server.hpp"

using namespace irc;

void CommandHandler::setupCommands()
{
    _commandMap["PASS"]     = &CommandHandler::handlePass;
    _commandMap["NICK"]     = &CommandHandler::handleNick;
    _commandMap["USER"]     = &CommandHandler::handleUser;
    _commandMap["CAP"]      = &CommandHandler::handleCap;
    _commandMap["JOIN"]     = &CommandHandler::handleJoin;
    _commandMap["INVITE"]   = &CommandHandler::handleInvite;
    _commandMap["TOPIC"]    = &CommandHandler::handleTopic;
    _commandMap["MODE"]     = &CommandHandler::handleMode;
}

CommandHandler::CommandHandler(Server& server) : _server(server)
{
    setupCommands();
}

void CommandHandler::handle(Client *cl)
{
    std::vector<std::string> &mssgs = cl->getReceivedMessages();

    while (!mssgs.empty())
    {
        // Get the whole line
        std::stringstream line;
        line << mssgs.back();
        mssgs.pop_back();

        // Extract single command
        std::string command;
        std::getline(line, command, ' ');

        // Find the command in the map
        std::map<std::string, CommandFn>::iterator it = _commandMap.find(command);
        if (it != _commandMap.end())
            (this->*(it->second))(cl, line);
        else
            cl->receiveMsg(ERR_UNKNOWN_COMMAND, command);
    }
}

void CommandHandler::handlePass(Client *client, std::stringstream &command)
{
    std::string word;

    // Client sent the password they used to connect to the server
    std::getline(command, word, ' ');
    if (word != _server._password)
        // The passwords do not match -- send the message it's incorrect
        client->receiveMsg(ERR_PASSWDMISMATCH);
    else
        client->setPassword(word); // set password if correct
}

void CommandHandler::handleUser(Client *client, std::stringstream& command)
{
    std::string word;
    
    // Username
    std::getline(command, word, ' ');
    client->setUsername(word);

    // Mode, Asterix
    std::getline(command, word, ' ');
    std::getline(command, word, ' ');

    // Real name
    std::getline(command, word, ' ');
    client->setRealname(word);
}

void CommandHandler::handleNick(Client *client, std::stringstream& command)
{
    std::string word;
   
    std::getline(command, word, ' ');
    client->setNickname(word);
}

void CommandHandler::handleCap(Client *client, std::stringstream& command)
{
    std::string word;
    
    std::getline(command, word, ' ');
    if (word == "LS")
        client->receiveMsg(":server CAP * LS :\r\n"); // No capabilities
    else // Do not support any other than LS
        client->receiveMsg(ERR_UNKNOWN_COMMAND, word);
}

// JOIN <channels> [<keys>]
void CommandHandler::handleJoin(Client *client, std::stringstream &command)
{
    std::string channel;
    std::getline(command, channel, ' ');

    // ! The keys to channels are not yet implemented
    std::stringstream channelList;
    channelList << channel;
    while (1)
    {
        std::getline(channelList, channel, ',');
        if (channelList.eof())
            break ;
        if (channel[0] != '#' || channel.size() < 2)
            continue ;

        Channel *ch;
        std::pair<Channel *, bool> optChannel = _server._channelsByName.find(channel);
        if (!optChannel.second)
            ch = _server.createChannel(client, channel);
        else
            ch = optChannel.first;
        
        std::string msg = 
            ":" + client->getFullUserPrefix() + " JOIN " + ":" + channel;
        ch->broadcast(msg);
        ch->addMember(client);
        client->receiveMsg(RPL_NAMREPLY, client->getNickname() + " = " + channel);
        client->receiveMsg(RPL_ENDOFNAMES, client->getNickname() + " " + channel);
    }
}

// KICK <channel> <client> :[<message>]
void CommandHandler::handleKick(Client *client, std::stringstream &command)
{
    std::string channel, member, message;
    std::getline(command, channel, ' ');
    std::getline(command, member, ' ');
    std::getline(command, message, ' ');

    if (channel[0] != '#' || channel.size() < 2)
        return ;
    std::pair<Channel *, bool> optCh = _server._channelsByName.find(channel);
    if (!optCh.second)
        return client->receiveMsg(ERR_NOSUCHCHANNEL, channel);

    Channel *ch = optCh.first;
    if (!ch->hasMember(client))
        return client->receiveMsg(ERR_NOTONCHANNEL, client->getNickname() + channel);

    if (!ch->hasOperator(client))
        return client->receiveMsg(ERR_CHANOPRIVSNEEDED, client->getNickname() + channel);
        
    std::pair<Client *, bool> optMember = ch->hasMember(member);
    if (!optMember.second)
        return client->receiveMsg(ERR_USERNOTINCHANNEL, member + " " + channel);

    // :<source> KICK <channel> <target> :<reason>
    std::string msg = 
        ":" + client->getNickname() + "!" + client->getUsername() + "@host" +
        " KICK " + channel + " " + member + " :" + message + "\r\n";

    ch->broadcast(msg);
    ch->removeMember(optMember.first);
}

// INVITE <nickname> <channel>
void CommandHandler::handleInvite(Client *client, std::stringstream &command)
{
    std::string nickname, channel;

    std::getline(command, nickname, ' ');
    std::getline(command, channel, ' ');

    if (channel[0] != '#' || channel.size() < 2)
        return ;

    std::pair<Channel *, bool> optChannel = _server._channelsByName.find(channel);
    if (!optChannel.second)
        return client->receiveMsg(ERR_NOSUCHCHANNEL, channel);

    Channel *ch = optChannel.first;
    if (!ch->hasMember(client))
        return client->receiveMsg(ERR_NOTONCHANNEL, client->getIrcNickname() + " " + channel);
    
    if ((ch->getModes() & Channel::E_INVITE_ONLY) && !ch->hasOperator(client))
        return client->receiveMsg(ERR_CHANOPRIVSNEEDED, client->getIrcNickname() + " " + channel);

    std::pair<Client *, bool> target = _server._clientsByName.find(nickname);
    if (!target.second)
        return client->receiveMsg(ERR_NOSUCHNICK);

    Client *invitee = target.first;

    // :<inviter>!<user>@<host> INVITE <invitee> :<channel>
    invitee->receiveMsg(client->getFullUserPrefix() + " INVITE " + invitee->getNickname() + ":" + channel); // ! Not sure about username

    // :server 341 <inviter> <invitee> <channel>
    client->receiveMsg(RPL_INVITING, client->getNickname() + " " + invitee->getNickname() + " " + channel);
}

void CommandHandler::handleTopic(Client *client, std::stringstream &command)
{

}

void CommandHandler::handleMode(Client *client, std::stringstream &command)
{
    // std::string first, flags;

    // std::getline(command, first, ' ');
    // std::getline(command, flags, ' ');

    // // Setting modes for a channel
    // if (first[0] == '#')
    // {
    //     std::map<std::string, Channel>::iterator it = _channels.find(first);
    //     if (it == _channels.end())
    //         return ; // No such channel :(

    //     Channel &ch = it->second;
    //     if (!client.isOperatorOf(ch))
    //         return ;
            
    //     if (!flags.empty())
    //     {
    //         unsigned int modes = 0;
    //         for (size_t i = 1; i < flags.size(); ++i)
    //         {
    //             switch (flags[i])
    //             {
    //             case 'i':
    //                 modes |= modes & Channel::E_INVITE_ONLY;
    //                 break;
    //             case 't':
    //                 modes |= modes & Channel::E_TOPIC_RESTRICT;
    //                 break;
    //             case 'k':
    //                 modes |= modes & Channel::E_CHANNEL_KEY;
    //                 break;
    //             case 'l':
    //                 modes |= modes & Channel::E_USER_LIMIT;
    //                 break;
    //             }
    //         }
    //         if (flags[0] == '+')
    //             ch.addModes(modes);
    //         else
    //             ch.removeModes(modes);
    //     }
    //     else
    //     {
    //         // Wanna know the current channel's modes
    //         // ? Do we want to implement?
    //     }
    // }
    // else // no '#'? then it's a user hahaaaaa
    // {

    // }
}
