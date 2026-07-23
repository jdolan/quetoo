/*
 * Copyright(c) 2026 QuetooPlus.
 * SPDX-License-Identifier: GPL-2.0-or-later
 */

#include "r_rtx_output.h"

#include <assert.h>
#include <string.h>

#if BUILD_RTX

static bool R_Rtx_AllocateImageMemory(const r_rtx_device_t *device, VkImage image,
                                      VkDeviceMemory *memory) {
  VkMemoryRequirements requirements;
  vkGetImageMemoryRequirements(device->device, image, &requirements);

  uint32_t type;
  if (!R_Rtx_FindMemoryType(device, requirements.memoryTypeBits,
                            VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT, &type)) {
    return false;
  }

  const VkMemoryAllocateInfo allocate = {
    .sType = VK_STRUCTURE_TYPE_MEMORY_ALLOCATE_INFO,
    .allocationSize = requirements.size,
    .memoryTypeIndex = type,
  };
  return vkAllocateMemory(device->device, &allocate, NULL, memory) == VK_SUCCESS &&
         vkBindImageMemory(device->device, image, *memory, 0) == VK_SUCCESS;
}

static bool R_Rtx_AllocateReadbackMemory(const r_rtx_device_t *device, VkBuffer buffer,
                                         VkDeviceMemory *memory, void **pixels) {
  VkMemoryRequirements requirements;
  vkGetBufferMemoryRequirements(device->device, buffer, &requirements);

  uint32_t type;
  if (!R_Rtx_FindMemoryType(device, requirements.memoryTypeBits,
                            VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT | VK_MEMORY_PROPERTY_HOST_COHERENT_BIT,
                            &type)) {
    return false;
  }

  const VkMemoryAllocateInfo allocate = {
    .sType = VK_STRUCTURE_TYPE_MEMORY_ALLOCATE_INFO,
    .allocationSize = requirements.size,
    .memoryTypeIndex = type,
  };
  return vkAllocateMemory(device->device, &allocate, NULL, memory) == VK_SUCCESS &&
         vkBindBufferMemory(device->device, buffer, *memory, 0) == VK_SUCCESS &&
         vkMapMemory(device->device, *memory, 0, requirements.size, 0, pixels) == VK_SUCCESS;
}

bool R_Rtx_OutputInit(const r_rtx_device_t *device, r_rtx_output_t *output, VkExtent2D extent) {
  assert(device);
  assert(output);
  assert(extent.width && extent.height);
  memset(output, 0, sizeof(*output));
  output->extent = extent;

  const VkImageCreateInfo image = {
    .sType = VK_STRUCTURE_TYPE_IMAGE_CREATE_INFO,
    .imageType = VK_IMAGE_TYPE_2D,
    .format = VK_FORMAT_R8G8B8A8_UNORM,
    .extent = { extent.width, extent.height, 1 },
    .mipLevels = 1,
    .arrayLayers = 1,
    .samples = VK_SAMPLE_COUNT_1_BIT,
    .tiling = VK_IMAGE_TILING_OPTIMAL,
    .usage = VK_IMAGE_USAGE_STORAGE_BIT | VK_IMAGE_USAGE_TRANSFER_SRC_BIT,
    .initialLayout = VK_IMAGE_LAYOUT_UNDEFINED,
  };
  if (vkCreateImage(device->device, &image, NULL, &output->image) != VK_SUCCESS ||
      !R_Rtx_AllocateImageMemory(device, output->image, &output->image_memory)) {
    R_Rtx_OutputShutdown(device, output);
    return false;
  }

  const VkImageViewCreateInfo image_view = {
    .sType = VK_STRUCTURE_TYPE_IMAGE_VIEW_CREATE_INFO,
    .image = output->image,
    .viewType = VK_IMAGE_VIEW_TYPE_2D,
    .format = VK_FORMAT_R8G8B8A8_UNORM,
    .subresourceRange = { VK_IMAGE_ASPECT_COLOR_BIT, 0, 1, 0, 1 },
  };
  if (vkCreateImageView(device->device, &image_view, NULL, &output->image_view) != VK_SUCCESS) {
    R_Rtx_OutputShutdown(device, output);
    return false;
  }

  const VkBufferCreateInfo readback = {
    .sType = VK_STRUCTURE_TYPE_BUFFER_CREATE_INFO,
    .size = (VkDeviceSize) extent.width * extent.height * 4,
    .usage = VK_BUFFER_USAGE_TRANSFER_DST_BIT,
    .sharingMode = VK_SHARING_MODE_EXCLUSIVE,
  };
  if (vkCreateBuffer(device->device, &readback, NULL, &output->readback) != VK_SUCCESS ||
      !R_Rtx_AllocateReadbackMemory(device, output->readback, &output->readback_memory, &output->pixels)) {
    R_Rtx_OutputShutdown(device, output);
    return false;
  }

  return true;
}

