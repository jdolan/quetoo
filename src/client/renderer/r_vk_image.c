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

/**
 * @file
 * @brief Vulkan texture upload and the bindless combined-image-sampler array.
 * Surfaces are uploaded to device-local images with full mip chains and
 * registered into a single large descriptor array, addressed by a stable
 * index. Both the 2D UI pass and the RTX closest-hit shader sample from this
 * one array, so a texture is uploaded once regardless of which Vulkan feature
 * set the device exposes. The translation unit is compiled only when
 * `BUILD_VULKAN` is defined; the OpenGL backend is unaffected.
 */

/**
 * @brief Maximum number of textures the bindless array can hold. The descriptor
 * binding is `PARTIALLY_BOUND`, so unused slots cost nothing.
 */
#define R_VK_MAX_TEXTURES 4096

/**
 * @brief A single uploaded texture (device-local image + its view and memory).
 */
typedef struct {
  VkImage image;
  VkDeviceMemory memory;
  VkImageView view;
} r_vk_texture_t;

/**
 * @brief The bindless texture array state. Index 0 is reserved for a 1x1 opaque
 * white texture so untextured 2D draws can sample a neutral value.
 */
static struct {
  _Bool initialized;

  VkSampler sampler;

  VkDescriptorSetLayout set_layout;
  VkDescriptorPool descriptor_pool;
  VkDescriptorSet descriptor_set;

  uint32_t count;
  r_vk_texture_t textures[R_VK_MAX_TEXTURES];

  GHashTable *by_path; // (gchar *) path -> (gpointer) (index + 1)
} r_vk_images;

/**
 * @brief Selects a memory type satisfying `bits` with the desired `want` flags.
 * A private copy of the RTX helper so this unit stays independent of r_vk_rtx.c
 * and is usable on devices without the ray-tracing feature set.
 */
static uint32_t R_Vk_Img_FindMemoryType(uint32_t bits, VkMemoryPropertyFlags want) {

  VkPhysicalDeviceMemoryProperties mp;
  vkGetPhysicalDeviceMemoryProperties(r_vk.physical_device, &mp);

  for (uint32_t i = 0; i < mp.memoryTypeCount; i++) {
    if ((bits & (1u << i)) && (mp.memoryTypes[i].propertyFlags & want) == want) {
      return i;
    }
  }

  Com_Error(ERROR_FATAL, "No suitable Vulkan memory type\n");
  return 0;
}

/**
 * @brief Allocates and begins a one-shot primary command buffer.
 */
static VkCommandBuffer R_Vk_Img_BeginCommand(void) {

  const VkCommandBufferAllocateInfo ai = {
    .sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_ALLOCATE_INFO,
    .commandPool = r_vk.command_pool,
    .level = VK_COMMAND_BUFFER_LEVEL_PRIMARY,
    .commandBufferCount = 1
  };

  VkCommandBuffer cb;
  vkAllocateCommandBuffers(r_vk.device, &ai, &cb);

  const VkCommandBufferBeginInfo bi = {
    .sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_BEGIN_INFO,
    .flags = VK_COMMAND_BUFFER_USAGE_ONE_TIME_SUBMIT_BIT
  };
  vkBeginCommandBuffer(cb, &bi);
  return cb;
}

/**
 * @brief Ends, submits and waits on a one-shot command buffer, then frees it.
 */
static void R_Vk_Img_EndCommand(VkCommandBuffer cb) {

  vkEndCommandBuffer(cb);

  const VkSubmitInfo si = {
    .sType = VK_STRUCTURE_TYPE_SUBMIT_INFO,
    .commandBufferCount = 1,
    .pCommandBuffers = &cb
  };

  vkQueueSubmit(r_vk.graphics_queue, 1, &si, VK_NULL_HANDLE);
  vkQueueWaitIdle(r_vk.graphics_queue);
  vkFreeCommandBuffers(r_vk.device, r_vk.command_pool, 1, &cb);
}

/**
 * @brief Lazily creates the sampler, the bindless descriptor set, and the
 * reserved white fallback texture at index 0. Safe to call repeatedly.
 */
