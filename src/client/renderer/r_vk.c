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

  // bindless texture sampling for the 2D UI and RTX closest-hit shaders. Core in
  // Vulkan 1.2 and enabled regardless of the ray-tracing feature set, so the 2D
  // path works even on devices without RTX. The RTX feature chain follows it.
  VkPhysicalDeviceDescriptorIndexingFeatures indexing_features = {
    .sType = VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_DESCRIPTOR_INDEXING_FEATURES,
    .shaderSampledImageArrayNonUniformIndexing = VK_TRUE,
    .runtimeDescriptorArray = VK_TRUE,
    .descriptorBindingPartiallyBound = VK_TRUE,
    .descriptorBindingSampledImageUpdateAfterBind = VK_TRUE,
    .pNext = r_vk.rtx ? (void *) &buffer_address_features : NULL
  };

  VkDeviceCreateInfo create_info = {
    .sType = VK_STRUCTURE_TYPE_DEVICE_CREATE_INFO,
    .queueCreateInfoCount = queue_count,
    .pQueueCreateInfos = queue_infos,
    .enabledExtensionCount = extension_count,
    .ppEnabledExtensionNames = extensions,
    .pNext = &indexing_features
  };

  const VkResult result = vkCreateDevice(r_vk.physical_device, &create_info, NULL, &r_vk.device);
  if (result != VK_SUCCESS) {
    Com_Error(ERROR_FATAL, "vkCreateDevice failed (%d)\n", result);
  }

  vkGetDeviceQueue(r_vk.device, r_vk.graphics_queue_family, 0, &r_vk.graphics_queue);
  vkGetDeviceQueue(r_vk.device, r_vk.present_queue_family, 0, &r_vk.present_queue);
}

/**
 * @brief Chooses a surface format, preferring 32-bit sRGB.
 */
static VkSurfaceFormatKHR R_Vk_ChooseSurfaceFormat(void) {

  uint32_t count = 0;
  vkGetPhysicalDeviceSurfaceFormatsKHR(r_vk.physical_device, r_vk.surface, &count, NULL);

  VkSurfaceFormatKHR *formats = Mem_Malloc(sizeof(VkSurfaceFormatKHR) * count);
  vkGetPhysicalDeviceSurfaceFormatsKHR(r_vk.physical_device, r_vk.surface, &count, formats);

  VkSurfaceFormatKHR chosen = formats[0];

  for (uint32_t i = 0; i < count; i++) {
    if (formats[i].format == VK_FORMAT_B8G8R8A8_SRGB &&
        formats[i].colorSpace == VK_COLOR_SPACE_SRGB_NONLINEAR_KHR) {
      chosen = formats[i];
      break;
    }
  }

  Mem_Free(formats);
  return chosen;
}

/**
 * @brief Chooses a present mode, preferring mailbox (low-latency triple buffering)
 * and falling back to FIFO, which is always available.
 */
static VkPresentModeKHR R_Vk_ChoosePresentMode(void) {

  uint32_t count = 0;
  vkGetPhysicalDeviceSurfacePresentModesKHR(r_vk.physical_device, r_vk.surface, &count, NULL);

  VkPresentModeKHR *modes = Mem_Malloc(sizeof(VkPresentModeKHR) * count);
  vkGetPhysicalDeviceSurfacePresentModesKHR(r_vk.physical_device, r_vk.surface, &count, modes);

  VkPresentModeKHR chosen = VK_PRESENT_MODE_FIFO_KHR;

  for (uint32_t i = 0; i < count; i++) {
    if (modes[i] == VK_PRESENT_MODE_MAILBOX_KHR) {
      chosen = modes[i];
      break;
    }
  }

  Mem_Free(modes);
  return chosen;
}

/**
 * @brief Resolves the swapchain extent from the surface capabilities, clamping the
 * window's pixel size when the surface allows a choice.
 */
