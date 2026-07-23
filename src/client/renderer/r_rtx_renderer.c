/*
 * Copyright(c) 2026 QuetooPlus.
 * SPDX-License-Identifier: GPL-2.0-or-later
 */

#include "r_local.h"
#include "r_rtx_renderer.h"

#if BUILD_RTX

#include "r_rtx_bridge.h"
#include "r_rtx_device.h"
#include "r_rtx_output.h"
#include "r_rtx_probe.h"
#include "r_rtx_scene.h"

/**
 * @file
 * @brief Isolated RTX/Vulkan renderer facade.
 *
 * SDL_gpu remains Quetoo's primary renderer and presentation path. This module owns
 * all native Vulkan ray-tracing state and exposes a narrow hook to the main renderer:
 * register cvars/commands, optionally render a view, and shut down. Keeping that seam
 * tight mirrors the QuetooPlus render-isolation approach without forcing a full
 * backend-vtable migration into this feature slice.
 */

static cvar_t *r_rtx;
static cvar_t *r_rtx_overlay;
static cvar_t *r_rtx_supported;
static cvar_t *r_rtx_smoke;

static r_rtx_device_t r_rtx_device;
static r_rtx_output_t r_rtx_output;
static r_rtx_bridge_t r_rtx_bridge;

static bool R_Rtx_EnsureOutput(VkExtent2D extent) {
  if (!r_rtx_device.device) {
    if (!R_Rtx_DeviceInit(&r_rtx_device)) {
      Com_Warn("Native Vulkan RTX device unavailable; disabling r_rtx.\n");
      Cvar_ForceSetInteger(r_rtx->name, 0);
      Cvar_ForceSetInteger(r_rtx_smoke->name, 0);
      return false;
    }
    Com_Print("Native Vulkan RTX device initialized.\n");
  }

  if (r_rtx_output.extent.width != extent.width || r_rtx_output.extent.height != extent.height) {
    R_Rtx_SceneShutdown(&r_rtx_device);
    R_Rtx_BridgeShutdown(&r_rtx_bridge);
    R_Rtx_OutputShutdown(&r_rtx_device, &r_rtx_output);

    if (!R_Rtx_OutputInit(&r_rtx_device, &r_rtx_output, extent) ||
        !R_Rtx_BridgeInit(&r_rtx_bridge, extent)) {
      Com_Warn("Native Vulkan RTX output bridge failed; disabling r_rtx.\n");
      Cvar_ForceSetInteger(r_rtx->name, 0);
      Cvar_ForceSetInteger(r_rtx_smoke->name, 0);
      return false;
    }
  }

  return true;
}

bool R_Rtx_RenderView(const r_view_t *view) {
  if ((!r_rtx->integer && !r_rtx_smoke->integer) || !r_context.device->commands) {
    return false;
  }

  const bool smoke = r_rtx_smoke->integer != 0;
  const bool scene = r_rtx->integer != 0 && !smoke;
  const VkExtent2D extent = (smoke || scene) ? (VkExtent2D) {
    (uint32_t) r_context.device->framebuffer->size.w,
    (uint32_t) r_context.device->framebuffer->size.h
  } : (VkExtent2D) { 256, 64 };

  if (!R_Rtx_EnsureOutput(extent)) {
    return false;
  }

  const float color[] = { .08f, .01f, .12f, 1.f };
  const bool rendered = scene ? R_Rtx_SceneRender(&r_rtx_device, &r_rtx_output, view) :
      R_Rtx_OutputClear(&r_rtx_device, &r_rtx_output, color);
  if (!rendered || !R_Rtx_BridgeUpload(&r_rtx_bridge, &r_rtx_output)) {
    Com_Warn("Native Vulkan RTX frame failed; disabling r_rtx.\n");
    Cvar_ForceSetInteger(r_rtx->name, 0);
    Cvar_ForceSetInteger(r_rtx_smoke->name, 0);
    return false;
  }

  if (smoke || scene) {
    R_Rtx_BridgeDraw(&r_rtx_bridge, 0, 0, r_context.w, r_context.h, color_white);
    if (!r_rtx_overlay->integer) {
      return true;
    }
  }

  if (r_rtx_overlay->integer) {
    const int32_t w = 256;
    const int32_t h = 64;
    const int32_t x = r_context.w > w + 36 ? r_context.w - w - 24 : 12;
    const int32_t y = r_context.h > h + 36 ? r_context.h - h - 24 : 12;
    if (!scene) {
      R_Rtx_BridgeDraw(&r_rtx_bridge, x, y, w, h, Color4f(1.f, 1.f, 1.f, .72f));
    }
    R_Draw2DString(x + 14, y + 14, "RTX/VK", color_white);
    R_Draw2DString(x + 14, y + 34, scene ? "ray-traced scene active" : "native Vulkan active", color_white);
  }

  return smoke || scene;
}

void R_Rtx_InitLocal(void) {
  r_rtx = Cvar_Add("r_rtx", "0", CVAR_ARCHIVE, "Enable the isolated native Vulkan RTX renderer.");
  r_rtx_overlay = Cvar_Add("r_rtx_overlay", "1", CVAR_ARCHIVE, "Draw a small RTX/VK status label when r_rtx is enabled.");
  r_rtx_supported = Cvar_Add("r_rtx_supported", "0", CVAR_NO_SET, "True when native Vulkan RTX support is available.");
  r_rtx_smoke = Cvar_Add("r_rtx_smoke", "0", CVAR_DEVELOPER, "Draw the native Vulkan RTX output bridge full-screen (developer tool).");

  const bool rtx_supported = R_Rtx_IsSupported();
  Cvar_ForceSetInteger(r_rtx_supported->name, rtx_supported);
  Com_Print("Native Vulkan RTX support: ^%d%s^7\n", rtx_supported ? 2 : 1,
            rtx_supported ? "available" : "unavailable");

  if (!rtx_supported) {
    Cvar_ForceSetInteger(r_rtx->name, 0);
    Cvar_ForceSetInteger(r_rtx_smoke->name, 0);
  }
}

void R_Rtx_InitCommands(void) {
  Cmd_Add("r_rtx_probe", R_Rtx_Probe_f, CMD_RENDERER, "Report whether a native Vulkan RTX device is available.");
}

void R_Rtx_RendererShutdown(void) {
  R_Rtx_SceneShutdown(&r_rtx_device);
  R_Rtx_BridgeShutdown(&r_rtx_bridge);
  R_Rtx_OutputShutdown(&r_rtx_device, &r_rtx_output);
  R_Rtx_DeviceShutdown(&r_rtx_device);
}

#endif
