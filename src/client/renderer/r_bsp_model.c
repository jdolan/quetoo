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
 * @brief Loads BSP planes into renderer plane structures.
 */
static void R_LoadBspPlanes(RenderBspModel *bsp) {
  RenderBspPlane *out;

  const CmBspPlane *in = bsp->cm->planes;

  bsp->num_planes = bsp->cm->num_planes;
  bsp->planes = out = Mem_LinkMalloc(bsp->num_planes * sizeof(*out), bsp);

  for (int32_t i = 0; i < bsp->num_planes; i++, out++, in++) {
    out->cm = in;
  }
}

/**
 * @brief Loads and registers BSP materials.
 */
static void R_LoadBspMaterials(RenderModel *mod) {

  RenderMaterial **out;
  const BspMaterial *in = mod->bsp->cm->file->materials;

  mod->bsp->num_materials = mod->bsp->cm->file->num_materials;
  mod->bsp->materials = out = Mem_LinkMalloc(mod->bsp->num_materials * sizeof(*out), mod->bsp);

  for (int32_t i = 0; i < mod->bsp->num_materials; i++, in++, out++) {
    *out = R_LoadMaterial(in->name, ASSET_CONTEXT_TEXTURES);
    R_RegisterDependency((RenderMedia *) mod, (RenderMedia *) *out);
  }
}

/**
 * @brief Loads BSP brush sides.
 */
