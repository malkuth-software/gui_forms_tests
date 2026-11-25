#!/usr/bin/env python3
"""The attach socket end to end (MCP 1.0.5; the owner's attach design,
revision 3): a GUI.Forms program's socket speaks MCP itself, and a plain-text
console when the first line is not JSON.

Starts the MCP launch probe with --mcp-shared in a private run directory
(HOME on macOS, XDG_RUNTIME_DIR elsewhere), and checks, under the owner's UCI
model (MCP round 5: no prompts, no sessions, no tokens; the program owns
control and its record):
- --mcp-help prints the help tree with no socket and exits;
- the program says its socket on standard error, and --mcp-socket prints it;
- the discovery entry <pid>.info names the socket, counts the tools and
  says who may connect;
- JSON mode: initialize (the instructions say who may connect and carry no
  token), tools/list with probe_ready, tools/call; a second MCP client
  connected at the same time is served too;
- two one-shot connections in a row: the first act takes control (the
  countdown), the second acts at once with no question, so control
  persisted across connections;
- console mode: help, dotted help paths, tools, call with key=value, an
  unknown tool, the transcript (every connection's lines, labelled), quit;
- the command folder (/tmp/<id>, the owner's zero-binary door): the
  printed command, modes 0700 and 0600, an echo/cat round trip through
  sh -c, two acts chained across separate shells keep control, the
  request-id echo, the discovery entry's door line;
- the program ends when its input closes, and the socket, the entry and the
  command folder go with it.
Another user's peer cannot be made here; the rule is checked in
gui_forms_automation_attach_tests.

  attach_socket_check.py <probe executable>

Exit 77 (skipped) where Python has no AF_UNIX (Windows' Python).
"""

import json
import os
import re
import select
import socket
import stat
import subprocess
import sys
import tempfile
import time

TIMEOUT = 30.0
PROMPT = b"gui-forms> "


def fail(message):
    print("attach socket check FAILED: " + message, file=sys.stderr)
    sys.exit(1)


def require(condition, message):
    if not condition:
        fail(message)


class Connection:
    """One socket connection, read by lines or up to the console's prompt."""

    def __init__(self, path):
        self.sock = socket.socket(socket.AF_UNIX, socket.SOCK_STREAM)
        self.sock.settimeout(TIMEOUT)
        self.sock.connect(path)
        self.buffer = b""

    def send(self, text):
        self.sock.sendall(text.encode("utf-8"))

    def _fill(self):
        chunk = self.sock.recv(65536)
        if not chunk:
            raise EOFError("the program closed the connection")
        self.buffer += chunk

    def line(self):
        while b"\n" not in self.buffer:
            self._fill()
        line, _, self.buffer = self.buffer.partition(b"\n")
        return line.decode("utf-8")

    def until_prompt(self):
        while PROMPT not in self.buffer:
            self._fill()
        text, _, self.buffer = self.buffer.partition(PROMPT)
        return text.decode("utf-8")

    def message(self, wanted_id):
        """The JSON-RPC response with id `wanted_id`, skipping notifications."""
        while True:
            parsed = json.loads(self.line())
            if parsed.get("id") == wanted_id:
                return parsed

    def close(self):
        self.sock.close()


def request(identifier, method, params=None):
    message = {"jsonrpc": "2.0", "id": identifier, "method": method}
    if params is not None:
        message["params"] = params
    return json.dumps(message) + "\n"


def initialize(identifier):
    params = {"protocolVersion": "2025-06-18", "capabilities": {},
              "clientInfo": {"name": "attach-check", "version": "1"}}
    return request(identifier, "initialize", params)


INITIALIZED = json.dumps({"jsonrpc": "2.0", "method": "notifications/initialized"}) + "\n"


DOOR_LINES = []


def socket_from_stderr(process):
    deadline = time.monotonic() + TIMEOUT
    seen = DOOR_LINES
    while time.monotonic() < deadline:
        ready, _, _ = select.select([process.stderr], [], [], 0.5)
        if not ready:
            if process.poll() is not None:
                break
            continue
        line = process.stderr.readline().decode("utf-8", "replace")
        if not line:
            break
        seen.append(line.rstrip())
        found = re.match(r"GUI\.Forms: MCP socket: (.+)$", line.rstrip())
        if found:
            return found.group(1)
    fail("the program did not say its socket; it said: " + " | ".join(seen))
    return ""


def read_info(path):
    entry = {}
    with open(path, encoding="utf-8") as file:
        for line in file:
            key, _, value = line.partition(":")
            entry[key.strip()] = value.strip()
    return entry


