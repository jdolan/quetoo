// Phase 4 core validation: hardware ray tracing on real RTX hardware.
// Builds a BLAS (one triangle) + TLAS, a ray-tracing pipeline (raygen/miss/
// closest-hit) and shader binding table, then vkCmdTraceRaysKHR into a storage
// image and reads it back. A center ray must HIT the triangle (closest-hit color)
// and a corner ray must MISS (background). This is the RTX core that Quetoo's
// phase 4 needs. Build: cc rt.c -lvulkan -o rt
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

// ray tracing entry points (loaded via vkGetDeviceProcAddr)
static PFN_vkGetBufferDeviceAddressKHR pGetBufferDeviceAddress;
static PFN_vkCreateAccelerationStructureKHR pCreateAS;
static PFN_vkDestroyAccelerationStructureKHR pDestroyAS;
static PFN_vkGetAccelerationStructureBuildSizesKHR pGetASBuildSizes;
static PFN_vkCmdBuildAccelerationStructuresKHR pCmdBuildAS;
static PFN_vkGetAccelerationStructureDeviceAddressKHR pGetASDeviceAddress;
static PFN_vkCreateRayTracingPipelinesKHR pCreateRTPipelines;
static PFN_vkGetRayTracingShaderGroupHandlesKHR pGetRTGroupHandles;
static PFN_vkCmdTraceRaysKHR pCmdTraceRays;

static uint32_t find_mem(uint32_t bits, VkMemoryPropertyFlags want) {
  VkPhysicalDeviceMemoryProperties mp;
  vkGetPhysicalDeviceMemoryProperties(phys, &mp);
  for (uint32_t i = 0; i < mp.memoryTypeCount; i++)
    if ((bits & (1u << i)) && (mp.memoryTypes[i].propertyFlags & want) == want)
      return i;
  printf("FAIL no memory type\n"); exit(1);
}

typedef struct { VkBuffer buf; VkDeviceMemory mem; VkDeviceAddress addr; } buffer_t;

static buffer_t make_buffer(VkDeviceSize size, VkBufferUsageFlags usage,
                            VkMemoryPropertyFlags props, const void *data) {
  buffer_t b = { 0 };
  VkBufferCreateInfo ci = { .sType = VK_STRUCTURE_TYPE_BUFFER_CREATE_INFO,
    .size = size, .usage = usage, .sharingMode = VK_SHARING_MODE_EXCLUSIVE };
  CHECK(vkCreateBuffer(dev, &ci, NULL, &b.buf));
  VkMemoryRequirements req; vkGetBufferMemoryRequirements(dev, b.buf, &req);
  VkMemoryAllocateFlagsInfo fi = { .sType = VK_STRUCTURE_TYPE_MEMORY_ALLOCATE_FLAGS_INFO,
    .flags = VK_MEMORY_ALLOCATE_DEVICE_ADDRESS_BIT };
  VkMemoryAllocateInfo ai = { .sType = VK_STRUCTURE_TYPE_MEMORY_ALLOCATE_INFO,
    .pNext = &fi, .allocationSize = req.size,
    .memoryTypeIndex = find_mem(req.memoryTypeBits, props) };
  CHECK(vkAllocateMemory(dev, &ai, NULL, &b.mem));
  CHECK(vkBindBufferMemory(dev, b.buf, b.mem, 0));
  if (data) {
    void *p; CHECK(vkMapMemory(dev, b.mem, 0, size, 0, &p));
    memcpy(p, data, size); vkUnmapMemory(dev, b.mem);
  }
  if (usage & VK_BUFFER_USAGE_SHADER_DEVICE_ADDRESS_BIT) {
    VkBufferDeviceAddressInfo bi = { .sType = VK_STRUCTURE_TYPE_BUFFER_DEVICE_ADDRESS_INFO,
      .buffer = b.buf };
    b.addr = pGetBufferDeviceAddress(dev, &bi);
  }
  return b;
}

