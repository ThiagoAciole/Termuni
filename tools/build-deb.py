#!/usr/bin/env python3
"""Build a local Ubuntu .deb from the Termuni default Meson profile."""
import argparse
import json
import os
from pathlib import Path
import shutil
import subprocess
import sys
import tempfile


ROOT = Path(__file__).resolve().parent.parent


def run(command, **kwargs):
    print("+", " ".join(map(str, command)), flush=True)
    result = subprocess.run(command, **kwargs)
    if result.returncode != 0:
        if result.stdout:
            print(result.stdout, end="")
        if result.stderr:
            print(result.stderr, end="", file=sys.stderr)
        raise subprocess.CalledProcessError(result.returncode, command)
    return result


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--builddir", type=Path, default=Path("build/package"))
    parser.add_argument("--output", type=Path, default=Path("build/packages"))
    args = parser.parse_args()
    builddir = args.builddir if args.builddir.is_absolute() else ROOT / args.builddir
    output = args.output if args.output.is_absolute() else ROOT / args.output

    for tool in ("meson", "ninja", "dpkg-deb", "dpkg-shlibdeps"):
        if shutil.which(tool) is None:
            parser.error(f"required command not found: {tool}")

    setup = ["meson", "setup", str(builddir), str(ROOT), "--buildtype=release",
             "--prefix=/usr", "-Dprofile=default"]
    if (builddir / "meson-private/coredata.dat").exists():
        options = json.loads(subprocess.check_output(
            ["meson", "introspect", str(builddir), "--buildoptions"], text=True))
        values = {item["name"]: item["value"] for item in options}
        if values.get("profile") != "default" or values.get("prefix") != "/usr":
            parser.error("refusing to reconfigure a builddir that is not already default with prefix /usr; choose a fresh --builddir")
        setup.append("--reconfigure")
    run(setup)
    run(["meson", "compile", "-C", str(builddir)])
    info = json.loads(subprocess.check_output(
        ["meson", "introspect", str(builddir), "--projectinfo"], text=True))

    with tempfile.TemporaryDirectory(prefix="termuni-deb-") as temporary:
        package_root = Path(temporary) / "root"
        package_root.mkdir()
        env = os.environ.copy()
        env["DESTDIR"] = str(package_root)
        run(["meson", "install", "-C", str(builddir), "--no-rebuild"], env=env)

        binary = package_root / "usr/bin/termuni"
        icon = package_root / "usr/share/icons/hicolor/scalable/apps/io.github.ThiagoAciole.Termuni.svg"
        desktop = package_root / "usr/share/applications/io.github.ThiagoAciole.Termuni.desktop"
        if not all(path.is_file() for path in (binary, icon, desktop)):
            parser.error("staged default profile is missing the binary, desktop entry, or app icon")

        metadata = package_root / "DEBIAN"
        metadata.mkdir()
        control = metadata / "control"
        control.write_text(
            "Package: io.github.thiagoaciole.termuni\n"
            f"Version: {info['version']}\n"
            "Section: x11\nPriority: optional\n"
            f"Architecture: {subprocess.check_output(['dpkg', '--print-architecture'], text=True).strip()}\n"
            "Maintainer: Termuni Developers <termuni@localhost>\n"
            "Description: Termuni terminal emulator\n"
            " GTK 4 terminal emulator with tabs and libghostty-vt.\n",
            encoding="utf-8")
        debian_dir = package_root / "debian"
        debian_dir.mkdir()
        (debian_dir / "control").write_text(
            "Source: termuni\n" + control.read_text(encoding="utf-8"), encoding="utf-8")
        result = run(["dpkg-shlibdeps", "-O", f"-e{binary}"], cwd=package_root,
                     capture_output=True, text=True)
        depends = next((line.split("=", 1)[1] for line in result.stdout.splitlines()
                        if line.startswith("shlibs:Depends=")), "")
        shutil.rmtree(debian_dir)
        with control.open("a", encoding="utf-8") as stream:
            stream.write(f"Depends: {depends}\n" if depends else "")

        maintainer_script = """#!/bin/sh
set -e
if [ -x /usr/bin/glib-compile-schemas ]; then /usr/bin/glib-compile-schemas /usr/share/glib-2.0/schemas; fi
if [ -x /usr/bin/gtk4-update-icon-cache ]; then /usr/bin/gtk4-update-icon-cache -q -t -f /usr/share/icons/hicolor; fi
if [ -x /usr/bin/update-desktop-database ]; then /usr/bin/update-desktop-database -q /usr/share/applications; fi
exit 0
"""
        for name in ("postinst", "postrm"):
            script = metadata / name
            script.write_text(maintainer_script, encoding="utf-8")
            script.chmod(0o755)

        output.mkdir(parents=True, exist_ok=True)
        architecture = subprocess.check_output(["dpkg", "--print-architecture"], text=True).strip()
        package = output / f"io.github.thiagoaciole.termuni_{info['version']}_{architecture}.deb"
        run(["dpkg-deb", "--root-owner-group", "--build", str(package_root), str(package)])

    run(["dpkg-deb", "--info", str(package)])
    print(f"Built local package: {package}")


if __name__ == "__main__":
    main()
