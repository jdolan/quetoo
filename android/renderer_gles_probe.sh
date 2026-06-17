#!/bin/bash
# Compile the renderer for GL ES (x86_64, emulator ABI) against the staged deps +
# glad-GLES loader, to surface the remaining GLES breakers as real errors.
NDK=/opt/android-ndk-r27c; PREFIX=/root/android-x86_64
CC="$NDK/toolchains/llvm/prebuilt/linux-x86_64/bin/x86_64-linux-android24-clang"
SRC=/root/quetoo-android/src
R="$SRC/client/renderer"
INC="-I/root/qg-probe/stub -I/root/qg-probe/compat -I/root/quetoo-android/android/qglib -I$SRC -I$SRC/client -I$R -I$SRC/shared -I$SRC/common -I$SRC/collision -I$PREFIX/include -I$PREFIX/include/SDL3 -I/root/Objectively/Sources -I/root/ObjectivelyMVC/Sources"
CFLAGS="-std=gnu11 -fPIC -O2 -DQUETOO_GLES -D_GNU_SOURCE $INC"
cd /tmp; rm -rf rgles; mkdir rgles; cd rgles
ok=0; fail=0
for f in "$R"/*.c; do
  b=$(basename "$f")
  if "$CC" $CFLAGS -c "$f" -o "${b%.c}.o" 2>/tmp/re; then ok=$((ok+1)); else
    fail=$((fail+1)); echo "FAIL $b: $(grep -m1 -E 'error:|file not found' /tmp/re | sed -E 's#.*(error:|fatal error:) ##')"
  fi
done
echo "renderer GLES x86_64: ok=$ok fail=$fail"
