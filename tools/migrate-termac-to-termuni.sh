#!/bin/sh
# One-time copy of Termac user data to the Termuni locations.
# Copies only: the old files stay in place, so removing the old package keeps them.
set -eu

OLD_DCONF=/thiago/aciole/Termac/
NEW_DCONF=/io/github/ThiagoAciole/Termuni/
OLD_STATE="${XDG_STATE_HOME:-$HOME/.local/state}/termac"
NEW_STATE="${XDG_STATE_HOME:-$HOME/.local/state}/termuni"
OLD_CACHE="${XDG_CACHE_HOME:-$HOME/.cache}/termac"
NEW_CACHE="${XDG_CACHE_HOME:-$HOME/.cache}/termuni"

if [ -n "$(dconf list "$NEW_DCONF" 2>/dev/null || true)" ]; then
    echo "Settings already exist at $NEW_DCONF; not overwriting." >&2
else
    dconf dump "$OLD_DCONF" | dconf load "$NEW_DCONF"
    echo "Settings copied: $OLD_DCONF -> $NEW_DCONF"
fi

for pair in "$OLD_STATE:$NEW_STATE" "$OLD_CACHE:$NEW_CACHE"; do
    old=${pair%%:*}
    new=${pair#*:}
    if [ -d "$old" ] && [ ! -e "$new" ]; then
        cp -a "$old" "$new"
        echo "Copied $old -> $new"
    else
        echo "Skipped $old (missing, or $new already exists)"
    fi
done