static void R_LoadBspBrushSides(RenderBspModel *bsp) {
  RenderBspBrushSide *out;

  const BspBrushSide *in = bsp->cm->file->brush_sides;

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
 * @brief Loads BSP patches.
 */
static void R_LoadBspPatches(RenderBspModel *bsp) {

  const BspPatch *in = bsp->cm->file->patches;

  bsp->num_patches = bsp->cm->file->num_patches;
  RenderBspPatch *out = bsp->patches = Mem_LinkMalloc(bsp->num_patches * sizeof(*out), bsp);

  for (int32_t i = 0; i < bsp->num_patches; i++, in++, out++) {

    if (in->material > -1) {
      out->material = bsp->materials[in->material];
    }

    out->contents = in->contents;
    out->surface = in->surface;
  }
}

/**
 * @brief Loads BSP vertex data.
 */
static void R_LoadBspVertexes(RenderBspModel *bsp) {

  bsp->num_vertexes = bsp->cm->file->num_vertexes;
  RenderBspVertex *out = bsp->vertexes = Mem_LinkMalloc(bsp->num_vertexes * sizeof(*out), bsp);

  const BspVertex *in = bsp->cm->file->vertexes;
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
 * @brief Loads BSP triangle indices.
 */
static void R_LoadBspElements(RenderBspModel *bsp) {

  bsp->num_elements = bsp->cm->file->num_elements;
  uint32_t *out = bsp->elements = Mem_LinkMalloc(bsp->num_elements * sizeof(*out), bsp);

  const int32_t *in = bsp->cm->file->elements;
  for (int32_t i = 0; i < bsp->num_elements; i++, in++, out++) {
    *out = *in;
  }
}

/**
 * @brief Loads BSP faces.
 */
static void R_LoadBspFaces(RenderBspModel *bsp) {

  const BspFace *in = bsp->cm->file->faces;
  RenderBspFace *out;

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
 * @brief Loads BSP leafs.
 */
static void R_LoadBspLeafs(RenderBspModel *bsp) {
  RenderBspLeaf *out;

  const BspLeaf *in = bsp->cm->file->leafs;

  bsp->num_leafs = bsp->cm->file->num_leafs;
  bsp->leafs = out = Mem_LinkMalloc(bsp->num_leafs * sizeof(*out), bsp);

  for (int32_t i = 0; i < bsp->num_leafs; i++, in++, out++) {
    out->contents = in->contents;
    out->bounds = in->bounds;
  }
}

/**
 * @brief Loads BSP nodes.
 */
static void R_LoadBspNodes(RenderBspModel *bsp) {
  RenderBspNode *out;

  const BspNode *in = bsp->cm->file->nodes;

  bsp->num_nodes = bsp->cm->file->num_nodes;
  bsp->nodes = out = Mem_LinkMalloc(bsp->num_nodes * sizeof(*out), bsp);

  for (int32_t i = 0; i < bsp->num_nodes; i++, in++, out++) {

    out->contents = in->contents;
    out->plane = bsp->planes + in->plane;
    out->bounds = in->bounds;
    out->visible_bounds = in->visible_bounds;

    out->faces = bsp->faces + in->first_face;
    out->num_faces = in->num_faces;

    RenderBspFace *f = out->faces;
    for (int32_t j = 0; j < out->num_faces; j++, f++) {
      f->node = out;
    }

    for (int32_t j = 0; j < 2; j++) {
      const int32_t c = in->children[j];
      if (c >= 0) {
        out->children[j] = bsp->nodes + c;
      } else {
        out->children[j] = (RenderBspNode *) (bsp->leafs + (-1 - c));
      }
    }
  }
}

/**
 * @brief Links a BSP node to its parent and inline model.
 */
static void R_SetupBspNode(RenderBspInlineModel *model, RenderBspNode *parent, RenderBspNode *node) {

  node->model = model;
  node->parent = parent;

  if (node->contents > CONTENTS_NODE) {
    return;
  }

  R_SetupBspNode(model, node, node->children[0]);
  R_SetupBspNode(model, node, node->children[1]);
}

/**
 * @brief Loads BSP draw batches.
 */
static void R_LoadBspDrawElements(RenderBspModel *bsp) {
  RenderBspDrawElements *out;

  bsp->num_draw_elements = bsp->cm->file->num_draw_elements;
  bsp->draw_elements = out = Mem_LinkMalloc(bsp->num_draw_elements * sizeof(*out), bsp);

  const BspDrawElements *in = bsp->cm->file->draw_elements;
  for (int32_t i = 0; i < bsp->num_draw_elements; i++, in++, out++) {

    if (in->material > -1) {
      out->material = bsp->materials[in->material];
    }
    out->surface = in->surface;

    out->bounds = in->bounds;

    out->elements = (void *) (in->first_element * sizeof(uint32_t));
    out->num_elements = in->num_elements;

    if (out->material && out->material->cm->stage_flags & (STAGE_STRETCH | STAGE_ROTATE)) {

      Vec2 st_mins = Vec2_Mins();
      Vec2 st_maxs = Vec2_Maxs();

      const uint32_t *e = bsp->elements + in->first_element;
      for (int32_t j = 0; j < out->num_elements; j++, e++) {
        const RenderBspVertex *v = &bsp->vertexes[*e];

        st_mins = Vec2_Minf(st_mins, v->diffusemap);
        st_maxs = Vec2_Maxf(st_maxs, v->diffusemap);
      }

      out->st_origin = Vec2_Scale(Vec2_Add(st_mins, st_maxs), .5f);
    }
  }
}

/**
 * @brief Loads BSP blocks and their decal state.
 */
static void R_LoadBspBlocks(RenderBspModel *bsp) {
  RenderBspBlock *out;

  bsp->num_blocks = bsp->cm->file->num_blocks;
  bsp->blocks = out = Mem_LinkMalloc(bsp->num_blocks * sizeof(RenderBspBlock), bsp);

  const BspBlock *in = bsp->cm->file->blocks;
  for (int32_t i = 0; i < bsp->num_blocks; i++, in++, out++) {

    out->node = bsp->nodes + in->node;
    out->draw_elements = bsp->draw_elements + in->first_draw_element;
    out->num_draw_elements = in->num_draw_elements;
    out->visible_bounds = in->visible_bounds;

    for (int32_t j = 0; j < out->num_draw_elements; j++) {
      out->surface |= out->draw_elements[j].surface;
    }

    RenderBspBlockDecals *decals = &out->decals;

    decals->triangles = $(alloc(Vector), initWithSize, sizeof(RenderDecalTriangle));

  }

  const BspFace *in_face = bsp->cm->file->faces;
  RenderBspFace *out_face = bsp->faces;
  for (int32_t i = 0; i < bsp->num_faces; i++, in_face++, out_face++) {
    if (in_face->block >= 0 && in_face->block < bsp->num_blocks) {
      out_face->block = &bsp->blocks[in_face->block];
    }
  }
}

/**
 * @brief Loads BSP inline models.
 */
static void R_LoadBspInlineModels(RenderBspModel *bsp) {
  RenderBspInlineModel *out;

  const BspModel *in = bsp->cm->file->models;

  bsp->num_inline_models = bsp->cm->file->num_models;
  bsp->inline_models = out = Mem_LinkMalloc(bsp->num_inline_models * sizeof(*out), bsp);

  for (int32_t i = 0; i < bsp->num_inline_models; i++, in++, out++) {

    out->entity = bsp->cm->entities[in->entity];
    out->head_node = bsp->nodes + in->head_node;

    out->visible_bounds = in->visible_bounds;

    out->faces = bsp->faces + in->first_face;
    out->num_faces = in->num_faces;

    out->depth_pass_elements = bsp->draw_elements + in->first_depth_pass_elements;
    out->num_depth_pass_elements = in->num_depth_pass_elements;

    out->draw_elements = bsp->draw_elements + in->first_draw_elements;
    out->num_draw_elements = in->num_draw_elements;

    out->blocks = bsp->blocks + in->first_block;
    out->num_blocks = in->num_blocks;

    R_SetupBspNode(out, NULL, out->head_node);
  }
}

/**
 * @brief Loads BSP portals, attaching each to the draw elements of the face that shows it.
 * @details The compiler resolves a portal's two frames and the draw elements its face was
 * emitted to, so all that is left here is to build the two frames, and to find the inline model
 * the face belongs to. The transform between them is composed per frame, since the model drawing
 * the face may be a mover.
 */
static void R_LoadBspPortals(RenderModel *mod) {

  RenderBspModel *bsp = mod->bsp;

  bsp->num_portals = bsp->cm->file->num_portals;
  if (!bsp->num_portals) {
    return;
  }

  RenderBspPortal *out = bsp->portals = Mem_LinkMalloc(sizeof(*out) * bsp->num_portals, bsp);

  const BspPortal *in = bsp->cm->file->portals;
  for (int32_t i = 0; i < bsp->num_portals; i++, in++, out++) {

    if (in->draw_elements < 0 || in->draw_elements >= bsp->num_draw_elements) {
      Com_Warn("Portal @ %s has invalid draw elements %d\n", vtos(in->entry_origin), in->draw_elements);
      continue;
    }

    out->origin = in->entry_origin;
    out->bounds = bsp->draw_elements[in->draw_elements].bounds;
    out->normal = Vec3_Negate(in->entry_forward);

    out->entry = Mat4_FromVectors(in->entry_forward,
                                  Vec3_Cross(in->entry_forward, in->entry_up),
                                  in->entry_up,
                                  in->entry_origin);

    out->exit = Mat4_FromVectors(in->exit_forward,
                                 Vec3_Cross(in->exit_forward, in->exit_up),
                                 in->exit_up,
                                 in->exit_origin);

    bsp->draw_elements[in->draw_elements].portal = out;

    for (int32_t j = 0; j < bsp->num_inline_models; j++) {

      const RenderBspInlineModel *m = &bsp->inline_models[j];
      const ptrdiff_t first = m->draw_elements - bsp->draw_elements;

      if (in->draw_elements >= first && in->draw_elements < first + m->num_draw_elements) {
        out->model = (RenderModel *) R_FindMedia(va("%s#%d", mod->media.name, j), R_MEDIA_MODEL);
        break;
      }
    }
  }
}

/**
 * @brief Loads BSP lights.
 *//**
 * @brief Loads BSP lights.
 */
static void R_LoadBspLights(RenderBspModel *bsp) {

  const BspLight *in = bsp->cm->file->lights;

  bsp->num_lights = bsp->cm->file->num_lights;
  RenderBspLight *out = bsp->lights = Mem_LinkMalloc(sizeof(*out) * bsp->num_lights, bsp);

  for (int32_t i = 0; i < bsp->num_lights; i++, in++, out++) {

    out->entity = bsp->cm->entities[in->entity];
    out->origin = in->origin;
    out->color = in->color;
    out->radius = in->radius;
    out->intensity = in->intensity;
    out->bounds = in->bounds;
    q_strlcpy(out->style, in->style, sizeof(out->style));
    out->drift = in->drift;
    out->draw_elements = bsp->draw_elements + in->first_draw_elements;
    out->num_draw_elements = in->num_draw_elements;
    out->target_entity = in->target_entity > 0 ? bsp->cm->entities[in->target_entity] : NULL;
  }
}

/**
 * @brief Appends merged voxel bounds to an occlusion query.
 */
static void R_AppendOcclusionQueryVoxels(RenderOcclusionQuery *query, const BspVoxels *voxels, const int32_t *indices, int32_t num_indices) {

  if (!num_indices) {
    return;
  }

  Box3 *boxes = Mem_Malloc(num_indices * sizeof(Box3));

  const int32_t xy = voxels->size.x * voxels->size.y;

  for (int32_t i = 0; i < num_indices; i++) {
    const int32_t index = indices[i];

    const int32_t z = index / xy;
    const int32_t rem = index % xy;
    const int32_t y = rem / voxels->size.x;
    const int32_t x = rem % voxels->size.x;

    const Vec3 mins = Vec3_Add(voxels->bounds.mins, Vec3_Scale(MakeVec3(x, y, z), BSP_VOXEL_SIZE));
    const Vec3 maxs = Vec3_Add(mins, MakeVec3(BSP_VOXEL_SIZE, BSP_VOXEL_SIZE, BSP_VOXEL_SIZE));

    boxes[i] = MakeBox3(mins, maxs);
  }

  Box3 *merged;
  const size_t num_merged = Box3_Merge(boxes, num_indices, &merged);

  Mem_Free(boxes);

  for (size_t i = 0; i < num_merged; i++) {
    R_AppendOcclusionQueryBox(query, merged[i]);
  }

  free(merged);
}

/**
 * @brief Builds BSP block and light occlusion queries from voxel coverage.
 */
static void R_LoadBspOcclusionQueries(RenderBspModel *bsp) {

  const BspFile *file = bsp->cm->file;
  const BspVoxels *voxels = file->voxels;

  RenderBspBlock *block = bsp->blocks;
  const BspBlock *in_block = file->blocks;
  for (int32_t i = 0; i < bsp->num_blocks; i++, block++, in_block++) {

    const Box3 bounds = Box3_Union(block->node->bounds, block->visible_bounds);
    block->query = R_AllocOcclusionQuery(bounds);

    R_AppendOcclusionQueryVoxels(block->query, voxels,
                                 file->block_voxels + in_block->first_voxel, in_block->num_voxels);

    if (!block->query->num_boxes) {
      R_AppendOcclusionQueryBox(block->query, bounds);
    }
  }

  RenderBspLight *light = bsp->lights;
  const BspLight *in_light = file->lights;
  for (int32_t i = 0; i < bsp->num_lights; i++, light++, in_light++) {

    light->query = R_AllocOcclusionQuery(light->bounds);

    R_AppendOcclusionQueryVoxels(light->query, voxels,
                                 file->light_voxels + in_light->first_voxel, in_light->num_voxels);

    if (!light->query->num_boxes) {
      R_AppendOcclusionQueryBox(light->query, light->bounds);
    }
  }
}

/**
 * @brief Loads BSP voxel lighting data.
 */
static void R_LoadBspVoxels(RenderModel *mod) {

  const BspVoxels *in = mod->bsp->cm->file->voxels;
  const byte *data = (byte *) in + sizeof(BspVoxels);

  RenderBspVoxels *out = &mod->bsp->voxels;

  out->size = in->size;
  out->num_voxels = out->size.x * out->size.y * out->size.z;
  out->num_light_indices = in->num_light_indices;
  out->bounds = in->bounds;

  const byte *caustics_data = data;
  data += out->num_voxels * sizeof(byte) * 3;

  out->caustics = (RenderImage *) R_AllocMedia("voxel_caustics", sizeof(RenderImage), R_MEDIA_IMAGE);
  out->caustics->media.Free = R_FreeImage;
  out->caustics->type = IMG_VOXELS;
  out->caustics->width = out->size.x;
  out->caustics->height = out->size.y;
  out->caustics->depth = out->size.z;

  byte *caustics_rgba = Mem_Malloc(out->num_voxels * 4);
  for (int32_t i = 0; i < out->num_voxels; i++) {
    caustics_rgba[i * 4 + 0] = caustics_data[i * 3 + 0];
    caustics_rgba[i * 4 + 1] = caustics_data[i * 3 + 1];
    caustics_rgba[i * 4 + 2] = caustics_data[i * 3 + 2];
    caustics_rgba[i * 4 + 3] = 255;
  }

  out->caustics->texture = $(r_context.device, createTexture, &(SDL_GPUTextureCreateInfo) {
    .type = SDL_GPU_TEXTURETYPE_3D,
    .format = SDL_GPU_TEXTUREFORMAT_R8G8B8A8_UNORM,
    .usage = SDL_GPU_TEXTUREUSAGE_SAMPLER,
    .width = (Uint32) out->size.x,
    .height = (Uint32) out->size.y,
    .layer_count_or_depth = (Uint32) out->size.z,
    .num_levels = 1,
    .sample_count = SDL_GPU_SAMPLECOUNT_1,
  }, caustics_rgba);

  Mem_Free(caustics_rgba);

  const int32_t *light_data = (const int32_t *) data;
  data += out->num_voxels * sizeof(int32_t) * 2;

  out->light_data = (RenderImage *) R_AllocMedia("voxel_light_data", sizeof(RenderImage), R_MEDIA_IMAGE);
  out->light_data->media.Free = R_FreeImage;
  out->light_data->type = IMG_VOXELS;
  out->light_data->width = out->size.x;
  out->light_data->height = out->size.y;
  out->light_data->depth = out->size.z;

  out->light_data_buffer = $(r_context.device, createBufferWithConstMem,
      SDL_GPU_BUFFERUSAGE_GRAPHICS_STORAGE_READ,
      light_data,
      out->num_voxels * sizeof(int32_t) * 2);

  const int32_t *light_indices_data = (const int32_t *) data;
  data += out->num_light_indices * sizeof(int32_t);

  if (out->num_light_indices > 0) {
    out->light_indices_buffer = $(r_context.device, createBufferWithConstMem,
        SDL_GPU_BUFFERUSAGE_GRAPHICS_STORAGE_READ,
        light_indices_data,
        out->num_light_indices * sizeof(int32_t));
  }

  out->light_indices = (RenderImage *) R_AllocMedia("voxel_light_indices", sizeof(RenderImage), R_MEDIA_IMAGE);
  out->light_indices->media.Free = R_FreeImage;
  out->light_indices->type = IMG_VOXELS;

  const byte *occlusion_data = data;

  out->occlusion = (RenderImage *) R_AllocMedia("voxel_occlusion", sizeof(RenderImage), R_MEDIA_IMAGE);
  out->occlusion->media.Free = R_FreeImage;
  out->occlusion->type = IMG_VOXELS;
  out->occlusion->width = out->size.x;
  out->occlusion->height = out->size.y;
  out->occlusion->depth = out->size.z;

  out->occlusion->texture = $(r_context.device, createTexture, &(SDL_GPUTextureCreateInfo) {
    .type = SDL_GPU_TEXTURETYPE_3D,
    .format = SDL_GPU_TEXTUREFORMAT_R8G8_UNORM,
    .usage = SDL_GPU_TEXTUREUSAGE_SAMPLER,
    .width = (Uint32) out->size.x,
    .height = (Uint32) out->size.y,
    .layer_count_or_depth = (Uint32) out->size.z,
    .num_levels = 1,
    .sample_count = SDL_GPU_SAMPLECOUNT_1,
  }, occlusion_data);

  if (r_draw_bsp_voxels->value) {
    
    out->voxels = Mem_LinkMalloc(out->num_voxels * sizeof(RenderBspVoxel), mod->bsp);

    for (int32_t u = 0; u < out->size.z; u++) {
      for (int32_t t = 0; t < out->size.y; t++) {
        for (int32_t s = 0; s < out->size.x; s++) {
          const int32_t voxel_index = (u * out->size.y + t) * out->size.x + s;
          RenderBspVoxel *voxel = &out->voxels[voxel_index];

          const Vec3 voxel_mins = MakeVec3(
            out->bounds.mins.x + s * BSP_VOXEL_SIZE,
            out->bounds.mins.y + t * BSP_VOXEL_SIZE,
            out->bounds.mins.z + u * BSP_VOXEL_SIZE
          );

          const Vec3 voxel_maxs = Vec3_Add(voxel_mins, MakeVec3(BSP_VOXEL_SIZE, BSP_VOXEL_SIZE, BSP_VOXEL_SIZE));
          voxel->bounds = MakeBox3(voxel_mins, voxel_maxs);

          const int32_t first_light_index = light_data[voxel_index * 2 + 0];
          const int32_t num_light_indices = light_data[voxel_index * 2 + 1];

          voxel->num_lights = num_light_indices;

          if (voxel->num_lights > 0) {
            voxel->lights = Mem_LinkMalloc(voxel->num_lights * sizeof(RenderBspLight *), mod->bsp);

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

}

/**
 * @brief Creates BSP vertex and index buffers.
 */
static void R_LoadBspVertexArray(RenderModel *mod) {

  RenderBspModel *bsp = mod->bsp;

  bsp->vertex_buffer = $(r_context.device, createBufferWithConstMem, SDL_GPU_BUFFERUSAGE_VERTEX,
                         bsp->vertexes, bsp->num_vertexes * sizeof(RenderBspVertex));

  bsp->elements_buffer = $(r_context.device, createBufferWithConstMem, SDL_GPU_BUFFERUSAGE_INDEX,
                           bsp->elements, bsp->num_elements * sizeof(uint32_t));

  $(bsp->vertex_buffer, setName, va("%s vertexes", mod->media.name));
  $(bsp->elements_buffer, setName, va("%s elements", mod->media.name));
}

/**
 * @brief Creates renderer models for BSP inline models.
 */
static void R_SetupBspInlineModels(RenderModel *mod) {

  RenderBspInlineModel *in = mod->bsp->inline_models;
  for (int32_t i = 0; i < mod->bsp->num_inline_models; i++, in++) {

    char name[MAX_QPATH];
    q_snprintf(name, sizeof(name), "%s#%d", mod->media.name, i);

    RenderModel *out = (RenderModel *) R_AllocMedia(name, sizeof(RenderModel), R_MEDIA_MODEL);

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
 * @brief Loads the sky cubemap specified in worldspawn.
 */
static void R_LoadBspSky(RenderModel *mod) {

  const char *name = Cm_EntityValue(Cm_Worldspawn(), "sky")->nullable_string;
  if (name) {
    mod->bsp->sky = R_LoadImage(va("sky/%s", name), IMG_CUBEMAP);
  } else {
    mod->bsp->sky = NULL;
  }

  if (mod->bsp->sky == NULL) {
    if (name) {
      Com_Warn("Failed to load sky sky/%s\n", name);
    } else {
      Com_Warn("Failed to load sky (no sky key on worldspawn)\n");
    }

    mod->bsp->sky = R_LoadImage("sky/template", IMG_CUBEMAP);
    if (mod->bsp->sky == NULL) {
      Com_Error(ERROR_DROP, "Failed to load default sky\n");
    }
  }
}

/**
 * @brief BSP lumps required by the renderer.
 */
#define R_BSP_LUMPS ( \
  (1 << BSP_LUMP_PATCHES) | \
  (1 << BSP_LUMP_VERTEXES) | \
  (1 << BSP_LUMP_ELEMENTS) | \
  (1 << BSP_LUMP_FACES) | \
  (1 << BSP_LUMP_DRAW_ELEMENTS) | \
  (1 << BSP_LUMP_BLOCKS) | \
  (1 << BSP_LUMP_LIGHTS) | \
  (1 << BSP_LUMP_VOXELS) | \
  (1 << BSP_LUMP_LIGHT_VOXELS) | \
  (1 << BSP_LUMP_BLOCK_VOXELS) | \
  (1 << BSP_LUMP_PORTALS) \
)

/**
 * @brief Loads a BSP model into renderer structures.
 */
static void R_LoadBspModel(RenderModel *mod, void *buffer) {

  BspHeader *header = (BspHeader *) buffer;

  mod->bsp = Mem_LinkMalloc(sizeof(RenderBspModel), mod);
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
  R_SetupBspInlineModels(mod);
  R_LoadBspLights(mod->bsp);
  R_LoadBspPortals(mod);
  R_FreeOcclusionQueries();
  R_LoadBspOcclusionQueries(mod->bsp);
  R_LoadBspVoxels(mod);
  R_LoadBspSky(mod);

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
 * @brief Registers BSP model media dependencies.
 */
static void R_RegisterBspModel(RenderMedia *self) {

  RenderModel *mod = (RenderModel *) self;

  R_RegisterDependency(self, (RenderMedia *) mod->bsp->voxels.caustics);
  R_RegisterDependency(self, (RenderMedia *) mod->bsp->voxels.occlusion);
  R_RegisterDependency(self, (RenderMedia *) mod->bsp->voxels.light_data);
  R_RegisterDependency(self, (RenderMedia *) mod->bsp->voxels.light_indices);
  R_RegisterDependency(self, (RenderMedia *) mod->bsp->sky);

  r_models.world = mod;
}

/**
 * @brief Frees BSP model GPU resources.
 */
static void R_FreeBspModel(RenderMedia *self) {
  RenderModel *mod = (RenderModel *) self;

  RenderBspModel *bsp = mod->bsp;

  bsp->vertex_buffer = release(bsp->vertex_buffer);
  bsp->elements_buffer = release(bsp->elements_buffer);

  RenderBspBlock *block = bsp->blocks;
  for (int32_t i = 0; i < bsp->num_blocks; i++, block++) {

    release(block->decals.triangles);
    block->decals.vertex_buffer = release(block->decals.vertex_buffer);
  }

  bsp->voxels.light_data_buffer = release(bsp->voxels.light_data_buffer);
  bsp->voxels.light_indices_buffer = release(bsp->voxels.light_indices_buffer);

}

/**
 * @brief BSP model format descriptor.
 */
const RenderModelFormat r_bsp_model_format = {
  .extension = "bsp",
  .type = MODEL_BSP,
  .Load = R_LoadBspModel,
  .Register = R_RegisterBspModel,
  .Free = R_FreeBspModel
};
