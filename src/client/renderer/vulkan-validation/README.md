# Vulkan / RTX validation harnesses

Standalone, dependency-free programs (link only `libvulkan`) that validate the
rendering techniques the Quetoo Vulkan backend is built on, on real hardware. They
mirror the logic in `../r_vk.c` but isolate it from the engine so each technique can
be pixel-verified independently. All three were run on an **NVIDIA GeForce RTX 3090**
(driver 575) and pass.

| Harness | Proves | Build | Result on RTX 3090 |
|---|---|---|---|
| `vk_probe` (see `r_vk.c` `R_Vk_ScoreDevice`) | device selection + RTX detection | `cc vk_probe.c -lvulkan` | selects 3090 (score 11001), `RTX: available` |
| `raster.c` + `tri.vert`/`tri.frag` | SPIR-V → graphics pipeline → draw → readback (phase 2) | `glslangValidator -V tri.vert -o tri.vert.spv` (and `.frag`); `cc raster.c -lvulkan -o raster` | center=triangle, corner=clear, **PASS** |
| `rt.c` + `rt.rgen`/`rt.rmiss`/`rt.rchit` | hardware ray tracing: BLAS+TLAS, RT pipeline, SBT, `vkCmdTraceRaysKHR` (phase 4 core) | `glslangValidator --target-env vulkan1.2 -V rt.rgen -o rt.rgen.spv` (and miss/rchit); `cc rt.c -lvulkan -o rt` | center=ray-traced hit, corner=ray-traced miss/bg, **PASS** |
| `scene.c` + `scene.rgen`/`scene.rmiss`/`scene_shadow.rmiss`/`scene.rchit` | RTX render of real geometry: multi-triangle scene, point light, Lambertian shading, **hardware shadow rays**, per-triangle SSBO, two miss groups | compile the four shaders with `--target-env vulkan1.2`; `cc scene.c -lvulkan -lm -o scene`; `./scene > out.ppm` | renders `rtx-scene-render.png` (floor + pyramid, lit + ray-traced shadow) on the 3090 |

`rtx-scene-render.png` is the actual output of `scene.c` on an RTX 3090 — a ray-traced
image of 3D geometry with a point light and ray-traced shadows. The same raygen /
closest-hit / shadow-ray structure is what phase 4 runs over Quetoo's BSP + mesh
acceleration structures.

These are not built by the autotools project; they are reference + manual regression
checks. The RT harness (`rt.c`) is the template for the engine's phase-4 acceleration
structure + ray-tracing-pipeline integration: swap the single hard-coded triangle for
BSP/mesh geometry and the storage image for the renderer's HDR target.

See `../VULKAN_RTX.md` for the full architecture and roadmap.
