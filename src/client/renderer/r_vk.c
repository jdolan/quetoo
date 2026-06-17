/*
 * Copyright(c) 1997-2001 id Software, Inc.
 * Copyright(c) 2002 The Quakeforge Project.
 * Copyright(c) 2006 Quetoo.
 *
 * This program is free software; you can redistribute it and/or
 * modify it under the terms of the GNU General Public License
 * as published by the Free Software Foundation; either version 2
 * of the License, or (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.
 *
 * See the GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, write to the Free Software
 * Foundation, Inc., 59 Temple Place - Suite 330, Boston, MA  02111-1307, USA.
 */

#include "r_local.h"

#if BUILD_VULKAN

#include <SDL3/SDL_vulkan.h>

r_vk_t r_vk;

#define R_VK_INVALID_QUEUE_FAMILY ((uint32_t) -1)

/**
 * @brief The device extensions required for hardware ray tracing ("RTX"). A device
 * exposing all of these can build acceleration structures and run ray tracing
 * pipelines; see VULKAN_RTX.md phase B.
 */
static const char *r_vk_rtx_extensions[] = {
  VK_KHR_ACCELERATION_STRUCTURE_EXTENSION_NAME,
  VK_KHR_RAY_TRACING_PIPELINE_EXTENSION_NAME,
  VK_KHR_DEFERRED_HOST_OPERATIONS_EXTENSION_NAME,
  VK_KHR_BUFFER_DEVICE_ADDRESS_EXTENSION_NAME
};

/**
 * @brief Creates the Vulkan instance, enabling the surface extensions SDL3 requires
 * for our window. Validation layers are enabled opportunistically and never fatally.
 */
static void R_Vk_CreateInstance(void) {

  uint32_t sdl_count = 0;
  const char * const *sdl_extensions = SDL_Vulkan_GetInstanceExtensions(&sdl_count);
  if (!sdl_extensions) {
    Com_Error(ERROR_FATAL, "SDL_Vulkan_GetInstanceExtensions: %s\n", SDL_GetError());
  }

  const VkApplicationInfo application_info = {
    .sType = VK_STRUCTURE_TYPE_APPLICATION_INFO,
    .pApplicationName = "Quetoo",
    .applicationVersion = VK_MAKE_VERSION(1, 0, 0),
    .pEngineName = "Quetoo",
    .engineVersion = VK_MAKE_VERSION(1, 0, 0),
    .apiVersion = VK_API_VERSION_1_2
  };

  VkInstanceCreateInfo create_info = {
    .sType = VK_STRUCTURE_TYPE_INSTANCE_CREATE_INFO,
    .pApplicationInfo = &application_info,
    .enabledExtensionCount = sdl_count,
    .ppEnabledExtensionNames = sdl_extensions
  };

  // enable the standard validation layer if the loader has it
  const char *validation_layer = "VK_LAYER_KHRONOS_validation";

  uint32_t layer_count = 0;
  vkEnumerateInstanceLayerProperties(&layer_count, NULL);

  VkLayerProperties *layers = Mem_Malloc(sizeof(VkLayerProperties) * layer_count);
  vkEnumerateInstanceLayerProperties(&layer_count, layers);

  for (uint32_t i = 0; i < layer_count; i++) {
    if (!g_strcmp0(layers[i].layerName, validation_layer)) {
      create_info.enabledLayerCount = 1;
      create_info.ppEnabledLayerNames = &validation_layer;
      break;
    }
  }

  Mem_Free(layers);

  const VkResult result = vkCreateInstance(&create_info, NULL, &r_vk.instance);
  if (result != VK_SUCCESS) {
    Com_Error(ERROR_FATAL, "vkCreateInstance failed (%d)\n", result);
  }
}

/**
 * @brief Returns true if the given device exposes all of the named extensions.
 */
static _Bool R_Vk_DeviceHasExtensions(VkPhysicalDevice device, const char **names, uint32_t count) {

  uint32_t available_count = 0;
  vkEnumerateDeviceExtensionProperties(device, NULL, &available_count, NULL);

  VkExtensionProperties *available = Mem_Malloc(sizeof(VkExtensionProperties) * available_count);
  vkEnumerateDeviceExtensionProperties(device, NULL, &available_count, available);

  _Bool has_all = true;
  for (uint32_t i = 0; i < count; i++) {
    _Bool found = false;
    for (uint32_t j = 0; j < available_count; j++) {
      if (!g_strcmp0(available[j].extensionName, names[i])) {
        found = true;
        break;
      }
    }
    if (!found) {
      has_all = false;
      break;
    }
  }

  Mem_Free(available);
  return has_all;
}

