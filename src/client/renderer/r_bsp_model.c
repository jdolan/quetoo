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
#include "r_local.h"

/**
 * @brief Loads BSP plane data from the collision model into renderer plane structures.
 */
static void R_LoadBspPlanes(r_bsp_model_t *bsp) {
  r_bsp_plane_t *out;

  const cm_bsp_plane_t *in = bsp->cm->planes;

  bsp->num_planes = bsp->cm->num_planes;
  bsp->planes = out = Mem_LinkMalloc(bsp->num_planes * sizeof(*out), bsp);

  for (int32_t i = 0; i < bsp->num_planes; i++, out++, in++) {
    out->cm = in;
  }
}

/**
 * @brief Loads and registers all BSP materials referenced in the map file.
 */
static void R_LoadBspMaterials(r_model_t *mod) {

  r_material_t **out;
  const bsp_material_t *in = mod->bsp->cm->file->materials;

  mod->bsp->num_materials = mod->bsp->cm->file->num_materials;
  mod->bsp->materials = out = Mem_LinkMalloc(mod->bsp->num_materials * sizeof(*out), mod->bsp);

  for (int32_t i = 0; i < mod->bsp->num_materials; i++, in++, out++) {
    *out = R_LoadMaterial(in->name, ASSET_CONTEXT_TEXTURES);
    R_RegisterDependency((r_media_t *) mod, (r_media_t *) *out);
  }
}

/**
 * @brief Loads BSP brush side data, linking planes and materials.
 */
static void R_LoadBspBrushSides(r_bsp_model_t *bsp) {
  r_bsp_brush_side_t *out;

  const bsp_brush_side_t *in = bsp->cm->file->brush_sides;

  bsp->num_brush_sides = bsp->cm->file->num_brush_sides;
  bsp->brush_sides = out = Mem_LinkMalloc(bsp->num_brush_sides * sizeof(*out), bsp);

  for (int32_t i = 0; i < bsp->num_brush_sides; i++, in++, out++) {

    out->plane = bsp->planes + in->plane;

    if (in->material > -1) {
      out->material = bsp->materials[in->material];
    }

    out->axis[0] = in->axis[0];
    out->axis[1] = in->axis[1];

    out->contents = in->contents;
    out->surface = in->surface;
    out->value = in->value;
  }
}

/**
 * @brief Loads BSP patch (curved surface) data, linking materials.
 */
static void R_LoadBspPatches(r_bsp_model_t *bsp) {

  const bsp_patch_t *in = bsp->cm->file->patches;

  bsp->num_patches = bsp->cm->file->num_patches;
  r_bsp_patch_t *out = bsp->patches = Mem_LinkMalloc(bsp->num_patches * sizeof(*out), bsp);

  for (int32_t i = 0; i < bsp->num_patches; i++, in++, out++) {

    if (in->material > -1) {
      out->material = bsp->materials[in->material];
    }

    out->contents = in->contents;
    out->surface = in->surface;
  }
}

/**
 * @brief Loads BSP vertex data (positions, normals, tangents, diffuse UVs, colors).
 */
static void R_LoadBspVertexes(r_bsp_model_t *bsp) {

  bsp->num_vertexes = bsp->cm->file->num_vertexes;
  r_bsp_vertex_t *out = bsp->vertexes = Mem_LinkMalloc(bsp->num_vertexes * sizeof(*out), bsp);

  const bsp_vertex_t *in = bsp->cm->file->vertexes;
  for (int32_t i = 0; i < bsp->num_vertexes; i++, in++, out++) {

    out->position = in->position;
    out->normal = in->normal;
    out->tangent = in->tangent;
    out->bitangent = in->bitangent;
    out->diffusemap = in->diffusemap;
    out->color = in->color;
  }
}

/**
 * @brief Loads BSP triangle element (index) data.
 */
