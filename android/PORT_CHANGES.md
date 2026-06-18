# Quetoo on Android — What Had to Change

A retrospective companion to [`PORTING.md`](./PORTING.md) (the up-front assessment/plan).
This document is the *outcome*: the concrete changes that took Quetoo from a desktop
GL 4.x engine to a client that **boots to the menu and renders maps in-game on a real
Android device** (Samsung Galaxy S24 Ultra, Adreno 750, Android 15/16, GL ES 3.2 driver),
tracking [jdolan/quetoo#856](https://github.com/jdolan/quetoo/issues/856).

The port is **client-only** (no dedicated server build; the listen-server code is kept but
the curses console and the legacy HTTP client are stubbed). Everything is gated so the
desktop GL/Win/Linux/macOS builds are unchanged — engine-side switches are `#ifdef QUETOO_GLES`
(C) and `#ifdef GL_ES` (GLSL); platform switches are `#ifdef __ANDROID__`.

---

## 1. Platform & build system

Android is the Linux *kernel* with the **Bionic** libc and **no GNU userspace** — every
dependency is cross-compiled against the **NDK** (r27c) and shipped inside the APK.

- **Root `android/CMakeLists.txt` + `cmake/`** — NDK build entry for `libmain.so`,
  `libcgame.so`, `libgame.so`. Shared toolchain settings with the iOS effort (#855).
- **Dependency stack, all cross-compiled for `arm64-v8a` (device) and `x86_64` (emulator)**
  via `android/build_all_ndk.sh`: SDL3, SDL3_image, SDL3_ttf, OpenAL-soft, PhysicsFS,
  libsndfile. See [`DEPENDENCIES.md`](./DEPENDENCIES.md).
- **Objectively / ObjectivelyMVC** have no NDK build upstream — they are cross-compiled
  here too (static `.a` for the compat libs, shared `.so` for Objectively/MVC). MVC's
  official `main` already ships an **OpenGL ES 3.0 renderer**, which is used as-is.
- **APK assembly without Gradle** — `android/build_apk_manual.sh` does the
  `javac → d8 → aapt2 link → zip → zipalign → apksigner` pipeline directly, so the build
  host needs only the SDK build-tools (no Gradle/AGP). `android/app/` holds the
  `SDLActivity` subclass + manifest.
- **16 KB page alignment** — Android 15 flags, and future devices require, 16 KB-aligned
  ELF load segments. All shipped `.so` are linked with `-Wl,-z,max-page-size=16384`
  (engine via `add_link_options`; deps via `CMAKE_SHARED_LINKER_FLAGS`; the direct-clang
  Objectively/MVC links inline). Verify with `llvm-readelf -lW <so> | grep LOAD` → `0x4000`.

---

## 2. glib2 → `qglib` in-tree shim

Quetoo includes `<glib.h>` centrally via `src/quetoo.h`; glib has no NDK build. Rather than
port glib (which drags in libffi/pcre/intl/iconv for ~5% actually used), `android/qglib/`
implements exactly the subset Quetoo uses — primitives delegate to SDL3, and the seven
container types (`GArray`, `GList`, `GSList`, `GPtrArray`, `GQueue`, `GHashTable`, `GString`)
plus `GChecksum`/`GDateTime`/`GRand`/`GRegex` are reimplemented with glib-faithful semantics.
It is differential-tested against system glib-2.0 (≈350 checks, identical results), so it is a
drop-in on **every** platform. Full rationale + API inventory in [`PORTING.md`](./PORTING.md) §3.

---

## 3. Platform / system layer (`#ifdef __ANDROID__`)

| Area | Change |
|---|---|
| Entry point | `#include <SDL3/SDL_main.h>` in `main.c` (Android needs SDL's `main` shim). |
| PhysicsFS | Init with `PHYSFS_AndroidInit{ SDL_GetAndroidJNIEnv(), SDL_GetAndroidActivity() }`. |
| Module loading | `Sys_OpenLibrary` loads by soname (`dlopen("lib<name>.so")`) — `game.so`/`cgame.so` stay `dlopen`'d (Android supports it). |
| Filesystem | Writable user dir via `SDL_GetPrefPath()`; assets extracted from the APK (see §4). |
| Logging | `Com_Print` → `__android_log` (logcat tag `Quetoo`); `Com_Warn` → the on-device `quetoo.log`. |
| Crash handler | `Sys_CrashSignal` unwinds via `_Unwind_Backtrace` + `dladdr` and logs ndk-stack-style frames to logcat (Bionic `backtrace()` is API 33+, we target 24, so `HAVE_EXECINFO` is off). |
| Desktop-only ops | `Sys_InstallDesktopEntry` / `Sys_InstallLocalBin` are guarded `&& !defined(__ANDROID__)` — `__linux__` is true on Bionic, so they otherwise ran and crashed the SDL thread. |
| Fullscreen | `r_fullscreen` defaults to exclusive on Android (immersive, hides system bars) + manifest `windowLayoutInDisplayCutoutMode=shortEdges`. No 1080p mode swap — use `r_framebuffer_scale` for 3D render resolution. |

---

## 4. Asset packaging & the extraction shadow

Game data ships as `assets/default/default.pk3` in the APK, extracted on first run to
`SDL_GetPrefPath()/default/` and auto-mounted (`FS_AUTO_LOAD_ARCHIVES`).

**Gotcha worth upstreaming a real fix for:** `SDL_IOFromFile` resolves the *writable*
extracted copy before the APK asset, so a stale extracted pk3 is never replaced on update.
A size-check re-extract was added, but the clean fix is to read the asset via
`AAssetManager` directly. (During development, pushing the pk3 over `run-as` sidesteps it.)

---

## 5. GL ES 3.0 renderer

The renderer targets **`#version 300 es`** (GL ES 3.0) for device coverage, even though the
test device exposes ES 3.2 — so the shaders avoid 3.1/3.2-only features. Almost all of the
shader changes are plain **GLSL-ES-3.0 strictness vs desktop GL 4.x** and apply equally to
the iOS 3.0 target (#855). Deep dive in [`RENDERER_GLES.md`](./RENDERER_GLES.md); summary:

### 5a. Shader (GLSL) changes
- **`#version 300 es` preamble** (`shaders/version_es.glsl`, swapped in for `version.glsl`
  when `QUETOO_GLES`): ES requires explicit **default precision** qualifiers for `float`/`int`
  and every sampler type — added once here, gating all shaders with no per-file edits.
- **in/out interface blocks → named struct varyings.** `out vertex_data { … } vertex;` needs
  GLSL ES 3.20; rewritten as `struct … { }; out … vertex;` (valid on ES 3.00 *and* desktop).
- **Implicit int/uint→float conversions** that desktop allows but ES rejects:
  `ticks * .001` → `float(ticks) * …`; `1.0/textureSize()` (ivec) and `gl_FragCoord.xy/viewport`
  → `vec2(...)`; `uint age/lifetime` → `float(...)`; int literal into a `mat2` element → `0.0`.
- **`textureQueryLod`** (GL 4.0 / ES 3.2) → reconstructed from screen-space derivatives:
  `lod = 0.5*log2(max(dot(dPdx,dPdx), dot(dPdy,dPdy)))`.
- **`textureLod` with an integer LOD** → float (`6` → `6.0`).
- **`isamplerBuffer` / texture-buffer objects** (GL 3.1+/ES 3.2): the voxel light-index TBO
  is repacked to an **integer 2D texture** on ES (`ivec2(i % W, i / W)`).
- **`sampler2DMS` / MSAA** (depth-resolve, ES 3.1) — guarded desktop-only.
- **`MAX_LIGHTS` 96 → 32** — the UBO-backed `lights[MAX_LIGHTS]` array overran
  `GL_MAX_VERTEX_UNIFORM_VECTORS` (256) on stricter drivers (a *link* failure that offline
  validators miss). C (`r_types.h`) and GLSL (`uniforms.glsl`) kept in lockstep.

### 5b. Renderer (C) changes
- **`R_DrawElementsBaseVertex`** is ES 3.2 core / `OES`|`EXT`, not ES 3.0 — resolved at
  runtime via `SDL_GL_GetProcAddress`, falling back to plain `glDrawElements`. (A no-op stub
  silently broke occlusion-query culling → black world, and all mesh/shadow draws.)
- **`glBlitFramebuffer`** (scene FBO → default framebuffer) throws `GL_INVALID_OPERATION`
  on Adreno — on ES the final tonemap pass renders **directly to the default framebuffer**
  instead of blitting.
- Texture readback (`glGetTexImage`), BGR swizzles, and the glad **GLES loader** have ES seams.

### 5c. The nasty runtime bug — Adreno `inout` + early `return`
Compiles clean, only shows at runtime, and was the cause of the world rendering as **flat
per-texture average colors** while meshes were perfect:

`bsp_fs`'s `parallax_occlusion_mapping(in vertex, inout fragment)` sets
`fragment.parallax = vertex.diffusemap` then `return`s early when POM is disabled. **The
Adreno GLSL compiler drops the `inout` write-back across that early return**, so
`fragment.parallax` reached the caller divergent → the diffuse `texture()` computed a huge
implicit LOD off it → every BSP surface sampled its top (flat) mip. Meshes sample at their
own texcoord (no POM) and were unaffected. **Fix:** set the texcoord in the *caller* scope
and skip the POM call on ES (an in-*function* `#ifdef GL_ES return` does **not** work — same
dropped write-back). This one is likely Adreno-specific; the §5a items are not.

---

## 6. Online stack (HTTPS)

Upstream routes HTTP through Objectively's `RESTClient → URLSession → libcurl`, and the
endpoints (`giblets.quetoo.org`, S3) are HTTPS. `android/build_http_stack.sh` cross-builds
**mbedTLS → libcurl (`-DCURL_USE_MBEDTLS=ON`) → libObjectively.so** (with the
`URLSession`/`RESTClient`/JSON layer). libcurl(mbedTLS) has no system trust store, so a
**`cacert.pem`** is shipped as a real file (`Sys_UserDir()/default/cacert.pem`, extracted
like the pk3) and wired via `CURLOPT_CAINFO`.

---

## 7. Known issues / remaining work

- **Shadow atlas FBO** comes back incomplete on Adreno (depth-atlas format) — shadows
  disabled gracefully; needs an ES-compatible depth atlas.
- **`R_CopyFramebufferAttachment`** throws `GL_INVALID_OPERATION` on the ES copy path.
- **Virtual on-screen controls** (cgame overlay, shared with #855) are scaffolded
  (`CONTROLS.md`, `cl_touch.{h,c}`) but not yet wired to input.
- Full game data is ~1.2 GB — production needs on-demand asset download rather than a
  monolithic APK.
- The pk3 **extraction shadow** (§4) deserves an `AAssetManager`-based fix.

---

## 8. Building

```
# Dependencies (per ABI):  android/build_all_ndk.sh <abi> <prefix>
#                          android/build_http_stack.sh <abi> <prefix>
# Engine:                  cmake -S android -B <build> -DQUETOO_GLES=ON \
#                            -DCMAKE_TOOLCHAIN_FILE=$NDK/build/cmake/android.toolchain.cmake \
#                            -DANDROID_ABI=<abi> -DANDROID_PLATFORM=android-24 ...
# APK:                     android/build_apk_manual.sh
```

See [`DEPENDENCIES.md`](./DEPENDENCIES.md), [`RENDERER_GLES.md`](./RENDERER_GLES.md),
[`ASSETS.md`](./ASSETS.md), and [`LIFECYCLE.md`](./LIFECYCLE.md) for specifics.