/**
 * @brief Resolves the graphics and presentation queue families for the given device,
 * writing `R_VK_INVALID_QUEUE_FAMILY` for any that is unavailable.
 */
static void R_Vk_FindQueueFamilies(VkPhysicalDevice device, uint32_t *graphics, uint32_t *present) {

  *graphics = R_VK_INVALID_QUEUE_FAMILY;
  *present = R_VK_INVALID_QUEUE_FAMILY;

  uint32_t count = 0;
  vkGetPhysicalDeviceQueueFamilyProperties(device, &count, NULL);

  VkQueueFamilyProperties *families = Mem_Malloc(sizeof(VkQueueFamilyProperties) * count);
  vkGetPhysicalDeviceQueueFamilyProperties(device, &count, families);

  for (uint32_t i = 0; i < count; i++) {

    if (families[i].queueFlags & VK_QUEUE_GRAPHICS_BIT) {
      if (*graphics == R_VK_INVALID_QUEUE_FAMILY) {
        *graphics = i;
      }
    }

    VkBool32 present_support = VK_FALSE;
    vkGetPhysicalDeviceSurfaceSupportKHR(device, i, r_vk.surface, &present_support);
    if (present_support) {
      if (*present == R_VK_INVALID_QUEUE_FAMILY) {
        *present = i;
      }
    }
  }

  Mem_Free(families);
}

/**
 * @brief Scores a candidate device for selection. Higher is better; zero means the
 * device is unusable (no graphics + present queue or no swapchain). Discrete GPUs and
 * RTX-capable GPUs are strongly preferred.
 */
static uint32_t R_Vk_ScoreDevice(VkPhysicalDevice device) {

  uint32_t graphics, present;
  R_Vk_FindQueueFamilies(device, &graphics, &present);

  if (graphics == R_VK_INVALID_QUEUE_FAMILY || present == R_VK_INVALID_QUEUE_FAMILY) {
    return 0;
  }

  const char *swapchain[] = { VK_KHR_SWAPCHAIN_EXTENSION_NAME };
  if (!R_Vk_DeviceHasExtensions(device, swapchain, lengthof(swapchain))) {
    return 0;
  }

  VkPhysicalDeviceProperties properties;
  vkGetPhysicalDeviceProperties(device, &properties);

  uint32_t score = 1;

  if (properties.deviceType == VK_PHYSICAL_DEVICE_TYPE_DISCRETE_GPU) {
    score += 1000;
  }

  if (R_Vk_DeviceHasExtensions(device, r_vk_rtx_extensions, lengthof(r_vk_rtx_extensions))) {
    score += 10000;
  }

  return score;
}

/**
 * @brief Selects the best available physical device, preferring discrete, RTX-capable
 * GPUs, and records whether ray tracing is available on the chosen device.
 */
static void R_Vk_SelectPhysicalDevice(void) {

  uint32_t count = 0;
  vkEnumeratePhysicalDevices(r_vk.instance, &count, NULL);

  if (count == 0) {
    Com_Error(ERROR_FATAL, "No Vulkan-capable devices found\n");
  }

  VkPhysicalDevice *devices = Mem_Malloc(sizeof(VkPhysicalDevice) * count);
  vkEnumeratePhysicalDevices(r_vk.instance, &count, devices);

  uint32_t best_score = 0;
  for (uint32_t i = 0; i < count; i++) {
    const uint32_t score = R_Vk_ScoreDevice(devices[i]);
    if (score > best_score) {
      best_score = score;
      r_vk.physical_device = devices[i];
    }
  }

  Mem_Free(devices);

  if (best_score == 0) {
    Com_Error(ERROR_FATAL, "No suitable Vulkan device (graphics + presentation)\n");
  }

  vkGetPhysicalDeviceProperties(r_vk.physical_device, &r_vk.physical_device_properties);

  R_Vk_FindQueueFamilies(r_vk.physical_device, &r_vk.graphics_queue_family, &r_vk.present_queue_family);

  r_vk.rtx = R_Vk_DeviceHasExtensions(r_vk.physical_device, r_vk_rtx_extensions,
                                      lengthof(r_vk_rtx_extensions));

  Com_Print("  Vulkan device: %s\n", r_vk.physical_device_properties.deviceName);
  Com_Print("  Ray tracing (RTX): %s\n", r_vk.rtx ? "available" : "not available");
}

/**
 * @brief Creates the logical device and retrieves the graphics and presentation
 * queues. The ray tracing extensions and their feature chain are enabled when the
 * device supports them, so phase B can build acceleration structures without a second
 * device creation.
 */
