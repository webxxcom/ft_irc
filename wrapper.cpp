// tiny_irc_wrapper.cpp
// C++98 minimal IRC terminal wrapper (Linux/macOS)
// Build: g++ -std=c++98 wrapper.cpp -o ircwrap -g

#include <iostream>
#include <string>
#include <sstream>
#include <vector>
#include <cstring>
#include <cstdlib>
#include <cstdio>

#include <unistd.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <netdb.h>
#include <arpa/inet.h>
#include <map>
#include <fstream>

static void sendLine(int sock, const std::string& line)
{
    std::string msg = line + "\r\n";
    send(sock, msg.c_str(), msg.size(), 0);
}

struct ActiveReceive
{
    int fd;
    std::ofstream ofs;
    long expected;
    long received;
    std::string token;
    std::string filename;

    ActiveReceive() : fd(-1), expected(0), received(0) { }
};

static ActiveReceive g_rx;

static void receiveFileChunk(int sock)
{
    if (g_rx.fd < 0)
        return;

    char buf[4096];
    int n = recv(g_rx.fd, buf, sizeof(buf), 0);

    if (n > 0)
    {
        long remain = g_rx.expected - g_rx.received;
        if (n > remain)
            n = remain;

        g_rx.ofs.write(buf, n);

        if (!g_rx.ofs)
        {
            std::cout << "\n[FILE] write failed\n";
            sendLine(sock, "FILE REJ " + g_rx.token + " :connect failed");
            close(g_rx.fd);
            g_rx.fd = -1;
            return;
        }

        g_rx.received += n;

        if (g_rx.received >= g_rx.expected)
        {
            std::cout << "\n[FILE] completed: "
                      << g_rx.filename << "\n";

            g_rx.ofs.close();
            close(g_rx.fd);
            g_rx.fd = -1;
        }
    }
    else if (n == 0)
    {
        std::cout << "\n[FILE] sender closed connection\n";
        g_rx.ofs.close();
        close(g_rx.fd);
        g_rx.fd = -1;
    }
    else
    {
        std::cout << "\n[FILE] recv error\n";
        sendLine(sock, "FILE REJ " + g_rx.token + " :connect failed");
        g_rx.ofs.close();
    close(g_rx.fd);
        g_rx.fd = -1;
    }
}

static int connectToServer(const std::string& host, const std::string& port)
{
    struct addrinfo hints;
    struct addrinfo *res, *p;

    std::memset(&hints, 0, sizeof(hints));
    hints.ai_family   = AF_UNSPEC;
    hints.ai_socktype = SOCK_STREAM;

    if (getaddrinfo(host.c_str(), port.c_str(), &hints, &res) != 0)
        return -1;

    int sock = -1;

    for (p = res; p; p = p->ai_next)
    {
        sock = socket(p->ai_family, p->ai_socktype, p->ai_protocol);
        if (sock < 0)
            continue;

        if (connect(sock, p->ai_addr, p->ai_addrlen) == 0)
            break;
        else
            std::cout << "connect failed: " << strerror(errno) << "\n";

        close(sock);
        sock = -1;
    }

    freeaddrinfo(res);
    return sock;
}

static std::string trimLeft(const std::string& s)
{
    size_t i = 0;
    while (i < s.size() && (s[i] == ' ' || s[i] == '\t'))
        ++i;
    return s.substr(i);
}

struct PendingFileRequest
{
    std::string token;
    std::string filename;
    long        filesize;
    int         port;
};

static std::map<std::string, PendingFileRequest> g_requests;