static void R_LoadBspElements(r_bsp_model_t *bsp) {

  bsp->num_elements = bsp->cm->file->num_elements;
  uint32_t *out = bsp->elements = Mem_LinkMalloc(bsp->num_elements * sizeof(*out), bsp);

  const int32_t *in = bsp->cm->file->elements;
  for (int32_t i = 0; i < bsp->num_elements; i++, in++, out++) {
    *out = *in;
  }
}

/**
 * @brief Loads all `r_bsp_face_t` for the specified BSP model.
 */
static void R_LoadBspFaces(r_bsp_model_t *bsp) {

  const bsp_face_t *in = bsp->cm->file->faces;
  r_bsp_face_t *out;

  bsp->num_faces = bsp->cm->file->num_faces;
  bsp->faces = out = Mem_LinkMalloc(bsp->num_faces * sizeof(*out), bsp);

  for (int32_t i = 0; i < bsp->num_faces; i++, in++, out++) {

    if (in->brush_side >= 0) {
      out->brush_side = bsp->brush_sides + in->brush_side;
      out->plane = bsp->planes + in->plane;
    } else {
      out->patch = bsp->patches + in->patch;
      out->plane = NULL;
    }

    out->bounds = in->bounds;

    out->vertexes = bsp->vertexes + in->first_vertex;
    out->num_vertexes = in->num_vertexes;

    out->elements = (void *) (in->first_element * sizeof(uint32_t));
    out->num_elements = in->num_elements;
  }
}

/**
 * @brief Loads all `r_bsp_leaf_t` for the specified BSP model.
 */
static void R_LoadBspLeafs(r_bsp_model_t *bsp) {
  r_bsp_leaf_t *out;

  const bsp_leaf_t *in = bsp->cm->file->leafs;

  bsp->num_leafs = bsp->cm->file->num_leafs;
  bsp->leafs = out = Mem_LinkMalloc(bsp->num_leafs * sizeof(*out), bsp);

  for (int32_t i = 0; i < bsp->num_leafs; i++, in++, out++) {
    out->contents = in->contents;
    out->bounds = in->bounds;
  }
}

/**
 * @brief Loads all `r_bsp_node_t` for the specified BSP model.
 */
static void R_LoadBspNodes(r_bsp_model_t *bsp) {
  r_bsp_node_t *out;

  const bsp_node_t *in = bsp->cm->file->nodes;

  bsp->num_nodes = bsp->cm->file->num_nodes;
  bsp->nodes = out = Mem_LinkMalloc(bsp->num_nodes * sizeof(*out), bsp);

  for (int32_t i = 0; i < bsp->num_nodes; i++, in++, out++) {

    out->contents = in->contents;
    out->plane = bsp->planes + in->plane;
    out->bounds = in->bounds;
    out->visible_bounds = in->visible_bounds;

    out->faces = bsp->faces + in->first_face;
    out->num_faces = in->num_faces;

    r_bsp_face_t *f = out->faces;
    for (int32_t j = 0; j < out->num_faces; j++, f++) {
      f->node = out;
    }

    for (int32_t j = 0; j < 2; j++) {
      const int32_t c = in->children[j];
      if (c >= 0) {
        out->children[j] = bsp->nodes + c;
      } else {
        out->children[j] = (r_bsp_node_t *) (bsp->leafs + (-1 - c));
      }
    }
  }
}

/**
 * @brief Sets up in-memory relationships between node, parent and model.
 */
static void R_SetupBspNode(r_bsp_inline_model_t *model, r_bsp_node_t *parent, r_bsp_node_t *node) {

  node->model = model;
  node->parent = parent;

  if (node->contents > CONTENTS_NODE) {
    return;
  }

  R_SetupBspNode(model, node, node->children[0]);
  R_SetupBspNode(model, node, node->children[1]);
}

/**
 * @brief Loads draw elements batches, computing texture coordinate origins for animated stages.
 */
