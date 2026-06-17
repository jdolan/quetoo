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
 * @brief In-engine hardware ray tracing (RTX). Builds acceleration structures from
 * the loaded BSP world geometry, runs a ray-tracing pipeline from the player's view,
 * and presents the ray-traced image through the Vulkan swapchain. The technique is
 * validated standalone in vulkan-validation/ (see VULKAN_RTX.md phase 4); this is its
 * integration into the running renderer.
 */

#define R_RTX_HIT_BIAS 1.0f

/**
 * @brief Per-triangle shading data uploaded to the closest-hit shader.
 */
typedef struct {
  vec4_t normal;
  vec4_t albedo;
} r_rtx_tri_t;

/**
 * @brief A light uploaded for the closest-hit shader: xyz = origin, w = radius;
 * rgb = color, a = intensity.
 */
typedef struct {
  vec4_t origin_radius;
  vec4_t color_intensity;
} r_rtx_light_t;

/**
 * @brief Push constants matching shaders/rtx.*. The camera basis is derived in the
 * raygen shader from eye + target (Z-up), so only the eye, look-at target, light and
 * field-of-view tangents are uploaded.
 */
typedef struct {
  vec4_t eye;
  vec4_t target;
  vec4_t light;
  vec4_t params; // x,y = tan(fov/2) including aspect
} r_rtx_push_t;

static struct {
  _Bool initialized;
  _Bool world_built;

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

  VkImage image;
  VkDeviceMemory image_memory;
  VkImageView image_view;
  VkExtent2D extent;

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

  // world bounds, used to frame a fallback camera when no valid player view exists
  vec3_t world_mins;
  vec3_t world_maxs;

  // temporal accumulation state: the sample count resets to 0 whenever the camera
  // moves, then climbs each frame so jittered soft shadows / anti-aliasing converge
  vec3_t last_eye;
  vec3_t last_target;
  uint32_t accum_frame;
} r_rtx;

/**
 * @brief Finds a memory type index with the requested properties.
 */
