#!/bin/bash
# Build the FULL Quetoo Android dependency stack for one ABI into a prefix.
# Usage: build_all_ndk.sh <abi> <prefix>   e.g. build_all_ndk.sh x86_64 /root/android-x86_64
#   ABIs: arm64-v8a | x86_64    (clang target derived below)
# Re-uses the already-cloned sources under /root (SDL, SDL_image, SDL_ttf,
# openal-soft, physfs, libsndfile, Objectively, ObjectivelyMVC) + the qglib shim.
set -e
ABI="${1:-x86_64}"
PREFIX="${2:-/root/android-$ABI}"
NDK=/opt/android-ndk-r27c
API=24
TC="$NDK/build/cmake/android.toolchain.cmake"
case "$ABI" in
  arm64-v8a) TRIPLE=aarch64-linux-android ;;
  x86_64)    TRIPLE=x86_64-linux-android ;;
  *) echo "unknown ABI $ABI"; exit 1 ;;
esac
CC="$NDK/toolchains/llvm/prebuilt/linux-x86_64/bin/${TRIPLE}${API}-clang"
mkdir -p "$PREFIX"
# -Wl,-z,max-page-size=16384: 16 KB ELF segment alignment for arm64 Android (#856).
CM="-DCMAKE_TOOLCHAIN_FILE=$TC -DANDROID_ABI=$ABI -DANDROID_PLATFORM=android-$API -DCMAKE_PREFIX_PATH=$PREFIX -DCMAKE_FIND_ROOT_PATH=$PREFIX -DCMAKE_FIND_ROOT_PATH_MODE_PACKAGE=BOTH -DCMAKE_BUILD_TYPE=Release -DCMAKE_SHARED_LINKER_FLAGS=-Wl,-z,max-page-size=16384"

cmake_dep() {
  local dir="$1"; shift
  echo "=== $dir ($ABI) ==="
  cd "/root/$dir"; rm -rf "ba-$ABI"; mkdir "ba-$ABI"; cd "ba-$ABI"
  cmake $CM "$@" .. >cm.log 2>&1 && cmake --build . -j6 >b.log 2>&1 && cmake --install . --prefix "$PREFIX" >/dev/null 2>&1 \
    && echo "$dir OK" || { echo "$dir FAIL"; tail -15 cm.log b.log 2>/dev/null; exit 1; }
}

cmake_dep SDL        -DSDL_SHARED=ON -DSDL_STATIC=OFF -DSDL_TEST_LIBRARY=OFF
cmake_dep SDL_image  -DSDLIMAGE_SAMPLES=OFF -DSDLIMAGE_TESTS=OFF -DSDLIMAGE_VENDORED=OFF -DBUILD_SHARED_LIBS=ON
cmake_dep SDL_ttf    -DSDLTTF_VENDORED=ON -DSDLTTF_SAMPLES=OFF -DBUILD_SHARED_LIBS=ON
cmake_dep openal-soft -DALSOFT_EXAMPLES=OFF -DALSOFT_UTILS=OFF -DALSOFT_TESTS=OFF -DLIBTYPE=SHARED
cmake_dep physfs     -DPHYSFS_BUILD_TEST=OFF -DPHYSFS_BUILD_SHARED=ON -DPHYSFS_BUILD_STATIC=ON
cmake_dep libsndfile -DBUILD_TESTING=OFF -DBUILD_PROGRAMS=OFF -DBUILD_EXAMPLES=OFF -DENABLE_EXTERNAL_LIBS=OFF -DBUILD_SHARED_LIBS=ON

# --- compat-shim static libs (qglib, Objectively, ObjectivelyMVC) ---
QG=/root/quetoo-android/android/qglib
COMPAT=/root/obj-compat
CFLAGS="-std=gnu11 -fPIC -O2 -I$COMPAT -include $COMPAT/android_compat.h"
mkdir -p "$PREFIX/include" "$PREFIX/lib"
echo "=== qglib ($ABI) ==="
cd "$QG"; rm -rf "ba-$ABI"; mkdir "ba-$ABI"; cd "ba-$ABI"
for q in "$QG"/qglib_*.c; do "$CC" -std=gnu11 -fPIC -O2 -c "$q" -o "$(basename "${q%.c}").o"; done
ar rcs "$PREFIX/lib/libqglib.a" *.o; cp "$QG"/qglib*.h "$PREFIX/include/" && echo "qglib OK"
echo "=== Objectively ($ABI) ==="
cd /root/Objectively; rm -rf "ba-$ABI"; mkdir "ba-$ABI"; cd "ba-$ABI"
for f in /root/Objectively/Sources/Objectively/*.c; do b=$(basename "$f"); case "$b" in URLSession*.c) continue;; esac
  "$CC" $CFLAGS -I/root/Objectively/Sources -c "$f" -o "${b%.c}.o"; done
"$CC" $CFLAGS -I/root/Objectively/Sources -c "$COMPAT/android_compat.c" -o ac.o
"$CC" $CFLAGS -I/root/Objectively/Sources -c "$COMPAT/iconv.c" -o ic.o
ar rcs "$PREFIX/lib/libObjectively.a" *.o; cp -r /root/Objectively/Sources/Objectively "$PREFIX/include/" && echo "Objectively OK"
echo "=== ObjectivelyMVC ($ABI) ==="
cd /root/ObjectivelyMVC; rm -rf "ba-$ABI"; mkdir "ba-$ABI"; cd "ba-$ABI"
for f in /root/ObjectivelyMVC/Sources/ObjectivelyMVC/*.c; do b=$(basename "$f")
  "$CC" $CFLAGS -Wno-deprecated-declarations -I/root/ObjectivelyMVC/Sources -I/root/Objectively/Sources -I"$PREFIX/include" -I"$PREFIX/include/SDL3" -c "$f" -o "${b%.c}.o"; done
ar rcs "$PREFIX/lib/libObjectivelyMVC.a" *.o; cp -r /root/ObjectivelyMVC/Sources/ObjectivelyMVC "$PREFIX/include/" && echo "ObjectivelyMVC OK"

echo "==== FULL STACK ($ABI) staged in $PREFIX ===="
ls "$PREFIX/lib"/lib*.{so,a} 2>/dev/null | xargs -n1 basename | sort -u