static void R_LoadBspDrawElements(r_bsp_model_t *bsp) {
  r_bsp_draw_elements_t *out;

  bsp->num_draw_elements = bsp->cm->file->num_draw_elements;
  bsp->draw_elements = out = Mem_LinkMalloc(bsp->num_draw_elements * sizeof(*out), bsp);

  const bsp_draw_elements_t *in = bsp->cm->file->draw_elements;
  for (int32_t i = 0; i < bsp->num_draw_elements; i++, in++, out++) {

    out->material = bsp->materials[in->material];
    out->surface = in->surface;

    out->bounds = in->bounds;

    out->elements = (void *) (in->first_element * sizeof(uint32_t));
    out->num_elements = in->num_elements;

    if (out->material->cm->stage_flags & (STAGE_STRETCH | STAGE_ROTATE)) {

      vec2_t st_mins = Vec2_Mins();
      vec2_t st_maxs = Vec2_Maxs();

      const uint32_t *e = bsp->elements + in->first_element;
      for (int32_t j = 0; j < out->num_elements; j++, e++) {
        const r_bsp_vertex_t *v = &bsp->vertexes[*e];

        st_mins = Vec2_Minf(st_mins, v->diffusemap);
        st_maxs = Vec2_Maxf(st_maxs, v->diffusemap);
      }

      out->st_origin = Vec2_Scale(Vec2_Add(st_mins, st_maxs), .5f);
    }
  }
}

/**
 * @brief Loads BSP blocks and allocates GPU buffers for per-block decal geometry.
 */
static void R_LoadBspBlocks(r_bsp_model_t *bsp) {
  r_bsp_block_t *out;

  bsp->num_blocks = bsp->cm->file->num_blocks;
  bsp->blocks = out = Mem_LinkMalloc(bsp->num_blocks * sizeof(r_bsp_block_t), bsp);

  const bsp_block_t *in = bsp->cm->file->blocks;
  for (int32_t i = 0; i < bsp->num_blocks; i++, in++, out++) {

    out->node = bsp->nodes + in->node;
    out->draw_elements = bsp->draw_elements + in->first_draw_element;
    out->num_draw_elements = in->num_draw_elements;
    out->visible_bounds = in->visible_bounds;

    for (int32_t j = 0; j < out->num_draw_elements; j++) {
      out->surface |= out->draw_elements[j].surface;
    }

    r_bsp_block_decals_t *decals = &out->decals;

    decals->triangles = $(alloc(Vector), initWithSize, sizeof(r_decal_triangle_t));

    // TODO(#864): port per-block decal geometry to a dynamic Buffer (deferred; decals not yet drawn).
  }

  const bsp_face_t *in_face = bsp->cm->file->faces;
  r_bsp_face_t *out_face = bsp->faces;
  for (int32_t i = 0; i < bsp->num_faces; i++, in_face++, out_face++) {
    if (in_face->block >= 0 && in_face->block < bsp->num_blocks) {
      out_face->block = &bsp->blocks[in_face->block];
    }
  }
}

/**
 * @brief Loads all BSP inline (brush) models and sets up their visible bounds and draw data.
 */
static void R_LoadBspInlineModels(r_bsp_model_t *bsp) {
  r_bsp_inline_model_t *out;

  const bsp_model_t *in = bsp->cm->file->models;

  bsp->num_inline_models = bsp->cm->file->num_models;
  bsp->inline_models = out = Mem_LinkMalloc(bsp->num_inline_models * sizeof(*out), bsp);

  for (int32_t i = 0; i < bsp->num_inline_models; i++, in++, out++) {

    out->entity = bsp->cm->entities[in->entity];
    out->head_node = bsp->nodes + in->head_node;

    out->visible_bounds = in->visible_bounds;

    out->faces = bsp->faces + in->first_face;
    out->num_faces = in->num_faces;

    out->depth_pass_elements = (void *) (in->first_depth_pass_element * sizeof(uint32_t));
    out->num_depth_pass_elements = in->num_depth_pass_elements;

    out->draw_elements = bsp->draw_elements + in->first_draw_elements;
    out->num_draw_elements = in->num_draw_elements;

    out->blocks = bsp->blocks + in->first_block;
    out->num_blocks = in->num_blocks;

    R_SetupBspNode(out, NULL, out->head_node);
  }
}