static void R_Vk_CreateDevice(void) {

  const float priority = 1.f;

  VkDeviceQueueCreateInfo queue_infos[2];
  uint32_t queue_count = 0;

  queue_infos[queue_count++] = (VkDeviceQueueCreateInfo) {
    .sType = VK_STRUCTURE_TYPE_DEVICE_QUEUE_CREATE_INFO,
    .queueFamilyIndex = r_vk.graphics_queue_family,
    .queueCount = 1,
    .pQueuePriorities = &priority
  };

  if (r_vk.present_queue_family != r_vk.graphics_queue_family) {
    queue_infos[queue_count++] = (VkDeviceQueueCreateInfo) {
      .sType = VK_STRUCTURE_TYPE_DEVICE_QUEUE_CREATE_INFO,
      .queueFamilyIndex = r_vk.present_queue_family,
      .queueCount = 1,
      .pQueuePriorities = &priority
    };
  }

  // always enable the swapchain; enable the RTX set when available
  const char *extensions[1 + lengthof(r_vk_rtx_extensions)];
  uint32_t extension_count = 0;
  extensions[extension_count++] = VK_KHR_SWAPCHAIN_EXTENSION_NAME;

  if (r_vk.rtx) {
    for (size_t i = 0; i < lengthof(r_vk_rtx_extensions); i++) {
      extensions[extension_count++] = r_vk_rtx_extensions[i];
    }
  }

  VkPhysicalDeviceRayTracingPipelineFeaturesKHR ray_tracing_features = {
    .sType = VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_RAY_TRACING_PIPELINE_FEATURES_KHR,
    .rayTracingPipeline = VK_TRUE
  };

  VkPhysicalDeviceAccelerationStructureFeaturesKHR acceleration_features = {
    .sType = VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_ACCELERATION_STRUCTURE_FEATURES_KHR,
    .accelerationStructure = VK_TRUE,
    .pNext = &ray_tracing_features
  };

  VkPhysicalDeviceBufferDeviceAddressFeatures buffer_address_features = {
    .sType = VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_BUFFER_DEVICE_ADDRESS_FEATURES,
    .bufferDeviceAddress = VK_TRUE,
    .pNext = &acceleration_features
  };

  VkDeviceCreateInfo create_info = {
    .sType = VK_STRUCTURE_TYPE_DEVICE_CREATE_INFO,
    .queueCreateInfoCount = queue_count,
    .pQueueCreateInfos = queue_infos,
    .enabledExtensionCount = extension_count,
    .ppEnabledExtensionNames = extensions,
    .pNext = r_vk.rtx ? (const void *) &buffer_address_features : NULL
  };

  const VkResult result = vkCreateDevice(r_vk.physical_device, &create_info, NULL, &r_vk.device);
  if (result != VK_SUCCESS) {
    Com_Error(ERROR_FATAL, "vkCreateDevice failed (%d)\n", result);
  }

  vkGetDeviceQueue(r_vk.device, r_vk.graphics_queue_family, 0, &r_vk.graphics_queue);
  vkGetDeviceQueue(r_vk.device, r_vk.present_queue_family, 0, &r_vk.present_queue);
}

/**
 * @brief Initializes the Vulkan backend: instance, surface, device selection (with
 * RTX detection), and logical device. This is the foundation; swapchain and frame
 * submission arrive in later phases (see VULKAN_RTX.md).
 */
void R_Vk_Init(SDL_Window *window) {

  memset(&r_vk, 0, sizeof(r_vk));

  R_Vk_CreateInstance();

  if (!SDL_Vulkan_CreateSurface(window, r_vk.instance, NULL, &r_vk.surface)) {
    Com_Error(ERROR_FATAL, "SDL_Vulkan_CreateSurface: %s\n", SDL_GetError());
  }

  R_Vk_SelectPhysicalDevice();

  R_Vk_CreateDevice();
}

/**
 * @brief Tears down the Vulkan backend in reverse order of creation.
 */
void R_Vk_Shutdown(void) {

  if (r_vk.device) {
    vkDeviceWaitIdle(r_vk.device);
    vkDestroyDevice(r_vk.device, NULL);
  }

  if (r_vk.surface) {
    vkDestroySurfaceKHR(r_vk.instance, r_vk.surface, NULL);
  }

  if (r_vk.instance) {
    vkDestroyInstance(r_vk.instance, NULL);
  }

  memset(&r_vk, 0, sizeof(r_vk));
}

#endif /* BUILD_VULKAN */
