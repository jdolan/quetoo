/*
 * Copyright(c) 2026 QuetooPlus.
 * SPDX-License-Identifier: GPL-2.0-or-later
 */

#include "r_local.h"
#include "r_rtx_scene.h"

#if BUILD_RTX

typedef struct {
  vec4_t normal;
  vec4_t albedo;
} r_rtx_tri_t;

typedef struct {
  vec4_t origin_radius;
  vec4_t color_intensity;
} r_rtx_light_t;

typedef struct {
  vec4_t eye;
  vec4_t target;
  vec4_t light;
  vec4_t params;
} r_rtx_push_t;

static struct {
  bool initialized;
  const r_model_t *world;
  VkImage output_image;

  PFN_vkGetBufferDeviceAddressKHR GetBufferDeviceAddress;
  PFN_vkCreateAccelerationStructureKHR CreateAccelerationStructure;
  PFN_vkDestroyAccelerationStructureKHR DestroyAccelerationStructure;
  PFN_vkGetAccelerationStructureBuildSizesKHR GetBuildSizes;
  PFN_vkCmdBuildAccelerationStructuresKHR CmdBuildAccelerationStructures;
  PFN_vkGetAccelerationStructureDeviceAddressKHR GetAccelerationStructureDeviceAddress;
  PFN_vkCreateRayTracingPipelinesKHR CreateRayTracingPipelines;
  PFN_vkGetRayTracingShaderGroupHandlesKHR GetShaderGroupHandles;
  PFN_vkCmdTraceRaysKHR CmdTraceRays;

  VkPhysicalDeviceRayTracingPipelinePropertiesKHR properties;

  VkAccelerationStructureKHR blas;
  VkBuffer blas_buffer;
  VkDeviceMemory blas_memory;
  VkAccelerationStructureKHR tlas;
  VkBuffer tlas_buffer;
  VkDeviceMemory tlas_memory;

  VkBuffer tri_buffer;
  VkDeviceMemory tri_memory;
  VkBuffer light_buffer;
  VkDeviceMemory light_memory;
  uint32_t num_lights;

  VkDescriptorSetLayout set_layout;
  VkDescriptorPool descriptor_pool;
  VkDescriptorSet descriptor_set;
  VkPipelineLayout pipeline_layout;
  VkPipeline pipeline;

  VkBuffer sbt_buffer;
  VkDeviceMemory sbt_memory;
  VkStridedDeviceAddressRegionKHR rgen_region;
  VkStridedDeviceAddressRegionKHR miss_region;
  VkStridedDeviceAddressRegionKHR hit_region;
  VkStridedDeviceAddressRegionKHR call_region;

  vec3_t world_mins;
  vec3_t world_maxs;
  vec3_t last_eye;
  vec3_t last_target;
  uint32_t accum_frame;
} r_rtx_scene;