/**
 * @brief Loads static BSP lights from the map file for dynamic shadow and lighting.
 */
static void R_LoadBspLights(r_bsp_model_t *bsp) {

  const bsp_light_t *in = bsp->cm->file->lights;

  bsp->num_lights = bsp->cm->file->num_lights;
  r_bsp_light_t *out = bsp->lights = Mem_LinkMalloc(sizeof(*out) * bsp->num_lights, bsp);

  for (int32_t i = 0; i < bsp->num_lights; i++, in++, out++) {

    out->entity = bsp->cm->entities[in->entity];
    out->origin = in->origin;
    out->color = in->color;
    out->radius = in->radius;
    out->intensity = in->intensity;
    out->bounds = in->bounds;
    q_strlcpy(out->style, in->style, sizeof(out->style));
    out->drift = in->drift;
    out->depth_pass_elements = (void *) (in->first_depth_pass_element * sizeof(uint32_t));
    out->num_depth_pass_elements = in->num_depth_pass_elements;
    out->target_entity = in->target_entity > 0 ? bsp->cm->entities[in->target_entity] : NULL;
  }
}

/**
 * @brief Loads the voxel grid and light assignment data for clustered forward lighting.
 */
static void R_LoadBspVoxels(r_model_t *mod) {

  const bsp_voxels_t *in = mod->bsp->cm->file->voxels;
  const byte *data = (byte *) in + sizeof(bsp_voxels_t);

  r_bsp_voxels_t *out = &mod->bsp->voxels;

  out->size = in->size;
  out->num_voxels = out->size.x * out->size.y * out->size.z;
  out->num_light_indices = in->num_light_indices;
  out->bounds = in->bounds;

  const byte *caustics_data = data;
  data += out->num_voxels * sizeof(byte) * 3; // RGB

  out->caustics = (r_image_t *) R_AllocMedia("voxel_caustics", sizeof(r_image_t), R_MEDIA_IMAGE);
  out->caustics->media.Free = R_FreeImage;
  out->caustics->type = IMG_VOXELS;
  out->caustics->width = out->size.x;
  out->caustics->height = out->size.y;
  out->caustics->depth = out->size.z;

  R_UploadImage(out->caustics, caustics_data);

  const int32_t *light_data = (const int32_t *) data;
  data += out->num_voxels * sizeof(int32_t) * 2;

  out->light_data = (r_image_t *) R_AllocMedia("voxel_light_data", sizeof(r_image_t), R_MEDIA_IMAGE);
  out->light_data->media.Free = R_FreeImage;
  out->light_data->type = IMG_VOXELS;
  out->light_data->width = out->size.x;
  out->light_data->height = out->size.y;
  out->light_data->depth = out->size.z;

  // The per-voxel (first_index, count) pairs, as a 3D RG32I texture (isampler3D).
  out->light_data->texture = $(r_device.device, createTexture, &(SDL_GPUTextureCreateInfo) {
    .type = SDL_GPU_TEXTURETYPE_3D,
    .format = SDL_GPU_TEXTUREFORMAT_R32G32_INT,
    .usage = SDL_GPU_TEXTUREUSAGE_SAMPLER,
    .width = (Uint32) out->size.x,
    .height = (Uint32) out->size.y,
    .layer_count_or_depth = (Uint32) out->size.z,
    .num_levels = 1,
    .sample_count = SDL_GPU_SAMPLECOUNT_1,
  }, light_data);

  const int32_t *light_indices_data = (const int32_t *) data;
  data += out->num_light_indices * sizeof(int32_t);

  // The flat light index vector, as a read-only R32I storage buffer.
  if (out->num_light_indices > 0) {
    out->light_indices_buffer = $(r_device.device, createBufferWithConstMem,
        SDL_GPU_BUFFERUSAGE_GRAPHICS_STORAGE_READ,
        light_indices_data,
        out->num_light_indices * sizeof(int32_t));
  }

  // Retained as a media placeholder for dependency registration; its GPU data
  // now lives in out->light_indices_buffer rather than a texture.
  out->light_indices = (r_image_t *) R_AllocMedia("voxel_light_indices", sizeof(r_image_t), R_MEDIA_IMAGE);
  out->light_indices->media.Free = R_FreeImage;
  out->light_indices->type = IMG_VOXELS;

  const byte *occlusion_data = data;

  out->occlusion = (r_image_t *) R_AllocMedia("voxel_occlusion", sizeof(r_image_t), R_MEDIA_IMAGE);
  out->occlusion->media.Free = R_FreeImage;
  out->occlusion->type = IMG_VOXELS;
  out->occlusion->width = out->size.x;
  out->occlusion->height = out->size.y;
  out->occlusion->depth = out->size.z;

  R_UploadImage(out->occlusion, occlusion_data);

  if (r_draw_bsp_voxels->value) {
    
    out->voxels = Mem_LinkMalloc(out->num_voxels * sizeof(r_bsp_voxel_t), mod->bsp);

    for (int32_t u = 0; u < out->size.z; u++) {
      for (int32_t t = 0; t < out->size.y; t++) {
        for (int32_t s = 0; s < out->size.x; s++) {
          const int32_t voxel_index = (u * out->size.y + t) * out->size.x + s;
          r_bsp_voxel_t *voxel = &out->voxels[voxel_index];

          const vec3_t voxel_mins = Vec3(
            out->bounds.mins.x + s * BSP_VOXEL_SIZE,
            out->bounds.mins.y + t * BSP_VOXEL_SIZE,
            out->bounds.mins.z + u * BSP_VOXEL_SIZE
          );

          const vec3_t voxel_maxs = Vec3_Add(voxel_mins, Vec3(BSP_VOXEL_SIZE, BSP_VOXEL_SIZE, BSP_VOXEL_SIZE));
          voxel->bounds = Box3(voxel_mins, voxel_maxs);

          const int32_t first_light_index = light_data[voxel_index * 2 + 0];
          const int32_t num_light_indices = light_data[voxel_index * 2 + 1];

          voxel->num_lights = num_light_indices;

          if (voxel->num_lights > 0) {
            voxel->lights = Mem_LinkMalloc(voxel->num_lights * sizeof(r_bsp_light_t *), mod->bsp);

            for (int32_t i = 0; i < voxel->num_lights; i++) {
              const int32_t light_id = light_indices_data[first_light_index + i];

              if (light_id >= 0 && light_id < mod->bsp->num_lights) {
                voxel->lights[i] = &mod->bsp->lights[light_id];
              } else {
                voxel->lights[i] = NULL;
              }
            }
          } else {
            voxel->lights = NULL;
          }
        }
      }
    }
  }

  R_GetError(NULL);
}