def json_mode(path):
    first = Connection(path)
    first.send(initialize(1) + INITIALIZED + request(2, "tools/list"))
    answer = first.message(1)
    require("result" in answer, "initialize answers: " + json.dumps(answer))
    instructions = answer["result"].get("instructions", "")
    require("your own user account" in instructions and "token" not in instructions.lower(),
            "the instructions say who may connect and carry no token: " + instructions[:300])
    require("--mcp-help" in instructions, "the instructions point to --mcp-help")
    tools = [tool["name"] for tool in first.message(2)["result"]["tools"]]
    require("probe_ready" in tools and "act" in tools, "tools/list has the probe's tool and act: " + repr(tools))
    # A second client connected at the same time is served too: commands
    # from every connection run one at a time.
    other = Connection(path)
    other.send(initialize("o") + INITIALIZED + request("p", "tools/call", {"name": "probe_ready", "arguments": {}}))
    require("result" in other.message("o"), "a second MCP client is served while the first is connected")
    require(other.message("p")["result"]["content"][0]["text"] == "ready", "and its calls run")
    first.send(request(3, "tools/call", {"name": "probe_ready", "arguments": {}}))
    called = first.message(3)
    require(called["result"]["content"][0]["text"] == "ready", "tools/call runs the tool: " + json.dumps(called))
    other.close()
    first.close()


def one_shot(path, line):
    """Sends one console line on a fresh connection and closes it; the
    answer and how long it took."""
    started = time.monotonic()
    connection = Connection(path)
    connection.send(line + "\n")
    answer = connection.until_prompt()
    connection.close()
    return answer, time.monotonic() - started


def one_shot_connections(path):
    press = 'call act {"action":"press","target":"probe.button"}'
    first, _ = one_shot(path, press)
    require(not first.startswith("error:") and "refused" not in first,
            "a one-shot connection acts (the first act's countdown allows): " + first)
    second, took = one_shot(path, press)
    require(not second.startswith("error:") and took < 8.0,
            "the next one-shot connection acts at once, with no question: %.1f s: %s" % (took, second))
    require("allow" not in (first + second).lower() and "token" not in (first + second).lower(),
            "nothing asks and nothing names a token")


def console_mode(path):
    console = Connection(path)
    console.send("help\n")
    help_text = console.until_prompt()
    require("Commands" in help_text and "call <tool>" in help_text and "help [path]" in help_text,
            "help lists the commands")
    require("your own user account" in help_text and "token" not in help_text.lower(),
            "help says who may connect and names no token")
    console.send("help mcp.connections\n")
    require("-- Connections --" in console.until_prompt(), "a dotted path opens a section")
    console.send("help probe_redy\n")
    require("`help probe_ready`" in console.until_prompt(), "an unknown path names the nearest")
    console.send("tools\n")
    require("probe_ready" in console.until_prompt(), "tools lists the probe's tool")
    console.send("call probe_ready\n")
    require(console.until_prompt().strip() == "ready", "call answers the tool's text")
    console.send("call find name=Probe role=button\n")
    require("probe.button" in console.until_prompt(), "key=value arguments reach the tool")
    console.send("call no_such_tool\n")
    require(console.until_prompt().startswith("error:"), "an unknown tool answers an error")
    console.send("transcript\n")
    transcript = console.until_prompt()
    require("call find name=Probe role=button" in transcript and "== session console pid " in transcript and
            "== session attach-check" in transcript,
            "the transcript holds every connection's lines under their labels:\n" + transcript)
    console.send("quit\n")
    require(console.line() == "closed", "quit closes the connection")
    console.close()


def door_mode(path, entry):
    """The command folder through plain shells: no client but sh."""
    found = [re.match(r"GUI\.Forms: \S+ ([0-9a-f]{8})\. Send commands \(chain them with && or ;\): (.*)$", line) for line in DOOR_LINES]
    found = [match for match in found if match]
    require(found, "the program prints the command folder's line: " + " | ".join(DOOR_LINES))
    door = found[-1].group(1)
    folder = "/tmp/" + door
    require(found[-1].group(2) == "echo '<command>' > %s/in; cat %s/out" % (folder, folder),
            "the line is the exact command: " + found[-1].group(2))
    require(entry.get("door") == door, "the discovery entry names the folder: " + repr(entry))
    status = os.lstat(folder)
    require(stat.S_ISDIR(status.st_mode) and (status.st_mode & 0o777) == 0o700 and status.st_uid == os.getuid(),
            "the folder is the user's own, mode 0700: " + oct(status.st_mode))
    for name in ("in", "out"):
        fifo = os.lstat(folder + "/" + name)
        require(stat.S_ISFIFO(fifo.st_mode) and (fifo.st_mode & 0o777) == 0o600, name + " is a FIFO, mode 0600")

    def send(command):
        started = time.monotonic()
        done = subprocess.run(["sh", "-c", "echo '%s' > %s/in; cat %s/out" % (command, folder, folder)],
                              capture_output=True, timeout=TIMEOUT)
        return done.stdout.decode("utf-8"), time.monotonic() - started

    reply, _ = send("help")
    require("probe_ready" in reply and reply.rstrip().endswith(". ok"), "help through sh: " + reply[-200:])
    reply, _ = send("probe_ready")
    require(reply == "ready\n. ok\n", "a tool by its name, framed: " + repr(reply))
    press = 'act action=press target=probe.button'
    first, _ = send(press)
    second, took = send("#42 " + press)
    require(first.rstrip().endswith(". ok") and second.rstrip().endswith(". #42 ok") and took < 8.0,
            "two acts chained across shells keep control, the request ID echoed: %.1f s: %s" % (took, second[-200:]))
    reply, _ = send("#e1 no_such_tool")
    require(reply.rstrip().endswith(". #e1 error"), "an error's end line says so: " + reply[-200:])
    # Chains (owner): && stops at a failure, ; goes on, and separators in
    # quoted values or JSON are not separators.
    reply, _ = send("#c1 probe_ready && no_such_tool && probe_ready")
    require(reply.startswith("[1] ready\n[2] error:") and "[3] not run" in reply and
            reply.rstrip().endswith(". #c1 error at 2"), "a && chain stops at its failure: " + repr(reply))
    reply, _ = send("no_such_tool ; probe_ready")
    require("[2] ready" in reply and reply.rstrip().endswith(". error at 1"), "a ; chain goes on: " + repr(reply))
    reply, _ = send('find name="a && b; c" ; probe_ready')
    require("[2] ready" in reply and "[3]" not in reply, "separators inside quotes are not separators: " + reply)
    reply, _ = send("probe_ready && probe_ready")
    require(reply == "[1] ready\n[2] ready\n. ok\n", "a chain that succeeds ends ok: " + repr(reply))
    subprocess.run(["sh", "-c", "echo 'help mcp.control' > %s/in" % folder], timeout=TIMEOUT)
    later = subprocess.run(["sh", "-c", "cat %s/out" % folder], capture_output=True, timeout=TIMEOUT)
    require("-- Control --" in later.stdout.decode("utf-8"), "a reply waits for a reader in a later shell")
    require("probe_ready" in open(folder + "/help", encoding="utf-8").read(), "help is a file too")
    require("pid: " in open(folder + "/info", encoding="utf-8").read(), "info names the process")
    return folder


