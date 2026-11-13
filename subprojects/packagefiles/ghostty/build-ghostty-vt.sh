#!/bin/sh
set -eu

zig=$1
source_dir=$2
prefix=$3
stamp=$4

version=$($zig version)
if [ "$version" != 0.16.0 ]; then
  echo "libghostty-vt requires Zig 0.16.0, found $version" >&2
  exit 1
fi

cd "$source_dir"
# TERMAC_GHOSTTY_ZIG_FLAGS lets tools/valgrind-churn.sh build without SIMD, which valgrind cannot run.
# shellcheck disable=SC2086
"$zig" build -Demit-lib-vt ${TERMAC_GHOSTTY_ZIG_FLAGS:-} --prefix "$prefix"
test -f "$prefix/include/ghostty/vt.h"
test -f "$prefix/lib/libghostty-vt.a"
touch "$stamp"
