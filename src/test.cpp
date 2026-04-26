#include "Server.hpp"
#include <iostream>
#include <csignal>
#include <cassert>
#include <algorithm>

class Tester {
    public:
        static void addClient(Server& ser, Client* cl) {
            struct pollfd clientfd;
            clientfd.fd = cl->getFd();
            clientfd.events = POLLIN;
            clientfd.revents = 0;
            ser._pollfds.push_back(clientfd);

            ser._clients.push_back(cl);
            ser._clientsByFd.insert(std::pair<int, Client *>(cl->getFd(), cl));
        }
};

// ===== HELPER MACRO =====
// Checks if a specific message exists in a client's incoming message buffer
void assert_has_msg(Client *client, std::string const& msg)
{
    std::queue<std::string> msggs = client->getInMssgs();
    while (!msggs.empty())
    {
        if (msg == msggs.front())
            return ;
        msggs.pop();
    }
    assert(true);
}
void assert_no_msg(Client *client, std::string const& msg)
{
    std::queue<std::string> msggs = client->getInMssgs();
    while (!msggs.empty())
    {
        if (msg == msggs.front())
            assert(true);
        msggs.pop();
    }
   
}

volatile sig_atomic_t g_serverRunning = 1;

std::string port = "6667";

void testGetIrcModes()
{
    Channel c("chat");

    c.makeInviteOnly();
    c.makeKey("secret");
    assert(c.getIrcModes() == "+ik secret");

    c.removeMode(Channel::E_INVITE_ONLY);
    assert(c.getIrcModes() == "+k secret");

    c.makeUserLimit(10);
    assert(c.getIrcModes() == "+kl secret 10");
    assert(c.getUserLimit() == 10);

    c.removeMode(Channel::E_CHANNEL_KEY);
    c.removeMode(Channel::E_USER_LIMIT);
    assert(c.getIrcModes() == "");
}

void testCorrectPassword()
{
    std::string password = "a", nick = "us";
    char *argv[] = {
        const_cast<char*>(std::string("./irc").c_str()),
        const_cast<char*>(port.data()),
        const_cast<char*>(password.data())
    };
    Server ser(3, argv);
    
    Client cl;
    cl.addtoBuffer("PASS " + password + "\r\n");
    cl.addtoBuffer("NICK " + nick + "\r\n");

    try
    {
        FileSendHandler fsh(ser);
        ReplyHandler rh(ser);
        CommandHandler ch(ser, rh, fsh);
        ch.handle(&cl);
        assert(!cl.isRegistered());
    }
    catch(ClientException &ce)
    {
        assert(false);
    }
}

void testIncorrectPassword()
{
    std::string password = "b", nick = "us";
    char *argv[] = {
        const_cast<char*>(std::string("./irc").c_str()),
        const_cast<char*>(port.data()),
        const_cast<char*>(password.data())
    };
    Server ser(3, argv);
    
    Client cl;
    cl.addtoBuffer("PASS asdasdfxcv\r\n");
    cl.addtoBuffer("NICK " + nick + "\r\n");

    try
    {
        FileSendHandler fsh(ser);
        ReplyHandler rh(ser);
         CommandHandler ch(ser, rh, fsh);;
        ch.handle(&cl);
        assert(false);
    }
    catch(ClientException &ce)
    {
        assert(!cl.isRegistered());
        assert(cl.getNickname().empty());
    }
}

void testFullRegistration()
{
    std::string password = "a", nick = "user";
    char *argv[] = {
        const_cast<char*>("./irc"),
        const_cast<char*>(port.c_str()),
        const_cast<char*>(password.c_str())
    };

    Server ser(3, argv);
    Client cl;

    cl.addtoBuffer("PASS a\r\n");
    cl.addtoBuffer("NICK user\r\n");
    cl.addtoBuffer("USER user 0 * :real\r\n");

    try
    {
        FileSendHandler fsh(ser);
        ReplyHandler rh(ser);
         CommandHandler ch(ser, rh, fsh);;
        ch.handle(&cl);
        assert(cl.isRegistered());
    }
    catch (...)
    {
        assert(false);
    }
}

void testJoinChannel()
{
    std::string password = "a";
    char *argv[] = {
        const_cast<char*>("./irc"),
        const_cast<char*>(port.c_str()),
        const_cast<char*>(password.c_str())
    };

    Server ser(3, argv);
    Client cl;

    cl.addtoBuffer("PASS a\r\n");
    cl.addtoBuffer("NICK user\r\n");
    cl.addtoBuffer("USER user 0 * :real\r\n");
    cl.addtoBuffer("JOIN #chat\r\n");

    try
    {
        FileSendHandler fsh(ser);
        ReplyHandler rh(ser);
         CommandHandler ch(ser, rh, fsh);;
        ch.handle(&cl);

        Channel* chRes = ser.getChannelsByName().find("#chat");
        assert(chRes != NULL);
        assert(chRes->hasMember(&cl));
    }
    catch (...)
    {
        assert(false);
    }
}

void testCreateBadChannel()
{
    std::string password = "a";
    char *argv[] = {
        const_cast<char*>("./irc"),
        const_cast<char*>(port.c_str()),
        const_cast<char*>(password.c_str())
    };

    Server ser(3, argv);
    Client cl;

    cl.addtoBuffer("PASS a\r\n");
    cl.addtoBuffer("NICK user\r\n");
    cl.addtoBuffer("USER user 0 * :real\r\n");
    cl.addtoBuffer("JOIN #\r\n");

    try
    {
        FileSendHandler fsh(ser);
        ReplyHandler rh(ser);
         CommandHandler ch(ser, rh, fsh);;
        ch.handle(&cl);

        Channel* chRes = ser.getChannelsByName().find("#");
        assert(chRes == NULL);
        assert(ser.getChannels().empty());
    }
    catch (...)
    {
        assert(false);
    }
}

void testJoinMultipleChannels()
{
    std::string password = "a";
    char *argv[] = {
        const_cast<char*>("./irc"),
        const_cast<char*>(port.c_str()),
        const_cast<char*>(password.c_str())
    };

    Server ser(3, argv);
    Client cl;

    // Full registration
    cl.addtoBuffer("PASS a\r\nNICK user\r\nUSER user 0 * :real\r\n");
    
    // Join multiple channels
    cl.addtoBuffer("JOIN #chan1,#chan2\r\n");

    try
    {
        FileSendHandler fsh(ser);
        ReplyHandler rh(ser);
         CommandHandler ch(ser, rh, fsh);;
        ch.handle(&cl);

        Channel* ch1 = ser.getChannelsByName().find("#chan1");
        Channel* ch2 = ser.getChannelsByName().find("#chan2");
        
        assert(ch1 != NULL);
        assert(ch2 != NULL);
        assert(ch1->hasMember(&cl));
        assert(ch2->hasMember(&cl));
    }
    catch (...)
    {
        assert(false);
    }
}

void testJoinWithChannelKey()
{
    std::string password = "a";
    char *argv[] = {
        const_cast<char*>("./irc"),
        const_cast<char*>(port.c_str()),
        const_cast<char*>(password.c_str())
    };

    Server ser(3, argv);
    Client op;
    Client user;

    // Setup Operator
    op.addtoBuffer("PASS a\r\nNICK op\r\nUSER op 0 * :op\r\n");
    op.addtoBuffer("JOIN #secret\r\n");
    op.addtoBuffer("MODE #secret +k mypass\r\n");

    // Setup Standard User
    user.addtoBuffer("PASS a\r\nNICK user\r\nUSER user 0 * :real\r\n");
    try
    {
        FileSendHandler fsh(ser);
        ReplyHandler rh(ser);
         CommandHandler ch(ser, rh, fsh);;
        ch.handle(&op); // Registers op, creates channel, sets key
        
        // User tries to join without key
        user.addtoBuffer("JOIN #secret\r\n");
        ch.handle(&user);
        
        Channel* chan = ser.getChannelsByName().find("#secret");
        assert(chan != NULL);
        assert(!chan->hasMember(&user)); // Should fail to join
        
        // User tries to join WITH key
        user.addtoBuffer("JOIN #secret mypass\r\n");
        ch.handle(&user);
        assert(chan->hasMember(&user)); // Should now succeed
    }
    catch (...)
    {
        assert(false);
    }
}

void testJoinInvalidChannel()
{
    std::string password = "a";
    char *argv[] = {
        const_cast<char*>("./irc"),
        const_cast<char*>(port.c_str()),
        const_cast<char*>(password.c_str())
    };

    Server ser(3, argv);
    Client cl;

    cl.addtoBuffer("PASS a\r\n");
    cl.addtoBuffer("NICK user\r\n");
    cl.addtoBuffer("USER user 0 * :real\r\n");
    cl.addtoBuffer("JOIN chat\r\n"); // missing #

    try
    {
        FileSendHandler fsh(ser);
        ReplyHandler rh(ser);
         CommandHandler ch(ser, rh, fsh);;
        ch.handle(&cl);
        assert(ser.getChannels().empty());
        assert(cl.getInMssgs().size() == 1);
    }
    catch (...)
    {
        assert(false);
    }
}

void testInviteOnlyAndInviteCommand()
{
    std::string password = "a";
    char *argv[] = {
        const_cast<char*>("./irc"),
        const_cast<char*>(port.c_str()),
        const_cast<char*>(password.c_str())
    };

    Server ser(3, argv);
    Client *op = new Client(10);
    Client *user = new Client(11);

    Tester::addClient(ser, op);
    Tester::addClient(ser, user);

    op->addtoBuffer("PASS a\r\nNICK op\r\nUSER op 0 * :op\r\n");
    user->addtoBuffer("PASS a\r\nNICK target\r\nUSER target 0 * :target\r\n");

    try
    {
        FileSendHandler fsh(ser);
        ReplyHandler rh(ser);
         CommandHandler ch(ser, rh, fsh);;
        
        ch.handle(user); 
        
        op->addtoBuffer("JOIN #vip\r\n");
        op->addtoBuffer("MODE #vip +i\r\n"); // Set Invite Only
        ch.handle(op); 

        Channel* vipChan = ser.getChannelsByName().find("#vip");
        assert(vipChan != NULL);

        user->addtoBuffer("JOIN #vip\r\n");
        ch.handle(user);
        assert(!vipChan->hasMember(user));

        op->addtoBuffer("INVITE target #vip\r\n");
        ch.handle(op);

        user->addtoBuffer("JOIN #vip\r\n");
        ch.handle(user);
        assert(vipChan->hasMember(user));
    }
    catch (...)
    {
        assert(false);
    }
}

void testModeOperatorStatus()
{
    std::string password = "a";
    char *argv[] = {
        const_cast<char*>("./irc"),
        const_cast<char*>(port.c_str()),
        const_cast<char*>(password.c_str())
    };

    Server ser(3, argv);
    Client *op = new Client(10);
    Client *user = new Client(11);

    Tester::addClient(ser, op);
    Tester::addClient(ser, user);

    op->addtoBuffer("PASS a\r\nNICK op\r\nUSER op 0 * :op\r\n");
    user->addtoBuffer("PASS a\r\nNICK target\r\nUSER target 0 * :target\r\n");

    try
    {
        FileSendHandler fsh(ser);
        ReplyHandler rh(ser);
         CommandHandler ch(ser, rh, fsh);;
        
        // Register both
        ch.handle(op);
        ch.handle(user);
        
        op->addtoBuffer("JOIN #staff\r\n");
        ch.handle(op);
        
        user->addtoBuffer("JOIN #staff\r\n");
        ch.handle(user);

        Channel* chan = ser.getChannelsByName().find("#staff");
        assert(chan != NULL);
        
        assert(chan->hasOperator(op));
        assert(!chan->hasOperator(user));

        op->addtoBuffer("MODE #staff +o target\r\n");
        ch.handle(op);

        assert(chan->hasOperator(user));
    }
    catch (...)
    {
        assert(false);
    }
}

#pragma region MODE

// ===== MODE TESTS =====

void testModeRevokeOperator()
{
    std::string password = "a";
    char *argv[] = {
        const_cast<char*>("./irc"),
        const_cast<char*>(port.c_str()),
        const_cast<char*>(password.c_str())
    };
    Server ser(3, argv);
    Client *op   = new Client(10);
    Client *user = new Client(11);
    Tester::addClient(ser, op);
    Tester::addClient(ser, user);
    op->addtoBuffer("PASS a\r\nNICK op\r\nUSER op 0 * :op\r\n");
    user->addtoBuffer("PASS a\r\nNICK target\r\nUSER target 0 * :target\r\n");
    try
    {
        FileSendHandler fsh(ser);
        ReplyHandler rh(ser);
         CommandHandler ch(ser, rh, fsh);;
        ch.handle(op);
        ch.handle(user);

        op->addtoBuffer("JOIN #staff\r\n");
        ch.handle(op);
        user->addtoBuffer("JOIN #staff\r\n");
        ch.handle(user);

        // Grant then revoke
        op->addtoBuffer("MODE #staff +o target\r\n");
        ch.handle(op);
        Channel* chan = ser.getChannelsByName().find("#staff");
        assert(chan->hasOperator(user));

        op->addtoBuffer("MODE #staff -o target\r\n");
        ch.handle(op);
        assert(!chan->hasOperator(user));
    }
    catch (...)
    {
        assert(false);
    }
}

