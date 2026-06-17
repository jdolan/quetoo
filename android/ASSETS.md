# Quetoo Android — Asset Packaging & First-Run Data

How the Quetoo Android port
([jdolan/quetoo#856](https://github.com/jdolan/quetoo/issues/856), work item
**#7**) gets its read-only game data (`default.pk3` and friends) onto the device
and into the engine's PhysicsFS search path.

This documents the **engine-side** decision and its single touch point in
`src/common/filesystem.c` (`Fs_Init`), and how it ties into the existing
first-run updater (`Installer_Init`) and `SDL_GetPrefPath`. The Gradle/app-side
summary lives in [`app/README.md` § "Game data (`default.pk3`)"](./app/README.md);
this file is the authoritative description of how the two halves connect.

---

## TL;DR

- Read-only assets ship **inside the APK** under
  `android/app/src/main/assets/default/default.pk3`.
- On **first run**, `Fs_Init` extracts that asset into app-internal storage and
  mounts the extracted copy with PhysicsFS. On **every later run** it skips
  extraction and just mounts the copy already on disk.
- Writable data (configs, screenshots, demos) and the extracted read-only data
  both live under `SDL_GetPrefPath("WickedOldGames", "Quetoo")` — on Android that
  is the app-private internal-storage dir (`Context.getFilesDir()`), e.g.
  `/data/data/org.quetoo.android/files/WickedOldGames/Quetoo/`.
- The existing `Installer_Init` network updater (used on desktop) still works on
  Android and can fetch/refresh content into the same `data_dir` afterwards — the
  bundled pk3 is just the offline-capable baseline.

---

## 1. Why a copy is needed at all (APK assets are not real files)

PhysicsFS mounts real directories and archive files on the device filesystem
(`PHYSFS_mount` → see `Fs_AddToSearchPath` in `src/common/filesystem.c`). Android
**APK assets are not real files**: they live compressed inside the `.apk` and are
reachable only through Android's `AAssetManager`. So a bundled `default.pk3`
cannot be mounted in place — it must first be materialised as an actual file in a
writable location.

SDL3 bridges this without any JNI glue: on Android, **`SDL_IOFromFile()` opens a
*relative* path against the APK `assets/` directory** (it probes internal storage
first, then falls back to the asset manager). We use that to read the bundled
pk3, and write a real copy into `SDL_GetPrefPath`-backed internal storage, which
PhysicsFS can then mount like on any other platform.

---

## 2. Where the asset lives in the Gradle project

Place the bundled archive at:

```
android/app/src/main/assets/default/default.pk3
```

`assets/` is the Android-standard location and needs **no** extra Gradle
configuration (`build.gradle` already notes this; AGP packs everything under
`src/main/assets/` into the APK verbatim).

### Layout choice: `assets/default/default.pk3`, not `assets/default.pk3`

The engine addresses game content as `<game>/<archive>` — the default game is
`DEFAULT_GAME` = `"default"` (see `src/common/common.h`), and on every platform
the search path is built from `data_dir/default/*.pk3` (see the
`Fs_AddToSearchPathv(fs_state.data_dir, DEFAULT_GAME, NULL)` calls in `Fs_Init`).

To keep the Android asset tree a **mirror** of the on-device `data_dir` tree, the
asset path includes the `default/` game subdir. The engine extracts the asset
`default/default.pk3` to `<data_dir>/default/default.pk3`, so the on-device
layout matches the desktop layout exactly and the existing
`Fs_AddToSearchPathv(..., DEFAULT_GAME, NULL)` mounts pick it up unchanged.

> Note: an earlier draft of `app/README.md` showed a flat `assets/default.pk3`.
> The engine implementation expects the **`assets/default/default.pk3`** layout
> documented here (game-subdir mirrored). If you place it flat, either move it or
> adjust the asset path string in `Fs_Android_ExtractAsset`'s caller in `Fs_Init`.

---

## 3. First-run extraction — the engine side (`src/common/filesystem.c`)

All Android-specific logic is confined to the `#elif defined(__ANDROID__)` arm of
the platform `switch` in `Fs_Init`, plus a static helper
`Fs_Android_ExtractAsset` (also `#if defined(__ANDROID__)`-guarded). Desktop
builds are entirely unaffected — the arm is preprocessed out when `__ANDROID__`
is not defined.

### Path resolution

```
data_dir  = Sys_UserDir() + "/data"      // Sys_UserDir() == SDL_GetPrefPath(...)
base_dir  = data_dir
lib_dir   = data_dir
```

`Sys_UserDir()` (in `src/common/sys.c`) is already `SDL_GetPrefPath`-backed, so
on Android it resolves to app-internal storage with no runtime permission and no
new code. A `data` subdir keeps engine-managed content separate from user-owned
files (configs/screenshots/demos), which continue to land directly under
`Sys_UserDir()` via `Fs_AddUserSearchPath` / `Fs_SetWriteDir` (the writable path
is unchanged — see [`LIFECYCLE.md` §6](./LIFECYCLE.md)).

> The desktop arms (`__APPLE__` / `__linux__` / `_WIN32`) derive `base_dir` from
> `Sys_ExecutablePath()`. On Android the engine runs as `libmain.so` loaded by
> SDL's `SDLActivity`, so there is no install tree to parse; the Android arm
> ignores `path` (`(void) path;`) and derives everything from `Sys_UserDir()`.

### Extraction (first run only)

`Fs_Android_ExtractAsset("default/default.pk3", "<data_dir>/default")`:

1. If `<data_dir>/default/default.pk3` already exists → return (no-op). This is
   the common case after first launch, **and** it deliberately does not overwrite
   a copy the updater may have refreshed (see §4).
2. Otherwise open the APK asset with `SDL_IOFromFile("default/default.pk3", "rb")`
   (relative path → APK `assets/`), slurp it with `SDL_LoadFile_IO`, `g_mkdir_*`
   the destination tree, and write it out with `SDL_IOFromFile(dest, "wb")` /
   `SDL_WriteIO` / `SDL_CloseIO`.

After the `switch`, the **existing** unconditional mounts run for all platforms:

```c
Fs_AddToSearchPathv(fs_state.data_dir, DEFAULT_GAME, NULL);  // mounts <data_dir>/default (and its *.pk3)
Fs_AddUserSearchPath(DEFAULT_GAME);                          // writable user dir + write target
```

Because `FS_AUTO_LOAD_ARCHIVES` is set (`Fs_Init(FS_AUTO_LOAD_ARCHIVES)` in
`src/main/main.c`), mounting the `default` directory auto-mounts the
`default.pk3` inside it — no explicit pk3 mount call is needed.

### Failure handling

Extraction failure is **non-fatal**: it warns to the log (visible in `logcat`)
rather than aborting, because a dev build may sideload data via a `-path` CLI arg
or rely on the updater. A clear warning makes a missing-assets misconfiguration
obvious immediately instead of surfacing later as "no maps / no models".

### TODO left in code

`Fs_Android_ExtractAsset` currently extracts **only `default.pk3`**. A
`// TODO(#856)` in the Android arm describes extending it to extract *all*
bundled archives once the shipped asset set is finalised. `AAssetManager`
directory enumeration is not exposed through SDL, so the recommended approach is
a **build-time-generated manifest** (e.g. `assets/default/manifest.txt` listing
the pk3s) that the extractor reads and iterates, extracting each archive with the
same IOStream copy.

---

## 4. How it ties into `Installer_Init` (first-run updater)

The desktop engine already has a first-run / update path:
`Installer_Init(...)` (`src/common/installer.c`, called from `Com_Init` in
`src/main/main.c` right after `Fs_Init`) compares a local data version against a
remote one and downloads/installs official content into `Fs_DataDir()`. After it
runs, `main.c` re-adds the data search paths:

```c
Installer_Init(dedicated->value ? Sv_InstallerFrame : Cl_InstallerFrame);
Fs_AddToSearchPathv(Fs_DataDir(), NULL);
Fs_AddToSearchPathv(Fs_DataDir(), game->string, NULL);
```

On Android `Fs_DataDir()` now returns `<Sys_UserDir()>/data`, so the updater
writes into the **same tree** our APK extraction populated. Ordering and
interaction:

1. `Fs_Init` runs **first** and guarantees a baseline `default.pk3` exists on
   disk (extracted from the APK) — so the game is playable offline even if the
   network/updater is unavailable.
2. `Installer_Init` runs **after** and may fetch newer content into the same
   `data_dir`. Because our extractor **never overwrites an existing file**, a
   newer installer-provided `default.pk3` is preserved across relaunches — we do
   not clobber it with the older bundled baseline.
3. `Installer_Init` early-returns when `version->integer == -1` (the scaffold's
   default `VERSION`/`PACKAGE_VERSION` is `"-1"`), so on a dev build the updater
   is effectively disabled and the **bundled asset is the sole data source** —
   which is exactly why bundling + first-run extraction is the right default for
   the port.

### Bundle vs. download — the decision

| Strategy | APK size | Offline | Engine work |
|---|---|---|---|
| **Bundle in APK + first-run extract** (this doc) | larger | yes | the `__ANDROID__` arm (done) |
| Download-only via `Installer_Init` | smallest | no (first run needs network) | none beyond path resolution |

**Decision: bundle + first-run extract**, for two reasons: (a) it makes the app
playable offline out of the box, and (b) with the scaffold's `VERSION == "-1"`
the network updater is disabled, so a bundled baseline is required for the engine
to find any data at all. The two are **not mutually exclusive** — ship a minimal
pk3 in the APK and let `Installer_Init` pull the rest — and Android's ~100–200 MB
base-APK limit may *force* this hybrid (large data → an
[asset pack](https://developer.android.com/guide/playcore/asset-delivery) or
download) regardless. The engine code already supports the hybrid: extract
whatever ships, then let the updater add more into the same dir.

---

## 5. Summary

| Concern | Where | Status |
|---|---|---|
| Bundled asset location | `android/app/src/main/assets/default/default.pk3` | place the file (Gradle: no config needed) |
| Path resolution (read base/data dir) | `Fs_Init` `#elif defined(__ANDROID__)` arm, `src/common/filesystem.c` | **done** — `data_dir = Sys_UserDir()/data` |
| First-run extract | `Fs_Android_ExtractAsset`, `src/common/filesystem.c` | **done** for `default.pk3`; `TODO(#856)` to generalise to all archives |
| Mount | existing `Fs_AddToSearchPathv(data_dir, DEFAULT_GAME, NULL)` + `FS_AUTO_LOAD_ARCHIVES` | **done** — no Android-specific mount code |
| Writable dir | `SDL_GetPrefPath` → `Sys_UserDir` → `Fs_SetWriteDir` | unchanged (see `LIFECYCLE.md` §6) |
| Content updates | `Installer_Init` (`src/main/main.c`, `src/common/installer.c`) writes into the same `data_dir` | works as-is; extractor won't clobber updater data |