/**
 * @brief Creates and configures the BSP vertex array object and associated GPU buffers.
 */
static void R_LoadBspVertexArray(r_model_t *mod) {

  r_bsp_model_t *bsp = mod->bsp;

  bsp->vertex_buffer = $(r_device.device, createBufferWithConstMem, SDL_GPU_BUFFERUSAGE_VERTEX,
                         bsp->vertexes, bsp->num_vertexes * sizeof(r_bsp_vertex_t));

  bsp->elements_buffer = $(r_device.device, createBufferWithConstMem, SDL_GPU_BUFFERUSAGE_INDEX,
                           bsp->elements, bsp->num_elements * sizeof(uint32_t));
}

/**
 * @brief Creates a position-only VAO for the BSP depth pre-pass.
 * @remarks Under SDL_gpu the depth pre-pass reuses the world vertex/index
 * buffers directly; vertex layout is described by the GraphicsPipeline, so no
 * separate GPU object is required here.
 */
static void R_LoadBspDepthPassVertexArray(r_model_t *mod) {
}

/**
 * @brief Creates an `r_model_t` for each inline model so that entities may reference them.
 */
static void R_SetupBspInlineModels(r_model_t *mod) {

  r_bsp_inline_model_t *in = mod->bsp->inline_models;
  for (int32_t i = 0; i < mod->bsp->num_inline_models; i++, in++) {

    char name[MAX_QPATH];
    q_snprintf(name, sizeof(name), "%s#%d", mod->media.name, i);

    r_model_t *out = (r_model_t *) R_AllocMedia(name, sizeof(r_model_t), R_MEDIA_MODEL);

    out->type = MODEL_BSP_INLINE;
    out->bsp_inline = in;
    out->bounds = in->visible_bounds;

    mod->bounds = Box3_Union(mod->bounds, out->bounds);
    if (i == 0) {
      mod->bsp->worldspawn = out;
    }

    R_RegisterDependency(&mod->media, &out->media);
  }
}

