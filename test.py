import socket
import struct
import time
import subprocess

class IRCClient:
    def __init__(self, host, port, log):
        self.sock = socket.socket()
        self.sock.settimeout(0.2)
        self.sock.connect((host, port))
        self.log = log

    def send(self, msg):
        self.log.write(">> " + msg + "\n")
        self.sock.send((msg + "\r\n").encode())

    def recv(self):
        time.sleep(0.1)
        data = self.sock.recv(4096).decode()
        self.log.write("<< " + data.strip() + "\n")
        return data

    def close(self):
        self.sock.close()

import time

# ---------------------------------------------------------------------------
# Helpers
# ---------------------------------------------------------------------------

def registered(client, nick="testuser", password="a"):
    client.send(f"PASS {password}")
    client.send(f"NICK {nick}")
    client.send(f"USER {nick} 0 * :{nick}")
    wait_for(client, "001")


def wait_for(client, expected, timeout=1.0):
    end = time.time() + timeout
    data = ""

    while time.time() < end:
        try:
            chunk = client.recv()
        except Exception:
            continue
        if chunk:
            data += chunk

            if callable(expected):
                if expected(data):
                    return data
            else:
                if expected in data:
                    return data

    raise AssertionError(f"Did not receive expected: {expected}")

def wait_for_all(client, expected_list, timeout=1.0):
    end = time.time() + timeout
    data = ""

    while time.time() < end:
        try:
            chunk = client.recv()
        except Exception:
            continue
        if chunk:
            data += chunk

            if all(e in data for e in expected_list):
                return data

    raise AssertionError(f"Did not receive all expected: {expected_list}")

def expect_no_message(client, unexpected, timeout=0.3):
    end = time.time() + timeout
    data = ""

    while time.time() < end:
        try:
            chunk = client.recv()
        except Exception:
            continue

        if not chunk:
            continue

        data += chunk

        if unexpected in data:
            raise AssertionError(f"Unexpected message: {unexpected}")

# ===========================================================================
# PASS
# ===========================================================================

def test_pass_wrong_password(make_client):
    """Wrong password should be rejected (464)."""

    client = make_client()

    client.send("PASS wrongpassword")
    client.send("NICK passtest")
    client.send("USER passtest 0 * :passtest")
    resp = client.recv()
    assert "464" in resp


def test_pass_missing_param(make_client):
    """PASS with no argument should return ERR_NEEDMOREPARAMS (461)."""
    client = make_client()

    client.send("PASS")
    resp = client.recv()
    assert "461" in resp


def test_pass_after_registration(make_client):
    """Sending PASS after already registered should be rejected (462)."""
    client = make_client()

    registered(client)
    client.send("PASS a")
    resp = client.recv()
    assert "462" in resp


# ===========================================================================
# NICK
# ===========================================================================

def test_nick_missing_param(make_client):
    """NICK with no argument → ERR_NONICKNAMEGIVEN (431)."""
    
    client = make_client()

    client.send("PASS a")
    client.send("NICK")

    resp = wait_for(client, "431")

    assert "431" in resp


def test_nick_invalid_characters(make_client):
    """NICK containing spaces or forbidden chars → ERR_ERRONEUSNICKNAME (432)."""
    client = make_client()

    client.send("PASS a")
    client.send("NICK badnick!")
    resp = client.recv()
    assert "432" in resp


def test_nick_change_after_registration(make_client):
    """Registered user can change their nick; others should see a NICK broadcast."""
    
    client = make_client()
    client2 = make_client()

    registered(client, nick="original")
    registered(client2, nick="observer")

    client.send("JOIN #nicktest")
    client2.send("JOIN #nicktest")

    wait_for_all(client, ["JOIN #nicktest"])
    wait_for_all(client2, ["JOIN #nicktest"])

    client.send("NICK renamed")

    resp1 = wait_for(client, "NICK")
    resp2 = wait_for(client2, "NICK")

    assert "renamed" in resp1
    assert "renamed" in resp2


def test_nick_duplicate(make_client):
    """Two clients using the same nick → ERR_NICKNAMEINUSE (433)."""
        
    client = make_client()
    client2 = make_client()
    
    registered(client, nick="dupenick")
    
    client2.send("PASS a")
    client2.send("NICK dupenick")
    resp = client2.recv()
    assert "433" in resp


# ===========================================================================
# USER
# ===========================================================================

def test_user_missing_params(make_client):
    """USER with fewer than 4 params → ERR_NEEDMOREPARAMS (461)."""
    client = make_client()

    client.send("PASS a")
    client.send("NICK usertest")
    client.send("USER usertest")
    resp = client.recv()
    assert "461" in resp


def test_user_already_registered(make_client):
    """Sending USER again after full registration → ERR_ALREADYREGISTERED (462)."""
    client = make_client()

    registered(client)
    
    client.send("USER newuser 0 * :New User")
    resp = client.recv()
    assert "462" in resp


def test_user_registration_welcome(make_client):
    """Completing registration should trigger a 001 RPL_WELCOME."""
    client = make_client()
    
    client.send("PASS a")
    client.send("NICK welcometest")
    client.send("USER welcometest 0 * :Welcome Test")
    resp = client.recv()
    assert "001" in resp


# ===========================================================================
# JOIN
# ===========================================================================