static VkExtent2D R_Vk_ChooseExtent(SDL_Window *window, const VkSurfaceCapabilitiesKHR *caps) {

  if (caps->currentExtent.width != UINT32_MAX) {
    return caps->currentExtent;
  }

  int32_t w, h;
  SDL_GetWindowSizeInPixels(window, &w, &h);

  VkExtent2D extent = { (uint32_t) w, (uint32_t) h };
  extent.width = Clampf(extent.width, caps->minImageExtent.width, caps->maxImageExtent.width);
  extent.height = Clampf(extent.height, caps->minImageExtent.height, caps->maxImageExtent.height);
  return extent;
}

/**
 * @brief Creates the swapchain and retrieves its color images.
 */
static void R_Vk_CreateSwapchain(SDL_Window *window) {

  VkSurfaceCapabilitiesKHR caps;
  vkGetPhysicalDeviceSurfaceCapabilitiesKHR(r_vk.physical_device, r_vk.surface, &caps);

  const VkSurfaceFormatKHR format = R_Vk_ChooseSurfaceFormat();
  const VkPresentModeKHR present_mode = R_Vk_ChoosePresentMode();
  const VkExtent2D extent = R_Vk_ChooseExtent(window, &caps);

  uint32_t image_count = caps.minImageCount + 1;
  if (caps.maxImageCount > 0 && image_count > caps.maxImageCount) {
    image_count = caps.maxImageCount;
  }

  VkSwapchainCreateInfoKHR create_info = {
    .sType = VK_STRUCTURE_TYPE_SWAPCHAIN_CREATE_INFO_KHR,
    .surface = r_vk.surface,
    .minImageCount = image_count,
    .imageFormat = format.format,
    .imageColorSpace = format.colorSpace,
    .imageExtent = extent,
    .imageArrayLayers = 1,
    .imageUsage = VK_IMAGE_USAGE_COLOR_ATTACHMENT_BIT,
    .preTransform = caps.currentTransform,
    .compositeAlpha = VK_COMPOSITE_ALPHA_OPAQUE_BIT_KHR,
    .presentMode = present_mode,
    .clipped = VK_TRUE,
    .oldSwapchain = VK_NULL_HANDLE
  };

  const uint32_t indices[] = { r_vk.graphics_queue_family, r_vk.present_queue_family };
  if (r_vk.graphics_queue_family != r_vk.present_queue_family) {
    create_info.imageSharingMode = VK_SHARING_MODE_CONCURRENT;
    create_info.queueFamilyIndexCount = lengthof(indices);
    create_info.pQueueFamilyIndices = indices;
  } else {
    create_info.imageSharingMode = VK_SHARING_MODE_EXCLUSIVE;
  }

  if (vkCreateSwapchainKHR(r_vk.device, &create_info, NULL, &r_vk.swapchain) != VK_SUCCESS) {
    Com_Error(ERROR_FATAL, "vkCreateSwapchainKHR failed\n");
  }

  r_vk.swapchain_format = format.format;
  r_vk.swapchain_extent = extent;

  vkGetSwapchainImagesKHR(r_vk.device, r_vk.swapchain, &r_vk.swapchain_image_count, NULL);
  r_vk.swapchain_images = Mem_Malloc(sizeof(VkImage) * r_vk.swapchain_image_count);
  vkGetSwapchainImagesKHR(r_vk.device, r_vk.swapchain, &r_vk.swapchain_image_count,
                          r_vk.swapchain_images);
}

/**
 * @brief Creates a 2D image view for each swapchain image.
 */
