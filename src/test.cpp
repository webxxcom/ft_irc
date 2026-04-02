#include "Server.hpp"
#include <iostream>
#include <csignal>
#include <cassert>

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

volatile sig_atomic_t g_serverRunning = 1;

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
    std::string password = "a", nick = "us", port = "6667";
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
        CommandHandler ch(ser);
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
    std::string password = "b", nick = "us", port = "6667";
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
        CommandHandler ch(ser);
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
    std::string password = "a", nick = "user", port = "6667";
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
        CommandHandler ch(ser);
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
    std::string password = "a", port = "6667";
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
        CommandHandler ch(ser);
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
    std::string password = "a", port = "6667";
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
        CommandHandler ch(ser);
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
    std::string password = "a", port = "6667";
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
        CommandHandler ch(ser);
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
    std::string password = "a", port = "6667";
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
        CommandHandler ch(ser);
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
    std::string password = "a", port = "6667";
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
        CommandHandler ch(ser);
        ch.handle(&cl);
        assert(ser.getChannels().empty());
        assert(cl.getInMsg().size() == 1);
    }
    catch (...)
    {
        assert(false);
    }
}

void testInviteOnlyAndInviteCommand()
{
    std::string password = "a", port = "6667";
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
        CommandHandler ch(ser);
        
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
    std::string password = "a", port = "6667";
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
        CommandHandler ch(ser);
        
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


// ===== MODE TESTS =====

void testModeRevokeOperator()
{
    std::string password = "a", port = "6667";
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
        CommandHandler ch(ser);
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
    std::string password = "a", port = "6667";
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
        CommandHandler ch(ser);
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
    }
    catch (...)
    {
        assert(false);
    }
}

void testModeSetInviteOnly()
{
    std::string password = "a", port = "6667";
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
        CommandHandler ch(ser);
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
    std::string password = "a", port = "6667";
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
        CommandHandler ch(ser);
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
    std::string password = "a", port = "6667";
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
        CommandHandler ch(ser);
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
    std::string password = "a", port = "6667";
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
        CommandHandler ch(ser);
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
    std::string password = "a", port = "6667";
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
        CommandHandler ch(ser);
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
    std::string password = "a", port = "6667";
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
        CommandHandler ch(ser);
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
    std::string password = "a", port = "6667";
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
        CommandHandler ch(ser);
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
    std::string password = "a", port = "6667";
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
        CommandHandler ch(ser);
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

// ===== INVITE TESTS =====

void testInviteUserToInviteOnlyChannel()
{
    std::string password = "a", port = "6667";
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
        CommandHandler ch(ser);
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
    std::string password = "a", port = "6667";
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
        CommandHandler ch(ser);
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
    std::string password = "a", port = "6667";
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
        CommandHandler ch(ser);
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
    std::string password = "a", port = "6667";
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
        CommandHandler ch(ser);
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
    std::string password = "a", port = "6667";
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
        CommandHandler ch(ser);
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

// ===== JOIN EDGE CASE TESTS =====

void testJoinWithCorrectKey()
{
    std::string password = "a", port = "6667";
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
        CommandHandler ch(ser);
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
    std::string password = "a", port = "6667";
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
        CommandHandler ch(ser);
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
    std::string password = "a", port = "6667";
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
        CommandHandler ch(ser);
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
    std::string password = "a", port = "6667";
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
        CommandHandler ch(ser);
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
    std::string password = "a", port = "6667";
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
        CommandHandler ch(ser);
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

int main()
{
    std::stringstream buffer;
    std::streambuf* oldCoutBuffer = std::cout.rdbuf();

    std::cout.rdbuf(buffer.rdbuf());

    testCorrectPassword();
    testIncorrectPassword();
    testFullRegistration();
    testFirstJoinerBecomesOperator();
    testJoinAlreadyInChannel();
    testJoinChannelAtUserLimit();
    testJoinChannel();
    testCreateBadChannel();
    testJoinInvalidChannel();
    testJoinMultipleChannels();
    //testJoinWithChannelKey(); // ! implement the keys to channels
    testModeOperatorStatus();
    testModeRevokeOperator();
    //testInviteOnlyAndInviteCommand(); // ! need to store if a user was invited 
    
    std::cout.rdbuf(oldCoutBuffer);
    std::cout << "All tests finished" << std::endl;
}