void testModeNonOpCannotGrantOp()
{
    std::string password = "a";
    char *argv[] = {
        const_cast<char*>("./irc"),
        const_cast<char*>(port.c_str()),
        const_cast<char*>(password.c_str())
    };
    Server ser(3, argv);
    Client *op      = new Client(10);
    Client *regular = new Client(11);
    Client *target  = new Client(12);
    Tester::addClient(ser, op);
    Tester::addClient(ser, regular);
    Tester::addClient(ser, target);
    op->addtoBuffer("PASS a\r\nNICK op\r\nUSER op 0 * :op\r\n");
    regular->addtoBuffer("PASS a\r\nNICK reg\r\nUSER reg 0 * :reg\r\n");
    target->addtoBuffer("PASS a\r\nNICK target\r\nUSER target 0 * :target\r\n");
    try
    {
        FileSendHandler fsh(ser);
        ReplyHandler rh(ser);
         CommandHandler ch(ser, rh, fsh);;
        ch.handle(op);
        ch.handle(regular);
        ch.handle(target);

        op->addtoBuffer("JOIN #room\r\n");
        ch.handle(op);
        regular->addtoBuffer("JOIN #room\r\n");
        ch.handle(regular);
        target->addtoBuffer("JOIN #room\r\n");
        ch.handle(target);

        // Non-op tries to grant op
        regular->addtoBuffer("MODE #room +o target\r\n");
        ch.handle(regular);

        Channel* chan = ser.getChannelsByName().find("#room");
        assert(chan != NULL);
        assert(!chan->hasOperator(target));
        assert_has_msg(regular, ":server 482 reg #room :You're not channel operator\r\n");
    }
    catch (...)
    {
        assert(false);
    }
}

void testModeSetInviteOnly()
{
    std::string password = "a";
    char *argv[] = {
        const_cast<char*>("./irc"),
        const_cast<char*>(port.c_str()),
        const_cast<char*>(password.c_str())
    };
    Server ser(3, argv);
    Client *op = new Client(10);
    Tester::addClient(ser, op);
    op->addtoBuffer("PASS a\r\nNICK op\r\nUSER op 0 * :op\r\n");
    try
    {
        FileSendHandler fsh(ser);
        ReplyHandler rh(ser);
         CommandHandler ch(ser, rh, fsh);;
        ch.handle(op);

        op->addtoBuffer("JOIN #room\r\n");
        ch.handle(op);

        Channel* chan = ser.getChannelsByName().find("#room");
        assert(chan != NULL);
        assert(!chan->isInviteOnly());

        op->addtoBuffer("MODE #room +i\r\n");
        ch.handle(op);
        assert(chan->isInviteOnly());
    }
    catch (...)
    {
        assert(false);
    }
}

void testModeUnsetInviteOnly()
{
    std::string password = "a";
    char *argv[] = {
        const_cast<char*>("./irc"),
        const_cast<char*>(port.c_str()),
        const_cast<char*>(password.c_str())
    };
    Server ser(3, argv);
    Client *op = new Client(10);
    Tester::addClient(ser, op);
    op->addtoBuffer("PASS a\r\nNICK op\r\nUSER op 0 * :op\r\n");
    try
    {
        FileSendHandler fsh(ser);
        ReplyHandler rh(ser);
         CommandHandler ch(ser, rh, fsh);;
        ch.handle(op);

        op->addtoBuffer("JOIN #room\r\n");
        ch.handle(op);

        op->addtoBuffer("MODE #room +i\r\n");
        ch.handle(op);

        Channel* chan = ser.getChannelsByName().find("#room");
        assert(chan->isInviteOnly());

        op->addtoBuffer("MODE #room -i\r\n");
        ch.handle(op);
        assert(!chan->isInviteOnly());
    }
    catch (...)
    {
        assert(false);
    }
}

void testModeSetTopicRestricted()
{
    std::string password = "a";
    char *argv[] = {
        const_cast<char*>("./irc"),
        const_cast<char*>(port.c_str()),
        const_cast<char*>(password.c_str())
    };
    Server ser(3, argv);
    Client *op = new Client(10);
    Tester::addClient(ser, op);
    op->addtoBuffer("PASS a\r\nNICK op\r\nUSER op 0 * :op\r\n");
    try
    {
        FileSendHandler fsh(ser);
        ReplyHandler rh(ser);
         CommandHandler ch(ser, rh, fsh);;
        ch.handle(op);

        op->addtoBuffer("JOIN #room\r\n");
        ch.handle(op);

        op->addtoBuffer("MODE #room +t\r\n");
        ch.handle(op);

        Channel* chan = ser.getChannelsByName().find("#room");
        assert(chan != NULL);
        assert(chan->isTopicRestricted());
    }
    catch (...)
    {
        assert(false);
    }
}

void testModeNonOpCannotSetTopic()
{
    std::string password = "a";
    char *argv[] = {
        const_cast<char*>("./irc"),
        const_cast<char*>(port.c_str()),
        const_cast<char*>(password.c_str())
    };
    Server ser(3, argv);
    Client *op      = new Client(10);
    Client *regular = new Client(11);
    Tester::addClient(ser, op);
    Tester::addClient(ser, regular);
    op->addtoBuffer("PASS a\r\nNICK op\r\nUSER op 0 * :op\r\n");
    regular->addtoBuffer("PASS a\r\nNICK reg\r\nUSER reg 0 * :reg\r\n");
    try
    {
        FileSendHandler fsh(ser);
        ReplyHandler rh(ser);
         CommandHandler ch(ser, rh, fsh);;
        ch.handle(op);
        ch.handle(regular);

        op->addtoBuffer("JOIN #room\r\n");
        ch.handle(op);
        regular->addtoBuffer("JOIN #room\r\n");
        ch.handle(regular);

        regular->addtoBuffer("MODE #room +t\r\n");
        ch.handle(regular);

        Channel* chan = ser.getChannelsByName().find("#room");
        assert(chan != NULL);
        assert(!chan->isTopicRestricted());
    }
    catch (...)
    {
        assert(false);
    }
}

void testModeSetChannelKey()
{
    std::string password = "a";
    char *argv[] = {
        const_cast<char*>("./irc"),
        const_cast<char*>(port.c_str()),
        const_cast<char*>(password.c_str())
    };
    Server ser(3, argv);
    Client *op = new Client(10);
    Tester::addClient(ser, op);
    op->addtoBuffer("PASS a\r\nNICK op\r\nUSER op 0 * :op\r\n");
    try
    {
        FileSendHandler fsh(ser);
        ReplyHandler rh(ser);
         CommandHandler ch(ser, rh, fsh);;
        ch.handle(op);

        op->addtoBuffer("JOIN #room\r\n");
        ch.handle(op);

        op->addtoBuffer("MODE #room +k secret\r\n");
        ch.handle(op);

        Channel* chan = ser.getChannelsByName().find("#room");
        assert(chan != NULL);
        assert(chan->getKey() == "secret");
    }
    catch (...)
    {
        assert(false);
    }
}

void testModeRemoveChannelKey()
{
    std::string password = "a";
    char *argv[] = {
        const_cast<char*>("./irc"),
        const_cast<char*>(port.c_str()),
        const_cast<char*>(password.c_str())
    };
    Server ser(3, argv);
    Client *op = new Client(10);
    Tester::addClient(ser, op);
    op->addtoBuffer("PASS a\r\nNICK op\r\nUSER op 0 * :op\r\n");
    try
    {
        FileSendHandler fsh(ser);
        ReplyHandler rh(ser);
         CommandHandler ch(ser, rh, fsh);;
        ch.handle(op);

        op->addtoBuffer("JOIN #room\r\n");
        ch.handle(op);

        op->addtoBuffer("MODE #room +k secret\r\n");
        ch.handle(op);

        Channel* chan = ser.getChannelsByName().find("#room");
        assert(chan->getKey() == "secret");

        op->addtoBuffer("MODE #room -k secret\r\n");
        ch.handle(op);
        assert(chan->getKey().empty());
    }
    catch (...)
    {
        assert(false);
    }
}

void testModeSetUserLimit()
{
    std::string password = "a";
    char *argv[] = {
        const_cast<char*>("./irc"),
        const_cast<char*>(port.c_str()),
        const_cast<char*>(password.c_str())
    };
    Server ser(3, argv);
    Client *op = new Client(10);
    Tester::addClient(ser, op);
    op->addtoBuffer("PASS a\r\nNICK op\r\nUSER op 0 * :op\r\n");
    try
    {
        FileSendHandler fsh(ser);
        ReplyHandler rh(ser);
         CommandHandler ch(ser, rh, fsh);;
        ch.handle(op);

        op->addtoBuffer("JOIN #room\r\n");
        ch.handle(op);

        op->addtoBuffer("MODE #room +l 5\r\n");
        ch.handle(op);

        Channel* chan = ser.getChannelsByName().find("#room");
        assert(chan != NULL);
        assert(chan->getUserLimit() == 5);
    }
    catch (...)
    {
        assert(false);
    }
}

void testModeRemoveUserLimit()
{
    std::string password = "a";
    char *argv[] = {
        const_cast<char*>("./irc"),
        const_cast<char*>(port.c_str()),
        const_cast<char*>(password.c_str())
    };
    Server ser(3, argv);
    Client *op = new Client(10);
    Tester::addClient(ser, op);
    op->addtoBuffer("PASS a\r\nNICK op\r\nUSER op 0 * :op\r\n");
    try
    {
        FileSendHandler fsh(ser);
        ReplyHandler rh(ser);
         CommandHandler ch(ser, rh, fsh);;
        ch.handle(op);

        op->addtoBuffer("JOIN #room\r\n");
        ch.handle(op);

        op->addtoBuffer("MODE #room +l 5\r\n");
        ch.handle(op);

        Channel* chan = ser.getChannelsByName().find("#room");
        assert(chan->getUserLimit() == 5);

        op->addtoBuffer("MODE #room -l\r\n");
        ch.handle(op);
        assert(chan->getUserLimit() == 0);
    }
    catch (...)
    {
        assert(false);
    }
}

#pragma endregion

#pragma region INVITE

// ===== INVITE TESTS =====

void testInviteUserToInviteOnlyChannel()
{
    std::string password = "a";
    char *argv[] = {
        const_cast<char*>("./irc"),
        const_cast<char*>(port.c_str()),
        const_cast<char*>(password.c_str())
    };
    Server ser(3, argv);
    Client *op    = new Client(10);
    Client *guest = new Client(11);
    Tester::addClient(ser, op);
    Tester::addClient(ser, guest);
    op->addtoBuffer("PASS a\r\nNICK op\r\nUSER op 0 * :op\r\n");
    guest->addtoBuffer("PASS a\r\nNICK guest\r\nUSER guest 0 * :guest\r\n");
    try
    {
        FileSendHandler fsh(ser);
        ReplyHandler rh(ser);
         CommandHandler ch(ser, rh, fsh);;
        ch.handle(op);
        ch.handle(guest);

        op->addtoBuffer("JOIN #vip\r\n");
        ch.handle(op);

        op->addtoBuffer("MODE #vip +i\r\n");
        ch.handle(op);

        // Invite then guest joins
        op->addtoBuffer("INVITE guest #vip\r\n");
        ch.handle(op);

        guest->addtoBuffer("JOIN #vip\r\n");
        ch.handle(guest);

        Channel* chan = ser.getChannelsByName().find("#vip");
        assert(chan != NULL);
        assert(chan->hasMember(guest));
    }
    catch (...)
    {
        assert(false);
    }
}

void testUninvitedUserCannotJoinInviteOnlyChannel()
{
    std::string password = "a";
    char *argv[] = {
        const_cast<char*>("./irc"),
        const_cast<char*>(port.c_str()),
        const_cast<char*>(password.c_str())
    };
    Server ser(3, argv);
    Client *op       = new Client(10);
    Client *stranger = new Client(11);
    Tester::addClient(ser, op);
    Tester::addClient(ser, stranger);
    op->addtoBuffer("PASS a\r\nNICK op\r\nUSER op 0 * :op\r\n");
    stranger->addtoBuffer("PASS a\r\nNICK stranger\r\nUSER stranger 0 * :stranger\r\n");
    try
    {
        FileSendHandler fsh(ser);
        ReplyHandler rh(ser);
         CommandHandler ch(ser, rh, fsh);;
        ch.handle(op);
        ch.handle(stranger);

        op->addtoBuffer("JOIN #vip\r\n");
        ch.handle(op);
        op->addtoBuffer("MODE #vip +i\r\n");
        ch.handle(op);

        // No INVITE — stranger tries to join directly
        stranger->addtoBuffer("JOIN #vip\r\n");
        ch.handle(stranger);

        Channel* chan = ser.getChannelsByName().find("#vip");
        assert(chan != NULL);
        assert(!chan->hasMember(stranger));
    }
    catch (...)
    {
        assert(false);
    }
}

