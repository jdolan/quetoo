# Vulkan / RTX renderer backend — architecture & roadmap

Status: **foundation in progress.** This document is the design contract for adding a
Vulkan rendering backend to Quetoo and, on top of it, ray-traced lighting ("RTX").
It is grounded in the renderer as it exists today; file/line references are accurate
as of the branch this lands on.

## 1. Where we are starting from

Quetoo's renderer is a **forward renderer on OpenGL 4.1 core**:

- Context: `r_context.c` creates an `SDL_WINDOW_OPENGL` window and an
  `SDL_GLContext` (4.1 core, forward-compatible), then loads entry points with
  `gladLoaderLoadGL()`. The loader header is `r_gl.h` (glad 2.0.8, `gl:core=4.1`).
- GL is called **directly** throughout the renderer — there is no backend
  abstraction. `r_context_t` (in `r_types.h`) even stores `SDL_GLContext context`
  and uses `GLint w, h`.
- Shaders are GLSL, compiled at runtime by `r_program.c` from
  `src/client/renderer/shaders/*.glsl` (forward passes: `bsp`, `mesh`, `sky`,
  `decal`, `shadow`, `depth_pass`, `post`, `draw_2d`, `draw_3d`, `sprite`, plus the
  shared includes `common.glsl`, `light.glsl`, `material.glsl`, `uniforms.glsl`).
- Init/teardown are linear: `R_Init()` in `r_main.c` calls `R_InitContext()` then a
  fixed sequence of subsystem inits (`R_InitMedia`, `R_InitImages`,
  `R_InitDepthPass`, `R_InitShadows`, `R_InitDraw2D/3D`, `R_InitModels`,
  `R_InitSprites`, `R_InitLights`, `R_InitDecals`, `R_InitSky`, `R_InitPost`).
- Build: autotools (`configure.ac` + per-dir `Makefile.am`). `4.1` is chosen because
  that is macOS's OpenGL ceiling.

Implication: **OpenGL 4.1 has no ray tracing.** RTX requires Vulkan 1.2+ with
`VK_KHR_acceleration_structure` and either `VK_KHR_ray_tracing_pipeline` or
`VK_KHR_ray_query`. So the work is necessarily two layers: (A) a Vulkan
rasterization backend that reaches parity with the GL forward renderer, then
(B) ray-traced lighting built on acceleration structures derived from BSP + mesh
geometry. This is the Q2RTX approach adapted to Quetoo's BSP and material system.

## 2. Design principles

1. **Coexistence, not replacement.** GL stays the default and shipping backend until
   Vulkan reaches parity. Selection is via a new `r_backend` cvar
   (`gl` | `vulkan`), latched (requires `r_restart`). The Vulkan code is compiled
   only when `--enable-vulkan` is passed to `./configure` (defines `BUILD_VULKAN`);
   default builds are byte-for-byte unchanged.
2. **Mirror existing structure.** New files use the `r_` prefix and the existing
   init/shutdown lifecycle. The Vulkan context lives in `r_vk.c`/`r_vk.h`, mirroring
   how `r_context.c` owns the GL context. No new architectural idioms.
3. **Surface the seam minimally first.** The first increment does not abstract every
   GL call (that is mechanical but enormous). It introduces the backend *selection*
   and a Vulkan *device* that can be created, queried for RTX capability, and torn
   down cleanly — the load-bearing first step every later phase depends on.
4. **SDL3 owns the loader.** We load Vulkan through SDL3
   (`SDL_Vulkan_GetVkGetInstanceProcAddr`, `SDL_Vulkan_GetInstanceExtensions`,
   `SDL_Vulkan_CreateSurface`) rather than adding volk, so we reuse the windowing
   dependency Quetoo already has.

## 3. Target backend architecture

```
r_backend (cvar) ──► R_InitContext()
                       ├─ gl:     SDL_GL_CreateContext + gladLoaderLoadGL   (today)
                       └─ vulkan: R_Vk_Init()  ── r_vk.c
                                    ├─ VkInstance (SDL3 surface extensions, validation in debug)
                                    ├─ VkSurfaceKHR (from SDL_Window)
                                    ├─ physical-device selection
                                    │     prefers discrete GPU exposing
                                    │     VK_KHR_acceleration_structure +
                                    │     VK_KHR_ray_tracing_pipeline  → sets r_vk.rtx
                                    ├─ VkDevice + graphics/present queues
                                    └─ (later) swapchain, command pools, VMA allocator
```

Later phases fill in: swapchain + framebuffers, a SPIR-V program loader paralleling
`r_program.c`, descriptor/pipeline layouts, and a draw submission path that the
existing pass code (`r_bsp_draw`, `r_mesh_draw`, …) targets through a thin backend
vtable.

## 4. Shader pipeline

GLSL stays the authoring language. Vulkan consumes **SPIR-V**, so the build gains a
`glslangValidator`/`glslc` step that compiles `shaders/*.glsl` → `*.spv`. The
existing `#include`-style preamble handling in `r_program.c` (and the `version.glsl`
/ `version_es.glsl` preamble already used for the Android GLES port) maps cleanly
onto `-V` compilation. RTX adds new stages authored as `.rgen` / `.rmiss` / `.rchit`
ray-tracing shaders.

## 5. RTX lighting plan (phase B)

- **BLAS** (bottom-level accel structures) from static BSP world geometry and from
  mesh models (`r_mesh_model*`). World BLAS is built once per map load; mesh BLAS are
  cached per model.
- **TLAS** (top-level) rebuilt per frame from visible entities (`r_entity`) +
  world instance.
- **Materials**: reuse Quetoo's PBR-ish material inputs (`material.glsl`) as the
  closest-hit shading inputs; lights come from `r_light`.
- **Output**: ray-traced direct + one-bounce indirect into an HDR target, then reuse
  the existing `r_post` tonemap/bloom chain for display. A/B against the GL forward
  result for validation.
- Denoising (SVGF or a vendor denoiser) is a later sub-phase; start with high spp /
  static-camera correctness before real-time denoise.

## 6. Phased roadmap

- **Phase 0 — Foundation (this branch).** `r_backend` cvar, `--enable-vulkan`,
  `r_vk.c/h`: instance + surface + RTX-aware device selection + teardown. Prints the
  chosen device and whether ray tracing is available. No frame is drawn yet.
- **Phase 1 — Clear screen.** Swapchain, render pass, command buffers; present a
  cleared frame through the Vulkan path. Proves the window/present plumbing.
- **Phase 2 — 2D parity.** Port `draw_2d` (HUD/console) to SPIR-V pipelines — the
  smallest self-contained pass, enough to render menus on Vulkan.
- **Phase 3 — World + mesh raster parity.** BSP and mesh passes, depth, materials,
  lights, sky, post. Vulkan becomes a usable alternative backend.
- **Phase 4 — RTX.** Acceleration structures, ray-tracing pipeline, RT lighting into
  the HDR target, then denoise. `r_rtx` cvar gates the ray-traced path.

## 7. Build & verification notes

Vulkan compilation needs the Vulkan SDK headers/loader and `glslang`. Because the
default configure path does **not** define `BUILD_VULKAN`, contributors without the
SDK build exactly as before. CI/build-container work (the Debian 12 build box) gains
a `--enable-vulkan` job once Phase 1 can present a frame. Until then, the Vulkan
translation unit is compile-gated and verified in isolation.
