# Renderer Port to OpenGL ES 3.0 (`#define QUETOO_GLES`)

Renderer-specific companion to [`PORTING.md`](./PORTING.md), tracking
[jdolan/quetoo#856](https://github.com/jdolan/quetoo/issues/856) (Android) and the
shared GL ES 3.0 work with [#855](https://github.com/jdolan/quetoo/issues/855) (iOS).

This document enumerates every place in `src/client/renderer/` where the desktop
**OpenGL 4.1 Core** renderer uses functionality that is **absent or different in
OpenGL ES 3.0**, and prescribes the dual-code-path strategy behind `#define QUETOO_GLES`.

> **STRICT:** This is analysis only. No engine file has been modified. File:line
> references are against the current worktree (`Eclipse1982/quetoo` `main`).

---

## 0. Current baseline (what we're porting from)

| Fact | Evidence |
|---|---|
| Target API is **GL 4.1 Core, forward-compatible** | `r_context.c:138-142` (`SDL_GL_CONTEXT_PROFILE_CORE`, major 4 minor 1, `FORWARD_COMPATIBLE`) |
| Loader is **glad 2.0.8**, generated for `gl:core=4.1` + 2 extensions | `r_gl.h:1-26` header banner; `gladLoaderLoadGL()` at `r_context.c:197` |
| Loaded extensions: `GL_ARB_texture_filter_anisotropic`, `GL_ARB_texture_storage` | `r_gl.h:22` |
| GLSL is **`#version 410`**, prepended to every shader | `shaders/version.glsl:22` |
| Every program is `version.glsl` + `uniforms.glsl` + `common.glsl` + stage files | `r_program.c:35-37` (`R_ShaderDescriptor`) |
| 29 GLSL files; shaders are **loaded from disk** at runtime via `Fs_Load("shaders/%s")` | `r_program.c:70`; `shaders/*.glsl` |
| Error handling is a manual `glGetError` wrapper (`R_GetError`), **not** `glDebugMessageCallback` | ~40 call sites; no debug-callback usage found |

**Good news up front:** the GLSL is already modern (GLSL 3.30+ style) and the C side
already uses VAOs, UBOs, `glTexStorage*`, immutable textures, `glBlitFramebuffer`,
`glDrawBuffers` (plural), and `GL_ANY_SAMPLES_PASSED` occlusion — all **core in
GL ES 3.0**. The port is real but bounded: a handful of genuinely-missing entry points,
one missing buffer *type*, the GLSL `#version`/`precision` preamble, and the context/loader
bootstrap.

---

## 1. Severity summary (the short list)

| # | Feature | Site(s) | GLES 3.0? | Disposition |
|---|---|---|---|---|
| A | `glPolygonMode(GL_LINE)` wireframe | `r_main.c:266,276` | **No** | **Drop** (accept loss, per #856) |
| B | GPU **timer** queries (`GL_TIME_ELAPSED`/`glQueryCounter`/`…ui64v`) | loaded `r_gl.c`; **0 call sites** | **No** | **Drop** (already unused) |
| C | `glDrawElementsBaseVertex` / `…InstancedBaseVertex` | `r_mesh_draw.c`, `r_shadow.c`, `r_bsp_draw.c`, `r_occlude.c:192` | **No** (ES 3.2) | **Emulate** (offset indices / rebind buffer) |
| D | **Texture buffer objects** (`GL_TEXTURE_BUFFER`, `glTexBuffer`, `isamplerBuffer`) | `r_bsp_model.c:464-473`, `r_image.c:179-182`, `shaders/uniforms.glsl:238`, `shaders/voxel.glsl:62` | **No** (ES 3.2) | **Emulate** (repack as integer 2D/3D texture / UBO / SSBO) |
| E | `glGetTexImage` + `glGetTexLevelParameteriv` | `r_image.c:464,442-444,472-473`, `r_framebuffer.c:259` | **No** (3.1+) | **Emulate** via FBO + `glReadPixels`; track sizes CPU-side |
| F | `glReadPixels`/`glGetTexImage` with **`GL_BGR`** | `r_image.c:140`, `r_framebuffer.c:259` | **No** | **Emulate** (read RGBA, swizzle on CPU) |
| G | `GL_DEPTH_CLAMP` | `r_shadow.c:379,403` | **No** | **Drop** → accept "Peter Panning" (per #856) |
| H | `glDrawBuffer` (**singular**, `GL_NONE`) | `r_shadow.c:560` | **No** | **Trivial swap** → `glDrawBuffers(0, …)` |
| I | `GL_TEXTURE_MAX_ANISOTROPY` | `r_image.c:193` | **Ext only** | **Guard** behind ES ext probe |
| J | GLSL `#version 410`, no `precision` qualifiers | `shaders/version.glsl:22`; all `*_fs.glsl` | **No** | **Rewrite** preamble (`#version 300 es` + precision) |
| K | Context/loader bootstrap (Core 4.1 vs ES 3.0; glad GL vs GLES) | `r_context.c:138-148,197,228` | n/a | **Branch** under `QUETOO_GLES` |

Items deliberately **verified as already-fine** (no work): VAOs, UBOs (`std140`),
`glTexStorage2D/3D`, `glFramebufferTextureLayer`, `sampler2DArray`/`sampler2DArrayShadow`/
`samplerCube`/`sampler3D`/`isampler3D`, `glBlitFramebuffer`, `glCopyTexSubImage2D`,
`glDrawBuffers` (plural), `glGenerateMipmap`, occlusion via `GL_ANY_SAMPLES_PASSED`,
`gl_FragDepth`, `texture()`/`textureLod()`/`texelFetch()` (3D), `in`/`out`/`layout(location=)`.
No buffer mapping, fences, `glBufferStorage`, geometry/compute/tessellation shaders,
`glClipControl`, seamless-cubemap toggle, `GL_TEXTURE_LOD_BIAS`, sRGB-framebuffer toggle,
or multisample *textures* are used in the renderer (some are loaded by glad but never called).

---

## 2. Findings in detail

### A. Wireframe debug — `glPolygonMode(GL_FRONT_AND_BACK, GL_LINE)` — **DROP**

- **Where:** `r_main.c:266` (set `GL_LINE` when `r_draw_wireframe->integer`), `r_main.c:276` (restore `GL_FILL`).
- **Why it breaks:** ES has no `glPolygonMode`; `GL_LINE`/`GL_FILL` polygon modes do not exist. Loaded at `r_gl.c:286,564` but the entry point is null on ES.
- **Mitigation — DROP (accept-sacrifice).** #856 explicitly accepts losing the wireframe debug view. Under `QUETOO_GLES`, compile out both calls (or make `R_SetPolygonMode()` a no-op). Note the shaders *also* carry a `wireframe` uniform flag (`uniforms.glsl:152`) used for line-overlay shading; that path is fine and can stay, but true `GL_LINE` rasterization is gone. A future real wireframe on ES needs a barycentric-coordinate geometry trick — out of scope.

### B. GPU timer queries — **DROP (already dead code)**

- **Where:** `glQueryCounter`, `glGetQueryObjecti64v`, `glGetQueryObjectui64v`, `GL_TIME_ELAPSED`, `GL_TIMESTAMP` are declared/loaded in `r_gl.c:210-213,342,887-894` and `r_gl.h:1002-1003`.
- **Key finding:** **there are zero call sites** for any of these in the renderer `.c` files. The only queries actually issued are **occlusion** queries using `GL_ANY_SAMPLES_PASSED` (`r_occlude.c:191-193`), which **is** core in ES 3.0.
- **Why the issue lists it:** ES 3.0 has no `GL_TIME_ELAPSED`/`glQueryCounter` (timer queries arrive via `EXT_disjoint_timer_query`), so the *capability* is dropped — but nothing depends on it. **Mitigation:** simply omit these from the GLES loader; no functional change. The `i64`/`ui64` getters can stay unused.

### C. `glDrawElementsBaseVertex` (and instanced variant) — **EMULATE**

- **Where (hot path):** `r_mesh_draw.c:132,312`; `r_shadow.c:203,218`; `r_bsp_draw.c` (via mesh/face draw); `r_occlude.c:192`. Declared/loaded `r_gl.c:136-139,862-863`.
- **Why it breaks:** `glDrawElementsBaseVertex` is **GL ES 3.2**, not 3.0. The renderer relies on a per-call `base_vertex` to index into shared vertex buffers (e.g. `face->base_vertex`, `mesh->base_vertex`, `query->base_vertex`).
- **Mitigation — EMULATE.** Three viable approaches, in order of preference:
  1. **Bake the base vertex into the index buffer** at load time so a plain `glDrawElements` suffices (costs one index rewrite per mesh; no per-frame cost). Cleanest for static BSP/mesh data.
  2. **Adjust the vertex-attrib pointer base** by re-pointing the VAO's bound `GL_ARRAY_BUFFER` offset (`glVertexAttribPointer` with `base_vertex * stride`) before each draw, then `glDrawElements`. More per-call overhead.
  3. Where `base_vertex == 0` (some paths), call `glDrawElements` directly.
  Wrap as `R_DrawElementsBaseVertex(mode, count, type, indices, base)` that compiles to the native call on desktop and an emulation on `QUETOO_GLES`. The occlusion box draw (`r_occlude.c:192`, 36 indices, `indices==NULL`) is trivial to convert via approach 2.
  - *Optional:* probe `GL_OES_draw_elements_base_vertex` / `EXT_…` at runtime and use the native path when present (common on arm64 GPUs) — but do not depend on it.

### D. Texture Buffer Objects (voxel light indices) — **EMULATE** (highest-effort item)

- **Where:**
  - C: `r_bsp_model.c:463-473` creates a `GL_TEXTURE_BUFFER`, `glBufferData`s the per-voxel light-index array, and binds it through an `r_image_t` with `target = GL_TEXTURE_BUFFER`; `r_image.c:179-182` calls `glTexBuffer(GL_TEXTURE_BUFFER, GL_R32I, buffer)`.
  - GLSL: `shaders/uniforms.glsl:238` declares `uniform isamplerBuffer texture_voxel_light_indices;`, fetched at `shaders/voxel.glsl:62` via `texelFetch(texture_voxel_light_indices, index).x`.
- **Why it breaks:** `GL_TEXTURE_BUFFER`, `glTexBuffer`, and the GLSL `samplerBuffer`/`isamplerBuffer` types are **GL ES 3.2** (or `EXT_texture_buffer`), absent from 3.0. This is part of the per-voxel clustered-lighting index list (companion to the `RG32I` 3D `texture_voxel_light_data`, which *is* fine — `r_image.c:454-456`, `voxel.glsl:53`).
- **Mitigation — EMULATE.** The TBO is a 1-D array of `int32` indexed linearly. Options:
  1. **Repack into an integer 2D texture** (`GL_R32I`, e.g. width = N, height = ceil(count/N)); replace `texelFetch(buf, i)` with `texelFetch(tex, ivec2(i % N, i / N), 0)`. Pure ES-3.0, no extensions. **Recommended.**
  2. **UBO/SSBO:** SSBOs are ES 3.1 (not 3.0); a UBO is size-limited (`MAX_UNIFORM_BLOCK_SIZE` ≥ 16 KiB only) and likely too small for the full index list. Reject for 3.0.
  3. Probe `EXT_texture_buffer` and keep the TBO when present (many arm64 drivers expose it), else fall back to (1).
- **Touches:** `r_image_t` plumbing (a new `IMG_*`/target for the repacked texture), `r_bsp_model.c` upload, and the two GLSL functions in `voxel.glsl`. This is the one finding that ripples into both C and GLSL and warrants a dedicated sub-task.

### E. `glGetTexImage` / `glGetTexLevelParameteriv` — **EMULATE**

- **Where:**
  - `r_image.c:464` `glGetTexImage(target, level, format, GL_UNSIGNED_BYTE, pixels)` — the texture/material **dump** tool, looping mip levels.
  - `r_image.c:442-444,472-473` `glGetTexLevelParameteriv(... GL_TEXTURE_WIDTH/HEIGHT/DEPTH)` — queries texture dimensions per level.
  - `r_framebuffer.c:259` `glGetTexImage(GL_TEXTURE_2D, 0, GL_BGR, …)` — reads a framebuffer **attachment texture** back to an `SDL_Surface` (screenshots).
- **Why it breaks:** ES has **no** `glGetTexImage`; `glGetTexLevelParameteriv` is ES **3.1**. ES can only read pixels from a *framebuffer* via `glReadPixels`.
- **Mitigation — EMULATE.**
  - Replace texture readback with: attach the texture (or each cubemap face / array layer / mip) to a temporary FBO via `glFramebufferTexture2D`/`glFramebufferTextureLayer`, then `glReadPixels`. The renderer already does FBO-attachment reads in `r_framebuffer.c:235-261`, so the pattern exists.
  - Replace `glGetTexLevelParameteriv` dimension queries with the **CPU-side** values already stored on `r_image_t` (`width`, `height`, `depth`, `levels` — see `r_image.c:217-221`), computing per-mip sizes by halving. No GL query needed.
  - These are **developer/screenshot tools**, not hot paths; correctness over speed.

### F. `GL_BGR` pixel transfer — **EMULATE**

- **Where:** `r_image.c:140` `glReadPixels(..., GL_BGR, ...)`; `r_framebuffer.c:259` `glGetTexImage(..., GL_BGR, ...)`.
- **Why it breaks:** ES 3.0 `glReadPixels` accepts only `GL_RGBA`/`GL_RGBA_INTEGER` plus one implementation-defined format/type pair (`GL_IMPLEMENTATION_COLOR_READ_FORMAT/TYPE`). `GL_BGR`/`GL_BGRA` are not guaranteed.
- **Mitigation — EMULATE.** Read as `GL_RGBA`/`GL_UNSIGNED_BYTE` and byte-swap to BGR on the CPU when writing the `SDL_Surface` (or create the surface as `SDL_PIXELFORMAT_ABGR8888`/`RGBA32` and let SDL handle the channel order). Screenshot path only; trivial.

### G. `GL_DEPTH_CLAMP` — **DROP → accept "Peter Panning"**

- **Where:** `r_shadow.c:379` (`glEnable`) / `:403` (`glDisable`), wrapping shadow-atlas rendering.
- **Why it breaks:** ES 3.0 has no `GL_DEPTH_CLAMP` (no `NV/EXT_depth_clamp` guaranteed). Depth clamp keeps geometry in front of the near plane from being clipped, which the shadow pass uses to avoid light-frustum near-plane clipping artifacts.
- **Mitigation — DROP, accept the sacrifice.** Per #856, "Peter Panning" (shadows detaching slightly from occluders) is an accepted trade-off on mobile. Compile out the enable/disable under `QUETOO_GLES`. Optional partial remedy: clamp depth in the shadow **vertex** shader (`shadow_vs.glsl`) by saturating `gl_Position.z`, or pull the shadow near-plane in — but the accepted baseline is simply to drop it.

### H. `glDrawBuffer(GL_NONE)` (singular) — **TRIVIAL SWAP**

- **Where:** `r_shadow.c:560` `glDrawBuffer(GL_NONE)` + `glReadBuffer(GL_NONE)` for the depth-only shadow FBO. Declared/loaded `r_gl.c:133,541`.
- **Why it breaks:** ES has no singular `glDrawBuffer`; only `glDrawBuffers` (plural). `glReadBuffer(GL_NONE)` **is** legal in ES 3.0.
- **Mitigation — SWAP** to `glDrawBuffers(1, (GLenum[]){ GL_NONE })` (a depth-only FBO with no color draw buffer). The codebase already uses `glDrawBuffers` everywhere else (`r_main.c:261`, `r_framebuffer.c:131`, `r_post.c:232`), so this is consistent. `glReadBuffer(GL_BACK)` (`r_framebuffer.c:176,220`) is also fine in ES 3.0.

### I. `GL_TEXTURE_MAX_ANISOTROPY` — **GUARD**

- **Where:** `r_image.c:193` `glTexParameterf(target, GL_TEXTURE_MAX_ANISOTROPY, r_image_state.anisotropy)`. Desktop relies on `GL_ARB_texture_filter_anisotropic` (`r_gl.h:22`).
- **Why it differs:** On ES this is the `EXT_texture_filter_anisotropic` extension (very widely supported on Android, but still an *extension*, not core 3.0). Setting it without the extension raises `GL_INVALID_ENUM`.
- **Mitigation — GUARD.** Probe the extension at init (store a bool in the GL config); only set the parameter (and clamp to `GL_MAX_TEXTURE_MAX_ANISOTROPY`) when present. Default `r_anisotropy` to 1 (off) when absent.

### J. GLSL preamble: `#version` + `precision` — **REWRITE the two preamble files**

- **Where:** `shaders/version.glsl:22` is the **single** `#version 410` line, prepended to *every* program by `R_ShaderDescriptor` (`r_program.c:35`).
- **Why it breaks:** ES needs `#version 300 es`. Critically, **ES fragment shaders require an explicit default `precision`** for `float`/`int`/sampler types — desktop GLSL has none, and the renderer declares none (`grep` for `precision` across `shaders/` = **0 hits**). Without it, every fragment shader fails to compile on ES.
- **What's already OK (verified):** shaders use `in`/`out` (not `attribute`/`varying`), `layout(location=N) in` for vertex attributes (`bsp_vs.glsl:22-27`), bare `out vec4 out_color` for fragment output (defaults to draw buffer 0 — legal in ES 3.0; `bsp_fs.glsl:26` et al.), `texture()`/`textureLod()`/`texelFetch()` (no `texture2D`/`gl_FragColor`/`gl_FragData`), `gl_FragDepth` (`shadow_fs.glsl:26`, legal in ES 3.0), and `std140` UBOs (`uniforms.glsl:68,195`). **No `#extension`, no `gl_ClipDistance`, no `noperspective`, no 1D samplers.**
- **Mitigation — REWRITE preamble under `QUETOO_GLES`:**
  - `version.glsl` → emit `#version 300 es`.
  - Add a precision block (in `version.glsl` or a new `precision.glsl` injected right after it):
    ```glsl
    precision highp float;
    precision highp int;
    precision highp sampler2DArray;
    precision highp sampler3D;
    precision highp isampler3D;
    precision highp sampler2DArrayShadow;
    // + samplerCube, sampler2D defaults
    ```
    `highp` is required for the voxel/lighting math; verify against `GL_FRAGMENT_PRECISION_HIGH`. Use `mediump` selectively only if a target GPU lacks `highp` in fragments (rare on ES 3.0 hardware).
  - The `isamplerBuffer` line (`uniforms.glsl:238`) and `voxel.glsl:62` change per **Finding D** (repacked integer texture), not here.
  - **Delivery mechanism is already perfect:** because `version.glsl` is a real file `Fs_Load`ed at runtime (`r_program.c:70`) and concatenated ahead of all sources, swapping its contents (or selecting an `-es` variant) flips the entire shader corpus with **no per-shader edits**. Two options: (a) ship a `version.glsl` whose body is chosen at build time, or (b) have `R_ShaderDescriptor` prepend `version_es.glsl`/`precision.glsl` when `QUETOO_GLES`. Prefer (b) — keeps both preambles in-tree and diffable.

### K. Context creation + GL loader — **BRANCH**

- **Where:** `r_context.c:138-148` requests Core 4.1 forward-compatible; `r_context.c:197` `gladLoaderLoadGL()`; `r_context.c:228` `gladLoaderUnloadGL()`. The function-pointer table + `#define glFoo glad_glFoo` macros live in `r_gl.h`/`r_gl.c` (glad, `gl:core=4.1`).
- **Why it breaks:** The GLES driver won't return a Core-4.1 context; the desktop glad loader resolves desktop-only entry points (many of which are null on ES). ES needs `SDL_GL_CONTEXT_PROFILE_ES`, major 3 / minor 0, and a **separate glad GLES loader**.
- **Mitigation — BRANCH under `QUETOO_GLES`:**
  - `r_context.c`: request `SDL_GL_CONTEXT_PROFILE_ES`, `MAJOR=3`, `MINOR=0`; drop `FORWARD_COMPATIBLE` (Core-only). Depth size 24 and RGBA8 are fine; alpha 0 OK.
  - **Loader:** generate a second glad header for `gles2:3.0` (glad calls ES "gles2" with version 3.0) and select it via `#ifdef QUETOO_GLES`. Keep the macro-renamed `glFoo` surface identical so the rest of the renderer is unchanged. On Android the loader can also be `SDL_GL_GetProcAddress`/`eglGetProcAddress`-backed.
  - Entry points that don't exist on ES (`glPolygonMode`, `glDrawBuffer`, `glGetTexImage`, `glTexBuffer`, `glDrawElementsBaseVertex`, timer queries) must **not** be referenced on the ES path — guaranteed by the per-finding `#ifdef`s above.

---

## 3. Recommended `#define QUETOO_GLES` strategy

**Goal:** one shared renderer, two GL profiles, minimal `#ifdef` sprawl, no behavioral
regression on desktop.

### 3.1 Where the dual paths live

1. **A new compatibility header — `r_gl_compat.h`** (the primary seam). Include it from
   `r_local.h` after the GL headers. It centralizes the differences so call sites stay clean:
   - Selects the glad loader: `#ifdef QUETOO_GLES` → GLES 3.0 glad header, else the existing `r_gl.h`.
   - Provides **inline shims** with the desktop signature that the renderer already calls:
     - `R_DrawElementsBaseVertex(...)` → native on desktop; emulated on ES (Finding C).
     - `R_SetPolygonMode(mode)` → `glPolygonMode` on desktop; no-op on ES (Finding A).
     - `R_DrawBufferNone()` → `glDrawBuffer(GL_NONE)` vs `glDrawBuffers(1,{GL_NONE})` (Finding H).
     - `R_GetTextureImage(...)` → `glGetTexImage` vs FBO+`glReadPixels` (Findings E/F).
     - `R_SetDepthClamp(bool)` → enable/disable vs no-op (Finding G).
     - `R_SetAnisotropy(...)` → guarded (Finding I).
   - Defines any missing tokens used only on desktop as needed, and a `R_GLES` runtime/compile flag.
   This keeps **per-call `#ifdef`s out of the hot rendering code** — they collapse into ~6 inline helpers in one header.

2. **`r_context.c`** — the only file with a genuine `#ifdef QUETOO_GLES` block in logic
   (context attributes + loader init/shutdown, Finding K). Small and unavoidable.

3. **`r_gl.{c,h}`** — *do not hand-edit.* These are glad-generated. Generate a **parallel**
   GLES header (`r_gl_es.{c,h}` from `glad --api='gles2:3.0' --extensions=GL_EXT_texture_filter_anisotropic,GL_EXT_texture_buffer,GL_OES_draw_elements_base_vertex c --loader`)
   and let `r_gl_compat.h` pick one. Optional ES extensions (`EXT_texture_buffer`,
   base-vertex) are included so the loader can light up native fast paths when a device
   exposes them, with the emulation as the guaranteed floor.

4. **GLSL preamble files** — `version.glsl` stays desktop; add `version_es.glsl` +
   `precision.glsl`; `R_ShaderDescriptor` (`r_program.c:35`) chooses which to prepend under
   `QUETOO_GLES`. **All 29 shaders flip with zero per-file edits** (Finding J). The only
   in-shader change is the voxel light-index fetch (Finding D), gated with a small
   `#ifdef`-style include swap (e.g. `voxel.glsl` includes the TBO or the repacked-texture
   variant).

### 3.2 Approach: `#ifdef`s vs runtime

- Use **compile-time `QUETOO_GLES`** for everything that differs in *API surface* (entry
  points, context, GLSL preamble) — a device is one or the other.
- Use **runtime extension probes** (a `r_gl_config` struct populated from
  `glGetString(GL_EXTENSIONS)` / `GL_NUM_EXTENSIONS`) for the *optional* ES fast paths:
  anisotropy, `EXT_texture_buffer`, base-vertex draw. This gives one ES binary that runs
  on both feature-poor and feature-rich Android GPUs.

### 3.3 GLSL porting approach (summary)

1. Swap preamble to `#version 300 es` + mandatory `precision` (Finding J) — the single
   highest-leverage change, done once in the loader.
2. Repack the voxel light-index TBO to an integer 2D texture and update the two `voxel.glsl`
   fetch helpers (Finding D) — the only real shader logic change.
3. Everything else (samplers, `in`/`out`, `layout`, UBOs, `texelFetch`/`textureLod`,
   `gl_FragDepth`) is already ES-3.0-clean — verified, no changes.
4. Validate each program links on a real ES 3.0 context (the existing `R_LoadProgram`
   error log at `r_program.c:120-123` already dumps source + log on failure — keep it).

### 3.4 Suggested work order (renderer item, ~1–2 wk per `PORTING.md` §5 item 3)

1. **GLSL preamble** swap + precision (Finding J) — unblocks all shader compilation.
2. **Context + dual glad loader** (Finding K) — get an ES context that clears the screen.
3. **`r_gl_compat.h` shims** for the trivial/drop items (A, B, G, H, I, E, F).
4. **`glDrawElementsBaseVertex` emulation** (Finding C) — unblocks all geometry.
5. **Voxel TBO → integer texture** (Finding D) — unblocks BSP voxel lighting; the long pole.
6. Bring-up validation on `arm64-v8a` + emulator (`x86_64`); confirm Peter-Panning and
   missing-wireframe are the only intended visual deltas.

---

## 4. Appendix — full evidence index (file:line)

**Desktop-only entry points actually called (need shim/emulation/drop):**
- `glPolygonMode` — `r_main.c:266,276`
- `glDrawElementsBaseVertex` — `r_mesh_draw.c:132,312`, `r_shadow.c:203,218`, `r_occlude.c:192`
- `glDrawBuffer` (singular) — `r_shadow.c:560`
- `glGetTexImage` — `r_image.c:464`, `r_framebuffer.c:259`
- `glGetTexLevelParameteriv` — `r_image.c:442-444,472-473`
- `glTexBuffer` / `GL_TEXTURE_BUFFER` — `r_image.c:179-182`, `r_bsp_model.c:464-473`
- `GL_DEPTH_CLAMP` — `r_shadow.c:379,403`
- `GL_TEXTURE_MAX_ANISOTROPY` — `r_image.c:193`
- `glReadPixels(GL_BGR)` — `r_image.c:140`

**Loaded by glad but never called (drop from ES loader, no code change):**
- Timer queries: `glQueryCounter` `r_gl.c:342,894`; `glGetQueryObject[u]i64v` `r_gl.c:210-213,887-888`; `GL_TIME_ELAPSED`/`GL_TIMESTAMP` `r_gl.h:1002-1003`.
- `glTexImage2DMultisample` `r_gl.c:372,876`; `glProvokingVertex` `r_gl.c:341,874`; `glMapBufferRange`/`glFenceSync`/`glClientWaitSync` `r_gl.c:271,151,88` (no usage found).

**Verified ES-3.0-safe (no work — do not touch):**
- VAOs: `glGenVertexArrays`/`glBindVertexArray` across `r_bsp_model.c`, `r_draw_2d/3d.c`, `r_mesh_*`, `r_sprite.c`, `r_post.c`, `r_sky.c`, `r_decal.c`, `r_occlude.c`, `r_model.c`.
- `glDrawBuffers` (plural) — `r_main.c:261,284,301,312`, `r_framebuffer.c:131,141`, `r_post.c:232,243`.
- `glTexStorage2D/3D` — `r_image.c:218,220`; `glFramebufferTextureLayer` — `r_shadow.c:396,558`.
- `glBlitFramebuffer` — `r_framebuffer.c:206`; `glCopyTexSubImage2D` — `r_framebuffer.c:171`.
- Occlusion `GL_ANY_SAMPLES_PASSED` + `glGetQueryObjectiv` — `r_occlude.c:183-193`.
- `glGetIntegerv(GL_MAX_ARRAY_TEXTURE_LAYERS)` — `r_shadow.c:478`.

**GLSL (verified):**
- Single `#version` source — `shaders/version.glsl:22`.
- Preamble injection — `r_program.c:35-37`; runtime load — `r_program.c:70`.
- Modern attributes/outputs — `bsp_vs.glsl:22-27`, `*_fs.glsl:26` (`out vec4 out_color`).
- `gl_FragDepth` — `shadow_fs.glsl:26`. `textureLod`/`texelFetch`(3D) — `bsp_fs.glsl:94`, `material.glsl:328-343`, `voxel.glsl:53`.
- **`isamplerBuffer`** (the one non-ES sampler) — `uniforms.glsl:238`, fetch `voxel.glsl:62`.
- UBOs `std140` — `uniforms.glsl:68` (`uniforms_block`), `:195` (`lights_block`).
- **Zero** `precision` qualifiers anywhere under `shaders/` (must be added for ES).