static void R_Vk_InitImages(void) {

  if (r_vk_images.initialized) {
    return;
  }

  const VkSamplerCreateInfo sci = {
    .sType = VK_STRUCTURE_TYPE_SAMPLER_CREATE_INFO,
    .magFilter = VK_FILTER_LINEAR,
    .minFilter = VK_FILTER_LINEAR,
    .mipmapMode = VK_SAMPLER_MIPMAP_MODE_LINEAR,
    .addressModeU = VK_SAMPLER_ADDRESS_MODE_REPEAT,
    .addressModeV = VK_SAMPLER_ADDRESS_MODE_REPEAT,
    .addressModeW = VK_SAMPLER_ADDRESS_MODE_REPEAT,
    .maxLod = VK_LOD_CLAMP_NONE,
    .borderColor = VK_BORDER_COLOR_FLOAT_OPAQUE_WHITE
  };
  if (vkCreateSampler(r_vk.device, &sci, NULL, &r_vk_images.sampler) != VK_SUCCESS) {
    Com_Error(ERROR_FATAL, "vkCreateSampler failed\n");
  }

  // a single large, partially-bound, update-after-bind array of samplers
  const VkDescriptorBindingFlags binding_flags =
    VK_DESCRIPTOR_BINDING_PARTIALLY_BOUND_BIT |
    VK_DESCRIPTOR_BINDING_UPDATE_AFTER_BIND_BIT;

  const VkDescriptorSetLayoutBindingFlagsCreateInfo bfci = {
    .sType = VK_STRUCTURE_TYPE_DESCRIPTOR_SET_LAYOUT_BINDING_FLAGS_CREATE_INFO,
    .bindingCount = 1,
    .pBindingFlags = &binding_flags
  };

  const VkDescriptorSetLayoutBinding binding = {
    .binding = 0,
    .descriptorType = VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER,
    .descriptorCount = R_VK_MAX_TEXTURES,
    .stageFlags = VK_SHADER_STAGE_ALL
  };

  const VkDescriptorSetLayoutCreateInfo dslci = {
    .sType = VK_STRUCTURE_TYPE_DESCRIPTOR_SET_LAYOUT_CREATE_INFO,
    .pNext = &bfci,
    .flags = VK_DESCRIPTOR_SET_LAYOUT_CREATE_UPDATE_AFTER_BIND_POOL_BIT,
    .bindingCount = 1,
    .pBindings = &binding
  };
  if (vkCreateDescriptorSetLayout(r_vk.device, &dslci, NULL, &r_vk_images.set_layout) != VK_SUCCESS) {
    Com_Error(ERROR_FATAL, "vkCreateDescriptorSetLayout (bindless textures) failed\n");
  }

  const VkDescriptorPoolSize pool_size = {
    .type = VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER,
    .descriptorCount = R_VK_MAX_TEXTURES
  };

  const VkDescriptorPoolCreateInfo dpci = {
    .sType = VK_STRUCTURE_TYPE_DESCRIPTOR_POOL_CREATE_INFO,
    .flags = VK_DESCRIPTOR_POOL_CREATE_UPDATE_AFTER_BIND_BIT,
    .maxSets = 1,
    .poolSizeCount = 1,
    .pPoolSizes = &pool_size
  };
  if (vkCreateDescriptorPool(r_vk.device, &dpci, NULL, &r_vk_images.descriptor_pool) != VK_SUCCESS) {
    Com_Error(ERROR_FATAL, "vkCreateDescriptorPool (bindless textures) failed\n");
  }

  const VkDescriptorSetAllocateInfo dsai = {
    .sType = VK_STRUCTURE_TYPE_DESCRIPTOR_SET_ALLOCATE_INFO,
    .descriptorPool = r_vk_images.descriptor_pool,
    .descriptorSetCount = 1,
    .pSetLayouts = &r_vk_images.set_layout
  };
  if (vkAllocateDescriptorSets(r_vk.device, &dsai, &r_vk_images.descriptor_set) != VK_SUCCESS) {
    Com_Error(ERROR_FATAL, "vkAllocateDescriptorSets (bindless textures) failed\n");
  }

  r_vk_images.by_path = g_hash_table_new_full(g_str_hash, g_str_equal, g_free, NULL);
  r_vk_images.initialized = true;

  // reserve index 0 as 1x1 opaque white
  const byte white[4] = { 255, 255, 255, 255 };
  SDL_Surface *surface = SDL_CreateSurfaceFrom(1, 1, SDL_PIXELFORMAT_RGBA32, (void *) white, 4);
  R_Vk_UploadSurface(surface);
  SDL_DestroySurface(surface);
}

/**
 * @brief Writes the texture at `index` into the bindless descriptor array.
 */