bool R_Rtx_OutputClear(const r_rtx_device_t *device, r_rtx_output_t *output, const float color[4]) {
  VkCommandBuffer command;
  const VkCommandBufferAllocateInfo allocate = {
    .sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_ALLOCATE_INFO,
    .commandPool = device->command_pool,
    .level = VK_COMMAND_BUFFER_LEVEL_PRIMARY,
    .commandBufferCount = 1,
  };
  if (vkAllocateCommandBuffers(device->device, &allocate, &command) != VK_SUCCESS ||
      vkBeginCommandBuffer(command, &(VkCommandBufferBeginInfo) {
        .sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_BEGIN_INFO,
        .flags = VK_COMMAND_BUFFER_USAGE_ONE_TIME_SUBMIT_BIT,
      }) != VK_SUCCESS) {
    return false;
  }

  const VkImageMemoryBarrier to_transfer = {
    .sType = VK_STRUCTURE_TYPE_IMAGE_MEMORY_BARRIER,
    .srcAccessMask = output->layout == VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL ? VK_ACCESS_TRANSFER_READ_BIT : 0,
    .dstAccessMask = VK_ACCESS_TRANSFER_WRITE_BIT,
    .oldLayout = output->layout,
    .newLayout = VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL,
    .image = output->image,
    .subresourceRange = { VK_IMAGE_ASPECT_COLOR_BIT, 0, 1, 0, 1 },
  };
  vkCmdPipelineBarrier(command,
                       output->layout == VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL ? VK_PIPELINE_STAGE_TRANSFER_BIT : VK_PIPELINE_STAGE_TOP_OF_PIPE_BIT,
                       VK_PIPELINE_STAGE_TRANSFER_BIT,
                       0, 0, NULL, 0, NULL, 1, &to_transfer);
  vkCmdClearColorImage(command, output->image, VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL,
                       &(VkClearColorValue) { .float32 = { color[0], color[1], color[2], color[3] } },
                       1, &to_transfer.subresourceRange);

  VkImageMemoryBarrier to_read = to_transfer;
  to_read.srcAccessMask = VK_ACCESS_TRANSFER_WRITE_BIT;
  to_read.dstAccessMask = VK_ACCESS_TRANSFER_READ_BIT;
  to_read.oldLayout = VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL;
  to_read.newLayout = VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL;
  vkCmdPipelineBarrier(command, VK_PIPELINE_STAGE_TRANSFER_BIT, VK_PIPELINE_STAGE_TRANSFER_BIT,
                       0, 0, NULL, 0, NULL, 1, &to_read);
  vkCmdCopyImageToBuffer(command, output->image, VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL,
                         output->readback, 1, &(VkBufferImageCopy) {
                           .imageSubresource = { VK_IMAGE_ASPECT_COLOR_BIT, 0, 0, 1 },
                           .imageExtent = { output->extent.width, output->extent.height, 1 },
                         });

  const bool succeeded = vkEndCommandBuffer(command) == VK_SUCCESS &&
      vkQueueSubmit(device->queue, 1, &(VkSubmitInfo) {
        .sType = VK_STRUCTURE_TYPE_SUBMIT_INFO, .commandBufferCount = 1, .pCommandBuffers = &command,
      }, VK_NULL_HANDLE) == VK_SUCCESS && vkQueueWaitIdle(device->queue) == VK_SUCCESS;
  vkFreeCommandBuffers(device->device, device->command_pool, 1, &command);
  if (succeeded) {
    output->layout = VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL;
  }
  return succeeded;
}

bool R_Rtx_OutputReadback(const r_rtx_device_t *device, r_rtx_output_t *output, VkCommandBuffer command) {
  const VkImageMemoryBarrier to_read = {
    .sType = VK_STRUCTURE_TYPE_IMAGE_MEMORY_BARRIER,
    .srcAccessMask = VK_ACCESS_SHADER_WRITE_BIT,
    .dstAccessMask = VK_ACCESS_TRANSFER_READ_BIT,
    .oldLayout = VK_IMAGE_LAYOUT_GENERAL,
    .newLayout = VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL,
    .image = output->image,
    .subresourceRange = { VK_IMAGE_ASPECT_COLOR_BIT, 0, 1, 0, 1 },
  };
  vkCmdPipelineBarrier(command, VK_PIPELINE_STAGE_RAY_TRACING_SHADER_BIT_KHR,
                       VK_PIPELINE_STAGE_TRANSFER_BIT, 0, 0, NULL, 0, NULL, 1, &to_read);
  vkCmdCopyImageToBuffer(command, output->image, VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL,
                         output->readback, 1, &(VkBufferImageCopy) {
                           .imageSubresource = { VK_IMAGE_ASPECT_COLOR_BIT, 0, 0, 1 },
                           .imageExtent = { output->extent.width, output->extent.height, 1 },
                         });
  output->layout = VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL;
  return true;
}

void R_Rtx_OutputShutdown(const r_rtx_device_t *device, r_rtx_output_t *output) {
  if (!output) {
    return;
  }
  if (output->readback_memory && output->pixels) {
    vkUnmapMemory(device->device, output->readback_memory);
  }
  if (output->readback) vkDestroyBuffer(device->device, output->readback, NULL);
  if (output->readback_memory) vkFreeMemory(device->device, output->readback_memory, NULL);
  if (output->image_view) vkDestroyImageView(device->device, output->image_view, NULL);
  if (output->image) vkDestroyImage(device->device, output->image, NULL);
  if (output->image_memory) vkFreeMemory(device->device, output->image_memory, NULL);
  memset(output, 0, sizeof(*output));
}

#endif