def test_join_missing_param(make_client):
    """JOIN with no argument → ERR_NEEDMOREPARAMS (461)."""
    client = make_client()
    registered(client)
    
    client.send("JOIN")
    resp = client.recv()
    assert "461" in resp


def test_join_valid_channel(make_client):
    """Successful JOIN echoes the channel name back."""
    client = make_client()
    registered(client)
    
    client.send("JOIN #general")
    resp = client.recv()
    assert "#general" in resp


def test_join_invalid_channel_name(make_client):
    """Channel names must start with #; otherwise ERR_NOSUCHCHANNEL (403)."""
    client = make_client()
    registered(client)
    
    client.send("JOIN nochannel")
    resp = client.recv()
    assert "403" in resp


def test_join_multiple_channels(make_client):
    """JOIN accepts a comma-separated list of channels."""
    client = make_client()
    registered(client)

    client.send("JOIN #multi1,#multi2")

    resp = wait_for_all(client, ["JOIN #multi1", "JOIN #multi2"])

    assert "JOIN #multi1" in resp
    assert "JOIN #multi2" in resp


def test_join_before_registration(make_client):
    """Commands before full registration should be rejected (451)."""
    client = make_client()
    client.send("JOIN #early")
    resp = client.recv()
    assert "451" in resp


# ===========================================================================
# PART
# ===========================================================================

def test_part_missing_param(make_client):
    """PART with no argument → ERR_NEEDMOREPARAMS (461)."""
    client = make_client()
    registered(client)
    
    client.send("PART")
    resp = client.recv()
    assert "461" in resp


def test_part_not_in_channel(make_client):
    """PART from existing channel without being a member → ERR_NOTONCHANNEL (442)."""
    
    creator = make_client()
    client = make_client()

    registered(creator, nick="cl1")
    registered(client, nick="cl2")

    # create the channel
    creator.send("JOIN #notjoined")
    wait_for(creator, "JOIN #notjoined")

    # now client tries to PART without joining
    client.send("PART #notjoined")
    resp = wait_for(client, "442")

    assert "442" in resp


def test_part_valid(make_client):
    """Successful PART echoes channel name."""
    client = make_client()
    registered(client)
    
    client.send("JOIN #parttest")
    
    client.send("PART #parttest")
    resp = client.recv()
    assert "#parttest" in resp


def test_part_with_message(make_client):
    """PART with a reason message should be accepted."""
    client = make_client()
    registered(client)
    
    client.send("JOIN #partmsg")
    
    client.send("PART #partmsg :Goodbye!")
    resp = client.recv()
    assert "#partmsg" in resp
    assert ":Goodbye!" in resp


def test_part_broadcasts_to_others(make_client):
    """Other members of the channel should receive the PART message."""
    client = make_client()
    client2 = make_client()
    registered(client, nick="parter")
    registered(client2, nick="watcher")
    
    
    client.send("JOIN #broadcast")
    client2.send("JOIN #broadcast")
    wait_for(client, "366")
    wait_for(client2, "366")
    

    client.send("PART #broadcast :leaving")
    resp = client2.recv()
    assert "PART" in resp
    assert "#broadcast" in resp


# ===========================================================================
# PRIVMSG
# ===========================================================================

def test_privmsg_missing_params(make_client):
    """PRIVMSG with no target →  (411)."""
    client = make_client()
    registered(client)
    
    client.send("PRIVMSG")
    resp = client.recv()
    assert "411" in resp


def test_privmsg_no_text(make_client):
    """PRIVMSG with target but no text → ERR_NOTEXTTOSEND (412)."""
    client = make_client()
    registered(client)
    
    client.send("JOIN #somewhere")
    client.send("PRIVMSG #somewhere")
    resp = client.recv()
    assert "412" in resp


def test_privmsg_channel_delivery(make_client):
    """Message sent to a channel is received by other members."""
    client = make_client()
    client2 = make_client()
    registered(client, nick="sender")
    registered(client2, nick="receiver")
    
    
    client.send("JOIN #chat")
    client2.send("JOIN #chat")
    wait_for(client, "366")
    wait_for(client2, "366")
    

    client.send("PRIVMSG #chat :Hello everyone!")
    resp = client2.recv()
    assert "Hello everyone!" in resp


def test_privmsg_received_by_sender(make_client):
    """The sender should NOT receive their own PRIVMSG echo."""

    client = make_client()
    client2 = make_client()

    registered(client, nick="selfecho")
    registered(client2, nick="peer")

    client.send("JOIN #echo")
    client2.send("JOIN #echo")

    wait_for(client, "366")
    wait_for(client2, "366")

    client.send("PRIVMSG #echo :no echo please")

    # receiver MUST get it
    resp2 = wait_for(client2, "no echo please")
    assert "echo please" in resp2

    # sender must get it
    expect_no_message(client, "selfecho!")


def test_privmsg_direct_message(make_client):
    """Direct (nick-to-nick) PRIVMSG is delivered to the target client."""
    
    client = make_client()
    client2 = make_client()

    registered(client, nick="dmsender")
    registered(client2, nick="dmreceiver")

    client.send("PRIVMSG dmreceiver :private hello")

    resp = wait_for(client2, "private hello")

    assert "PRIVMSG dmreceiver" in resp
    assert "private hello" in resp
    assert "dmsender" in resp