static void handleCommand(int sock, const std::string& input)
{
    std::stringstream ss(input);
    std::string cmd;
    ss >> cmd;

    if (cmd == "/nick")
    {
        std::string nick;
        ss >> nick;
        sendLine(sock, "NICK " + nick);
    }
    else if (cmd == "/user")
    {
        std::string user;
        ss >> user;
        sendLine(sock, "USER " + user + " 0 * :" + user);
    }
    else if (cmd == "/join")
    {
        std::string chan;
        ss >> chan;
        sendLine(sock, "JOIN " + chan);
    }
    else if (cmd == "/part")
    {
        std::string chan;
        ss >> chan;
        sendLine(sock, "PART " + chan);
    }
    else if (cmd == "/msg")
    {
        std::string target;
        ss >> target;

        std::string text;
        std::getline(ss, text);
        text = trimLeft(text);

        sendLine(sock, "PRIVMSG " + target + " :" + text);
    }
    else if (cmd == "/quit")
    {
        sendLine(sock, "QUIT :bye");
    }
    else if (cmd == "/send")
    {
        std::string nick, file;
        ss >> nick >> file;

        if (nick.empty() || file.empty())
        {
            std::cout << "Usage: /send <nick> <file>\n";
            return;
        }

        sendLine(sock, "FILE SN " + nick + " " + file);
    }
    else if (cmd == "/accept")
    {
        std::string token;
        ss >> token;

        if (token.empty())
        {
            std::cout << "Usage: /accept <token>\n";
            return;
        }

        std::map<std::string, PendingFileRequest>::iterator it =
            g_requests.find(token);

        if (it == g_requests.end())
        {
            std::cout << "Unknown token\n";
            return;
        }

        std::stringstream portStr;
        portStr << it->second.port;

        int fileSock = connectToServer("localhost", portStr.str());
        if (fileSock < 0)
        {
            std::cout << "Failed to connect for file transfer\n";
            sendLine(sock, "FILE REJ " + token + " :connect failed");
            return;
        }

        g_rx.fd       = fileSock;
        g_rx.expected = it->second.filesize;
        g_rx.received = 0;
        g_rx.token    = it->second.token;
        g_rx.filename = it->second.filename;

        g_rx.ofs.open(g_rx.filename.c_str(), std::ios::binary);

        if (!g_rx.ofs)
        {
            std::cout << "Cannot create file\n";
            sendLine(sock, "FILE REJ " + token + " :connect failed");
            ::close(fileSock);
            g_rx.fd = -1;
            return;
        }

        std::cout << "Connected for file transfer: "
                << it->second.filename << "\n";

        sendLine(sock, "FILE ACC " + token);
    }
    else if (cmd == "/reject")
    {
        std::string token;
        ss >> token;

        if (token.empty())
        {
            std::cout << "Usage: /reject <token>\n";
            return;
        }

        sendLine(sock, "FILE REJ " + token);
    }
    else
    {
        // send plain text raw if no slash command
        sendLine(sock, input);
    }
}

static void handleServerLine(const std::string& line)
{
    std::stringstream ss(line);
    std::string a, b, c;

    ss >> c >> a >> b;

    // FILE REQ <token> <filename> <filesize> <port>
    if (a == "FILE" && b == "REQ")
    {
        PendingFileRequest req;
        ss >> req.token >> req.filename >> req.filesize >> req.port;

        if (req.token.empty())
            return;

        g_requests[req.token] = req;

        std::cout
            << "\n[FILE REQUEST]\n"
            << " token: "    << req.token    << "\n"
            << " file: "     << req.filename << "\n"
            << " size: "     << req.filesize << " bytes\n"
            << " port: "     << req.port     << "\n"
            << "Use /accept " << req.token
            << " or /reject " << req.token << "\n";
    }
    else
        std::cout << line;
}


int main(int argc, char** argv)
{
    if (argc != 3)
    {
        std::cout << "Usage: ./ircwrap <host> <port>\n";
        return 1;
    }

    int sock = connectToServer(argv[1], argv[2]);
    if (sock < 0)
    {
        std::cerr << "Connection failed\n";
        return 1;
    }

    std::cout << "Connected to " << argv[1] << ":" << argv[2] << "\n";

    fd_set readfds;
    std::string recvBuffer;
    char buf[1024];

    while (true)
    {
        FD_ZERO(&readfds);
    
        FD_SET(sock, &readfds);
        FD_SET(STDIN_FILENO, &readfds);
    
        int maxfd = sock;
        if (STDIN_FILENO > maxfd)
            maxfd = STDIN_FILENO;
    
        if (g_rx.fd >= 0)
        {
            FD_SET(g_rx.fd, &readfds);
    
            if (g_rx.fd > maxfd)
                maxfd = g_rx.fd;
        }
    
        if (select(maxfd + 1, &readfds, NULL, NULL, NULL) < 0)
            break;
    
        // file transfer socket
        if (g_rx.fd >= 0 && FD_ISSET(g_rx.fd, &readfds))
            receiveFileChunk(sock);
    
        // stdin
        if (FD_ISSET(STDIN_FILENO, &readfds))
        {
            std::string line;
            if (!std::getline(std::cin, line))
                break;
    
            handleCommand(sock, line);
        }
    
        // irc socket
        if (FD_ISSET(sock, &readfds))
        {
            int n = recv(sock, buf, sizeof(buf) - 1, 0);
    
            if (n <= 0)
            {
                std::cout << "Disconnected.\n";
                break;
            }
    
            buf[n] = '\0';
            recvBuffer += buf;
    
            size_t pos;
            while ((pos = recvBuffer.find('\n')) != std::string::npos)
            {
                std::string line = recvBuffer.substr(0, pos);
                recvBuffer.erase(0, pos + 1);
    
                if (!line.empty() && line[line.size() - 1] == '\r')
                    line.erase(line.size() - 1);
    
                handleServerLine(line);
    
                if (line.substr(0, 5) == "PING ")
                    sendLine(sock, "PONG " + line.substr(5));
            }
        }
    }
}