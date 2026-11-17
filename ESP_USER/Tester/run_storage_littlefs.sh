#!/usr/bin/env bash
# Run production storage on the ESP8266 upstream LittleFS version in virtual NOR.
set -euo pipefail
DIR="$(cd "$(dirname "$0")" && pwd)"
SRC="${LITTLEFS_SOURCE:?Set LITTLEFS_SOURCE to littlefs sources at 6a53d76e90af33f0656333c1db09bd337fa75d23}"
BUILD="${SIMPLE_TEST_BUILD:-/tmp/changeover_storage_littlefs}"
mkdir -p "$BUILD"
gcc -O1 -g -DLFS_NO_DEBUG -DLFS_NO_WARN -DLFS_NO_ERROR -c "$SRC/lfs.c" -o "$BUILD/lfs.o"
gcc -O1 -g -c "$SRC/lfs_util.c" -o "$BUILD/lfs_util.o"
g++ -std=gnu++17 -O1 -g -Wall -Wextra -Werror -Wno-unused-function -Wno-unused-variable \
 -I "$DIR/../Simulator" -I "$DIR/../Simulator/stubinc" -I "$SRC" \
 "$DIR/host_test_storage_littlefs.cpp" "$BUILD/lfs.o" "$BUILD/lfs_util.o" \
 -Wl,--wrap=fopen,--wrap=fileno,--wrap=ftruncate -o "$BUILD/storage_littlefs"
for profile in 0 1 2; do "$BUILD/storage_littlefs" "$profile"; done