/**
 * @brief Allocates an occlusion query for each block node and light source in the world model.
 * @remarks Other inline models will instead allocate queries as entities.
 */
static void R_LoadBspOcclusionQueries(r_bsp_model_t *bsp) {

  r_bsp_block_t *block = bsp->inline_models->blocks;
  for (int32_t i = 0; i < bsp->inline_models->num_blocks; i++, block++) {

    /*
     * Collectively, block node bounds cover all valid positions in the BSP with no gaps between
     * them. However, during BSP compilation, when faces are assigned to the block node that best
     * contains them, they may actually reside partially outside of the block, and therefore expand
     * the block's visible bounds. For this reason, we use the union of block->node->bounds and
     * block->visible_bounds for the occlusion query AABB.
     */

    const box3_t bounds = Box3_Union(block->node->bounds, block->visible_bounds);
    block->query = R_AllocOcclusionQuery(bounds);
  }

  r_bsp_light_t *light = bsp->lights;
  for (int32_t i = 0; i < bsp->num_lights; i++, light++) {
    light->query = R_AllocOcclusionQuery(light->bounds);
  }
}

/**
 * @brief Extra lumps we need to load for the renderer.
 */
#define R_BSP_LUMPS ( \
  (1 << BSP_LUMP_PATCHES) | \
  (1 << BSP_LUMP_VERTEXES) | \
  (1 << BSP_LUMP_ELEMENTS) | \
  (1 << BSP_LUMP_FACES) | \
  (1 << BSP_LUMP_DRAW_ELEMENTS) | \
  (1 << BSP_LUMP_BLOCKS) | \
  (1 << BSP_LUMP_LIGHTS) | \
  (1 << BSP_LUMP_VOXELS) \
)

/**
 * @brief Loads a BSP model from a binary file buffer, initializing all renderer structures.
 */
