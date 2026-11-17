#!/usr/bin/python3
"""Check real AT-SPI text in private Xvfb/D-Bus, never the desktop session bus."""
import argparse
import json
import os
from pathlib import Path
import shutil
import subprocess
import sys
import tempfile
import time


def wait(predicate, seconds=8):
    deadline = time.monotonic() + seconds
    while time.monotonic() < deadline:
        if predicate():
            return
        time.sleep(0.03)
    raise AssertionError("Timed out waiting for private AT-SPI fixture")


def probe(root, with_orca=False):
    import gi
    gi.require_version('Atspi', '2.0')
    from gi.repository import Atspi, GLib
    wait(lambda: (root / 'ready').exists())

    def find(obj):
        if obj.get_role() == Atspi.Role.TERMINAL:
            return obj
        for index in range(obj.get_child_count()):
            child = obj.get_child_at_index(index)
            if child is not None:
                result = find(child)
                if result is not None:
                    return result
        return None

    found = []
    def discover():
        terminal = find(Atspi.get_desktop(0))
        if terminal is not None:
            found.append(terminal)
            return True
        return False
    wait(discover)
    terminal = found[0]
    text = terminal.get_text_iface()
    expected = "ATSPITERM 日本 é 👩‍💻"
    assert Atspi.Text.get_text(text, 0, len(expected)) == expected
    assert Atspi.Text.get_caret_offset(text) == 9
    assert Atspi.Text.get_n_selections(text) == 1
    selection = Atspi.Text.get_selection(text, 0)
    assert (selection.start_offset, selection.end_offset) == (0, 9)
    orca_speech = None
    if with_orca:
        from orca.ax_text import AXText
        from orca import script_manager, speech_presenter
        assert AXText.get_all_text(terminal).startswith(expected)
        assert AXText.get_caret_offset(terminal) == 9
        script = script_manager.get_manager().get_script(terminal.get_application(), terminal)
        orca_speech = speech_presenter.get_presenter().generate_speech_string(script, terminal)
        assert 'ATSPITERM' in orca_speech, orca_speech
    events = []
    def changed(event, *unused):
        events.append(event.type)
    listener = Atspi.EventListener.new(changed)
    for kind in ['object:text-changed', 'object:text-caret-moved', 'object:text-selection-changed']:
        listener.register(kind)
    (root / 'change').touch()
    context = GLib.MainContext.default()
    deadline = time.monotonic() + 6
    while time.monotonic() < deadline:
        while context.pending():
            context.iteration(False)
        if (root / 'changed').exists() and any('text-changed:insert' in e for e in events):
            break
        time.sleep(0.03)
    assert Atspi.Text.get_text(text, 0, 9) == "UPDATED!!"
    assert Atspi.Text.get_caret_offset(text) == 11
    assert Atspi.Text.get_n_selections(text) == 0
    assert any('text-changed:delete' in e for e in events), events
    assert any('text-changed:insert' in e for e in events), events
    assert any('text-caret-moved' in e for e in events), events
    assert any('text-selection-changed' in e for e in events), events
    (root / 'quit').touch()
    print(json.dumps({'text': expected, 'caret_before': 9, 'caret_after': 11,
                      'selection_before': [0, 9], 'selection_after': [],
                      'events': events, 'display': os.environ.get('DISPLAY'),
                      'orca_generated_speech': orca_speech}, ensure_ascii=False))


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('--binary', type=Path, default=Path('build/development/tests/test-ui'))
    parser.add_argument('--dbus-daemon', default=shutil.which('dbus-daemon'))
    parser.add_argument('--probe', type=Path)
    parser.add_argument('--orca', action='store_true', help='Check installed Orca readers and speech generation; no audio playback')
    parser.add_argument('--in-xvfb', action='store_true', help=argparse.SUPPRESS)
    args = parser.parse_args()
    if args.probe:
        probe(args.probe, args.orca)
        return 0
    # Never reuse an inherited desktop/Xwayland display for this fixture.
    if not args.in_xvfb:
        return subprocess.call(['xvfb-run', '-a', sys.executable, str(Path(__file__).resolve()),
                                '--in-xvfb', '--binary', str(args.binary.resolve()),
                                '--dbus-daemon', args.dbus_daemon or ''] + (['--orca'] if args.orca else []))
    if not args.dbus_daemon:
        print('SKIP: dbus-daemon unavailable; provide --dbus-daemon PATH')
        return 77
    registry = shutil.which('at-spi2-registryd')
    if not registry:
        registry = next((str(p) for p in [Path('/usr/libexec/at-spi2-registryd'),
                        Path('/usr/lib/at-spi2-core/at-spi2-registryd')] if p.exists()), None)
    if not registry:
        print('SKIP: AT-SPI registry unavailable')
        return 77
    with tempfile.TemporaryDirectory(prefix='termuni-private-atspi-') as directory:
        root = Path(directory)
        env = os.environ.copy()
        for key in ['DBUS_SESSION_BUS_ADDRESS', 'AT_SPI_BUS_ADDRESS', 'DBUS_STARTER_ADDRESS',
                    'DBUS_STARTER_BUS_TYPE', 'SESSION_MANAGER', 'NO_AT_BRIDGE']:
            env.pop(key, None)
        for key in ['HOME', 'XDG_CONFIG_HOME', 'XDG_DATA_HOME', 'XDG_CACHE_HOME', 'XDG_STATE_HOME', 'XDG_RUNTIME_DIR']:
            path = root / key
            path.mkdir(mode=0o700)
            env[key] = str(path)
        env.update(GSETTINGS_BACKEND='memory', GIO_USE_VFS='local', GTK_A11Y='atspi',
                   GDK_BACKEND='x11', TERMUNI_ATSPI_FIXTURE=str(root))
        config = root / 'bus.conf'
        # No service directories: do not autostart portals/keyrings on this test bus.
        config.write_text('<busconfig><type>session</type><listen>unix:tmpdir=' + str(root) +
            '</listen><auth>EXTERNAL</auth><policy context="default"><allow send_destination="*"/>'
            '<allow receive_sender="*"/><allow own="*"/></policy></busconfig>')
        procs, logs = [], []
        def start(command, name, address=False):
            log = open(root / (name + '.log'), 'w+')
            logs.append(log)
            p = subprocess.Popen(command, env=env, stdout=subprocess.PIPE if address else log,
                                 stderr=log, text=True, start_new_session=True)
            procs.append(p)
            if address:
                value = p.stdout.readline().strip()
                assert value.startswith('unix:'), value
                return value
            return p
        try:
            command = [args.dbus_daemon, '--nofork', '--nopidfile', '--print-address=1', '--config-file=' + str(config)]
            env['DBUS_SESSION_BUS_ADDRESS'] = start(command, 'session-bus', True)
            env['AT_SPI_BUS_ADDRESS'] = start(command, 'a11y-bus', True)
            start([registry], 'registry')
            app = start([str(args.binary.resolve()), '-p', '/ui/accessible-text-atspi-fixture'], 'application')
            probe_command = [sys.executable, str(Path(__file__).resolve()), '--probe', str(root)]
            if args.orca:
                probe_command.append('--orca')
            subprocess.run(probe_command, env=env, check=True, timeout=25)
            assert app.wait(timeout=8) == 0
        except BaseException:
            for log in logs:
                log.flush(); log.seek(0)
                print(log.read(), file=sys.stderr)
            raise
        finally:
            (root / 'quit').touch()
            for p in reversed(procs):
                if p.poll() is None:
                    p.terminate()
                    try:
                        p.wait(timeout=3)
                    except subprocess.TimeoutExpired:
                        p.kill(); p.wait()
            for log in logs:
                log.close()
    return 0

if __name__ == '__main__':
    sys.exit(main())
