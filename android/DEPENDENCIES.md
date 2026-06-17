# Quetoo Android — NDK Dependency Status

Cross-compiling the engine for Android (arm64-v8a + x86_64, NDK r27c, clang 18, Bionic).
Status of each native dependency. Validated on the `quetoo-build` LXC with the
NDK at `/opt/android-ndk-r27c`.

> ## ✅ v78 status (2026-06-17): full online stack + GL ES 3.0 MVC — BOOTS TO MENU
>
> The clean re-port onto upstream **v78** (`feature/android-port-v78`) builds and
> links for **both x86_64 and arm64-v8a**, and the x86_64 build boots on the
> emulator through full init to a **rendered GL ES 3.0 main menu** (no crash). The
> earlier "HTTP deferred / MVC stubbed / renderer is the long pole" plan below is
> **superseded** — all of it is done. Key facts for reproducing the dependency layer:
>
> - **Full HTTPS online stack (path B), not stubbed.** v78 moved the HTTP client into
>   Objectively's `RESTClient → URLSession → libcurl`. The engine's endpoints are
>   HTTPS (`giblets.quetoo.org/api/{guid,stats}`, `*.s3.amazonaws.com`), so a TLS
>   backend is required. `android/build_http_stack.sh <abi> <prefix>` cross-builds,
>   per ABI: **mbedTLS 3.6.2** (TLS) → **libcurl 8.11.1** (`-DHTTP_ONLY=ON
>   -DCURL_USE_MBEDTLS=ON`, no other protocols) → **`libObjectively.so` including the
>   URLSession + RESTClient + JSON layer** (the old `build_all_ndk.sh` skipped
>   `URLSession*.c`). The engine links `libObjectively.so`; curl/mbedTLS load
>   transitively (DT_NEEDED). Ship `libcurl.so` + `libmbed{tls,x509,crypto}.so` in
>   the APK. (Runtime TODO: HTTPS needs a CA bundle — ship `cacert.pem` /
>   `CURLOPT_CAINFO`; offline play + http:// map downloads work without it.)
> - **Objectively & ObjectivelyMVC bumped to `origin/main`.** Objectively `0c4f120`
>   adds `RESTClient`/`JSONContext`; **ObjectivelyMVC `d2f70f4` "Replace FFP Renderer
>   with OpenGL ES 3.0 implementation"** is jdolan's official GLES MVC renderer (use
>   it — no custom GLES MVC needed). `Resource` now lives in **Objectively**, not MVC.
> - **Required Objectively patch (`Class.c`): `dlsym(_handle,s)` → `dlsym(RTLD_DEFAULT,s)`.**
>   Bionic's `dlopen(NULL,0)` handle can't resolve the main program's class-archetype
>   symbols (`_RESTClient`, …); `RTLD_DEFAULT` searches the global scope. Without it
>   the OO runtime can't find classes by name.
> - **Headers must match in BOTH include roots.** The engine compiles with
>   `-I/usr/local/include` (pkg-config `.pc`) **and** `-I<prefix>/include` (via
>   `CMAKE_PREFIX_PATH`). If either holds *stale* Objectively/MVC headers, a subclass
>   (e.g. `QuetooRenderer`) is sized against an old, smaller `Renderer` and
>   `Class::_initialize` aborts: `assertion "superclass->def.instanceSize <=
>   def->instanceSize"`. Refresh new headers into `/usr/local/include` **and**
>   `/root/android-<abi>/include` after bumping the libs.
> - **UI resources must be in `default.pk3`.** The cgame loads view layouts via
>   `ui/**/*.json` + `*.css` (MVC `awakeWithResourceName`, resolved by `Ui_Data` →
>   `Fs_Load`). v78 added `ui/home/{Stats,Leaderboard}ViewController.{json,css}`; a
>   missing file makes `View::awakeWithResource` assert `"resource"`. Refresh
>   `src/cgame/default/ui/**/*.{json,css}` into the pk3 (same as the shaders).
> - **Build order per ABI:** `build_all_ndk.sh <abi>` (base deps) → `build_http_stack.sh
>   <abi>` (mbedTLS+curl+Objectively-HTTP) → rebuild `libObjectivelyMVC.so` (GLES) →
>   refresh prefix headers → `cmake -S android -B qa-v78-<abi> -DANDROID_ABI=<abi>
>   -DCMAKE_PREFIX_PATH=/root/android-<abi> -DQUETOO_GLES=ON` → `cmake --build`.

