#!/bin/bash
# Cross-build the remaining client audio deps (OpenAL-soft, libsndfile) for
# Android arm64 into the staging prefix. Uses the NDK toolchain + the
# FIND_ROOT_PATH fix so find_package sees the prefix.
set -e
NDK=/opt/android-ndk-r27c
PREFIX=/root/android-arm64
TC="$NDK/build/cmake/android.toolchain.cmake"
CM="-DCMAKE_TOOLCHAIN_FILE=$TC -DANDROID_ABI=arm64-v8a -DANDROID_PLATFORM=android-24 -DCMAKE_PREFIX_PATH=$PREFIX -DCMAKE_FIND_ROOT_PATH=$PREFIX -DCMAKE_FIND_ROOT_PATH_MODE_PACKAGE=BOTH -DCMAKE_BUILD_TYPE=Release"

cd /root
[ -d openal-soft ] || git clone --depth 1 https://github.com/kcat/openal-soft.git >/dev/null 2>&1
cd openal-soft; rm -rf ba; mkdir ba; cd ba
echo "=== OpenAL-soft ==="
if cmake $CM -DALSOFT_EXAMPLES=OFF -DALSOFT_UTILS=OFF -DALSOFT_TESTS=OFF -DLIBTYPE=SHARED .. >cm.log 2>&1 \
   && cmake --build . -j6 >b.log 2>&1 && cmake --install . --prefix "$PREFIX" >/dev/null 2>&1; then
  echo OPENAL_OK
else
  echo OPENAL_FAIL; tail -20 cm.log b.log 2>/dev/null
fi

cd /root
[ -d libsndfile ] || git clone --depth 1 https://github.com/libsndfile/libsndfile.git >/dev/null 2>&1
cd libsndfile; rm -rf ba; mkdir ba; cd ba
echo "=== libsndfile ==="
if cmake $CM -DBUILD_TESTING=OFF -DBUILD_PROGRAMS=OFF -DBUILD_EXAMPLES=OFF -DENABLE_EXTERNAL_LIBS=OFF -DBUILD_SHARED_LIBS=ON .. >cm.log 2>&1 \
   && cmake --build . -j6 >b.log 2>&1 && cmake --install . --prefix "$PREFIX" >/dev/null 2>&1; then
  echo SNDFILE_OK
else
  echo SNDFILE_FAIL; tail -20 cm.log b.log 2>/dev/null
fi

echo "=== staged libs ==="
ls -la "$PREFIX"/lib/lib*.so 2>/dev/null
