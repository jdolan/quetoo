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

/**
 * @brief
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
 * @brief
 */
static void R_LoadBspMaterials(r_model_t *mod) {
  char path[MAX_QPATH];

  g_snprintf(path, sizeof(path), "%s.mat", mod->media.name);

  GList *materials = NULL;

  R_LoadMaterials(path, ASSET_CONTEXT_TEXTURES, &materials);

  g_list_free(materials);

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
 * @brief
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
 * @brief
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
 * @brief
 */
static void R_LoadBspElements(r_bsp_model_t *bsp) {

  bsp->num_elements = bsp->cm->file->num_elements;
  GLuint *out = bsp->elements = Mem_LinkMalloc(bsp->num_elements * sizeof(*out), bsp);

  const int32_t *in = bsp->cm->file->elements;
  for (int32_t i = 0; i < bsp->num_elements; i++, in++, out++) {
    *out = *in;
  }
}

/**
 * @brief Loads all r_bsp_face_t for the specified BSP model.
 */
static void R_LoadBspFaces(r_bsp_model_t *bsp) {

  const bsp_face_t *in = bsp->cm->file->faces;
  r_bsp_face_t *out;

  bsp->num_faces = bsp->cm->file->num_faces;
  bsp->faces = out = Mem_LinkMalloc(bsp->num_faces * sizeof(*out), bsp);

  for (int32_t i = 0; i < bsp->num_faces; i++, in++, out++) {

    out->brush_side = bsp->brush_sides + in->brush_side;
    out->plane = bsp->planes + in->plane;

    out->bounds = in->bounds;

    out->vertexes = bsp->vertexes + in->first_vertex;
    out->num_vertexes = in->num_vertexes;

    out->elements = (GLvoid *) (in->first_element * sizeof(GLuint));
    out->num_elements = in->num_elements;
  }
}

/**
 * @brief Loads all r_bsp_leaf_t for the specified BSP model.
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
 * @brief Loads all r_bsp_node_t for the specified BSP model.
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

  r_bsp_face_t *face = node->faces;
  for (int32_t i = 0; i < node->num_faces; i++, face++) {
    face->node = node;
  }

  R_SetupBspNode(model, node, node->children[0]);
  R_SetupBspNode(model, node, node->children[1]);
}

/**
 * @brief
 */
static void R_LoadBspDrawElements(r_bsp_model_t *bsp) {
  r_bsp_draw_elements_t *out;

  bsp->num_draw_elements = bsp->cm->file->num_draw_elements;
  bsp->draw_elements = out = Mem_LinkMalloc(bsp->num_draw_elements * sizeof(*out), bsp);

  const bsp_draw_elements_t *in = bsp->cm->file->draw_elements;
  for (int32_t i = 0; i < bsp->num_draw_elements; i++, in++, out++) {

    out->plane = bsp->planes + in->plane;
    out->material = bsp->materials[in->material];
    out->surface = in->surface;

    out->bounds = in->bounds;

    out->elements = (GLvoid *) (in->first_element * sizeof(GLuint));
    out->num_elements = in->num_elements;

    if ((out->surface & SURF_ALPHA_TEST) && out->material->cm->alpha_test == 0.f) {
      out->material->cm->alpha_test = MATERIAL_ALPHA_TEST;
    }

    if (out->material->cm->stage_flags & (STAGE_STRETCH | STAGE_ROTATE)) {

      vec2_t st_mins = Vec2_Mins();
      vec2_t st_maxs = Vec2_Maxs();

      const GLuint *e = bsp->elements + in->first_element;
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
 * @brief
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
  }
}

/**
 * @brief
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

    out->depth_pass_elements = (GLvoid *) (in->first_depth_pass_element * sizeof(GLuint));
    out->num_depth_pass_elements = in->num_depth_pass_elements;

    out->draw_elements = bsp->draw_elements + in->first_draw_elements;
    out->num_draw_elements = in->num_draw_elements;

    out->blocks = bsp->blocks + in->first_block;
    out->num_blocks = in->num_blocks;

    R_SetupBspNode(out, NULL, out->head_node);
  }
}

/**
 * @brief
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
    out->depth_pass_elements = (GLvoid *) (in->first_depth_pass_element * sizeof(GLuint));
    out->num_depth_pass_elements = in->num_depth_pass_elements;
  }
}

/**
 * @brief
 */
static void R_LoadBspVoxels(r_model_t *mod) {

  const bsp_voxels_t *in = mod->bsp->cm->file->voxels;
  const byte *data = (byte *) in + sizeof(bsp_voxels_t);

  r_bsp_voxels_t *out = mod->bsp->voxels = Mem_LinkMalloc(sizeof(*out), mod->bsp);

  out->size = in->size;
  out->num_voxels = out->size.x * out->size.y * out->size.z;
  out->num_light_indices = in->num_light_indices;

  const vec3_t voxels_size = Vec3_Scale(Vec3i_CastVec3(out->size), BSP_VOXEL_SIZE);
  const vec3_t world_size = Box3_Size(mod->bsp->inline_models->visible_bounds);
  const vec3_t padding = Vec3_Scale(Vec3_Subtract(voxels_size, world_size), 0.5f);

  out->bounds = Box3_Expand3(mod->bsp->inline_models->visible_bounds, padding);

  const GLsizei levels = log2f(Mini(Mini(out->size.x, out->size.y), out->size.z)) + 1;

  out->diffuse = (r_image_t *) R_AllocMedia("voxel_diffuse", sizeof(r_image_t), R_MEDIA_IMAGE);
  out->diffuse->media.Free = R_FreeImage;
  out->diffuse->type = IMG_VOXELS;
  out->diffuse->width = out->size.x;
  out->diffuse->height = out->size.y;
  out->diffuse->depth = out->size.z;
  out->diffuse->target = GL_TEXTURE_3D;
  out->diffuse->levels = levels;
  out->diffuse->minify = GL_LINEAR_MIPMAP_LINEAR;
  out->diffuse->magnify = GL_LINEAR;
  out->diffuse->internal_format = GL_RGBA8;
  out->diffuse->format = GL_RGBA;
  out->diffuse->pixel_type = GL_UNSIGNED_BYTE;

  glActiveTexture(GL_TEXTURE0 + TEXTURE_VOXEL_DIFFUSE);

  R_UploadImage(out->diffuse, data);
  data += out->num_voxels * sizeof(color32_t);

  out->fog = (r_image_t *) R_AllocMedia("voxel_fog", sizeof(r_image_t), R_MEDIA_IMAGE);
  out->fog->media.Free = R_FreeImage;
  out->fog->type = IMG_VOXELS;
  out->fog->width = out->size.x;
  out->fog->height = out->size.y;
  out->fog->depth = out->size.z;
  out->fog->target = GL_TEXTURE_3D;
  out->fog->levels = levels;
  out->fog->minify = GL_LINEAR_MIPMAP_LINEAR;
  out->fog->magnify = GL_LINEAR;
  out->fog->internal_format = GL_RGBA8;
  out->fog->format = GL_RGBA;
  out->fog->pixel_type = GL_UNSIGNED_BYTE;

  glActiveTexture(GL_TEXTURE0 + TEXTURE_VOXEL_FOG);

  R_UploadImage(out->fog, data);
  data += out->num_voxels * sizeof(color32_t);

  out->light_data = (r_image_t *) R_AllocMedia("voxel_light_data", sizeof(r_image_t), R_MEDIA_IMAGE);
  out->light_data->media.Free = R_FreeImage;
  out->light_data->type = IMG_VOXELS;
  out->light_data->width = out->size.x;
  out->light_data->height = out->size.y;
  out->light_data->depth = out->size.z;
  out->light_data->target = GL_TEXTURE_3D;
  out->light_data->minify = GL_NEAREST;
  out->light_data->magnify = GL_NEAREST;
  out->light_data->internal_format = GL_RG32I;
  out->light_data->format = GL_RG_INTEGER;
  out->light_data->pixel_type = GL_INT;

  glActiveTexture(GL_TEXTURE0 + TEXTURE_VOXEL_LIGHT_DATA);

  R_UploadImage(out->light_data, data);
  data += out->num_voxels * sizeof(int32_t) * 2;

  glGenBuffers(1, &out->light_indices_buffer);
  glBindBuffer(GL_TEXTURE_BUFFER, out->light_indices_buffer);
  glBufferData(GL_TEXTURE_BUFFER, out->num_light_indices * sizeof(int32_t), data, GL_STATIC_DRAW);
  glBindBuffer(GL_TEXTURE_BUFFER, 0);

  out->light_indices = (r_image_t *) R_AllocMedia("voxel_light_indices", sizeof(r_image_t), R_MEDIA_IMAGE);
  out->light_indices->media.Free = R_FreeImage;
  out->light_indices->type = IMG_VOXELS;
  out->light_data->target = GL_TEXTURE_BUFFER;
  out->light_data->internal_format = GL_R32I;
  out->light_data->buffer = out->light_indices_buffer;

  glActiveTexture(GL_TEXTURE0 + TEXTURE_VOXEL_LIGHT_INDICES);

  R_SetupImage(out->light_data);

  glActiveTexture(GL_TEXTURE0 + TEXTURE_DIFFUSEMAP);

  R_GetError(NULL);
}

/**
 * @brief
 */
static void R_LoadBspVertexArray(r_model_t *mod) {

  glGenBuffers(1, &mod->bsp->vertex_buffer);
  glBindBuffer(GL_ARRAY_BUFFER, mod->bsp->vertex_buffer);
  glBufferData(GL_ARRAY_BUFFER, mod->bsp->num_vertexes * sizeof(r_bsp_vertex_t), mod->bsp->vertexes, GL_STATIC_DRAW);

  glGenBuffers(1, &mod->bsp->elements_buffer);
  glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, mod->bsp->elements_buffer);
  glBufferData(GL_ELEMENT_ARRAY_BUFFER, mod->bsp->num_elements * sizeof(GLuint), mod->bsp->elements, GL_STATIC_DRAW);

  glGenVertexArrays(1, &mod->bsp->vertex_array);
  glBindVertexArray(mod->bsp->vertex_array);

  glBindBuffer(GL_ARRAY_BUFFER, mod->bsp->vertex_buffer);
  glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, mod->bsp->elements_buffer);

  glVertexAttribPointer(0, 3, GL_FLOAT, GL_FALSE, sizeof(r_bsp_vertex_t), (void *) offsetof(r_bsp_vertex_t, position));
  glVertexAttribPointer(1, 3, GL_FLOAT, GL_FALSE, sizeof(r_bsp_vertex_t), (void *) offsetof(r_bsp_vertex_t, normal));
  glVertexAttribPointer(2, 3, GL_FLOAT, GL_FALSE, sizeof(r_bsp_vertex_t), (void *) offsetof(r_bsp_vertex_t, tangent));
  glVertexAttribPointer(3, 3, GL_FLOAT, GL_FALSE, sizeof(r_bsp_vertex_t), (void *) offsetof(r_bsp_vertex_t, bitangent));
  glVertexAttribPointer(4, 2, GL_FLOAT, GL_FALSE, sizeof(r_bsp_vertex_t), (void *) offsetof(r_bsp_vertex_t, diffusemap));
  glVertexAttribPointer(5, 4, GL_UNSIGNED_BYTE, GL_TRUE, sizeof(r_bsp_vertex_t), (void *) offsetof(r_bsp_vertex_t, color));

  glEnableVertexAttribArray(0);
  glEnableVertexAttribArray(1);
  glEnableVertexAttribArray(2);
  glEnableVertexAttribArray(3);
  glEnableVertexAttribArray(4);
  glEnableVertexAttribArray(5);

  glBindVertexArray(0);

  R_GetError(mod->media.name);
}