def test_privmsg_nonexistent_user(make_client):
    """PRIVMSG to unknown nick → ERR_NOSUCHNICK (401)."""
    client = make_client()
    registered(client)
    
    client.send("PRIVMSG ghostuser :hello?")
    resp = client.recv()
    assert "401" in resp


def test_privmsg_target_only_no_colon(make_client):
    """PRIVMSG <target> with no text (no colon) → 412 or 461."""
    client = make_client()
    registered(client, nick="u2")
    client.send("PRIVMSG #somewhere")
    resp = wait_for(client, lambda r: "412" in r or "461" in r)
    assert ("412" in resp) or ("461" in resp)


# ---------------------------------------------------------------------------
# ERR_NOTREGISTERED (451) – command before full registration
# ---------------------------------------------------------------------------

def test_privmsg_before_registration(make_client):
    """PRIVMSG before completing registration → 451."""
    client = make_client()
    client.send("PRIVMSG #test :too early")
    resp = wait_for(client, "451")
    assert "451" in resp


def test_privmsg_after_nick_before_user(make_client):
    """PRIVMSG after NICK but before USER → 451."""
    client = make_client()
    client.send("PASS a")
    client.send("NICK halfregistered")
    client.send("PRIVMSG #test :not yet")
    resp = wait_for(client, "451")
    assert "451" in resp


def test_privmsg_nonexistent_nick(make_client):
    """PRIVMSG to a nick that is not connected → 401."""
    client = make_client()
    registered(client, nick="sender1")
    client.send("PRIVMSG ghostuser :hello?")
    resp = wait_for(client, "401")
    assert "401" in resp


def test_privmsg_nonexistent_channel(make_client):
    """PRIVMSG to a channel that does not exist → 401 or 403."""
    client = make_client()
    registered(client, nick="sender2")
    client.send("PRIVMSG #nosuchroom :hello")
    resp = wait_for(client, lambda r: "401" in r or "403" in r)
    assert ("401" in resp) or ("403" in resp)


def test_privmsg_channel_delivered_to_others(make_client):
    """Message to a channel is received by all other members."""
    client  = make_client()
    client2 = make_client()
    registered(client,  nick="chansend")
    registered(client2, nick="chanrecv")

    client.send("JOIN #delivery")
    client2.send("JOIN #delivery")
    wait_for(client,  "366")
    wait_for(client2, "366")

    client.send("PRIVMSG #delivery :ping")
    resp = wait_for(client2, "ping")
    assert "ping" in resp


def test_privmsg_channel_message_includes_sender_prefix(make_client):
    """Channel PRIVMSG received by others must carry the sender's nick prefix."""
    client  = make_client()
    client2 = make_client()
    registered(client,  nick="prefixsend")
    registered(client2, nick="prefixrecv")

    client.send("JOIN #prefix")
    client2.send("JOIN #prefix")
    wait_for(client,  "366")
    wait_for(client2, "366")

    client.send("PRIVMSG #prefix :check prefix")
    resp = wait_for(client2, "check prefix")
    assert "prefixsend" in resp   # :prefixsend!… prefix expected


def test_privmsg_multiple_members_all_receive(make_client):
    """All members of a channel (not just the first) receive the message."""
    c1 = make_client()
    c2 = make_client()
    c3 = make_client()
    registered(c1, nick="mc1")
    registered(c2, nick="mc2")
    registered(c3, nick="mc3")

    for c in (c1, c2, c3):
        c.send("JOIN #multi")
    wait_for(c1, "366")
    wait_for(c2, "366")
    wait_for(c3, "366")

    c1.send("PRIVMSG #multi :broadcast")
    r2 = wait_for(c2, "broadcast")
    r3 = wait_for(c3, "broadcast")
    assert "broadcast" in r2
    assert "broadcast" in r3


def test_privmsg_after_part_not_delivered(make_client):
    """After PARTing, a client must no longer receive messages from that channel."""

    sender = make_client()
    leaver = make_client()

    registered(sender, nick="stillhere")
    registered(leaver, nick="hasparted")

    sender.send("JOIN #leavechan")
    leaver.send("JOIN #leavechan")

    wait_for(sender, "366")
    wait_for(leaver, "366")

    leaver.send("PART #leavechan")
    wait_for(leaver, "PART")

    sender.send("PRIVMSG #leavechan :after part")

    expect_no_message(leaver, "after part")


def test_privmsg_direct_delivered(make_client):
    """Direct PRIVMSG is delivered to the target client."""
    sender   = make_client()
    receiver = make_client()
    registered(sender,   nick="dmsend")
    registered(receiver, nick="dmrecv")

    sender.send("PRIVMSG dmrecv :hey there")
    resp = wait_for(receiver, "hey there")
    assert "hey there" in resp


def test_privmsg_direct_includes_sender_prefix(make_client):
    """Direct PRIVMSG received by the target must carry the sender's nick prefix."""
    sender   = make_client()
    receiver = make_client()
    registered(sender,   nick="dmprefixsend")
    registered(receiver, nick="dmprefixrecv")

    sender.send("PRIVMSG dmprefixrecv :prefix check")
    resp = wait_for(receiver, "prefix check")
    assert "dmprefixsend" in resp


