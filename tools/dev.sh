#!/usr/bin/env bash
# Builds and starts this checkout's development executable.
# --restart closes only this executable's shells and tabs (never other roots).
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

# Serialize two invocations without holding the lock in the launched app.
exec 9>"$root/.termac-dev.lock"
flock -n 9 || { echo "Another development launch is in progress for $root" >&2; exit 1; }

if [ ! -d "$build" ]; then
	meson setup "$build" "$root" -Dprofile=development
fi
# Refuse a default-profile build before compiling or signalling any process.
if ! grep -qxF '#define APPLICATION_ID "thiago.aciole.Termac.Devel"' "$build/config.h"; then
	echo "Refusing to launch/restart: $build is not a development-profile build." >&2
	exit 1
fi
meson compile -C "$build"

matches_binary() {
	local target
	target="$(readlink "/proc/$1/exe" 2>/dev/null)" || return 1
	# A running inode may have been replaced by the build above.
	[[ "$target" == "$binary" || "$target" == "$binary (deleted)" ]]
}
matching_pids() {
	local process pid name
	for process in /proc/[0-9]*; do
		pid="${process##*/}"
		# Cheap candidate filter; executable identity below decides the scope.
		IFS= read -r name < "$process/comm" 2>/dev/null || continue
		[[ "$name" == termac ]] || continue
		if matches_binary "$pid"; then printf '%s\n' "$pid"; fi
	done
}

mapfile -t pids < <(matching_pids)
if [ "${#pids[@]}" -gt 0 ]; then
	if [ "$restart" -eq 0 ]; then
		echo "Development binary $binary is already running (PID: ${pids[*]}); use --restart." >&2
		exit 0
	fi
	echo "Restarting $binary (PID: ${pids[*]}): its shells and tabs will close." >&2
	for pid in "${pids[@]}"; do
		# Recheck identity immediately before signalling; exited/reused PIDs are ignored.
		if matches_binary "$pid"; then kill -TERM "$pid"; fi
	done
	for ((attempt = 0; attempt < 50; attempt++)); do
		mapfile -t pids < <(matching_pids)
		[ "${#pids[@]}" -eq 0 ] && break
		sleep 0.1
	done
	if [ "${#pids[@]}" -gt 0 ]; then
		echo "Development instance did not stop (PID: ${pids[*]}); refusing to start a duplicate." >&2
		exit 1
	fi
fi

locale_dir="$build/locale"
while read -r lang; do
	case "$lang" in ''|\#*) continue ;; esac
	mkdir -p "$locale_dir/$lang/LC_MESSAGES"
	msgfmt -o "$locale_dir/$lang/LC_MESSAGES/termac.mo" "$root/po/$lang.po"
done < "$root/po/LINGUAS"

mkdir -p "$(dirname "$log")"
TERMAC_LOCALEDIR="$locale_dir" setsid "$binary" 9>&- >"$log" 2>&1 < /dev/null &
pid=$!
# Detect startup errors and single-instance handoff instead of claiming success.
sleep 0.5
if ! matches_binary "$pid"; then
	echo "Development binary exited during startup; inspect $log" >&2
	exit 1
fi
echo "Termac dev started (PID: $pid, executable: $binary, log: $log)"