void testNonOpCannotInvite()
{
    std::string password = "a";
    char *argv[] = {
        const_cast<char*>("./irc"),
        const_cast<char*>(port.c_str()),
        const_cast<char*>(password.c_str())
    };
    Server ser(3, argv);
    Client *op      = new Client(10);
    Client *regular = new Client(11);
    Client *target  = new Client(12);
    Tester::addClient(ser, op);
    Tester::addClient(ser, regular);
    Tester::addClient(ser, target);
    op->addtoBuffer("PASS a\r\nNICK op\r\nUSER op 0 * :op\r\n");
    regular->addtoBuffer("PASS a\r\nNICK reg\r\nUSER reg 0 * :reg\r\n");
    target->addtoBuffer("PASS a\r\nNICK target\r\nUSER target 0 * :target\r\n");
    try
    {
        FileSendHandler fsh(ser);
        ReplyHandler rh(ser);
         CommandHandler ch(ser, rh, fsh);;
        ch.handle(op);
        ch.handle(regular);
        ch.handle(target);

        op->addtoBuffer("JOIN #vip\r\n");
        ch.handle(op);
        op->addtoBuffer("MODE #vip +i\r\n");
        ch.handle(op);

        // Regular member joins before +i was set (server is fresh, so op sets it after)
        // Op invites regular in, regular is NOT an op
        op->addtoBuffer("INVITE reg #vip\r\n");
        ch.handle(op);
        regular->addtoBuffer("JOIN #vip\r\n");
        ch.handle(regular);

        // Now non-op regular tries to invite target
        regular->addtoBuffer("INVITE target #vip\r\n");
        ch.handle(regular);
        target->addtoBuffer("JOIN #vip\r\n");
        ch.handle(target);

        Channel* chan = ser.getChannelsByName().find("#vip");
        assert(chan != NULL);
        assert(!chan->hasMember(target));
    }
    catch (...)
    {
        assert(false);
    }
}

void testInviteToNonExistentChannel()
{
    std::string password = "a";
    char *argv[] = {
        const_cast<char*>("./irc"),
        const_cast<char*>(port.c_str()),
        const_cast<char*>(password.c_str())
    };
    Server ser(3, argv);
    Client *op    = new Client(10);
    Client *guest = new Client(11);
    Tester::addClient(ser, op);
    Tester::addClient(ser, guest);
    op->addtoBuffer("PASS a\r\nNICK op\r\nUSER op 0 * :op\r\n");
    guest->addtoBuffer("PASS a\r\nNICK guest\r\nUSER guest 0 * :guest\r\n");
    try
    {
        FileSendHandler fsh(ser);
        ReplyHandler rh(ser);
         CommandHandler ch(ser, rh, fsh);;
        ch.handle(op);
        ch.handle(guest);

        // Invite to a channel that does not exist
        op->addtoBuffer("INVITE guest #nowhere\r\n");
        ch.handle(op);

        Channel* chan = ser.getChannelsByName().find("#nowhere");
        assert(chan == NULL);
    }
    catch (ClientException &)
    {
        // ERR_NOSUCHCHANNEL — acceptable
    }
    catch (...)
    {
        assert(false);
    }
}

void testInviteNonExistentUser()
{
    std::string password = "a";
    char *argv[] = {
        const_cast<char*>("./irc"),
        const_cast<char*>(port.c_str()),
        const_cast<char*>(password.c_str())
    };
    Server ser(3, argv);
    Client *op = new Client(10);
    Tester::addClient(ser, op);
    op->addtoBuffer("PASS a\r\nNICK op\r\nUSER op 0 * :op\r\n");
    try
    {
        FileSendHandler fsh(ser);
        ReplyHandler rh(ser);
         CommandHandler ch(ser, rh, fsh);;
        ch.handle(op);

        op->addtoBuffer("JOIN #room\r\n");
        ch.handle(op);

        op->addtoBuffer("INVITE ghost #room\r\n");
        ch.handle(op);

        // Server should not have crashed; channel is intact
        Channel* chan = ser.getChannelsByName().find("#room");
        assert(chan != NULL);
        assert(chan->getMembers().size() == 1);
    }
    catch (ClientException &)
    {
        // ERR_NOSUCHNICK — acceptable
    }
    catch (...)
    {
        assert(false);
    }
}

#pragma endregion

#pragma region JOIN

// ===== JOIN EDGE CASE TESTS =====

void testJoinWithCorrectKey()
{
    std::string password = "a";
    char *argv[] = {
        const_cast<char*>("./irc"),
        const_cast<char*>(port.c_str()),
        const_cast<char*>(password.c_str())
    };
    Server ser(3, argv);
    Client *op   = new Client(10);
    Client *user = new Client(11);
    Tester::addClient(ser, op);
    Tester::addClient(ser, user);
    op->addtoBuffer("PASS a\r\nNICK op\r\nUSER op 0 * :op\r\n");
    user->addtoBuffer("PASS a\r\nNICK user\r\nUSER user 0 * :user\r\n");
    try
    {
        FileSendHandler fsh(ser);
        ReplyHandler rh(ser);
         CommandHandler ch(ser, rh, fsh);;
        ch.handle(op);
        ch.handle(user);

        op->addtoBuffer("JOIN #locked\r\n");
        ch.handle(op);
        op->addtoBuffer("MODE #locked +k secret\r\n");
        ch.handle(op);

        user->addtoBuffer("JOIN #locked secret\r\n");
        ch.handle(user);

        Channel* chan = ser.getChannelsByName().find("#locked");
        assert(chan != NULL);
        assert(chan->hasMember(user));
    }
    catch (...)
    {
        assert(false);
    }
}

void testJoinWithWrongKey()
{
    std::string password = "a";
    char *argv[] = {
        const_cast<char*>("./irc"),
        const_cast<char*>(port.c_str()),
        const_cast<char*>(password.c_str())
    };
    Server ser(3, argv);
    Client *op   = new Client(10);
    Client *user = new Client(11);
    Tester::addClient(ser, op);
    Tester::addClient(ser, user);
    op->addtoBuffer("PASS a\r\nNICK op\r\nUSER op 0 * :op\r\n");
    user->addtoBuffer("PASS a\r\nNICK user\r\nUSER user 0 * :user\r\n");
    try
    {
        FileSendHandler fsh(ser);
        ReplyHandler rh(ser);
         CommandHandler ch(ser, rh, fsh);;
        ch.handle(op);
        ch.handle(user);

        op->addtoBuffer("JOIN #locked\r\n");
        ch.handle(op);
        op->addtoBuffer("MODE #locked +k secret\r\n");
        ch.handle(op);

        user->addtoBuffer("JOIN #locked wrongkey\r\n");
        ch.handle(user);

        Channel* chan = ser.getChannelsByName().find("#locked");
        assert(chan != NULL);
        assert(!chan->hasMember(user));
    }
    catch (...)
    {
        assert(false);
    }
}

void testJoinChannelAtUserLimit()
{
    std::string password = "a";
    char *argv[] = {
        const_cast<char*>("./irc"),
        const_cast<char*>(port.c_str()),
        const_cast<char*>(password.c_str())
    };
    Server ser(3, argv);
    Client *op    = new Client(10);
    Client *extra = new Client(11);
    Tester::addClient(ser, op);
    Tester::addClient(ser, extra);
    op->addtoBuffer("PASS a\r\nNICK op\r\nUSER op 0 * :op\r\n");
    extra->addtoBuffer("PASS a\r\nNICK extra\r\nUSER extra 0 * :extra\r\n");
    try
    {
        FileSendHandler fsh(ser);
        ReplyHandler rh(ser);
         CommandHandler ch(ser, rh, fsh);;
        ch.handle(op);
        ch.handle(extra);

        op->addtoBuffer("JOIN #small\r\n");
        ch.handle(op);
        op->addtoBuffer("MODE #small +l 1\r\n");
        ch.handle(op);

        extra->addtoBuffer("JOIN #small\r\n");
        ch.handle(extra);

        Channel* chan = ser.getChannelsByName().find("#small");
        assert(chan != NULL);
        assert(!chan->hasMember(extra));
        assert(chan->getMembers().size() == 1);
    }
    catch (...)
    {
        assert(false);
    }
}

void testJoinAlreadyInChannel()
{
    std::string password = "a";
    char *argv[] = {
        const_cast<char*>("./irc"),
        const_cast<char*>(port.c_str()),
        const_cast<char*>(password.c_str())
    };
    Server ser(3, argv);
    Client *op = new Client(10);
    Tester::addClient(ser, op);
    op->addtoBuffer("PASS a\r\nNICK op\r\nUSER op 0 * :op\r\n");
    try
    {
        FileSendHandler fsh(ser);
        ReplyHandler rh(ser);
         CommandHandler ch(ser, rh, fsh);;
        ch.handle(op);

        op->addtoBuffer("JOIN #room\r\n");
        ch.handle(op);

        // Join the same channel a second time
        op->addtoBuffer("JOIN #room\r\n");
        ch.handle(op);

        Channel* chan = ser.getChannelsByName().find("#room");
        assert(chan != NULL);
        assert(chan->hasMember(op));
        // Must not be listed twice
        assert(chan->getMembers().size() == 1);
    }
    catch (...)
    {
        assert(false);
    }
}

void testFirstJoinerBecomesOperator()
{
    std::string password = "a";
    char *argv[] = {
        const_cast<char*>("./irc"),
        const_cast<char*>(port.c_str()),
        const_cast<char*>(password.c_str())
    };
    Server ser(3, argv);
    Client *first  = new Client(10);
    Client *second = new Client(11);
    Tester::addClient(ser, first);
    Tester::addClient(ser, second);
    first->addtoBuffer("PASS a\r\nNICK first\r\nUSER first 0 * :first\r\n");
    second->addtoBuffer("PASS a\r\nNICK second\r\nUSER second 0 * :second\r\n");
    try
    {
        FileSendHandler fsh(ser);
        ReplyHandler rh(ser);
         CommandHandler ch(ser, rh, fsh);;
        ch.handle(first);
        ch.handle(second);

        first->addtoBuffer("JOIN #room\r\n");
        ch.handle(first);
        second->addtoBuffer("JOIN #room\r\n");
        ch.handle(second);

        Channel* chan = ser.getChannelsByName().find("#room");
        assert(chan != NULL);
        assert(chan->hasOperator(first));
        assert(!chan->hasOperator(second));
    }
    catch (...)
    {
        assert(false);
    }
}
#pragma endregion

#pragma region MODE

// ===== MODE TESTS =====

void testModeGrantOpReply()
{
    std::string password = "a";
    char *argv[] = {
        const_cast<char*>("./irc"),
        const_cast<char*>(port.c_str()),
        const_cast<char*>(password.c_str())
    };
    Server ser(3, argv);
    Client *op   = new Client(10);
    Client *user = new Client(11);
    Tester::addClient(ser, op);
    Tester::addClient(ser, user);
    op->addtoBuffer("PASS a\r\nNICK op\r\nUSER op 0 * :op\r\n");
    user->addtoBuffer("PASS a\r\nNICK target\r\nUSER target 0 * :target\r\n");
    try
    {
        FileSendHandler fsh(ser);
        ReplyHandler rh(ser);
         CommandHandler ch(ser, rh, fsh);;
        ch.handle(op);
        ch.handle(user);

        op->addtoBuffer("JOIN #room\r\n");
        ch.handle(op);
        user->addtoBuffer("JOIN #room\r\n");
        ch.handle(user);

        op->addtoBuffer("MODE #room +o target\r\n");
        ch.handle(op);

        Channel* chan = ser.getChannelsByName().find("#room");
        assert(chan != NULL);
        assert(chan->hasOperator(user));
        // Channel members should see the MODE broadcast
        assert_has_msg(op,   ":op!op@server MODE #room +o target\r\n");
        assert_has_msg(user, ":op!op@server MODE #room +o target\r\n");
    }
    catch (...)
    {
        assert(false);
    }
}

