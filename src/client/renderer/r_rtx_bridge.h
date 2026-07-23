#pragma once

#include "r_rtx_output.h"

#if BUILD_RTX
typedef struct {
  Texture *texture;
  TransferBuffer *transfer;
  VkExtent2D extent;
} r_rtx_bridge_t;

bool R_Rtx_BridgeInit(r_rtx_bridge_t *bridge, VkExtent2D extent);
bool R_Rtx_BridgeUpload(r_rtx_bridge_t *bridge, const r_rtx_output_t *output);
void R_Rtx_BridgeDraw(const r_rtx_bridge_t *bridge, int32_t x, int32_t y,
                      int32_t w, int32_t h, color_t color);
void R_Rtx_BridgeShutdown(r_rtx_bridge_t *bridge);
#endif
