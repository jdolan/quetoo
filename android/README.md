# Quetoo — Android Platform Port

Private WIP port of Quetoo to Android (SDL3 + OpenGL ES 3.0).
Tracks upstream **[jdolan/quetoo#856](https://github.com/jdolan/quetoo/issues/856)**.

> Status: **scaffolding**. No Android build exists yet. Development is local-only
> on branch `feature/android-port`; this private repo is the home for the effort.

## Layout (planned)

```
android/
  README.md          <- this file
  PORTING.md         <- technical assessment, strategy, and work breakdown
  app/               <- (planned) Android Studio / Gradle project
  CMakeLists.txt     <- (planned) NDK build entry, shared with iOS (#855)
```

The port sits alongside the existing `apple/` and `linux/` platform directories.
Engine source under `src/` is shared; Android-specific additions live here and in
a future root-level `CMakeLists.txt`.

## Quick orientation

- **Base engine:** `Eclipse1982/quetoo` `main` (this branch was cut from it).
- **Upstream issue:** #856 (Android), shares renderer + glib2 + virtual-controls
  work with the iOS port #855.
- **First work item:** glib2 replacement (the shared mobile prerequisite). See
  [`PORTING.md`](./PORTING.md).

## Why a private repo

This is exploratory work that modifies the engine broadly (renderer, build system,
a glib compatibility layer). Keeping it private and isolated from the public fork
avoids churn on `Eclipse1982/quetoo` until the port is coherent enough to upstream
as PR(s) against #856 / #855.