static void R_Vk_WriteTextureDescriptor(uint32_t index) {

  const VkDescriptorImageInfo image_info = {
    .sampler = r_vk_images.sampler,
    .imageView = r_vk_images.textures[index].view,
    .imageLayout = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL
  };

  const VkWriteDescriptorSet write = {
    .sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET,
    .dstSet = r_vk_images.descriptor_set,
    .dstBinding = 0,
    .dstArrayElement = index,
    .descriptorCount = 1,
    .descriptorType = VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER,
    .pImageInfo = &image_info
  };

  vkUpdateDescriptorSets(r_vk.device, 1, &write, 0, NULL);
}

/**
 * @brief Uploads an SDL surface to a device-local, mipmapped, sampled image and
 * registers it in the bindless array. Returns the array index (0, the white
 * texture, on failure).
 */
uint32_t R_Vk_UploadSurface(SDL_Surface *surface) {

  R_Vk_InitImages();

  if (!surface) {
    return 0;
  }

  if (r_vk_images.count >= R_VK_MAX_TEXTURES) {
    Com_Warn("Vulkan bindless texture array full (%d)\n", R_VK_MAX_TEXTURES);
    return 0;
  }

  SDL_Surface *rgba = SDL_ConvertSurface(surface, SDL_PIXELFORMAT_RGBA32);
  if (!rgba) {
    return 0;
  }

  const uint32_t w = (uint32_t) rgba->w;
  const uint32_t h = (uint32_t) rgba->h;

  uint32_t levels = 1;
  for (uint32_t m = Maxi(w, h); m > 1; m >>= 1) {
    levels++;
  }

  // device-local sampled image; TRANSFER_SRC is needed for mip generation
  const VkImageCreateInfo ici = {
    .sType = VK_STRUCTURE_TYPE_IMAGE_CREATE_INFO,
    .imageType = VK_IMAGE_TYPE_2D,
    .format = VK_FORMAT_R8G8B8A8_UNORM,
    .extent = { w, h, 1 },
    .mipLevels = levels,
    .arrayLayers = 1,
    .samples = VK_SAMPLE_COUNT_1_BIT,
    .tiling = VK_IMAGE_TILING_OPTIMAL,
    .usage = VK_IMAGE_USAGE_SAMPLED_BIT | VK_IMAGE_USAGE_TRANSFER_DST_BIT | VK_IMAGE_USAGE_TRANSFER_SRC_BIT,
    .initialLayout = VK_IMAGE_LAYOUT_UNDEFINED
  };

  VkImage image;
  if (vkCreateImage(r_vk.device, &ici, NULL, &image) != VK_SUCCESS) {
    Com_Error(ERROR_FATAL, "vkCreateImage failed\n");
  }

  VkMemoryRequirements req;
  vkGetImageMemoryRequirements(r_vk.device, image, &req);
  const VkMemoryAllocateInfo iai = {
    .sType = VK_STRUCTURE_TYPE_MEMORY_ALLOCATE_INFO,
    .allocationSize = req.size,
    .memoryTypeIndex = R_Vk_Img_FindMemoryType(req.memoryTypeBits, VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT)
  };
  VkDeviceMemory memory;
  if (vkAllocateMemory(r_vk.device, &iai, NULL, &memory) != VK_SUCCESS) {
    Com_Error(ERROR_FATAL, "vkAllocateMemory (image) failed\n");
  }
  vkBindImageMemory(r_vk.device, image, memory, 0);

  // host-visible staging buffer holding tightly-packed RGBA rows
  const VkDeviceSize size = (VkDeviceSize) w * h * 4;
  const VkBufferCreateInfo bci = {
    .sType = VK_STRUCTURE_TYPE_BUFFER_CREATE_INFO,
    .size = size,
    .usage = VK_BUFFER_USAGE_TRANSFER_SRC_BIT,
    .sharingMode = VK_SHARING_MODE_EXCLUSIVE
  };
  VkBuffer staging;
  if (vkCreateBuffer(r_vk.device, &bci, NULL, &staging) != VK_SUCCESS) {
    Com_Error(ERROR_FATAL, "vkCreateBuffer (staging) failed\n");
  }
  VkMemoryRequirements sreq;
  vkGetBufferMemoryRequirements(r_vk.device, staging, &sreq);
  const VkMemoryAllocateInfo sai = {
    .sType = VK_STRUCTURE_TYPE_MEMORY_ALLOCATE_INFO,
    .allocationSize = sreq.size,
    .memoryTypeIndex = R_Vk_Img_FindMemoryType(sreq.memoryTypeBits,
      VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT | VK_MEMORY_PROPERTY_HOST_COHERENT_BIT)
  };
  VkDeviceMemory staging_memory;
  if (vkAllocateMemory(r_vk.device, &sai, NULL, &staging_memory) != VK_SUCCESS) {
    Com_Error(ERROR_FATAL, "vkAllocateMemory (staging) failed\n");
  }
  vkBindBufferMemory(r_vk.device, staging, staging_memory, 0);

  void *mapped;
  vkMapMemory(r_vk.device, staging_memory, 0, size, 0, &mapped);
  for (uint32_t y = 0; y < h; y++) {
    memcpy((byte *) mapped + (size_t) y * w * 4, (const byte *) rgba->pixels + (size_t) y * rgba->pitch, (size_t) w * 4);
  }
  vkUnmapMemory(r_vk.device, staging_memory);

  VkCommandBuffer cb = R_Vk_Img_BeginCommand();

  // all levels UNDEFINED -> TRANSFER_DST
  VkImageMemoryBarrier barrier = {
    .sType = VK_STRUCTURE_TYPE_IMAGE_MEMORY_BARRIER,
    .srcQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED,
    .dstQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED,
    .image = image,
    .subresourceRange = { VK_IMAGE_ASPECT_COLOR_BIT, 0, levels, 0, 1 },
    .oldLayout = VK_IMAGE_LAYOUT_UNDEFINED,
    .newLayout = VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL,
    .srcAccessMask = 0,
    .dstAccessMask = VK_ACCESS_TRANSFER_WRITE_BIT
  };
  vkCmdPipelineBarrier(cb, VK_PIPELINE_STAGE_TOP_OF_PIPE_BIT, VK_PIPELINE_STAGE_TRANSFER_BIT,
    0, 0, NULL, 0, NULL, 1, &barrier);

  const VkBufferImageCopy copy = {
    .imageSubresource = { VK_IMAGE_ASPECT_COLOR_BIT, 0, 0, 1 },
    .imageExtent = { w, h, 1 }
  };
  vkCmdCopyBufferToImage(cb, staging, image, VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL, 1, &copy);

  // generate the mip chain by successive linear blits
  int32_t mip_w = (int32_t) w;
  int32_t mip_h = (int32_t) h;
  barrier.subresourceRange.levelCount = 1;
  for (uint32_t i = 1; i < levels; i++) {

    barrier.subresourceRange.baseMipLevel = i - 1;
    barrier.oldLayout = VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL;
    barrier.newLayout = VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL;
    barrier.srcAccessMask = VK_ACCESS_TRANSFER_WRITE_BIT;
    barrier.dstAccessMask = VK_ACCESS_TRANSFER_READ_BIT;
    vkCmdPipelineBarrier(cb, VK_PIPELINE_STAGE_TRANSFER_BIT, VK_PIPELINE_STAGE_TRANSFER_BIT,
      0, 0, NULL, 0, NULL, 1, &barrier);

    const VkImageBlit blit = {
      .srcSubresource = { VK_IMAGE_ASPECT_COLOR_BIT, i - 1, 0, 1 },
      .srcOffsets = { { 0, 0, 0 }, { mip_w, mip_h, 1 } },
      .dstSubresource = { VK_IMAGE_ASPECT_COLOR_BIT, i, 0, 1 },
      .dstOffsets = { { 0, 0, 0 }, { mip_w > 1 ? mip_w / 2 : 1, mip_h > 1 ? mip_h / 2 : 1, 1 } }
    };
    vkCmdBlitImage(cb, image, VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL,
      image, VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL, 1, &blit, VK_FILTER_LINEAR);

    barrier.oldLayout = VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL;
    barrier.newLayout = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL;
    barrier.srcAccessMask = VK_ACCESS_TRANSFER_READ_BIT;
    barrier.dstAccessMask = VK_ACCESS_SHADER_READ_BIT;
    vkCmdPipelineBarrier(cb, VK_PIPELINE_STAGE_TRANSFER_BIT, VK_PIPELINE_STAGE_FRAGMENT_SHADER_BIT,
      0, 0, NULL, 0, NULL, 1, &barrier);

    if (mip_w > 1) {
      mip_w /= 2;
    }
    if (mip_h > 1) {
      mip_h /= 2;
    }
  }

  // last level TRANSFER_DST -> SHADER_READ_ONLY
  barrier.subresourceRange.baseMipLevel = levels - 1;
  barrier.oldLayout = VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL;
  barrier.newLayout = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL;
  barrier.srcAccessMask = VK_ACCESS_TRANSFER_WRITE_BIT;
  barrier.dstAccessMask = VK_ACCESS_SHADER_READ_BIT;
  vkCmdPipelineBarrier(cb, VK_PIPELINE_STAGE_TRANSFER_BIT, VK_PIPELINE_STAGE_FRAGMENT_SHADER_BIT,
    0, 0, NULL, 0, NULL, 1, &barrier);

  R_Vk_Img_EndCommand(cb);

  vkDestroyBuffer(r_vk.device, staging, NULL);
  vkFreeMemory(r_vk.device, staging_memory, NULL);

  const VkImageViewCreateInfo ivci = {
    .sType = VK_STRUCTURE_TYPE_IMAGE_VIEW_CREATE_INFO,
    .image = image,
    .viewType = VK_IMAGE_VIEW_TYPE_2D,
    .format = VK_FORMAT_R8G8B8A8_UNORM,
    .subresourceRange = { VK_IMAGE_ASPECT_COLOR_BIT, 0, levels, 0, 1 }
  };
  VkImageView view;
  if (vkCreateImageView(r_vk.device, &ivci, NULL, &view) != VK_SUCCESS) {
    Com_Error(ERROR_FATAL, "vkCreateImageView (texture) failed\n");
  }

  const uint32_t index = r_vk_images.count++;
  r_vk_images.textures[index] = (r_vk_texture_t) { image, memory, view };
  R_Vk_WriteTextureDescriptor(index);

  SDL_DestroySurface(rgba);
  return index;
}

