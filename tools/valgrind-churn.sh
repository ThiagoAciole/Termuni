#!/usr/bin/env bash
# Runs the tab churn UI test under valgrind and prints the leak summary.
#
#   tools/valgrind-churn.sh [rounds]    rounds of open-and-close tab (default 10)
#
# Compare two runs of different length: a definite loss that grows with the
# rounds is a leak per tab; one that stays the same is one-time allocation.
# Needs valgrind, xvfb-run and Zig 0.16.0.
set -euo pipefail

root="$(cd "$(dirname "$(readlink -f "${BASH_SOURCE[0]}")")/.." && pwd)"
rounds="${1:-10}"
logdir="${TERMAC_VALGRIND_LOGDIR:-$(mktemp -d -t termac-valgrind-XXXXXX)}"
log="$logdir/valgrind-rounds-$rounds.log"

for tool in valgrind xvfb-run; do
	command -v "$tool" >/dev/null || { echo "missing: $tool" >&2; exit 2; }
done
# valgrind cannot run the SIMD code of libghostty-vt, so it gets its own build
# tree with SIMD off. The first run builds it (a few minutes).
vbuild="$root/build-valgrind"
if [ ! -d "$vbuild" ]; then
	(cd "$root" && TERMAC_GHOSTTY_ZIG_FLAGS=-Dsimd=false meson setup build-valgrind -Dprofile=development)
fi
(cd "$root" && TERMAC_GHOSTTY_ZIG_FLAGS=-Dsimd=false meson compile -C build-valgrind >/dev/null)

set +e
GSETTINGS_BACKEND=memory GDK_BACKEND=x11 TERMAC_CHURN_ROUNDS="$rounds" \
	xvfb-run -a valgrind --leak-check=full --show-leak-kinds=definite \
	--errors-for-leak-kinds=definite --error-exitcode=0 \
	--suppressions="$root/tools/valgrind.supp" --log-file="$log" \
	"$vbuild/tests/test-ui" -p /ui/window-tabs -p /ui/tab-churn >"$logdir/test-$rounds.out" 2>&1
status=$?
set -e

echo "test exit status: $status   log: $log"
grep -E 'definitely lost:|Invalid (read|write)|Conditional jump|uninitialised' "$log" | sort | uniq -c || true
echo "--- Termac frames in definite losses ---"
grep -E 'termac_[a-z_]+' "$log" | sed -E 's/.*(termac_[a-z_0-9]+).*/\1/' | sort | uniq -c | sort -rn | head -10 || true
exit 0