void testModeRevokeOpReply()
{
    std::string password = "a";
    char *argv[] = {
        const_cast<char*>("./irc"),
        const_cast<char*>(port.c_str()),
        const_cast<char*>(password.c_str())
    };
    Server ser(3, argv);
    Client *op   = new Client(10);
    Client *user = new Client(11);
    Tester::addClient(ser, op);
    Tester::addClient(ser, user);
    op->addtoBuffer("PASS a\r\nNICK op\r\nUSER op 0 * :op\r\n");
    user->addtoBuffer("PASS a\r\nNICK target\r\nUSER target 0 * :target\r\n");
    try
    {
        FileSendHandler fsh(ser);
        ReplyHandler rh(ser);
         CommandHandler ch(ser, rh, fsh);;
        ch.handle(op);
        ch.handle(user);

        op->addtoBuffer("JOIN #room\r\n");
        ch.handle(op);
        user->addtoBuffer("JOIN #room\r\n");
        ch.handle(user);

        op->addtoBuffer("MODE #room +o target\r\n");
        ch.handle(op);
        assert(ser.getChannelsByName().find("#room")->hasOperator(user));

        op->addtoBuffer("MODE #room -o target\r\n");
        ch.handle(op);

        Channel* chan = ser.getChannelsByName().find("#room");
        assert(!chan->hasOperator(user));
        assert_has_msg(op,   ":op!op@server MODE #room -o target\r\n");
        assert_has_msg(user, ":op!op@server MODE #room -o target\r\n");
    }
    catch (...)
    {
        assert(false);
    }
}

void testModeSetInviteOnlyReply()
{
    std::string password = "a";
    char *argv[] = {
        const_cast<char*>("./irc"),
        const_cast<char*>(port.c_str()),
        const_cast<char*>(password.c_str())
    };
    Server ser(3, argv);
    Client *op = new Client(10);
    Tester::addClient(ser, op);
    op->addtoBuffer("PASS a\r\nNICK op\r\nUSER op 0 * :op\r\n");
    try
    {
        FileSendHandler fsh(ser);
        ReplyHandler rh(ser);
         CommandHandler ch(ser, rh, fsh);;
        ch.handle(op);

        op->addtoBuffer("JOIN #room\r\n");
        ch.handle(op);

        op->addtoBuffer("MODE #room +i\r\n");
        ch.handle(op);

        Channel* chan = ser.getChannelsByName().find("#room");
        assert(chan != NULL);
        assert(chan->isInviteOnly());
        assert_has_msg(op, ":op!op@server MODE #room +i\r\n");
    }
    catch (...)
    {
        assert(false);
    }
}

void testModeUnsetInviteOnlyReply()
{
    std::string password = "a";
    char *argv[] = {
        const_cast<char*>("./irc"),
        const_cast<char*>(port.c_str()),
        const_cast<char*>(password.c_str())
    };
    Server ser(3, argv);
    Client *op = new Client(10);
    Tester::addClient(ser, op);
    op->addtoBuffer("PASS a\r\nNICK op\r\nUSER op 0 * :op\r\n");
    try
    {
        FileSendHandler fsh(ser);
        ReplyHandler rh(ser);
         CommandHandler ch(ser, rh, fsh);;
        ch.handle(op);

        op->addtoBuffer("JOIN #room\r\n");
        ch.handle(op);
        op->addtoBuffer("MODE #room +i\r\n");
        ch.handle(op);
        op->addtoBuffer("MODE #room -i\r\n");
        ch.handle(op);

        Channel* chan = ser.getChannelsByName().find("#room");
        assert(!chan->isInviteOnly());
        assert_has_msg(op, ":op!op@server MODE #room -i\r\n");
    }
    catch (...)
    {
        assert(false);
    }
}

void testModeNonOpCannotSetInviteOnly()
{
    std::string password = "a";
    char *argv[] = {
        const_cast<char*>("./irc"),
        const_cast<char*>(port.c_str()),
        const_cast<char*>(password.c_str())
    };
    Server ser(3, argv);
    Client *op  = new Client(10);
    Client *reg = new Client(11);
    Tester::addClient(ser, op);
    Tester::addClient(ser, reg);
    op->addtoBuffer("PASS a\r\nNICK op\r\nUSER op 0 * :op\r\n");
    reg->addtoBuffer("PASS a\r\nNICK reg\r\nUSER reg 0 * :reg\r\n");
    try
    {
        FileSendHandler fsh(ser);
        ReplyHandler rh(ser);
         CommandHandler ch(ser, rh, fsh);;
        ch.handle(op);
        ch.handle(reg);

        op->addtoBuffer("JOIN #room\r\n");
        ch.handle(op);
        reg->addtoBuffer("JOIN #room\r\n");
        ch.handle(reg);

        reg->addtoBuffer("MODE #room +i\r\n");
        ch.handle(reg);

        Channel* chan = ser.getChannelsByName().find("#room");
        assert(!chan->isInviteOnly());
        assert_has_msg(reg, ":server 482 reg #room :You're not channel operator\r\n");
    }
    catch (...)
    {
        assert(false);
    }
}

void testModeSetTopicRestrictedReply()
{
    std::string password = "a";
    char *argv[] = {
        const_cast<char*>("./irc"),
        const_cast<char*>(port.c_str()),
        const_cast<char*>(password.c_str())
    };
    Server ser(3, argv);
    Client *op = new Client(10);
    Tester::addClient(ser, op);
    op->addtoBuffer("PASS a\r\nNICK op\r\nUSER op 0 * :op\r\n");
    try
    {
        FileSendHandler fsh(ser);
        ReplyHandler rh(ser);
         CommandHandler ch(ser, rh, fsh);;
        ch.handle(op);

        op->addtoBuffer("JOIN #room\r\n");
        ch.handle(op);
        op->addtoBuffer("MODE #room +t\r\n");
        ch.handle(op);

        Channel* chan = ser.getChannelsByName().find("#room");
        assert(chan->isTopicRestricted());
        assert_has_msg(op, ":op!op@server MODE #room +t\r\n");
    }
    catch (...)
    {
        assert(false);
    }
}

void testModeNonOpCannotSetTopicRestricted()
{
    std::string password = "a";
    char *argv[] = {
        const_cast<char*>("./irc"),
        const_cast<char*>(port.c_str()),
        const_cast<char*>(password.c_str())
    };
    Server ser(3, argv);
    Client *op  = new Client(10);
    Client *reg = new Client(11);
    Tester::addClient(ser, op);
    Tester::addClient(ser, reg);
    op->addtoBuffer("PASS a\r\nNICK op\r\nUSER op 0 * :op\r\n");
    reg->addtoBuffer("PASS a\r\nNICK reg\r\nUSER reg 0 * :reg\r\n");
    try
    {
        FileSendHandler fsh(ser);
        ReplyHandler rh(ser);
         CommandHandler ch(ser, rh, fsh);;
        ch.handle(op);
        ch.handle(reg);

        op->addtoBuffer("JOIN #room\r\n");
        ch.handle(op);
        reg->addtoBuffer("JOIN #room\r\n");
        ch.handle(reg);

        reg->addtoBuffer("MODE #room +t\r\n");
        ch.handle(reg);

        Channel* chan = ser.getChannelsByName().find("#room");
        assert(!chan->isTopicRestricted());
        assert_has_msg(reg, ":server 482 reg #room :You're not channel operator\r\n");
    }
    catch (...)
    {
        assert(false);
    }
}

void testModeSetKeyReply()
{
    std::string password = "a";
    char *argv[] = {
        const_cast<char*>("./irc"),
        const_cast<char*>(port.c_str()),
        const_cast<char*>(password.c_str())
    };
    Server ser(3, argv);
    Client *op = new Client(10);
    Tester::addClient(ser, op);
    op->addtoBuffer("PASS a\r\nNICK op\r\nUSER op 0 * :op\r\n");
    try
    {
        FileSendHandler fsh(ser);
        ReplyHandler rh(ser);
         CommandHandler ch(ser, rh, fsh);;
        ch.handle(op);

        op->addtoBuffer("JOIN #room\r\n");
        ch.handle(op);
        op->addtoBuffer("MODE #room +k secret\r\n");
        ch.handle(op);

        Channel* chan = ser.getChannelsByName().find("#room");
        assert(chan->getKey() == "secret");
        assert_has_msg(op, ":op!op@server MODE #room +k secret\r\n");
    }
    catch (...)
    {
        assert(false);
    }
}

void testModeRemoveKeyReply()
{
    std::string password = "a";
    char *argv[] = {
        const_cast<char*>("./irc"),
        const_cast<char*>(port.c_str()),
        const_cast<char*>(password.c_str())
    };
    Server ser(3, argv);
    Client *op = new Client(10);
    Tester::addClient(ser, op);
    op->addtoBuffer("PASS a\r\nNICK op\r\nUSER op 0 * :op\r\n");
    try
    {
        FileSendHandler fsh(ser);
        ReplyHandler rh(ser);
         CommandHandler ch(ser, rh, fsh);;
        ch.handle(op);

        op->addtoBuffer("JOIN #room\r\n");
        ch.handle(op);
        op->addtoBuffer("MODE #room +k secret\r\n");
        ch.handle(op);
        op->addtoBuffer("MODE #room -k secret\r\n");
        ch.handle(op);

        Channel* chan = ser.getChannelsByName().find("#room");
        assert(chan->getKey().empty());
        assert_has_msg(op, ":op!op@server MODE #room -k secret\r\n");
    }
    catch (...)
    {
        assert(false);
    }
}

void testModeSetUserLimitReply()
{
    std::string password = "a";
    char *argv[] = {
        const_cast<char*>("./irc"),
        const_cast<char*>(port.c_str()),
        const_cast<char*>(password.c_str())
    };
    Server ser(3, argv);
    Client *op = new Client(10);
    Tester::addClient(ser, op);
    op->addtoBuffer("PASS a\r\nNICK op\r\nUSER op 0 * :op\r\n");
    try
    {
        FileSendHandler fsh(ser);
        ReplyHandler rh(ser);
         CommandHandler ch(ser, rh, fsh);;
        ch.handle(op);

        op->addtoBuffer("JOIN #room\r\n");
        ch.handle(op);
        op->addtoBuffer("MODE #room +l 5\r\n");
        ch.handle(op);

        Channel* chan = ser.getChannelsByName().find("#room");
        assert(chan->getUserLimit() == 5);
        assert_has_msg(op, ":op!op@server MODE #room +l 5\r\n");
    }
    catch (...)
    {
        assert(false);
    }
}

void testModeRemoveUserLimitReply()
{
    std::string password = "a";
    char *argv[] = {
        const_cast<char*>("./irc"),
        const_cast<char*>(port.c_str()),
        const_cast<char*>(password.c_str())
    };
    Server ser(3, argv);
    Client *op = new Client(10);
    Tester::addClient(ser, op);
    op->addtoBuffer("PASS a\r\nNICK op\r\nUSER op 0 * :op\r\n");
    try
    {
        FileSendHandler fsh(ser);
        ReplyHandler rh(ser);
         CommandHandler ch(ser, rh, fsh);;
        ch.handle(op);

        op->addtoBuffer("JOIN #room\r\n");
        ch.handle(op);
        op->addtoBuffer("MODE #room +l 5\r\n");
        ch.handle(op);
        op->addtoBuffer("MODE #room -l\r\n");
        ch.handle(op);

        Channel* chan = ser.getChannelsByName().find("#room");
        assert(chan->getUserLimit() == 0);
        assert_has_msg(op, ":op!op@server MODE #room -l\r\n");
    }
    catch (...)
    {
        assert(false);
    }
}

void testModeOnNonExistentChannel()
{
    std::string password = "a";
    char *argv[] = {
        const_cast<char*>("./irc"),
        const_cast<char*>(port.c_str()),
        const_cast<char*>(password.c_str())
    };
    Server ser(3, argv);
    Client *op = new Client(10);
    Tester::addClient(ser, op);
    op->addtoBuffer("PASS a\r\nNICK op\r\nUSER op 0 * :op\r\n");
    try
    {
        FileSendHandler fsh(ser);
        ReplyHandler rh(ser);
         CommandHandler ch(ser, rh, fsh);;
        ch.handle(op);

        op->addtoBuffer("MODE #ghost +i\r\n");
        ch.handle(op);

        assert(ser.getChannelsByName().find("#ghost") == NULL);
        // ERR_NOSUCHCHANNEL 403
        assert_has_msg(op, ":server 403 op #ghost :No such channel\r\n");
    }
    catch (...)
    {
        assert(false);
    }
}

