# Quetoo Android — Gradle / Android Studio project

This is the Android application module for the Quetoo port
([jdolan/quetoo#856](https://github.com/jdolan/quetoo/issues/856)). It wraps the
native engine — built by the NDK via CMake — in **SDL3's** Android Java glue.

> Status: **scaffold**. This builds *once its dependencies are present* (NDK,
> CMake, the SDL3 Java glue, and `android/CMakeLists.txt`). Items marked
> `TODO(version)` in the Gradle files must be pinned to your installed tooling.

## Layout

```
android/
  CMakeLists.txt              <- NDK build entry (SEPARATE task; referenced here)
  app/
    settings.gradle           <- single-module project; wrapper instructions
    build.gradle              <- AGP, SDKs, NDK, externalNativeBuild -> ../CMakeLists.txt
    gradle.properties         <- AndroidX + Gradle daemon/build settings
    proguard-rules.pro        <- keep rules for SDL/JNI (R8 safety net)
    README.md                 <- this file
    src/main/
      AndroidManifest.xml     <- landscape, GLES 3.0 required, QuetooActivity
      java/org/quetoo/android/
        QuetooActivity.java   <- extends SDLActivity; getLibraries() -> SDL3, main
      java/org/libsdl/app/    <- (you add) SDL3 Java glue — see below
      assets/                 <- (optional) bundled default.pk3 — see below
```

## The native build contract (with `android/CMakeLists.txt`)

`build.gradle` points `externalNativeBuild.cmake.path` at `../CMakeLists.txt`
(i.e. `android/CMakeLists.txt`). That CMake project is produced by a separate
task; this module only **consumes** it. The contract this scaffold assumes:

- The engine entry target is built as a **shared library named `main`** →
  `libmain.so`. SDL's `<SDL3/SDL_main.h>` renames the engine `main()`
  (in `src/main/main.c`) to `SDL_main`, and `SDLActivity` invokes it.
- `libSDL3.so` is built (or consumed prebuilt) by that same CMake project and
  ends up in the APK's `jniLibs`. `QuetooActivity.getLibraries()` loads
  `SDL3` then `main`.
- The loadable game modules `game.so` / `cgame.so` are produced too, but are
  **`dlopen`'d at runtime** by the engine (not loaded by SDL). They must land in
  the per-ABI native lib dir so the dynamic loader can find them.
- ABIs are selected by Gradle (`ndk.abiFilters` = `arm64-v8a`, `x86_64`);
  CMake is invoked once per ABI by AGP.
- Compile-time switch `-DQUETOO_GLES=ON` is passed for the GL ES 3.0 code path
  (see `../PORTING.md` §4).

If any of those names differ in the final CMakeLists, update
`getLibraries()` and `build.gradle` to match.

## SDL3 Java glue — how it's sourced

SDL ships its Android Java layer (`org.libsdl.app.SDLActivity` and friends) as
**plain `.java` source files**, not a Maven artifact. `QuetooActivity` subclasses
`SDLActivity`, so those sources must be on the app's compile path. Pick ONE:

1. **Vendor the sources (preferred — guarantees a version match).** Copy the glue
   from the SDL checkout used by the native build:
   ```
   SDL/android-project/app/src/main/java/org/libsdl/app/*.java
       ->  android/app/src/main/java/org/libsdl/app/
   ```
   The Java glue version **must match** the `libSDL3.so` it talks to (the JNI
   signatures are version-locked), so copy from the *same* SDL revision CMake
   links. This scaffold's package layout already expects them here.

   > These files are NOT included in this scaffold (they are SDL's, GPL-incompatible
   > to vendor blindly — SDL is zlib-licensed, which is fine to include, but they
   > are intentionally left for you to copy from the pinned SDL revision rather
   > than fork a stale copy).

2. **Point a source set at the SDL checkout** (no copy). In `build.gradle`:
   ```groovy
   android.sourceSets.main.java.srcDirs += '/path/to/SDL/android-project/app/src/main/java'
   ```

3. **Consume an SDL `.aar`** if/when you publish/obtain one for your SDL revision:
   ```groovy
   dependencies { implementation 'org.libsdl:sdl3:<version>' } // TODO(version)
   ```
   Then remove the vendored `org/libsdl/app` sources to avoid duplicate classes.

## Game data (`default.pk3`)

Quetoo needs its game data (`default.pk3`, maps, etc.). The writable user data
dir on Android is `SDL_GetPrefPath()` (app-private storage); see `../PORTING.md`
§4–§5. Two supported strategies:

1. **First-run download via the engine updater (recommended; smallest APK).**
   The desktop engine already downloads/installs game data on first launch
   (`Installer_Init` in `src/main/main.c`, README "On first launch … download").
   Keep that path on Android: ship no data in the APK, let the updater populate
   `SDL_GetPrefPath()` on first run. Requires `INTERNET` (already in the
   manifest). This matches how every desktop platform ships.

2. **Bundle `default.pk3` as an APK asset (offline-capable, larger APK).**
   Drop the pk3 into `src/main/assets/`:
   ```
   android/app/src/main/assets/default.pk3
   ```
   Assets are read-only and *not* a real filesystem path, so on first run the
   engine must copy `default.pk3` out of the APK (via `AAssetManager`) into
   `SDL_GetPrefPath()` before PhysicsFS mounts it. (That copy step is engine-side
   work tracked under PORTING.md item 7; this module only places the asset.)
   Note Android's 100 MB base-APK limit — large data may force an asset pack or
   strategy (1) regardless.

Strategy (1) needs nothing in this module; strategy (2) needs the asset file
plus the engine-side extraction. They are not mutually exclusive (bundle a
minimal pk3, download the rest).

## Building

### 1. Generate the Gradle wrapper (one-time, you must run this)

This scaffold deliberately does **not** include the wrapper binary
(`gradle/wrapper/gradle-wrapper.jar`) or `gradlew`/`gradlew.bat`. With a system
Gradle installed, generate them from this directory:

```sh
cd android/app
gradle wrapper --gradle-version 8.9
```

(Gradle 8.9 pairs with AGP 8.7.x; bump both together — see the `TODO(version)`
notes in `build.gradle`. After this step, prefer `./gradlew` over a system
Gradle so the build is reproducible.)

### 2. Pin tooling versions

Open `build.gradle` / `gradle.properties` and resolve every `TODO(version)`:
- **AGP** (`com.android.application` plugin version) — 8.6.1+ required for
  `compileSdk 35`.
- **`compileSdk` / `targetSdk`** — 35 (Android 15) as scaffolded.
- **`minSdk`** — 24 (Android 7.0; ~95% coverage, Vulkan-capable, per #856).
- **`ndkVersion`** — an NDK installed via the SDK Manager (r27 LTS scaffolded).
- **CMake `version`** — match the one bundled with that NDK / Android Studio.

### 3. Build the APK

```sh
./gradlew assembleDebug
# output: build/outputs/apk/debug/app-debug.apk
```

Android Studio: *Open* the `android/app` directory as a project; it will pick up
`settings.gradle` and prompt to install any missing NDK/CMake.

## Notes

- **Orientation:** locked to `landscape` in the manifest.
- **API target in manifest:** `android:isGame`, `appCategory="game"`, and broad
  `configChanges` (so rotation/resize don't recreate the SDL surface).
- **TV:** `LEANBACK_LAUNCHER` + optional touchscreen make it Android-TV eligible;
  not a primary target.
- **Single module:** to keep all scaffold files under `android/app/`, this is a
  one-module Gradle build rooted here (see `settings.gradle`). Splitting SDL into
  its own module later is straightforward.
