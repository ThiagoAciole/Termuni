#!/usr/bin/env python3
"""One action plus a fresh rendered observation in the opt-in real-agent test.

Usage: agent-qa-control.py CONTROL {snapshot,type,key,drop,clipboard,quit} [PAYLOAD]
For type, PAYLOAD is a UTF-8 text file. For key it is an X keysym, optionally
Ctrl+ or Shift+ prefixed. For drop/clipboard it is an image path. The test owns
its Xvfb display; this does not control or restart an existing user window.
"""
import argparse
import time
from pathlib import Path

parser = argparse.ArgumentParser(description=__doc__)
parser.add_argument('control', type=Path)
parser.add_argument('operation', choices=['snapshot', 'type', 'key', 'drop', 'clipboard', 'quit'])
parser.add_argument('payload', nargs='?', default='')
args = parser.parse_args()
ack = Path(str(args.control) + '.ack')
if not ack.exists():
    parser.error('driver is not ready: no .ack file')
previous = ack.read_text().splitlines()[0]
if args.operation == 'type':
    payload = Path(args.payload).read_text()
elif args.operation in {'drop', 'clipboard'}:
    payload = str(Path(args.payload).resolve(strict=True))
else:
    payload = args.payload
if args.control.exists():
    parser.error('a previous action is still pending')
pending = Path(str(args.control) + '.pending')
pending.write_text(args.operation + '\n' + payload)
pending.replace(args.control)
if args.operation == 'quit':
    raise SystemExit(0)
for _ in range(300):
    if ack.exists() and ack.read_text().splitlines()[0] != previous:
        print(Path(str(args.control) + '.screen.txt').read_text())
        break
    time.sleep(0.05)
else:
    raise SystemExit('driver did not acknowledge the action within 15 seconds')
