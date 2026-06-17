#!/bin/bash
# NDK compile-probe of the engine CORE (everything the dedicated server needs:
# common/collision/net/server/shared/game) against qglib + the cross-built deps
# (SDL3, PhysicsFS, Objectively headers). Reports how many compile clean under
# Bionic and the precise remaining blockers. Compile-only (no link).
NDK=/opt/android-ndk-r27c
CC="$NDK/toolchains/llvm/prebuilt/linux-x86_64/bin/aarch64-linux-android24-clang"
SRC=/root/quetoo-build/src
QG=/root/quetoo-android/android/qglib
INC="-I/root/qg-probe/stub -I/root/qg-probe/compat -I$QG -I$SRC -I$SRC/shared -I$SRC/common -I$SRC/collision -I$SRC/net -I$SRC/server -I$SRC/game -I$SRC/game/default -I/root/SDL/include -I/root/physfs/src -I/root/Objectively/Sources"
CFLAGS="-std=gnu11 -fPIC -O2 -D_GNU_SOURCE $INC"
cd /tmp; rm -rf core; mkdir core; cd core
ok=0; fail=0
for f in "$SRC"/common/*.c "$SRC"/collision/*.c "$SRC"/net/*.c "$SRC"/server/*.c "$SRC"/shared/*.c; do
  b=$(basename "$f")
  if "$CC" $CFLAGS -c "$f" -o "${b%.c}.o" 2>/tmp/ce; then ok=$((ok+1)); else
    fail=$((fail+1))
    sym=$(grep -oE "undeclared (function|identifier) '[A-Za-z0-9_]+'|'[A-Za-z0-9_/.]+' file not found" /tmp/ce | head -1)
    [ -z "$sym" ] && sym=$(grep -E "error:" /tmp/ce | head -1 | sed -E "s#.*error: ##")
    echo "  FAIL $b  --  $sym"
  fi
done
echo "==== engine core (server-side) NDK: ok=$ok fail=$fail ===="
