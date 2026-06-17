// Phase 2 validation: offscreen Vulkan rasterization on real hardware.
// Builds a graphics pipeline from SPIR-V (compiled from tri.vert/tri.frag),
// draws a triangle into an offscreen image, copies it back to host memory, and
// verifies the center pixel is the triangle (non-clear). Proves the SPIR-V ->
// pipeline -> draw -> readback path that Quetoo's Vulkan backend (phase 2) needs.
// Build: cc raster.c -lvulkan -o raster
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdint.h>
#include <vulkan/vulkan.h>

#define W 64
#define H 64
#define CHECK(x) do { VkResult _r = (x); if (_r != VK_SUCCESS) { \
  printf("FAIL %s = %d\n", #x, _r); exit(1); } } while (0)

static VkInstance inst;
static VkPhysicalDevice phys;
static VkDevice dev;
static VkQueue queue;
static uint32_t qfam = (uint32_t) -1;

static uint32_t find_mem(uint32_t bits, VkMemoryPropertyFlags want) {
  VkPhysicalDeviceMemoryProperties mp;
  vkGetPhysicalDeviceMemoryProperties(phys, &mp);
  for (uint32_t i = 0; i < mp.memoryTypeCount; i++)
    if ((bits & (1u << i)) && (mp.memoryTypes[i].propertyFlags & want) == want)
      return i;
  printf("FAIL no memory type\n"); exit(1);
}

static VkShaderModule load_spv(const char *path) {
  FILE *f = fopen(path, "rb");
  if (!f) { printf("FAIL open %s\n", path); exit(1); }
  fseek(f, 0, SEEK_END); long n = ftell(f); fseek(f, 0, SEEK_SET);
  uint32_t *code = malloc(n);
  if (fread(code, 1, n, f) != (size_t) n) { printf("FAIL read %s\n", path); exit(1); }
  fclose(f);
  VkShaderModuleCreateInfo ci = { .sType = VK_STRUCTURE_TYPE_SHADER_MODULE_CREATE_INFO,
    .codeSize = n, .pCode = code };
  VkShaderModule m; CHECK(vkCreateShaderModule(dev, &ci, NULL, &m));
  free(code);
  return m;
}

int main(void) {
  VkApplicationInfo ai = { .sType = VK_STRUCTURE_TYPE_APPLICATION_INFO,
    .pApplicationName = "raster", .apiVersion = VK_API_VERSION_1_2 };
  VkInstanceCreateInfo ici = { .sType = VK_STRUCTURE_TYPE_INSTANCE_CREATE_INFO,
    .pApplicationInfo = &ai };
  CHECK(vkCreateInstance(&ici, NULL, &inst));

  uint32_t n = 1;
  vkEnumeratePhysicalDevices(inst, &n, &phys);
  // prefer a discrete GPU
  uint32_t cnt = 0; vkEnumeratePhysicalDevices(inst, &cnt, NULL);
  VkPhysicalDevice *devs = malloc(sizeof(*devs) * cnt);
  vkEnumeratePhysicalDevices(inst, &cnt, devs);
  for (uint32_t i = 0; i < cnt; i++) {
    VkPhysicalDeviceProperties p; vkGetPhysicalDeviceProperties(devs[i], &p);
    if (p.deviceType == VK_PHYSICAL_DEVICE_TYPE_DISCRETE_GPU) { phys = devs[i]; break; }
  }
  VkPhysicalDeviceProperties pp; vkGetPhysicalDeviceProperties(phys, &pp);
  printf("Device: %s\n", pp.deviceName);

  uint32_t qn = 0; vkGetPhysicalDeviceQueueFamilyProperties(phys, &qn, NULL);
  VkQueueFamilyProperties *qf = malloc(sizeof(*qf) * qn);
  vkGetPhysicalDeviceQueueFamilyProperties(phys, &qn, qf);
  for (uint32_t i = 0; i < qn; i++)
    if (qf[i].queueFlags & VK_QUEUE_GRAPHICS_BIT) { qfam = i; break; }

  float prio = 1.0f;
  VkDeviceQueueCreateInfo qci = { .sType = VK_STRUCTURE_TYPE_DEVICE_QUEUE_CREATE_INFO,
    .queueFamilyIndex = qfam, .queueCount = 1, .pQueuePriorities = &prio };
  VkDeviceCreateInfo dci = { .sType = VK_STRUCTURE_TYPE_DEVICE_CREATE_INFO,
    .queueCreateInfoCount = 1, .pQueueCreateInfos = &qci };
  CHECK(vkCreateDevice(phys, &dci, NULL, &dev));
  vkGetDeviceQueue(dev, qfam, 0, &queue);

  const VkFormat fmt = VK_FORMAT_R8G8B8A8_UNORM;

  // offscreen color image
  VkImageCreateInfo imgci = { .sType = VK_STRUCTURE_TYPE_IMAGE_CREATE_INFO,
    .imageType = VK_IMAGE_TYPE_2D, .format = fmt, .extent = { W, H, 1 },
    .mipLevels = 1, .arrayLayers = 1, .samples = VK_SAMPLE_COUNT_1_BIT,
    .tiling = VK_IMAGE_TILING_OPTIMAL,
    .usage = VK_IMAGE_USAGE_COLOR_ATTACHMENT_BIT | VK_IMAGE_USAGE_TRANSFER_SRC_BIT,
    .initialLayout = VK_IMAGE_LAYOUT_UNDEFINED };
  VkImage img; CHECK(vkCreateImage(dev, &imgci, NULL, &img));
  VkMemoryRequirements ireq; vkGetImageMemoryRequirements(dev, img, &ireq);
  VkMemoryAllocateInfo imai = { .sType = VK_STRUCTURE_TYPE_MEMORY_ALLOCATE_INFO,
    .allocationSize = ireq.size,
    .memoryTypeIndex = find_mem(ireq.memoryTypeBits, VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT) };
  VkDeviceMemory imem; CHECK(vkAllocateMemory(dev, &imai, NULL, &imem));
  CHECK(vkBindImageMemory(dev, img, imem, 0));

  VkImageViewCreateInfo ivci = { .sType = VK_STRUCTURE_TYPE_IMAGE_VIEW_CREATE_INFO,
    .image = img, .viewType = VK_IMAGE_VIEW_TYPE_2D, .format = fmt,
    .subresourceRange = { VK_IMAGE_ASPECT_COLOR_BIT, 0, 1, 0, 1 } };
  VkImageView view; CHECK(vkCreateImageView(dev, &ivci, NULL, &view));

  // render pass: clear -> store -> TRANSFER_SRC for readback
  VkAttachmentDescription att = { .format = fmt, .samples = VK_SAMPLE_COUNT_1_BIT,
    .loadOp = VK_ATTACHMENT_LOAD_OP_CLEAR, .storeOp = VK_ATTACHMENT_STORE_OP_STORE,
    .stencilLoadOp = VK_ATTACHMENT_LOAD_OP_DONT_CARE, .stencilStoreOp = VK_ATTACHMENT_STORE_OP_DONT_CARE,
    .initialLayout = VK_IMAGE_LAYOUT_UNDEFINED, .finalLayout = VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL };
  VkAttachmentReference ref = { 0, VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL };
  VkSubpassDescription sub = { .pipelineBindPoint = VK_PIPELINE_BIND_POINT_GRAPHICS,
    .colorAttachmentCount = 1, .pColorAttachments = &ref };
  VkRenderPassCreateInfo rpci = { .sType = VK_STRUCTURE_TYPE_RENDER_PASS_CREATE_INFO,
    .attachmentCount = 1, .pAttachments = &att, .subpassCount = 1, .pSubpasses = &sub };
  VkRenderPass rp; CHECK(vkCreateRenderPass(dev, &rpci, NULL, &rp));

  VkFramebufferCreateInfo fbci = { .sType = VK_STRUCTURE_TYPE_FRAMEBUFFER_CREATE_INFO,
    .renderPass = rp, .attachmentCount = 1, .pAttachments = &view,
    .width = W, .height = H, .layers = 1 };
  VkFramebuffer fb; CHECK(vkCreateFramebuffer(dev, &fbci, NULL, &fb));

  // pipeline
  VkShaderModule vs = load_spv("tri.vert.spv");
  VkShaderModule fs = load_spv("tri.frag.spv");
  VkPipelineShaderStageCreateInfo stages[2] = {
    { .sType = VK_STRUCTURE_TYPE_PIPELINE_SHADER_STAGE_CREATE_INFO,
      .stage = VK_SHADER_STAGE_VERTEX_BIT, .module = vs, .pName = "main" },
    { .sType = VK_STRUCTURE_TYPE_PIPELINE_SHADER_STAGE_CREATE_INFO,
      .stage = VK_SHADER_STAGE_FRAGMENT_BIT, .module = fs, .pName = "main" } };
  VkPipelineVertexInputStateCreateInfo vi = { .sType = VK_STRUCTURE_TYPE_PIPELINE_VERTEX_INPUT_STATE_CREATE_INFO };
  VkPipelineInputAssemblyStateCreateInfo ia = { .sType = VK_STRUCTURE_TYPE_PIPELINE_INPUT_ASSEMBLY_STATE_CREATE_INFO,
    .topology = VK_PRIMITIVE_TOPOLOGY_TRIANGLE_LIST };
  VkViewport vp = { 0, 0, W, H, 0, 1 };
  VkRect2D sc = { { 0, 0 }, { W, H } };
  VkPipelineViewportStateCreateInfo vps = { .sType = VK_STRUCTURE_TYPE_PIPELINE_VIEWPORT_STATE_CREATE_INFO,
    .viewportCount = 1, .pViewports = &vp, .scissorCount = 1, .pScissors = &sc };
  VkPipelineRasterizationStateCreateInfo rs = { .sType = VK_STRUCTURE_TYPE_PIPELINE_RASTERIZATION_STATE_CREATE_INFO,
    .polygonMode = VK_POLYGON_MODE_FILL, .cullMode = VK_CULL_MODE_NONE,
    .frontFace = VK_FRONT_FACE_CLOCKWISE, .lineWidth = 1.0f };
  VkPipelineMultisampleStateCreateInfo ms = { .sType = VK_STRUCTURE_TYPE_PIPELINE_MULTISAMPLE_STATE_CREATE_INFO,
    .rasterizationSamples = VK_SAMPLE_COUNT_1_BIT };
  VkPipelineColorBlendAttachmentState cba = { .colorWriteMask = 0xf };
  VkPipelineColorBlendStateCreateInfo cb = { .sType = VK_STRUCTURE_TYPE_PIPELINE_COLOR_BLEND_STATE_CREATE_INFO,
    .attachmentCount = 1, .pAttachments = &cba };
  VkPipelineLayoutCreateInfo plci = { .sType = VK_STRUCTURE_TYPE_PIPELINE_LAYOUT_CREATE_INFO };
  VkPipelineLayout pl; CHECK(vkCreatePipelineLayout(dev, &plci, NULL, &pl));
  VkGraphicsPipelineCreateInfo gpci = { .sType = VK_STRUCTURE_TYPE_GRAPHICS_PIPELINE_CREATE_INFO,
    .stageCount = 2, .pStages = stages, .pVertexInputState = &vi, .pInputAssemblyState = &ia,
    .pViewportState = &vps, .pRasterizationState = &rs, .pMultisampleState = &ms,
    .pColorBlendState = &cb, .layout = pl, .renderPass = rp, .subpass = 0 };
  VkPipeline pipe; CHECK(vkCreateGraphicsPipelines(dev, VK_NULL_HANDLE, 1, &gpci, NULL, &pipe));

  // readback buffer
  VkBufferCreateInfo bci = { .sType = VK_STRUCTURE_TYPE_BUFFER_CREATE_INFO,
    .size = W * H * 4, .usage = VK_BUFFER_USAGE_TRANSFER_DST_BIT,
    .sharingMode = VK_SHARING_MODE_EXCLUSIVE };
  VkBuffer buf; CHECK(vkCreateBuffer(dev, &bci, NULL, &buf));
  VkMemoryRequirements breq; vkGetBufferMemoryRequirements(dev, buf, &breq);
  VkMemoryAllocateInfo bmai = { .sType = VK_STRUCTURE_TYPE_MEMORY_ALLOCATE_INFO,
    .allocationSize = breq.size, .memoryTypeIndex = find_mem(breq.memoryTypeBits,
      VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT | VK_MEMORY_PROPERTY_HOST_COHERENT_BIT) };
  VkDeviceMemory bmem; CHECK(vkAllocateMemory(dev, &bmai, NULL, &bmem));
  CHECK(vkBindBufferMemory(dev, buf, bmem, 0));

  // record
  VkCommandPoolCreateInfo cpci = { .sType = VK_STRUCTURE_TYPE_COMMAND_POOL_CREATE_INFO,
    .queueFamilyIndex = qfam };
  VkCommandPool pool; CHECK(vkCreateCommandPool(dev, &cpci, NULL, &pool));
  VkCommandBufferAllocateInfo cbai = { .sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_ALLOCATE_INFO,
    .commandPool = pool, .level = VK_COMMAND_BUFFER_LEVEL_PRIMARY, .commandBufferCount = 1 };
  VkCommandBuffer cmd; CHECK(vkAllocateCommandBuffers(dev, &cbai, &cmd));
  VkCommandBufferBeginInfo cbbi = { .sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_BEGIN_INFO };
  CHECK(vkBeginCommandBuffer(cmd, &cbbi));
  VkClearValue clear = { .color = { .float32 = { 0.05f, 0.05f, 0.05f, 1.0f } } };
  VkRenderPassBeginInfo rpbi = { .sType = VK_STRUCTURE_TYPE_RENDER_PASS_BEGIN_INFO,
    .renderPass = rp, .framebuffer = fb, .renderArea = { { 0, 0 }, { W, H } },
    .clearValueCount = 1, .pClearValues = &clear };
  vkCmdBeginRenderPass(cmd, &rpbi, VK_SUBPASS_CONTENTS_INLINE);
  vkCmdBindPipeline(cmd, VK_PIPELINE_BIND_POINT_GRAPHICS, pipe);
  vkCmdDraw(cmd, 3, 1, 0, 0);
  vkCmdEndRenderPass(cmd);
  VkBufferImageCopy copy = { .imageSubresource = { VK_IMAGE_ASPECT_COLOR_BIT, 0, 0, 1 },
    .imageExtent = { W, H, 1 } };
  vkCmdCopyImageToBuffer(cmd, img, VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL, buf, 1, &copy);
  CHECK(vkEndCommandBuffer(cmd));

  VkSubmitInfo si = { .sType = VK_STRUCTURE_TYPE_SUBMIT_INFO,
    .commandBufferCount = 1, .pCommandBuffers = &cmd };
  CHECK(vkQueueSubmit(queue, 1, &si, VK_NULL_HANDLE));
  CHECK(vkQueueWaitIdle(queue));

  // verify
  uint8_t *px; CHECK(vkMapMemory(dev, bmem, 0, W * H * 4, 0, (void **) &px));
  uint8_t *center = px + (H / 2 * W + W / 2) * 4;
  uint8_t *corner = px; // (0,0) should be the clear color
  printf("center pixel rgba = %u %u %u %u\n", center[0], center[1], center[2], center[3]);
  printf("corner pixel rgba = %u %u %u %u\n", corner[0], corner[1], corner[2], corner[3]);
  int triangle_drawn = (center[0] + center[1] + center[2]) > 60; // brighter than clear
  int corner_is_clear = corner[0] < 40 && corner[1] < 40 && corner[2] < 40;
  vkUnmapMemory(dev, bmem);

  printf("%s: triangle rendered=%d, background clear=%d\n",
    (triangle_drawn && corner_is_clear) ? "PASS" : "FAIL", triangle_drawn, corner_is_clear);
  return (triangle_drawn && corner_is_clear) ? 0 : 1;
}