static bool R_Rtx_LoadProcs(const r_rtx_device_t *device) {
#define R_RTX_PROC(field, api) \
  r_rtx_scene.field = (PFN_##api) vkGetDeviceProcAddr(device->device, #api); \
  if (!r_rtx_scene.field) { Com_Warn("RTX: failed to load %s\n", #api); return false; }

  R_RTX_PROC(GetBufferDeviceAddress, vkGetBufferDeviceAddressKHR);
  R_RTX_PROC(CreateAccelerationStructure, vkCreateAccelerationStructureKHR);
  R_RTX_PROC(DestroyAccelerationStructure, vkDestroyAccelerationStructureKHR);
  R_RTX_PROC(GetBuildSizes, vkGetAccelerationStructureBuildSizesKHR);
  R_RTX_PROC(CmdBuildAccelerationStructures, vkCmdBuildAccelerationStructuresKHR);
  R_RTX_PROC(GetAccelerationStructureDeviceAddress, vkGetAccelerationStructureDeviceAddressKHR);
  R_RTX_PROC(CreateRayTracingPipelines, vkCreateRayTracingPipelinesKHR);
  R_RTX_PROC(GetShaderGroupHandles, vkGetRayTracingShaderGroupHandlesKHR);
  R_RTX_PROC(CmdTraceRays, vkCmdTraceRaysKHR);
#undef R_RTX_PROC

  r_rtx_scene.properties.sType = VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_RAY_TRACING_PIPELINE_PROPERTIES_KHR;
  VkPhysicalDeviceProperties2 properties = {
    .sType = VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_PROPERTIES_2,
    .pNext = &r_rtx_scene.properties,
  };
  vkGetPhysicalDeviceProperties2(device->physical_device, &properties);
  return true;
}

static bool R_Rtx_CreateBuffer(const r_rtx_device_t *device, VkDeviceSize size,
                               VkBufferUsageFlags usage, VkMemoryPropertyFlags properties,
                               const void *data, VkBuffer *out_buffer,
                               VkDeviceMemory *out_memory, VkDeviceAddress *out_address) {
  const VkBufferCreateInfo buffer = {
    .sType = VK_STRUCTURE_TYPE_BUFFER_CREATE_INFO,
    .size = size,
    .usage = usage,
    .sharingMode = VK_SHARING_MODE_EXCLUSIVE,
  };
  if (vkCreateBuffer(device->device, &buffer, NULL, out_buffer) != VK_SUCCESS) {
    return false;
  }

  VkMemoryRequirements requirements;
  vkGetBufferMemoryRequirements(device->device, *out_buffer, &requirements);

  uint32_t type;
  if (!R_Rtx_FindMemoryType(device, requirements.memoryTypeBits, properties, &type)) {
    vkDestroyBuffer(device->device, *out_buffer, NULL);
    *out_buffer = VK_NULL_HANDLE;
    return false;
  }

  VkMemoryAllocateFlagsInfo flags = {
    .sType = VK_STRUCTURE_TYPE_MEMORY_ALLOCATE_FLAGS_INFO,
    .flags = usage & VK_BUFFER_USAGE_SHADER_DEVICE_ADDRESS_BIT ? VK_MEMORY_ALLOCATE_DEVICE_ADDRESS_BIT : 0,
  };
  const VkMemoryAllocateInfo allocate = {
    .sType = VK_STRUCTURE_TYPE_MEMORY_ALLOCATE_INFO,
    .pNext = flags.flags ? &flags : NULL,
    .allocationSize = requirements.size,
    .memoryTypeIndex = type,
  };
  if (vkAllocateMemory(device->device, &allocate, NULL, out_memory) != VK_SUCCESS ||
      vkBindBufferMemory(device->device, *out_buffer, *out_memory, 0) != VK_SUCCESS) {
    if (*out_memory) {
      vkFreeMemory(device->device, *out_memory, NULL);
      *out_memory = VK_NULL_HANDLE;
    }
    vkDestroyBuffer(device->device, *out_buffer, NULL);
    *out_buffer = VK_NULL_HANDLE;
    return false;
  }

  if (data) {
    void *mapped;
    if (vkMapMemory(device->device, *out_memory, 0, size, 0, &mapped) != VK_SUCCESS) {
      return false;
    }
    memcpy(mapped, data, (size_t) size);
    vkUnmapMemory(device->device, *out_memory);
  }

  if (out_address) {
    *out_address = r_rtx_scene.GetBufferDeviceAddress(device->device, &(VkBufferDeviceAddressInfo) {
      .sType = VK_STRUCTURE_TYPE_BUFFER_DEVICE_ADDRESS_INFO,
      .buffer = *out_buffer,
    });
  }
  return true;
}

static bool R_Rtx_BeginCommand(const r_rtx_device_t *device, VkCommandBuffer *command) {
  if (vkAllocateCommandBuffers(device->device, &(VkCommandBufferAllocateInfo) {
    .sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_ALLOCATE_INFO,
    .commandPool = device->command_pool,
    .level = VK_COMMAND_BUFFER_LEVEL_PRIMARY,
    .commandBufferCount = 1,
  }, command) != VK_SUCCESS) {
    return false;
  }
  return vkBeginCommandBuffer(*command, &(VkCommandBufferBeginInfo) {
    .sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_BEGIN_INFO,
    .flags = VK_COMMAND_BUFFER_USAGE_ONE_TIME_SUBMIT_BIT,
  }) == VK_SUCCESS;
}

static bool R_Rtx_EndCommand(const r_rtx_device_t *device, VkCommandBuffer command) {
  const bool succeeded = vkEndCommandBuffer(command) == VK_SUCCESS &&
      vkQueueSubmit(device->queue, 1, &(VkSubmitInfo) {
        .sType = VK_STRUCTURE_TYPE_SUBMIT_INFO,
        .commandBufferCount = 1,
        .pCommandBuffers = &command,
      }, VK_NULL_HANDLE) == VK_SUCCESS &&
      vkQueueWaitIdle(device->queue) == VK_SUCCESS;
  vkFreeCommandBuffers(device->device, device->command_pool, 1, &command);
  return succeeded;
}

static VkShaderModule R_Rtx_LoadShader(const r_rtx_device_t *device, const char *name) {
  void *buffer;
  const int64_t len = Fs_Load(name, &buffer);
  if (len <= 0) {
    Com_Warn("RTX: failed to load shader %s\n", name);
    return VK_NULL_HANDLE;
  }

  VkShaderModule module = VK_NULL_HANDLE;
  if (vkCreateShaderModule(device->device, &(VkShaderModuleCreateInfo) {
    .sType = VK_STRUCTURE_TYPE_SHADER_MODULE_CREATE_INFO,
    .codeSize = (size_t) len,
    .pCode = buffer,
  }, NULL, &module) != VK_SUCCESS) {
    Com_Warn("RTX: failed to create shader module %s\n", name);
  }
  Fs_Free(buffer);
  return module;
}

static uint32_t R_Rtx_Align(uint32_t value, uint32_t alignment) {
  return (value + alignment - 1) & ~(alignment - 1);
}

static vec3_t R_Rtx_MaterialColor(const r_material_t *material) {
  return material ? Vec3(material->color.r, material->color.g, material->color.b) : Vec3(0.6f, 0.6f, 0.6f);
}

static bool R_Rtx_BuildWorld(const r_rtx_device_t *device) {
  const r_model_t *world = R_WorldModel();
  if (!world || !world->bsp || world->bsp->num_vertexes <= 0 || world->bsp->num_elements < 3) {
    return false;
  }

  const r_bsp_model_t *bsp = world->bsp;
  const int32_t num_vertexes = bsp->num_vertexes;
  vec3_t *positions = Mem_TagMalloc(sizeof(vec3_t) * num_vertexes, MEM_TAG_RENDERER);
  r_rtx_scene.world_mins = Vec3_Mins();
  r_rtx_scene.world_maxs = Vec3_Maxs();
  for (int32_t i = 0; i < num_vertexes; i++) {
    positions[i] = bsp->vertexes[i].position;
    r_rtx_scene.world_mins = Vec3_Minf(r_rtx_scene.world_mins, positions[i]);
    r_rtx_scene.world_maxs = Vec3_Maxf(r_rtx_scene.world_maxs, positions[i]);
  }

  int32_t draw_elements_count = 0;
  for (int32_t d = 0; d < bsp->num_draw_elements; d++) {
    draw_elements_count += bsp->draw_elements[d].num_elements;
  }

  int32_t num_elements = draw_elements_count >= 3 ? draw_elements_count : bsp->num_elements;
  int32_t num_tris = num_elements / 3;
  uint32_t *elements = Mem_TagMalloc(sizeof(uint32_t) * num_elements, MEM_TAG_RENDERER);
  r_rtx_tri_t *tris = Mem_TagMalloc(sizeof(r_rtx_tri_t) * num_tris, MEM_TAG_RENDERER);

  if (draw_elements_count >= 3) {
    int32_t e_out = 0;
    for (int32_t d = 0; d < bsp->num_draw_elements; d++) {
      const r_bsp_draw_elements_t *draw = &bsp->draw_elements[d];
      const vec3_t albedo = R_Rtx_MaterialColor(draw->material);
      const uint32_t first = (uint32_t) ((uintptr_t) draw->elements / sizeof(uint32_t));
      for (int32_t j = 0; j + 2 < draw->num_elements; j += 3) {
        const uint32_t i0 = bsp->elements[first + j + 0];
        const uint32_t i1 = bsp->elements[first + j + 1];
        const uint32_t i2 = bsp->elements[first + j + 2];
        elements[e_out + 0] = i0;
        elements[e_out + 1] = i1;
        elements[e_out + 2] = i2;
        const vec3_t n = Vec3_Normalize(Vec3_Cross(Vec3_Subtract(positions[i1], positions[i0]),
                                                   Vec3_Subtract(positions[i2], positions[i0])));
        tris[e_out / 3].normal = Vec4(n.x, n.y, n.z, 0.f);
        tris[e_out / 3].albedo = Vec4(albedo.x, albedo.y, albedo.z, 0.f);
        e_out += 3;
      }
    }
    num_elements = e_out;
    num_tris = num_elements / 3;
  } else {
    memcpy(elements, bsp->elements, sizeof(uint32_t) * num_elements);
    for (int32_t t = 0; t < num_tris; t++) {
      const vec3_t a = positions[elements[t * 3 + 0]];
      const vec3_t b = positions[elements[t * 3 + 1]];
      const vec3_t c = positions[elements[t * 3 + 2]];
      const vec3_t n = Vec3_Normalize(Vec3_Cross(Vec3_Subtract(b, a), Vec3_Subtract(c, a)));
      tris[t].normal = Vec4(n.x, n.y, n.z, 0.f);
      tris[t].albedo = Vec4(0.45f + 0.45f * fabsf(n.x), 0.45f + 0.45f * fabsf(n.y), 0.45f + 0.45f * fabsf(n.z), 0.f);
    }
  }

  const VkMemoryPropertyFlags host = VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT | VK_MEMORY_PROPERTY_HOST_COHERENT_BIT;
  const VkMemoryPropertyFlags local = VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT;
  const VkBufferUsageFlags as_input = VK_BUFFER_USAGE_ACCELERATION_STRUCTURE_BUILD_INPUT_READ_ONLY_BIT_KHR |
                                      VK_BUFFER_USAGE_SHADER_DEVICE_ADDRESS_BIT;
  VkBuffer vbuf = VK_NULL_HANDLE, ibuf = VK_NULL_HANDLE, scratch_buf = VK_NULL_HANDLE;
  VkBuffer inst_buf = VK_NULL_HANDLE, tscratch_buf = VK_NULL_HANDLE;
  VkDeviceMemory vmem = VK_NULL_HANDLE, imem = VK_NULL_HANDLE, scratch_mem = VK_NULL_HANDLE;
  VkDeviceMemory inst_mem = VK_NULL_HANDLE, tscratch_mem = VK_NULL_HANDLE;
  VkDeviceAddress vaddr = 0, iaddr = 0, scratch = 0, inst_addr = 0, tscratch = 0;

  bool ok = R_Rtx_CreateBuffer(device, sizeof(vec3_t) * num_vertexes, as_input, host, positions, &vbuf, &vmem, &vaddr) &&
            R_Rtx_CreateBuffer(device, sizeof(uint32_t) * num_elements, as_input, host, elements, &ibuf, &imem, &iaddr);

  VkAccelerationStructureGeometryKHR geo = {
    .sType = VK_STRUCTURE_TYPE_ACCELERATION_STRUCTURE_GEOMETRY_KHR,
    .geometryType = VK_GEOMETRY_TYPE_TRIANGLES_KHR,
    .flags = VK_GEOMETRY_OPAQUE_BIT_KHR,
    .geometry.triangles = {
      .sType = VK_STRUCTURE_TYPE_ACCELERATION_STRUCTURE_GEOMETRY_TRIANGLES_DATA_KHR,
      .vertexFormat = VK_FORMAT_R32G32B32_SFLOAT,
      .vertexData.deviceAddress = vaddr,
      .vertexStride = sizeof(vec3_t),
      .maxVertex = (uint32_t) (num_vertexes - 1),
      .indexType = VK_INDEX_TYPE_UINT32,
      .indexData.deviceAddress = iaddr,
    },
  };
  VkAccelerationStructureBuildGeometryInfoKHR bgi = {
    .sType = VK_STRUCTURE_TYPE_ACCELERATION_STRUCTURE_BUILD_GEOMETRY_INFO_KHR,
    .type = VK_ACCELERATION_STRUCTURE_TYPE_BOTTOM_LEVEL_KHR,
    .flags = VK_BUILD_ACCELERATION_STRUCTURE_PREFER_FAST_TRACE_BIT_KHR,
    .mode = VK_BUILD_ACCELERATION_STRUCTURE_MODE_BUILD_KHR,
    .geometryCount = 1,
    .pGeometries = &geo,
  };
  uint32_t prim_count = (uint32_t) num_tris;
  VkAccelerationStructureBuildSizesInfoKHR sizes = { .sType = VK_STRUCTURE_TYPE_ACCELERATION_STRUCTURE_BUILD_SIZES_INFO_KHR };
  if (ok) {
    r_rtx_scene.GetBuildSizes(device->device, VK_ACCELERATION_STRUCTURE_BUILD_TYPE_DEVICE_KHR, &bgi, &prim_count, &sizes);
    ok = R_Rtx_CreateBuffer(device, sizes.accelerationStructureSize,
                            VK_BUFFER_USAGE_ACCELERATION_STRUCTURE_STORAGE_BIT_KHR | VK_BUFFER_USAGE_SHADER_DEVICE_ADDRESS_BIT,
                            local, NULL, &r_rtx_scene.blas_buffer, &r_rtx_scene.blas_memory, NULL) &&
         r_rtx_scene.CreateAccelerationStructure(device->device, &(VkAccelerationStructureCreateInfoKHR) {
           .sType = VK_STRUCTURE_TYPE_ACCELERATION_STRUCTURE_CREATE_INFO_KHR,
           .buffer = r_rtx_scene.blas_buffer,
           .size = sizes.accelerationStructureSize,
           .type = VK_ACCELERATION_STRUCTURE_TYPE_BOTTOM_LEVEL_KHR,
         }, NULL, &r_rtx_scene.blas) == VK_SUCCESS &&
         R_Rtx_CreateBuffer(device, sizes.buildScratchSize,
                            VK_BUFFER_USAGE_STORAGE_BUFFER_BIT | VK_BUFFER_USAGE_SHADER_DEVICE_ADDRESS_BIT,
                            local, NULL, &scratch_buf, &scratch_mem, &scratch);
  }
  if (ok) {
    bgi.dstAccelerationStructure = r_rtx_scene.blas;
    bgi.scratchData.deviceAddress = scratch;
    VkAccelerationStructureBuildRangeInfoKHR range = { .primitiveCount = (uint32_t) num_tris };
    const VkAccelerationStructureBuildRangeInfoKHR *prange = &range;
    VkCommandBuffer command;
    ok = R_Rtx_BeginCommand(device, &command);
    if (ok) {
      r_rtx_scene.CmdBuildAccelerationStructures(command, 1, &bgi, &prange);
      ok = R_Rtx_EndCommand(device, command);
    }
  }

  if (ok) {
    const VkAccelerationStructureDeviceAddressInfoKHR address_info = {
      .sType = VK_STRUCTURE_TYPE_ACCELERATION_STRUCTURE_DEVICE_ADDRESS_INFO_KHR,
      .accelerationStructure = r_rtx_scene.blas,
    };
    VkAccelerationStructureInstanceKHR instance = {
      .transform = { .matrix = { { 1, 0, 0, 0 }, { 0, 1, 0, 0 }, { 0, 0, 1, 0 } } },
      .mask = 0xFF,
      .accelerationStructureReference = r_rtx_scene.GetAccelerationStructureDeviceAddress(device->device, &address_info),
    };
    ok = R_Rtx_CreateBuffer(device, sizeof(instance), as_input, host, &instance, &inst_buf, &inst_mem, &inst_addr);

    VkAccelerationStructureGeometryKHR tgeo = {
      .sType = VK_STRUCTURE_TYPE_ACCELERATION_STRUCTURE_GEOMETRY_KHR,
      .geometryType = VK_GEOMETRY_TYPE_INSTANCES_KHR,
      .flags = VK_GEOMETRY_OPAQUE_BIT_KHR,
      .geometry.instances = {
        .sType = VK_STRUCTURE_TYPE_ACCELERATION_STRUCTURE_GEOMETRY_INSTANCES_DATA_KHR,
        .data.deviceAddress = inst_addr,
      },
    };
    VkAccelerationStructureBuildGeometryInfoKHR tbgi = {
      .sType = VK_STRUCTURE_TYPE_ACCELERATION_STRUCTURE_BUILD_GEOMETRY_INFO_KHR,
      .type = VK_ACCELERATION_STRUCTURE_TYPE_TOP_LEVEL_KHR,
      .flags = VK_BUILD_ACCELERATION_STRUCTURE_PREFER_FAST_TRACE_BIT_KHR,
      .mode = VK_BUILD_ACCELERATION_STRUCTURE_MODE_BUILD_KHR,
      .geometryCount = 1,
      .pGeometries = &tgeo,
    };
    uint32_t one = 1;
    VkAccelerationStructureBuildSizesInfoKHR tsizes = { .sType = VK_STRUCTURE_TYPE_ACCELERATION_STRUCTURE_BUILD_SIZES_INFO_KHR };
    if (ok) {
      r_rtx_scene.GetBuildSizes(device->device, VK_ACCELERATION_STRUCTURE_BUILD_TYPE_DEVICE_KHR, &tbgi, &one, &tsizes);
      ok = R_Rtx_CreateBuffer(device, tsizes.accelerationStructureSize,
                              VK_BUFFER_USAGE_ACCELERATION_STRUCTURE_STORAGE_BIT_KHR | VK_BUFFER_USAGE_SHADER_DEVICE_ADDRESS_BIT,
                              local, NULL, &r_rtx_scene.tlas_buffer, &r_rtx_scene.tlas_memory, NULL) &&
           r_rtx_scene.CreateAccelerationStructure(device->device, &(VkAccelerationStructureCreateInfoKHR) {
             .sType = VK_STRUCTURE_TYPE_ACCELERATION_STRUCTURE_CREATE_INFO_KHR,
             .buffer = r_rtx_scene.tlas_buffer,
             .size = tsizes.accelerationStructureSize,
             .type = VK_ACCELERATION_STRUCTURE_TYPE_TOP_LEVEL_KHR,
           }, NULL, &r_rtx_scene.tlas) == VK_SUCCESS &&
           R_Rtx_CreateBuffer(device, tsizes.buildScratchSize,
                              VK_BUFFER_USAGE_STORAGE_BUFFER_BIT | VK_BUFFER_USAGE_SHADER_DEVICE_ADDRESS_BIT,
                              local, NULL, &tscratch_buf, &tscratch_mem, &tscratch);
    }
    if (ok) {
      tbgi.dstAccelerationStructure = r_rtx_scene.tlas;
      tbgi.scratchData.deviceAddress = tscratch;
      VkAccelerationStructureBuildRangeInfoKHR trange = { .primitiveCount = 1 };
      const VkAccelerationStructureBuildRangeInfoKHR *ptrange = &trange;
      VkCommandBuffer command;
      ok = R_Rtx_BeginCommand(device, &command);
      if (ok) {
        r_rtx_scene.CmdBuildAccelerationStructures(command, 1, &tbgi, &ptrange);
        ok = R_Rtx_EndCommand(device, command);
      }
    }
  }

  if (ok) {
    ok = R_Rtx_CreateBuffer(device, sizeof(r_rtx_tri_t) * num_tris, VK_BUFFER_USAGE_STORAGE_BUFFER_BIT,
                            host, tris, &r_rtx_scene.tri_buffer, &r_rtx_scene.tri_memory, NULL);
  }
  if (ok) {
    r_rtx_scene.num_lights = (uint32_t) (bsp->num_lights > 0 ? bsp->num_lights : 1);
    r_rtx_light_t *lights = Mem_TagMalloc(sizeof(r_rtx_light_t) * r_rtx_scene.num_lights, MEM_TAG_RENDERER);
    if (bsp->num_lights > 0) {
      for (int32_t i = 0; i < bsp->num_lights; i++) {
        const r_bsp_light_t *light = &bsp->lights[i];
        lights[i].origin_radius = Vec4(light->origin.x, light->origin.y, light->origin.z, light->radius);
        lights[i].color_intensity = Vec4(light->color.x, light->color.y, light->color.z, light->intensity);
      }
    } else {
      const vec3_t center = Vec3_Scale(Vec3_Add(r_rtx_scene.world_mins, r_rtx_scene.world_maxs), 0.5f);
      const float radius = Vec3_Distance(r_rtx_scene.world_maxs, r_rtx_scene.world_mins);
      lights[0].origin_radius = Vec4(center.x, center.y, r_rtx_scene.world_maxs.z, radius);
      lights[0].color_intensity = Vec4(1.f, 1.f, 1.f, 1.f);
    }
    ok = R_Rtx_CreateBuffer(device, sizeof(r_rtx_light_t) * r_rtx_scene.num_lights,
                            VK_BUFFER_USAGE_STORAGE_BUFFER_BIT, host, lights,
                            &r_rtx_scene.light_buffer, &r_rtx_scene.light_memory, NULL);
    Mem_Free(lights);
  }

  if (scratch_buf) { vkDestroyBuffer(device->device, scratch_buf, NULL); vkFreeMemory(device->device, scratch_mem, NULL); }
  if (tscratch_buf) { vkDestroyBuffer(device->device, tscratch_buf, NULL); vkFreeMemory(device->device, tscratch_mem, NULL); }
  if (vbuf) { vkDestroyBuffer(device->device, vbuf, NULL); vkFreeMemory(device->device, vmem, NULL); }
  if (ibuf) { vkDestroyBuffer(device->device, ibuf, NULL); vkFreeMemory(device->device, imem, NULL); }
  if (inst_buf) { vkDestroyBuffer(device->device, inst_buf, NULL); vkFreeMemory(device->device, inst_mem, NULL); }
  Mem_Free(positions);
  Mem_Free(elements);
  Mem_Free(tris);

  r_rtx_scene.world = world;
  if (ok) {
    Com_Print("RTX: built acceleration structures for %d triangles and %u lights\n", num_tris, r_rtx_scene.num_lights);
  }
  return ok;
}

static bool R_Rtx_CreateDescriptors(const r_rtx_device_t *device, const r_rtx_output_t *output) {
  const VkDescriptorSetLayoutBinding bindings[] = {
    { 0, VK_DESCRIPTOR_TYPE_ACCELERATION_STRUCTURE_KHR, 1, VK_SHADER_STAGE_RAYGEN_BIT_KHR | VK_SHADER_STAGE_CLOSEST_HIT_BIT_KHR, NULL },
    { 1, VK_DESCRIPTOR_TYPE_STORAGE_IMAGE, 1, VK_SHADER_STAGE_RAYGEN_BIT_KHR, NULL },
    { 2, VK_DESCRIPTOR_TYPE_STORAGE_BUFFER, 1, VK_SHADER_STAGE_CLOSEST_HIT_BIT_KHR, NULL },
    { 3, VK_DESCRIPTOR_TYPE_STORAGE_BUFFER, 1, VK_SHADER_STAGE_CLOSEST_HIT_BIT_KHR, NULL },
  };
  if (vkCreateDescriptorSetLayout(device->device, &(VkDescriptorSetLayoutCreateInfo) {
    .sType = VK_STRUCTURE_TYPE_DESCRIPTOR_SET_LAYOUT_CREATE_INFO,
    .bindingCount = lengthof(bindings),
    .pBindings = bindings,
  }, NULL, &r_rtx_scene.set_layout) != VK_SUCCESS) {
    return false;
  }

  const VkDescriptorPoolSize sizes[] = {
    { VK_DESCRIPTOR_TYPE_ACCELERATION_STRUCTURE_KHR, 1 },
    { VK_DESCRIPTOR_TYPE_STORAGE_IMAGE, 1 },
    { VK_DESCRIPTOR_TYPE_STORAGE_BUFFER, 2 },
  };
  if (vkCreateDescriptorPool(device->device, &(VkDescriptorPoolCreateInfo) {
    .sType = VK_STRUCTURE_TYPE_DESCRIPTOR_POOL_CREATE_INFO,
    .maxSets = 1,
    .poolSizeCount = lengthof(sizes),
    .pPoolSizes = sizes,
  }, NULL, &r_rtx_scene.descriptor_pool) != VK_SUCCESS) {
    return false;
  }

  if (vkAllocateDescriptorSets(device->device, &(VkDescriptorSetAllocateInfo) {
    .sType = VK_STRUCTURE_TYPE_DESCRIPTOR_SET_ALLOCATE_INFO,
    .descriptorPool = r_rtx_scene.descriptor_pool,
    .descriptorSetCount = 1,
    .pSetLayouts = &r_rtx_scene.set_layout,
  }, &r_rtx_scene.descriptor_set) != VK_SUCCESS) {
    return false;
  }

  const VkWriteDescriptorSetAccelerationStructureKHR acceleration = {
    .sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET_ACCELERATION_STRUCTURE_KHR,
    .accelerationStructureCount = 1,
    .pAccelerationStructures = &r_rtx_scene.tlas,
  };
  const VkDescriptorImageInfo image = {
    .imageView = output->image_view,
    .imageLayout = VK_IMAGE_LAYOUT_GENERAL,
  };
  const VkDescriptorBufferInfo tris = { .buffer = r_rtx_scene.tri_buffer, .range = VK_WHOLE_SIZE };
  const VkDescriptorBufferInfo lights = { .buffer = r_rtx_scene.light_buffer, .range = VK_WHOLE_SIZE };
  const VkWriteDescriptorSet writes[] = {
    { .sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET, .pNext = &acceleration, .dstSet = r_rtx_scene.descriptor_set,
      .dstBinding = 0, .descriptorCount = 1, .descriptorType = VK_DESCRIPTOR_TYPE_ACCELERATION_STRUCTURE_KHR },
    { .sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET, .dstSet = r_rtx_scene.descriptor_set,
      .dstBinding = 1, .descriptorCount = 1, .descriptorType = VK_DESCRIPTOR_TYPE_STORAGE_IMAGE, .pImageInfo = &image },
    { .sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET, .dstSet = r_rtx_scene.descriptor_set,
      .dstBinding = 2, .descriptorCount = 1, .descriptorType = VK_DESCRIPTOR_TYPE_STORAGE_BUFFER, .pBufferInfo = &tris },
    { .sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET, .dstSet = r_rtx_scene.descriptor_set,
      .dstBinding = 3, .descriptorCount = 1, .descriptorType = VK_DESCRIPTOR_TYPE_STORAGE_BUFFER, .pBufferInfo = &lights },
  };
  vkUpdateDescriptorSets(device->device, lengthof(writes), writes, 0, NULL);
  return true;
}

static bool R_Rtx_CreatePipeline(const r_rtx_device_t *device) {
  VkShaderModule rgen = R_Rtx_LoadShader(device, "shaders/rtx.rgen.spv");
  VkShaderModule rmiss = R_Rtx_LoadShader(device, "shaders/rtx.rmiss.spv");
  VkShaderModule smiss = R_Rtx_LoadShader(device, "shaders/rtx_shadow.rmiss.spv");
  VkShaderModule rchit = R_Rtx_LoadShader(device, "shaders/rtx.rchit.spv");
  if (!rgen || !rmiss || !smiss || !rchit) {
    return false;
  }

  const VkPipelineShaderStageCreateInfo stages[] = {
    { .sType = VK_STRUCTURE_TYPE_PIPELINE_SHADER_STAGE_CREATE_INFO, .stage = VK_SHADER_STAGE_RAYGEN_BIT_KHR, .module = rgen, .pName = "main" },
    { .sType = VK_STRUCTURE_TYPE_PIPELINE_SHADER_STAGE_CREATE_INFO, .stage = VK_SHADER_STAGE_MISS_BIT_KHR, .module = rmiss, .pName = "main" },
    { .sType = VK_STRUCTURE_TYPE_PIPELINE_SHADER_STAGE_CREATE_INFO, .stage = VK_SHADER_STAGE_MISS_BIT_KHR, .module = smiss, .pName = "main" },
    { .sType = VK_STRUCTURE_TYPE_PIPELINE_SHADER_STAGE_CREATE_INFO, .stage = VK_SHADER_STAGE_CLOSEST_HIT_BIT_KHR, .module = rchit, .pName = "main" },
  };
  const VkRayTracingShaderGroupCreateInfoKHR groups[] = {
    { .sType = VK_STRUCTURE_TYPE_RAY_TRACING_SHADER_GROUP_CREATE_INFO_KHR, .type = VK_RAY_TRACING_SHADER_GROUP_TYPE_GENERAL_KHR, .generalShader = 0, .closestHitShader = VK_SHADER_UNUSED_KHR, .anyHitShader = VK_SHADER_UNUSED_KHR, .intersectionShader = VK_SHADER_UNUSED_KHR },
    { .sType = VK_STRUCTURE_TYPE_RAY_TRACING_SHADER_GROUP_CREATE_INFO_KHR, .type = VK_RAY_TRACING_SHADER_GROUP_TYPE_GENERAL_KHR, .generalShader = 1, .closestHitShader = VK_SHADER_UNUSED_KHR, .anyHitShader = VK_SHADER_UNUSED_KHR, .intersectionShader = VK_SHADER_UNUSED_KHR },
    { .sType = VK_STRUCTURE_TYPE_RAY_TRACING_SHADER_GROUP_CREATE_INFO_KHR, .type = VK_RAY_TRACING_SHADER_GROUP_TYPE_GENERAL_KHR, .generalShader = 2, .closestHitShader = VK_SHADER_UNUSED_KHR, .anyHitShader = VK_SHADER_UNUSED_KHR, .intersectionShader = VK_SHADER_UNUSED_KHR },
    { .sType = VK_STRUCTURE_TYPE_RAY_TRACING_SHADER_GROUP_CREATE_INFO_KHR, .type = VK_RAY_TRACING_SHADER_GROUP_TYPE_TRIANGLES_HIT_GROUP_KHR, .generalShader = VK_SHADER_UNUSED_KHR, .closestHitShader = 3, .anyHitShader = VK_SHADER_UNUSED_KHR, .intersectionShader = VK_SHADER_UNUSED_KHR },
  };
  const VkPushConstantRange push = {
    .stageFlags = VK_SHADER_STAGE_RAYGEN_BIT_KHR | VK_SHADER_STAGE_CLOSEST_HIT_BIT_KHR,
    .offset = 0,
    .size = sizeof(r_rtx_push_t),
  };
  if (vkCreatePipelineLayout(device->device, &(VkPipelineLayoutCreateInfo) {
    .sType = VK_STRUCTURE_TYPE_PIPELINE_LAYOUT_CREATE_INFO,
    .setLayoutCount = 1,
    .pSetLayouts = &r_rtx_scene.set_layout,
    .pushConstantRangeCount = 1,
    .pPushConstantRanges = &push,
  }, NULL, &r_rtx_scene.pipeline_layout) != VK_SUCCESS) {
    return false;
  }

  const VkRayTracingPipelineCreateInfoKHR pipeline = {
    .sType = VK_STRUCTURE_TYPE_RAY_TRACING_PIPELINE_CREATE_INFO_KHR,
    .stageCount = lengthof(stages),
    .pStages = stages,
    .groupCount = lengthof(groups),
    .pGroups = groups,
    .maxPipelineRayRecursionDepth = 2,
    .layout = r_rtx_scene.pipeline_layout,
  };
  if (r_rtx_scene.CreateRayTracingPipelines(device->device, VK_NULL_HANDLE, VK_NULL_HANDLE,
                                            1, &pipeline, NULL, &r_rtx_scene.pipeline) != VK_SUCCESS) {
    return false;
  }
  vkDestroyShaderModule(device->device, rgen, NULL);
  vkDestroyShaderModule(device->device, rmiss, NULL);
  vkDestroyShaderModule(device->device, smiss, NULL);
  vkDestroyShaderModule(device->device, rchit, NULL);

  const uint32_t handle = r_rtx_scene.properties.shaderGroupHandleSize;
  const uint32_t base = r_rtx_scene.properties.shaderGroupBaseAlignment;
  const uint32_t stride = R_Rtx_Align(handle, r_rtx_scene.properties.shaderGroupHandleAlignment);
  uint8_t *handles = Mem_TagMalloc(handle * 4, MEM_TAG_RENDERER);
  if (r_rtx_scene.GetShaderGroupHandles(device->device, r_rtx_scene.pipeline, 0, 4, handle * 4, handles) != VK_SUCCESS) {
    Mem_Free(handles);
    return false;
  }

  const uint32_t rgen_off = 0;
  const uint32_t miss_off = R_Rtx_Align(stride, base);
  const uint32_t hit_off = miss_off + R_Rtx_Align(2 * stride, base);
  const uint32_t sbt_size = hit_off + R_Rtx_Align(stride, base);
  VkDeviceAddress sbt_addr;
  if (!R_Rtx_CreateBuffer(device, sbt_size,
                          VK_BUFFER_USAGE_SHADER_BINDING_TABLE_BIT_KHR | VK_BUFFER_USAGE_SHADER_DEVICE_ADDRESS_BIT,
                          VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT | VK_MEMORY_PROPERTY_HOST_COHERENT_BIT,
                          NULL, &r_rtx_scene.sbt_buffer, &r_rtx_scene.sbt_memory, &sbt_addr)) {
    Mem_Free(handles);
    return false;
  }

  uint8_t *sbt;
  if (vkMapMemory(device->device, r_rtx_scene.sbt_memory, 0, sbt_size, 0, (void **) &sbt) != VK_SUCCESS) {
    Mem_Free(handles);
    return false;
  }
  memcpy(sbt + rgen_off, handles + handle * 0, handle);
  memcpy(sbt + miss_off + stride * 0, handles + handle * 1, handle);
  memcpy(sbt + miss_off + stride * 1, handles + handle * 2, handle);
  memcpy(sbt + hit_off, handles + handle * 3, handle);
  vkUnmapMemory(device->device, r_rtx_scene.sbt_memory);
  Mem_Free(handles);

  r_rtx_scene.rgen_region = (VkStridedDeviceAddressRegionKHR) { sbt_addr + rgen_off, stride, stride };
  r_rtx_scene.miss_region = (VkStridedDeviceAddressRegionKHR) { sbt_addr + miss_off, stride, 2 * stride };
  r_rtx_scene.hit_region = (VkStridedDeviceAddressRegionKHR) { sbt_addr + hit_off, stride, stride };
  r_rtx_scene.call_region = (VkStridedDeviceAddressRegionKHR) { 0, 0, 0 };
  return true;
}

static bool R_Rtx_InitScene(const r_rtx_device_t *device, const r_rtx_output_t *output) {
  if (!R_Rtx_LoadProcs(device) || !R_Rtx_BuildWorld(device) ||
      !R_Rtx_CreateDescriptors(device, output) || !R_Rtx_CreatePipeline(device)) {
    R_Rtx_SceneShutdown(device);
    return false;
  }
  r_rtx_scene.output_image = output->image;
  r_rtx_scene.initialized = true;
  return true;
}

static void R_Rtx_Barrier(VkCommandBuffer command, VkImage image, VkImageLayout from, VkImageLayout to,
                          VkAccessFlags src, VkAccessFlags dst,
                          VkPipelineStageFlags src_stage, VkPipelineStageFlags dst_stage) {
  vkCmdPipelineBarrier(command, src_stage, dst_stage, 0, 0, NULL, 0, NULL, 1, &(VkImageMemoryBarrier) {
    .sType = VK_STRUCTURE_TYPE_IMAGE_MEMORY_BARRIER,
    .oldLayout = from,
    .newLayout = to,
    .srcQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED,
    .dstQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED,
    .image = image,
    .subresourceRange = { VK_IMAGE_ASPECT_COLOR_BIT, 0, 1, 0, 1 },
    .srcAccessMask = src,
    .dstAccessMask = dst,
  });
}

bool R_Rtx_SceneRender(const r_rtx_device_t *device, r_rtx_output_t *output, const r_view_t *view) {
  const r_model_t *world = R_WorldModel();
  if (!device || !device->device || !output || !output->image || !view || !world || !world->bsp) {
    return false;
  }
  if (!r_rtx_scene.initialized || r_rtx_scene.world != world || r_rtx_scene.output_image != output->image) {
    R_Rtx_SceneShutdown(device);
    if (!R_Rtx_InitScene(device, output)) {
      return false;
    }
  }

  const float aspect = (float) output->extent.width / (float) output->extent.height;
  vec3_t origin = view->origin;
  vec3_t target;
  vec2_t tan_fov;
  if (Vec3_Length(origin) < 1.f) {
    const vec3_t center = Vec3_Scale(Vec3_Add(r_rtx_scene.world_mins, r_rtx_scene.world_maxs), 0.5f);
    const vec3_t size = Vec3_Subtract(r_rtx_scene.world_maxs, r_rtx_scene.world_mins);
    origin = Vec3(center.x, center.y - size.y * 0.5f, r_rtx_scene.world_maxs.z + size.z * 0.6f);
    target = center;
    tan_fov = Vec2(0.55f * aspect, 0.55f);
  } else {
    vec3_t forward, right, up;
    Vec3_Vectors(view->angles, &forward, &right, &up);
    target = Vec3_Add(origin, forward);
    const float tx = tanf(Radians(view->fov.x) * .5f);
    tan_fov = Vec2(tx, tx / aspect);
  }

  if (Vec3_Distance(origin, r_rtx_scene.last_eye) > 0.1f ||
      Vec3_Distance(target, r_rtx_scene.last_target) > 0.1f) {
    r_rtx_scene.accum_frame = 0;
  } else {
    r_rtx_scene.accum_frame++;
  }
  r_rtx_scene.last_eye = origin;
  r_rtx_scene.last_target = target;

  const r_rtx_push_t push = {
    .eye = Vec4(origin.x, origin.y, origin.z, 0.f),
    .target = Vec4(target.x, target.y, target.z, 0.f),
    .light = Vec4(origin.x, origin.y, origin.z + 512.f, 0.f),
    .params = Vec4(tan_fov.x, tan_fov.y, (float) r_rtx_scene.num_lights, (float) r_rtx_scene.accum_frame),
  };

  VkCommandBuffer command;
  if (!R_Rtx_BeginCommand(device, &command)) {
    return false;
  }
  R_Rtx_Barrier(command, output->image, output->layout, VK_IMAGE_LAYOUT_GENERAL,
                output->layout == VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL ? VK_ACCESS_TRANSFER_READ_BIT : 0,
                VK_ACCESS_SHADER_WRITE_BIT,
                output->layout == VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL ? VK_PIPELINE_STAGE_TRANSFER_BIT : VK_PIPELINE_STAGE_TOP_OF_PIPE_BIT,
                VK_PIPELINE_STAGE_RAY_TRACING_SHADER_BIT_KHR);
  vkCmdBindPipeline(command, VK_PIPELINE_BIND_POINT_RAY_TRACING_KHR, r_rtx_scene.pipeline);
  vkCmdBindDescriptorSets(command, VK_PIPELINE_BIND_POINT_RAY_TRACING_KHR, r_rtx_scene.pipeline_layout, 0, 1,
                          &r_rtx_scene.descriptor_set, 0, NULL);
  vkCmdPushConstants(command, r_rtx_scene.pipeline_layout,
                     VK_SHADER_STAGE_RAYGEN_BIT_KHR | VK_SHADER_STAGE_CLOSEST_HIT_BIT_KHR,
                     0, sizeof(push), &push);
  r_rtx_scene.CmdTraceRays(command, &r_rtx_scene.rgen_region, &r_rtx_scene.miss_region,
                           &r_rtx_scene.hit_region, &r_rtx_scene.call_region,
                           output->extent.width, output->extent.height, 1);
  R_Rtx_OutputReadback(device, output, command);
  return R_Rtx_EndCommand(device, command);
}

void R_Rtx_SceneShutdown(const r_rtx_device_t *device) {
  if (!device || !device->device) {
    memset(&r_rtx_scene, 0, sizeof(r_rtx_scene));
    return;
  }

  vkDeviceWaitIdle(device->device);
  if (r_rtx_scene.pipeline) vkDestroyPipeline(device->device, r_rtx_scene.pipeline, NULL);
  if (r_rtx_scene.pipeline_layout) vkDestroyPipelineLayout(device->device, r_rtx_scene.pipeline_layout, NULL);
  if (r_rtx_scene.descriptor_pool) vkDestroyDescriptorPool(device->device, r_rtx_scene.descriptor_pool, NULL);
  if (r_rtx_scene.set_layout) vkDestroyDescriptorSetLayout(device->device, r_rtx_scene.set_layout, NULL);
  if (r_rtx_scene.sbt_buffer) { vkDestroyBuffer(device->device, r_rtx_scene.sbt_buffer, NULL); vkFreeMemory(device->device, r_rtx_scene.sbt_memory, NULL); }
  if (r_rtx_scene.tri_buffer) { vkDestroyBuffer(device->device, r_rtx_scene.tri_buffer, NULL); vkFreeMemory(device->device, r_rtx_scene.tri_memory, NULL); }
  if (r_rtx_scene.light_buffer) { vkDestroyBuffer(device->device, r_rtx_scene.light_buffer, NULL); vkFreeMemory(device->device, r_rtx_scene.light_memory, NULL); }
  if (r_rtx_scene.tlas && r_rtx_scene.DestroyAccelerationStructure) r_rtx_scene.DestroyAccelerationStructure(device->device, r_rtx_scene.tlas, NULL);
  if (r_rtx_scene.tlas_buffer) { vkDestroyBuffer(device->device, r_rtx_scene.tlas_buffer, NULL); vkFreeMemory(device->device, r_rtx_scene.tlas_memory, NULL); }
  if (r_rtx_scene.blas && r_rtx_scene.DestroyAccelerationStructure) r_rtx_scene.DestroyAccelerationStructure(device->device, r_rtx_scene.blas, NULL);
  if (r_rtx_scene.blas_buffer) { vkDestroyBuffer(device->device, r_rtx_scene.blas_buffer, NULL); vkFreeMemory(device->device, r_rtx_scene.blas_memory, NULL); }
  memset(&r_rtx_scene, 0, sizeof(r_rtx_scene));
}

#endif