static uint32_t R_Rtx_FindMemoryType(uint32_t bits, VkMemoryPropertyFlags want) {

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
 * @brief Allocates a buffer, optionally uploading `data` and resolving its device address.
 */
static VkDeviceAddress R_Rtx_CreateBuffer(VkDeviceSize size, VkBufferUsageFlags usage,
                                          VkMemoryPropertyFlags props, const void *data,
                                          VkBuffer *out_buffer, VkDeviceMemory *out_memory) {

  const VkBufferCreateInfo ci = {
    .sType = VK_STRUCTURE_TYPE_BUFFER_CREATE_INFO,
    .size = size,
    .usage = usage,
    .sharingMode = VK_SHARING_MODE_EXCLUSIVE
  };

  VkBuffer buffer;
  if (vkCreateBuffer(r_vk.device, &ci, NULL, &buffer) != VK_SUCCESS) {
    Com_Error(ERROR_FATAL, "vkCreateBuffer failed\n");
  }

  VkMemoryRequirements req;
  vkGetBufferMemoryRequirements(r_vk.device, buffer, &req);

  VkMemoryAllocateFlagsInfo flags = {
    .sType = VK_STRUCTURE_TYPE_MEMORY_ALLOCATE_FLAGS_INFO,
    .flags = VK_MEMORY_ALLOCATE_DEVICE_ADDRESS_BIT
  };

  const VkMemoryAllocateInfo ai = {
    .sType = VK_STRUCTURE_TYPE_MEMORY_ALLOCATE_INFO,
    .pNext = &flags,
    .allocationSize = req.size,
    .memoryTypeIndex = R_Rtx_FindMemoryType(req.memoryTypeBits, props)
  };

  VkDeviceMemory memory;
  if (vkAllocateMemory(r_vk.device, &ai, NULL, &memory) != VK_SUCCESS) {
    Com_Error(ERROR_FATAL, "vkAllocateMemory failed\n");
  }

  vkBindBufferMemory(r_vk.device, buffer, memory, 0);

  if (data) {
    void *p;
    vkMapMemory(r_vk.device, memory, 0, size, 0, &p);
    memcpy(p, data, size);
    vkUnmapMemory(r_vk.device, memory);
  }

  VkDeviceAddress address = 0;
  if (usage & VK_BUFFER_USAGE_SHADER_DEVICE_ADDRESS_BIT) {
    const VkBufferDeviceAddressInfo bi = {
      .sType = VK_STRUCTURE_TYPE_BUFFER_DEVICE_ADDRESS_INFO,
      .buffer = buffer
    };
    address = r_rtx.GetBufferDeviceAddress(r_vk.device, &bi);
  }

  *out_buffer = buffer;
  *out_memory = memory;
  return address;
}

/**
 * @brief Allocates and begins a one-shot command buffer.
 */
static VkCommandBuffer R_Rtx_BeginCommand(void) {

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
static void R_Rtx_EndCommand(VkCommandBuffer cb) {

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
 * @brief Loads a SPIR-V module from the filesystem.
 */
static VkShaderModule R_Rtx_LoadShader(const char *name) {

  void *buffer;
  const int64_t len = Fs_Load(name, &buffer);
  if (len == -1) {
    Com_Error(ERROR_FATAL, "Failed to load RTX shader %s\n", name);
  }

  const VkShaderModuleCreateInfo ci = {
    .sType = VK_STRUCTURE_TYPE_SHADER_MODULE_CREATE_INFO,
    .codeSize = (size_t) len,
    .pCode = (const uint32_t *) buffer
  };

  VkShaderModule module;
  if (vkCreateShaderModule(r_vk.device, &ci, NULL, &module) != VK_SUCCESS) {
    Com_Error(ERROR_FATAL, "vkCreateShaderModule failed for %s\n", name);
  }

  Fs_Free(buffer);
  return module;
}

static uint32_t R_Rtx_Align(uint32_t value, uint32_t alignment) {
  return (value + alignment - 1) & ~(alignment - 1);
}

/**
 * @brief Loads the ray tracing entry points and pipeline properties.
 */
static void R_Rtx_LoadProcs(void) {

#define R_RTX_PROC(field, api) \
  r_rtx.field = (PFN_##api) vkGetDeviceProcAddr(r_vk.device, #api); \
  if (!r_rtx.field) { Com_Error(ERROR_FATAL, "Failed to load %s\n", #api); }

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

  r_rtx.properties.sType = VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_RAY_TRACING_PIPELINE_PROPERTIES_KHR;
  VkPhysicalDeviceProperties2 p2 = {
    .sType = VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_PROPERTIES_2,
    .pNext = &r_rtx.properties
  };
  vkGetPhysicalDeviceProperties2(r_vk.physical_device, &p2);
}

/**
 * @brief Computes a representative (average) diffuse color for a material so RTX
 * surfaces are tinted by their actual texture. Full per-pixel sampling is future work.
 */
static vec3_t R_Rtx_MaterialColor(const r_material_t *material) {

  const vec3_t fallback = Vec3(0.6f, 0.6f, 0.6f);
  if (!material || !material->cm || !*material->cm->diffusemap.path) {
    return fallback;
  }

  SDL_Surface *surface = Img_LoadSurface(material->cm->diffusemap.path);
  if (!surface) {
    return fallback;
  }

  SDL_Surface *rgba = SDL_ConvertSurface(surface, SDL_PIXELFORMAT_RGBA32);
  SDL_DestroySurface(surface);
  if (!rgba) {
    return fallback;
  }

  uint64_t r = 0, g = 0, b = 0, n = 0;
  const uint8_t *pixels = rgba->pixels;
  for (int32_t y = 0; y < rgba->h; y++) {
    const uint8_t *row = pixels + (size_t) y * rgba->pitch;
    for (int32_t x = 0; x < rgba->w; x++) {
      const uint8_t *p = row + x * 4;
      if (p[3] < 8) {
        continue;
      }
      r += p[0]; g += p[1]; b += p[2]; n++;
    }
  }
  SDL_DestroySurface(rgba);

  if (n == 0) {
    return fallback;
  }
  return Vec3((float) r / n / 255.f, (float) g / n / 255.f, (float) b / n / 255.f);
}

/**
 * @brief Appends a mesh model's bind-pose (frame 0) geometry into the world geometry
 * arrays, translated to @p origin, so it is included in the ray-traced scene. The
 * arrays are grown in place; their counts are updated through the in/out pointers.
 * @details Mesh vertices are frame-major, so frame 0 is simply the first
 * `face->num_vertexes` entries of each face. Face elements are local to the face, so
 * they are rebased onto the appended vertices. Each face's triangles are tinted by
 * that face's material diffuse color.
 */
static void R_Rtx_AppendMesh(const char *name, const vec3_t origin, const float scale,
                             vec3_t **positions, uint32_t **elements, r_rtx_tri_t **tris,
                             int32_t *num_vertexes, int32_t *num_elements, int32_t *num_tris) {

  r_model_t *mod = R_LoadModel(name);
  if (!mod || !mod->mesh || mod->mesh->num_faces <= 0) {
    return;
  }

  const r_mesh_model_t *mesh = mod->mesh;

  int32_t add_v = 0, add_e = 0;
  for (int32_t i = 0; i < mesh->num_faces; i++) {
    add_v += mesh->faces[i].num_vertexes;
    add_e += mesh->faces[i].num_elements;
  }
  if (add_v <= 0 || add_e < 3) {
    return;
  }

  const int32_t base_v = *num_vertexes;
  const int32_t base_e = *num_elements;
  const int32_t base_t = *num_tris;

  *positions = Mem_Realloc(*positions, sizeof(vec3_t) * (base_v + add_v));
  *elements = Mem_Realloc(*elements, sizeof(uint32_t) * (base_e + add_e));
  *tris = Mem_Realloc(*tris, sizeof(r_rtx_tri_t) * (base_t + add_e / 3));

  vec3_t *pos = *positions;
  uint32_t *elem = *elements;
  r_rtx_tri_t *tri = *tris;

  int32_t vo = base_v, eo = base_e;
  for (int32_t i = 0; i < mesh->num_faces; i++) {
    const r_mesh_face_t *face = &mesh->faces[i];
    const vec3_t albedo = R_Rtx_MaterialColor(face->material);

    for (int32_t k = 0; k < face->num_vertexes; k++) {
      pos[vo + k] = Vec3_Add(Vec3_Scale(face->vertexes[k].position, scale), origin);
    }

    for (int32_t j = 0; j + 2 < face->num_elements; j += 3) {
      const uint32_t a = vo + face->elements[j + 0];
      const uint32_t b = vo + face->elements[j + 1];
      const uint32_t c = vo + face->elements[j + 2];
      elem[eo + 0] = a;
      elem[eo + 1] = b;
      elem[eo + 2] = c;
      const vec3_t n = Vec3_Normalize(Vec3_Cross(Vec3_Subtract(pos[b], pos[a]),
                                                 Vec3_Subtract(pos[c], pos[a])));
      const int32_t t = eo / 3;
      tri[t].normal = Vec4(n.x, n.y, n.z, 0.f);
      tri[t].albedo = Vec4(albedo.x, albedo.y, albedo.z, 0.f);
      eo += 3;
    }

    vo += face->num_vertexes;
  }

  *num_vertexes = vo;
  *num_elements = eo;
  *num_tris = eo / 3;

  Com_Print("  RTX: appended mesh %s (%d verts, %d tris) at %.0f %.0f %.0f\n",
            name, add_v, add_e / 3, origin.x, origin.y, origin.z);
}

/**
 * @brief Builds the bottom- and top-level acceleration structures from the loaded BSP
 * world geometry, plus the per-triangle shading buffer.
 */
static void R_Rtx_BuildWorld(void) {

  // source world geometry from the renderer's loaded BSP model. Its vertex/element
  // arrays persist for the map's lifetime, unlike the collision file lumps which are
  // unloaded after R_LoadBspModel.
  const r_model_t *world = R_WorldModel();
  if (!world || !world->bsp) {
    return;
  }

  const r_bsp_model_t *bsp = world->bsp;
  int32_t num_vertexes = bsp->num_vertexes;

  if (num_vertexes <= 0 || bsp->num_elements < 3) {
    return;
  }

  // tightly-packed positions for the acceleration structure, tracking world bounds
  vec3_t *positions = Mem_Malloc(sizeof(vec3_t) * num_vertexes);
  r_rtx.world_mins = Vec3_Mins();
  r_rtx.world_maxs = Vec3_Maxs();
  for (int32_t i = 0; i < num_vertexes; i++) {
    positions[i] = bsp->vertexes[i].position;
    r_rtx.world_mins = Vec3_Minf(r_rtx.world_mins, positions[i]);
    r_rtx.world_maxs = Vec3_Maxf(r_rtx.world_maxs, positions[i]);
  }

  // average each material's diffuse texture once so every triangle can be tinted
  vec3_t *mat_color = NULL;
  if (bsp->num_materials > 0) {
    mat_color = Mem_Malloc(sizeof(vec3_t) * bsp->num_materials);
    for (int32_t i = 0; i < bsp->num_materials; i++) {
      mat_color[i] = R_Rtx_MaterialColor(bsp->materials[i]);

      // upload the diffuse texture into the bindless array; the closest-hit
      // shader samples it for real per-pixel texturing (phase 3)
      if (bsp->materials[i] && bsp->materials[i]->cm) {
        R_Vk_LoadTexture(bsp->materials[i]->cm->diffusemap.path);
      }
    }
    Com_Print("  RTX: uploaded %u Vulkan textures to the bindless array\n", R_Vk_TextureCount());
  }

  // count the elements covered by draw elements (opaque faces batched per material)
  int32_t draw_elements_count = 0;
  for (int32_t d = 0; d < bsp->num_draw_elements; d++) {
    draw_elements_count += bsp->draw_elements[d].num_elements;
  }

  uint32_t *elements;
  r_rtx_tri_t *tris;
  int32_t num_elements;
  int32_t num_tris;

  if (draw_elements_count >= 3) {
    // build the ray-traced geometry from the draw elements: this is exactly what the
    // rasterizer draws, and each batch carries its material, so every triangle gets
    // its real (averaged) diffuse color.
    elements = Mem_Malloc(sizeof(uint32_t) * draw_elements_count);
    tris = Mem_Malloc(sizeof(r_rtx_tri_t) * (draw_elements_count / 3));
    int32_t e_out = 0;
    for (int32_t d = 0; d < bsp->num_draw_elements; d++) {
      const r_bsp_draw_elements_t *de = &bsp->draw_elements[d];
      vec3_t albedo = Vec3(0.6f, 0.6f, 0.6f);
      if (mat_color) {
        for (int32_t i = 0; i < bsp->num_materials; i++) {
          if (bsp->materials[i] == de->material) {
            albedo = mat_color[i];
            break;
          }
        }
      }
      const uint32_t first = (uint32_t) ((uintptr_t) de->elements / sizeof(uint32_t));
      for (int32_t j = 0; j + 2 < de->num_elements; j += 3) {
        const uint32_t i0 = (uint32_t) bsp->elements[first + j + 0];
        const uint32_t i1 = (uint32_t) bsp->elements[first + j + 1];
        const uint32_t i2 = (uint32_t) bsp->elements[first + j + 2];
        elements[e_out + 0] = i0;
        elements[e_out + 1] = i1;
        elements[e_out + 2] = i2;
        const vec3_t n = Vec3_Normalize(Vec3_Cross(Vec3_Subtract(positions[i1], positions[i0]),
                                                   Vec3_Subtract(positions[i2], positions[i0])));
        const int32_t t = e_out / 3;
        tris[t].normal = Vec4(n.x, n.y, n.z, 0.f);
        tris[t].albedo = Vec4(albedo.x, albedo.y, albedo.z, 0.f);
        e_out += 3;
      }
    }
    num_elements = e_out;
    num_tris = num_elements / 3;
  } else {
    // fallback: trace the full element array with an orientation tint
    num_elements = bsp->num_elements;
    num_tris = num_elements / 3;
    elements = Mem_Malloc(sizeof(uint32_t) * num_elements);
    for (int32_t i = 0; i < num_elements; i++) {
      elements[i] = (uint32_t) bsp->elements[i];
    }
    tris = Mem_Malloc(sizeof(r_rtx_tri_t) * num_tris);
    for (int32_t t = 0; t < num_tris; t++) {
      const vec3_t a = positions[elements[t * 3 + 0]];
      const vec3_t b = positions[elements[t * 3 + 1]];
      const vec3_t c = positions[elements[t * 3 + 2]];
      const vec3_t n = Vec3_Normalize(Vec3_Cross(Vec3_Subtract(b, a), Vec3_Subtract(c, a)));
      tris[t].normal = Vec4(n.x, n.y, n.z, 0.f);
      tris[t].albedo = Vec4(0.45f + 0.45f * fabsf(n.x),
                            0.45f + 0.45f * fabsf(n.y),
                            0.45f + 0.45f * fabsf(n.z), 0.f);
    }
  }

  if (mat_color) {
    Mem_Free(mat_color);
  }

  Com_Print("  RTX: geometry %d tris from %d draw elements, %d materials\n",
            num_tris, bsp->num_draw_elements, bsp->num_materials);

  // place mesh models into the ray-traced scene. With no live entity feed under the
  // Vulkan path yet, drop the qforcer player model at the center of the world so the
  // mesh-in-acceleration-structure path is exercised end to end.
  if (r_rtx_test_model->string[0]) {
    const vec3_t center = Vec3_Scale(Vec3_Add(r_rtx.world_mins, r_rtx.world_maxs), 0.5f);
    const float size_z = r_rtx.world_maxs.z - r_rtx.world_mins.z;
    // float it above the rooftops so the bounds-overview camera always has a clear,
    // unoccluded line of sight to it
    const vec3_t where = Vec3(center.x, center.y, r_rtx.world_maxs.z + size_z * 0.15f);
    R_Rtx_AppendMesh(r_rtx_test_model->string, where, r_rtx_test_scale->value,
                     &positions, &elements, &tris, &num_vertexes, &num_elements, &num_tris);
  }

  const VkMemoryPropertyFlags host = VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT | VK_MEMORY_PROPERTY_HOST_COHERENT_BIT;
  const VkMemoryPropertyFlags local = VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT;
  const VkBufferUsageFlags as_input = VK_BUFFER_USAGE_ACCELERATION_STRUCTURE_BUILD_INPUT_READ_ONLY_BIT_KHR |
                                      VK_BUFFER_USAGE_SHADER_DEVICE_ADDRESS_BIT;

  VkBuffer vbuf, ibuf, scratch_buf, inst_buf, tscratch_buf;
  VkDeviceMemory vmem, imem, scratch_mem, inst_mem, tscratch_mem;

  const VkDeviceAddress vaddr = R_Rtx_CreateBuffer(sizeof(vec3_t) * num_vertexes, as_input, host, positions, &vbuf, &vmem);
  const VkDeviceAddress iaddr = R_Rtx_CreateBuffer(sizeof(uint32_t) * num_elements, as_input, host, elements, &ibuf, &imem);

  // bottom-level
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
      .indexData.deviceAddress = iaddr
    }
  };

  VkAccelerationStructureBuildGeometryInfoKHR bgi = {
    .sType = VK_STRUCTURE_TYPE_ACCELERATION_STRUCTURE_BUILD_GEOMETRY_INFO_KHR,
    .type = VK_ACCELERATION_STRUCTURE_TYPE_BOTTOM_LEVEL_KHR,
    .flags = VK_BUILD_ACCELERATION_STRUCTURE_PREFER_FAST_TRACE_BIT_KHR,
    .mode = VK_BUILD_ACCELERATION_STRUCTURE_MODE_BUILD_KHR,
    .geometryCount = 1,
    .pGeometries = &geo
  };

  uint32_t prim_count = (uint32_t) num_tris;
  VkAccelerationStructureBuildSizesInfoKHR sizes = {
    .sType = VK_STRUCTURE_TYPE_ACCELERATION_STRUCTURE_BUILD_SIZES_INFO_KHR
  };
  r_rtx.GetBuildSizes(r_vk.device, VK_ACCELERATION_STRUCTURE_BUILD_TYPE_DEVICE_KHR, &bgi, &prim_count, &sizes);

  R_Rtx_CreateBuffer(sizes.accelerationStructureSize,
    VK_BUFFER_USAGE_ACCELERATION_STRUCTURE_STORAGE_BIT_KHR | VK_BUFFER_USAGE_SHADER_DEVICE_ADDRESS_BIT,
    local, NULL, &r_rtx.blas_buffer, &r_rtx.blas_memory);

  VkAccelerationStructureCreateInfoKHR aci = {
    .sType = VK_STRUCTURE_TYPE_ACCELERATION_STRUCTURE_CREATE_INFO_KHR,
    .buffer = r_rtx.blas_buffer,
    .size = sizes.accelerationStructureSize,
    .type = VK_ACCELERATION_STRUCTURE_TYPE_BOTTOM_LEVEL_KHR
  };
  r_rtx.CreateAccelerationStructure(r_vk.device, &aci, NULL, &r_rtx.blas);

  const VkDeviceAddress scratch = R_Rtx_CreateBuffer(sizes.buildScratchSize,
    VK_BUFFER_USAGE_STORAGE_BUFFER_BIT | VK_BUFFER_USAGE_SHADER_DEVICE_ADDRESS_BIT,
    local, NULL, &scratch_buf, &scratch_mem);

  bgi.dstAccelerationStructure = r_rtx.blas;
  bgi.scratchData.deviceAddress = scratch;

  VkAccelerationStructureBuildRangeInfoKHR range = { .primitiveCount = (uint32_t) num_tris };
  const VkAccelerationStructureBuildRangeInfoKHR *prange = &range;

  VkCommandBuffer cb = R_Rtx_BeginCommand();
  r_rtx.CmdBuildAccelerationStructures(cb, 1, &bgi, &prange);
  R_Rtx_EndCommand(cb);

  const VkAccelerationStructureDeviceAddressInfoKHR dai = {
    .sType = VK_STRUCTURE_TYPE_ACCELERATION_STRUCTURE_DEVICE_ADDRESS_INFO_KHR,
    .accelerationStructure = r_rtx.blas
  };
  const VkDeviceAddress blas_address = r_rtx.GetAccelerationStructureDeviceAddress(r_vk.device, &dai);

  // top-level (single identity instance)
  VkAccelerationStructureInstanceKHR instance = {
    .transform = { .matrix = { { 1, 0, 0, 0 }, { 0, 1, 0, 0 }, { 0, 0, 1, 0 } } },
    .mask = 0xFF,
    .accelerationStructureReference = blas_address
  };
  const VkDeviceAddress inst_addr = R_Rtx_CreateBuffer(sizeof(instance), as_input, host, &instance, &inst_buf, &inst_mem);

  VkAccelerationStructureGeometryKHR tgeo = {
    .sType = VK_STRUCTURE_TYPE_ACCELERATION_STRUCTURE_GEOMETRY_KHR,
    .geometryType = VK_GEOMETRY_TYPE_INSTANCES_KHR,
    .flags = VK_GEOMETRY_OPAQUE_BIT_KHR,
    .geometry.instances = {
      .sType = VK_STRUCTURE_TYPE_ACCELERATION_STRUCTURE_GEOMETRY_INSTANCES_DATA_KHR,
      .data.deviceAddress = inst_addr
    }
  };
  VkAccelerationStructureBuildGeometryInfoKHR tbgi = {
    .sType = VK_STRUCTURE_TYPE_ACCELERATION_STRUCTURE_BUILD_GEOMETRY_INFO_KHR,
    .type = VK_ACCELERATION_STRUCTURE_TYPE_TOP_LEVEL_KHR,
    .flags = VK_BUILD_ACCELERATION_STRUCTURE_PREFER_FAST_TRACE_BIT_KHR,
    .mode = VK_BUILD_ACCELERATION_STRUCTURE_MODE_BUILD_KHR,
    .geometryCount = 1,
    .pGeometries = &tgeo
  };
  uint32_t one = 1;
  VkAccelerationStructureBuildSizesInfoKHR tsizes = {
    .sType = VK_STRUCTURE_TYPE_ACCELERATION_STRUCTURE_BUILD_SIZES_INFO_KHR
  };
  r_rtx.GetBuildSizes(r_vk.device, VK_ACCELERATION_STRUCTURE_BUILD_TYPE_DEVICE_KHR, &tbgi, &one, &tsizes);

  R_Rtx_CreateBuffer(tsizes.accelerationStructureSize,
    VK_BUFFER_USAGE_ACCELERATION_STRUCTURE_STORAGE_BIT_KHR | VK_BUFFER_USAGE_SHADER_DEVICE_ADDRESS_BIT,
    local, NULL, &r_rtx.tlas_buffer, &r_rtx.tlas_memory);

  VkAccelerationStructureCreateInfoKHR taci = {
    .sType = VK_STRUCTURE_TYPE_ACCELERATION_STRUCTURE_CREATE_INFO_KHR,
    .buffer = r_rtx.tlas_buffer,
    .size = tsizes.accelerationStructureSize,
    .type = VK_ACCELERATION_STRUCTURE_TYPE_TOP_LEVEL_KHR
  };
  r_rtx.CreateAccelerationStructure(r_vk.device, &taci, NULL, &r_rtx.tlas);

  const VkDeviceAddress tscratch = R_Rtx_CreateBuffer(tsizes.buildScratchSize,
    VK_BUFFER_USAGE_STORAGE_BUFFER_BIT | VK_BUFFER_USAGE_SHADER_DEVICE_ADDRESS_BIT,
    local, NULL, &tscratch_buf, &tscratch_mem);

  tbgi.dstAccelerationStructure = r_rtx.tlas;
  tbgi.scratchData.deviceAddress = tscratch;
  VkAccelerationStructureBuildRangeInfoKHR trange = { .primitiveCount = 1 };
  const VkAccelerationStructureBuildRangeInfoKHR *ptrange = &trange;

  cb = R_Rtx_BeginCommand();
  r_rtx.CmdBuildAccelerationStructures(cb, 1, &tbgi, &ptrange);
  R_Rtx_EndCommand(cb);

  R_Rtx_CreateBuffer(sizeof(r_rtx_tri_t) * num_tris, VK_BUFFER_USAGE_STORAGE_BUFFER_BIT,
    host, tris, &r_rtx.tri_buffer, &r_rtx.tri_memory);

  // upload the map's lights for the closest-hit shader. Always allocate at least one
  // entry (a sky-ish fill light) so the buffer/descriptor is valid even on maps with
  // no compiled lights.
  r_rtx.num_lights = (uint32_t) (world->bsp->num_lights > 0 ? world->bsp->num_lights : 1);
  r_rtx_light_t *lights = Mem_Malloc(sizeof(r_rtx_light_t) * r_rtx.num_lights);
  if (world->bsp->num_lights > 0) {
    for (int32_t i = 0; i < world->bsp->num_lights; i++) {
      const r_bsp_light_t *l = &world->bsp->lights[i];
      lights[i].origin_radius = Vec4(l->origin.x, l->origin.y, l->origin.z, l->radius);
      lights[i].color_intensity = Vec4(l->color.x, l->color.y, l->color.z, l->intensity);
    }
  } else {
    const vec3_t c = Vec3_Scale(Vec3_Add(r_rtx.world_mins, r_rtx.world_maxs), 0.5f);
    const float r = Vec3_Distance(r_rtx.world_maxs, r_rtx.world_mins);
    lights[0].origin_radius = Vec4(c.x, c.y, r_rtx.world_maxs.z, r);
    lights[0].color_intensity = Vec4(1.f, 1.f, 1.f, 1.f);
  }
  R_Rtx_CreateBuffer(sizeof(r_rtx_light_t) * r_rtx.num_lights, VK_BUFFER_USAGE_STORAGE_BUFFER_BIT,
    host, lights, &r_rtx.light_buffer, &r_rtx.light_memory);
  Mem_Free(lights);
  Com_Print("  RTX: %u lights\n", r_rtx.num_lights);

  // scratch and input buffers are no longer needed
  vkDestroyBuffer(r_vk.device, scratch_buf, NULL); vkFreeMemory(r_vk.device, scratch_mem, NULL);
  vkDestroyBuffer(r_vk.device, tscratch_buf, NULL); vkFreeMemory(r_vk.device, tscratch_mem, NULL);
  vkDestroyBuffer(r_vk.device, vbuf, NULL); vkFreeMemory(r_vk.device, vmem, NULL);
  vkDestroyBuffer(r_vk.device, ibuf, NULL); vkFreeMemory(r_vk.device, imem, NULL);
  vkDestroyBuffer(r_vk.device, inst_buf, NULL); vkFreeMemory(r_vk.device, inst_mem, NULL);

  Mem_Free(positions);
  Mem_Free(elements);
  Mem_Free(tris);

  r_rtx.world_built = true;
  Com_Print("  RTX: built acceleration structures for %d triangles\n", num_tris);
}

/**
 * @brief Creates the storage image that the ray-tracing pipeline writes into.
 */
static void R_Rtx_CreateImage(void) {

  r_rtx.extent = r_vk.swapchain_extent;

  const VkImageCreateInfo ici = {
    .sType = VK_STRUCTURE_TYPE_IMAGE_CREATE_INFO,
    .imageType = VK_IMAGE_TYPE_2D,
    .format = VK_FORMAT_R8G8B8A8_UNORM,
    .extent = { r_rtx.extent.width, r_rtx.extent.height, 1 },
    .mipLevels = 1,
    .arrayLayers = 1,
    .samples = VK_SAMPLE_COUNT_1_BIT,
    .tiling = VK_IMAGE_TILING_OPTIMAL,
    .usage = VK_IMAGE_USAGE_STORAGE_BIT | VK_IMAGE_USAGE_TRANSFER_SRC_BIT,
    .initialLayout = VK_IMAGE_LAYOUT_UNDEFINED
  };
  vkCreateImage(r_vk.device, &ici, NULL, &r_rtx.image);

  VkMemoryRequirements req;
  vkGetImageMemoryRequirements(r_vk.device, r_rtx.image, &req);
  const VkMemoryAllocateInfo ai = {
    .sType = VK_STRUCTURE_TYPE_MEMORY_ALLOCATE_INFO,
    .allocationSize = req.size,
    .memoryTypeIndex = R_Rtx_FindMemoryType(req.memoryTypeBits, VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT)
  };
  vkAllocateMemory(r_vk.device, &ai, NULL, &r_rtx.image_memory);
  vkBindImageMemory(r_vk.device, r_rtx.image, r_rtx.image_memory, 0);

  const VkImageViewCreateInfo ivci = {
    .sType = VK_STRUCTURE_TYPE_IMAGE_VIEW_CREATE_INFO,
    .image = r_rtx.image,
    .viewType = VK_IMAGE_VIEW_TYPE_2D,
    .format = VK_FORMAT_R8G8B8A8_UNORM,
    .subresourceRange = { VK_IMAGE_ASPECT_COLOR_BIT, 0, 1, 0, 1 }
  };
  vkCreateImageView(r_vk.device, &ivci, NULL, &r_rtx.image_view);
}

/**
 * @brief Creates the descriptor set (TLAS + storage image + per-triangle buffer).
 */
static void R_Rtx_CreateDescriptors(void) {

  const VkDescriptorSetLayoutBinding binds[] = {
    { 0, VK_DESCRIPTOR_TYPE_ACCELERATION_STRUCTURE_KHR, 1, VK_SHADER_STAGE_RAYGEN_BIT_KHR | VK_SHADER_STAGE_CLOSEST_HIT_BIT_KHR, NULL },
    { 1, VK_DESCRIPTOR_TYPE_STORAGE_IMAGE, 1, VK_SHADER_STAGE_RAYGEN_BIT_KHR, NULL },
    { 2, VK_DESCRIPTOR_TYPE_STORAGE_BUFFER, 1, VK_SHADER_STAGE_CLOSEST_HIT_BIT_KHR, NULL },
    { 3, VK_DESCRIPTOR_TYPE_STORAGE_BUFFER, 1, VK_SHADER_STAGE_CLOSEST_HIT_BIT_KHR, NULL }
  };
  const VkDescriptorSetLayoutCreateInfo dslci = {
    .sType = VK_STRUCTURE_TYPE_DESCRIPTOR_SET_LAYOUT_CREATE_INFO,
    .bindingCount = lengthof(binds),
    .pBindings = binds
  };
  vkCreateDescriptorSetLayout(r_vk.device, &dslci, NULL, &r_rtx.set_layout);

  const VkDescriptorPoolSize sizes[] = {
    { VK_DESCRIPTOR_TYPE_ACCELERATION_STRUCTURE_KHR, 1 },
    { VK_DESCRIPTOR_TYPE_STORAGE_IMAGE, 1 },
    { VK_DESCRIPTOR_TYPE_STORAGE_BUFFER, 2 }
  };
  const VkDescriptorPoolCreateInfo dpci = {
    .sType = VK_STRUCTURE_TYPE_DESCRIPTOR_POOL_CREATE_INFO,
    .maxSets = 1,
    .poolSizeCount = lengthof(sizes),
    .pPoolSizes = sizes
  };
  vkCreateDescriptorPool(r_vk.device, &dpci, NULL, &r_rtx.descriptor_pool);

  const VkDescriptorSetAllocateInfo dsai = {
    .sType = VK_STRUCTURE_TYPE_DESCRIPTOR_SET_ALLOCATE_INFO,
    .descriptorPool = r_rtx.descriptor_pool,
    .descriptorSetCount = 1,
    .pSetLayouts = &r_rtx.set_layout
  };
  vkAllocateDescriptorSets(r_vk.device, &dsai, &r_rtx.descriptor_set);

  const VkWriteDescriptorSetAccelerationStructureKHR was = {
    .sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET_ACCELERATION_STRUCTURE_KHR,
    .accelerationStructureCount = 1,
    .pAccelerationStructures = &r_rtx.tlas
  };
  const VkDescriptorImageInfo image_info = { .imageView = r_rtx.image_view, .imageLayout = VK_IMAGE_LAYOUT_GENERAL };
  const VkDescriptorBufferInfo buffer_info = { .buffer = r_rtx.tri_buffer, .range = VK_WHOLE_SIZE };
  const VkDescriptorBufferInfo light_info = { .buffer = r_rtx.light_buffer, .range = VK_WHOLE_SIZE };

  const VkWriteDescriptorSet writes[] = {
    { .sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET, .pNext = &was, .dstSet = r_rtx.descriptor_set,
      .dstBinding = 0, .descriptorCount = 1, .descriptorType = VK_DESCRIPTOR_TYPE_ACCELERATION_STRUCTURE_KHR },
    { .sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET, .dstSet = r_rtx.descriptor_set,
      .dstBinding = 1, .descriptorCount = 1, .descriptorType = VK_DESCRIPTOR_TYPE_STORAGE_IMAGE, .pImageInfo = &image_info },
    { .sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET, .dstSet = r_rtx.descriptor_set,
      .dstBinding = 2, .descriptorCount = 1, .descriptorType = VK_DESCRIPTOR_TYPE_STORAGE_BUFFER, .pBufferInfo = &buffer_info },
    { .sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET, .dstSet = r_rtx.descriptor_set,
      .dstBinding = 3, .descriptorCount = 1, .descriptorType = VK_DESCRIPTOR_TYPE_STORAGE_BUFFER, .pBufferInfo = &light_info }
  };
  vkUpdateDescriptorSets(r_vk.device, lengthof(writes), writes, 0, NULL);
}

/**
 * @brief Creates the ray-tracing pipeline and its shader binding table.
 */
static void R_Rtx_CreatePipeline(void) {

  VkShaderModule rgen = R_Rtx_LoadShader("shaders/rtx.rgen.spv");
  VkShaderModule rmiss = R_Rtx_LoadShader("shaders/rtx.rmiss.spv");
  VkShaderModule smiss = R_Rtx_LoadShader("shaders/rtx_shadow.rmiss.spv");
  VkShaderModule rchit = R_Rtx_LoadShader("shaders/rtx.rchit.spv");

  const VkPipelineShaderStageCreateInfo stages[] = {
    { .sType = VK_STRUCTURE_TYPE_PIPELINE_SHADER_STAGE_CREATE_INFO, .stage = VK_SHADER_STAGE_RAYGEN_BIT_KHR, .module = rgen, .pName = "main" },
    { .sType = VK_STRUCTURE_TYPE_PIPELINE_SHADER_STAGE_CREATE_INFO, .stage = VK_SHADER_STAGE_MISS_BIT_KHR, .module = rmiss, .pName = "main" },
    { .sType = VK_STRUCTURE_TYPE_PIPELINE_SHADER_STAGE_CREATE_INFO, .stage = VK_SHADER_STAGE_MISS_BIT_KHR, .module = smiss, .pName = "main" },
    { .sType = VK_STRUCTURE_TYPE_PIPELINE_SHADER_STAGE_CREATE_INFO, .stage = VK_SHADER_STAGE_CLOSEST_HIT_BIT_KHR, .module = rchit, .pName = "main" }
  };
  const VkRayTracingShaderGroupCreateInfoKHR groups[] = {
    { .sType = VK_STRUCTURE_TYPE_RAY_TRACING_SHADER_GROUP_CREATE_INFO_KHR, .type = VK_RAY_TRACING_SHADER_GROUP_TYPE_GENERAL_KHR, .generalShader = 0, .closestHitShader = VK_SHADER_UNUSED_KHR, .anyHitShader = VK_SHADER_UNUSED_KHR, .intersectionShader = VK_SHADER_UNUSED_KHR },
    { .sType = VK_STRUCTURE_TYPE_RAY_TRACING_SHADER_GROUP_CREATE_INFO_KHR, .type = VK_RAY_TRACING_SHADER_GROUP_TYPE_GENERAL_KHR, .generalShader = 1, .closestHitShader = VK_SHADER_UNUSED_KHR, .anyHitShader = VK_SHADER_UNUSED_KHR, .intersectionShader = VK_SHADER_UNUSED_KHR },
    { .sType = VK_STRUCTURE_TYPE_RAY_TRACING_SHADER_GROUP_CREATE_INFO_KHR, .type = VK_RAY_TRACING_SHADER_GROUP_TYPE_GENERAL_KHR, .generalShader = 2, .closestHitShader = VK_SHADER_UNUSED_KHR, .anyHitShader = VK_SHADER_UNUSED_KHR, .intersectionShader = VK_SHADER_UNUSED_KHR },
    { .sType = VK_STRUCTURE_TYPE_RAY_TRACING_SHADER_GROUP_CREATE_INFO_KHR, .type = VK_RAY_TRACING_SHADER_GROUP_TYPE_TRIANGLES_HIT_GROUP_KHR, .generalShader = VK_SHADER_UNUSED_KHR, .closestHitShader = 3, .anyHitShader = VK_SHADER_UNUSED_KHR, .intersectionShader = VK_SHADER_UNUSED_KHR }
  };

  const VkPushConstantRange pcr = {
    .stageFlags = VK_SHADER_STAGE_RAYGEN_BIT_KHR | VK_SHADER_STAGE_CLOSEST_HIT_BIT_KHR,
    .offset = 0,
    .size = sizeof(r_rtx_push_t)
  };
  const VkPipelineLayoutCreateInfo plci = {
    .sType = VK_STRUCTURE_TYPE_PIPELINE_LAYOUT_CREATE_INFO,
    .setLayoutCount = 1,
    .pSetLayouts = &r_rtx.set_layout,
    .pushConstantRangeCount = 1,
    .pPushConstantRanges = &pcr
  };
  vkCreatePipelineLayout(r_vk.device, &plci, NULL, &r_rtx.pipeline_layout);

  const VkRayTracingPipelineCreateInfoKHR rpci = {
    .sType = VK_STRUCTURE_TYPE_RAY_TRACING_PIPELINE_CREATE_INFO_KHR,
    .stageCount = lengthof(stages),
    .pStages = stages,
    .groupCount = lengthof(groups),
    .pGroups = groups,
    .maxPipelineRayRecursionDepth = 2,
    .layout = r_rtx.pipeline_layout
  };
  if (r_rtx.CreateRayTracingPipelines(r_vk.device, VK_NULL_HANDLE, VK_NULL_HANDLE, 1, &rpci, NULL, &r_rtx.pipeline) != VK_SUCCESS) {
    Com_Error(ERROR_FATAL, "vkCreateRayTracingPipelinesKHR failed\n");
  }

  vkDestroyShaderModule(r_vk.device, rgen, NULL);
  vkDestroyShaderModule(r_vk.device, rmiss, NULL);
  vkDestroyShaderModule(r_vk.device, smiss, NULL);
  vkDestroyShaderModule(r_vk.device, rchit, NULL);

  // shader binding table: rgen | miss(2) | hit(1)
  const uint32_t handle = r_rtx.properties.shaderGroupHandleSize;
  const uint32_t base = r_rtx.properties.shaderGroupBaseAlignment;
  const uint32_t stride = R_Rtx_Align(handle, r_rtx.properties.shaderGroupHandleAlignment);

  uint8_t *handles = Mem_Malloc(handle * 4);
  r_rtx.GetShaderGroupHandles(r_vk.device, r_rtx.pipeline, 0, 4, handle * 4, handles);

  const uint32_t rgen_off = 0;
  const uint32_t miss_off = R_Rtx_Align(stride, base);
  const uint32_t hit_off = miss_off + R_Rtx_Align(2 * stride, base);
  const uint32_t sbt_size = hit_off + R_Rtx_Align(stride, base);

  const VkDeviceAddress sbt_addr = R_Rtx_CreateBuffer(sbt_size,
    VK_BUFFER_USAGE_SHADER_BINDING_TABLE_BIT_KHR | VK_BUFFER_USAGE_SHADER_DEVICE_ADDRESS_BIT,
    VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT | VK_MEMORY_PROPERTY_HOST_COHERENT_BIT, NULL, &r_rtx.sbt_buffer, &r_rtx.sbt_memory);

  uint8_t *sbt;
  vkMapMemory(r_vk.device, r_rtx.sbt_memory, 0, sbt_size, 0, (void **) &sbt);
  memcpy(sbt + rgen_off, handles + handle * 0, handle);
  memcpy(sbt + miss_off + stride * 0, handles + handle * 1, handle);
  memcpy(sbt + miss_off + stride * 1, handles + handle * 2, handle);
  memcpy(sbt + hit_off, handles + handle * 3, handle);
  vkUnmapMemory(r_vk.device, r_rtx.sbt_memory);
  Mem_Free(handles);

  r_rtx.rgen_region = (VkStridedDeviceAddressRegionKHR) { sbt_addr + rgen_off, stride, stride };
  r_rtx.miss_region = (VkStridedDeviceAddressRegionKHR) { sbt_addr + miss_off, stride, 2 * stride };
  r_rtx.hit_region = (VkStridedDeviceAddressRegionKHR) { sbt_addr + hit_off, stride, stride };
  r_rtx.call_region = (VkStridedDeviceAddressRegionKHR) { 0, 0, 0 };
}

/**
 * @brief Returns true if hardware ray tracing is available and a world is loaded, so
 * the RTX path can render this frame.
 */
_Bool R_Vk_RtxAvailable(void) {
  const r_model_t *world = R_WorldModel();
  return r_vk.rtx && world != NULL && world->bsp != NULL;
}

/**
 * @brief Lazily builds all RTX resources for the loaded world.
 */
static void R_Rtx_Init(void) {

  R_Rtx_LoadProcs();
  R_Rtx_BuildWorld();
  if (!r_rtx.world_built) {
    return;
  }
  R_Rtx_CreateImage();
  R_Rtx_CreateDescriptors();
  R_Rtx_CreatePipeline();
  r_rtx.initialized = true;
}

/**
 * @brief Records an image layout transition.
 */
static void R_Rtx_Barrier(VkCommandBuffer cb, VkImage image, VkImageLayout from, VkImageLayout to,
                          VkAccessFlags src, VkAccessFlags dst, VkPipelineStageFlags src_stage, VkPipelineStageFlags dst_stage) {

  const VkImageMemoryBarrier b = {
    .sType = VK_STRUCTURE_TYPE_IMAGE_MEMORY_BARRIER,
    .oldLayout = from,
    .newLayout = to,
    .srcQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED,
    .dstQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED,
    .image = image,
    .subresourceRange = { VK_IMAGE_ASPECT_COLOR_BIT, 0, 1, 0, 1 },
    .srcAccessMask = src,
    .dstAccessMask = dst
  };
  vkCmdPipelineBarrier(cb, src_stage, dst_stage, 0, 0, NULL, 0, NULL, 1, &b);
}

/**
 * @brief Ray-traces the loaded world from the given view and presents the result.
 * Returns true if it rendered, false if the RTX path is unavailable (caller falls
 * back to clearing the screen).
 */
_Bool R_Vk_RtxRenderView(const r_view_t *view) {

  if (!r_vk.rtx || !view) {
    return false;
  }

  if (!r_rtx.initialized) {
    R_Rtx_Init();
    if (!r_rtx.initialized) {
      Com_Print("RTX: init failed (world_built=%d)\n", r_rtx.world_built);
      return false;
    }
  }

  const uint32_t frame = r_vk.current_frame;
  vkWaitForFences(r_vk.device, 1, &r_vk.in_flight[frame], VK_TRUE, UINT64_MAX);

  uint32_t image_index;
  if (vkAcquireNextImageKHR(r_vk.device, r_vk.swapchain, UINT64_MAX,
                            r_vk.image_available[frame], VK_NULL_HANDLE, &image_index) == VK_ERROR_OUT_OF_DATE_KHR) {
    return true;
  }
  vkResetFences(r_vk.device, 1, &r_vk.in_flight[frame]);

  // eye + look-at target (Quetoo is Z-up). When a valid player view is available the
  // world is ray-traced first-person from it; otherwise (e.g. before spawn) it is
  // framed from an elevated bounds camera so the loaded map is always visible. The
  // `r_rtx_overview` cvar forces the bounds camera.
  const float aspect = (float) r_rtx.extent.width / (float) r_rtx.extent.height;
  vec3_t origin = view->origin;
  vec3_t target;
  vec2_t tan_fov;

  if (r_rtx_overview->integer || Vec3_Length(origin) < 1.f) {
    const vec3_t center = Vec3_Scale(Vec3_Add(r_rtx.world_mins, r_rtx.world_maxs), 0.5f);
    const vec3_t size = Vec3_Subtract(r_rtx.world_maxs, r_rtx.world_mins);
    origin = Vec3(center.x, center.y - size.y * 0.5f, r_rtx.world_maxs.z + size.z * 0.6f);
    target = center;
    tan_fov = Vec2(0.55f * aspect, 0.55f);
  } else {
    vec3_t forward, right, up;
    Vec3_Vectors(view->angles, &forward, &right, &up);
    target = Vec3_Add(origin, forward);
    // derive vertical fov from the horizontal fov and the swapchain aspect; the
    // engine's view->fov.y is computed from the (unused) GL framebuffer and is NaN
    const float tx = tanf(Radians(view->fov.x) * .5f);
    tan_fov = Vec2(tx, tx / aspect);
  }

  // reset temporal accumulation whenever the camera moves; otherwise advance the
  // sample index so the shader converges jittered soft shadows and anti-aliasing
  if (Vec3_Distance(origin, r_rtx.last_eye) > 0.1f || Vec3_Distance(target, r_rtx.last_target) > 0.1f) {
    r_rtx.accum_frame = 0;
  } else {
    r_rtx.accum_frame++;
  }
  r_rtx.last_eye = origin;
  r_rtx.last_target = target;

  r_rtx_push_t pc = {
    .eye = Vec4(origin.x, origin.y, origin.z, 0.f),
    .target = Vec4(target.x, target.y, target.z, 0.f),
    .light = Vec4(origin.x, origin.y, origin.z + 512.f, 0.f),
    .params = Vec4(tan_fov.x, tan_fov.y, (float) r_rtx.num_lights, (float) r_rtx.accum_frame)
  };

  VkCommandBuffer cb = r_vk.command_buffers[frame];
  vkResetCommandBuffer(cb, 0);
  const VkCommandBufferBeginInfo bi = { .sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_BEGIN_INFO };
  vkBeginCommandBuffer(cb, &bi);

  R_Rtx_Barrier(cb, r_rtx.image, VK_IMAGE_LAYOUT_UNDEFINED, VK_IMAGE_LAYOUT_GENERAL,
    0, VK_ACCESS_SHADER_WRITE_BIT, VK_PIPELINE_STAGE_TOP_OF_PIPE_BIT, VK_PIPELINE_STAGE_RAY_TRACING_SHADER_BIT_KHR);

  vkCmdBindPipeline(cb, VK_PIPELINE_BIND_POINT_RAY_TRACING_KHR, r_rtx.pipeline);
  vkCmdBindDescriptorSets(cb, VK_PIPELINE_BIND_POINT_RAY_TRACING_KHR, r_rtx.pipeline_layout, 0, 1, &r_rtx.descriptor_set, 0, NULL);
  vkCmdPushConstants(cb, r_rtx.pipeline_layout, VK_SHADER_STAGE_RAYGEN_BIT_KHR | VK_SHADER_STAGE_CLOSEST_HIT_BIT_KHR, 0, sizeof(pc), &pc);
  r_rtx.CmdTraceRays(cb, &r_rtx.rgen_region, &r_rtx.miss_region, &r_rtx.hit_region, &r_rtx.call_region,
    r_rtx.extent.width, r_rtx.extent.height, 1);

  // blit the traced image onto the acquired swapchain image
  R_Rtx_Barrier(cb, r_rtx.image, VK_IMAGE_LAYOUT_GENERAL, VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL,
    VK_ACCESS_SHADER_WRITE_BIT, VK_ACCESS_TRANSFER_READ_BIT, VK_PIPELINE_STAGE_RAY_TRACING_SHADER_BIT_KHR, VK_PIPELINE_STAGE_TRANSFER_BIT);
  R_Rtx_Barrier(cb, r_vk.swapchain_images[image_index], VK_IMAGE_LAYOUT_UNDEFINED, VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL,
    0, VK_ACCESS_TRANSFER_WRITE_BIT, VK_PIPELINE_STAGE_TOP_OF_PIPE_BIT, VK_PIPELINE_STAGE_TRANSFER_BIT);

  const VkImageBlit blit = {
    .srcSubresource = { VK_IMAGE_ASPECT_COLOR_BIT, 0, 0, 1 },
    .srcOffsets = { { 0, 0, 0 }, { (int32_t) r_rtx.extent.width, (int32_t) r_rtx.extent.height, 1 } },
    .dstSubresource = { VK_IMAGE_ASPECT_COLOR_BIT, 0, 0, 1 },
    .dstOffsets = { { 0, 0, 0 }, { (int32_t) r_vk.swapchain_extent.width, (int32_t) r_vk.swapchain_extent.height, 1 } }
  };
  vkCmdBlitImage(cb, r_rtx.image, VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL,
    r_vk.swapchain_images[image_index], VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL, 1, &blit, VK_FILTER_LINEAR);

  // composite the 2D UI (HUD / console / menu) over the ray-traced frame; the 2D
  // load render pass preserves the blit and transitions the image to present
  R_Vk_Draw2D(cb, image_index, true);

  vkEndCommandBuffer(cb);

  const VkPipelineStageFlags wait_stage = VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT;
  const VkSubmitInfo si = {
    .sType = VK_STRUCTURE_TYPE_SUBMIT_INFO,
    .waitSemaphoreCount = 1,
    .pWaitSemaphores = &r_vk.image_available[frame],
    .pWaitDstStageMask = &wait_stage,
    .commandBufferCount = 1,
    .pCommandBuffers = &cb,
    .signalSemaphoreCount = 1,
    .pSignalSemaphores = &r_vk.render_finished[frame]
  };
  vkQueueSubmit(r_vk.graphics_queue, 1, &si, r_vk.in_flight[frame]);

  const VkPresentInfoKHR pi = {
    .sType = VK_STRUCTURE_TYPE_PRESENT_INFO_KHR,
    .waitSemaphoreCount = 1,
    .pWaitSemaphores = &r_vk.render_finished[frame],
    .swapchainCount = 1,
    .pSwapchains = &r_vk.swapchain,
    .pImageIndices = &image_index
  };
  vkQueuePresentKHR(r_vk.present_queue, &pi);

  r_vk.current_frame = (frame + 1) % R_VK_MAX_FRAMES_IN_FLIGHT;
  return true;
}

/**
 * @brief Releases all RTX resources.
 */
void R_Vk_RtxShutdown(void) {

  if (!r_rtx.initialized && !r_rtx.world_built) {
    return;
  }

  vkDeviceWaitIdle(r_vk.device);

  if (r_rtx.pipeline) { vkDestroyPipeline(r_vk.device, r_rtx.pipeline, NULL); }
  if (r_rtx.pipeline_layout) { vkDestroyPipelineLayout(r_vk.device, r_rtx.pipeline_layout, NULL); }
  if (r_rtx.descriptor_pool) { vkDestroyDescriptorPool(r_vk.device, r_rtx.descriptor_pool, NULL); }
  if (r_rtx.set_layout) { vkDestroyDescriptorSetLayout(r_vk.device, r_rtx.set_layout, NULL); }
  if (r_rtx.sbt_buffer) { vkDestroyBuffer(r_vk.device, r_rtx.sbt_buffer, NULL); vkFreeMemory(r_vk.device, r_rtx.sbt_memory, NULL); }
  if (r_rtx.image_view) { vkDestroyImageView(r_vk.device, r_rtx.image_view, NULL); }
  if (r_rtx.image) { vkDestroyImage(r_vk.device, r_rtx.image, NULL); vkFreeMemory(r_vk.device, r_rtx.image_memory, NULL); }
  if (r_rtx.tri_buffer) { vkDestroyBuffer(r_vk.device, r_rtx.tri_buffer, NULL); vkFreeMemory(r_vk.device, r_rtx.tri_memory, NULL); }
  if (r_rtx.light_buffer) { vkDestroyBuffer(r_vk.device, r_rtx.light_buffer, NULL); vkFreeMemory(r_vk.device, r_rtx.light_memory, NULL); }
  if (r_rtx.tlas) { r_rtx.DestroyAccelerationStructure(r_vk.device, r_rtx.tlas, NULL); }
  if (r_rtx.tlas_buffer) { vkDestroyBuffer(r_vk.device, r_rtx.tlas_buffer, NULL); vkFreeMemory(r_vk.device, r_rtx.tlas_memory, NULL); }
  if (r_rtx.blas) { r_rtx.DestroyAccelerationStructure(r_vk.device, r_rtx.blas, NULL); }
  if (r_rtx.blas_buffer) { vkDestroyBuffer(r_vk.device, r_rtx.blas_buffer, NULL); vkFreeMemory(r_vk.device, r_rtx.blas_memory, NULL); }

  memset(&r_rtx, 0, sizeof(r_rtx));
}

#endif /* BUILD_VULKAN */