/**
 * @brief Loads the image at `path` (cached by name) and uploads it to the
 * bindless array. Returns the array index, or 0 (white) if the path is empty or
 * the image cannot be loaded.
 */
uint32_t R_Vk_LoadTexture(const char *path) {

  R_Vk_InitImages();

  if (!path || !*path) {
    return 0;
  }

  gpointer cached = g_hash_table_lookup(r_vk_images.by_path, path);
  if (cached) {
    return GPOINTER_TO_UINT(cached) - 1;
  }

  SDL_Surface *surface = Img_LoadSurface(path);
  if (!surface) {
    Com_Debug(DEBUG_RENDERER, "Failed to load Vulkan texture %s\n", path);
    return 0;
  }

  const uint32_t index = R_Vk_UploadSurface(surface);
  SDL_DestroySurface(surface);

  g_hash_table_insert(r_vk_images.by_path, g_strdup(path), GUINT_TO_POINTER(index + 1));
  return index;
}

/**
 * @brief Returns the bindless index of the reserved 1x1 white texture.
 */
uint32_t R_Vk_WhiteTexture(void) {
  R_Vk_InitImages();
  return 0;
}

/**
 * @brief Returns the number of textures currently uploaded (including white).
 */
uint32_t R_Vk_TextureCount(void) {
  return r_vk_images.count;
}