def test_privmsg_direct_not_echoed_to_sender(make_client):
    """Sender of a direct PRIVMSG must not receive their own message back."""
    sender   = make_client()
    receiver = make_client()
    registered(sender,   nick="dmnoecho")
    registered(receiver, nick="dmnoechopeer")

    sender.send("PRIVMSG dmnoechopeer :whisper")
    wait_for(receiver, "whisper")   # receiver gets it

    got_echo = True
    try:
        wait_for(sender, "whisper", timeout=0.5)
    except AssertionError:
        got_echo = False

    assert not got_echo, "sender should not receive their own PRIVMSG echo"


def test_privmsg_direct_to_self(make_client):
    """PRIVMSG to one's own nick – server may deliver or return an error; must not crash."""
    client = make_client()
    registered(client, nick="selftalk")
    client.send("PRIVMSG selftalk :talking to myself")
    # Acceptable: either delivered back (some servers do this) or an error code.
    resp = wait_for(client, lambda r: ("talking to myself" in r) or ("401" in r) or ("404" in r))
    assert ("talking to myself" in resp) or ("401" in resp) or ("404" in resp)


def test_privmsg_case_insensitive_nick(make_client):
    """PRIVMSG to a nick with different casing must still be delivered."""
    
    sender   = make_client()
    receiver = make_client()

    registered(sender,   nick="CaseSend")
    registered(receiver, nick="caserecv")

    sender.send("PRIVMSG CASERECV :case test")

    resp = wait_for(receiver, "case test")

    assert "case test" in resp
    assert "PRIVMSG caserecv" in resp or "PRIVMSG CASERECV" in resp
    assert "CaseSend" in resp


def test_privmsg_case_insensitive_channel(make_client):
    """Channel names are case-insensitive; #Chan and #chan are the same channel."""

    c1 = make_client()
    c2 = make_client()

    registered(c1, nick="chancase1")
    registered(c2, nick="chancase2")

    c1.send("JOIN #CaseChannel")
    c2.send("JOIN #casechannel")

    wait_for(c1, "366")
    wait_for(c2, "366")

    c1.send("PRIVMSG #CASECHANNEL :hello mixed case")

    resp = wait_for(c2, "hello mixed case")

    assert "hello mixed case" in resp
    assert "PRIVMSG" in resp
    assert "chancase1" in resp


def test_privmsg_long_message(make_client):
    """A message near the 512-byte IRC line limit must be delivered intact."""
    sender   = make_client()
    receiver = make_client()
    registered(sender,   nick="longsend")
    registered(receiver, nick="longrecv")

    sender.send("JOIN #longmsg")
    receiver.send("JOIN #longmsg")
    wait_for(sender,   "366")
    wait_for(receiver, "366")

    # Build a message that fills most of the available payload space
    payload = "A" * 400
    sender.send(f"PRIVMSG #longmsg :{payload}")
    resp = wait_for(receiver, payload[:20])   # check at least the start arrived
    assert payload[:20] in resp


def test_privmsg_message_with_spaces(make_client):
    """Message text after the colon may contain spaces; all must be delivered."""
    sender   = make_client()
    receiver = make_client()
    registered(sender,   nick="spacesend")
    registered(receiver, nick="spacerecv")

    sender.send("JOIN #spacemsg")
    receiver.send("JOIN #spacemsg")
    wait_for(sender,   "366")
    wait_for(receiver, "366")

    sender.send("PRIVMSG #spacemsg :hello world with spaces")
    resp = wait_for(receiver, "hello world with spaces")
    assert "hello world with spaces" in resp


def test_privmsg_message_with_special_chars(make_client):
    """Message text with IRC-legal special characters must be delivered intact."""
    sender   = make_client()
    receiver = make_client()
    registered(sender,   nick="specsend")
    registered(receiver, nick="specrecv")

    sender.send("JOIN #specmsg")
    receiver.send("JOIN #specmsg")
    wait_for(sender,   "366")
    wait_for(receiver, "366")

    sender.send("PRIVMSG #specmsg :!@#$%^&*()-_=+[]{}|;',.<>?")
    resp = wait_for(receiver, "!@#")
    assert "!@#" in resp


def test_privmsg_colon_in_message_body(make_client):
    """A colon inside the message body (after the leading colon) must be preserved."""
    sender   = make_client()
    receiver = make_client()
    registered(sender,   nick="colonsend")
    registered(receiver, nick="colonrecv")

    sender.send("JOIN #colonmsg")
    receiver.send("JOIN #colonmsg")
    wait_for(sender,   "366")
    wait_for(receiver, "366")

    sender.send("PRIVMSG #colonmsg :time is 12:34:56")
    resp = wait_for(receiver, "12:34:56")
    assert "12:34:56" in resp


def test_privmsg_unicode_text(make_client):
    """UTF-8 / unicode payload must survive the round-trip."""
    sender   = make_client()
    receiver = make_client()
    registered(sender,   nick="unisend")
    registered(receiver, nick="unirecv")

    sender.send("JOIN #unicode")
    receiver.send("JOIN #unicode")
    wait_for(sender,   "366")
    wait_for(receiver, "366")

    sender.send("PRIVMSG #unicode :héllo wörld 🎉")
    resp = wait_for(receiver, "héllo")
    assert "héllo" in resp


