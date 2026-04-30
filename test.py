import socket
import time
import subprocess

class IRCClient:
    def __init__(self, host, port, log):
        self.sock = socket.socket()
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

def test_join_missing_param(client):
    client.send("PASS a")
    client.send("NICK test")
    client.send("USER test 0 * :test")

    client.send("JOIN")
    resp = client.recv()

    assert "461" in resp

def test_nick_missing_param(client):
    client.send("PASS a")
    client.send("NICK")
    resp = client.recv()
    assert "431" in resp

tests = [
    ("JOIN without params", test_join_missing_param),
    ("NICK without params", test_nick_missing_param),
]

def run_tests():
    client_log = open("client.log", "w")
    server_log = open("server.log", "w")

    server = subprocess.Popen(
        ["./irc", "6667", "a"],
        stdout=server_log,
        stderr=server_log
    )

    time.sleep(1)

    passed = 0

    for name, test in tests:
        print("Running:", name)

        try:
            client = IRCClient("127.0.0.1", 6667, client_log)
            test(client)
            client.close()

            print("  OK")
            passed += 1

        except Exception as e:
            print("  FAIL:", e)
            print("Check client.log")
            break

    server.terminate()
    client_log.close()
    server_log.close()

    if passed == len(tests):
        print("tests finished")

if __name__ == "__main__":
    run_tests()