/**
 * @brief
 */
static void R_LoadBspDepthPassVertexArray(r_model_t *mod) {

  glGenVertexArrays(1, &mod->bsp->depth_pass.vertex_array);
  glBindVertexArray(mod->bsp->depth_pass.vertex_array);

  glBindBuffer(GL_ARRAY_BUFFER, mod->bsp->vertex_buffer);
  glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, mod->bsp->elements_buffer);

  glVertexAttribPointer(0, 3, GL_FLOAT, GL_FALSE, sizeof(r_bsp_vertex_t), (void *) offsetof(r_bsp_vertex_t, position));
  glEnableVertexAttribArray(0);

  glBindVertexArray(0);

  R_GetError(mod->media.name);
}

/**
 * @brief Creates an `r_model_t` for each inline model so that entities may reference them.
 */
static void R_SetupBspInlineModels(r_model_t *mod) {

  r_bsp_inline_model_t *in = mod->bsp->inline_models;
  for (int32_t i = 0; i < mod->bsp->num_inline_models; i++, in++) {

    char name[MAX_QPATH];
    g_snprintf(name, sizeof(name), "%s#%d", mod->media.name, i);

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
    block->query = R_AllocOcclusionQuery(block->visible_bounds);
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
  (1 << BSP_LUMP_VERTEXES) | \
  (1 << BSP_LUMP_ELEMENTS) | \
  (1 << BSP_LUMP_FACES) | \
  (1 << BSP_LUMP_DRAW_ELEMENTS) | \
  (1 << BSP_LUMP_BLOCKS) | \
  (1 << BSP_LUMP_LIGHTS) | \
  (1 << BSP_LUMP_VOXELS) \
)

/**
 * @brief
 */
static void R_LoadBspModel(r_model_t *mod, void *buffer) {

  bsp_header_t *header = (bsp_header_t *) buffer;

  mod->bsp = Mem_LinkMalloc(sizeof(r_bsp_model_t), mod);
  mod->bsp->cm = Cm_Bsp();

  // load in lumps that the renderer needs
  Bsp_LoadLumps(header, mod->bsp->cm->file, R_BSP_LUMPS);

  R_LoadBspPlanes(mod->bsp);
  R_LoadBspMaterials(mod);
  R_LoadBspBrushSides(mod->bsp);
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

  if (r_draw_bsp_voxels->value) {
    Bsp_UnloadLumps(mod->bsp->cm->file, R_BSP_LUMPS & ~(1 << BSP_LUMP_VOXELS));
  } else {
    Bsp_UnloadLumps(mod->bsp->cm->file, R_BSP_LUMPS);
  }

  R_LoadBspOcclusionQueries(mod->bsp);

  Com_Debug(DEBUG_RENDERER, "!================================\n");
  Com_Debug(DEBUG_RENDERER, "!R_LoadBspModel:      %s\n", mod->media.name);
  Com_Debug(DEBUG_RENDERER, "!  Vertexes:          %d\n", mod->bsp->num_vertexes);
  Com_Debug(DEBUG_RENDERER, "!  Elements:          %d\n", mod->bsp->num_elements);
  Com_Debug(DEBUG_RENDERER, "!  Faces:             %d\n", mod->bsp->num_faces);
  Com_Debug(DEBUG_RENDERER, "!  Draw elements:     %d\n", mod->bsp->num_draw_elements);
  Com_Debug(DEBUG_RENDERER, "!================================\n");
}

/**
 * @brief
 */
static void R_RegisterBspModel(r_media_t *self) {

  r_model_t *mod = (r_model_t *) self;

  R_RegisterDependency(self, (r_media_t *) mod->bsp->voxels->diffuse);
  R_RegisterDependency(self, (r_media_t *) mod->bsp->voxels->fog);
  R_RegisterDependency(self, (r_media_t *) mod->bsp->voxels->light_data);
  R_RegisterDependency(self, (r_media_t *) mod->bsp->voxels->light_indices);

  r_models.world = mod;
}

/**
 * @brief
 */
static void R_FreeBspModel(r_media_t *self) {
  r_model_t *mod = (r_model_t *) self;

  r_bsp_model_t *bsp = mod->bsp;

  glDeleteBuffers(1, &bsp->vertex_buffer);
  glDeleteBuffers(1, &bsp->elements_buffer);

  glDeleteVertexArrays(1, &bsp->vertex_array);
  glDeleteVertexArrays(1, &bsp->depth_pass.vertex_array);

  r_bsp_block_t *block = bsp->inline_models->blocks;
  for (int32_t j = 0; j < bsp->inline_models->num_blocks; j++, block++) {
    R_FreeOcclusionQuery(block->query);
  }

  r_bsp_light_t *light = bsp->lights;
  for (int32_t i = 0; i < bsp->num_lights; i++, light++) {
    R_FreeOcclusionQuery(light->query);
  }

  glDeleteBuffers(1, &bsp->voxels->light_indices_buffer);

  R_GetError(NULL);
}

/**
 * @brief
 */
const r_model_format_t r_bsp_model_format = {
  .extension = "bsp",
  .type = MODEL_BSP,
  .Load = R_LoadBspModel,
  .Register = R_RegisterBspModel,
  .Free = R_FreeBspModel
};