def test_privmsg_to_disconnected_user(make_client):
    """PRIVMSG to a nick that was connected but has since disconnected → 401."""
    sender   = make_client()
    receiver = make_client()
    registered(sender,   nick="alivesend")
    registered(receiver, nick="aliverecv")

    # Disconnect the receiver
    receiver.close()
    time.sleep(0.1)

    sender.send("PRIVMSG aliverecv :are you there?")
    resp = wait_for(sender, "401")
    assert "401" in resp


def test_privmsg_channel_message_after_rejoin(make_client):
    """After PARTing and re-JOINing, client receives messages normally again."""
    sender  = make_client()
    rejoiner = make_client()
    registered(sender,   nick="rejsend")
    registered(rejoiner, nick="rejoin")

    sender.send("JOIN #rejoin")
    rejoiner.send("JOIN #rejoin")
    wait_for(sender,   "366")
    wait_for(rejoiner, "366")

    rejoiner.send("PART #rejoin")
    wait_for(rejoiner, "PART")

    rejoiner.send("JOIN #rejoin")
    wait_for(rejoiner, "366")

    sender.send("PRIVMSG #rejoin :welcome back")
    resp = wait_for(rejoiner, "welcome back")
    assert "welcome back" in resp


# ---------------------------------------------------------------------------
# Test registry
# --------------------------------------------------------------------------

# ===========================================================================
# TOPIC
# ===========================================================================

def test_topic_missing_param(make_client):
    """TOPIC with no argument → ERR_NEEDMOREPARAMS (461)."""
    client = make_client()
    registered(client)

    client.send("TOPIC")
    resp = client.recv()
    assert "461" in resp


def test_topic_not_in_channel(make_client):
    """Setting a topic in a channel the client hasn't joined → ERR_NOTONCHANNEL (442)."""
    client = make_client()
    registered(client)

    client.send("TOPIC #notjoined :Some topic")
    resp = client.recv()
    assert "442" in resp


def test_topic_set_and_get(make_client):
    """Client sets a topic; querying it should return the same text (332)."""
    client = make_client()
    registered(client)

    client.send("JOIN #topicroom")

    client.send("TOPIC #topicroom :Hello World Topic")

    client.send("TOPIC #topicroom")
    resp = client.recv()
    assert ("332" in resp) or ("Hello World Topic" in resp)


def test_topic_broadcast(make_client):
    """When a topic changes, other channel members are notified."""
    client = make_client()
    client2 = make_client()
    registered(client, nick="topicsetter")
    registered(client2, nick="topicwatcher")


    client.send("JOIN #topicbroadcast")
    client2.send("JOIN #topicbroadcast")



    client.send("TOPIC #topicbroadcast :New topic!")
    resp = client2.recv()
    assert "New topic!" in resp


def test_topic_clear(make_client):
    """Setting an empty topic should clear it."""
    client = make_client()
    registered(client)

    client.send("JOIN #cleartopic")

    client.send("TOPIC #cleartopic :initial")

    client.send("TOPIC #cleartopic :")
    resp = client.recv()
    # Server should accept the command without error
    assert ("331" in resp) or ("TOPIC" in resp)



# ===========================================================================
# CAP
# ===========================================================================

def test_cap_ls(make_client):
    """CAP LS should list supported capabilities."""
    client = make_client()
    client.send("CAP LS")
    resp = client.recv()
    assert "CAP" in resp
    assert "LS" in resp


def test_cap_end(make_client):
    """CAP END during negotiation should allow registration to continue."""
    
    client = make_client()

    client.send("CAP LS")
    wait_for(client, "CAP")   # ensure LS response arrived

    client.send("CAP END")
    client.send("PASS a")
    client.send("NICK capendtest")
    client.send("USER capendtest 0 * :capendtest")

    resp = wait_for(client, "001")

    assert "001" in resp


def test_cap_ls_302(make_client):
    """CAP LS 302 (extended list) should be handled gracefully."""
    client = make_client()
    client.send("CAP LS 302")
    resp = client.recv()
    assert "CAP" in resp


# ===========================================================================
# General / edge cases
# ===========================================================================

def test_unknown_command(make_client):
    """Unknown commands → ERR_UNKNOWNCOMMAND (421)."""
    
    client = make_client()
    registered(client)
    
    client.send("FAKECOMMAND arg1 arg2")
    
    resp = wait_for(client, "421")
    
    assert "421" in resp
    assert "FAKECOMMAND" in resp


def test_command_before_registration(make_client):
    """Protected commands before registration → ERR_NOTREGISTERED (451)."""
    client = make_client()

    client.send("PRIVMSG #test :too early")
    resp = client.recv()
    assert "451" in resp


def test_empty_message(make_client):
    """Sending a blank line should not crash the server."""
    client = make_client()
    registered(client)
    
    client.send("")
    # Server should stay alive; sending a valid command still works
    client.send("PRIVMSG #test :still alive")
    # No assertion on response — we just verify no exception is raised


def test_oversized_nick(make_client):
    """Nick longer than the IRC maximum (9 chars) → ERR_ERRONEUSNICKNAME (432)."""
    client = make_client()
    client.send("PASS a")
    client.send("NICK averylongnickname")
    resp = client.recv()
    assert "432" in resp