static VkCommandPool pool;
static VkCommandBuffer begin_cmd(void) {
  VkCommandBufferAllocateInfo ai = { .sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_ALLOCATE_INFO,
    .commandPool = pool, .level = VK_COMMAND_BUFFER_LEVEL_PRIMARY, .commandBufferCount = 1 };
  VkCommandBuffer c; CHECK(vkAllocateCommandBuffers(dev, &ai, &c));
  VkCommandBufferBeginInfo bi = { .sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_BEGIN_INFO };
  CHECK(vkBeginCommandBuffer(c, &bi));
  return c;
}
static void end_cmd(VkCommandBuffer c) {
  CHECK(vkEndCommandBuffer(c));
  VkSubmitInfo si = { .sType = VK_STRUCTURE_TYPE_SUBMIT_INFO,
    .commandBufferCount = 1, .pCommandBuffers = &c };
  CHECK(vkQueueSubmit(queue, 1, &si, VK_NULL_HANDLE));
  CHECK(vkQueueWaitIdle(queue));
}

static VkShaderModule load_spv(const char *path) {
  FILE *f = fopen(path, "rb"); if (!f) { printf("FAIL open %s\n", path); exit(1); }
  fseek(f, 0, SEEK_END); long n = ftell(f); fseek(f, 0, SEEK_SET);
  uint32_t *code = malloc(n);
  if (fread(code, 1, n, f) != (size_t) n) { printf("FAIL read\n"); exit(1); }
  fclose(f);
  VkShaderModuleCreateInfo ci = { .sType = VK_STRUCTURE_TYPE_SHADER_MODULE_CREATE_INFO,
    .codeSize = n, .pCode = code };
  VkShaderModule m; CHECK(vkCreateShaderModule(dev, &ci, NULL, &m)); free(code);
  return m;
}

static uint32_t aligned(uint32_t v, uint32_t a) { return (v + a - 1) & ~(a - 1); }