static void R_Vk_CreateImageViews(void) {

  r_vk.swapchain_image_views = Mem_Malloc(sizeof(VkImageView) * r_vk.swapchain_image_count);

  for (uint32_t i = 0; i < r_vk.swapchain_image_count; i++) {

    const VkImageViewCreateInfo create_info = {
      .sType = VK_STRUCTURE_TYPE_IMAGE_VIEW_CREATE_INFO,
      .image = r_vk.swapchain_images[i],
      .viewType = VK_IMAGE_VIEW_TYPE_2D,
      .format = r_vk.swapchain_format,
      .subresourceRange = {
        .aspectMask = VK_IMAGE_ASPECT_COLOR_BIT,
        .levelCount = 1,
        .layerCount = 1
      }
    };

    if (vkCreateImageView(r_vk.device, &create_info, NULL, &r_vk.swapchain_image_views[i]) != VK_SUCCESS) {
      Com_Error(ERROR_FATAL, "vkCreateImageView failed\n");
    }
  }
}

/**
 * @brief Creates the render pass: a single color attachment that is cleared on load,
 * stored, and transitioned to the present layout.
 */
static void R_Vk_CreateRenderPass(void) {

  const VkAttachmentDescription color_attachment = {
    .format = r_vk.swapchain_format,
    .samples = VK_SAMPLE_COUNT_1_BIT,
    .loadOp = VK_ATTACHMENT_LOAD_OP_CLEAR,
    .storeOp = VK_ATTACHMENT_STORE_OP_STORE,
    .stencilLoadOp = VK_ATTACHMENT_LOAD_OP_DONT_CARE,
    .stencilStoreOp = VK_ATTACHMENT_STORE_OP_DONT_CARE,
    .initialLayout = VK_IMAGE_LAYOUT_UNDEFINED,
    .finalLayout = VK_IMAGE_LAYOUT_PRESENT_SRC_KHR
  };

  const VkAttachmentReference color_ref = {
    .attachment = 0,
    .layout = VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL
  };

  const VkSubpassDescription subpass = {
    .pipelineBindPoint = VK_PIPELINE_BIND_POINT_GRAPHICS,
    .colorAttachmentCount = 1,
    .pColorAttachments = &color_ref
  };

  const VkSubpassDependency dependency = {
    .srcSubpass = VK_SUBPASS_EXTERNAL,
    .dstSubpass = 0,
    .srcStageMask = VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT,
    .dstStageMask = VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT,
    .srcAccessMask = 0,
    .dstAccessMask = VK_ACCESS_COLOR_ATTACHMENT_WRITE_BIT
  };

  const VkRenderPassCreateInfo create_info = {
    .sType = VK_STRUCTURE_TYPE_RENDER_PASS_CREATE_INFO,
    .attachmentCount = 1,
    .pAttachments = &color_attachment,
    .subpassCount = 1,
    .pSubpasses = &subpass,
    .dependencyCount = 1,
    .pDependencies = &dependency
  };

  if (vkCreateRenderPass(r_vk.device, &create_info, NULL, &r_vk.render_pass) != VK_SUCCESS) {
    Com_Error(ERROR_FATAL, "vkCreateRenderPass failed\n");
  }
}

/**
 * @brief Creates a framebuffer for each swapchain image view.
 */
static void R_Vk_CreateFramebuffers(void) {

  r_vk.framebuffers = Mem_Malloc(sizeof(VkFramebuffer) * r_vk.swapchain_image_count);

  for (uint32_t i = 0; i < r_vk.swapchain_image_count; i++) {

    const VkFramebufferCreateInfo create_info = {
      .sType = VK_STRUCTURE_TYPE_FRAMEBUFFER_CREATE_INFO,
      .renderPass = r_vk.render_pass,
      .attachmentCount = 1,
      .pAttachments = &r_vk.swapchain_image_views[i],
      .width = r_vk.swapchain_extent.width,
      .height = r_vk.swapchain_extent.height,
      .layers = 1
    };

    if (vkCreateFramebuffer(r_vk.device, &create_info, NULL, &r_vk.framebuffers[i]) != VK_SUCCESS) {
      Com_Error(ERROR_FATAL, "vkCreateFramebuffer failed\n");
    }
  }
}