def help_without_socket(probe, environment):
    printed = subprocess.run([probe, "--mcp-help"], capture_output=True, env=environment, timeout=TIMEOUT)
    text = printed.stdout.decode("utf-8")
    require(printed.returncode == 0 and "probe_ready" in text and "your own user account" in text,
            "--mcp-help prints the top of the tree: " + text[:300])
    require(b"MCP socket:" not in printed.stderr, "--mcp-help opens no socket")
    section = subprocess.run([probe, "--mcp-help", "mcp.control"], capture_output=True, env=environment,
                             timeout=TIMEOUT)
    require(section.returncode == 0 and "-- Control --" in section.stdout.decode("utf-8"),
            "--mcp-help takes a dotted path")


def main():
    if not hasattr(socket, "AF_UNIX"):
        print("attach socket check skipped: this Python has no AF_UNIX")
        return 77
    if len(sys.argv) != 2:
        print(__doc__, file=sys.stderr)
        return 2
    probe = sys.argv[1]
    # A short base: a Unix socket's path has room for about 100 bytes, and
    # macOS's temporary directory alone takes half of it.
    base = "/tmp" if os.path.isdir("/tmp") else None
    with tempfile.TemporaryDirectory(prefix="gfa", dir=base) as private:
        os.chmod(private, 0o700)
        environment = dict(os.environ)
        environment["GUI_FORMS_MCP_ATTACH"] = "1"
        environment["HOME"] = private
        environment["XDG_RUNTIME_DIR"] = private
        help_without_socket(probe, environment)
        process = subprocess.Popen([probe, "--mcp-shared"], stdin=subprocess.PIPE, stdout=subprocess.DEVNULL,
                                   stderr=subprocess.PIPE, env=environment)
        folder = ""
        try:
            path = socket_from_stderr(process)
            require(os.path.basename(path) == str(process.pid) + ".sock", "the socket is <pid>.sock: " + path)
            info = path[: -len(".sock")] + ".info"
            entry = {}
            deadline = time.monotonic() + TIMEOUT
            while time.monotonic() < deadline:
                entry = read_info(info) if os.path.exists(info) else {}
                if entry.get("tools", "0") != "0":
                    break
                time.sleep(0.1)
            require(entry.get("socket") == path and entry.get("pid") == str(process.pid),
                    "the discovery entry names the socket: " + repr(entry))
            require(int(entry.get("tools", "0")) > 0, "the discovery entry counts the tools: " + repr(entry))
            require("your own user account" in entry.get("access", ""), "the entry says who may connect: " + repr(entry))
            printed = subprocess.run([probe, "--mcp-socket"], capture_output=True, env=environment,
                                     timeout=TIMEOUT)
            require(printed.returncode == 0 and path in printed.stdout.decode("utf-8").splitlines(),
                    "--mcp-socket prints the running program's socket: " + printed.stdout.decode("utf-8"))
            json_mode(path)
            one_shot_connections(path)
            console_mode(path)
            folder = door_mode(path, entry)
        finally:
            # The portable shutdown: the launching client's input closes.
            process.stdin.close()
            try:
                process.wait(timeout=TIMEOUT)
            except subprocess.TimeoutExpired:
                process.kill()
                process.wait()
                fail("the program did not end when its input closed")
        require(not os.path.exists(path) and not os.path.exists(info), "the socket and the entry go with the program")
        require(not os.path.exists(folder), "the command folder goes with the program")
    print("attach socket check passed")
    return 0


if __name__ == "__main__":
    sys.exit(main())
