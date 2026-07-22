/*
 * Copyright(c) 2026 QuetooPlus.
 * SPDX-License-Identifier: GPL-2.0-or-later
 */

#pragma once

#include "r_rtx_device.h"

#if BUILD_RTX

typedef struct {
  VkImage image;
  VkDeviceMemory image_memory;
  VkBuffer readback;
  VkDeviceMemory readback_memory;
  void *pixels;
  VkExtent2D extent;
  VkImageLayout layout;
} r_rtx_output_t;

bool R_Rtx_OutputInit(const r_rtx_device_t *device, r_rtx_output_t *output, VkExtent2D extent);
bool R_Rtx_OutputClear(const r_rtx_device_t *device, r_rtx_output_t *output, const float color[4]);
void R_Rtx_OutputShutdown(const r_rtx_device_t *device, r_rtx_output_t *output);

#endif