/**
 * @brief Creates the command pool and the per-frame primary command buffers.
 */
static void R_Vk_CreateCommandBuffers(void) {

  const VkCommandPoolCreateInfo pool_info = {
    .sType = VK_STRUCTURE_TYPE_COMMAND_POOL_CREATE_INFO,
    .flags = VK_COMMAND_POOL_CREATE_RESET_COMMAND_BUFFER_BIT,
    .queueFamilyIndex = r_vk.graphics_queue_family
  };

  if (vkCreateCommandPool(r_vk.device, &pool_info, NULL, &r_vk.command_pool) != VK_SUCCESS) {
    Com_Error(ERROR_FATAL, "vkCreateCommandPool failed\n");
  }

  const VkCommandBufferAllocateInfo alloc_info = {
    .sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_ALLOCATE_INFO,
    .commandPool = r_vk.command_pool,
    .level = VK_COMMAND_BUFFER_LEVEL_PRIMARY,
    .commandBufferCount = R_VK_MAX_FRAMES_IN_FLIGHT
  };

  if (vkAllocateCommandBuffers(r_vk.device, &alloc_info, r_vk.command_buffers) != VK_SUCCESS) {
    Com_Error(ERROR_FATAL, "vkAllocateCommandBuffers failed\n");
  }
}

/**
 * @brief Creates the per-frame semaphores and fences.
 */
static void R_Vk_CreateSyncObjects(void) {

  const VkSemaphoreCreateInfo semaphore_info = {
    .sType = VK_STRUCTURE_TYPE_SEMAPHORE_CREATE_INFO
  };

  const VkFenceCreateInfo fence_info = {
    .sType = VK_STRUCTURE_TYPE_FENCE_CREATE_INFO,
    .flags = VK_FENCE_CREATE_SIGNALED_BIT
  };

  for (uint32_t i = 0; i < R_VK_MAX_FRAMES_IN_FLIGHT; i++) {
    if (vkCreateSemaphore(r_vk.device, &semaphore_info, NULL, &r_vk.image_available[i]) != VK_SUCCESS ||
        vkCreateSemaphore(r_vk.device, &semaphore_info, NULL, &r_vk.render_finished[i]) != VK_SUCCESS ||
        vkCreateFence(r_vk.device, &fence_info, NULL, &r_vk.in_flight[i]) != VK_SUCCESS) {
      Com_Error(ERROR_FATAL, "Failed to create Vulkan synchronization primitives\n");
    }
  }
}

/**
 * @brief Records and submits a frame that clears the swapchain image to the given
 * color and presents it. This is the phase 1 milestone: a complete acquire / record /
 * submit / present cycle. Later phases record real geometry into the same render pass.
 */
