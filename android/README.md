# Quetoo — Android Platform Port

Port of Quetoo to Android (SDL3 + OpenGL ES 3.0), tracking
**[jdolan/quetoo#856](https://github.com/jdolan/quetoo/issues/856)**. Shares the
GL ES renderer, the glib2 replacement, and virtual-controls work with the iOS port
[#855](https://github.com/jdolan/quetoo/issues/855).

> **Status:** client builds for `arm64-v8a` (device) and `x86_64` (emulator), packages
> into a signed APK, boots through full init to a rendered GL ES 3.0 **main menu**, and
> **renders maps in-game with full textures + lighting** on real hardware (tested on a
> Samsung Galaxy S24 Ultra, Adreno 750). Client-only; desktop GL builds are unchanged.

## Documents

| File | Contents |
|---|---|
| [`PORT_CHANGES.md`](./PORT_CHANGES.md) | **Retrospective: everything that had to change to run on Android** (start here). |
| [`PORTING.md`](./PORTING.md) | The up-front technical assessment, strategy, and work breakdown. |
| [`RENDERER_GLES.md`](./RENDERER_GLES.md) | GL ES 3.0 renderer audit + findings (A–J). |
| [`DEPENDENCIES.md`](./DEPENDENCIES.md) | Dependency stack and NDK cross-build status. |
| [`ASSETS.md`](./ASSETS.md) · [`LIFECYCLE.md`](./LIFECYCLE.md) · [`CONTROLS.md`](./CONTROLS.md) | Asset packaging, app lifecycle/filesystem, virtual controls. |

## Layout

```
android/
  README.md               <- this file
  PORT_CHANGES.md         <- what changed (retrospective)
  PORTING.md              <- assessment / plan
  CMakeLists.txt, cmake/  <- NDK build entry (shared with iOS #855)
  app/                    <- Android app (SDLActivity subclass + manifest)
  qglib/                  <- in-tree glib2 replacement (portable C + SDL3)
  build_all_ndk.sh        <- cross-build SDL3/image/ttf, OpenAL, PhysicsFS, sndfile
  build_http_stack.sh     <- cross-build mbedTLS + libcurl + libObjectively (HTTPS)
  build_apk_manual.sh     <- assemble + sign the APK without Gradle
  *.md                    <- subsystem notes
```

The port sits alongside the existing `apple/` and `linux/` platform directories; engine
source under `src/` is shared, with Android specifics gated behind `QUETOO_GLES` (renderer)
and `__ANDROID__` (platform). See [`PORT_CHANGES.md`](./PORT_CHANGES.md) for the full story.
