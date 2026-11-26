#!/usr/bin/env python3
"""The attach socket end to end (MCP 1.0.5; the owner's attach design,
revision 3): a GUI.Forms program's socket speaks MCP itself, and a plain-text
console when the first line is not JSON.

Starts the MCP launch probe with --mcp-shared in a private run directory
(HOME on macOS, XDG_RUNTIME_DIR elsewhere), and checks, under the owner's UCI
model (MCP round 5: no prompts, no sessions, no tokens; the program owns
control and its record):
- --mcp-help prints the help tree with no socket and exits, and --mcp (the
  help panel) prints its top, saying the program is not shared;
- a launch without a flag has no MCP: no socket, no discovery entry, no
  command folder and no MCP lines;
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
  request-id echo, a blank line answered "error: empty command", the
  discovery entry's door line;
- --mcp-fcfs: started in the background with its input at /dev/null, it
  stays up, its start-up text names the mode, and a chained act runs at
  once (first come, first served: no countdown); a hard disconnect ends it
  and its folder;
- the --mcp-shared program stays up when its input closes (the end of
  standard input ends only the stdio session), its folder still answers,
  and a hard disconnect through the folder ends it, the socket, the entry
  and the command folder going with it.
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
STDERR_PARTIAL = [b""]


def read_stderr_lines(process):
    """The program's complete standard error lines that have arrived, appended
    to DOOR_LINES; None at its end. Read from the descriptor itself: a
    buffered readline after select leaves the lines of one write in Python's
    buffer, where select no longer sees them (the start-up text is one
    write)."""
    ready, _, _ = select.select([process.stderr], [], [], 0.5)
    if not ready:
        return [] if process.poll() is None else None
    chunk = os.read(process.stderr.fileno(), 65536)
    if not chunk:
        return None
    data = STDERR_PARTIAL[0] + chunk
    parts = data.split(b"\n")
    STDERR_PARTIAL[0] = parts.pop()
    lines = [part.decode("utf-8", "replace").rstrip() for part in parts]
    DOOR_LINES.extend(lines)
    return lines


def socket_from_stderr(process):
    deadline = time.monotonic() + TIMEOUT
    seen = DOOR_LINES
    while time.monotonic() < deadline:
        lines = read_stderr_lines(process)
        if lines is None:
            break
        for line in lines:
            found = re.match(r"GUI\.Forms: MCP socket: (.+)$", line)
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


def door_mode(path, entry, process):
    """The command folder through plain shells: no client but sh."""
    # The start-up text comes at the first window's next turn, after the
    # socket's line.
    deadline = time.monotonic() + TIMEOUT
    while time.monotonic() < deadline and not any(line.startswith("GUI.Forms:   help:") for line in DOOR_LINES):
        if read_stderr_lines(process) is None:
            break
    text = "\n".join(DOOR_LINES)
    named = re.search(r"GUI\.Forms: \S+ is a GUI\.Forms program .*command folder is /tmp/([0-9a-f]{8})$", text, re.M)
    sent = re.search(r"^GUI\.Forms:   send: (.*)$", text, re.M)
    require(named is not None and sent is not None, "the program prints the command folder's lines: " + text)
    door = named.group(1)
    folder = "/tmp/" + door
    require(sent.group(1) == "echo '<command>' > %s/in; cat %s/out" % (folder, folder),
            "the send line is the exact command: " + sent.group(1))
    require(re.search(r"^GUI\.Forms:   mode: --mcp-shared: ", text, re.M) and
            re.search(r"^GUI\.Forms:   chain: .*&&.*;", text, re.M) and
            re.search(r"^GUI\.Forms:   help: send help, then help \S+ .*--mcp-help.*release_control.*disconnect",
                      text, re.M),
            "the start-up text names the mode, chaining, help and how to end: " + text)
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
    # An empty or blank line is answered, so `cat out` never blocks.
    for blank in ("", "   "):
        reply, _ = send(blank)
        require(reply == "error: empty command\n. error\n", "a blank line gets a reply: " + repr(reply))
    reply, _ = send("probe_ready && ")
    require("[2] error: empty command" in reply and reply.rstrip().endswith(". error at 2"),
            "an empty chained part is an error: " + repr(reply))
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


def door_folders():
    """The command folders now in /tmp: name to the pid their info names."""
    found = {}
    for name in os.listdir("/tmp"):
        if not re.fullmatch(r"[0-9a-f]{8}", name):
            continue
        try:
            with open("/tmp/%s/info" % name, encoding="utf-8") as read:
                pid = re.search(r"^pid: (\d+)$", read.read(), re.M)
        except OSError:
            continue
        if pid:
            found[name] = int(pid.group(1))
    return found


def unflagged_launch(probe, environment, private):
    """No flag, no MCP (owner, MCP 1.0.6): the window runs, and the program
    makes no socket, no discovery entry and no command folder, and prints no
    MCP lines."""
    errors_path = os.path.join(private, "unflagged.err")
    with open(errors_path, "wb") as errors, open(os.devnull, "rb") as nothing:
        process = subprocess.Popen([probe], stdin=nothing, stdout=subprocess.PIPE, stderr=errors, env=environment,
                                   start_new_session=True)
    try:
        time.sleep(3.0)
        require(process.poll() is None, "the unflagged program runs")
        listed = [name for name in os.listdir(private) if name.endswith(".sock") or name.endswith(".info")]
        run_directory = os.path.join(private, "Library", "Caches", "gui-forms", "run")
        if os.path.isdir(run_directory):
            listed += os.listdir(run_directory)
        mine = [name for name, pid in door_folders().items() if pid == process.pid]
        require(not listed and not mine, "an unflagged launch makes no socket, entry or folder: %r %r" % (listed, mine))
    finally:
        process.terminate()
        try:
            process.wait(timeout=TIMEOUT)
        except subprocess.TimeoutExpired:
            process.kill()
            process.wait()
    printed = process.stdout.read().decode("utf-8", "replace")
    with open(errors_path, encoding="utf-8", errors="replace") as read:
        text = read.read()
    require("GUI.Forms: MCP socket" not in text and "command folder" not in text and "GUI.Forms" not in printed,
            "an unflagged launch prints no MCP lines: " + (text + printed)[:300])


def fcfs_launch(probe, environment, private):
    """--mcp-fcfs: started in the background with its input at /dev/null, the
    program stays up (the end of its input ends only the stdio session),
    names the mode in its start-up text, and a chained command acts at once
    (first come, first served: no countdown)."""
    errors_path = os.path.join(private, "fcfs-launch.err")
    with open(errors_path, "wb") as errors, open(os.devnull, "rb") as nothing:
        process = subprocess.Popen([probe, "--mcp-fcfs"], stdin=nothing, stdout=subprocess.DEVNULL,
                                   stderr=errors, env=environment, start_new_session=True)
    folder = ""
    try:
        deadline = time.monotonic() + TIMEOUT
        found = None
        while time.monotonic() < deadline and found is None and process.poll() is None:
            time.sleep(0.2)
            with open(errors_path, encoding="utf-8", errors="replace") as read:
                text = read.read()
                found = re.search(r"command folder is /tmp/([0-9a-f]{8})$", text, re.M)
                if found and not re.search(r"^GUI\.Forms:   mode: --mcp-fcfs: first come, first served", text,
                                           re.M):
                    found = None
        require(found is not None, "--mcp-fcfs prints the folder's line and names the mode")
        folder = "/tmp/" + found.group(1)
        time.sleep(1.0)
        require(process.poll() is None, "--mcp-fcfs stays up with its input at /dev/null")
        started = time.monotonic()
        done = subprocess.run(["sh", "-c", "echo 'act action=press target=probe.button && probe_ready' > %s/in; "
                               "cat %s/out" % (folder, folder)], capture_output=True, timeout=TIMEOUT)
        took = time.monotonic() - started
        reply = done.stdout.decode("utf-8")
        require(reply.startswith("[1] ok") and "[2] ready" in reply and reply.rstrip().endswith(". ok") and took < 5.0,
                "a chained act runs at once, no countdown: %.1f s: %s" % (took, reply[-300:]))
        subprocess.run(["sh", "-c", "echo 'disconnect mode=hard discard_unsaved=true' > %s/in; cat %s/out"
                        % (folder, folder)], capture_output=True, timeout=TIMEOUT)
        process.wait(timeout=TIMEOUT)
    except subprocess.TimeoutExpired:
        fail("the --mcp-fcfs program did not end after a hard disconnect")
    finally:
        if process.poll() is None:
            process.kill()
            process.wait()
    require(not os.path.exists(folder), "the --mcp-fcfs program's folder goes with it")


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
    panel = subprocess.run([probe, "--mcp"], capture_output=True, env=environment, timeout=TIMEOUT)
    text = panel.stdout.decode("utf-8")
    require(panel.returncode == 0 and "isn't shared with a model right now" in text and "--mcp-fcfs" in text and
            "Send: echo '<command>' > /tmp/<id>/in" in text and "probe_ready" in text,
            "--mcp prints the help panel: " + text[:400])
    require(b"MCP socket:" not in panel.stderr, "--mcp opens no socket")


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
        unflagged_launch(probe, environment, private)
        fcfs_launch(probe, environment, private)
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
            folder = door_mode(path, entry, process)
            # The end of standard input ends the stdio session only: the
            # program stays up and its folder answers (owner, MCP 1.0.6).
            process.stdin.close()
            time.sleep(1.5)
            require(process.poll() is None, "the program stays up when its input closes")
            # An act through the folder (the countdown allows it).
            done = subprocess.run(["sh", "-c", "echo 'act action=press target=probe.button' > %s/in; cat %s/out"
                                   % (folder, folder)], capture_output=True, timeout=TIMEOUT)
            require(done.stdout.decode("utf-8").rstrip().endswith(". ok"),
                    "the folder acts after the input closed: " + done.stdout.decode("utf-8")[-200:])
            subprocess.run(["sh", "-c", "echo 'disconnect mode=hard discard_unsaved=true' > %s/in; cat %s/out"
                            % (folder, folder)], capture_output=True, timeout=TIMEOUT)
        finally:
            if not process.stdin.closed:
                process.stdin.close()
            try:
                process.wait(timeout=TIMEOUT)
            except subprocess.TimeoutExpired:
                process.kill()
                process.wait()
                fail("the program did not end after a hard disconnect through its folder")
        require(not os.path.exists(path) and not os.path.exists(info), "the socket and the entry go with the program")
        require(not os.path.exists(folder), "the command folder goes with the program")
    print("attach socket check passed")
    return 0


if __name__ == "__main__":
    sys.exit(main())
