// RTX scene render: ray-traces real 3D geometry (floor + pyramid) with a point
// light, Lambertian shading and hardware shadow rays, on real RTX hardware, and
// writes the result to a PPM image. Demonstrates the ray tracing path producing an
// actual rendered image of geometry -- the technique Quetoo's phase 4 applies to BSP
// and mesh geometry. Build: cc scene.c -lvulkan -lm -o scene ; ./scene > out.ppm
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdint.h>
#include <math.h>
#include <vulkan/vulkan.h>

#define W 640
#define H 480
#define CHECK(x) do { VkResult _r = (x); if (_r != VK_SUCCESS) { \
  fprintf(stderr, "FAIL %s = %d\n", #x, _r); exit(1); } } while (0)

static VkInstance inst;
static VkPhysicalDevice phys;
static VkDevice dev;
static VkQueue queue;
static uint32_t qfam = (uint32_t) -1;

static PFN_vkGetBufferDeviceAddressKHR pGetBufferDeviceAddress;
static PFN_vkCreateAccelerationStructureKHR pCreateAS;
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
    if ((bits & (1u << i)) && (mp.memoryTypes[i].propertyFlags & want) == want) return i;
  fprintf(stderr, "FAIL no memory type\n"); exit(1);
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
  VkMemoryAllocateInfo ai = { .sType = VK_STRUCTURE_TYPE_MEMORY_ALLOCATE_INFO, .pNext = &fi,
    .allocationSize = req.size, .memoryTypeIndex = find_mem(req.memoryTypeBits, props) };
  CHECK(vkAllocateMemory(dev, &ai, NULL, &b.mem));
  CHECK(vkBindBufferMemory(dev, b.buf, b.mem, 0));
  if (data) { void *p; CHECK(vkMapMemory(dev, b.mem, 0, size, 0, &p)); memcpy(p, data, size); vkUnmapMemory(dev, b.mem); }
  if (usage & VK_BUFFER_USAGE_SHADER_DEVICE_ADDRESS_BIT) {
    VkBufferDeviceAddressInfo bi = { .sType = VK_STRUCTURE_TYPE_BUFFER_DEVICE_ADDRESS_INFO, .buffer = b.buf };
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
  CHECK(vkBeginCommandBuffer(c, &bi)); return c;
}
static void end_cmd(VkCommandBuffer c) {
  CHECK(vkEndCommandBuffer(c));
  VkSubmitInfo si = { .sType = VK_STRUCTURE_TYPE_SUBMIT_INFO, .commandBufferCount = 1, .pCommandBuffers = &c };
  CHECK(vkQueueSubmit(queue, 1, &si, VK_NULL_HANDLE)); CHECK(vkQueueWaitIdle(queue));
}
static VkShaderModule load_spv(const char *path) {
  FILE *f = fopen(path, "rb"); if (!f) { fprintf(stderr, "open %s\n", path); exit(1); }
  fseek(f, 0, SEEK_END); long n = ftell(f); fseek(f, 0, SEEK_SET);
  uint32_t *code = malloc(n); if (fread(code, 1, n, f) != (size_t) n) { exit(1); } fclose(f);
  VkShaderModuleCreateInfo ci = { .sType = VK_STRUCTURE_TYPE_SHADER_MODULE_CREATE_INFO, .codeSize = n, .pCode = code };
  VkShaderModule m; CHECK(vkCreateShaderModule(dev, &ci, NULL, &m)); free(code); return m;
}
static uint32_t al(uint32_t v, uint32_t a) { return (v + a - 1) & ~(a - 1); }

// ---- scene geometry (built on CPU) ----
typedef struct { float n[4]; float albedo[4]; } tri_t;
static float verts[1024]; static uint32_t nverts;
static uint32_t indices[1024]; static uint32_t nidx;
static tri_t tris[256]; static uint32_t ntris;

static void add_tri(float ax,float ay,float az, float bx,float by,float bz,
                    float cx,float cy,float cz, float r,float g,float b) {
  uint32_t base = nverts;
  float v[9] = { ax,ay,az, bx,by,bz, cx,cy,cz };
  for (int i = 0; i < 9; i++) verts[nverts*3/3*3 + i] = 0; // placeholder
  memcpy(&verts[base*3], v, sizeof(v));
  nverts += 3;
  indices[nidx++] = base; indices[nidx++] = base+1; indices[nidx++] = base+2;
  // geometric normal
  float e1[3] = { bx-ax, by-ay, bz-az }, e2[3] = { cx-ax, cy-ay, cz-az };
  float nx = e1[1]*e2[2]-e1[2]*e2[1], ny = e1[2]*e2[0]-e1[0]*e2[2], nz = e1[0]*e2[1]-e1[1]*e2[0];
  float len = sqrtf(nx*nx+ny*ny+nz*nz); nx/=len; ny/=len; nz/=len;
  // orient outward from the scene interior point (0,0.4,0)
  float cxx=(ax+bx+cx)/3-0.0f, cyy=(ay+by+cy)/3-0.4f, czz=(az+bz+cz)/3-0.0f;
  if (nx*cxx+ny*cyy+nz*czz < 0) { nx=-nx; ny=-ny; nz=-nz; }
  tris[ntris].n[0]=nx; tris[ntris].n[1]=ny; tris[ntris].n[2]=nz; tris[ntris].n[3]=0;
  tris[ntris].albedo[0]=r; tris[ntris].albedo[1]=g; tris[ntris].albedo[2]=b; tris[ntris].albedo[3]=0;
  ntris++;
}

static void build_scene(void) {
  // floor
  add_tri(-3,0,-3,  3,0,-3,  3,0,3,  0.75f,0.75f,0.78f);
  add_tri(-3,0,-3,  3,0,3,  -3,0,3,  0.70f,0.70f,0.74f);
  // pyramid
  float S[3]={0,1.3f,0}, P0[3]={-0.8f,0,-0.6f}, P1[3]={0.8f,0,-0.6f}, P2[3]={0,0,0.9f};
  add_tri(S[0],S[1],S[2], P0[0],P0[1],P0[2], P1[0],P1[1],P1[2], 0.85f,0.22f,0.22f);
  add_tri(S[0],S[1],S[2], P1[0],P1[1],P1[2], P2[0],P2[1],P2[2], 0.25f,0.80f,0.32f);
  add_tri(S[0],S[1],S[2], P2[0],P2[1],P2[2], P0[0],P0[1],P0[2], 0.30f,0.45f,0.92f);
  add_tri(P0[0],P0[1],P0[2], P2[0],P2[1],P2[2], P1[0],P1[1],P1[2], 0.80f,0.75f,0.25f);
}

static VkAccelerationStructureKHR build_blas(VkDeviceAddress *out_addr) {
  const VkMemoryPropertyFlags HOST = VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT | VK_MEMORY_PROPERTY_HOST_COHERENT_BIT;
  const VkBufferUsageFlags IN = VK_BUFFER_USAGE_ACCELERATION_STRUCTURE_BUILD_INPUT_READ_ONLY_BIT_KHR | VK_BUFFER_USAGE_SHADER_DEVICE_ADDRESS_BIT;
  buffer_t vbo = make_buffer(nverts*3*sizeof(float), IN, HOST, verts);
  buffer_t ibo = make_buffer(nidx*sizeof(uint32_t), IN, HOST, indices);
  VkAccelerationStructureGeometryKHR geo = { .sType = VK_STRUCTURE_TYPE_ACCELERATION_STRUCTURE_GEOMETRY_KHR,
    .geometryType = VK_GEOMETRY_TYPE_TRIANGLES_KHR, .flags = VK_GEOMETRY_OPAQUE_BIT_KHR,
    .geometry.triangles = { .sType = VK_STRUCTURE_TYPE_ACCELERATION_STRUCTURE_GEOMETRY_TRIANGLES_DATA_KHR,
      .vertexFormat = VK_FORMAT_R32G32B32_SFLOAT, .vertexData.deviceAddress = vbo.addr, .vertexStride = 12,
      .maxVertex = nverts-1, .indexType = VK_INDEX_TYPE_UINT32, .indexData.deviceAddress = ibo.addr } };
  VkAccelerationStructureBuildGeometryInfoKHR bgi = { .sType = VK_STRUCTURE_TYPE_ACCELERATION_STRUCTURE_BUILD_GEOMETRY_INFO_KHR,
    .type = VK_ACCELERATION_STRUCTURE_TYPE_BOTTOM_LEVEL_KHR, .flags = VK_BUILD_ACCELERATION_STRUCTURE_PREFER_FAST_TRACE_BIT_KHR,
    .mode = VK_BUILD_ACCELERATION_STRUCTURE_MODE_BUILD_KHR, .geometryCount = 1, .pGeometries = &geo };
  uint32_t prim = ntris;
  VkAccelerationStructureBuildSizesInfoKHR sz = { .sType = VK_STRUCTURE_TYPE_ACCELERATION_STRUCTURE_BUILD_SIZES_INFO_KHR };
  pGetASBuildSizes(dev, VK_ACCELERATION_STRUCTURE_BUILD_TYPE_DEVICE_KHR, &bgi, &prim, &sz);
  buffer_t as_buf = make_buffer(sz.accelerationStructureSize,
    VK_BUFFER_USAGE_ACCELERATION_STRUCTURE_STORAGE_BIT_KHR | VK_BUFFER_USAGE_SHADER_DEVICE_ADDRESS_BIT,
    VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT, NULL);
  VkAccelerationStructureKHR as;
  VkAccelerationStructureCreateInfoKHR ci = { .sType = VK_STRUCTURE_TYPE_ACCELERATION_STRUCTURE_CREATE_INFO_KHR,
    .buffer = as_buf.buf, .size = sz.accelerationStructureSize, .type = VK_ACCELERATION_STRUCTURE_TYPE_BOTTOM_LEVEL_KHR };
  CHECK(pCreateAS(dev, &ci, NULL, &as));
  buffer_t scratch = make_buffer(sz.buildScratchSize,
    VK_BUFFER_USAGE_STORAGE_BUFFER_BIT | VK_BUFFER_USAGE_SHADER_DEVICE_ADDRESS_BIT, VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT, NULL);
  bgi.dstAccelerationStructure = as; bgi.scratchData.deviceAddress = scratch.addr;
  VkAccelerationStructureBuildRangeInfoKHR range = { .primitiveCount = ntris };
  const VkAccelerationStructureBuildRangeInfoKHR *pr = &range;
  VkCommandBuffer c = begin_cmd(); pCmdBuildAS(c, 1, &bgi, &pr); end_cmd(c);
  VkAccelerationStructureDeviceAddressInfoKHR dai = { .sType = VK_STRUCTURE_TYPE_ACCELERATION_STRUCTURE_DEVICE_ADDRESS_INFO_KHR, .accelerationStructure = as };
  *out_addr = pGetASDeviceAddress(dev, &dai);
  return as;
}

int main(void) {
  build_scene();
  VkApplicationInfo app = { .sType = VK_STRUCTURE_TYPE_APPLICATION_INFO, .pApplicationName = "scene", .apiVersion = VK_API_VERSION_1_2 };
  VkInstanceCreateInfo ici = { .sType = VK_STRUCTURE_TYPE_INSTANCE_CREATE_INFO, .pApplicationInfo = &app };
  CHECK(vkCreateInstance(&ici, NULL, &inst));
  uint32_t cnt = 0; vkEnumeratePhysicalDevices(inst, &cnt, NULL);
  VkPhysicalDevice *devs = malloc(sizeof(*devs)*cnt); vkEnumeratePhysicalDevices(inst, &cnt, devs); phys = devs[0];
  for (uint32_t i = 0; i < cnt; i++) { VkPhysicalDeviceProperties p; vkGetPhysicalDeviceProperties(devs[i], &p);
    if (p.deviceType == VK_PHYSICAL_DEVICE_TYPE_DISCRETE_GPU) { phys = devs[i]; break; } }
  VkPhysicalDeviceProperties pp; vkGetPhysicalDeviceProperties(phys, &pp);
  fprintf(stderr, "Device: %s\n", pp.deviceName);
  VkPhysicalDeviceRayTracingPipelinePropertiesKHR rtprops = { .sType = VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_RAY_TRACING_PIPELINE_PROPERTIES_KHR };
  VkPhysicalDeviceProperties2 props2 = { .sType = VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_PROPERTIES_2, .pNext = &rtprops };
  vkGetPhysicalDeviceProperties2(phys, &props2);
  uint32_t qn = 0; vkGetPhysicalDeviceQueueFamilyProperties(phys, &qn, NULL);
  VkQueueFamilyProperties *qf = malloc(sizeof(*qf)*qn); vkGetPhysicalDeviceQueueFamilyProperties(phys, &qn, qf);
  for (uint32_t i = 0; i < qn; i++) if (qf[i].queueFlags & VK_QUEUE_GRAPHICS_BIT) { qfam = i; break; }

  VkPhysicalDeviceBufferDeviceAddressFeatures bda = { .sType = VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_BUFFER_DEVICE_ADDRESS_FEATURES, .bufferDeviceAddress = VK_TRUE };
  VkPhysicalDeviceAccelerationStructureFeaturesKHR asf = { .sType = VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_ACCELERATION_STRUCTURE_FEATURES_KHR, .accelerationStructure = VK_TRUE, .pNext = &bda };
  VkPhysicalDeviceRayTracingPipelineFeaturesKHR rtf = { .sType = VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_RAY_TRACING_PIPELINE_FEATURES_KHR, .rayTracingPipeline = VK_TRUE, .pNext = &asf };
  const char *exts[] = { VK_KHR_ACCELERATION_STRUCTURE_EXTENSION_NAME, VK_KHR_RAY_TRACING_PIPELINE_EXTENSION_NAME,
    VK_KHR_DEFERRED_HOST_OPERATIONS_EXTENSION_NAME, VK_KHR_BUFFER_DEVICE_ADDRESS_EXTENSION_NAME };
  float prio = 1.0f;
  VkDeviceQueueCreateInfo qci = { .sType = VK_STRUCTURE_TYPE_DEVICE_QUEUE_CREATE_INFO, .queueFamilyIndex = qfam, .queueCount = 1, .pQueuePriorities = &prio };
  VkDeviceCreateInfo dci = { .sType = VK_STRUCTURE_TYPE_DEVICE_CREATE_INFO, .pNext = &rtf, .queueCreateInfoCount = 1,
    .pQueueCreateInfos = &qci, .enabledExtensionCount = 4, .ppEnabledExtensionNames = exts };
  CHECK(vkCreateDevice(phys, &dci, NULL, &dev));
  vkGetDeviceQueue(dev, qfam, 0, &queue);

  #define LOAD(p, n) p = (PFN_##n) vkGetDeviceProcAddr(dev, #n); if (!p) { fprintf(stderr, "load %s\n", #n); exit(1); }
  LOAD(pGetBufferDeviceAddress, vkGetBufferDeviceAddressKHR);
  LOAD(pCreateAS, vkCreateAccelerationStructureKHR);
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

  VkDeviceAddress blas_addr; VkAccelerationStructureKHR blas = build_blas(&blas_addr);

  // TLAS
  VkAccelerationStructureInstanceKHR id = { .transform = { .matrix = { {1,0,0,0},{0,1,0,0},{0,0,1,0} } },
    .mask = 0xFF, .accelerationStructureReference = blas_addr };
  buffer_t ib = make_buffer(sizeof(id), VK_BUFFER_USAGE_ACCELERATION_STRUCTURE_BUILD_INPUT_READ_ONLY_BIT_KHR | VK_BUFFER_USAGE_SHADER_DEVICE_ADDRESS_BIT, HOST, &id);
  VkAccelerationStructureGeometryKHR tg = { .sType = VK_STRUCTURE_TYPE_ACCELERATION_STRUCTURE_GEOMETRY_KHR,
    .geometryType = VK_GEOMETRY_TYPE_INSTANCES_KHR, .flags = VK_GEOMETRY_OPAQUE_BIT_KHR,
    .geometry.instances = { .sType = VK_STRUCTURE_TYPE_ACCELERATION_STRUCTURE_GEOMETRY_INSTANCES_DATA_KHR, .data.deviceAddress = ib.addr } };
  VkAccelerationStructureBuildGeometryInfoKHR tbgi = { .sType = VK_STRUCTURE_TYPE_ACCELERATION_STRUCTURE_BUILD_GEOMETRY_INFO_KHR,
    .type = VK_ACCELERATION_STRUCTURE_TYPE_TOP_LEVEL_KHR, .flags = VK_BUILD_ACCELERATION_STRUCTURE_PREFER_FAST_TRACE_BIT_KHR,
    .mode = VK_BUILD_ACCELERATION_STRUCTURE_MODE_BUILD_KHR, .geometryCount = 1, .pGeometries = &tg };
  uint32_t one = 1;
  VkAccelerationStructureBuildSizesInfoKHR tsz = { .sType = VK_STRUCTURE_TYPE_ACCELERATION_STRUCTURE_BUILD_SIZES_INFO_KHR };
  pGetASBuildSizes(dev, VK_ACCELERATION_STRUCTURE_BUILD_TYPE_DEVICE_KHR, &tbgi, &one, &tsz);
  buffer_t tas_buf = make_buffer(tsz.accelerationStructureSize, VK_BUFFER_USAGE_ACCELERATION_STRUCTURE_STORAGE_BIT_KHR | VK_BUFFER_USAGE_SHADER_DEVICE_ADDRESS_BIT, VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT, NULL);
  VkAccelerationStructureKHR tlas;
  VkAccelerationStructureCreateInfoKHR tci = { .sType = VK_STRUCTURE_TYPE_ACCELERATION_STRUCTURE_CREATE_INFO_KHR, .buffer = tas_buf.buf, .size = tsz.accelerationStructureSize, .type = VK_ACCELERATION_STRUCTURE_TYPE_TOP_LEVEL_KHR };
  CHECK(pCreateAS(dev, &tci, NULL, &tlas));
  buffer_t tscr = make_buffer(tsz.buildScratchSize, VK_BUFFER_USAGE_STORAGE_BUFFER_BIT | VK_BUFFER_USAGE_SHADER_DEVICE_ADDRESS_BIT, VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT, NULL);
  tbgi.dstAccelerationStructure = tlas; tbgi.scratchData.deviceAddress = tscr.addr;
  VkAccelerationStructureBuildRangeInfoKHR tr = { .primitiveCount = 1 };
  const VkAccelerationStructureBuildRangeInfoKHR *tpr = &tr;
  VkCommandBuffer c = begin_cmd(); pCmdBuildAS(c, 1, &tbgi, &tpr); end_cmd(c);

  // per-triangle SSBO
  buffer_t tribuf = make_buffer(ntris*sizeof(tri_t), VK_BUFFER_USAGE_STORAGE_BUFFER_BIT, HOST, tris);

  // storage image
  VkImageCreateInfo imgci = { .sType = VK_STRUCTURE_TYPE_IMAGE_CREATE_INFO, .imageType = VK_IMAGE_TYPE_2D,
    .format = VK_FORMAT_R8G8B8A8_UNORM, .extent = { W, H, 1 }, .mipLevels = 1, .arrayLayers = 1,
    .samples = VK_SAMPLE_COUNT_1_BIT, .tiling = VK_IMAGE_TILING_OPTIMAL,
    .usage = VK_IMAGE_USAGE_STORAGE_BIT | VK_IMAGE_USAGE_TRANSFER_SRC_BIT, .initialLayout = VK_IMAGE_LAYOUT_UNDEFINED };
  VkImage img; CHECK(vkCreateImage(dev, &imgci, NULL, &img));
  VkMemoryRequirements ireq; vkGetImageMemoryRequirements(dev, img, &ireq);
  VkMemoryAllocateInfo imai = { .sType = VK_STRUCTURE_TYPE_MEMORY_ALLOCATE_INFO, .allocationSize = ireq.size, .memoryTypeIndex = find_mem(ireq.memoryTypeBits, VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT) };
  VkDeviceMemory imem; CHECK(vkAllocateMemory(dev, &imai, NULL, &imem)); CHECK(vkBindImageMemory(dev, img, imem, 0));
  VkImageViewCreateInfo ivci = { .sType = VK_STRUCTURE_TYPE_IMAGE_VIEW_CREATE_INFO, .image = img, .viewType = VK_IMAGE_VIEW_TYPE_2D,
    .format = VK_FORMAT_R8G8B8A8_UNORM, .subresourceRange = { VK_IMAGE_ASPECT_COLOR_BIT, 0, 1, 0, 1 } };
  VkImageView view; CHECK(vkCreateImageView(dev, &ivci, NULL, &view));

  // descriptors
  VkDescriptorSetLayoutBinding binds[3] = {
    { 0, VK_DESCRIPTOR_TYPE_ACCELERATION_STRUCTURE_KHR, 1, VK_SHADER_STAGE_RAYGEN_BIT_KHR | VK_SHADER_STAGE_CLOSEST_HIT_BIT_KHR },
    { 1, VK_DESCRIPTOR_TYPE_STORAGE_IMAGE, 1, VK_SHADER_STAGE_RAYGEN_BIT_KHR },
    { 2, VK_DESCRIPTOR_TYPE_STORAGE_BUFFER, 1, VK_SHADER_STAGE_CLOSEST_HIT_BIT_KHR } };
  VkDescriptorSetLayoutCreateInfo dslci = { .sType = VK_STRUCTURE_TYPE_DESCRIPTOR_SET_LAYOUT_CREATE_INFO, .bindingCount = 3, .pBindings = binds };
  VkDescriptorSetLayout dsl; CHECK(vkCreateDescriptorSetLayout(dev, &dslci, NULL, &dsl));
  VkDescriptorPoolSize psz[3] = { { VK_DESCRIPTOR_TYPE_ACCELERATION_STRUCTURE_KHR, 1 }, { VK_DESCRIPTOR_TYPE_STORAGE_IMAGE, 1 }, { VK_DESCRIPTOR_TYPE_STORAGE_BUFFER, 1 } };
  VkDescriptorPoolCreateInfo dpci = { .sType = VK_STRUCTURE_TYPE_DESCRIPTOR_POOL_CREATE_INFO, .maxSets = 1, .poolSizeCount = 3, .pPoolSizes = psz };
  VkDescriptorPool dp; CHECK(vkCreateDescriptorPool(dev, &dpci, NULL, &dp));
  VkDescriptorSetAllocateInfo dsai = { .sType = VK_STRUCTURE_TYPE_DESCRIPTOR_SET_ALLOCATE_INFO, .descriptorPool = dp, .descriptorSetCount = 1, .pSetLayouts = &dsl };
  VkDescriptorSet ds; CHECK(vkAllocateDescriptorSets(dev, &dsai, &ds));
  VkWriteDescriptorSetAccelerationStructureKHR was = { .sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET_ACCELERATION_STRUCTURE_KHR, .accelerationStructureCount = 1, .pAccelerationStructures = &tlas };
  VkDescriptorImageInfo dii = { .imageView = view, .imageLayout = VK_IMAGE_LAYOUT_GENERAL };
  VkDescriptorBufferInfo dbi = { .buffer = tribuf.buf, .range = VK_WHOLE_SIZE };
  VkWriteDescriptorSet writes[3] = {
    { .sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET, .pNext = &was, .dstSet = ds, .dstBinding = 0, .descriptorCount = 1, .descriptorType = VK_DESCRIPTOR_TYPE_ACCELERATION_STRUCTURE_KHR },
    { .sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET, .dstSet = ds, .dstBinding = 1, .descriptorCount = 1, .descriptorType = VK_DESCRIPTOR_TYPE_STORAGE_IMAGE, .pImageInfo = &dii },
    { .sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET, .dstSet = ds, .dstBinding = 2, .descriptorCount = 1, .descriptorType = VK_DESCRIPTOR_TYPE_STORAGE_BUFFER, .pBufferInfo = &dbi } };
  vkUpdateDescriptorSets(dev, 3, writes, 0, NULL);

  // pipeline: rgen, miss(primary), miss(shadow), chit
  VkShaderModule rgen = load_spv("scene.rgen.spv"), rmiss = load_spv("scene.rmiss.spv"),
    smiss = load_spv("scene_shadow.rmiss.spv"), rchit = load_spv("scene.rchit.spv");
  VkPipelineShaderStageCreateInfo stages[4] = {
    { .sType = VK_STRUCTURE_TYPE_PIPELINE_SHADER_STAGE_CREATE_INFO, .stage = VK_SHADER_STAGE_RAYGEN_BIT_KHR, .module = rgen, .pName = "main" },
    { .sType = VK_STRUCTURE_TYPE_PIPELINE_SHADER_STAGE_CREATE_INFO, .stage = VK_SHADER_STAGE_MISS_BIT_KHR, .module = rmiss, .pName = "main" },
    { .sType = VK_STRUCTURE_TYPE_PIPELINE_SHADER_STAGE_CREATE_INFO, .stage = VK_SHADER_STAGE_MISS_BIT_KHR, .module = smiss, .pName = "main" },
    { .sType = VK_STRUCTURE_TYPE_PIPELINE_SHADER_STAGE_CREATE_INFO, .stage = VK_SHADER_STAGE_CLOSEST_HIT_BIT_KHR, .module = rchit, .pName = "main" } };
  VkRayTracingShaderGroupCreateInfoKHR groups[4] = {
    { .sType = VK_STRUCTURE_TYPE_RAY_TRACING_SHADER_GROUP_CREATE_INFO_KHR, .type = VK_RAY_TRACING_SHADER_GROUP_TYPE_GENERAL_KHR, .generalShader = 0, .closestHitShader = VK_SHADER_UNUSED_KHR, .anyHitShader = VK_SHADER_UNUSED_KHR, .intersectionShader = VK_SHADER_UNUSED_KHR },
    { .sType = VK_STRUCTURE_TYPE_RAY_TRACING_SHADER_GROUP_CREATE_INFO_KHR, .type = VK_RAY_TRACING_SHADER_GROUP_TYPE_GENERAL_KHR, .generalShader = 1, .closestHitShader = VK_SHADER_UNUSED_KHR, .anyHitShader = VK_SHADER_UNUSED_KHR, .intersectionShader = VK_SHADER_UNUSED_KHR },
    { .sType = VK_STRUCTURE_TYPE_RAY_TRACING_SHADER_GROUP_CREATE_INFO_KHR, .type = VK_RAY_TRACING_SHADER_GROUP_TYPE_GENERAL_KHR, .generalShader = 2, .closestHitShader = VK_SHADER_UNUSED_KHR, .anyHitShader = VK_SHADER_UNUSED_KHR, .intersectionShader = VK_SHADER_UNUSED_KHR },
    { .sType = VK_STRUCTURE_TYPE_RAY_TRACING_SHADER_GROUP_CREATE_INFO_KHR, .type = VK_RAY_TRACING_SHADER_GROUP_TYPE_TRIANGLES_HIT_GROUP_KHR, .generalShader = VK_SHADER_UNUSED_KHR, .closestHitShader = 3, .anyHitShader = VK_SHADER_UNUSED_KHR, .intersectionShader = VK_SHADER_UNUSED_KHR } };
  VkPipelineLayoutCreateInfo plci = { .sType = VK_STRUCTURE_TYPE_PIPELINE_LAYOUT_CREATE_INFO, .setLayoutCount = 1, .pSetLayouts = &dsl };
  VkPipelineLayout pl; CHECK(vkCreatePipelineLayout(dev, &plci, NULL, &pl));
  VkRayTracingPipelineCreateInfoKHR rpci = { .sType = VK_STRUCTURE_TYPE_RAY_TRACING_PIPELINE_CREATE_INFO_KHR,
    .stageCount = 4, .pStages = stages, .groupCount = 4, .pGroups = groups, .maxPipelineRayRecursionDepth = 2, .layout = pl };
  VkPipeline pipe; CHECK(pCreateRTPipelines(dev, VK_NULL_HANDLE, VK_NULL_HANDLE, 1, &rpci, NULL, &pipe));

  // SBT: rgen | miss(2) | hit(1)
  uint32_t hs = rtprops.shaderGroupHandleSize, base = rtprops.shaderGroupBaseAlignment;
  uint32_t stride = al(hs, rtprops.shaderGroupHandleAlignment);
  uint8_t *handles = malloc(hs * 4);
  CHECK(pGetRTGroupHandles(dev, pipe, 0, 4, hs * 4, handles));
  uint32_t rgenOff = 0, missOff = al(stride, base), hitOff = missOff + al(2 * stride, base);
  uint32_t sbtSize = hitOff + al(stride, base);
  buffer_t sbt = make_buffer(sbtSize, VK_BUFFER_USAGE_SHADER_BINDING_TABLE_BIT_KHR | VK_BUFFER_USAGE_SHADER_DEVICE_ADDRESS_BIT, HOST, NULL);
  uint8_t *sp; CHECK(vkMapMemory(dev, sbt.mem, 0, sbtSize, 0, (void **) &sp));
  memcpy(sp + rgenOff, handles + hs * 0, hs);
  memcpy(sp + missOff + stride * 0, handles + hs * 1, hs);
  memcpy(sp + missOff + stride * 1, handles + hs * 2, hs);
  memcpy(sp + hitOff, handles + hs * 3, hs);
  vkUnmapMemory(dev, sbt.mem);
  VkStridedDeviceAddressRegionKHR rgen_r = { sbt.addr + rgenOff, stride, stride };
  VkStridedDeviceAddressRegionKHR miss_r = { sbt.addr + missOff, stride, 2 * stride };
  VkStridedDeviceAddressRegionKHR hit_r = { sbt.addr + hitOff, stride, stride };
  VkStridedDeviceAddressRegionKHR call_r = { 0 };

  // trace
  c = begin_cmd();
  VkImageMemoryBarrier b1 = { .sType = VK_STRUCTURE_TYPE_IMAGE_MEMORY_BARRIER, .oldLayout = VK_IMAGE_LAYOUT_UNDEFINED, .newLayout = VK_IMAGE_LAYOUT_GENERAL,
    .image = img, .subresourceRange = { VK_IMAGE_ASPECT_COLOR_BIT, 0, 1, 0, 1 }, .srcQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED, .dstQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED, .dstAccessMask = VK_ACCESS_SHADER_WRITE_BIT };
  vkCmdPipelineBarrier(c, VK_PIPELINE_STAGE_TOP_OF_PIPE_BIT, VK_PIPELINE_STAGE_RAY_TRACING_SHADER_BIT_KHR, 0, 0, NULL, 0, NULL, 1, &b1);
  vkCmdBindPipeline(c, VK_PIPELINE_BIND_POINT_RAY_TRACING_KHR, pipe);
  vkCmdBindDescriptorSets(c, VK_PIPELINE_BIND_POINT_RAY_TRACING_KHR, pl, 0, 1, &ds, 0, NULL);
  pCmdTraceRays(c, &rgen_r, &miss_r, &hit_r, &call_r, W, H, 1);
  VkImageMemoryBarrier b2 = b1; b2.oldLayout = VK_IMAGE_LAYOUT_GENERAL; b2.newLayout = VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL;
  b2.srcAccessMask = VK_ACCESS_SHADER_WRITE_BIT; b2.dstAccessMask = VK_ACCESS_TRANSFER_READ_BIT;
  vkCmdPipelineBarrier(c, VK_PIPELINE_STAGE_RAY_TRACING_SHADER_BIT_KHR, VK_PIPELINE_STAGE_TRANSFER_BIT, 0, 0, NULL, 0, NULL, 1, &b2);
  buffer_t rb = make_buffer(W * H * 4, VK_BUFFER_USAGE_TRANSFER_DST_BIT, HOST, NULL);
  VkBufferImageCopy cp = { .imageSubresource = { VK_IMAGE_ASPECT_COLOR_BIT, 0, 0, 1 }, .imageExtent = { W, H, 1 } };
  vkCmdCopyImageToBuffer(c, img, VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL, rb.buf, 1, &cp);
  end_cmd(c);

  // write PPM to stdout
  uint8_t *px; CHECK(vkMapMemory(dev, rb.mem, 0, W * H * 4, 0, (void **) &px));
  printf("P6\n%d %d\n255\n", W, H);
  for (uint32_t i = 0; i < W * H; i++) { fwrite(px + i * 4, 1, 3, stdout); }
  vkUnmapMemory(dev, rb.mem);
  fprintf(stderr, "rendered %u triangles, %dx%d\n", ntris, W, H);
  return 0;
}
