/*
 * Copyright(c) 2026 QuetooPlus.
 *
 * This program is free software; you can redistribute it and/or
 * modify it under the terms of the GNU General Public License
 * as published by the Free Software Foundation; either version 2
 * of the License, or (at your option) any later version.
 */

#include "common/common.h"
#include "r_rtx_device.h"

#if BUILD_RTX

static const char *const r_rtx_extensions[] = {
  VK_KHR_ACCELERATION_STRUCTURE_EXTENSION_NAME,
  VK_KHR_BUFFER_DEVICE_ADDRESS_EXTENSION_NAME,
  VK_KHR_DEFERRED_HOST_OPERATIONS_EXTENSION_NAME,
  VK_KHR_RAY_QUERY_EXTENSION_NAME,
  VK_KHR_RAY_TRACING_PIPELINE_EXTENSION_NAME,
  VK_KHR_SPIRV_1_4_EXTENSION_NAME,
  VK_KHR_SHADER_FLOAT_CONTROLS_EXTENSION_NAME,
};

static bool R_Rtx_HasExtension(const VkExtensionProperties *extensions, uint32_t count, const char *name) {
  for (uint32_t i = 0; i < count; i++) {
    if (strcmp(extensions[i].extensionName, name) == 0) {
      return true;
    }
  }

  return false;
}

static bool R_Rtx_FindQueueFamily(VkPhysicalDevice device, uint32_t *queue_family) {
  uint32_t count = 0;
  vkGetPhysicalDeviceQueueFamilyProperties(device, &count, NULL);
  if (count == 0) {
    return false;
  }

  VkQueueFamilyProperties *families = Mem_TagMalloc(sizeof(*families) * count, MEM_TAG_RENDERER);
  vkGetPhysicalDeviceQueueFamilyProperties(device, &count, families);

  bool found = false;
  for (uint32_t i = 0; i < count; i++) {
    if (families[i].queueCount && (families[i].queueFlags & VK_QUEUE_GRAPHICS_BIT)) {
      *queue_family = i;
      found = true;
      break;
    }
  }

  Mem_Free(families);
  return found;
}

static bool R_Rtx_DeviceSupported(VkPhysicalDevice device, uint32_t *queue_family) {
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

  bool supported = true;
  for (size_t i = 0; i < lengthof(r_rtx_extensions); i++) {
    supported &= R_Rtx_HasExtension(extensions, count, r_rtx_extensions[i]);
  }
  Mem_Free(extensions);

  if (!supported || !R_Rtx_FindQueueFamily(device, queue_family)) {
    return false;
  }

  VkPhysicalDeviceAccelerationStructureFeaturesKHR acceleration = {
    .sType = VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_ACCELERATION_STRUCTURE_FEATURES_KHR,
  };
  VkPhysicalDeviceRayTracingPipelineFeaturesKHR ray_tracing = {
    .sType = VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_RAY_TRACING_PIPELINE_FEATURES_KHR,
    .pNext = &acceleration,
  };
  VkPhysicalDeviceRayQueryFeaturesKHR ray_query = {
    .sType = VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_RAY_QUERY_FEATURES_KHR,
    .pNext = &ray_tracing,
  };
  VkPhysicalDeviceDescriptorIndexingFeatures indexing = {
    .sType = VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_DESCRIPTOR_INDEXING_FEATURES,
    .pNext = &ray_query,
  };
  VkPhysicalDeviceBufferDeviceAddressFeatures buffer_address = {
    .sType = VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_BUFFER_DEVICE_ADDRESS_FEATURES,
    .pNext = &indexing,
  };
  VkPhysicalDeviceFeatures2 features = {
    .sType = VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_FEATURES_2,
    .pNext = &buffer_address,
  };
  vkGetPhysicalDeviceFeatures2(device, &features);

  return buffer_address.bufferDeviceAddress && acceleration.accelerationStructure &&
         ray_tracing.rayTracingPipeline && ray_query.rayQuery &&
         indexing.shaderSampledImageArrayNonUniformIndexing &&
         indexing.descriptorBindingSampledImageUpdateAfterBind &&
         indexing.descriptorBindingPartiallyBound && indexing.runtimeDescriptorArray;
}