/**
 * @brief Returns the bindless descriptor set layout (for pipeline layouts).
 */
VkDescriptorSetLayout R_Vk_TextureSetLayout(void) {
  R_Vk_InitImages();
  return r_vk_images.set_layout;
}

/**
 * @brief Returns the bindless descriptor set (to bind before drawing).
 */
VkDescriptorSet R_Vk_TextureSet(void) {
  R_Vk_InitImages();
  return r_vk_images.descriptor_set;
}

/**
 * @brief Destroys all texture resources. Called from R_Vk_Shutdown before the
 * device is destroyed.
 */
void R_Vk_ShutdownImages(void) {

  if (!r_vk_images.initialized) {
    return;
  }

  for (uint32_t i = 0; i < r_vk_images.count; i++) {
    vkDestroyImageView(r_vk.device, r_vk_images.textures[i].view, NULL);
    vkDestroyImage(r_vk.device, r_vk_images.textures[i].image, NULL);
    vkFreeMemory(r_vk.device, r_vk_images.textures[i].memory, NULL);
  }

  vkDestroyDescriptorPool(r_vk.device, r_vk_images.descriptor_pool, NULL);
  vkDestroyDescriptorSetLayout(r_vk.device, r_vk_images.set_layout, NULL);
  vkDestroySampler(r_vk.device, r_vk_images.sampler, NULL);

  g_hash_table_destroy(r_vk_images.by_path);

  memset(&r_vk_images, 0, sizeof(r_vk_images));
}

#endif /* BUILD_VULKAN */