| Dependency | NDK status | Notes |
|---|---|---|
| **qglib** (our shim) | ✅ **DONE — Bionic-clean** | All 13 sources cross-compile to `libqglib.a` (see `qglib/ndk_check.sh`). Replaces glib2. |
| **SDL3** | ✅ **built** | `libSDL3.so` (11 MB, arm64) cross-built with the NDK; installed to the staging prefix `/root/android-arm64`. |
| **SDL3_image** | ✅ **built** | `libSDL3_image.so` (arm64), staged. Needs `CMAKE_FIND_ROOT_PATH=$PREFIX` + `FIND_ROOT_PATH_MODE_PACKAGE=BOTH` so the NDK toolchain's find_package sees the prefix (the key cross-build fix). |
| **PhysicsFS** | ✅ **built** | `libphysfs.so`/`.a` (arm64), staged. |
| **OpenAL-soft** | ✅ **built** | `libopenal.so` (arm64), staged (`deps2_ndk_build.sh`). |
| **libsndfile** | ✅ **built** | `libsndfile.so` (arm64), staged (external codecs off — WAV/AIFF built-in). |
| **SDL3_ttf** | ✅ **built** | `libSDL3_ttf.so` (arm64, vendored FreeType), staged. MVC font dep. |
| **ObjectivelyMVC** | ✅ **built — 50/50** | `libObjectivelyMVC.a` (arm64, `objmvc_ndk_build.sh`): all 50 sources cross-compile clean under the NDK with the compat shims + GLES3 headers — confirming the GL was NOT a blocker (per upstream **ObjectivelyMVC#38**, MVC runs on mobile; touch works via SDL3 mouse-emulation, gaps are event/layout). Keep the cgame `ui/*` menus; `cl_touch` (#6) is the complementary in-game overlay. The #38 enhancements (gestures, virtual keyboard, 44pt targets, orientation) layer on later. |
| **Objectively** | ✅ **first-cut built** | `libObjectively.a` (arm64-v8a): 42 sources + `qsort_r`/`iconv` compat shims (`cmake/compat/`, built by `obj_ndk_build.sh`); URLSession/HTTP (6 files) deferred. |
| ~~libcurl~~ | n/a (indirect) | Quetoo doesn't use curl directly; it's pulled in **via Objectively's URLSession**. |
| ~~curses~~ | n/a | Dedicated-server console only — **Android is client-only**, not built. |

**Android target = the CLIENT only** (`libmain.so` + `libgame.so`/`libcgame.so`).
No dedicated server on a phone, so server-only code (`sv_console`/curses) and the
dedicated build are out of scope. Cross-built deps are staged into a shared
prefix `/root/android-arm64` (the dependency root for the final client link).

## Objectively NDK port — precise blockers (`android/objectively_ndk_probe.sh`)

40 of 48 `Sources/Objectively/*.c` compile clean with the NDK. The 8 that block:

1. **`qsort_r`** — `MutableArray.c`, `Vector.c`. Bionic has no `qsort_r`. Fix: a ~10-line
   shim (thread-local comparator trampoline over `qsort`, or a small merge/quns
   sort). Trivial; unblocks 2 files.
2. **`iconv`** — `String.c` (`iconv_open`/`iconv`). Bionic has no iconv. Objectively's
   `StringEncoding` set is ASCII, ISO-8859-1/2, MacRoman, UTF-16/32/8, WCHAR_T —
   but **Quetoo's in-game text only ever uses UTF-8 ↔ WCHAR_T** (WCHAR_T = UTF-32
   on Android). Fix: a minimal iconv shim implementing UTF-8 ↔ UTF-32/16 + ASCII +
   Latin-1, and asserting/erroring on the exotic ones (Latin-2/MacRoman) that
   Quetoo never requests. ~150–250 LOC; unblocks 1 file. (Or vendor a libiconv NDK
   build for full fidelity.)
