"""Starts a GUI.Forms program with --mcp-fcfs and checks that it serves MCP (cp37).

The program (tests/mcp_launch_probe.cpp) must answer initialize with its
server information and tools/list with the built-in tools (snapshot, act,
capture among them) and the probe's own tool probe_ready, registered in its
window's ready callback, then end within the timeout after a hard disconnect
(the end of standard input alone ends only the stdio session, never the
program: MCP 1.0.6). A build whose MCP has no transport (an STXlib
package without stxlib::mcp) answers nothing and fails here.

Then the start race: the program is launched again `launches` times, and
each time initialize, initialized and tools/list are written at once, before
the program has a window. Every first tools/list must carry the whole set
(1.0.3 bound its tools one by one while already serving, and a tools/list on
macos-arm64 saw two tools without act and capture).

    python3 -B tools/mcp_launch_check.py <program> [timeout seconds] [launches]

Prints one line per step and exits 0 when every step passed.
"""

import json
import queue
import subprocess
import sys
import threading

PROTOCOL_VERSION = '2025-06-18'
REQUIRED_TOOLS = ('snapshot', 'act', 'capture', 'probe_ready')
DEFAULT_LAUNCHES = 10


def reader(stream, replies):
    for raw in iter(stream.readline, b''):
        try:
            message = json.loads(raw.decode('utf-8'))
        except ValueError:
            continue
        if isinstance(message, dict) and 'id' in message:
            replies.put(message)


def send(process, message):
    process.stdin.write((json.dumps(message) + '\n').encode('utf-8'))
    process.stdin.flush()


def reply_to(replies, identity, timeout):
    while True:
        message = replies.get(timeout=timeout)
        if message.get('id') == identity:
            return message


def initialize_message(identity):
    return {'jsonrpc': '2.0', 'id': identity, 'method': 'initialize',
            'params': {'protocolVersion': PROTOCOL_VERSION, 'capabilities': {},
                       'clientInfo': {'name': 'gui-forms-mcp-launch-check', 'version': '1'}}}


def disconnect_message(identity):
    return {'jsonrpc': '2.0', 'id': identity, 'method': 'tools/call',
            'params': {'name': 'disconnect', 'arguments': {'mode': 'hard', 'discard_unsaved': True}}}


def end_program(process, replies, timeout):
    """A hard disconnect (an act: --mcp-fcfs takes control at once),
    answered, then the input closed; the exit code, or None."""
    try:
        send(process, disconnect_message(99))
        reply_to(replies, 99, timeout)
    except (queue.Empty, BrokenPipeError, OSError):
        pass
    try:
        process.stdin.close()
    except OSError:
        pass
    try:
        return process.wait(timeout=timeout)
    except subprocess.TimeoutExpired:
        process.kill()
        process.wait()
        return None


def start_race(program, timeout, launches):
    """The start race: the requests are written at launch. Failures counted."""
    failures = 0
    counts = set()
    for launch in range(launches):
        process = subprocess.Popen([program, '--mcp-fcfs'], stdin=subprocess.PIPE, stdout=subprocess.PIPE,
                                   stderr=subprocess.DEVNULL)
        replies = queue.Queue()
        threading.Thread(target=reader, args=(process.stdout, replies), daemon=True).start()
        problem = ''
        try:
            burst = [initialize_message(1), {'jsonrpc': '2.0', 'method': 'notifications/initialized'},
                     {'jsonrpc': '2.0', 'id': 2, 'method': 'tools/list', 'params': {}}]
            process.stdin.write(''.join(json.dumps(message) + '\n' for message in burst).encode('utf-8'))
            process.stdin.flush()
            if 'result' not in reply_to(replies, 1, timeout):
                problem = 'initialize failed'
            else:
                listed = reply_to(replies, 2, timeout)
                names = [tool.get('name', '') for tool in listed.get('result', {}).get('tools', [])]
                missing = [name for name in REQUIRED_TOOLS if name not in names]
                counts.add(len(names))
                if missing:
                    problem = '%d tools, missing %s' % (len(names), ', '.join(missing))
        except queue.Empty:
            problem = 'no reply within %s seconds' % timeout
        except (BrokenPipeError, OSError) as error:
            problem = 'the program closed its input: %s' % error
        code = end_program(process, replies, timeout)
        if not problem and code != 0:
            problem = 'exit %s' % code
        if problem:
            print('start race launch', launch + 1, 'FAILED', problem)
            failures += 1
    if len(counts) > 1:
        print('start race FAILED: the first tools/list differed between launches:', sorted(counts))
        failures += 1
    print('start race', 'ok' if not failures else 'FAILED', launches, 'launches')
    return failures


def main(argv):
    if len(argv) < 2:
        print('usage: mcp_launch_check.py <program> [timeout seconds] [launches]', file=sys.stderr)
        return 2
    program = argv[1]
    timeout = float(argv[2]) if len(argv) > 2 else 30.0
    launches = int(argv[3]) if len(argv) > 3 else DEFAULT_LAUNCHES
    process = subprocess.Popen([program, '--mcp-fcfs'], stdin=subprocess.PIPE, stdout=subprocess.PIPE,
                               stderr=subprocess.PIPE)
    replies = queue.Queue()
    threading.Thread(target=reader, args=(process.stdout, replies), daemon=True).start()
    failures = 0
    try:
        send(process, initialize_message(1))
        initialized = reply_to(replies, 1, timeout)
        server = initialized.get('result', {}).get('serverInfo', {})
        ok = 'result' in initialized and bool(server.get('name'))
        print('initialize', 'ok' if ok else 'FAILED', json.dumps(server))
        failures += 0 if ok else 1
        send(process, {'jsonrpc': '2.0', 'method': 'notifications/initialized'})
        send(process, {'jsonrpc': '2.0', 'id': 2, 'method': 'tools/list', 'params': {}})
        listed = reply_to(replies, 2, timeout)
        names = sorted(tool.get('name', '') for tool in listed.get('result', {}).get('tools', []))
        missing = [name for name in REQUIRED_TOOLS if name not in names]
        print('tools/list', 'ok' if not missing else 'FAILED', len(names), 'tools',
              ('missing ' + ', '.join(missing)) if missing else '')
        failures += 1 if missing else 0
    except queue.Empty:
        print('FAILED: no reply within', timeout, 'seconds (does this build have an MCP transport?)')
        failures += 1
    except (BrokenPipeError, OSError) as error:
        print('FAILED: the program closed its input:', error)
        failures += 1
    code = end_program(process, replies, timeout)
    if code is None:
        print('shutdown FAILED: still running', timeout, 'seconds after a hard disconnect')
        failures += 1
    else:
        print('shutdown', 'ok' if code == 0 else 'FAILED', 'exit', code)
        failures += 0 if code == 0 else 1
    if failures:
        sys.stderr.write(process.stderr.read().decode('utf-8', 'replace')[-2000:])
    failures += start_race(program, timeout, launches)
    return 1 if failures else 0


if __name__ == '__main__':
    sys.exit(main(sys.argv))