void R_Vk_DrawClear(float r, float g, float b) {

  const uint32_t frame = r_vk.current_frame;

  vkWaitForFences(r_vk.device, 1, &r_vk.in_flight[frame], VK_TRUE, UINT64_MAX);

  uint32_t image_index;
  VkResult result = vkAcquireNextImageKHR(r_vk.device, r_vk.swapchain, UINT64_MAX,
                                          r_vk.image_available[frame], VK_NULL_HANDLE, &image_index);
  if (result == VK_ERROR_OUT_OF_DATE_KHR) {
    return; // swapchain recreation is handled by a later phase
  }

  vkResetFences(r_vk.device, 1, &r_vk.in_flight[frame]);

  VkCommandBuffer cb = r_vk.command_buffers[frame];
  vkResetCommandBuffer(cb, 0);

  const VkCommandBufferBeginInfo begin_info = {
    .sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_BEGIN_INFO
  };
  vkBeginCommandBuffer(cb, &begin_info);

  // clear the frame to black and composite the 2D UI (menu / console) on top;
  // this is the no-world path, so there is no ray-traced image to preserve
  (void) r;
  (void) g;
  (void) b;
  R_Vk_Draw2D(cb, image_index, false);
  vkEndCommandBuffer(cb);

  const VkPipelineStageFlags wait_stage = VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT;

  const VkSubmitInfo submit_info = {
    .sType = VK_STRUCTURE_TYPE_SUBMIT_INFO,
    .waitSemaphoreCount = 1,
    .pWaitSemaphores = &r_vk.image_available[frame],
    .pWaitDstStageMask = &wait_stage,
    .commandBufferCount = 1,
    .pCommandBuffers = &cb,
    .signalSemaphoreCount = 1,
    .pSignalSemaphores = &r_vk.render_finished[frame]
  };

  if (vkQueueSubmit(r_vk.graphics_queue, 1, &submit_info, r_vk.in_flight[frame]) != VK_SUCCESS) {
    Com_Error(ERROR_FATAL, "vkQueueSubmit failed\n");
  }

  const VkPresentInfoKHR present_info = {
    .sType = VK_STRUCTURE_TYPE_PRESENT_INFO_KHR,
    .waitSemaphoreCount = 1,
    .pWaitSemaphores = &r_vk.render_finished[frame],
    .swapchainCount = 1,
    .pSwapchains = &r_vk.swapchain,
    .pImageIndices = &image_index
  };

  vkQueuePresentKHR(r_vk.present_queue, &present_info);

  r_vk.current_frame = (frame + 1) % R_VK_MAX_FRAMES_IN_FLIGHT;
}

/**
 * @brief Brings up the swapchain and everything required to present frames: image
 * views, render pass, framebuffers, command buffers, and synchronization. Must be
 * called after `R_Vk_Init`.
 */
void R_Vk_InitSwapchain(SDL_Window *window) {

  R_Vk_CreateSwapchain(window);
  R_Vk_CreateImageViews();
  R_Vk_CreateRenderPass();
  R_Vk_CreateFramebuffers();
  R_Vk_CreateCommandBuffers();
  R_Vk_CreateSyncObjects();
}

/**
 * @brief Destroys the swapchain and its dependent resources, in reverse order.
 */
static void R_Vk_DestroySwapchain(void) {

  for (uint32_t i = 0; i < R_VK_MAX_FRAMES_IN_FLIGHT; i++) {
    if (r_vk.render_finished[i]) {
      vkDestroySemaphore(r_vk.device, r_vk.render_finished[i], NULL);
    }
    if (r_vk.image_available[i]) {
      vkDestroySemaphore(r_vk.device, r_vk.image_available[i], NULL);
    }
    if (r_vk.in_flight[i]) {
      vkDestroyFence(r_vk.device, r_vk.in_flight[i], NULL);
    }
  }

  if (r_vk.command_pool) {
    vkDestroyCommandPool(r_vk.device, r_vk.command_pool, NULL);
  }

  if (r_vk.framebuffers) {
    for (uint32_t i = 0; i < r_vk.swapchain_image_count; i++) {
      vkDestroyFramebuffer(r_vk.device, r_vk.framebuffers[i], NULL);
    }
    Mem_Free(r_vk.framebuffers);
  }

  if (r_vk.render_pass) {
    vkDestroyRenderPass(r_vk.device, r_vk.render_pass, NULL);
  }

  if (r_vk.swapchain_image_views) {
    for (uint32_t i = 0; i < r_vk.swapchain_image_count; i++) {
      vkDestroyImageView(r_vk.device, r_vk.swapchain_image_views[i], NULL);
    }
    Mem_Free(r_vk.swapchain_image_views);
  }

  if (r_vk.swapchain_images) {
    Mem_Free(r_vk.swapchain_images);
  }

  if (r_vk.swapchain) {
    vkDestroySwapchainKHR(r_vk.device, r_vk.swapchain, NULL);
  }
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
    R_Vk_ShutdownDraw2D();
    R_Vk_ShutdownImages();
    R_Vk_DestroySwapchain();
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
