#!/usr/bin/env bash
# Local equivalent of the native CI job; never installs into the host prefix.
set -euo pipefail
cd "$(dirname "$(readlink -f "$0")")/.."
profile="${1:-development}"
case "$profile" in development|default) ;; *) echo 'profile: development or default' >&2; exit 2;; esac
build="${TERMUNI_CI_BUILD:-build/ci/$profile}"
setup=()
if [ -f "$build/meson-private/coredata.dat" ]; then setup+=(--reconfigure); fi
meson setup "$build" --buildtype=debugoptimized -Dprofile="$profile" --prefix=/usr "${setup[@]}"
meson compile -C "$build"
meson test -C "$build" --print-errorlogs
for catalog in po/*.po; do msgfmt --check --check-format -o /dev/null "$catalog"; done
python3 tools/check-agent-boundary.py src
stage_root="$(realpath "$build")/ci-install"
meson install -C "$build" --no-rebuild --destdir "$stage_root"
id=io.github.ThiagoAciole.Termuni
if [ "$profile" = development ]; then id+=.Devel; fi
stage="$stage_root/usr/share"
test -f "$stage/applications/$id.desktop"
test -f "$stage/metainfo/$id.metainfo.xml"
test -f "$stage/glib-2.0/schemas/$id.gschema.xml"
test -f "$stage/icons/hicolor/scalable/apps/$id.svg"
glib-compile-schemas --strict --dry-run "$stage/glib-2.0/schemas"
echo "PASS: $profile build, tests, translations and isolated installation"
