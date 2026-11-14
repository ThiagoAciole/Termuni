#!/usr/bin/env python3
"""Measure own Xvfb terminal sessions; save raw samples and environment as JSON."""
import argparse
import hashlib
import json
import os
from pathlib import Path
import platform
import re
import signal
import subprocess
import time

parser = argparse.ArgumentParser(description=__doc__)
parser.add_argument('--binary', type=Path, default=Path('build/tests/test-ui'))
parser.add_argument('--samples', type=int, default=3)
parser.add_argument('--lines', type=int, default=3000)
parser.add_argument('--output', type=Path, default=Path('build/benchmark.json'))
args = parser.parse_args()
if not 1 <= args.samples <= 20:
    parser.error('--samples must be between 1 and 20')
if not 1000 <= args.lines <= 30000:
    parser.error('--lines must be between 1000 and 30000')
report = {'environment': {'kernel': platform.platform(), 'backend': 'Xvfb/X11',
          'renderer': 'cairo/software', 'binary': str(args.binary.resolve()),
          'memory_scope': 'frontend process only; excludes shells',
          'idle_interval_ms': 500, 'resize_pump_ms': 30,
          'startup_scope': 'mapped_ms: create/map window and tabs; shell_ready includes 700ms input driver per tab',
          'output_scope': 'all tabs, including focus/input driver; complete files verified',
          'binary_sha256': hashlib.sha256(args.binary.read_bytes()).hexdigest()}, 'samples': []}
for package in ['gtk4', 'libadwaita-1']:
    report['environment'][package] = subprocess.check_output(['pkg-config', '--modversion', package], text=True).strip()
report['environment']['commit'] = subprocess.check_output(['git', 'rev-parse', 'HEAD'], text=True).strip()
report['environment']['dirty'] = bool(subprocess.check_output(['git', 'status', '--porcelain'], text=True).strip())
for tabs in [1, 5, 10]:
    for sample in range(args.samples):
        env = dict(os.environ, GSETTINGS_BACKEND='memory', GDK_BACKEND='x11',
                   GSK_RENDERER='cairo', TERMAC_BENCHMARK_TABS=str(tabs), TERMAC_BENCHMARK_LINES=str(args.lines))
        # Do not connect measurements to the user's desktop D-Bus.
        env['DBUS_SESSION_BUS_ADDRESS'] = 'unix:path=/nonexistent/termac-benchmark-bus'
        start = time.monotonic()
        run = subprocess.Popen(['xvfb-run', '-a', str(args.binary.resolve()), '-p', '/ui/benchmark'],
                               env=env, stdout=subprocess.PIPE, stderr=subprocess.PIPE,
                               text=True, start_new_session=True)
        try:
            stdout, stderr = run.communicate(timeout=180)
        except subprocess.TimeoutExpired:
            os.killpg(run.pid, signal.SIGKILL)
            run.communicate()
            raise
        if run.returncode:
            raise RuntimeError(stdout + stderr)
        match = re.search(r'^(?:# )?TERMAC_BENCHMARK (.+)$', stdout, re.M)
        if not match:
            raise RuntimeError('Benchmark did not emit a sample:\n' + stdout + stderr)
        result = json.loads(match.group(1))
        result.update(sample=sample + 1, process_elapsed_ms=round((time.monotonic() - start) * 1000, 3))
        report['samples'].append(result)
        args.output.parent.mkdir(parents=True, exist_ok=True)
        args.output.write_text(json.dumps(report, indent=2) + '\n')
        print(json.dumps(result), flush=True)
print(f'Saved {args.output}')
