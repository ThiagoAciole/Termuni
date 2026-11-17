#!/usr/bin/env python3
"""Exercise dev.sh against private compiled processes, never desktop sessions."""
import os
from pathlib import Path
import shutil
import signal
import subprocess
import tempfile
import time
import unittest

SOURCE = Path(__file__).resolve().parents[1]


class DevelopmentLauncher(unittest.TestCase):
    def test_checkout_and_profile_isolation(self):
        with tempfile.TemporaryDirectory(prefix="termuni-dev-test-") as tmp:
            base = Path(tmp)
            fixture = base / "fixture"
            subprocess.run(["cc", "-x", "c", "-", "-o", str(fixture)], input=b'''
#include <stdlib.h>
#include <signal.h>
#include <unistd.h>
int main(void) {
    if (getenv("FAKE_EXIT")) return 42;
    if (getenv("FAKE_IGNORE_TERM")) signal(SIGTERM, SIG_IGN);
    for (;;) pause();
}
''', check=True)
            bindir = base / "bin"
            bindir.mkdir()
            for tool in ("meson", "msgfmt"):
                path = bindir / tool
                path.write_text("#!/bin/sh\nsleep \"${FAKE_BUILD_DELAY:-0}\"\nexit 0\n")
                path.chmod(0o755)
            roots = [base / name for name in ("checkout-a", "checkout-b", "default")]
            for root in roots:
                (root / "tools").mkdir(parents=True)
                (root / "build/development/src").mkdir(parents=True)
                (root / "po").mkdir()
                (root / "po/LINGUAS").write_text("")
                (root / "build/development/config.h").write_text(
                    '#define APPLICATION_ID "io.github.ThiagoAciole.Termuni' +
                    ('' if root.name == 'default' else '.Devel') + '"\n')
                shutil.copy2(SOURCE / "tools/dev.sh", root / "tools/dev.sh")
                shutil.copy2(fixture, root / "build/development/src/termuni")
            env = dict(os.environ, PATH=str(bindir) + ":" + os.environ["PATH"],
                       TERMUNI_DEV_LOG=str(base / "dev.log"))
            processes = []
            children = []

            def launch(root, **extra):
                result = subprocess.run([str(root / "tools/dev.sh"), *extra.pop("args", [])],
                                        env=dict(env, **extra), text=True, capture_output=True)
                return result

            def pids(root):
                result = []
                for proc in Path('/proc').glob('[0-9]*'):
                    try:
                        target = os.readlink(proc / 'exe')
                    except OSError:
                        continue
                    if target in (str(root / 'build/development/src/termuni'), str(root / 'build/development/src/termuni') + ' (deleted)'):
                        result.append(int(proc.name))
                return result

            def collect():
                for root in roots:
                    processes.extend(pid for pid in pids(root) if pid not in processes)

            try:
                # Two different development roots and a default binary coexist.
                for root in roots[1:]:
                    child = subprocess.Popen([str(root / 'build/development/src/termuni')])
                    children.append(child)
                    processes.append(child.pid)
                time.sleep(0.1)
                starter = subprocess.Popen([str(roots[0] / 'tools/dev.sh')],
                                           env=dict(env, FAKE_BUILD_DELAY='1'),
                                           text=True, stdout=subprocess.PIPE, stderr=subprocess.PIPE)
                time.sleep(0.2)
                locked = launch(roots[0])
                self.assertNotEqual(locked.returncode, 0)
                self.assertIn('launch is in progress', locked.stderr)
                _, error = starter.communicate(timeout=10)
                self.assertEqual(starter.returncode, 0, error)
                collect()
                first = pids(roots[0])
                self.assertEqual(len(first), 1)
                protected = pids(roots[1]) + pids(roots[2])
                self.assertEqual(len(protected), 2)
                self.assertEqual(launch(roots[0]).returncode, 0)
                self.assertEqual(pids(roots[0]), first)

                # Simulate Meson replacing the running executable's inode.
                replacement = roots[0] / 'build/development/src/replacement'
                shutil.copy2(fixture, replacement)
                replacement.replace(roots[0] / 'build/development/src/termuni')
                result = launch(roots[0], args=['--restart'])
                self.assertEqual(result.returncode, 0, result.stderr)
                collect()
                second = pids(roots[0])
                self.assertEqual(len(second), 1)
                self.assertNotEqual(second, first)
                self.assertIn('shells and tabs will close', result.stderr)
                self.assertEqual(pids(roots[1]) + pids(roots[2]), protected)
                result = launch(roots[2], args=['--restart'])
                self.assertNotEqual(result.returncode, 0)
                self.assertIn('not a development-profile build', result.stderr)
                self.assertEqual(pids(roots[1]) + pids(roots[2]), protected)

                os.kill(second[0], signal.SIGTERM)
                time.sleep(0.1)
                result = launch(roots[0], FAKE_EXIT='1')
                self.assertNotEqual(result.returncode, 0)
                self.assertIn('exited during startup', result.stderr)
                result = launch(roots[0], FAKE_IGNORE_TERM='1')
                self.assertEqual(result.returncode, 0, result.stderr)
                collect()
                resistant = pids(roots[0])
                result = launch(roots[0], args=['--restart'])
                self.assertNotEqual(result.returncode, 0)
                self.assertIn('refusing to start a duplicate', result.stderr)
                self.assertEqual(pids(roots[0]), resistant)
                self.assertEqual(pids(roots[1]) + pids(roots[2]), protected)
            finally:
                collect()
                for pid in set(processes):
                    # Only the private fixture binaries can be signalled here.
                    try:
                        if os.readlink(f'/proc/{pid}/exe').startswith(str(base) + '/'):
                            os.kill(pid, signal.SIGKILL)
                    except (OSError, ProcessLookupError):
                        pass
                for child in children:
                    child.wait()
                for pid in set(processes):
                    try:
                        os.waitpid(pid, 0)
                    except ChildProcessError:
                        pass


if __name__ == '__main__':
    unittest.main()
