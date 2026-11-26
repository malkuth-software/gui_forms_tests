#!/usr/bin/env python3
"""Times the command folder against the socket console (MCP round 6, item 1).

Not a test: a measurement for the integrator. Start a GUI.Forms program with
--mcp-fcfs (so no act waits on a question), then run

  folder_timing.py <folder> <socket> [count] [command]

<folder> is the command folder the program prints (/tmp/<id>), <socket> the
socket it prints (or `program --mcp-socket`), count defaults to 100 and
command to `windows`, a cheap read every program has. It prints one line per
way of sending, in milliseconds per command:
- folder, sh: `echo '<command>' > in; cat out` in a new shell per command;
- folder, direct: the same pipes written and read from this process, so the
  shell's start-up is left out;
- folder, one line: all the commands on one line, joined by ` ; `;
- socket console: one connection, one command per line;
- socket console, one line: all the commands on one line on one connection.
The folder's cost should be the pipe's own: "folder, direct" close to
"socket console", and "folder, one line" close to "socket console, one line".
"""

import socket
import subprocess
import sys
import time

PROMPT = b"gui-forms> "
TIMEOUT = 60.0


def folder_reply(folder):
    """One reply from out: read until its status line (`. ok`, `. error ...`)."""
    with open(folder + "/out", "rb") as out:
        data = b""
        while True:
            chunk = out.read(65536)
            if not chunk:
                break
            data += chunk
    return data.decode("utf-8")


def folder_direct(folder, command):
    with open(folder + "/in", "wb") as sink:
        sink.write((command + "\n").encode("utf-8"))
    return folder_reply(folder)


def folder_sh(folder, command):
    done = subprocess.run(["sh", "-c", "echo '%s' > %s/in; cat %s/out" % (command, folder, folder)],
                          capture_output=True, timeout=TIMEOUT)
    return done.stdout.decode("utf-8")


class Console:
    def __init__(self, path):
        self.sock = socket.socket(socket.AF_UNIX, socket.SOCK_STREAM)
        self.sock.settimeout(TIMEOUT)
        self.sock.connect(path)
        self.buffer = b""

    def ask(self, line):
        self.sock.sendall((line + "\n").encode("utf-8"))
        return self.until_prompt()

    def until_prompt(self):
        while PROMPT not in self.buffer:
            chunk = self.sock.recv(65536)
            if not chunk:
                raise EOFError("the program closed the connection")
            self.buffer += chunk
        text, _, self.buffer = self.buffer.partition(PROMPT)
        return text.decode("utf-8")

    def close(self):
        self.sock.close()


def timed(label, count, run):
    started = time.monotonic()
    reply = run()
    elapsed = time.monotonic() - started
    print("%-28s %8.1f ms per command  (%d commands, %.2f s)" % (label, elapsed * 1000.0 / count, count, elapsed))
    return reply


def main():
    if len(sys.argv) < 3:
        print(__doc__)
        return 2
    folder = sys.argv[1].rstrip("/")
    path = sys.argv[2]
    count = int(sys.argv[3]) if len(sys.argv) > 3 else 100
    command = sys.argv[4] if len(sys.argv) > 4 else "windows"
    joined = " ; ".join([command] * count)

    def sh_each():
        return [folder_sh(folder, command) for _ in range(count)][-1]

    def direct_each():
        return [folder_direct(folder, command) for _ in range(count)][-1]

    def direct_line():
        return folder_direct(folder, joined)

    console = Console(path)
    console.until_prompt()

    def console_each():
        return [console.ask(command) for _ in range(count)][-1]

    def console_line():
        return console.ask(joined)

    # One untimed command each way first, so control is taken and nothing
    # one-off is counted.
    folder_direct(folder, command)
    console.ask(command)
    for label, run in (("folder, sh", sh_each), ("folder, direct", direct_each), ("folder, one line", direct_line),
                       ("socket console", console_each), ("socket console, one line", console_line)):
        reply = timed(label, count, run)
        lines = reply.strip().splitlines()
        if not lines or "error" in lines[-1]:
            print("  the last reply was an error: " + reply[-300:].replace("\n", " | "))
    console.close()
    return 0


if __name__ == "__main__":
    sys.exit(main())