# ===========================================================================
# INVITE
# ===========================================================================

def test_invite_delivers_and_replies(make_client):
    """INVITE notifies the invitee and replies RPL_INVITING (341) to the inviter."""
    inviter = make_client()
    invitee = make_client()
    registered(inviter, nick="invsend")
    registered(invitee, nick="invrecv")

    inviter.send("JOIN #invitesmoke")
    wait_for(inviter, "366")

    inviter.send("INVITE invrecv #invitesmoke")

    resp_invitee = wait_for(invitee, "INVITE")
    assert "INVITE" in resp_invitee
    assert "#invitesmoke" in resp_invitee

    resp_inviter = wait_for(inviter, "341")
    assert "341" in resp_inviter


def test_invite_nonexistent_nick(make_client):
    """INVITE to an unknown nick → ERR_NOSUCHNICK (401)."""
    inviter = make_client()
    registered(inviter, nick="invghost")

    inviter.send("JOIN #ghostinvite")
    wait_for(inviter, "366")

    inviter.send("INVITE nosuchuser #ghostinvite")
    resp = wait_for(inviter, "401")
    assert "401" in resp


# ===========================================================================
# MODE
# ===========================================================================

def test_mode_query_no_flags(make_client):
    """MODE <channel> with no flags returns the current modes (324)."""
    client = make_client()
    registered(client, nick="modequery")

    client.send("JOIN #modequery")
    wait_for(client, "366")

    client.send("MODE #modequery")
    resp = wait_for(client, "324")
    assert "324" in resp
    assert "#modequery" in resp


def test_mode_non_operator_rejected(make_client):
    """A non-operator setting a channel mode → ERR_CHANOPRIVSNEEDED (482)."""
    op = make_client()
    other = make_client()
    registered(op, nick="modeop")
    registered(other, nick="modenotop")

    op.send("JOIN #modeperm")
    wait_for(op, "366")
    other.send("JOIN #modeperm")
    wait_for(other, "366")

    other.send("MODE #modeperm +i")
    resp = wait_for(other, "482")
    assert "482" in resp


def test_mode_invite_only_blocks_join(make_client):
    """+i set by the operator blocks JOIN from a non-invited client (473)."""
    op = make_client()
    other = make_client()
    registered(op, nick="inviteop")
    registered(other, nick="invitenope")

    op.send("JOIN #inviteonly")
    wait_for(op, "366")

    op.send("MODE #inviteonly +i")
    wait_for(op, "MODE #inviteonly +i")

    other.send("JOIN #inviteonly")
    resp = wait_for(other, "473")
    assert "473" in resp


def test_mode_invite_only_allows_invited_join(make_client):
    """After INVITE, a client can JOIN an invite-only (+i) channel."""
    op = make_client()
    other = make_client()
    registered(op, nick="inviteop2")
    registered(other, nick="inviteyes")

    op.send("JOIN #inviteallowed")
    wait_for(op, "366")

    op.send("MODE #inviteallowed +i")
    wait_for(op, "MODE #inviteallowed +i")

    op.send("INVITE inviteyes #inviteallowed")
    wait_for(other, "INVITE")

    other.send("JOIN #inviteallowed")
    resp = wait_for(other, "366")
    assert "366" in resp


def test_mode_channel_key_join(make_client):
    """+k sets a channel key; JOIN with the wrong key is rejected (475), correct key succeeds."""
    op = make_client()
    wrong = make_client()
    right = make_client()
    registered(op, nick="keyop")
    registered(wrong, nick="keywrong")
    registered(right, nick="keyright")

    op.send("JOIN #keychan")
    wait_for(op, "366")

    op.send("MODE #keychan +k secret")
    wait_for(op, "MODE #keychan +k")

    wrong.send("JOIN #keychan wrongkey")
    resp = wait_for(wrong, "475")
    assert "475" in resp

    right.send("JOIN #keychan secret")
    resp = wait_for(right, "366")
    assert "366" in resp


def test_mode_user_limit_join(make_client):
    """+l 1 limits a channel to 1 member; a second JOIN attempt is rejected (471)."""
    op = make_client()
    blocked = make_client()
    registered(op, nick="limitop")
    registered(blocked, nick="limitblocked")

    op.send("JOIN #limitchan")
    wait_for(op, "366")

    op.send("MODE #limitchan +l 1")
    wait_for(op, "MODE #limitchan +l")

    blocked.send("JOIN #limitchan")
    resp = wait_for(blocked, "471")
    assert "471" in resp


def test_mode_grant_operator(make_client):
    """+o <nick> grants operator status; the broadcast reflects the change."""
    op = make_client()
    promoted = make_client()
    registered(op, nick="opgrantop")
    registered(promoted, nick="opgrantnew")

    op.send("JOIN #opgrant")
    wait_for(op, "366")
    promoted.send("JOIN #opgrant")
    wait_for(promoted, "366")

    op.send("MODE #opgrant +o opgrantnew")
    resp = wait_for(op, "+o")
    assert "+o" in resp
    assert "opgrantnew" in resp


# ===========================================================================
# KICK
# ===========================================================================

