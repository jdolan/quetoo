#!/bin/bash
# Stand up the Android SDK + emulator + an x86_64 AVD (KVM-accelerated) for
# testing the Quetoo port. Run on a host with KVM + Java. x86_64 image
# runs natively-fast under KVM; the engine/deps will be built for x86_64 too for
# the emulator (arm64-v8a remains the device deliverable).
set -e
export ANDROID_SDK_ROOT=$HOME/android-sdk
mkdir -p "$ANDROID_SDK_ROOT/cmdline-tools"
cd "$ANDROID_SDK_ROOT/cmdline-tools"
if [ ! -d latest ]; then
  echo "downloading cmdline-tools..."
  wget -q https://dl.google.com/android/repository/commandlinetools-linux-11076708_latest.zip -O ct.zip
  unzip -q ct.zip && rm ct.zip && mv cmdline-tools latest
fi
SM="$ANDROID_SDK_ROOT/cmdline-tools/latest/bin/sdkmanager"
yes | "$SM" --sdk_root="$ANDROID_SDK_ROOT" --licenses >/dev/null 2>&1 || true
echo "installing platform-tools, emulator, platform-34, system image..."
"$SM" --sdk_root="$ANDROID_SDK_ROOT" "platform-tools" "emulator" "platforms;android-34" "build-tools;34.0.0" "system-images;android-34;google_apis;x86_64" >/tmp/sdk.log 2>&1 \
  && echo SDK_OK || { echo SDK_FAIL; tail -25 /tmp/sdk.log; exit 1; }
AVDM="$ANDROID_SDK_ROOT/cmdline-tools/latest/bin/avdmanager"
echo no | "$AVDM" create avd -n quetoo -k "system-images;android-34;google_apis;x86_64" -d pixel_6 --force >/tmp/avd.log 2>&1 \
  && echo AVD_OK || { echo AVD_FAIL; cat /tmp/avd.log; exit 1; }
"$ANDROID_SDK_ROOT/platform-tools/adb" version | head -1
echo "SDK ready at $ANDROID_SDK_ROOT ; AVD 'quetoo' created"