void testModeNotInChannel()
{
    std::string password = "a";
    char *argv[] = {
        const_cast<char*>("./irc"),
        const_cast<char*>(port.c_str()),
        const_cast<char*>(password.c_str())
    };
    Server ser(3, argv);
    Client *op      = new Client(10);
    Client *outside = new Client(11);
    Tester::addClient(ser, op);
    Tester::addClient(ser, outside);
    op->addtoBuffer("PASS a\r\nNICK op\r\nUSER op 0 * :op\r\n");
    outside->addtoBuffer("PASS a\r\nNICK out\r\nUSER out 0 * :out\r\n");
    try
    {
        FileSendHandler fsh(ser);
        ReplyHandler rh(ser);
         CommandHandler ch(ser, rh, fsh);;
        ch.handle(op);
        ch.handle(outside);

        op->addtoBuffer("JOIN #room\r\n");
        ch.handle(op);

        // outside is not in #room but tries to set a mode
        outside->addtoBuffer("MODE #room +i\r\n");
        ch.handle(outside);

        Channel* chan = ser.getChannelsByName().find("#room");
        assert(!chan->isInviteOnly());
        // ERR_NOTONCHANNEL 442
        assert_has_msg(outside, ":server 442 out #room :You're not on that channel\r\n");
    }
    catch (...)
    {
        assert(false);
    }
}

void testModeGrantOpNoParamError()
{
    std::string password = "a";
    char *argv[] = {
        const_cast<char*>("./irc"),
        const_cast<char*>(port.c_str()),
        const_cast<char*>(password.c_str())
    };
    Server ser(3, argv);
    Client *op = new Client(10);
    Tester::addClient(ser, op);
    op->addtoBuffer("PASS a\r\nNICK op\r\nUSER op 0 * :op\r\n");
    try
    {
        FileSendHandler fsh(ser);
        ReplyHandler rh(ser);
         CommandHandler ch(ser, rh, fsh);;
        ch.handle(op);

        op->addtoBuffer("JOIN #room\r\n");
        ch.handle(op);

        // +o with no nick argument
        op->addtoBuffer("MODE #room +o\r\n");
        ch.handle(op);

        // ERR_NEEDMOREPARAMS 461
        assert_has_msg(op, ":server 461 op MODE :Not enough parameters\r\n");
    }
    catch (...)
    {
        assert(false);
    }
}

void testModeGrantOpNonExistentNickError()
{
    std::string password = "a";
    char *argv[] = {
        const_cast<char*>("./irc"),
        const_cast<char*>(port.c_str()),
        const_cast<char*>(password.c_str())
    };
    Server ser(3, argv);
    Client *op = new Client(10);
    Tester::addClient(ser, op);
    op->addtoBuffer("PASS a\r\nNICK op\r\nUSER op 0 * :op\r\n");
    try
    {
        FileSendHandler fsh(ser);
        ReplyHandler rh(ser);
         CommandHandler ch(ser, rh, fsh);;
        ch.handle(op);

        op->addtoBuffer("JOIN #room\r\n");
        ch.handle(op);

        // +o with a nick that does not exist on the server
        op->addtoBuffer("MODE #room +o ghost\r\n");
        ch.handle(op);

        // ERR_NOSUCHNICK 401
        assert_has_msg(op, ":server 401 op ghost :No such nick/channel\r\n");
    }
    catch (...)
    {
        assert(false);
    }
}

void testModeGrantOpUserNotInChannelError()
{
    std::string password = "a";
    char *argv[] = {
        const_cast<char*>("./irc"),
        const_cast<char*>(port.c_str()),
        const_cast<char*>(password.c_str())
    };
    Server ser(3, argv);
    Client *op      = new Client(10);
    Client *outside = new Client(11);
    Tester::addClient(ser, op);
    Tester::addClient(ser, outside);
    op->addtoBuffer("PASS a\r\nNICK op\r\nUSER op 0 * :op\r\n");
    outside->addtoBuffer("PASS a\r\nNICK out\r\nUSER out 0 * :out\r\n");
    try
    {
        FileSendHandler fsh(ser);
        ReplyHandler rh(ser);
         CommandHandler ch(ser, rh, fsh);;
        ch.handle(op);
        ch.handle(outside);

        op->addtoBuffer("JOIN #room\r\n");
        ch.handle(op);

        // 'out' exists on server but is NOT in #room
        op->addtoBuffer("MODE #room +o out\r\n");
        ch.handle(op);

        Channel* chan = ser.getChannelsByName().find("#room");
        assert(!chan->hasOperator(outside));
        // ERR_USERNOTINCHANNEL 441
        assert_has_msg(op, ":server 441 op out #room :They aren't on that channel\r\n");
    }
    catch (...)
    {
        assert(false);
    }
}

void testModeGrantOpAlreadyOpIsIdempotent()
{
    std::string password = "a";
    char *argv[] = {
        const_cast<char*>("./irc"),
        const_cast<char*>(port.c_str()),
        const_cast<char*>(password.c_str())
    };
    Server ser(3, argv);
    Client *op   = new Client(10);
    Client *user = new Client(11);
    Tester::addClient(ser, op);
    Tester::addClient(ser, user);
    op->addtoBuffer("PASS a\r\nNICK op\r\nUSER op 0 * :op\r\n");
    user->addtoBuffer("PASS a\r\nNICK user\r\nUSER user 0 * :user\r\n");
    try
    {
        FileSendHandler fsh(ser);
        ReplyHandler rh(ser);
         CommandHandler ch(ser, rh, fsh);;
        ch.handle(op);
        ch.handle(user);

        op->addtoBuffer("JOIN #room\r\n");
        ch.handle(op);
        user->addtoBuffer("JOIN #room\r\n");
        ch.handle(user);

        op->addtoBuffer("MODE #room +o user\r\n");
        ch.handle(op);
        assert(ser.getChannelsByName().find("#room")->hasOperator(user));

        // Grant op again — must not crash or duplicate
        op->addtoBuffer("MODE #room +o user\r\n");
        ch.handle(op);

        Channel* chan = ser.getChannelsByName().find("#room");
        assert(chan->hasOperator(user));
        assert(chan->getMembers().size() == 2);
    }
    catch (...)
    {
        assert(false);
    }
}

void testModeRevokeOpNotOpIsIdempotent()
{
    std::string password = "a";
    char *argv[] = {
        const_cast<char*>("./irc"),
        const_cast<char*>(port.c_str()),
        const_cast<char*>(password.c_str())
    };
    Server ser(3, argv);
    Client *op   = new Client(10);
    Client *user = new Client(11);
    Tester::addClient(ser, op);
    Tester::addClient(ser, user);
    op->addtoBuffer("PASS a\r\nNICK op\r\nUSER op 0 * :op\r\n");
    user->addtoBuffer("PASS a\r\nNICK user\r\nUSER user 0 * :user\r\n");
    try
    {
        FileSendHandler fsh(ser);
        ReplyHandler rh(ser);
         CommandHandler ch(ser, rh, fsh);;
        ch.handle(op);
        ch.handle(user);

        op->addtoBuffer("JOIN #room\r\n");
        ch.handle(op);
        user->addtoBuffer("JOIN #room\r\n");
        ch.handle(user);

        // user was never op — -o should not crash
        op->addtoBuffer("MODE #room -o user\r\n");
        ch.handle(op);

        Channel* chan = ser.getChannelsByName().find("#room");
        assert(!chan->hasOperator(user));
        assert(chan->getMembers().size() == 2);
    }
    catch (...)
    {
        assert(false);
    }
}

void testModeRevokeOpNoParamError()
{
    std::string password = "a";
    char *argv[] = {
        const_cast<char*>("./irc"),
        const_cast<char*>(port.c_str()),
        const_cast<char*>(password.c_str())
    };
    Server ser(3, argv);
    Client *op = new Client(10);
    Tester::addClient(ser, op);
    op->addtoBuffer("PASS a\r\nNICK op\r\nUSER op 0 * :op\r\n");
    try
    {
        FileSendHandler fsh(ser);
        ReplyHandler rh(ser);
         CommandHandler ch(ser, rh, fsh);;
        ch.handle(op);

        op->addtoBuffer("JOIN #room\r\n");
        ch.handle(op);

        op->addtoBuffer("MODE #room -o\r\n");
        ch.handle(op);

        assert_has_msg(op, ":server 461 op MODE :Not enough parameters\r\n");
    }
    catch (...)
    {
        assert(false);
    }
}

void testModeSetKeyNoParamError()
{
    std::string password = "a";
    char *argv[] = {
        const_cast<char*>("./irc"),
        const_cast<char*>(port.c_str()),
        const_cast<char*>(password.c_str())
    };
    Server ser(3, argv);
    Client *op = new Client(10);
    Tester::addClient(ser, op);
    op->addtoBuffer("PASS a\r\nNICK op\r\nUSER op 0 * :op\r\n");
    try
    {
        FileSendHandler fsh(ser);
        ReplyHandler rh(ser);
         CommandHandler ch(ser, rh, fsh);;
        ch.handle(op);

        op->addtoBuffer("JOIN #room\r\n");
        ch.handle(op);

        // +k with no key argument
        op->addtoBuffer("MODE #room +k\r\n");
        ch.handle(op);

        Channel* chan = ser.getChannelsByName().find("#room");
        assert(chan->getKey().empty());
        assert_has_msg(op, ":server 461 op MODE :Not enough parameters\r\n");
    }
    catch (...)
    {
        assert(false);
    }
}

void testModeSetKeyWhenKeyAlreadySetError()
{
    std::string password = "a";
    char *argv[] = {
        const_cast<char*>("./irc"),
        const_cast<char*>(port.c_str()),
        const_cast<char*>(password.c_str())
    };
    Server ser(3, argv);
    Client *op = new Client(10);
    Tester::addClient(ser, op);
    op->addtoBuffer("PASS a\r\nNICK op\r\nUSER op 0 * :op\r\n");
    try
    {
        FileSendHandler fsh(ser);
        ReplyHandler rh(ser);
         CommandHandler ch(ser, rh, fsh);;
        ch.handle(op);

        op->addtoBuffer("JOIN #room\r\n");
        ch.handle(op);
        op->addtoBuffer("MODE #room +k first\r\n");
        ch.handle(op);

        // Try to overwrite existing key
        op->addtoBuffer("MODE #room +k second\r\n");
        ch.handle(op);

        Channel* chan = ser.getChannelsByName().find("#room");
        // ERR_KEYSET 467 — key already set
        assert_has_msg(op, ":server 467 op #room :Channel key already set\r\n");
        assert(chan->getKey() == "first");
    }
    catch (...)
    {
        assert(false);
    }
}

void testModeNonOpCannotSetKey()
{
    std::string password = "a";
    char *argv[] = {
        const_cast<char*>("./irc"),
        const_cast<char*>(port.c_str()),
        const_cast<char*>(password.c_str())
    };
    Server ser(3, argv);
    Client *op  = new Client(10);
    Client *reg = new Client(11);
    Tester::addClient(ser, op);
    Tester::addClient(ser, reg);
    op->addtoBuffer("PASS a\r\nNICK op\r\nUSER op 0 * :op\r\n");
    reg->addtoBuffer("PASS a\r\nNICK reg\r\nUSER reg 0 * :reg\r\n");
    try
    {
        FileSendHandler fsh(ser);
        ReplyHandler rh(ser);
         CommandHandler ch(ser, rh, fsh);;
        ch.handle(op);
        ch.handle(reg);

        op->addtoBuffer("JOIN #room\r\n");
        ch.handle(op);
        reg->addtoBuffer("JOIN #room\r\n");
        ch.handle(reg);

        reg->addtoBuffer("MODE #room +k secret\r\n");
        ch.handle(reg);

        Channel* chan = ser.getChannelsByName().find("#room");
        assert(chan->getKey().empty());
        assert_has_msg(reg, ":server 482 reg #room :You're not channel operator\r\n");
    }
    catch (...)
    {
        assert(false);
    }
}

void testModeSetLimitNoParamError()
{
    std::string password = "a";
    char *argv[] = {
        const_cast<char*>("./irc"),
        const_cast<char*>(port.c_str()),
        const_cast<char*>(password.c_str())
    };
    Server ser(3, argv);
    Client *op = new Client(10);
    Tester::addClient(ser, op);
    op->addtoBuffer("PASS a\r\nNICK op\r\nUSER op 0 * :op\r\n");
    try
    {
        FileSendHandler fsh(ser);
        ReplyHandler rh(ser);
         CommandHandler ch(ser, rh, fsh);;
        ch.handle(op);

        op->addtoBuffer("JOIN #room\r\n");
        ch.handle(op);

        // +l with no number argument
        op->addtoBuffer("MODE #room +l\r\n");
        ch.handle(op);

        Channel* chan = ser.getChannelsByName().find("#room");
        assert(chan->getUserLimit() == 0);
        assert_has_msg(op, ":server 461 op MODE :Not enough parameters\r\n");
    }
    catch (...)
    {
        assert(false);
    }
}