static void R_LoadBspModel(r_model_t *mod, void *buffer) {

  bsp_header_t *header = (bsp_header_t *) buffer;

  mod->bsp = Mem_LinkMalloc(sizeof(r_bsp_model_t), mod);
  mod->bsp->cm = Cm_Bsp();

  Bsp_LoadLumps(header, mod->bsp->cm->file, R_BSP_LUMPS);

  R_LoadBspPlanes(mod->bsp);
  R_LoadBspMaterials(mod);
  R_LoadBspBrushSides(mod->bsp);
  R_LoadBspPatches(mod->bsp);
  R_LoadBspVertexes(mod->bsp);
  R_LoadBspElements(mod->bsp);
  R_LoadBspFaces(mod->bsp);
  R_LoadBspLeafs(mod->bsp);
  R_LoadBspNodes(mod->bsp);
  R_LoadBspDrawElements(mod->bsp);
  R_LoadBspBlocks(mod->bsp);
  R_LoadBspInlineModels(mod->bsp);
  R_LoadBspVertexArray(mod);
  R_LoadBspDepthPassVertexArray(mod);
  R_SetupBspInlineModels(mod);
  R_LoadBspLights(mod->bsp);
  R_LoadBspVoxels(mod);
  R_LoadBspOcclusionQueries(mod->bsp);

  Bsp_UnloadLumps(mod->bsp->cm->file, R_BSP_LUMPS);

  Com_Debug(DEBUG_RENDERER, "!================================\n");
  Com_Debug(DEBUG_RENDERER, "!R_LoadBspModel:  %s\n", mod->media.name);
  Com_Debug(DEBUG_RENDERER, "!  Planes:        %d\n", mod->bsp->num_planes);
  Com_Debug(DEBUG_RENDERER, "!  Materials:     %d\n", mod->bsp->num_materials);
  Com_Debug(DEBUG_RENDERER, "!  Brush sides:   %d\n", mod->bsp->num_brush_sides);
  Com_Debug(DEBUG_RENDERER, "!  Patches:       %d\n", mod->bsp->num_patches);
  Com_Debug(DEBUG_RENDERER, "!  Vertexes:      %d\n", mod->bsp->num_vertexes);
  Com_Debug(DEBUG_RENDERER, "!  Elements:      %d\n", mod->bsp->num_elements);
  Com_Debug(DEBUG_RENDERER, "!  Faces:         %d\n", mod->bsp->num_faces);
  Com_Debug(DEBUG_RENDERER, "!  Leafs:         %d\n", mod->bsp->num_leafs);
  Com_Debug(DEBUG_RENDERER, "!  Nodes:         %d\n", mod->bsp->num_nodes);
  Com_Debug(DEBUG_RENDERER, "!  Draw elements: %d\n", mod->bsp->num_draw_elements);
  Com_Debug(DEBUG_RENDERER, "!  Blocks:        %d\n", mod->bsp->num_blocks);
  Com_Debug(DEBUG_RENDERER, "!  Inline models: %d\n", mod->bsp->num_inline_models);
  Com_Debug(DEBUG_RENDERER, "!  Lights:        %d\n", mod->bsp->num_lights);
  Com_Debug(DEBUG_RENDERER, "!  Voxels:        %d\n", mod->bsp->voxels.num_voxels);
  Com_Debug(DEBUG_RENDERER, "!================================\n");
}

/**
 * @brief Registers BSP model media dependencies (voxel data and index textures).
 */
static void R_RegisterBspModel(r_media_t *self) {

  r_model_t *mod = (r_model_t *) self;

  R_RegisterDependency(self, (r_media_t *) mod->bsp->voxels.caustics);
  R_RegisterDependency(self, (r_media_t *) mod->bsp->voxels.occlusion);
  R_RegisterDependency(self, (r_media_t *) mod->bsp->voxels.light_data);
  R_RegisterDependency(self, (r_media_t *) mod->bsp->voxels.light_indices);

  r_models.world = mod;
}

/**
 * @brief Frees all GPU resources allocated for the BSP model.
 */
static void R_FreeBspModel(r_media_t *self) {
  r_model_t *mod = (r_model_t *) self;

  r_bsp_model_t *bsp = mod->bsp;

  bsp->vertex_buffer = release(bsp->vertex_buffer);
  bsp->elements_buffer = release(bsp->elements_buffer);

  r_bsp_block_t *block = bsp->blocks;
  for (int32_t i = 0; i < bsp->num_blocks; i++, block++) {

    release(block->decals.triangles);

    // TODO(#864): release per-block decal Buffer once decals are ported.

    R_FreeOcclusionQuery(block->query);
  }

  r_bsp_light_t *light = bsp->lights;
  for (int32_t i = 0; i < bsp->num_lights; i++, light++) {
    R_FreeOcclusionQuery(light->query);
  }

  bsp->voxels.light_indices_buffer = release(bsp->voxels.light_indices_buffer);

  R_GetError(NULL);
}

/**
 * @brief BSP model format descriptor registering load, register, and free callbacks.
 */
const r_model_format_t r_bsp_model_format = {
  .extension = "bsp",
  .type = MODEL_BSP,
  .Load = R_LoadBspModel,
  .Register = R_RegisterBspModel,
  .Free = R_FreeBspModel
};
