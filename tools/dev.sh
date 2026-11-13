#!/usr/bin/env bash
# Builds Termac and starts the development build.
#
#   tools/dev.sh            build and start (does nothing if already running)
#   tools/dev.sh --restart  build, stop the running instance, and start again
#
# Stopping the running instance closes its shell and tabs.
set -euo pipefail

root="$(cd "$(dirname "$(readlink -f "${BASH_SOURCE[0]}")")/.." && pwd)"
build="$root/build"
binary="$build/src/termac"
log="${TERMAC_DEV_LOG:-$HOME/.cache/termac/dev.log}"

restart=0
case "${1:-}" in
	--restart) restart=1 ;;
	"") ;;
	*) echo "usage: $0 [--restart]" >&2; exit 2 ;;
esac

if [ ! -d "$build" ]; then
	meson setup "$build" "$root" -Dprofile=development
fi
meson compile -C "$build"

if pgrep -x termac >/dev/null; then
	if [ "$restart" -eq 0 ]; then
		echo "Termac is already running; use --restart to start the new build." >&2
		exit 0
	fi
	pkill -x termac || true
	for _ in $(seq 1 50); do
		pgrep -x termac >/dev/null || break
		sleep 0.1
	done
fi

# Compile the translations into the build directory for the development build.
locale_dir="$build/locale"
while read -r lang; do
	case "$lang" in ''|\#*) continue ;; esac
	mkdir -p "$locale_dir/$lang/LC_MESSAGES"
	msgfmt -o "$locale_dir/$lang/LC_MESSAGES/termac.mo" "$root/po/$lang.po"
done < "$root/po/LINGUAS"

mkdir -p "$(dirname "$log")"
TERMAC_LOCALEDIR="$locale_dir" setsid "$binary" >"$log" 2>&1 < /dev/null &
echo "Termac dev started (log: $log)"