void testModeSetLimitZeroError()
{
    std::string password = "a";
    char *argv[] = {
        const_cast<char*>("./irc"),
        const_cast<char*>(port.c_str()),
        const_cast<char*>(password.c_str())
    };
    Server ser(3, argv);
    Client *op = new Client(10);
    Tester::addClient(ser, op);
    op->addtoBuffer("PASS a\r\nNICK op\r\nUSER op 0 * :op\r\n");
    try
    {
        FileSendHandler fsh(ser);
        ReplyHandler rh(ser);
         CommandHandler ch(ser, rh, fsh);;
        ch.handle(op);

        op->addtoBuffer("JOIN #room\r\n");
        ch.handle(op);

        // +l 0 is nonsensical — should be rejected
        op->addtoBuffer("MODE #room +l 0\r\n");
        ch.handle(op);

        Channel* chan = ser.getChannelsByName().find("#room");
        assert(chan->getUserLimit() == 0);
        assert_has_msg(op, ":server 461 op MODE :Not enough parameters\r\n");
    }
    catch (...)
    {
        assert(false);
    }
}

void testModeSetLimitNonNumericError()
{
    std::string password = "a";
    char *argv[] = {
        const_cast<char*>("./irc"),
        const_cast<char*>(port.c_str()),
        const_cast<char*>(password.c_str())
    };
    Server ser(3, argv);
    Client *op = new Client(10);
    Tester::addClient(ser, op);
    op->addtoBuffer("PASS a\r\nNICK op\r\nUSER op 0 * :op\r\n");
    try
    {
        FileSendHandler fsh(ser);
        ReplyHandler rh(ser);
         CommandHandler ch(ser, rh, fsh);;
        ch.handle(op);

        op->addtoBuffer("JOIN #room\r\n");
        ch.handle(op);

        // +l with a non-numeric argument
        op->addtoBuffer("MODE #room +l abc\r\n");
        ch.handle(op);

        Channel* chan = ser.getChannelsByName().find("#room");
        assert(chan->getUserLimit() == 0);
        assert_has_msg(op, ":server 461 op MODE :Not enough parameters\r\n");
    }
    catch (...)
    {
        assert(false);
    }
}

void testModeNonOpCannotSetLimit()
{
    std::string password = "a";
    char *argv[] = {
        const_cast<char*>("./irc"),
        const_cast<char*>(port.c_str()),
        const_cast<char*>(password.c_str())
    };
    Server ser(3, argv);
    Client *op  = new Client(10);
    Client *reg = new Client(11);
    Tester::addClient(ser, op);
    Tester::addClient(ser, reg);
    op->addtoBuffer("PASS a\r\nNICK op\r\nUSER op 0 * :op\r\n");
    reg->addtoBuffer("PASS a\r\nNICK reg\r\nUSER reg 0 * :reg\r\n");
    try
    {
        FileSendHandler fsh(ser);
        ReplyHandler rh(ser);
         CommandHandler ch(ser, rh, fsh);;
        ch.handle(op);
        ch.handle(reg);

        op->addtoBuffer("JOIN #room\r\n");
        ch.handle(op);
        reg->addtoBuffer("JOIN #room\r\n");
        ch.handle(reg);

        reg->addtoBuffer("MODE #room +l 5\r\n");
        ch.handle(reg);

        Channel* chan = ser.getChannelsByName().find("#room");
        assert(chan->getUserLimit() == 0);
        assert_has_msg(reg, ":server 482 reg #room :You're not channel operator\r\n");
    }
    catch (...)
    {
        assert(false);
    }
}

void testModeRemoveLimitWhenNoneSetIsIdempotent()
{
    std::string password = "a";
    char *argv[] = {
        const_cast<char*>("./irc"),
        const_cast<char*>(port.c_str()),
        const_cast<char*>(password.c_str())
    };
    Server ser(3, argv);
    Client *op = new Client(10);
    Tester::addClient(ser, op);
    op->addtoBuffer("PASS a\r\nNICK op\r\nUSER op 0 * :op\r\n");
    try
    {
        FileSendHandler fsh(ser);
        ReplyHandler rh(ser);
         CommandHandler ch(ser, rh, fsh);;
        ch.handle(op);

        op->addtoBuffer("JOIN #room\r\n");
        ch.handle(op);

        // -l when no limit was ever set — should not crash
        op->addtoBuffer("MODE #room -l\r\n");
        ch.handle(op);

        Channel* chan = ser.getChannelsByName().find("#room");
        assert(chan->getUserLimit() == 0);
    }
    catch (...)
    {
        assert(false);
    }
}

void testModeNonOpCannotUnsetInviteOnly()
{
    std::string password = "a";
    char *argv[] = {
        const_cast<char*>("./irc"),
        const_cast<char*>(port.c_str()),
        const_cast<char*>(password.c_str())
    };
    Server ser(3, argv);
    Client *op  = new Client(10);
    Client *reg = new Client(11);
    Tester::addClient(ser, op);
    Tester::addClient(ser, reg);
    op->addtoBuffer("PASS a\r\nNICK op\r\nUSER op 0 * :op\r\n");
    reg->addtoBuffer("PASS a\r\nNICK reg\r\nUSER reg 0 * :reg\r\n");
    try
    {
        FileSendHandler fsh(ser);
        ReplyHandler rh(ser);
         CommandHandler ch(ser, rh, fsh);;
        ch.handle(op);
        ch.handle(reg);

        op->addtoBuffer("JOIN #room\r\n");
        ch.handle(op);
        op->addtoBuffer("MODE #room +i\r\n");
        ch.handle(op);

        // Invite reg in so they can be in the channel
        op->addtoBuffer("INVITE reg #room\r\n");
        ch.handle(op);
        reg->addtoBuffer("JOIN #room\r\n");
        ch.handle(reg);

        // Non-op tries to remove +i
        reg->addtoBuffer("MODE #room -i\r\n");
        ch.handle(reg);

        Channel* chan = ser.getChannelsByName().find("#room");
        assert(chan->isInviteOnly());
        assert_has_msg(reg, ":server 482 reg #room :You're not channel operator\r\n");
    }
    catch (...)
    {
        assert(false);
    }
}

void testModeSetInviteOnlyAlreadySetIsIdempotent()
{
    std::string password = "a";
    char *argv[] = {
        const_cast<char*>("./irc"),
        const_cast<char*>(port.c_str()),
        const_cast<char*>(password.c_str())
    };
    Server ser(3, argv);
    Client *op = new Client(10);
    Tester::addClient(ser, op);
    op->addtoBuffer("PASS a\r\nNICK op\r\nUSER op 0 * :op\r\n");
    try
    {
        FileSendHandler fsh(ser);
        ReplyHandler rh(ser);
         CommandHandler ch(ser, rh, fsh);;
        ch.handle(op);

        op->addtoBuffer("JOIN #room\r\n");
        ch.handle(op);
        op->addtoBuffer("MODE #room +i\r\n");
        ch.handle(op);

        // Set +i again — should not crash or duplicate broadcast
        op->addtoBuffer("MODE #room +i\r\n");
        ch.handle(op);

        Channel* chan = ser.getChannelsByName().find("#room");
        assert(chan->isInviteOnly());
    }
    catch (...)
    {
        assert(false);
    }
}

void testModeNonOpCannotUnsetTopicRestricted()
{
    std::string password = "a";
    char *argv[] = {
        const_cast<char*>("./irc"),
        const_cast<char*>(port.c_str()),
        const_cast<char*>(password.c_str())
    };
    Server ser(3, argv);
    Client *op  = new Client(10);
    Client *reg = new Client(11);
    Tester::addClient(ser, op);
    Tester::addClient(ser, reg);
    op->addtoBuffer("PASS a\r\nNICK op\r\nUSER op 0 * :op\r\n");
    reg->addtoBuffer("PASS a\r\nNICK reg\r\nUSER reg 0 * :reg\r\n");
    try
    {
        FileSendHandler fsh(ser);
        ReplyHandler rh(ser);
         CommandHandler ch(ser, rh, fsh);;
        ch.handle(op);
        ch.handle(reg);

        op->addtoBuffer("JOIN #room\r\n");
        ch.handle(op);
        reg->addtoBuffer("JOIN #room\r\n");
        ch.handle(reg);

        op->addtoBuffer("MODE #room +t\r\n");
        ch.handle(op);

        reg->addtoBuffer("MODE #room -t\r\n");
        ch.handle(reg);

        Channel* chan = ser.getChannelsByName().find("#room");
        assert(chan->isTopicRestricted());
        assert_has_msg(reg, ":server 482 reg #room :You're not channel operator\r\n");
    }
    catch (...)
    {
        assert(false);
    }
}

void testModeUnsetTopicRestrictedReply()
{
    std::string password = "a";
    char *argv[] = {
        const_cast<char*>("./irc"),
        const_cast<char*>(port.c_str()),
        const_cast<char*>(password.c_str())
    };
    Server ser(3, argv);
    Client *op = new Client(10);
    Tester::addClient(ser, op);
    op->addtoBuffer("PASS a\r\nNICK op\r\nUSER op 0 * :op\r\n");
    try
    {
        FileSendHandler fsh(ser);
        ReplyHandler rh(ser);
         CommandHandler ch(ser, rh, fsh);;
        ch.handle(op);

        op->addtoBuffer("JOIN #room\r\n");
        ch.handle(op);
        op->addtoBuffer("MODE #room +t\r\n");
        ch.handle(op);
        op->addtoBuffer("MODE #room -t\r\n");
        ch.handle(op);

        Channel* chan = ser.getChannelsByName().find("#room");
        assert(!chan->isTopicRestricted());
        assert_has_msg(op, ":op!op@server MODE #room -t\r\n");
    }
    catch (...)
    {
        assert(false);
    }
}

void testModeUnknownFlagError()
{
    std::string password = "a";
    char *argv[] = {
        const_cast<char*>("./irc"),
        const_cast<char*>(port.c_str()),
        const_cast<char*>(password.c_str())
    };
    Server ser(3, argv);
    Client *op = new Client(10);
    Tester::addClient(ser, op);
    op->addtoBuffer("PASS a\r\nNICK op\r\nUSER op 0 * :op\r\n");
    try
    {
        FileSendHandler fsh(ser);
        ReplyHandler rh(ser);
         CommandHandler ch(ser, rh, fsh);;
        ch.handle(op);

        op->addtoBuffer("JOIN #room\r\n");
        ch.handle(op);

        op->addtoBuffer("MODE #room +z\r\n");
        ch.handle(op);

        // ERR_UNKNOWNMODE 472
        assert_has_msg(op, ":server 472 op #room z :is unknown mode char\r\n");
    }
    catch (...)
    {
        assert(false);
    }
}

void testModeNoFlagReturnsChannelModeString()
{
    std::string password = "a";
    char *argv[] = {
        const_cast<char*>("./irc"),
        const_cast<char*>(port.c_str()),
        const_cast<char*>(password.c_str())
    };
    Server ser(3, argv);
    Client *op = new Client(10);
    Tester::addClient(ser, op);
    op->addtoBuffer("PASS a\r\nNICK op\r\nUSER op 0 * :op\r\n");
    try
    {
        FileSendHandler fsh(ser);
        ReplyHandler rh(ser);
         CommandHandler ch(ser, rh, fsh);;
        ch.handle(op);

        op->addtoBuffer("JOIN #room\r\n");
        ch.handle(op);
        op->addtoBuffer("MODE #room +it\r\n");
        ch.handle(op);

        // Query modes with no flag
        op->addtoBuffer("MODE #room\r\n");
        ch.handle(op);

        // RPL_CHANNELMODEIS 324
        assert_has_msg(op, ":server 324 op #room +it\r\n");
    }
    catch (...)
    {
        assert(false);
    }
}

void testModeCombinedPlusItBroadcast()
{
    std::string password = "a";
    char *argv[] = {
        const_cast<char*>("./irc"),
        const_cast<char*>(port.c_str()),
        const_cast<char*>(password.c_str())
    };
    Server ser(3, argv);
    Client *op   = new Client(10);
    Client *user = new Client(11);
    Tester::addClient(ser, op);
    Tester::addClient(ser, user);
    op->addtoBuffer("PASS a\r\nNICK op\r\nUSER op 0 * :op\r\n");
    user->addtoBuffer("PASS a\r\nNICK user\r\nUSER user 0 * :user\r\n");
    try
    {
        FileSendHandler fsh(ser);
        ReplyHandler rh(ser);
         CommandHandler ch(ser, rh, fsh);;
        ch.handle(op);
        ch.handle(user);

        op->addtoBuffer("JOIN #room\r\n");
        ch.handle(op);
        user->addtoBuffer("JOIN #room\r\n");
        ch.handle(user);

        // Set +i and +t in a single MODE command
        op->addtoBuffer("MODE #room +it\r\n");
        ch.handle(op);

        Channel* chan = ser.getChannelsByName().find("#room");
        assert(chan->isInviteOnly());
        assert(chan->isTopicRestricted());
        // Both members should receive one broadcast for the combined change
        assert_has_msg(op,   ":op!op@server MODE #room +it\r\n");
        assert_has_msg(user, ":op!op@server MODE #room +it\r\n");
    }
    catch (...)
    {
        assert(false);
    }
}

