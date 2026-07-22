/*
 * Copyright(c) 2026 QuetooPlus.
 *
 * This program is free software; you can redistribute it and/or
 * modify it under the terms of the GNU General Public License
 * as published by the Free Software Foundation; either version 2
 * of the License, or (at your option) any later version.
 */

#include "common/common.h"

#if BUILD_RTX

#include <vulkan/vulkan.h>

static bool R_Rtx_HasExtension(const VkExtensionProperties *extensions, uint32_t count, const char *name) {
  for (uint32_t i = 0; i < count; i++) {
    if (strcmp(extensions[i].extensionName, name) == 0) {
      return true;
    }
  }

  return false;
}

static bool R_Rtx_DeviceSupported(VkPhysicalDevice device) {
  uint32_t count = 0;
  if (vkEnumerateDeviceExtensionProperties(device, NULL, &count, NULL) != VK_SUCCESS || count == 0) {
    return false;
  }

  VkExtensionProperties *extensions = Mem_TagMalloc(sizeof(*extensions) * count, MEM_TAG_RENDERER);
  const VkResult result = vkEnumerateDeviceExtensionProperties(device, NULL, &count, extensions);
  if (result != VK_SUCCESS) {
    Mem_Free(extensions);
    return false;
  }

  static const char *const required[] = {
    VK_KHR_ACCELERATION_STRUCTURE_EXTENSION_NAME,
    VK_KHR_BUFFER_DEVICE_ADDRESS_EXTENSION_NAME,
    VK_KHR_DEFERRED_HOST_OPERATIONS_EXTENSION_NAME,
    VK_KHR_RAY_TRACING_PIPELINE_EXTENSION_NAME,
    VK_KHR_SPIRV_1_4_EXTENSION_NAME,
    VK_KHR_SHADER_FLOAT_CONTROLS_EXTENSION_NAME,
  };

  bool supported = true;
  for (size_t i = 0; i < lengthof(required); i++) {
    supported &= R_Rtx_HasExtension(extensions, count, required[i]);
  }
  Mem_Free(extensions);

  if (!supported) {
    return false;
  }

  VkPhysicalDeviceAccelerationStructureFeaturesKHR acceleration = {
    .sType = VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_ACCELERATION_STRUCTURE_FEATURES_KHR,
  };
  VkPhysicalDeviceRayTracingPipelineFeaturesKHR ray_tracing = {
    .sType = VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_RAY_TRACING_PIPELINE_FEATURES_KHR,
    .pNext = &acceleration,
  };
  VkPhysicalDeviceBufferDeviceAddressFeatures buffer_address = {
    .sType = VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_BUFFER_DEVICE_ADDRESS_FEATURES,
    .pNext = &ray_tracing,
  };
  VkPhysicalDeviceFeatures2 features = {
    .sType = VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_FEATURES_2,
    .pNext = &buffer_address,
  };
  vkGetPhysicalDeviceFeatures2(device, &features);

  return buffer_address.bufferDeviceAddress && acceleration.accelerationStructure && ray_tracing.rayTracingPipeline;
}

bool R_Rtx_IsSupported(void) {
  uint32_t api_version = VK_API_VERSION_1_0;
  const PFN_vkEnumerateInstanceVersion enumerate_instance_version =
      (PFN_vkEnumerateInstanceVersion) vkGetInstanceProcAddr(NULL, "vkEnumerateInstanceVersion");
  if (enumerate_instance_version) {
    enumerate_instance_version(&api_version);
  }

  if (VK_API_VERSION_MAJOR(api_version) < 1 ||
      (VK_API_VERSION_MAJOR(api_version) == 1 && VK_API_VERSION_MINOR(api_version) < 2)) {
    return false;
  }

  const VkApplicationInfo application = {
    .sType = VK_STRUCTURE_TYPE_APPLICATION_INFO,
    .pApplicationName = PACKAGE_NAME,
    .applicationVersion = VK_MAKE_API_VERSION(0, 1, 0, 0),
    .pEngineName = PACKAGE_NAME,
    .engineVersion = VK_MAKE_API_VERSION(0, 1, 0, 0),
    .apiVersion = VK_API_VERSION_1_2,
  };
  const VkInstanceCreateInfo create_info = {
    .sType = VK_STRUCTURE_TYPE_INSTANCE_CREATE_INFO,
    .pApplicationInfo = &application,
  };

  VkInstance instance;
  if (vkCreateInstance(&create_info, NULL, &instance) != VK_SUCCESS) {
    return false;
  }

  uint32_t count = 0;
  VkResult result = vkEnumeratePhysicalDevices(instance, &count, NULL);
  if (result != VK_SUCCESS || count == 0) {
    vkDestroyInstance(instance, NULL);
    return false;
  }

  VkPhysicalDevice *devices = Mem_TagMalloc(sizeof(*devices) * count, MEM_TAG_RENDERER);
  result = vkEnumeratePhysicalDevices(instance, &count, devices);

  bool supported = false;
  if (result == VK_SUCCESS) {
    for (uint32_t i = 0; i < count && !supported; i++) {
      supported = R_Rtx_DeviceSupported(devices[i]);
    }
  }

  Mem_Free(devices);
  vkDestroyInstance(instance, NULL);
  return supported;
}

void R_Rtx_Probe_f(void) {
  const bool supported = R_Rtx_IsSupported();
  Com_Print("Native Vulkan RTX support: ^%d%s^7\n", supported ? 2 : 1,
            supported ? "available" : "unavailable");
}

#else

bool R_Rtx_IsSupported(void) {
  return false;
}

#endif
