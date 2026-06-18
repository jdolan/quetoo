#!/bin/bash
#
# build_apk_manual.sh — assemble a signed, installable Quetoo APK from prebuilt
# native libraries WITHOUT Gradle, using only the Android SDK build-tools.
#
# Context: the LXC build host has the NDK (cross-compiles libmain/game/cgame.so
# + the SDL3/OpenAL/... deps) but no Android SDK/Gradle. The native libs are
# staged here under $STAGING/jniLibs/<abi>/ and packaged into an APK on a host
# that has the SDK. This is the jniLibs path: Gradle would otherwise
# re-run CMake, which needs the NDK + cross-deps that live on the build container.
#
# Pipeline: javac -> d8 -> aapt2 link (manifest only, no res) -> add dex+libs
# -> zipalign -> apksigner (debug key). Produces $OUT_APK.
#
# Env (with defaults):
#   SDK     ~/android-sdk
#   BTVER   34.0.0   (build-tools)
#   APIJAR  android-34
#   ABI     x86_64
#   WORK    ~/quetoo-apk    (expects $WORK/staging/{jniLibs,java,AndroidManifest.xml})
set -uo pipefail

SDK="${SDK:-$HOME/android-sdk}"
BTVER="${BTVER:-34.0.0}"
APIJAR="${APIJAR:-android-34}"
ABI="${ABI:-x86_64}"
WORK="${WORK:-$HOME/quetoo-apk}"

BT="$SDK/build-tools/$BTVER"
AJ="$SDK/platforms/$APIJAR/android.jar"
ST="$WORK/staging"
OUT="$WORK/out"
OUT_APK="$WORK/quetoo-$ABI.apk"

step() { echo; echo "== $* =="; }
die()  { echo "FATAL: $*" >&2; exit 1; }

[ -f "$AJ" ] || die "android.jar not found: $AJ"
[ -d "$ST/jniLibs/$ABI" ] || die "staged libs not found: $ST/jniLibs/$ABI"

rm -rf "$OUT"; mkdir -p "$OUT/classes" "$OUT/dex"

# Gradle injects the package from build.gradle's `namespace`; a manual aapt2
# link needs it spelled out on the <manifest> tag.
if ! grep -q 'package=' "$ST/AndroidManifest.xml"; then
  sed -i 's#xmlns:tools="http://schemas.android.com/tools">#xmlns:tools="http://schemas.android.com/tools"\n    package="org.quetoo.android">#' "$ST/AndroidManifest.xml"
fi

# Hand-built APKs are most robust when Android extracts the .so to the app's
# nativeLibraryDir at install time. Ensure the flag is set on <application>.
if ! grep -q 'extractNativeLibs' "$ST/AndroidManifest.xml"; then
  sed -i 's/android:hasCode="true"/android:hasCode="true"\n        android:extractNativeLibs="true"/' "$ST/AndroidManifest.xml"
fi

step "javac"
find "$ST/java" -name '*.java' > "$OUT/sources.txt"
echo "  $(wc -l < "$OUT/sources.txt") sources"
javac -d "$OUT/classes" -classpath "$AJ" -source 17 -target 17 \
  -encoding UTF-8 @"$OUT/sources.txt" 2>"$OUT/javac.log"
rc=$?; echo "  javac rc=$rc"; [ $rc -eq 0 ] || { tail -25 "$OUT/javac.log"; die "javac failed"; }

step "d8 (classes.dex)"
"$BT/d8" --min-api 24 --output "$OUT/dex" \
  $(find "$OUT/classes" -name '*.class') 2>"$OUT/d8.log"
rc=$?; echo "  d8 rc=$rc"; [ $rc -eq 0 ] || { tail -25 "$OUT/d8.log"; die "d8 failed"; }

step "aapt2 link (base.apk)"
"$BT/aapt2" link -I "$AJ" --manifest "$ST/AndroidManifest.xml" \
  --min-sdk-version 24 --target-sdk-version 34 \
  -o "$OUT/base.apk" 2>"$OUT/aapt2.log"
rc=$?; echo "  aapt2 rc=$rc"; [ $rc -eq 0 ] || { tail -25 "$OUT/aapt2.log"; die "aapt2 link failed"; }

step "assemble (dex + native libs + assets)"
cd "$OUT" || die "cd $OUT"
cp dex/classes.dex .
mkdir -p "lib/$ABI"
cp "$ST/jniLibs/$ABI"/*.so "lib/$ABI/"
ASSET=""
[ -d "$ST/assets" ] && { cp -r "$ST/assets" .; ASSET="assets"; }
# the build host has no `zip`; use python's zipfile. Native .so are STORED (uncompressed)
# so zipalign -p can page-align them; dex/assets are deflated.
python3 - "$OUT/base.apk" "$ABI" "$ASSET" <<'PY'
import sys, os, glob, zipfile
apk, abi, asset = sys.argv[1], sys.argv[2], sys.argv[3]
z = zipfile.ZipFile(apk, 'a', zipfile.ZIP_DEFLATED)
z.write('classes.dex', 'classes.dex')
libs = sorted(glob.glob('lib/%s/*.so' % abi))
for so in libs:
    z.write(so, so, compress_type=zipfile.ZIP_STORED)
if asset and os.path.isdir(asset):
    for root, _, files in os.walk(asset):
        for f in files:
            p = os.path.join(root, f)
            z.write(p, p)
z.close()
print('  packaged dex + %d libs%s' % (len(libs), ' + assets' if asset else ''))
PY
[ $? -eq 0 ] || die "python zip assembly failed"

step "zipalign"
"$BT/zipalign" -p -f 4 "$OUT/base.apk" "$OUT/aligned.apk" || die "zipalign failed"

step "debug keystore"
if [ ! -f "$WORK/debug.keystore" ]; then
  keytool -genkeypair -keystore "$WORK/debug.keystore" \
    -storepass android -keypass android -alias androiddebugkey \
    -keyalg RSA -keysize 2048 -validity 10000 \
    -dname "CN=Android Debug,O=Android,C=US" 2>/dev/null || die "keytool failed"
  echo "  created $WORK/debug.keystore"
fi

step "apksigner"
"$BT/apksigner" sign --ks "$WORK/debug.keystore" \
  --ks-pass pass:android --key-pass pass:android \
  --out "$OUT_APK" "$OUT/aligned.apk" 2>"$OUT/sign.log"
rc=$?; echo "  apksigner rc=$rc"; [ $rc -eq 0 ] || { tail -25 "$OUT/sign.log"; die "apksigner failed"; }

"$BT/apksigner" verify "$OUT_APK" >/dev/null 2>&1 \
  && echo "  APK_SIGNED_OK" || echo "  WARN: signature verify failed"

echo
echo "BUILT: $OUT_APK"
ls -la "$OUT_APK"
