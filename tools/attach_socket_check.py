#!/usr/bin/env python3
"""The attach socket end to end (MCP 1.0.5; the owner's attach design,
revision 3): a GUI.Forms program's socket speaks MCP itself, and a plain-text
console when the first line is not JSON.

Starts the MCP launch probe with --mcp-shared (accepted without a question),
in a private run directory (HOME on macOS, XDG_RUNTIME_DIR elsewhere), and
checks:
- the program says its socket on standard error, and --mcp-socket prints it;
- the discovery entry <pid>.info names the socket and counts the tools;
- JSON mode: initialize (the instructions carry a session token),
  tools/list with probe_ready, tools/call;
- a resumed session across two connections (params.resume), and a refused
  resume with an unknown token (a JSON-RPC error on the initialize's id);
- console mode: help, tools, call with key=value, an unknown tool, the
  transcript (with the session labels), quit; and `resume <token>` after a
  dropped console connection;
- the socket and the entry go when the program ends.
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


def initialize(identifier, resume=None):
    params = {"protocolVersion": "2025-06-18", "capabilities": {},
              "clientInfo": {"name": "attach-check", "version": "1"}}
    if resume is not None:
        params["resume"] = resume
    return request(identifier, "initialize", params)


INITIALIZED = json.dumps({"jsonrpc": "2.0", "method": "notifications/initialized"}) + "\n"


def socket_from_stderr(process):
    deadline = time.monotonic() + TIMEOUT
    seen = []
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
    token = re.search(r"Session token: ([0-9a-f]{32})", instructions)
    require(token is not None, "the instructions carry a session token")
    token = token.group(1)
    tools = [tool["name"] for tool in first.message(2)["result"]["tools"]]
    require("probe_ready" in tools and "act" in tools, "tools/list has the probe's tool and act: " + repr(tools))
    first.send(request(3, "tools/call", {"name": "probe_ready", "arguments": {}}))
    called = first.message(3)
    require(called["result"]["content"][0]["text"] == "ready", "tools/call runs the tool: " + json.dumps(called))
    first.close()

    second = Connection(path)
    second.send(initialize(10, resume=token) + INITIALIZED + request(11, "tools/call", {"name": "windows"}))
    resumed = second.message(10)
    require("result" in resumed, "the session resumes on a new connection: " + json.dumps(resumed))
    windows = second.message(11)
    require(not windows["result"].get("isError"), "a resumed session calls tools: " + json.dumps(windows))
    second.close()

    refused = Connection(path)
    refused.send(initialize("x", resume="0" * 32))
    answer = refused.message("x")
    require(answer.get("error", {}).get("code") == -32001, "an unknown token is refused: " + json.dumps(answer))
    refused.close()
    return token


def console_mode(path):
    console = Connection(path)
    console.send("help\n")
    help_text = console.until_prompt()
    require("Commands:" in help_text and "call <tool>" in help_text, "help lists the commands")
    token = re.search(r"Session token: ([0-9a-f]{32})", help_text)
    require(token is not None, "help says the session token")
    token = token.group(1)
    console.send("help\n")
    console.until_prompt()
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
    require("call find name=Probe role=button" in transcript and "== session console " + token[:4] in transcript and
            "== session attach-check" in transcript,
            "the transcript holds both sessions under their labels:\n" + transcript)
    # The connection drops without quit: the session waits for a resume.
    console.close()
    again = Connection(path)
    again.send("resume " + token + "\n")
    require(again.until_prompt().startswith("resumed session " + token), "resume takes the session back")
    again.send("call probe_ready\n")
    require(again.until_prompt().strip() == "ready", "the resumed console calls tools")
    again.send("quit\n")
    require(again.line() == "session ended", "quit ends the session")
    again.close()
    after = Connection(path)
    after.send("resume " + token + "\n")
    require(after.line().startswith("refused:"), "a quit session cannot be resumed")
    after.close()


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
        process = subprocess.Popen([probe, "--mcp-shared"], stdin=subprocess.PIPE, stdout=subprocess.DEVNULL,
                                   stderr=subprocess.PIPE, env=environment)
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
            printed = subprocess.run([probe, "--mcp-socket"], capture_output=True, env=environment,
                                     timeout=TIMEOUT)
            require(printed.returncode == 0 and path in printed.stdout.decode("utf-8").splitlines(),
                    "--mcp-socket prints the running program's socket: " + printed.stdout.decode("utf-8"))
            json_mode(path)
            console_mode(path)
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
    print("attach socket check passed")
    return 0


if __name__ == "__main__":
    sys.exit(main())