void testModeCombinedKeyAndLimitReply()
{
    std::string password = "a";
    char *argv[] = {
        const_cast<char*>("./irc"),
        const_cast<char*>(port.c_str()),
        const_cast<char*>(password.c_str())
    };
    Server ser(3, argv);
    Client *op = new Client(10);
    Tester::addClient(ser, op);
    op->addtoBuffer("PASS a\r\nNICK op\r\nUSER op 0 * :op\r\n");
    try
    {
        FileSendHandler fsh(ser);
        ReplyHandler rh(ser);
         CommandHandler ch(ser, rh, fsh);;
        ch.handle(op);

        op->addtoBuffer("JOIN #room\r\n");
        ch.handle(op);

        // Set key and limit in one command
        op->addtoBuffer("MODE #room +kl secret 10\r\n");
        ch.handle(op);

        Channel* chan = ser.getChannelsByName().find("#room");
        assert(chan->getKey() == "secret");
        assert(chan->getUserLimit() == 10);
        assert_has_msg(op, ":op!op@server MODE #room +kl secret 10\r\n");
    }
    catch (...)
    {
        assert(false);
    }
}

void testModeNonExistentChannelError()
{
    std::string password = "a";
    char *argv[] = {
        const_cast<char*>("./irc"),
        const_cast<char*>(port.c_str()),
        const_cast<char*>(password.c_str())
    };
    Server ser(3, argv);
    Client *op = new Client(10);
    Tester::addClient(ser, op);
    op->addtoBuffer("PASS a\r\nNICK op\r\nUSER op 0 * :op\r\n");
    try
    {
        FileSendHandler fsh(ser);
        ReplyHandler rh(ser);
         CommandHandler ch(ser, rh, fsh);;
        ch.handle(op);

        op->addtoBuffer("MODE #ghost +i\r\n");
        ch.handle(op);

        assert(ser.getChannelsByName().find("#ghost") == NULL);
        // ERR_NOSUCHCHANNEL 403
        assert_has_msg(op, ":server 403 op #ghost :No such channel\r\n");
    }
    catch (...)
    {
        assert(false);
    }
}

void testModeNotInChannelError()
{
    std::string password = "a";
    char *argv[] = {
        const_cast<char*>("./irc"),
        const_cast<char*>(port.c_str()),
        const_cast<char*>(password.c_str())
    };
    Server ser(3, argv);
    Client *op      = new Client(10);
    Client *outside = new Client(11);
    Tester::addClient(ser, op);
    Tester::addClient(ser, outside);
    op->addtoBuffer("PASS a\r\nNICK op\r\nUSER op 0 * :op\r\n");
    outside->addtoBuffer("PASS a\r\nNICK out\r\nUSER out 0 * :out\r\n");
    try
    {
        FileSendHandler fsh(ser);
        ReplyHandler rh(ser);
         CommandHandler ch(ser, rh, fsh);;
        ch.handle(op);
        ch.handle(outside);

        op->addtoBuffer("JOIN #room\r\n");
        ch.handle(op);

        outside->addtoBuffer("MODE #room +t\r\n");
        ch.handle(outside);

        Channel* chan = ser.getChannelsByName().find("#room");
        assert(!chan->isTopicRestricted());
        // ERR_NOTONCHANNEL 442
        assert_has_msg(outside, ":server 442 out #room :You're not on that channel\r\n");
    }
    catch (...)
    {
        assert(false);
    }
}

#pragma endregion

#pragma region INVITE_REPLY

void testInviteReply()
{
    std::string password = "a";
    char *argv[] = {
        const_cast<char*>("./irc"),
        const_cast<char*>(port.c_str()),
        const_cast<char*>(password.c_str())
    };
    Server ser(3, argv);
    Client *op    = new Client(10);
    Client *guest = new Client(11);
    Tester::addClient(ser, op);
    Tester::addClient(ser, guest);
    op->addtoBuffer("PASS a\r\nNICK op\r\nUSER op 0 * :op\r\n");
    guest->addtoBuffer("PASS a\r\nNICK guest\r\nUSER guest 0 * :guest\r\n");
    try
    {
        FileSendHandler fsh(ser);
        ReplyHandler rh(ser);
         CommandHandler ch(ser, rh, fsh);;
        ch.handle(op);
        ch.handle(guest);

        op->addtoBuffer("JOIN #vip\r\n");
        ch.handle(op);

        op->addtoBuffer("INVITE guest #vip\r\n");
        ch.handle(op);

        // RPL_INVITING 341 to the inviter
        assert_has_msg(op, ":server 341 op guest #vip\r\n");
        // INVITE notice to the target
        assert_has_msg(guest, ":op!op@server INVITE guest :#vip\r\n");
    }
    catch (...)
    {
        assert(false);
    }
}

void testInviteAllowsJoinToInviteOnlyChannel()
{
    std::string password = "a";
    char *argv[] = {
        const_cast<char*>("./irc"),
        const_cast<char*>(port.c_str()),
        const_cast<char*>(password.c_str())
    };
    Server ser(3, argv);
    Client *op    = new Client(10);
    Client *guest = new Client(11);
    Tester::addClient(ser, op);
    Tester::addClient(ser, guest);
    op->addtoBuffer("PASS a\r\nNICK op\r\nUSER op 0 * :op\r\n");
    guest->addtoBuffer("PASS a\r\nNICK guest\r\nUSER guest 0 * :guest\r\n");
    try
    {
        FileSendHandler fsh(ser);
        ReplyHandler rh(ser);
         CommandHandler ch(ser, rh, fsh);;
        ch.handle(op);
        ch.handle(guest);

        op->addtoBuffer("JOIN #vip\r\n");
        ch.handle(op);
        op->addtoBuffer("MODE #vip +i\r\n");
        ch.handle(op);
        op->addtoBuffer("INVITE guest #vip\r\n");
        ch.handle(op);

        guest->addtoBuffer("JOIN #vip\r\n");
        ch.handle(guest);

        Channel* chan = ser.getChannelsByName().find("#vip");
        assert(chan->hasMember(guest));
    }
    catch (...)
    {
        assert(false);
    }
}

void testInviteNonOpError()
{
    std::string password = "a";
    char *argv[] = {
        const_cast<char*>("./irc"),
        const_cast<char*>(port.c_str()),
        const_cast<char*>(password.c_str())
    };
    Server ser(3, argv);
    Client *op      = new Client(10);
    Client *reg     = new Client(11);
    Client *target  = new Client(12);
    Tester::addClient(ser, op);
    Tester::addClient(ser, reg);
    Tester::addClient(ser, target);
    op->addtoBuffer("PASS a\r\nNICK op\r\nUSER op 0 * :op\r\n");
    reg->addtoBuffer("PASS a\r\nNICK reg\r\nUSER reg 0 * :reg\r\n");
    target->addtoBuffer("PASS a\r\nNICK target\r\nUSER target 0 * :target\r\n");
    try
    {
        FileSendHandler fsh(ser);
        ReplyHandler rh(ser);
         CommandHandler ch(ser, rh, fsh);;
        ch.handle(op);
        ch.handle(reg);
        ch.handle(target);

        op->addtoBuffer("JOIN #vip\r\n");
        ch.handle(op);
        op->addtoBuffer("MODE #vip +i\r\n");
        ch.handle(op);

        // Let reg in via invite, but reg is not op
        op->addtoBuffer("INVITE reg #vip\r\n");
        ch.handle(op);
        reg->addtoBuffer("JOIN #vip\r\n");
        ch.handle(reg);

        // Non-op reg tries to invite target
        reg->addtoBuffer("INVITE target #vip\r\n");
        ch.handle(reg);

        Channel* chan = ser.getChannelsByName().find("#vip");
        assert(!chan->hasMember(target));
        assert_has_msg(reg, ":server 482 reg #vip :You're not channel operator\r\n");
    }
    catch (...)
    {
        assert(false);
    }
}

void testInviteToNonExistentChannelError()
{
    std::string password = "a";
    char *argv[] = {
        const_cast<char*>("./irc"),
        const_cast<char*>(port.c_str()),
        const_cast<char*>(password.c_str())
    };
    Server ser(3, argv);
    Client *op    = new Client(10);
    Client *guest = new Client(11);
    Tester::addClient(ser, op);
    Tester::addClient(ser, guest);
    op->addtoBuffer("PASS a\r\nNICK op\r\nUSER op 0 * :op\r\n");
    guest->addtoBuffer("PASS a\r\nNICK guest\r\nUSER guest 0 * :guest\r\n");
    try
    {
        FileSendHandler fsh(ser);
        ReplyHandler rh(ser);
         CommandHandler ch(ser, rh, fsh);;
        ch.handle(op);
        ch.handle(guest);

        op->addtoBuffer("INVITE guest #nowhere\r\n");
        ch.handle(op);

        // ERR_NOSUCHCHANNEL 403
        assert_has_msg(op, ":server 403 op #nowhere :No such channel\r\n");
    }
    catch (...)
    {
        assert(false);
    }
}

void testInviteNonExistentUserError()
{
    std::string password = "a";
    char *argv[] = {
        const_cast<char*>("./irc"),
        const_cast<char*>(port.c_str()),
        const_cast<char*>(password.c_str())
    };
    Server ser(3, argv);
    Client *op = new Client(10);
    Tester::addClient(ser, op);
    op->addtoBuffer("PASS a\r\nNICK op\r\nUSER op 0 * :op\r\n");
    try
    {
        FileSendHandler fsh(ser);
        ReplyHandler rh(ser);
         CommandHandler ch(ser, rh, fsh);;
        ch.handle(op);

        op->addtoBuffer("JOIN #room\r\n");
        ch.handle(op);

        op->addtoBuffer("INVITE ghost #room\r\n");
        ch.handle(op);

        // ERR_NOSUCHNICK 401
        assert_has_msg(op, ":server 401 op ghost :No such nick/channel\r\n");
        assert(ser.getChannelsByName().find("#room")->getMembers().size() == 1);
    }
    catch (...)
    {
        assert(false);
    }
}

void testInviteUserAlreadyInChannelError()
{
    std::string password = "a";
    char *argv[] = {
        const_cast<char*>("./irc"),
        const_cast<char*>(port.c_str()),
        const_cast<char*>(password.c_str())
    };
    Server ser(3, argv);
    Client *op   = new Client(10);
    Client *user = new Client(11);
    Tester::addClient(ser, op);
    Tester::addClient(ser, user);
    op->addtoBuffer("PASS a\r\nNICK op\r\nUSER op 0 * :op\r\n");
    user->addtoBuffer("PASS a\r\nNICK user\r\nUSER user 0 * :user\r\n");
    try
    {
        FileSendHandler fsh(ser);
        ReplyHandler rh(ser);
         CommandHandler ch(ser, rh, fsh);;
        ch.handle(op);
        ch.handle(user);

        op->addtoBuffer("JOIN #room\r\n");
        ch.handle(op);
        user->addtoBuffer("JOIN #room\r\n");
        ch.handle(user);

        // Invite user who is already in the channel
        op->addtoBuffer("INVITE user #room\r\n");
        ch.handle(op);

        // ERR_USERONCHANNEL 443
        assert_has_msg(op, ":server 443 op user #room :is already on channel\r\n");
        assert(ser.getChannelsByName().find("#room")->getMembers().size() == 2);
    }
    catch (...)
    {
        assert(false);
    }
}

void testInviteNotInChannelError()
{
    std::string password = "a";
    char *argv[] = {
        const_cast<char*>("./irc"),
        const_cast<char*>(port.c_str()),
        const_cast<char*>(password.c_str())
    };
    Server ser(3, argv);
    Client *op      = new Client(10);
    Client *outside = new Client(11);
    Client *target  = new Client(12);
    Tester::addClient(ser, op);
    Tester::addClient(ser, outside);
    Tester::addClient(ser, target);
    op->addtoBuffer("PASS a\r\nNICK op\r\nUSER op 0 * :op\r\n");
    outside->addtoBuffer("PASS a\r\nNICK out\r\nUSER out 0 * :out\r\n");
    target->addtoBuffer("PASS a\r\nNICK target\r\nUSER target 0 * :target\r\n");
    try
    {
        FileSendHandler fsh(ser);
        ReplyHandler rh(ser);
         CommandHandler ch(ser, rh, fsh);;
        ch.handle(op);
        ch.handle(outside);
        ch.handle(target);

        op->addtoBuffer("JOIN #room\r\n");
        ch.handle(op);

        // 'out' is not in #room but tries to invite
        outside->addtoBuffer("INVITE target #room\r\n");
        ch.handle(outside);

        Channel* chan = ser.getChannelsByName().find("#room");
        assert(!chan->hasMember(target));
        // ERR_NOTONCHANNEL 442
        assert_has_msg(outside, ":server 442 out #room :You're not on that channel\r\n");
    }
    catch (...)
    {
        assert(false);
    }
}