def test_kick_removes_member(make_client):
    """An operator can KICK a member; the member is removed and notified."""
    op = make_client()
    target = make_client()
    registered(op, nick="kickop")
    registered(target, nick="kicktarget")

    op.send("JOIN #kicksmoke")
    wait_for(op, "366")
    target.send("JOIN #kicksmoke")
    wait_for(target, "366")

    op.send("KICK #kicksmoke kicktarget :bye")

    resp_target = wait_for(target, "KICK")
    assert "KICK" in resp_target
    assert "#kicksmoke" in resp_target

    # Target should no longer be treated as a member of the channel.
    target.send("PRIVMSG #kicksmoke :should not be delivered")
    resp = wait_for(target, "442")
    assert "442" in resp


def test_kick_non_operator_rejected(make_client):
    """A non-operator attempting KICK → ERR_CHANOPRIVSNEEDED (482)."""
    op = make_client()
    bystander = make_client()
    target = make_client()
    registered(op, nick="kickop2")
    registered(bystander, nick="kickbystander")
    registered(target, nick="kicktarget2")

    op.send("JOIN #kickperm")
    wait_for(op, "366")
    bystander.send("JOIN #kickperm")
    wait_for(bystander, "366")
    target.send("JOIN #kickperm")
    wait_for(target, "366")

    bystander.send("KICK #kickperm kicktarget2 :no")
    resp = wait_for(bystander, "482")
    assert "482" in resp


# ===========================================================================
# QUIT
# ===========================================================================

def test_quit_does_not_crash_server(make_client):
    """QUIT should be accepted without crashing or wedging the server.

    NOTE: CommandHandler::handleQuit is currently a stub - it does not
    actually disconnect the client or notify channel peers. This is a
    smoke test only (server stays alive and responsive); it does not
    verify real disconnect semantics, since those aren't implemented yet.
    """
    quitter = make_client()
    registered(quitter, nick="quitter")

    quitter.send("JOIN #quittest")
    wait_for(quitter, "366")

    quitter.send("QUIT :bye")

    other = make_client()
    registered(other, nick="afterquit")
    other.send("PING pingtoken")
    resp = wait_for(other, "PONG")
    assert "PONG" in resp


# ===========================================================================
# Regression tests
# ===========================================================================

def test_topic_set_without_leading_colon(make_client):
    """TOPIC <channel> <text-without-colon> must actually set the topic.

    Regression test: a stringstream-shadowing bug in the no-colon branch of
    handleTopic previously caused the topic to silently be set to an empty
    string instead of the text that was sent.
    """
    client = make_client()
    registered(client, nick="nocolon")

    client.send("JOIN #nocolontopic")
    wait_for(client, "366")

    client.send("TOPIC #nocolontopic someword")

    client.send("TOPIC #nocolontopic")
    resp = wait_for(client, "332")
    assert "332" in resp
    assert "someword" in resp


def test_client_hangup_does_not_starve_other_clients(make_client):
    """A client whose connection resets abruptly (RST, not a clean close)
    must not prevent the server from continuing to service other clients.

    Regression test: Server::handlePolls used to swap its handling of
    POLLERR/POLLHUP/POLLNVAL - a broken *server* socket was silently
    ignored, while a broken *client* fd caused the whole dispatch pass to
    `return` early, abandoning every other ready fd in that poll() batch.
    Since the dead fd kept reporting the same error on every subsequent
    poll(), this permanently starved any client registered after it.
    """
    victim = make_client()
    bystander = make_client()

    registered(victim, nick="hangupvictim")
    registered(bystander, nick="hangupbystander")

    # Force an abortive close (RST) instead of a graceful FIN, so the
    # server observes POLLERR/POLLHUP rather than a normal recv() == 0.
    victim.sock.setsockopt(socket.SOL_SOCKET, socket.SO_LINGER, struct.pack('ii', 1, 0))
    victim.sock.close()

    time.sleep(0.2)

    # The bystander, connected after the victim, must still be served.
    bystander.send("PING resetcheck")
    resp = wait_for(bystander, "PONG")
    assert "PONG" in resp


# ===========================================================================
# Test registry
# ===========================================================================

