#!/bin/bash
# Cross-compile ObjectivelyMVC into libObjectivelyMVC.a with the Android NDK.
# Uses the compat shims (qsort_r/iconv) + the staged SDL3/SDL3_ttf/SDL3_image +
# Objectively headers. Android GL = GLES3 (NDK sysroot provides <GLES3/gl3.h>).
set -e
NDK=/opt/android-ndk-r27c
PREFIX=/root/android-arm64
CC="$NDK/toolchains/llvm/prebuilt/linux-x86_64/bin/aarch64-linux-android24-clang"
MVC=/root/ObjectivelyMVC
OBJ=/root/Objectively
COMPAT=/root/obj-compat
CFLAGS="-std=gnu11 -fPIC -O2 -Wno-deprecated-declarations -I$MVC/Sources -I$OBJ/Sources -I$PREFIX/include -I$PREFIX/include/SDL3 -I$COMPAT -include $COMPAT/android_compat.h"
B=/tmp/mvcbuild; rm -rf "$B"; mkdir -p "$B"; cd "$B"
ok=0; fail=0
for f in "$MVC"/Sources/ObjectivelyMVC/*.c; do
  b=$(basename "$f")
  if "$CC" $CFLAGS -c "$f" -o "${b%.c}.o" 2>/tmp/mvce; then ok=$((ok+1)); else
    fail=$((fail+1)); echo "FAIL $b: $(grep -m1 -E 'error:|file not found' /tmp/mvce | sed -E 's#.*(error:|fatal error:) ##')"
  fi
done
echo "MVC compiled: ok=$ok fail=$fail"
if [ $fail -eq 0 ]; then
  ar rcs libObjectivelyMVC.a *.o
  echo "=== libObjectivelyMVC.a: $(stat -c%s libObjectivelyMVC.a) bytes, $(ar t libObjectivelyMVC.a | wc -l) objects (arm64) ==="
fi
