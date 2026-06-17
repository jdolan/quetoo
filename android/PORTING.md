# Quetoo Android Port — Technical Assessment & Plan

Tracks [jdolan/quetoo#856](https://github.com/jdolan/quetoo/issues/856).
Shared prerequisites with the iOS port [#855](https://github.com/jdolan/quetoo/issues/855):
GL ES 3.0 renderer, glib2 replacement, virtual controls.

---

## 1. Platform reality: Android is the Linux *kernel*, not a Linux *userspace*

This distinction drives the whole dependency story:

| Layer | Android | Consequence |
|---|---|---|
| Kernel | Modified Linux | syscalls, `dlopen()`, threads, sockets all work → the `game.so`/`cgame.so` module model is preserved (unlike iOS). |
| libc | **Bionic**, not glibc | glibc/GNU assumptions (locale, some pthread ext., full iconv) may be missing/different. |
| Userland | No GNU coreutils, no `/usr`, no package manager | Everything is cross-compiled against the **NDK** and shipped inside the APK. A dependency "just works" only if someone ported it to the NDK toolchain. |

SDL3, OpenAL-soft, PhysicsFS, libcurl all have known NDK builds. **glib2 is the odd
one out** — no official NDK support — which is why it is the first work item.

---

## 2. Reference ports studied (what to copy, what not to)

### austgl/quake2android (vanilla Quake 2, ~2010, API 4)
- **Single static `libquake2.so`** — engine + game + `ref_gl` all statically linked;
  *dodged* `dlopen` entirely (stock Q2 normally uses `gamei386.so` / `ref_gl.so`).
- **OpenGL ES 1.x** fixed-function (Q2's `ref_gl` is fixed-function → near-trivial map).
- Hand-written **Java/JNI glue** (GLSurfaceView, AudioTrack, touch buttons, accel aim).
- **No glib** — vanilla Q2 is C89 with id's own zone allocator.
- *Takeaway:* proves the **static-link fallback** for modules works; everything else
  is obsolete (ndk-build + ant) or now provided by SDL3.

### CarlGammaSagan/QIII4A (ioquake3, via Q3E wrapper)
- ioquake3 base — **same family as Quetoo** (SDL-based, GLSL renderer).
- Module loading: ships **both** `BUILD_GAME_SO` (dlopen'd native modules) **and**
  `BUILD_GAME_QVM` (portable bytecode VM). Proves **dlopen game modules work on Android**.
- Renderer: includes **rend2**, ioq3's GLSL shader renderer, with a **GLES adaptation**.
  Proves adapting a GLSL renderer to GLES on Android is a *solved pattern*, not research.
- **No glib** — ioquake3 has no glib dependency either.

### Cumulative conclusions
1. **dlopen native modules work on Android** → keep Quetoo's `game.so`/`cgame.so`
   (per #856); static-link is a validated fallback if a device misbehaves.
2. **GLSL → GLES is a known quantity** (rend2 did it) — the renderer is real work but
   de-risked. This is the GL ES 3.0 dual code path (`#define QUETOO_GLES`).
3. **SDL3 erases the entire hand-written Java/glue tier** (activity, EGL, audio, input).
4. **glib is the one thing no id-Tech reference port solves** — it never existed in that
   lineage. It is uniquely Quetoo's prerequisite. ← **start here.**

---

## 3. glib2 replacement assessment (the first work item)

Measured against `Eclipse1982/quetoo` `main` (vendored Windows prebuilt headers under
`Quetoo.vs15/` excluded). glib is included centrally via `src/quetoo.h` (`#include <glib.h>`).

### 3a. Good news: no glib threading
`common/thread.c` is built on **SDL** (`SDL_CreateThread`, `SDL_CreateMutex`, …).
Grep for `g_thread_*` / `g_mutex_*` / `g_cond_*` / `g_atomic_*` / `g_async_queue_*`
returns **nothing**. The hardest porting category does not exist here.

No GMainLoop, GObject, GIO, GVariant, GRegex, GSignal usage either.

### 3b. Tier 1 — primitives (high call-count, trivial): map onto SDL3 (already bundled)

| glib | replacement | sites |
|---|---|---|
| `g_strlcpy` / `g_strlcat` | `SDL_strlcpy` / `SDL_strlcat` | 156 / 88 |
| `g_snprintf` | `SDL_snprintf` | 146 |
| `g_ascii_strcasecmp` / `g_ascii_strncasecmp` | `SDL_strcasecmp` / `SDL_strncasecmp` | 17 / 8 |
| `g_str_has_prefix` / `g_str_has_suffix` / `g_str_equal` / `g_str_hash` | tiny inline | 33 / 20 / 7 / 6 |
| `g_strdup` / `g_strdup_printf` / `g_strsplit` / `g_strfreev` | SDL / small helpers | 12 / 3 / 5 / 6 |
| `g_malloc*` / `g_free` / `g_new*` / `g_realloc` | `SDL_malloc` / `SDL_calloc` / `SDL_free` / `SDL_realloc` | — |
| `g_get_monotonic_time` | `SDL_GetTicksNS` (scale) | 4 |
| `g_mkdir_with_parents` / `g_file_test` / `g_build_filename` | `SDL_GetPrefPath` + small fs helpers | 7 / 6 / 5 |

Note `cmd.c` / `cvar.c` use Quetoo-local **`g_stri_hash` / `g_stri_equal`** (case-insensitive)
— these are Quetoo's own functions, not glib, and carry over unchanged.

### 3c. Tier 2 — container types (the real work): 87 distinct functions

Types used: `GArray`, `GList`, `GSList`, `GHashTable` (+`GHashTableIter`), `GPtrArray`,
`GQueue`, `GString`. SDL has no equivalents → these need real implementations.

Pervasive, **including the loadable modules** — so the replacement must link into
`game.so` / `cgame.so`, not just the engine:

| Subsystem | glib call sites |
|---|---|
| common | 424 |
| game (`game.so`) | 282 |
| client | 246 |
| quemap | 153 |
| cgame (`cgame.so`) | 124 |
| collision | 102 |
| server | 67 |
| tests / master / net / shared / main | 44 / 25 / 7 / 14 / 5 |

Distinct container API surface to implement (87 functions):
`g_array_*` (14), `g_hash_table_*` (22), `g_list_*` (21), `g_slist_*` (8),
`g_ptr_array_*` (10), `g_queue_*` (10), `g_string_*` (8).

### 3d. Options

1. **Port glib to the NDK** — ✗ not recommended. No official NDK support; drags in
   libffi / libpcre / libintl / libiconv for ~5% of glib actually used.
2. **Objectively collections** (already a dep; #856's suggestion) — workable but invasive.
   Objectively is a refcounted OO object system; heavier idiom than `GArray`/`GHashTable`,
   adds alloc/retain-release to hot paths (AI nodes, array indexing, quemap). Also doesn't
   cover `GString` or the primitives.
3. ✅ **In-tree compat shim** — a self-contained portable-C library implementing exactly
   the 87-function subset, primitives delegating to SDL3. Keep glib-compatible
   typedefs/signatures so call sites barely change (header swap + mechanical fixes, not a
   rewrite). Removes a heavy external dep on **every** platform (Win/Linux/iOS benefit too),
   matching #856 calling glib2 the *shared* mobile prerequisite.

### 3e. Semantic watch-items (must be faithful)
Exact construction semantics observed in the tree:
- `GHashTable`: `g_hash_table_new_full(hash, equal, key_destroy, value_destroy)` with
  real destructors, e.g. `(g_str_hash, g_str_equal, g_free, g_free)`,
  `(R_MediaHash, R_MediaEqual, NULL, Mem_Free)`, and `g_direct_hash/g_direct_equal`;
  `g_hash_table_new(NULL, NULL)` (direct). Need `GHashTableIter` + `_iter_init/_next/_remove`.
- `GArray`: `g_array_new(zero_terminated, clear, elt_size)` and `g_array_sized_new`;
  `g_array_index` used as an **lvalue** macro; `clear`-on-grow must zero; `ref`/`unref`;
  `remove_index_fast`.
- `GPtrArray`: `g_ptr_array_new_with_free_func(free_fn)` element destructors;
  `sort_values` / `sort_values_with_data`.
- `GList`/`GSList`: `sort` / `sort_with_data` (stable), `find_custom`, `insert_sorted`.

### 3f. Effort
Matches #856's **1–2 weeks**. Container impls (~few hundred LOC each) + mechanical
migration. No threading work. The shim is the long pole that is genuinely Quetoo-specific.

---

## 4. Strategy decisions (carried into the work breakdown)

- **Modules:** keep `dlopen`'d `game.so`/`cgame.so` (Android supports it; QIII4A proves it).
  Keep a static-link build option as fallback (austgl proves it).
- **Graphics:** OpenGL ES 3.0 via a `#define QUETOO_GLES` dual path (shared w/ #855).
  Vulkan deferred (near-total renderer rewrite).
- **Platform layer:** SDL3 (activity, EGL/GL context, audio backend, input, lifecycle).
- **Filesystem:** `SDL_GetPrefPath()` for writable user data; `pk3` in the APK or
  first-run download via the existing updater.
- **Build:** add a root `CMakeLists.txt` (NDK, benefits iOS too) + a Gradle/Android
  Studio project under `android/app/`. ABIs: `arm64-v8a` (primary), `x86_64` (emulator),
  `armeabi-v7a` (optional).

---

## 5. Work breakdown

| # | Item | Depends on | Est. |
|---|---|---|---|
| 1 | **glib2 compat shim** (this doc §3) — primitives→SDL3 + 87-fn containers; migrate engine+modules; validate on existing Win/Linux build | — | 1–2 wk |
| 2 | Root `CMakeLists.txt` for NDK (+ desktop sanity) | 1 | 1–2 wk |
| 3 | GL ES 3.0 dual code path in `src/client/renderer/` | — (parallel) | 1–2 wk |
| 4 | Gradle / Android Studio project `android/app/`, SDL3 Android integration | 2 | 3–5 d |
| 5 | App lifecycle + filesystem paths (`SDL_GetPrefPath`, background/foreground, `AC_BACK`) | 4 | 2–3 d |
| 6 | Virtual controls in cgame (shared w/ #855) | 4 | 2–3 wk |
| 7 | Asset packaging (`default.pk3` in APK or first-run download) | 4 | 2–3 d |

**Recommended order:** 1 → (2 ∥ 3) → 4 → 5 → 7 → 6. Item 1 is validated on the desktop
build before any Android tooling exists, so it can't regress silently.

---

## 6. Status log
- _init_: repo + worktree scaffolded off `main` (`fab1d8a68`); assessment recorded.
  No engine code changed yet.
- _glib §3c (in progress)_: `android/qglib/` started. Fundamental types + callback
  signatures + pluggable allocation (`qglib.h`); **`GArray` implemented** with
  glib-faithful semantics, covered by 20 standalone unit tests (TDD: red→green).
  Builds via MSVC (`run_tests.bat`) and any C11 toolchain (`Makefile`). Still
  standalone — not yet wired into `src/quetoo.h`. Next containers: GPtrArray,
  GQueue, GList/GSList, GHashTable, GString.
- _dev env_: work moved to the **`quetoo-build` LXC** (pve2 ct 100; Debian 12,
  gcc, glib 2.88.0). Workflow: edit on the workstation, build/test in the
  container. A **differential harness** now builds the same suite against qglib
  (`make test`) and system glib-2.0 (`make test-glib`); both pass, so each
  container is verified behaviourally faithful to real glib.
- _glib §3c (containers complete)_: all seven container types implemented and
  dual-backend verified — **GArray, GList, GSList, GPtrArray, GQueue, GString,
  GHashTable**. The remaining five were built in parallel (subagents, one per
  container, isolated files) then integrated; `qglib.h` is now the umbrella
  header. Unified gate: **260 checks, identical on qglib and system glib-2.0**.
  Remaining for the shim: the string/format/mem/fs primitives (mostly map onto
  SDL3 — §3b), then wire `qglib.h` into `src/quetoo.h` behind a build switch and
  validate the full engine build.
- _glib §3 (COMPLETE — unit- and engine-verified)_: primitives (`qglib_util`),
  `GChecksum` (MD5), `GDateTime`/UUID/URI (`qglib_misc`), `GRand` (MT19937,
  glib-exact sequence), and `GRegex` (over POSIX `<regex.h>`) all implemented in
  parallel and integrated. **Unit gate: 11 modules, ~350 checks, identical on
  qglib and system glib-2.0.** Engine-verified via a compile-probe (`probe.sh`):
  a compat `<glib.h>`→`qglib.h` on the include path (no real glib present) lets
  representative TUs compile against the shim — **cmd.c, cvar.c, filesystem.c,
  cm_material.c, cm_entity.c, g_ai_node.c all syntax-check clean**. The probe
  found gaps the static audit missed (GRand, g_strcmp0, g_array_insert_vals,
  g_build_path, transitively-exposed `<float.h>`/`<time.h>`), all now closed.
- _findings_ (corrections to §"Dependencies"): **libcurl is NOT used** — HTTP is
  Objectively's `URLRequest`. **Objectively + ObjectivelyMVC have no NDK build**
  and are a real shared mobile prerequisite alongside glib (for #855/#856).
- _parallel artifacts landed_: `android/CMakeLists.txt` + `cmake/` (NDK build,
  `compat/glib.h` forwarder), `android/app/` (Gradle/SDLActivity scaffold),
  `RENDERER_GLES.md` (GL ES 3.0 audit — renderer is largely ES-ready), and
  `LIFECYCLE.md` (writable storage is free; engine already uses SDL_GetPrefPath).
- _all 7 items now have committed foundations_ (built in parallel by subagents,
  reviewed + integrated by the lead; each validated to not break the desktop build):
  - **#1 glib shim — DONE** (unit + engine verified).
  - **#2 CMake/NDK** — `android/CMakeLists.txt` + `cmake/` (targets, `compat/glib.h`
    + `compat/glib/gstdio.h` forwarders).
  - **#3 GL ES renderer** — foundation in: `r_gl_compat.h` seam + `version_es.glsl`,
    safe pieces implemented behind `#ifdef QUETOO_GLES`; hard breakers (TBO repack,
    base-vertex emulation, glGetTexImage readback, BGR swizzle, glad-GLES loader)
    TODO-stubbed; desktop GL path intact + compiles.
  - **#4 Gradle/Studio** — `android/app/` (SDLActivity, manifest, externalNativeBuild).
  - **#5 lifecycle + fs** — `LIFECYCLE.md` (writable storage free via SDL_GetPrefPath).
  - **#6 virtual controls** — `CONTROLS.md` + `src/client/cl_touch.{h,c}` scaffold
    (designed, compile-checked; not yet wired in).
  - **#7 asset packaging** — `Fs_Init` Android arm + first-run pk3 extract (SDL3
    IOStream) + `ASSETS.md`; desktop untouched.
- _remaining deep work_ (multi-week, needs the NDK toolchain set up): the renderer
  TODO breakers; full virtual-controls wiring (cgame overlay + cl_input hookup);
  the **Objectively/ObjectivelyMVC NDK cross-compile** (the other no-NDK dep);
  and the actual NDK link/build + APK packaging/run.