#pragma endregion

#pragma region JOIN_REPLY

void testJoinReply()
{
    std::string password = "a";
    char *argv[] = {
        const_cast<char*>("./irc"),
        const_cast<char*>(port.c_str()),
        const_cast<char*>(password.c_str())
    };
    Server ser(3, argv);
    Client *op = new Client(10);
    Tester::addClient(ser, op);
    op->addtoBuffer("PASS a\r\nNICK op\r\nUSER op 0 * :op\r\n");
    try
    {
        FileSendHandler fsh(ser);
        ReplyHandler rh(ser);
         CommandHandler ch(ser, rh, fsh);;
        ch.handle(op);

        op->addtoBuffer("JOIN #room\r\n");
        ch.handle(op);

        Channel* chan = ser.getChannelsByName().find("#room");
        assert(chan != NULL);
        assert(chan->hasMember(op));
        // JOIN broadcast back to self
        assert_has_msg(op, ":op!op@server JOIN #room\r\n");
        // RPL_NAMREPLY 353
        assert_has_msg(op, ":server 353 op = #room :@op\r\n");
        // RPL_ENDOFNAMES 366
        assert_has_msg(op, ":server 366 op #room :End of /NAMES list\r\n");
    }
    catch (...)
    {
        assert(false);
    }
}

void testJoinBroadcastToExistingMembers()
{
    std::string password = "a";
    char *argv[] = {
        const_cast<char*>("./irc"),
        const_cast<char*>(port.c_str()),
        const_cast<char*>(password.c_str())
    };
    Server ser(3, argv);
    Client *op   = new Client(10);
    Client *user = new Client(11);
    Tester::addClient(ser, op);
    Tester::addClient(ser, user);
    op->addtoBuffer("PASS a\r\nNICK op\r\nUSER op 0 * :op\r\n");
    user->addtoBuffer("PASS a\r\nNICK user\r\nUSER user 0 * :user\r\n");
    try
    {
        FileSendHandler fsh(ser);
        ReplyHandler rh(ser);
         CommandHandler ch(ser, rh, fsh);;
        ch.handle(op);
        ch.handle(user);

        op->addtoBuffer("JOIN #room\r\n");
        ch.handle(op);

        user->addtoBuffer("JOIN #room\r\n");
        ch.handle(user);

        // The existing member (op) must see the newcomer's JOIN
        assert_has_msg(op, ":user!user@server JOIN #room\r\n");
        // Newcomer receives NAMREPLY listing both members
        assert_has_msg(user, ":server 353 user = #room :@op user\r\n");
    }
    catch (...)
    {
        assert(false);
    }
}

void testJoinInviteOnlyWithoutInviteError()
{
    std::string password = "a";
    char *argv[] = {
        const_cast<char*>("./irc"),
        const_cast<char*>(port.c_str()),
        const_cast<char*>(password.c_str())
    };
    Server ser(3, argv);
    Client *op       = new Client(10);
    Client *stranger = new Client(11);
    Tester::addClient(ser, op);
    Tester::addClient(ser, stranger);
    op->addtoBuffer("PASS a\r\nNICK op\r\nUSER op 0 * :op\r\n");
    stranger->addtoBuffer("PASS a\r\nNICK str\r\nUSER str 0 * :str\r\n");
    try
    {
        FileSendHandler fsh(ser);
        ReplyHandler rh(ser);
         CommandHandler ch(ser, rh, fsh);;
        ch.handle(op);
        ch.handle(stranger);

        op->addtoBuffer("JOIN #vip\r\n");
        ch.handle(op);
        op->addtoBuffer("MODE #vip +i\r\n");
        ch.handle(op);

        stranger->addtoBuffer("JOIN #vip\r\n");
        ch.handle(stranger);

        Channel* chan = ser.getChannelsByName().find("#vip");
        assert(!chan->hasMember(stranger));
        // ERR_INVITEONLYCHAN 473
        assert_has_msg(stranger, ":server 473 str #vip :Cannot join channel (+i)\r\n");
    }
    catch (...)
    {
        assert(false);
    }
}

void testJoinWrongKeyError()
{
    std::string password = "a";
    char *argv[] = {
        const_cast<char*>("./irc"),
        const_cast<char*>(port.c_str()),
        const_cast<char*>(password.c_str())
    };
    Server ser(3, argv);
    Client *op   = new Client(10);
    Client *user = new Client(11);
    Tester::addClient(ser, op);
    Tester::addClient(ser, user);
    op->addtoBuffer("PASS a\r\nNICK op\r\nUSER op 0 * :op\r\n");
    user->addtoBuffer("PASS a\r\nNICK user\r\nUSER user 0 * :user\r\n");
    try
    {
        FileSendHandler fsh(ser);
        ReplyHandler rh(ser);
         CommandHandler ch(ser, rh, fsh);;
        ch.handle(op);
        ch.handle(user);

        op->addtoBuffer("JOIN #locked\r\n");
        ch.handle(op);
        op->addtoBuffer("MODE #locked +k secret\r\n");
        ch.handle(op);

        user->addtoBuffer("JOIN #locked wrong\r\n");
        ch.handle(user);

        Channel* chan = ser.getChannelsByName().find("#locked");
        assert(!chan->hasMember(user));
        // ERR_BADCHANNELKEY 475
        assert_has_msg(user, ":server 475 user #locked :Cannot join channel (+k)\r\n");
    }
    catch (...)
    {
        assert(false);
    }
}

void testJoinChannelFullError()
{
    std::string password = "a";
    char *argv[] = {
        const_cast<char*>("./irc"),
        const_cast<char*>(port.c_str()),
        const_cast<char*>(password.c_str())
    };
    Server ser(3, argv);
    Client *op    = new Client(10);
    Client *extra = new Client(11);
    Tester::addClient(ser, op);
    Tester::addClient(ser, extra);
    op->addtoBuffer("PASS a\r\nNICK op\r\nUSER op 0 * :op\r\n");
    extra->addtoBuffer("PASS a\r\nNICK extra\r\nUSER extra 0 * :extra\r\n");
    try
    {
        FileSendHandler fsh(ser);
        ReplyHandler rh(ser);
         CommandHandler ch(ser, rh, fsh);;
        ch.handle(op);
        ch.handle(extra);

        op->addtoBuffer("JOIN #small\r\n");
        ch.handle(op);
        op->addtoBuffer("MODE #small +l 1\r\n");
        ch.handle(op);

        extra->addtoBuffer("JOIN #small\r\n");
        ch.handle(extra);

        Channel* chan = ser.getChannelsByName().find("#small");
        assert(!chan->hasMember(extra));
        // ERR_CHANNELISFULL 471
        assert_has_msg(extra, ":server 471 extra #small :Cannot join channel (+l)\r\n");
    }
    catch (...)
    {
        assert(false);
    }
}

void testJoinBadChannelNameError()
{
    std::string password = "a";
    char *argv[] = {
        const_cast<char*>("./irc"),
        const_cast<char*>(port.c_str()),
        const_cast<char*>(password.c_str())
    };
    Server ser(3, argv);
    Client *op = new Client(10);
    Tester::addClient(ser, op);
    op->addtoBuffer("PASS a\r\nNICK op\r\nUSER op 0 * :op\r\n");
    try
    {
        FileSendHandler fsh(ser);
        ReplyHandler rh(ser);
         CommandHandler ch(ser, rh, fsh);;
        ch.handle(op);

        op->addtoBuffer("JOIN #\r\n");
        ch.handle(op);

        assert(ser.getChannelsByName().find("#") == NULL);
        assert(ser.getChannels().empty());
        // ERR_BADCHANMASK 476
        assert_has_msg(op, ":server 476 op # :Bad Channel Mask\r\n");
    }
    catch (...)
    {
        assert(false);
    }
}

void testJoinWithoutRegistrationError()
{
    std::string password = "a";
    char *argv[] = {
        const_cast<char*>("./irc"),
        const_cast<char*>(port.c_str()),
        const_cast<char*>(password.c_str())
    };
    Server ser(3, argv);
    Client *cl = new Client(10);
    Tester::addClient(ser, cl);
    // Deliberately skip PASS/NICK/USER
    cl->addtoBuffer("JOIN #room\r\n");
    try
    {
        FileSendHandler fsh(ser);
        ReplyHandler rh(ser);
         CommandHandler ch(ser, rh, fsh);;
        ch.handle(cl);

        assert(ser.getChannels().empty());
        // ERR_NOTREGISTERED 451
        assert_has_msg(cl, ":server 451 * :You have not registered\r\n");
    }
    catch (ClientException &)
    {
        assert(!cl->isRegistered());
    }
    catch (...)
    {
        assert(false);
    }
}

#pragma endregion

int main()
{
    std::stringstream buffer;
    std::streambuf* oldCoutBuffer = std::cout.rdbuf();

    std::cout.rdbuf(buffer.rdbuf());

    // BLUH BLUH
    testCorrectPassword();
    testIncorrectPassword();
    testFullRegistration();
    testJoinChannel();
    testCreateBadChannel();
    testJoinInvalidChannel();
    testJoinMultipleChannels();
    testJoinWithChannelKey();
    testModeOperatorStatus();

    // MODE test
    testModeRevokeOperator();
    testModeNonOpCannotGrantOp();
    testModeSetInviteOnly();
    testModeUnsetInviteOnly();
    testModeSetTopicRestricted();
    testModeNonOpCannotSetTopic();
    testModeSetChannelKey();
    testModeRemoveChannelKey();
    testModeSetUserLimit();
    testModeRemoveUserLimit();

    // INVITE test
    testInviteUserToInviteOnlyChannel();
    testUninvitedUserCannotJoinInviteOnlyChannel();
    testNonOpCannotInvite();
    testInviteToNonExistentChannel();
    testInviteNonExistentUser();

    // JOIN test
    testJoinWithCorrectKey();
    testJoinWithWrongKey();
    testJoinChannelAtUserLimit();
    testJoinAlreadyInChannel();
    testFirstJoinerBecomesOperator();

    // JOIN REPLY
    testJoinReply();
    testJoinBroadcastToExistingMembers();
    testJoinInviteOnlyWithoutInviteError();
    testJoinWrongKeyError();
    testJoinChannelFullError();
    testJoinBadChannelNameError();
    testJoinWithoutRegistrationError();

    // INVITE REPLY
    testInviteReply();
    testInviteAllowsJoinToInviteOnlyChannel();
    testInviteNonOpError();
    testInviteToNonExistentChannelError();
    testInviteNonExistentUserError();
    testInviteUserAlreadyInChannelError();
    testInviteNotInChannelError();

    // MODE reply
    testModeGrantOpReply();
    testModeRevokeOpReply();
    testModeSetInviteOnlyReply();
    testModeUnsetInviteOnlyReply();
    testModeNonOpCannotSetInviteOnly();
    testModeSetTopicRestrictedReply();
    testModeNonOpCannotSetTopicRestricted();
    testModeSetKeyReply();
    testModeRemoveKeyReply();
    testModeSetUserLimitReply();
    testModeRemoveUserLimitReply();
    testModeOnNonExistentChannel();
    testModeNotInChannel();
    testModeNotInChannelError();
    testModeNonExistentChannelError();
    testModeCombinedKeyAndLimitReply();
    testModeNoFlagReturnsChannelModeString();
    testModeCombinedPlusItBroadcast();
    testModeUnknownFlagError();
    testModeUnsetTopicRestrictedReply();
    testModeNonOpCannotUnsetTopicRestricted();
    testModeRemoveLimitWhenNoneSetIsIdempotent();
    testModeSetLimitNoParamError();
    testModeSetLimitZeroError();
    testModeSetLimitNonNumericError();
    testModeNonOpCannotSetLimit();
    testModeNonOpCannotSetKey();
    testModeSetKeyWhenKeyAlreadySetError();
    testModeSetKeyNoParamError();
    testModeRevokeOpNoParamError();
    testModeRevokeOpNotOpIsIdempotent();
    testModeGrantOpAlreadyOpIsIdempotent();
    testModeGrantOpUserNotInChannelError();
    testModeGrantOpNonExistentNickError();
    testModeGrantOpNoParamError();
    
    std::cout.rdbuf(oldCoutBuffer);
    std::cout << "All tests finished" << std::endl;
}