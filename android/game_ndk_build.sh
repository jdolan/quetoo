#!/bin/bash
# Cross-compile Quetoo's game module + shared code against qglib with the NDK and
# link libgame.so (arm64-v8a) — the first real engine module for Android. The
# game module is pure gameplay logic (no SDL/Objectively); engine import symbols
# (gi.*) stay undefined and are resolved by the engine at dlopen time.
NDK=${NDK:-/opt/android-ndk-r27c}
CC="$NDK/toolchains/llvm/prebuilt/linux-x86_64/bin/aarch64-linux-android24-clang"
SRC=${SRC:-/root/quetoo-build/src}
QG=${QG:-/root/quetoo-android/android/qglib}
# SDL3 cflags: shared/swap.c uses <SDL3/SDL_endian.h> (header-only byte-swap macros).
# Desktop SDL3 headers suffice for compile; a real arm64 build links SDL3-for-NDK.
SDL3_CFLAGS="$(pkg-config --cflags sdl3 2>/dev/null)"
INC="-I/root/qg-probe/stub -I/root/qg-probe/compat -I$QG -I$SRC -I$SRC/game -I$SRC/game/default -I$SRC/shared -I$SRC/collision -I$SRC/common $SDL3_CFLAGS"
CFLAGS="-std=gnu11 -fPIC -O2 $INC"
B=/tmp/gamebuild; rm -rf "$B"; mkdir -p "$B"; cd "$B"

ok=0; fail=0
for f in "$SRC"/game/default/*.c "$SRC"/shared/*.c; do
  b=$(basename "$f")
  if "$CC" $CFLAGS -c "$f" -o "${b%.c}.o" 2>/tmp/ge; then ok=$((ok+1)); else
    fail=$((fail+1)); echo "FAIL $b"; grep -E "error:" /tmp/ge | head -3; fi
done
echo "game+shared compiled: ok=$ok fail=$fail"
[ $fail -ne 0 ] && exit 1

# qglib objects (game uses the container/primitive subset)
for q in "$QG"/qglib_*.c; do "$CC" $CFLAGS -c "$q" -o "qg_$(basename "${q%.c}").o"; done

# link the shared library; undefined gi.* import symbols are resolved at load
if "$CC" -shared -o libgame.so *.o -Wl,-z,undefs 2>/tmp/le; then
  echo "=== libgame.so: $(stat -c%s libgame.so) bytes (arm64-v8a) ==="
  file libgame.so 2>/dev/null || true
  echo "exports G_LoadGame? $("$NDK"/toolchains/llvm/prebuilt/linux-x86_64/bin/llvm-nm -D libgame.so 2>/dev/null | grep -c ' T G_LoadGame')"
else
  echo "LINK FAILED"; head -20 /tmp/le
fi