3. **`libcurl`** — `URLSession*.c` (5 files). The HTTP layer. Fix options: (a) build
   libcurl for the NDK (well-trodden), or (b) **stub/disable URLSession for an
   initial offline bring-up** (HTTP is only needed for server browser / downloads /
   stats — not for launching + rendering + local play). Recommended first cut: (b),
   then revisit with a real HTTP backend (curl-Android or an SDL/Java bridge).

**Recommended first-cut path to a buildable Objectively:** add the `qsort_r` shim +
a minimal `iconv` shim, and compile-out the `URLSession*` translation units for
Android (guard with `#ifndef __ANDROID__` in the build's source list). That yields
an `libObjectively.a` for arm64 with HTTP deferred — enough to attempt the first
engine link for an offline build.

## Proven Android artifacts so far (arm64-v8a, NDK r27c)
- **`libqglib.a`** — the glib shim, NDK/Bionic-clean.
- **`libObjectively.a`** — Objectively (HTTP deferred), via compat shims.
- **`libgame.so`** — ✅ the **Quetoo game module**, a real ARM aarch64 ELF shared
  object (~416 KB) exporting `G_LoadGame`. 31 sources (game/default + shared)
  cross-compiled against qglib and linked. Built by `android/game_ndk_build.sh`.
  This proves: (a) the qglib shim links real engine code on Android, and (b)
  #856's preserved `game.so`/`cgame.so` `dlopen` module model works on the NDK.
  (The build-probe surfaced two more glib gaps now closed: `MIN`/`MAX`/`CLAMP`/`ABS`
  convenience macros and `g_list_nth`.) Note: links against the desktop SDL3
  *headers* (shared/swap.c uses SDL_endian macros, header-only); a real device
  build links SDL3-for-NDK.

## Path to a first APK
1. ✅ qglib → `libqglib.a`.
2. ✅ Objectively first-cut (qsort_r + iconv shims, URLSession deferred) → `libObjectively.a`.
3. ✅ **All third-party client libs cross-built + staged** in `/root/android-arm64`:
   SDL3, SDL3_image, OpenAL-soft, PhysicsFS, libsndfile. ObjectivelyMVC → stubbed
   (see table) — not built.
4. ⬅️ **GATING REAL-CODE WORK — the renderer GLES breakers** (`RENDERER_GLES.md`):
   the `glad`-GLES 3.0 loader (foundational — nothing GLES links without it), the
   TBO voxel-light repack, `glDrawElementsBaseVertex` emulation, and `glGetTexImage`
   FBO-readback. These are TODO-stubbed today, so the `QUETOO_GLES` renderer does
   **not** compile/link yet. **There is no shortcut to a GUI-client APK around this**
   — it is multi-day renderer engineering, best done as a focused block.
5. Stub MVC + cgame `ui/*` for `__ANDROID__`; cross-link `libcgame.so` + `libmain.so`
   via `android/CMakeLists.txt` (`-DANDROID_ABI=arm64-v8a -DCMAKE_TOOLCHAIN_FILE=...
   -DCMAKE_FIND_ROOT_PATH=/root/android-arm64 -DCMAKE_FIND_ROOT_PATH_MODE_PACKAGE=BOTH
   -DQUETOO_GLES=ON`).
6. Gradle assembles the APK from `android/app/` + the `.so`s + bundled `default.pk3`;
   test on emulator/device.

**Critical-path summary:** the dependency layer is done; the remaining work is
(4) the renderer GLES breakers [multi-day, real code], then (5) the link + the
MVC/ui stub-out, then (6) APK assembly + device test. Step 4 is the true long pole.
