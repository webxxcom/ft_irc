*This project has been created as part of the 42 curriculum by rkravche, ahavrank.*

# ft_irc

## Description

ft_irc is our own implementation of an IRC server, written from scratch in C++98. The
point of the project is to understand how IRC actually works under the hood — a single
process, non-blocking I/O, one `poll()` (or equivalent) loop multiplexing every client
socket, and a hand-rolled parser for the IRC protocol — without relying on any external
networking or IRC libraries.

The server supports multiple clients connecting at once, user registration (`PASS`,
`NICK`, `USER`), channels with operator privileges, and the channel modes required by
the subject (`i`, `t`, `k`, `o`, `l`). Beyond the mandatory part, the server also
implements:

- `JOIN`, `PART`, `KICK`, `INVITE`, `TOPIC`, `MODE`, `PRIVMSG`, `PING`, `QUIT`
- A basic DCC-style direct file transfer between clients (`FILE` command)
- A small standalone IRC bot (`RespectBot`, in [bots/](bots/)) that connects to the
  server like any other client
- A minimal terminal client ([wrapper.cpp](wrapper.cpp)) for testing the server without
  needing a full IRC client installed

The server can be tested with a real IRC client (e.g. irssi, WeeChat, HexChat) or with
the included lightweight wrapper/bot for quick manual checks.

## Instructions

### Build

```sh
make
```

This builds the `irc` binary from the sources in [src/](src/) using the C++98 standard.
Other targets: `make clean`, `make fclean`, `make re`.

### Run

```sh
./irc <port> <password>
```

- `port` — the TCP port the server listens on
- `password` — the connection password clients must supply with `PASS` before
  registering

### Connect

Point any IRC client at `localhost:<port>` with the configured password, or use the
bundled test client:

```sh
c++ -std=c++98 wrapper.cpp -o ircwrap
./ircwrap
```

### Bonus bot

```sh
cd bots && make
./rb <server_ip> <port> <password>
```

## Resources

- [RFC 1459 — Internet Relay Chat Protocol](https://datatracker.ietf.org/doc/html/rfc1459)
- [RFC 2812 — IRC Client Protocol](https://datatracker.ietf.org/doc/html/rfc2812)
- [Modern IRC Client Protocol (ircdocs.horse)](https://modern.ircdocs.horse/)
- `man poll`, `man socket`, `man getaddrinfo` — non-blocking I/O and networking on Linux
- [Beej's Guide to Network Programming](https://beej.us/guide/bgnet/)

**AI usage:** We used AI assistance mainly as a rubber duck and reference tool rather
than to generate the core logic — explaining IRC protocol quirks and RFC edge cases,
helping debug specific `poll()`/socket issues, and reviewing our own code for bugs,
memory leaks, and edge cases (e.g. partial reads/writes, malformed commands, disconnect
handling). The overall architecture, command handling, and channel/mode logic were
designed and written by us.
