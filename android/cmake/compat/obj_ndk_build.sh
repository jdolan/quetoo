#!/bin/bash
# Cross-compile Objectively into libObjectively.a with the Android NDK, using the
# compat shims in this directory (qsort_r via android_compat, iconv via iconv.c)
# and deferring the libcurl-dependent URLSession layer. See android/DEPENDENCIES.md.
set -e
NDK=${NDK:-/opt/android-ndk-r27c}
CC="$NDK/toolchains/llvm/prebuilt/linux-x86_64/bin/aarch64-linux-android24-clang"
OBJ=${OBJ:-/root/Objectively}
COMPAT="$(cd "$(dirname "$0")" && pwd)"
BUILD=/root/obj-ndk-build
rm -rf "$BUILD"; mkdir -p "$BUILD"; cd "$BUILD"

# iconv.h (this dir) shadows the absent Bionic one; android_compat.h force-included.
CFLAGS="-std=gnu11 -fPIC -O2 -Wall -I$OBJ/Sources -I$COMPAT -include $COMPAT/android_compat.h"

objs=""
"$CC" $CFLAGS -c "$COMPAT/android_compat.c" -o android_compat.o; objs="$objs android_compat.o"
"$CC" $CFLAGS -c "$COMPAT/iconv.c" -o iconv_compat.o;            objs="$objs iconv_compat.o"

ok=0; skip=0; fail=0
for f in "$OBJ"/Sources/Objectively/*.c; do
  b=$(basename "$f")
  case "$b" in URLSession*.c) skip=$((skip+1)); continue ;; esac  # HTTP deferred (#856)
  if "$CC" $CFLAGS -c "$f" -o "${b%.c}.o" 2>/tmp/oe; then
    objs="$objs ${b%.c}.o"; ok=$((ok+1))
  else
    fail=$((fail+1)); echo "FAIL $b"; head -6 /tmp/oe
  fi
done

if [ $fail -eq 0 ]; then
  ar rcs libObjectively.a $objs
  echo "=== libObjectively.a: $(stat -c%s libObjectively.a) bytes, $(ar t libObjectively.a | wc -l) objects (arm64-v8a) ==="
  echo "objectively: compiled=$ok, URLSession deferred=$skip, failed=$fail"
else
  echo "FAILURES: $fail"; exit 1
fi
