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
"$zig" build -Demit-lib-vt --prefix "$prefix"
test -f "$prefix/include/ghostty/vt.h"
test -f "$prefix/lib/libghostty-vt.a"
touch "$stamp"
