/*
 * Copyright(c) 2026 QuetooPlus.
 *
 * This program is free software; you can redistribute it and/or
 * modify it under the terms of the GNU General Public License
 * as published by the Free Software Foundation; either version 2
 * of the License, or (at your option) any later version.
 */

#pragma once

#include <stdbool.h>

#if BUILD_RTX
#include <vulkan/vulkan.h>

typedef struct {
  VkInstance instance;
  VkPhysicalDevice physical_device;
  VkDevice device;
  VkQueue queue;
  VkCommandPool command_pool;
  uint32_t queue_family;
} r_rtx_device_t;

/**
 * @brief Creates a headless Vulkan device with the features required by RTX.
 * @details The device deliberately has no surface or swapchain. Its result is
 * bridged into the SDL_gpu renderer, which remains responsible for Quetoo's
 * window, UI and presentation.
 */
bool R_Rtx_DeviceInit(r_rtx_device_t *device);

/**
 * @brief Submits an empty command buffer to verify the native RTX queue.
 */
bool R_Rtx_DeviceSmokeTest(const r_rtx_device_t *device);

/**
 * @brief Finds a memory type matching the Vulkan allocation requirements.
 */
bool R_Rtx_FindMemoryType(const r_rtx_device_t *device, uint32_t type_bits,
                          VkMemoryPropertyFlags properties, uint32_t *memory_type);

/**
 * @brief Waits for and releases a device created by R_Rtx_DeviceInit.
 */
void R_Rtx_DeviceShutdown(r_rtx_device_t *device);

#endif
