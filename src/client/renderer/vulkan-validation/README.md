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

These are not built by the autotools project; they are reference + manual regression
checks. The RT harness (`rt.c`) is the template for the engine's phase-4 acceleration
structure + ray-tracing-pipeline integration: swap the single hard-coded triangle for
BSP/mesh geometry and the storage image for the renderer's HDR target.

See `../VULKAN_RTX.md` for the full architecture and roadmap.