int main(void) {
  VkApplicationInfo app = { .sType = VK_STRUCTURE_TYPE_APPLICATION_INFO,
    .pApplicationName = "rt", .apiVersion = VK_API_VERSION_1_2 };
  VkInstanceCreateInfo ici = { .sType = VK_STRUCTURE_TYPE_INSTANCE_CREATE_INFO,
    .pApplicationInfo = &app };
  CHECK(vkCreateInstance(&ici, NULL, &inst));

  uint32_t cnt = 0; vkEnumeratePhysicalDevices(inst, &cnt, NULL);
  VkPhysicalDevice *devs = malloc(sizeof(*devs) * cnt);
  vkEnumeratePhysicalDevices(inst, &cnt, devs);
  phys = devs[0];
  for (uint32_t i = 0; i < cnt; i++) {
    VkPhysicalDeviceProperties p; vkGetPhysicalDeviceProperties(devs[i], &p);
    if (p.deviceType == VK_PHYSICAL_DEVICE_TYPE_DISCRETE_GPU) { phys = devs[i]; break; }
  }
  VkPhysicalDeviceProperties pp; vkGetPhysicalDeviceProperties(phys, &pp);
  printf("Device: %s\n", pp.deviceName);

  // ray tracing pipeline properties (SBT alignment)
  VkPhysicalDeviceRayTracingPipelinePropertiesKHR rtprops = {
    .sType = VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_RAY_TRACING_PIPELINE_PROPERTIES_KHR };
  VkPhysicalDeviceProperties2 props2 = {
    .sType = VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_PROPERTIES_2, .pNext = &rtprops };
  vkGetPhysicalDeviceProperties2(phys, &props2);

  uint32_t qn = 0; vkGetPhysicalDeviceQueueFamilyProperties(phys, &qn, NULL);
  VkQueueFamilyProperties *qf = malloc(sizeof(*qf) * qn);
  vkGetPhysicalDeviceQueueFamilyProperties(phys, &qn, qf);
  for (uint32_t i = 0; i < qn; i++)
    if (qf[i].queueFlags & VK_QUEUE_GRAPHICS_BIT) { qfam = i; break; }

  // device with RT feature chain
  VkPhysicalDeviceBufferDeviceAddressFeatures bda = {
    .sType = VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_BUFFER_DEVICE_ADDRESS_FEATURES,
    .bufferDeviceAddress = VK_TRUE };
  VkPhysicalDeviceAccelerationStructureFeaturesKHR asf = {
    .sType = VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_ACCELERATION_STRUCTURE_FEATURES_KHR,
    .accelerationStructure = VK_TRUE, .pNext = &bda };
  VkPhysicalDeviceRayTracingPipelineFeaturesKHR rtf = {
    .sType = VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_RAY_TRACING_PIPELINE_FEATURES_KHR,
    .rayTracingPipeline = VK_TRUE, .pNext = &asf };

  const char *exts[] = {
    VK_KHR_ACCELERATION_STRUCTURE_EXTENSION_NAME,
    VK_KHR_RAY_TRACING_PIPELINE_EXTENSION_NAME,
    VK_KHR_DEFERRED_HOST_OPERATIONS_EXTENSION_NAME,
    VK_KHR_BUFFER_DEVICE_ADDRESS_EXTENSION_NAME };
  float prio = 1.0f;
  VkDeviceQueueCreateInfo qci = { .sType = VK_STRUCTURE_TYPE_DEVICE_QUEUE_CREATE_INFO,
    .queueFamilyIndex = qfam, .queueCount = 1, .pQueuePriorities = &prio };
  VkDeviceCreateInfo dci = { .sType = VK_STRUCTURE_TYPE_DEVICE_CREATE_INFO, .pNext = &rtf,
    .queueCreateInfoCount = 1, .pQueueCreateInfos = &qci,
    .enabledExtensionCount = 4, .ppEnabledExtensionNames = exts };
  CHECK(vkCreateDevice(phys, &dci, NULL, &dev));
  vkGetDeviceQueue(dev, qfam, 0, &queue);

  #define LOAD(p, n) p = (PFN_##n) vkGetDeviceProcAddr(dev, #n); \
    if (!p) { printf("FAIL load %s\n", #n); exit(1); }
  LOAD(pGetBufferDeviceAddress, vkGetBufferDeviceAddressKHR);
  LOAD(pCreateAS, vkCreateAccelerationStructureKHR);
  LOAD(pDestroyAS, vkDestroyAccelerationStructureKHR);
  LOAD(pGetASBuildSizes, vkGetAccelerationStructureBuildSizesKHR);
  LOAD(pCmdBuildAS, vkCmdBuildAccelerationStructuresKHR);
  LOAD(pGetASDeviceAddress, vkGetAccelerationStructureDeviceAddressKHR);
  LOAD(pCreateRTPipelines, vkCreateRayTracingPipelinesKHR);
  LOAD(pGetRTGroupHandles, vkGetRayTracingShaderGroupHandlesKHR);
  LOAD(pCmdTraceRays, vkCmdTraceRaysKHR);

  VkCommandPoolCreateInfo cpci = { .sType = VK_STRUCTURE_TYPE_COMMAND_POOL_CREATE_INFO,
    .flags = VK_COMMAND_POOL_CREATE_RESET_COMMAND_BUFFER_BIT, .queueFamilyIndex = qfam };
  CHECK(vkCreateCommandPool(dev, &cpci, NULL, &pool));

  const VkMemoryPropertyFlags HOST = VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT | VK_MEMORY_PROPERTY_HOST_COHERENT_BIT;
  const VkBufferUsageFlags ASIN = VK_BUFFER_USAGE_ACCELERATION_STRUCTURE_BUILD_INPUT_READ_ONLY_BIT_KHR | VK_BUFFER_USAGE_SHADER_DEVICE_ADDRESS_BIT;

  // ---- BLAS: one triangle on the z=0 plane ----
  float verts[9] = { 0.0f, 0.6f, 0.0f,  0.6f, -0.6f, 0.0f,  -0.6f, -0.6f, 0.0f };
  uint32_t idx[3] = { 0, 1, 2 };
  buffer_t vbo = make_buffer(sizeof(verts), ASIN, HOST, verts);
  buffer_t ibo = make_buffer(sizeof(idx), ASIN, HOST, idx);

  VkAccelerationStructureGeometryKHR geo = {
    .sType = VK_STRUCTURE_TYPE_ACCELERATION_STRUCTURE_GEOMETRY_KHR,
    .geometryType = VK_GEOMETRY_TYPE_TRIANGLES_KHR, .flags = VK_GEOMETRY_OPAQUE_BIT_KHR,
    .geometry.triangles = {
      .sType = VK_STRUCTURE_TYPE_ACCELERATION_STRUCTURE_GEOMETRY_TRIANGLES_DATA_KHR,
      .vertexFormat = VK_FORMAT_R32G32B32_SFLOAT, .vertexData.deviceAddress = vbo.addr,
      .vertexStride = 12, .maxVertex = 2, .indexType = VK_INDEX_TYPE_UINT32,
      .indexData.deviceAddress = ibo.addr } };
  VkAccelerationStructureBuildGeometryInfoKHR bgi = {
    .sType = VK_STRUCTURE_TYPE_ACCELERATION_STRUCTURE_BUILD_GEOMETRY_INFO_KHR,
    .type = VK_ACCELERATION_STRUCTURE_TYPE_BOTTOM_LEVEL_KHR,
    .flags = VK_BUILD_ACCELERATION_STRUCTURE_PREFER_FAST_TRACE_BIT_KHR,
    .mode = VK_BUILD_ACCELERATION_STRUCTURE_MODE_BUILD_KHR,
    .geometryCount = 1, .pGeometries = &geo };
  uint32_t prim = 1;
  VkAccelerationStructureBuildSizesInfoKHR sz = {
    .sType = VK_STRUCTURE_TYPE_ACCELERATION_STRUCTURE_BUILD_SIZES_INFO_KHR };
  pGetASBuildSizes(dev, VK_ACCELERATION_STRUCTURE_BUILD_TYPE_DEVICE_KHR, &bgi, &prim, &sz);

  buffer_t blas_buf = make_buffer(sz.accelerationStructureSize,
    VK_BUFFER_USAGE_ACCELERATION_STRUCTURE_STORAGE_BIT_KHR | VK_BUFFER_USAGE_SHADER_DEVICE_ADDRESS_BIT,
    VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT, NULL);
  VkAccelerationStructureKHR blas;
  VkAccelerationStructureCreateInfoKHR asci = {
    .sType = VK_STRUCTURE_TYPE_ACCELERATION_STRUCTURE_CREATE_INFO_KHR,
    .buffer = blas_buf.buf, .size = sz.accelerationStructureSize,
    .type = VK_ACCELERATION_STRUCTURE_TYPE_BOTTOM_LEVEL_KHR };
  CHECK(pCreateAS(dev, &asci, NULL, &blas));
  buffer_t scratch = make_buffer(sz.buildScratchSize,
    VK_BUFFER_USAGE_STORAGE_BUFFER_BIT | VK_BUFFER_USAGE_SHADER_DEVICE_ADDRESS_BIT,
    VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT, NULL);
  bgi.dstAccelerationStructure = blas;
  bgi.scratchData.deviceAddress = scratch.addr;
  VkAccelerationStructureBuildRangeInfoKHR range = { .primitiveCount = 1 };
  const VkAccelerationStructureBuildRangeInfoKHR *pr = &range;
  VkCommandBuffer c = begin_cmd();
  pCmdBuildAS(c, 1, &bgi, &pr);
  end_cmd(c);
  VkAccelerationStructureDeviceAddressInfoKHR dai = {
    .sType = VK_STRUCTURE_TYPE_ACCELERATION_STRUCTURE_DEVICE_ADDRESS_INFO_KHR,
    .accelerationStructure = blas };
  VkDeviceAddress blas_addr = pGetASDeviceAddress(dev, &dai);

  // ---- TLAS: one instance of the BLAS ----
  VkAccelerationStructureInstanceKHR instd = {
    .transform = { .matrix = { {1,0,0,0}, {0,1,0,0}, {0,0,1,0} } },
    .mask = 0xFF, .accelerationStructureReference = blas_addr };
  buffer_t inst_buf = make_buffer(sizeof(instd), ASIN, HOST, &instd);

  VkAccelerationStructureGeometryKHR tgeo = {
    .sType = VK_STRUCTURE_TYPE_ACCELERATION_STRUCTURE_GEOMETRY_KHR,
    .geometryType = VK_GEOMETRY_TYPE_INSTANCES_KHR, .flags = VK_GEOMETRY_OPAQUE_BIT_KHR,
    .geometry.instances = {
      .sType = VK_STRUCTURE_TYPE_ACCELERATION_STRUCTURE_GEOMETRY_INSTANCES_DATA_KHR,
      .data.deviceAddress = inst_buf.addr } };
  VkAccelerationStructureBuildGeometryInfoKHR tbgi = {
    .sType = VK_STRUCTURE_TYPE_ACCELERATION_STRUCTURE_BUILD_GEOMETRY_INFO_KHR,
    .type = VK_ACCELERATION_STRUCTURE_TYPE_TOP_LEVEL_KHR,
    .flags = VK_BUILD_ACCELERATION_STRUCTURE_PREFER_FAST_TRACE_BIT_KHR,
    .mode = VK_BUILD_ACCELERATION_STRUCTURE_MODE_BUILD_KHR,
    .geometryCount = 1, .pGeometries = &tgeo };
  VkAccelerationStructureBuildSizesInfoKHR tsz = {
    .sType = VK_STRUCTURE_TYPE_ACCELERATION_STRUCTURE_BUILD_SIZES_INFO_KHR };
  pGetASBuildSizes(dev, VK_ACCELERATION_STRUCTURE_BUILD_TYPE_DEVICE_KHR, &tbgi, &prim, &tsz);
  buffer_t tlas_buf = make_buffer(tsz.accelerationStructureSize,
    VK_BUFFER_USAGE_ACCELERATION_STRUCTURE_STORAGE_BIT_KHR | VK_BUFFER_USAGE_SHADER_DEVICE_ADDRESS_BIT,
    VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT, NULL);
  VkAccelerationStructureKHR tlas;
  VkAccelerationStructureCreateInfoKHR tasci = {
    .sType = VK_STRUCTURE_TYPE_ACCELERATION_STRUCTURE_CREATE_INFO_KHR,
    .buffer = tlas_buf.buf, .size = tsz.accelerationStructureSize,
    .type = VK_ACCELERATION_STRUCTURE_TYPE_TOP_LEVEL_KHR };
  CHECK(pCreateAS(dev, &tasci, NULL, &tlas));
  buffer_t tscratch = make_buffer(tsz.buildScratchSize,
    VK_BUFFER_USAGE_STORAGE_BUFFER_BIT | VK_BUFFER_USAGE_SHADER_DEVICE_ADDRESS_BIT,
    VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT, NULL);
  tbgi.dstAccelerationStructure = tlas;
  tbgi.scratchData.deviceAddress = tscratch.addr;
  VkAccelerationStructureBuildRangeInfoKHR trange = { .primitiveCount = 1 };
  const VkAccelerationStructureBuildRangeInfoKHR *tpr = &trange;
  c = begin_cmd(); pCmdBuildAS(c, 1, &tbgi, &tpr); end_cmd(c);

  // ---- storage image ----
  VkImageCreateInfo imgci = { .sType = VK_STRUCTURE_TYPE_IMAGE_CREATE_INFO,
    .imageType = VK_IMAGE_TYPE_2D, .format = VK_FORMAT_R8G8B8A8_UNORM, .extent = { W, H, 1 },
    .mipLevels = 1, .arrayLayers = 1, .samples = VK_SAMPLE_COUNT_1_BIT,
    .tiling = VK_IMAGE_TILING_OPTIMAL,
    .usage = VK_IMAGE_USAGE_STORAGE_BIT | VK_IMAGE_USAGE_TRANSFER_SRC_BIT,
    .initialLayout = VK_IMAGE_LAYOUT_UNDEFINED };
  VkImage img; CHECK(vkCreateImage(dev, &imgci, NULL, &img));
  VkMemoryRequirements ireq; vkGetImageMemoryRequirements(dev, img, &ireq);
  VkMemoryAllocateInfo imai = { .sType = VK_STRUCTURE_TYPE_MEMORY_ALLOCATE_INFO,
    .allocationSize = ireq.size,
    .memoryTypeIndex = find_mem(ireq.memoryTypeBits, VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT) };
  VkDeviceMemory imem; CHECK(vkAllocateMemory(dev, &imai, NULL, &imem));
  CHECK(vkBindImageMemory(dev, img, imem, 0));
  VkImageViewCreateInfo ivci = { .sType = VK_STRUCTURE_TYPE_IMAGE_VIEW_CREATE_INFO,
    .image = img, .viewType = VK_IMAGE_VIEW_TYPE_2D, .format = VK_FORMAT_R8G8B8A8_UNORM,
    .subresourceRange = { VK_IMAGE_ASPECT_COLOR_BIT, 0, 1, 0, 1 } };
  VkImageView view; CHECK(vkCreateImageView(dev, &ivci, NULL, &view));

  // ---- descriptor set: TLAS + storage image ----
  VkDescriptorSetLayoutBinding binds[2] = {
    { 0, VK_DESCRIPTOR_TYPE_ACCELERATION_STRUCTURE_KHR, 1, VK_SHADER_STAGE_RAYGEN_BIT_KHR },
    { 1, VK_DESCRIPTOR_TYPE_STORAGE_IMAGE, 1, VK_SHADER_STAGE_RAYGEN_BIT_KHR } };
  VkDescriptorSetLayoutCreateInfo dslci = {
    .sType = VK_STRUCTURE_TYPE_DESCRIPTOR_SET_LAYOUT_CREATE_INFO,
    .bindingCount = 2, .pBindings = binds };
  VkDescriptorSetLayout dsl; CHECK(vkCreateDescriptorSetLayout(dev, &dslci, NULL, &dsl));
  VkDescriptorPoolSize psz[2] = {
    { VK_DESCRIPTOR_TYPE_ACCELERATION_STRUCTURE_KHR, 1 },
    { VK_DESCRIPTOR_TYPE_STORAGE_IMAGE, 1 } };
  VkDescriptorPoolCreateInfo dpci = { .sType = VK_STRUCTURE_TYPE_DESCRIPTOR_POOL_CREATE_INFO,
    .maxSets = 1, .poolSizeCount = 2, .pPoolSizes = psz };
  VkDescriptorPool dp; CHECK(vkCreateDescriptorPool(dev, &dpci, NULL, &dp));
  VkDescriptorSetAllocateInfo dsai = { .sType = VK_STRUCTURE_TYPE_DESCRIPTOR_SET_ALLOCATE_INFO,
    .descriptorPool = dp, .descriptorSetCount = 1, .pSetLayouts = &dsl };
  VkDescriptorSet ds; CHECK(vkAllocateDescriptorSets(dev, &dsai, &ds));
  VkWriteDescriptorSetAccelerationStructureKHR was = {
    .sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET_ACCELERATION_STRUCTURE_KHR,
    .accelerationStructureCount = 1, .pAccelerationStructures = &tlas };
  VkDescriptorImageInfo dii = { .imageView = view, .imageLayout = VK_IMAGE_LAYOUT_GENERAL };
  VkWriteDescriptorSet writes[2] = {
    { .sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET, .pNext = &was, .dstSet = ds,
      .dstBinding = 0, .descriptorCount = 1,
      .descriptorType = VK_DESCRIPTOR_TYPE_ACCELERATION_STRUCTURE_KHR },
    { .sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET, .dstSet = ds, .dstBinding = 1,
      .descriptorCount = 1, .descriptorType = VK_DESCRIPTOR_TYPE_STORAGE_IMAGE, .pImageInfo = &dii } };
  vkUpdateDescriptorSets(dev, 2, writes, 0, NULL);

  // ---- ray tracing pipeline ----
  VkShaderModule rgen = load_spv("rt.rgen.spv");
  VkShaderModule rmiss = load_spv("rt.rmiss.spv");
  VkShaderModule rchit = load_spv("rt.rchit.spv");
  VkPipelineShaderStageCreateInfo stages[3] = {
    { .sType = VK_STRUCTURE_TYPE_PIPELINE_SHADER_STAGE_CREATE_INFO,
      .stage = VK_SHADER_STAGE_RAYGEN_BIT_KHR, .module = rgen, .pName = "main" },
    { .sType = VK_STRUCTURE_TYPE_PIPELINE_SHADER_STAGE_CREATE_INFO,
      .stage = VK_SHADER_STAGE_MISS_BIT_KHR, .module = rmiss, .pName = "main" },
    { .sType = VK_STRUCTURE_TYPE_PIPELINE_SHADER_STAGE_CREATE_INFO,
      .stage = VK_SHADER_STAGE_CLOSEST_HIT_BIT_KHR, .module = rchit, .pName = "main" } };
  VkRayTracingShaderGroupCreateInfoKHR groups[3] = {
    { .sType = VK_STRUCTURE_TYPE_RAY_TRACING_SHADER_GROUP_CREATE_INFO_KHR,
      .type = VK_RAY_TRACING_SHADER_GROUP_TYPE_GENERAL_KHR, .generalShader = 0,
      .closestHitShader = VK_SHADER_UNUSED_KHR, .anyHitShader = VK_SHADER_UNUSED_KHR,
      .intersectionShader = VK_SHADER_UNUSED_KHR },
    { .sType = VK_STRUCTURE_TYPE_RAY_TRACING_SHADER_GROUP_CREATE_INFO_KHR,
      .type = VK_RAY_TRACING_SHADER_GROUP_TYPE_GENERAL_KHR, .generalShader = 1,
      .closestHitShader = VK_SHADER_UNUSED_KHR, .anyHitShader = VK_SHADER_UNUSED_KHR,
      .intersectionShader = VK_SHADER_UNUSED_KHR },
    { .sType = VK_STRUCTURE_TYPE_RAY_TRACING_SHADER_GROUP_CREATE_INFO_KHR,
      .type = VK_RAY_TRACING_SHADER_GROUP_TYPE_TRIANGLES_HIT_GROUP_KHR,
      .generalShader = VK_SHADER_UNUSED_KHR, .closestHitShader = 2,
      .anyHitShader = VK_SHADER_UNUSED_KHR, .intersectionShader = VK_SHADER_UNUSED_KHR } };
  VkPipelineLayoutCreateInfo plci = { .sType = VK_STRUCTURE_TYPE_PIPELINE_LAYOUT_CREATE_INFO,
    .setLayoutCount = 1, .pSetLayouts = &dsl };
  VkPipelineLayout pl; CHECK(vkCreatePipelineLayout(dev, &plci, NULL, &pl));
  VkRayTracingPipelineCreateInfoKHR rpci = {
    .sType = VK_STRUCTURE_TYPE_RAY_TRACING_PIPELINE_CREATE_INFO_KHR,
    .stageCount = 3, .pStages = stages, .groupCount = 3, .pGroups = groups,
    .maxPipelineRayRecursionDepth = 1, .layout = pl };
  VkPipeline pipe;
  CHECK(pCreateRTPipelines(dev, VK_NULL_HANDLE, VK_NULL_HANDLE, 1, &rpci, NULL, &pipe));

  // ---- shader binding table ----
  uint32_t handle = rtprops.shaderGroupHandleSize;
  uint32_t base = rtprops.shaderGroupBaseAlignment;
  uint32_t stride = aligned(handle, rtprops.shaderGroupHandleAlignment);
  uint8_t *handles = malloc(handle * 3);
  CHECK(pGetRTGroupHandles(dev, pipe, 0, 3, handle * 3, handles));
  // one group per region (raygen, miss, hit), each region base-aligned
  buffer_t sbt = make_buffer(base * 3,
    VK_BUFFER_USAGE_SHADER_BINDING_TABLE_BIT_KHR | VK_BUFFER_USAGE_SHADER_DEVICE_ADDRESS_BIT, HOST, NULL);
  uint8_t *sp; CHECK(vkMapMemory(dev, sbt.mem, 0, base * 3, 0, (void **) &sp));
  memcpy(sp + base * 0, handles + handle * 0, handle); // raygen
  memcpy(sp + base * 1, handles + handle * 1, handle); // miss
  memcpy(sp + base * 2, handles + handle * 2, handle); // hit
  vkUnmapMemory(dev, sbt.mem);
  VkStridedDeviceAddressRegionKHR rgen_r = { sbt.addr + base * 0, stride, stride };
  VkStridedDeviceAddressRegionKHR miss_r = { sbt.addr + base * 1, stride, stride };
  VkStridedDeviceAddressRegionKHR hit_r  = { sbt.addr + base * 2, stride, stride };
  VkStridedDeviceAddressRegionKHR call_r = { 0 };

  // ---- trace ----
  c = begin_cmd();
  VkImageMemoryBarrier toGeneral = { .sType = VK_STRUCTURE_TYPE_IMAGE_MEMORY_BARRIER,
    .oldLayout = VK_IMAGE_LAYOUT_UNDEFINED, .newLayout = VK_IMAGE_LAYOUT_GENERAL,
    .image = img, .subresourceRange = { VK_IMAGE_ASPECT_COLOR_BIT, 0, 1, 0, 1 },
    .srcQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED, .dstQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED,
    .dstAccessMask = VK_ACCESS_SHADER_WRITE_BIT };
  vkCmdPipelineBarrier(c, VK_PIPELINE_STAGE_TOP_OF_PIPE_BIT, VK_PIPELINE_STAGE_RAY_TRACING_SHADER_BIT_KHR,
    0, 0, NULL, 0, NULL, 1, &toGeneral);
  vkCmdBindPipeline(c, VK_PIPELINE_BIND_POINT_RAY_TRACING_KHR, pipe);
  vkCmdBindDescriptorSets(c, VK_PIPELINE_BIND_POINT_RAY_TRACING_KHR, pl, 0, 1, &ds, 0, NULL);
  pCmdTraceRays(c, &rgen_r, &miss_r, &hit_r, &call_r, W, H, 1);
  VkImageMemoryBarrier toSrc = toGeneral;
  toSrc.oldLayout = VK_IMAGE_LAYOUT_GENERAL; toSrc.newLayout = VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL;
  toSrc.srcAccessMask = VK_ACCESS_SHADER_WRITE_BIT; toSrc.dstAccessMask = VK_ACCESS_TRANSFER_READ_BIT;
  vkCmdPipelineBarrier(c, VK_PIPELINE_STAGE_RAY_TRACING_SHADER_BIT_KHR, VK_PIPELINE_STAGE_TRANSFER_BIT,
    0, 0, NULL, 0, NULL, 1, &toSrc);
  buffer_t rb = make_buffer(W * H * 4, VK_BUFFER_USAGE_TRANSFER_DST_BIT, HOST, NULL);
  VkBufferImageCopy copy = { .imageSubresource = { VK_IMAGE_ASPECT_COLOR_BIT, 0, 0, 1 },
    .imageExtent = { W, H, 1 } };
  vkCmdCopyImageToBuffer(c, img, VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL, rb.buf, 1, &copy);
  end_cmd(c);

  // ---- verify ----
  uint8_t *px; CHECK(vkMapMemory(dev, rb.mem, 0, W * H * 4, 0, (void **) &px));
  uint8_t *center = px + (H / 2 * W + W / 2) * 4;
  uint8_t *corner = px + (2 * W + 2) * 4;
  printf("center (ray HIT triangle) rgba = %u %u %u %u\n", center[0], center[1], center[2], center[3]);
  printf("corner (ray MISS -> bg)    rgba = %u %u %u %u\n", corner[0], corner[1], corner[2], corner[3]);
  int hit = (center[0] + center[1] + center[2]) > 80;
  int miss = corner[2] > corner[0] && (corner[0] + corner[1] + corner[2]) < 120;
  vkUnmapMemory(dev, rb.mem);
  printf("%s: ray-traced hit=%d, ray-traced miss-background=%d\n",
    (hit && miss) ? "PASS" : "FAIL", hit, miss);
  return (hit && miss) ? 0 : 1;
}
