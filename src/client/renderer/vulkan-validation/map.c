// RTX render of an actual Quetoo map: parses a Quetoo BSP (IBSP v78), builds a
// bottom-level acceleration structure from the world triangle soup (VERTEXES +
// ELEMENTS lumps), and ray-traces it on real RTX hardware with a point light,
// hardware shadow rays and a headlight fill. This is phase 4 applied to real game
// geometry. Build: cc map.c -lvulkan -lm -o map ; ./map <file.bsp> > out.ppm
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdint.h>
#include <math.h>
#include <vulkan/vulkan.h>

#define BSP_LUMP_VERTEXES 6
#define BSP_LUMP_ELEMENTS 7
#define VERTEX_STRIDE 60          // sizeof(bsp_vertex_t): pos+normal+tangent+bitangent+uv+color32
#define W 1024
#define H 576
#define CHECK(x) do { VkResult _r = (x); if (_r != VK_SUCCESS) { \
  fprintf(stderr, "FAIL %s = %d\n", #x, _r); exit(1); } } while (0)

static VkInstance inst; static VkPhysicalDevice phys; static VkDevice dev;
static VkQueue queue; static uint32_t qfam = (uint32_t) -1;
static PFN_vkGetBufferDeviceAddressKHR pGBA;
static PFN_vkCreateAccelerationStructureKHR pCreateAS;
static PFN_vkGetAccelerationStructureBuildSizesKHR pSizes;
static PFN_vkCmdBuildAccelerationStructuresKHR pBuild;
static PFN_vkGetAccelerationStructureDeviceAddressKHR pASAddr;
static PFN_vkCreateRayTracingPipelinesKHR pRTPipe;
static PFN_vkGetRayTracingShaderGroupHandlesKHR pHandles;
static PFN_vkCmdTraceRaysKHR pTrace;

static uint32_t find_mem(uint32_t bits, VkMemoryPropertyFlags want) {
  VkPhysicalDeviceMemoryProperties mp; vkGetPhysicalDeviceMemoryProperties(phys, &mp);
  for (uint32_t i = 0; i < mp.memoryTypeCount; i++)
    if ((bits & (1u << i)) && (mp.memoryTypes[i].propertyFlags & want) == want) return i;
  fprintf(stderr, "no mem type\n"); exit(1);
}
typedef struct { VkBuffer buf; VkDeviceMemory mem; VkDeviceAddress addr; } buffer_t;
static buffer_t mkbuf(VkDeviceSize size, VkBufferUsageFlags usage, VkMemoryPropertyFlags props, const void *data) {
  buffer_t b = { 0 };
  VkBufferCreateInfo ci = { .sType = VK_STRUCTURE_TYPE_BUFFER_CREATE_INFO, .size = size, .usage = usage, .sharingMode = VK_SHARING_MODE_EXCLUSIVE };
  CHECK(vkCreateBuffer(dev, &ci, NULL, &b.buf));
  VkMemoryRequirements req; vkGetBufferMemoryRequirements(dev, b.buf, &req);
  VkMemoryAllocateFlagsInfo fi = { .sType = VK_STRUCTURE_TYPE_MEMORY_ALLOCATE_FLAGS_INFO, .flags = VK_MEMORY_ALLOCATE_DEVICE_ADDRESS_BIT };
  VkMemoryAllocateInfo ai = { .sType = VK_STRUCTURE_TYPE_MEMORY_ALLOCATE_INFO, .pNext = &fi, .allocationSize = req.size, .memoryTypeIndex = find_mem(req.memoryTypeBits, props) };
  CHECK(vkAllocateMemory(dev, &ai, NULL, &b.mem)); CHECK(vkBindBufferMemory(dev, b.buf, b.mem, 0));
  if (data) { void *p; CHECK(vkMapMemory(dev, b.mem, 0, size, 0, &p)); memcpy(p, data, size); vkUnmapMemory(dev, b.mem); }
  if (usage & VK_BUFFER_USAGE_SHADER_DEVICE_ADDRESS_BIT) {
    VkBufferDeviceAddressInfo bi = { .sType = VK_STRUCTURE_TYPE_BUFFER_DEVICE_ADDRESS_INFO, .buffer = b.buf };
    b.addr = pGBA(dev, &bi);
  }
  return b;
}
static VkCommandPool pool;
static VkCommandBuffer beginc(void) {
  VkCommandBufferAllocateInfo ai = { .sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_ALLOCATE_INFO, .commandPool = pool, .level = VK_COMMAND_BUFFER_LEVEL_PRIMARY, .commandBufferCount = 1 };
  VkCommandBuffer c; CHECK(vkAllocateCommandBuffers(dev, &ai, &c));
  VkCommandBufferBeginInfo bi = { .sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_BEGIN_INFO }; CHECK(vkBeginCommandBuffer(c, &bi)); return c;
}
static void endc(VkCommandBuffer c) {
  CHECK(vkEndCommandBuffer(c));
  VkSubmitInfo si = { .sType = VK_STRUCTURE_TYPE_SUBMIT_INFO, .commandBufferCount = 1, .pCommandBuffers = &c };
  CHECK(vkQueueSubmit(queue, 1, &si, VK_NULL_HANDLE)); CHECK(vkQueueWaitIdle(queue));
}
static VkShaderModule spv(const char *path) {
  FILE *f = fopen(path, "rb"); if (!f) { fprintf(stderr, "open %s\n", path); exit(1); }
  fseek(f, 0, SEEK_END); long n = ftell(f); fseek(f, 0, SEEK_SET);
  uint32_t *code = malloc(n); if (fread(code, 1, n, f) != (size_t) n) exit(1); fclose(f);
  VkShaderModuleCreateInfo ci = { .sType = VK_STRUCTURE_TYPE_SHADER_MODULE_CREATE_INFO, .codeSize = n, .pCode = code };
  VkShaderModule m; CHECK(vkCreateShaderModule(dev, &ci, NULL, &m)); free(code); return m;
}
static uint32_t al(uint32_t v, uint32_t a) { return (v + a - 1) & ~(a - 1); }

typedef struct { float n[4]; float albedo[4]; } tri_t;

// parsed scene
static float *positions;   // vec3 * nverts (for AS)
static uint32_t nverts;
static uint32_t *elems;    // nelem indices
static uint32_t nelem;
static tri_t *tris; static uint32_t ntris;
static float bmin[3], bmax[3];

static int32_t rd32(const uint8_t *p) { int32_t v; memcpy(&v, p, 4); return v; }

static void load_bsp(const char *path) {
  FILE *f = fopen(path, "rb"); if (!f) { fprintf(stderr, "open %s\n", path); exit(1); }
  fseek(f, 0, SEEK_END); long sz = ftell(f); fseek(f, 0, SEEK_SET);
  uint8_t *data = malloc(sz); if (fread(data, 1, sz, f) != (size_t) sz) exit(1); fclose(f);
  if (memcmp(data, "IBSP", 4) != 0) { fprintf(stderr, "not IBSP\n"); exit(1); }
  int32_t version = rd32(data + 4);
  fprintf(stderr, "BSP version %d\n", version);

  int32_t voff = rd32(data + 8 + BSP_LUMP_VERTEXES * 8);
  int32_t vlen = rd32(data + 8 + BSP_LUMP_VERTEXES * 8 + 4);
  int32_t eoff = rd32(data + 8 + BSP_LUMP_ELEMENTS * 8);
  int32_t elen = rd32(data + 8 + BSP_LUMP_ELEMENTS * 8 + 4);

  nverts = vlen / VERTEX_STRIDE;
  nelem = elen / 4;
  fprintf(stderr, "vertexes=%u (lump %d bytes) elements=%u\n", nverts, vlen, nelem);

  positions = malloc((size_t) nverts * 3 * sizeof(float));
  for (int i = 0; i < 3; i++) { bmin[i] = 1e30f; bmax[i] = -1e30f; }
  for (uint32_t v = 0; v < nverts; v++) {
    const uint8_t *src = data + voff + (size_t) v * VERTEX_STRIDE;
    for (int i = 0; i < 3; i++) {
      float c; memcpy(&c, src + i * 4, 4);
      positions[v * 3 + i] = c;
      if (c < bmin[i]) bmin[i] = c;
      if (c > bmax[i]) bmax[i] = c;
    }
  }
  elems = malloc((size_t) nelem * sizeof(uint32_t));
  for (uint32_t e = 0; e < nelem; e++) elems[e] = (uint32_t) rd32(data + eoff + (size_t) e * 4);

  ntris = nelem / 3;
  tris = malloc((size_t) ntris * sizeof(tri_t));
  for (uint32_t t = 0; t < ntris; t++) {
    uint32_t a = elems[t * 3 + 0], b = elems[t * 3 + 1], c = elems[t * 3 + 2];
    float *pa = &positions[a * 3], *pb = &positions[b * 3], *pc = &positions[c * 3];
    float e1[3] = { pb[0]-pa[0], pb[1]-pa[1], pb[2]-pa[2] };
    float e2[3] = { pc[0]-pa[0], pc[1]-pa[1], pc[2]-pa[2] };
    float nx = e1[1]*e2[2]-e1[2]*e2[1], ny = e1[2]*e2[0]-e1[0]*e2[2], nz = e1[0]*e2[1]-e1[1]*e2[0];
    float len = sqrtf(nx*nx+ny*ny+nz*nz); if (len < 1e-9f) len = 1.f;
    nx/=len; ny/=len; nz/=len;
    tris[t].n[0]=nx; tris[t].n[1]=ny; tris[t].n[2]=nz; tris[t].n[3]=0;
    // orientation-tinted albedo reveals structure (floors/walls/ceilings differ)
    tris[t].albedo[0]=0.45f+0.45f*fabsf(nx);
    tris[t].albedo[1]=0.45f+0.45f*fabsf(ny);
    tris[t].albedo[2]=0.45f+0.45f*fabsf(nz);
    tris[t].albedo[3]=0;
  }
  fprintf(stderr, "triangles=%u bbox=[%.0f %.0f %.0f]..[%.0f %.0f %.0f]\n",
    ntris, bmin[0],bmin[1],bmin[2], bmax[0],bmax[1],bmax[2]);
  free(data);
}

typedef struct { float eye[4]; float target[4]; float light[4]; } pc_t;

int main(int argc, char **argv) {
  if (argc < 2) { fprintf(stderr, "usage: %s file.bsp\n", argv[0]); return 1; }
  load_bsp(argv[1]);

  VkApplicationInfo app = { .sType = VK_STRUCTURE_TYPE_APPLICATION_INFO, .pApplicationName = "map", .apiVersion = VK_API_VERSION_1_2 };
  VkInstanceCreateInfo ici = { .sType = VK_STRUCTURE_TYPE_INSTANCE_CREATE_INFO, .pApplicationInfo = &app };
  CHECK(vkCreateInstance(&ici, NULL, &inst));
  uint32_t cnt = 0; vkEnumeratePhysicalDevices(inst, &cnt, NULL);
  VkPhysicalDevice *devs = malloc(sizeof(*devs)*cnt); vkEnumeratePhysicalDevices(inst, &cnt, devs); phys = devs[0];
  for (uint32_t i = 0; i < cnt; i++) { VkPhysicalDeviceProperties p; vkGetPhysicalDeviceProperties(devs[i], &p);
    if (p.deviceType == VK_PHYSICAL_DEVICE_TYPE_DISCRETE_GPU) { phys = devs[i]; break; } }
  VkPhysicalDeviceProperties pp; vkGetPhysicalDeviceProperties(phys, &pp); fprintf(stderr, "Device: %s\n", pp.deviceName);
  VkPhysicalDeviceRayTracingPipelinePropertiesKHR rtp = { .sType = VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_RAY_TRACING_PIPELINE_PROPERTIES_KHR };
  VkPhysicalDeviceProperties2 p2 = { .sType = VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_PROPERTIES_2, .pNext = &rtp }; vkGetPhysicalDeviceProperties2(phys, &p2);
  uint32_t qn = 0; vkGetPhysicalDeviceQueueFamilyProperties(phys, &qn, NULL);
  VkQueueFamilyProperties *qf = malloc(sizeof(*qf)*qn); vkGetPhysicalDeviceQueueFamilyProperties(phys, &qn, qf);
  for (uint32_t i = 0; i < qn; i++) if (qf[i].queueFlags & VK_QUEUE_GRAPHICS_BIT) { qfam = i; break; }

  VkPhysicalDeviceBufferDeviceAddressFeatures bda = { .sType = VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_BUFFER_DEVICE_ADDRESS_FEATURES, .bufferDeviceAddress = VK_TRUE };
  VkPhysicalDeviceAccelerationStructureFeaturesKHR asf = { .sType = VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_ACCELERATION_STRUCTURE_FEATURES_KHR, .accelerationStructure = VK_TRUE, .pNext = &bda };
  VkPhysicalDeviceRayTracingPipelineFeaturesKHR rtf = { .sType = VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_RAY_TRACING_PIPELINE_FEATURES_KHR, .rayTracingPipeline = VK_TRUE, .pNext = &asf };
  const char *exts[] = { VK_KHR_ACCELERATION_STRUCTURE_EXTENSION_NAME, VK_KHR_RAY_TRACING_PIPELINE_EXTENSION_NAME, VK_KHR_DEFERRED_HOST_OPERATIONS_EXTENSION_NAME, VK_KHR_BUFFER_DEVICE_ADDRESS_EXTENSION_NAME };
  float prio = 1.0f; VkDeviceQueueCreateInfo qci = { .sType = VK_STRUCTURE_TYPE_DEVICE_QUEUE_CREATE_INFO, .queueFamilyIndex = qfam, .queueCount = 1, .pQueuePriorities = &prio };
  VkDeviceCreateInfo dci = { .sType = VK_STRUCTURE_TYPE_DEVICE_CREATE_INFO, .pNext = &rtf, .queueCreateInfoCount = 1, .pQueueCreateInfos = &qci, .enabledExtensionCount = 4, .ppEnabledExtensionNames = exts };
  CHECK(vkCreateDevice(phys, &dci, NULL, &dev)); vkGetDeviceQueue(dev, qfam, 0, &queue);
  #define LOAD(p,n) p=(PFN_##n)vkGetDeviceProcAddr(dev,#n); if(!p){fprintf(stderr,"load %s\n",#n);exit(1);}
  LOAD(pGBA, vkGetBufferDeviceAddressKHR); LOAD(pCreateAS, vkCreateAccelerationStructureKHR);
  LOAD(pSizes, vkGetAccelerationStructureBuildSizesKHR); LOAD(pBuild, vkCmdBuildAccelerationStructuresKHR);
  LOAD(pASAddr, vkGetAccelerationStructureDeviceAddressKHR); LOAD(pRTPipe, vkCreateRayTracingPipelinesKHR);
  LOAD(pHandles, vkGetRayTracingShaderGroupHandlesKHR); LOAD(pTrace, vkCmdTraceRaysKHR);
  VkCommandPoolCreateInfo cpci = { .sType = VK_STRUCTURE_TYPE_COMMAND_POOL_CREATE_INFO, .flags = VK_COMMAND_POOL_CREATE_RESET_COMMAND_BUFFER_BIT, .queueFamilyIndex = qfam };
  CHECK(vkCreateCommandPool(dev, &cpci, NULL, &pool));
  const VkMemoryPropertyFlags HOST = VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT | VK_MEMORY_PROPERTY_HOST_COHERENT_BIT;
  const VkMemoryPropertyFlags DEVL = VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT;
  const VkBufferUsageFlags ASIN = VK_BUFFER_USAGE_ACCELERATION_STRUCTURE_BUILD_INPUT_READ_ONLY_BIT_KHR | VK_BUFFER_USAGE_SHADER_DEVICE_ADDRESS_BIT;

  // BLAS from world geometry
  buffer_t vbo = mkbuf((VkDeviceSize) nverts*3*sizeof(float), ASIN, HOST, positions);
  buffer_t ibo = mkbuf((VkDeviceSize) nelem*sizeof(uint32_t), ASIN, HOST, elems);
  VkAccelerationStructureGeometryKHR geo = { .sType = VK_STRUCTURE_TYPE_ACCELERATION_STRUCTURE_GEOMETRY_KHR,
    .geometryType = VK_GEOMETRY_TYPE_TRIANGLES_KHR, .flags = VK_GEOMETRY_OPAQUE_BIT_KHR,
    .geometry.triangles = { .sType = VK_STRUCTURE_TYPE_ACCELERATION_STRUCTURE_GEOMETRY_TRIANGLES_DATA_KHR,
      .vertexFormat = VK_FORMAT_R32G32B32_SFLOAT, .vertexData.deviceAddress = vbo.addr, .vertexStride = 12,
      .maxVertex = nverts-1, .indexType = VK_INDEX_TYPE_UINT32, .indexData.deviceAddress = ibo.addr } };
  VkAccelerationStructureBuildGeometryInfoKHR bgi = { .sType = VK_STRUCTURE_TYPE_ACCELERATION_STRUCTURE_BUILD_GEOMETRY_INFO_KHR,
    .type = VK_ACCELERATION_STRUCTURE_TYPE_BOTTOM_LEVEL_KHR, .flags = VK_BUILD_ACCELERATION_STRUCTURE_PREFER_FAST_TRACE_BIT_KHR,
    .mode = VK_BUILD_ACCELERATION_STRUCTURE_MODE_BUILD_KHR, .geometryCount = 1, .pGeometries = &geo };
  VkAccelerationStructureBuildSizesInfoKHR sz = { .sType = VK_STRUCTURE_TYPE_ACCELERATION_STRUCTURE_BUILD_SIZES_INFO_KHR };
  pSizes(dev, VK_ACCELERATION_STRUCTURE_BUILD_TYPE_DEVICE_KHR, &bgi, &ntris, &sz);
  fprintf(stderr, "BLAS size %.1f MB scratch %.1f MB\n", sz.accelerationStructureSize/1e6, sz.buildScratchSize/1e6);
  buffer_t blasb = mkbuf(sz.accelerationStructureSize, VK_BUFFER_USAGE_ACCELERATION_STRUCTURE_STORAGE_BIT_KHR | VK_BUFFER_USAGE_SHADER_DEVICE_ADDRESS_BIT, DEVL, NULL);
  VkAccelerationStructureKHR blas;
  VkAccelerationStructureCreateInfoKHR aci = { .sType = VK_STRUCTURE_TYPE_ACCELERATION_STRUCTURE_CREATE_INFO_KHR, .buffer = blasb.buf, .size = sz.accelerationStructureSize, .type = VK_ACCELERATION_STRUCTURE_TYPE_BOTTOM_LEVEL_KHR };
  CHECK(pCreateAS(dev, &aci, NULL, &blas));
  buffer_t scr = mkbuf(sz.buildScratchSize, VK_BUFFER_USAGE_STORAGE_BUFFER_BIT | VK_BUFFER_USAGE_SHADER_DEVICE_ADDRESS_BIT, DEVL, NULL);
  bgi.dstAccelerationStructure = blas; bgi.scratchData.deviceAddress = scr.addr;
  VkAccelerationStructureBuildRangeInfoKHR rng = { .primitiveCount = ntris }; const VkAccelerationStructureBuildRangeInfoKHR *prng = &rng;
  VkCommandBuffer c = beginc(); pBuild(c, 1, &bgi, &prng); endc(c);
  VkAccelerationStructureDeviceAddressInfoKHR dai = { .sType = VK_STRUCTURE_TYPE_ACCELERATION_STRUCTURE_DEVICE_ADDRESS_INFO_KHR, .accelerationStructure = blas };
  VkDeviceAddress blas_addr = pASAddr(dev, &dai);

  // TLAS
  VkAccelerationStructureInstanceKHR id = { .transform = { .matrix = { {1,0,0,0},{0,1,0,0},{0,0,1,0} } }, .mask = 0xFF, .accelerationStructureReference = blas_addr };
  buffer_t ib = mkbuf(sizeof(id), ASIN, HOST, &id);
  VkAccelerationStructureGeometryKHR tg = { .sType = VK_STRUCTURE_TYPE_ACCELERATION_STRUCTURE_GEOMETRY_KHR, .geometryType = VK_GEOMETRY_TYPE_INSTANCES_KHR, .flags = VK_GEOMETRY_OPAQUE_BIT_KHR,
    .geometry.instances = { .sType = VK_STRUCTURE_TYPE_ACCELERATION_STRUCTURE_GEOMETRY_INSTANCES_DATA_KHR, .data.deviceAddress = ib.addr } };
  VkAccelerationStructureBuildGeometryInfoKHR tbgi = { .sType = VK_STRUCTURE_TYPE_ACCELERATION_STRUCTURE_BUILD_GEOMETRY_INFO_KHR, .type = VK_ACCELERATION_STRUCTURE_TYPE_TOP_LEVEL_KHR,
    .flags = VK_BUILD_ACCELERATION_STRUCTURE_PREFER_FAST_TRACE_BIT_KHR, .mode = VK_BUILD_ACCELERATION_STRUCTURE_MODE_BUILD_KHR, .geometryCount = 1, .pGeometries = &tg };
  uint32_t one = 1; VkAccelerationStructureBuildSizesInfoKHR tsz = { .sType = VK_STRUCTURE_TYPE_ACCELERATION_STRUCTURE_BUILD_SIZES_INFO_KHR };
  pSizes(dev, VK_ACCELERATION_STRUCTURE_BUILD_TYPE_DEVICE_KHR, &tbgi, &one, &tsz);
  buffer_t tb = mkbuf(tsz.accelerationStructureSize, VK_BUFFER_USAGE_ACCELERATION_STRUCTURE_STORAGE_BIT_KHR | VK_BUFFER_USAGE_SHADER_DEVICE_ADDRESS_BIT, DEVL, NULL);
  VkAccelerationStructureKHR tlas;
  VkAccelerationStructureCreateInfoKHR tci = { .sType = VK_STRUCTURE_TYPE_ACCELERATION_STRUCTURE_CREATE_INFO_KHR, .buffer = tb.buf, .size = tsz.accelerationStructureSize, .type = VK_ACCELERATION_STRUCTURE_TYPE_TOP_LEVEL_KHR };
  CHECK(pCreateAS(dev, &tci, NULL, &tlas));
  buffer_t tscr = mkbuf(tsz.buildScratchSize, VK_BUFFER_USAGE_STORAGE_BUFFER_BIT | VK_BUFFER_USAGE_SHADER_DEVICE_ADDRESS_BIT, DEVL, NULL);
  tbgi.dstAccelerationStructure = tlas; tbgi.scratchData.deviceAddress = tscr.addr;
  VkAccelerationStructureBuildRangeInfoKHR trng = { .primitiveCount = 1 }; const VkAccelerationStructureBuildRangeInfoKHR *ptrng = &trng;
  c = beginc(); pBuild(c, 1, &tbgi, &ptrng); endc(c);

  buffer_t tribuf = mkbuf((VkDeviceSize) ntris*sizeof(tri_t), VK_BUFFER_USAGE_STORAGE_BUFFER_BIT, HOST, tris);

  // storage image
  VkImageCreateInfo imgci = { .sType = VK_STRUCTURE_TYPE_IMAGE_CREATE_INFO, .imageType = VK_IMAGE_TYPE_2D, .format = VK_FORMAT_R8G8B8A8_UNORM,
    .extent = { W, H, 1 }, .mipLevels = 1, .arrayLayers = 1, .samples = VK_SAMPLE_COUNT_1_BIT, .tiling = VK_IMAGE_TILING_OPTIMAL,
    .usage = VK_IMAGE_USAGE_STORAGE_BIT | VK_IMAGE_USAGE_TRANSFER_SRC_BIT, .initialLayout = VK_IMAGE_LAYOUT_UNDEFINED };
  VkImage img; CHECK(vkCreateImage(dev, &imgci, NULL, &img));
  VkMemoryRequirements ir; vkGetImageMemoryRequirements(dev, img, &ir);
  VkMemoryAllocateInfo imai = { .sType = VK_STRUCTURE_TYPE_MEMORY_ALLOCATE_INFO, .allocationSize = ir.size, .memoryTypeIndex = find_mem(ir.memoryTypeBits, DEVL) };
  VkDeviceMemory imem; CHECK(vkAllocateMemory(dev, &imai, NULL, &imem)); CHECK(vkBindImageMemory(dev, img, imem, 0));
  VkImageViewCreateInfo ivci = { .sType = VK_STRUCTURE_TYPE_IMAGE_VIEW_CREATE_INFO, .image = img, .viewType = VK_IMAGE_VIEW_TYPE_2D, .format = VK_FORMAT_R8G8B8A8_UNORM, .subresourceRange = { VK_IMAGE_ASPECT_COLOR_BIT, 0, 1, 0, 1 } };
  VkImageView view; CHECK(vkCreateImageView(dev, &ivci, NULL, &view));

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
  VkWriteDescriptorSet wr[3] = {
    { .sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET, .pNext = &was, .dstSet = ds, .dstBinding = 0, .descriptorCount = 1, .descriptorType = VK_DESCRIPTOR_TYPE_ACCELERATION_STRUCTURE_KHR },
    { .sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET, .dstSet = ds, .dstBinding = 1, .descriptorCount = 1, .descriptorType = VK_DESCRIPTOR_TYPE_STORAGE_IMAGE, .pImageInfo = &dii },
    { .sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET, .dstSet = ds, .dstBinding = 2, .descriptorCount = 1, .descriptorType = VK_DESCRIPTOR_TYPE_STORAGE_BUFFER, .pBufferInfo = &dbi } };
  vkUpdateDescriptorSets(dev, 3, wr, 0, NULL);

  VkShaderModule rgen = spv("map.rgen.spv"), rmiss = spv("map.rmiss.spv"), smiss = spv("map_shadow.rmiss.spv"), rchit = spv("map.rchit.spv");
  VkPipelineShaderStageCreateInfo st[4] = {
    { .sType = VK_STRUCTURE_TYPE_PIPELINE_SHADER_STAGE_CREATE_INFO, .stage = VK_SHADER_STAGE_RAYGEN_BIT_KHR, .module = rgen, .pName = "main" },
    { .sType = VK_STRUCTURE_TYPE_PIPELINE_SHADER_STAGE_CREATE_INFO, .stage = VK_SHADER_STAGE_MISS_BIT_KHR, .module = rmiss, .pName = "main" },
    { .sType = VK_STRUCTURE_TYPE_PIPELINE_SHADER_STAGE_CREATE_INFO, .stage = VK_SHADER_STAGE_MISS_BIT_KHR, .module = smiss, .pName = "main" },
    { .sType = VK_STRUCTURE_TYPE_PIPELINE_SHADER_STAGE_CREATE_INFO, .stage = VK_SHADER_STAGE_CLOSEST_HIT_BIT_KHR, .module = rchit, .pName = "main" } };
  VkRayTracingShaderGroupCreateInfoKHR gr[4] = {
    { .sType = VK_STRUCTURE_TYPE_RAY_TRACING_SHADER_GROUP_CREATE_INFO_KHR, .type = VK_RAY_TRACING_SHADER_GROUP_TYPE_GENERAL_KHR, .generalShader = 0, .closestHitShader = VK_SHADER_UNUSED_KHR, .anyHitShader = VK_SHADER_UNUSED_KHR, .intersectionShader = VK_SHADER_UNUSED_KHR },
    { .sType = VK_STRUCTURE_TYPE_RAY_TRACING_SHADER_GROUP_CREATE_INFO_KHR, .type = VK_RAY_TRACING_SHADER_GROUP_TYPE_GENERAL_KHR, .generalShader = 1, .closestHitShader = VK_SHADER_UNUSED_KHR, .anyHitShader = VK_SHADER_UNUSED_KHR, .intersectionShader = VK_SHADER_UNUSED_KHR },
    { .sType = VK_STRUCTURE_TYPE_RAY_TRACING_SHADER_GROUP_CREATE_INFO_KHR, .type = VK_RAY_TRACING_SHADER_GROUP_TYPE_GENERAL_KHR, .generalShader = 2, .closestHitShader = VK_SHADER_UNUSED_KHR, .anyHitShader = VK_SHADER_UNUSED_KHR, .intersectionShader = VK_SHADER_UNUSED_KHR },
    { .sType = VK_STRUCTURE_TYPE_RAY_TRACING_SHADER_GROUP_CREATE_INFO_KHR, .type = VK_RAY_TRACING_SHADER_GROUP_TYPE_TRIANGLES_HIT_GROUP_KHR, .generalShader = VK_SHADER_UNUSED_KHR, .closestHitShader = 3, .anyHitShader = VK_SHADER_UNUSED_KHR, .intersectionShader = VK_SHADER_UNUSED_KHR } };
  VkPushConstantRange pcr = { VK_SHADER_STAGE_RAYGEN_BIT_KHR | VK_SHADER_STAGE_CLOSEST_HIT_BIT_KHR, 0, sizeof(pc_t) };
  VkPipelineLayoutCreateInfo plci = { .sType = VK_STRUCTURE_TYPE_PIPELINE_LAYOUT_CREATE_INFO, .setLayoutCount = 1, .pSetLayouts = &dsl, .pushConstantRangeCount = 1, .pPushConstantRanges = &pcr };
  VkPipelineLayout pl; CHECK(vkCreatePipelineLayout(dev, &plci, NULL, &pl));
  VkRayTracingPipelineCreateInfoKHR rpci = { .sType = VK_STRUCTURE_TYPE_RAY_TRACING_PIPELINE_CREATE_INFO_KHR, .stageCount = 4, .pStages = st, .groupCount = 4, .pGroups = gr, .maxPipelineRayRecursionDepth = 2, .layout = pl };
  VkPipeline pipe; CHECK(pRTPipe(dev, VK_NULL_HANDLE, VK_NULL_HANDLE, 1, &rpci, NULL, &pipe));

  uint32_t hs = rtp.shaderGroupHandleSize, base = rtp.shaderGroupBaseAlignment, stride = al(hs, rtp.shaderGroupHandleAlignment);
  uint8_t *hnd = malloc(hs*4); CHECK(pHandles(dev, pipe, 0, 4, hs*4, hnd));
  uint32_t rgo = 0, mso = al(stride, base), hto = mso + al(2*stride, base), sbtsz = hto + al(stride, base);
  buffer_t sbt = mkbuf(sbtsz, VK_BUFFER_USAGE_SHADER_BINDING_TABLE_BIT_KHR | VK_BUFFER_USAGE_SHADER_DEVICE_ADDRESS_BIT, HOST, NULL);
  uint8_t *sp; CHECK(vkMapMemory(dev, sbt.mem, 0, sbtsz, 0, (void**)&sp));
  memcpy(sp+rgo, hnd+hs*0, hs); memcpy(sp+mso+stride*0, hnd+hs*1, hs); memcpy(sp+mso+stride*1, hnd+hs*2, hs); memcpy(sp+hto, hnd+hs*3, hs);
  vkUnmapMemory(dev, sbt.mem);
  VkStridedDeviceAddressRegionKHR rr = { sbt.addr+rgo, stride, stride }, mr = { sbt.addr+mso, stride, 2*stride }, hr = { sbt.addr+hto, stride, stride }, cr = { 0 };

  // camera + light from bbox (Z-up)
  float cx=(bmin[0]+bmax[0])*0.5f, cy=(bmin[1]+bmax[1])*0.5f, cz=(bmin[2]+bmax[2])*0.5f;
  float ex=bmax[0]-bmin[0], ey=bmax[1]-bmin[1], ez=bmax[2]-bmin[2];
  pc_t pc;
  pc.eye[0]=cx - ex*0.42f; pc.eye[1]=cy - ey*0.42f; pc.eye[2]=cz + ez*0.30f; pc.eye[3]=0;
  pc.target[0]=cx; pc.target[1]=cy; pc.target[2]=cz; pc.target[3]=0;
  pc.light[0]=cx + ex*0.2f; pc.light[1]=cy + ey*0.2f; pc.light[2]=bmax[2] - ez*0.08f; pc.light[3]=0;

  // trace
  c = beginc();
  VkImageMemoryBarrier b1 = { .sType = VK_STRUCTURE_TYPE_IMAGE_MEMORY_BARRIER, .oldLayout = VK_IMAGE_LAYOUT_UNDEFINED, .newLayout = VK_IMAGE_LAYOUT_GENERAL, .image = img, .subresourceRange = { VK_IMAGE_ASPECT_COLOR_BIT, 0,1,0,1 }, .srcQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED, .dstQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED, .dstAccessMask = VK_ACCESS_SHADER_WRITE_BIT };
  vkCmdPipelineBarrier(c, VK_PIPELINE_STAGE_TOP_OF_PIPE_BIT, VK_PIPELINE_STAGE_RAY_TRACING_SHADER_BIT_KHR, 0,0,NULL,0,NULL,1,&b1);
  vkCmdBindPipeline(c, VK_PIPELINE_BIND_POINT_RAY_TRACING_KHR, pipe);
  vkCmdBindDescriptorSets(c, VK_PIPELINE_BIND_POINT_RAY_TRACING_KHR, pl, 0, 1, &ds, 0, NULL);
  vkCmdPushConstants(c, pl, VK_SHADER_STAGE_RAYGEN_BIT_KHR | VK_SHADER_STAGE_CLOSEST_HIT_BIT_KHR, 0, sizeof(pc), &pc);
  pTrace(c, &rr, &mr, &hr, &cr, W, H, 1);
  VkImageMemoryBarrier b2 = b1; b2.oldLayout = VK_IMAGE_LAYOUT_GENERAL; b2.newLayout = VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL; b2.srcAccessMask = VK_ACCESS_SHADER_WRITE_BIT; b2.dstAccessMask = VK_ACCESS_TRANSFER_READ_BIT;
  vkCmdPipelineBarrier(c, VK_PIPELINE_STAGE_RAY_TRACING_SHADER_BIT_KHR, VK_PIPELINE_STAGE_TRANSFER_BIT, 0,0,NULL,0,NULL,1,&b2);
  buffer_t rb = mkbuf((VkDeviceSize) W*H*4, VK_BUFFER_USAGE_TRANSFER_DST_BIT, HOST, NULL);
  VkBufferImageCopy cp = { .imageSubresource = { VK_IMAGE_ASPECT_COLOR_BIT, 0,0,1 }, .imageExtent = { W, H, 1 } };
  vkCmdCopyImageToBuffer(c, img, VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL, rb.buf, 1, &cp);
  endc(c);

  uint8_t *px; CHECK(vkMapMemory(dev, rb.mem, 0, (VkDeviceSize) W*H*4, 0, (void**)&px));
  printf("P6\n%d %d\n255\n", W, H);
  for (uint32_t i = 0; i < W*H; i++) fwrite(px+i*4, 1, 3, stdout);
  fprintf(stderr, "rendered %s: %u triangles at %dx%d\n", argv[1], ntris, W, H);
  return 0;
}