tests = [
    # PASS
    ("PASS wrong password",              test_pass_wrong_password),
    ("PASS missing param",               test_pass_missing_param),
    ("PASS after registration",          test_pass_after_registration),
    # NICK
    ("NICK without params",              test_nick_missing_param),
    ("NICK invalid characters",          test_nick_invalid_characters),
    ("NICK change after registration",   test_nick_change_after_registration),
    ("NICK duplicate",                   test_nick_duplicate),
    # USER
    ("USER missing params",              test_user_missing_params),
    ("USER already registered",          test_user_already_registered),
    ("USER registration welcome",        test_user_registration_welcome),
    # JOIN
    ("JOIN without params",              test_join_missing_param),
    ("JOIN valid channel",               test_join_valid_channel),
    ("JOIN invalid channel name",        test_join_invalid_channel_name),
    ("JOIN multiple channels",           test_join_multiple_channels),
    ("JOIN before registration",         test_join_before_registration),
    # PART
    ("PART missing param",               test_part_missing_param),
    ("PART not in channel",              test_part_not_in_channel),
    ("PART valid",                       test_part_valid),
    ("PART with message",                test_part_with_message),
    ("PART broadcasts to others",        test_part_broadcasts_to_others),

    # # PRIVMSG
    ("PRIVMSG missing params",                          test_privmsg_missing_params),
    ("PRIVMSG no text",                                 test_privmsg_no_text),
    ("PRIVMSG channel delivery",                        test_privmsg_channel_delivery),
    ("PRIVMSG received by sender",                      test_privmsg_received_by_sender),
    ("PRIVMSG direct message",                          test_privmsg_direct_message),
    ("PRIVMSG nonexistent user",                        test_privmsg_nonexistent_user),
    ("PRIVMSG target only no colon 412/461",            test_privmsg_target_only_no_colon),
    ("PRIVMSG before registration → 451",               test_privmsg_before_registration),
    ("PRIVMSG after NICK before USER → 451",            test_privmsg_after_nick_before_user),
    ("PRIVMSG nonexistent nick → 401",                  test_privmsg_nonexistent_nick),
    ("PRIVMSG channel delivered to others",             test_privmsg_channel_delivered_to_others),
    ("PRIVMSG channel carries sender prefix",           test_privmsg_channel_message_includes_sender_prefix),
    ("PRIVMSG all channel members receive",             test_privmsg_multiple_members_all_receive),
    ("PRIVMSG not delivered after PART",                test_privmsg_after_part_not_delivered),
    ("PRIVMSG direct delivered",                        test_privmsg_direct_delivered),
    ("PRIVMSG direct carries sender prefix",            test_privmsg_direct_includes_sender_prefix),
    ("PRIVMSG direct not echoed to sender",             test_privmsg_direct_not_echoed_to_sender),
    ("PRIVMSG direct to self",                          test_privmsg_direct_to_self),
    ("PRIVMSG case-insensitive nick",                   test_privmsg_case_insensitive_nick),
    ("PRIVMSG case-insensitive channel",                test_privmsg_case_insensitive_channel),
    ("PRIVMSG long message delivered intact",           test_privmsg_long_message),
    ("PRIVMSG message with spaces",                     test_privmsg_message_with_spaces),
    ("PRIVMSG message with special chars",              test_privmsg_message_with_special_chars),
    ("PRIVMSG colon inside body preserved",             test_privmsg_colon_in_message_body),
    ("PRIVMSG unicode text round-trip",                 test_privmsg_unicode_text),
    ("PRIVMSG to disconnected user → 401",              test_privmsg_to_disconnected_user),
    ("PRIVMSG channel message after rejoin",            test_privmsg_channel_message_after_rejoin),

    # CAP
    ("CAP LS",                           test_cap_ls),
    ("CAP END",                          test_cap_end),
    ("CAP LS 302",                       test_cap_ls_302),

    # General
    ("Unknown command",                  test_unknown_command),
    ("Command before registration",      test_command_before_registration),
    ("Empty message",                    test_empty_message),
    ("Oversized nick",                   test_oversized_nick),

    # TOPIC
    ("TOPIC missing param",              test_topic_missing_param),
    ("TOPIC not in channel",             test_topic_not_in_channel),
    ("TOPIC set and get",                test_topic_set_and_get),
    ("TOPIC broadcast",                  test_topic_broadcast),
    ("TOPIC clear",                      test_topic_clear),

    # INVITE
    ("INVITE delivers and replies 341",  test_invite_delivers_and_replies),
    ("INVITE nonexistent nick → 401",    test_invite_nonexistent_nick),

    # MODE
    ("MODE query no flags → 324",        test_mode_query_no_flags),
    ("MODE non-operator rejected → 482", test_mode_non_operator_rejected),
    ("MODE +i blocks JOIN → 473",        test_mode_invite_only_blocks_join),
    ("MODE +i allows invited JOIN",      test_mode_invite_only_allows_invited_join),
    ("MODE +k channel key JOIN",         test_mode_channel_key_join),
    ("MODE +l user limit JOIN → 471",    test_mode_user_limit_join),
    ("MODE +o grants operator",          test_mode_grant_operator),

    # KICK
    ("KICK removes member",              test_kick_removes_member),
    ("KICK non-operator rejected → 482", test_kick_non_operator_rejected),

    # QUIT
    ("QUIT does not crash server",       test_quit_does_not_crash_server),

    # Regression tests
    ("TOPIC set without leading colon",          test_topic_set_without_leading_colon),
    ("Client hangup does not starve others",     test_client_hangup_does_not_starve_other_clients),
]

def run_tests():
    client_log = open("client.log", "w")
    server_log = open("server.log", "w")

    server = subprocess.Popen(
        ["valgrind", "--leak-check=full", "--show-leak-kinds=all", "--track-origins=yes", "./irc", "6667", "a"],
        stdout=server_log,
        stderr=server_log
    )

    time.sleep(1)

    passed = 0

    for name, test in tests:
        print("Running:", name)

        clients = []

        def make_client():
            c = IRCClient("127.0.0.1", 6667, client_log)
            clients.append(c)
            return c

        try:
            test(make_client)

            print("  OK")
            passed += 1

        except Exception as e:
            print("  FAIL:", e)
            print("Check client.log")
            break

        finally:
            for c in clients:
                try:
                    c.close()
                except:
                    pass
        time.sleep(0.05)
    server.terminate()
    client_log.close()
    server_log.close()

    if passed == len(tests):
        print("tests finished")

if __name__ == "__main__":
    run_tests()