bool R_Rtx_DeviceInit(r_rtx_device_t *device) {
  assert(device);
  memset(device, 0, sizeof(*device));

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
  if (vkCreateInstance(&create_info, NULL, &device->instance) != VK_SUCCESS) {
    return false;
  }

  uint32_t count = 0;
  VkResult result = vkEnumeratePhysicalDevices(device->instance, &count, NULL);
  if (result != VK_SUCCESS || count == 0) {
    R_Rtx_DeviceShutdown(device);
    return false;
  }

  VkPhysicalDevice *devices = Mem_TagMalloc(sizeof(*devices) * count, MEM_TAG_RENDERER);
  result = vkEnumeratePhysicalDevices(device->instance, &count, devices);
  if (result == VK_SUCCESS) {
    for (uint32_t i = 0; i < count && device->physical_device == VK_NULL_HANDLE; i++) {
      uint32_t queue_family;
      if (R_Rtx_DeviceSupported(devices[i], &queue_family)) {
        device->physical_device = devices[i];
        device->queue_family = queue_family;
      }
    }
  }
  Mem_Free(devices);

  if (device->physical_device == VK_NULL_HANDLE) {
    R_Rtx_DeviceShutdown(device);
    return false;
  }

  const float priority = 1.f;
  const VkDeviceQueueCreateInfo queue = {
    .sType = VK_STRUCTURE_TYPE_DEVICE_QUEUE_CREATE_INFO,
    .queueFamilyIndex = device->queue_family,
    .queueCount = 1,
    .pQueuePriorities = &priority,
  };
  VkPhysicalDeviceAccelerationStructureFeaturesKHR acceleration = {
    .sType = VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_ACCELERATION_STRUCTURE_FEATURES_KHR,
    .accelerationStructure = VK_TRUE,
  };
  VkPhysicalDeviceRayTracingPipelineFeaturesKHR ray_tracing = {
    .sType = VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_RAY_TRACING_PIPELINE_FEATURES_KHR,
    .pNext = &acceleration,
    .rayTracingPipeline = VK_TRUE,
  };
  VkPhysicalDeviceRayQueryFeaturesKHR ray_query = {
    .sType = VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_RAY_QUERY_FEATURES_KHR,
    .pNext = &ray_tracing,
    .rayQuery = VK_TRUE,
  };
  VkPhysicalDeviceDescriptorIndexingFeatures indexing = {
    .sType = VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_DESCRIPTOR_INDEXING_FEATURES,
    .pNext = &ray_query,
    .shaderSampledImageArrayNonUniformIndexing = VK_TRUE,
    .descriptorBindingSampledImageUpdateAfterBind = VK_TRUE,
    .descriptorBindingPartiallyBound = VK_TRUE,
    .runtimeDescriptorArray = VK_TRUE,
  };
  VkPhysicalDeviceBufferDeviceAddressFeatures buffer_address = {
    .sType = VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_BUFFER_DEVICE_ADDRESS_FEATURES,
    .pNext = &indexing,
    .bufferDeviceAddress = VK_TRUE,
  };
  const VkDeviceCreateInfo device_info = {
    .sType = VK_STRUCTURE_TYPE_DEVICE_CREATE_INFO,
    .pNext = &buffer_address,
    .queueCreateInfoCount = 1,
    .pQueueCreateInfos = &queue,
    .enabledExtensionCount = lengthof(r_rtx_extensions),
    .ppEnabledExtensionNames = r_rtx_extensions,
  };
  if (vkCreateDevice(device->physical_device, &device_info, NULL, &device->device) != VK_SUCCESS) {
    R_Rtx_DeviceShutdown(device);
    return false;
  }

  vkGetDeviceQueue(device->device, device->queue_family, 0, &device->queue);

  const VkCommandPoolCreateInfo command_pool = {
    .sType = VK_STRUCTURE_TYPE_COMMAND_POOL_CREATE_INFO,
    .flags = VK_COMMAND_POOL_CREATE_RESET_COMMAND_BUFFER_BIT,
    .queueFamilyIndex = device->queue_family,
  };
  if (vkCreateCommandPool(device->device, &command_pool, NULL, &device->command_pool) != VK_SUCCESS) {
    R_Rtx_DeviceShutdown(device);
    return false;
  }

  return true;
}

bool R_Rtx_DeviceSmokeTest(const r_rtx_device_t *device) {
  assert(device);
  assert(device->device);
  assert(device->command_pool);

  VkCommandBuffer command;
  const VkCommandBufferAllocateInfo allocate = {
    .sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_ALLOCATE_INFO,
    .commandPool = device->command_pool,
    .level = VK_COMMAND_BUFFER_LEVEL_PRIMARY,
    .commandBufferCount = 1,
  };
  if (vkAllocateCommandBuffers(device->device, &allocate, &command) != VK_SUCCESS) {
    return false;
  }

  const VkCommandBufferBeginInfo begin = {
    .sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_BEGIN_INFO,
    .flags = VK_COMMAND_BUFFER_USAGE_ONE_TIME_SUBMIT_BIT,
  };
  bool succeeded = vkBeginCommandBuffer(command, &begin) == VK_SUCCESS &&
                   vkEndCommandBuffer(command) == VK_SUCCESS;
  if (succeeded) {
    const VkSubmitInfo submit = {
      .sType = VK_STRUCTURE_TYPE_SUBMIT_INFO,
      .commandBufferCount = 1,
      .pCommandBuffers = &command,
    };
    succeeded = vkQueueSubmit(device->queue, 1, &submit, VK_NULL_HANDLE) == VK_SUCCESS &&
                vkQueueWaitIdle(device->queue) == VK_SUCCESS;
  }

  vkFreeCommandBuffers(device->device, device->command_pool, 1, &command);
  return succeeded;
}

bool R_Rtx_FindMemoryType(const r_rtx_device_t *device, uint32_t type_bits,
                          VkMemoryPropertyFlags properties, uint32_t *memory_type) {
  VkPhysicalDeviceMemoryProperties memory;
  vkGetPhysicalDeviceMemoryProperties(device->physical_device, &memory);

  for (uint32_t i = 0; i < memory.memoryTypeCount; i++) {
    if ((type_bits & (1u << i)) &&
        (memory.memoryTypes[i].propertyFlags & properties) == properties) {
      *memory_type = i;
      return true;
    }
  }

  return false;
}

void R_Rtx_DeviceShutdown(r_rtx_device_t *device) {
  if (!device) {
    return;
  }

  if (device->device) {
    vkDeviceWaitIdle(device->device);
    if (device->command_pool) {
      vkDestroyCommandPool(device->device, device->command_pool, NULL);
    }
    vkDestroyDevice(device->device, NULL);
  }
  if (device->instance) {
    vkDestroyInstance(device->instance, NULL);
  }
  memset(device, 0, sizeof(*device));
}

#endif
