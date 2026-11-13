#!/usr/bin/env python3
"""Run SDK-built /ui/flatpak-host-session and check its PID from the host.

Usage: tools/test-flatpak-host.py APP_BUILD_DIR MODULE_SOURCE_DIR
Build with flatpak-builder --keep-build-dirs first. The module's tests/test-ui
must be built with the current tests/test-ui.c. No files or user sessions are removed.
"""
import os
import re
import subprocess
import sys
import time
import tempfile
from pathlib import Path

app_dir, source_dir = (str(Path(arg).resolve()) for arg in sys.argv[1:3])
test_path = sys.argv[3] if len(sys.argv) == 4 else "/ui/flatpak-host-session"
cache = tempfile.TemporaryDirectory(prefix="termac-flatpak-test-")
command = [
    'xvfb-run', '-a', 'flatpak', 'build', '--socket=x11', '--share=ipc',
    '--talk-name=org.freedesktop.Flatpak', '--env=GDK_BACKEND=x11',
    '--env=GSETTINGS_BACKEND=memory',
    f'--env=TERMAC_TEST_CACHE_DIR={cache.name}',
    f'--bind-mount={cache.name}={cache.name}',
    f'--env=TERMAC_TEST_HOST_SHELL={os.environ.get("SHELL", "/bin/sh")}',
    f'--bind-mount=/run/build/termac={source_dir}', app_dir,
    '/run/build/termac/_flatpak_build/tests/test-ui',
    '-p', test_path,
]
result = subprocess.run(command, capture_output=True, text=True, timeout=45)
output = result.stdout + result.stderr
print(output)
assert result.returncode == 0, 'Flatpak UI test failed'
assert re.search(r'^ok \d+ ' + re.escape(test_path) + '$', output, re.M), 'Test did not run'
if test_path != '/ui/flatpak-host-session':
    sys.exit(0)
pid = int(re.search(r'host-sleep-pid=(\d+)', output).group(1))
for _ in range(30):
    try:
        os.kill(pid, 0)
    except ProcessLookupError:
        print(f'PASS: host sleep {pid} was reaped after closing its terminal')
        break
    time.sleep(0.1)
else:
    raise AssertionError(f'Host sleep {pid} survived closing the terminal')
