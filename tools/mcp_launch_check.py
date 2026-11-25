"""Starts a GUI.Forms program with --mcp and checks that it serves MCP (cp37).

The program (tests/mcp_launch_probe.cpp) must answer initialize with its
server information and tools/list with the built-in tools (snapshot, act,
capture among them), then end within the timeout once its standard input
closes, the portable shutdown. A build whose MCP has no transport (an STXlib
package without stxlib::mcp) answers nothing and fails here.

    python3 -B tools/mcp_launch_check.py <program> [timeout seconds]

Prints one line per step and exits 0 when every step passed.
"""

import json
import queue
import subprocess
import sys
import threading

PROTOCOL_VERSION = '2025-06-18'
REQUIRED_TOOLS = ('snapshot', 'act', 'capture')


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


def main(argv):
    if len(argv) < 2:
        print('usage: mcp_launch_check.py <program> [timeout seconds]', file=sys.stderr)
        return 2
    program = argv[1]
    timeout = float(argv[2]) if len(argv) > 2 else 30.0
    process = subprocess.Popen([program, '--mcp'], stdin=subprocess.PIPE, stdout=subprocess.PIPE,
                               stderr=subprocess.PIPE)
    replies = queue.Queue()
    threading.Thread(target=reader, args=(process.stdout, replies), daemon=True).start()
    failures = 0
    try:
        send(process, {'jsonrpc': '2.0', 'id': 1, 'method': 'initialize',
                       'params': {'protocolVersion': PROTOCOL_VERSION, 'capabilities': {},
                                  'clientInfo': {'name': 'gui-forms-mcp-launch-check', 'version': '1'}}})
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
    try:
        process.stdin.close()
    except OSError:
        pass
    try:
        code = process.wait(timeout=timeout)
        print('shutdown', 'ok' if code == 0 else 'FAILED', 'exit', code)
        failures += 0 if code == 0 else 1
    except subprocess.TimeoutExpired:
        process.kill()
        process.wait()
        print('shutdown FAILED: still running', timeout, 'seconds after its input closed')
        failures += 1
    if failures:
        sys.stderr.write(process.stderr.read().decode('utf-8', 'replace')[-2000:])
    return 1 if failures else 0


if __name__ == '__main__':
    sys.exit(main(sys.argv))
