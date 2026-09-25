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
 * Foundation, Inc., 59 Temple Place - Suite 330, Boston, MA 02111-1307, USA.
 */

#include "r_local.h"

/**
 * @brief Loads a mesh config from a file.
 */
static void R_LoadMeshConfig(RenderMeshConfig *config, const char *path) {
  void *buf;
  char token[MAX_STRING_CHARS];

  memset(config, 0, sizeof(*config));

  config->transform = Mat4_Identity();
  config->scale = 1.f;

  if (Fs_Load(path, &buf) == -1) {
    return;
  }

  Parser parser = Parse_Init((const char *) buf, PARSER_DEFAULT);

  while (true) {

    if (!Parse_Token(&parser, PARSE_DEFAULT, token, sizeof(token))) {
      break;
    }

    if (!q_strcmp(token, "translate")) {

      Vec3 v;
      if (Parse_Primitive(&parser, PARSE_DEFAULT | PARSE_WITHIN_QUOTES | PARSE_NO_WRAP, PARSE_FLOAT, v.xyz, 3) != 3) {
        break;
      }

      config->translate = v;
      config->transform = Mat4_ConcatTranslation(config->transform, v);
      continue;
    }

    if (!q_strcmp(token, "rotate")) {

      Vec3 v;
      if (Parse_Primitive(&parser, PARSE_DEFAULT | PARSE_WITHIN_QUOTES | PARSE_NO_WRAP, PARSE_FLOAT, v.xyz, 3) != 3) {
        break;
      }

      config->rotate = v;
      config->transform = Mat4_ConcatRotation3(config->transform, v);
      continue;
    }

    if (!q_strcmp(token, "scale")) {

      float v;
      if (!Parse_Primitive(&parser, PARSE_DEFAULT | PARSE_WITHIN_QUOTES | PARSE_NO_WRAP, PARSE_FLOAT, &v, 1)) {
        break;
      }

      config->scale = v;
      config->transform = Mat4_ConcatScale(config->transform, v);
      continue;
    }

    if (!q_strcmp(token, "muzzle")) {

      Vec3 v;
      if (Parse_Primitive(&parser, PARSE_DEFAULT | PARSE_WITHIN_QUOTES | PARSE_NO_WRAP, PARSE_FLOAT, v.xyz, 3) != 3) {
        break;
      }

      config->muzzle = v;
      continue;
    }
  }

  Fs_Free(buf);
}

/**
 * @brief Loads the mesh configs for a model.
 */
void R_LoadMeshConfigs(RenderModel *mod) {
  char path[MAX_QPATH];

  Dirname(mod->media.name, path);

  R_LoadMeshConfig(&mod->mesh->config.world, va("%s/world.cfg", path));
  R_LoadMeshConfig(&mod->mesh->config.link, va("%s/link.cfg", path));
  R_LoadMeshConfig(&mod->mesh->config.view, va("%s/view.cfg", path));
}

/**
 * @brief Returns true if the mesh config has no effect: no translation or rotation, and a scale of
 * one, or of zero, which a config of only zeros holds.
 */
static bool R_MeshConfigIsEmpty(const RenderMeshConfig *config) {
  return Vec3_Equal(config->translate, Vec3_Zero()) &&
         Vec3_Equal(config->rotate, Vec3_Zero()) &&
         (config->scale == 1.f || config->scale == 0.f) &&
         Vec3_Equal(config->muzzle, Vec3_Zero());
}

/**
 * @brief Saves a mesh config to a file, deleting the file if the config is empty.
 * @return True if the file was written or deleted.
 */
static bool R_SaveMeshConfig(const RenderMeshConfig *cfg, const char *path) {

  if (R_MeshConfigIsEmpty(cfg)) {
    if (Fs_Exists(path)) {
      if (Fs_Delete(path)) {
        Com_Debug(DEBUG_RENDERER, "Deleted %s\n", path);
      } else {
        Com_Warn("Failed to delete %s\n", path);
        return false;
      }
    }
    return true;
  }

  File *file = Fs_OpenWrite(path);
  if (!file) {
    Com_Warn("Failed to write %s\n", path);
    return false;
  }

  char line[MAX_STRING_CHARS];
  size_t len;

  if (!Vec3_Equal(cfg->translate, Vec3_Zero())) {
    len = (size_t) q_snprintf(line, sizeof(line), "translate %g %g %g\n", cfg->translate.x, cfg->translate.y, cfg->translate.z);
    Fs_Write(file, line, 1, len);
  }

  if (!Vec3_Equal(cfg->rotate, Vec3_Zero())) {
    len = (size_t) q_snprintf(line, sizeof(line), "rotate %g %g %g\n", cfg->rotate.x, cfg->rotate.y, cfg->rotate.z);
    Fs_Write(file, line, 1, len);
  }

  if (cfg->scale != 1.f) {
    len = (size_t) q_snprintf(line, sizeof(line), "scale %g\n", cfg->scale);
    Fs_Write(file, line, 1, len);
  }

  if (!Vec3_Equal(cfg->muzzle, Vec3_Zero())) {
    len = (size_t) q_snprintf(line, sizeof(line), "muzzle %g %g %g\n", cfg->muzzle.x, cfg->muzzle.y, cfg->muzzle.z);
    Fs_Write(file, line, 1, len);
  }

  Fs_Close(file);
  Com_Debug(DEBUG_RENDERER, "Wrote %s\n", path);
  return true;
}

/**
 * @brief Media enumerator for R_SaveMeshConfigs_f: saves the world config of a dirty mesh model.
 * @remarks The link and view configs are authored by hand, and are never written.
 */
static void R_SaveMeshConfigs_enumerator(const RenderMedia *media, void *data) {

  if (media->type != R_MEDIA_MODEL) {
    return;
  }

  RenderModel *mod = (RenderModel *) media;
  if (!IS_MESH_MODEL(mod) || !mod->mesh->config.world.dirty) {
    return;
  }

  char path[MAX_QPATH];
  Dirname(mod->media.name, path);

  if (R_SaveMeshConfig(&mod->mesh->config.world, va("%s/world.cfg", path))) {
    mod->mesh->config.world.dirty = false;
    (*(int32_t *) data)++;
  }
}

/**
 * @brief Saves the world configs of all dirty mesh models.
 */
void R_SaveMeshConfigs_f(void) {

  int32_t count = 0;

  R_EnumerateMedia(R_SaveMeshConfigs_enumerator, &count);

  Com_Print("Saved %d mesh config(s)\n", count);
}

/**
 * @brief Calculates tangents for each mesh vertex.
 */
static void R_LoadMeshTangents(RenderModel *mod) {

  assert(mod->mesh);

  const RenderMeshFace *face = mod->mesh->faces;
  for (int32_t i = 0; i < mod->mesh->numFaces; i++, face++) {

    CmVertex *vertexes = Mem_Malloc(sizeof(CmVertex) * face->numVertexes);

    for (int32_t j = 0; j < mod->mesh->numFrames; j++) {

      RenderMeshVertex *v = face->vertexes + face->numVertexes * j;
      for (int32_t k = 0; k < face->numVertexes; k++, v++) {
        vertexes[k] = (CmVertex) {
          .position = &v->position,
          .normal = &v->normal,
          .tangent = &v->tangent,
          .bitangent = &v->bitangent,
          .st = &v->diffusemap
        };
      }

      Cm_Tangents(vertexes, 0, face->numVertexes, (int32_t *) face->elements, face->numElements);
    }

    Mem_Free(vertexes);
  }
}

/**
 * @brief Consolidates a mesh model's vertex and element data into GPU buffers.
 */
void R_LoadMeshVertexArray(RenderModel *mod) {

  assert(mod->mesh);

  RenderMeshModel *mesh = mod->mesh;

  if (!mesh->numFaces) {
    return;
  }

  {
    const RenderMeshFace *face = mesh->faces;
    for (int32_t i = 0; i < mesh->numFaces; i++, face++) {
      mesh->numVertexes += face->numVertexes;
      mesh->numElements += face->numElements;
    }
  }

  assert(mesh->numVertexes);
  assert(mesh->numElements);

  mesh->vertexes = Mem_LinkMalloc(mesh->numVertexes * mesh->numFrames * sizeof(RenderMeshVertex), mesh);
  mesh->elements = Mem_LinkMalloc(mesh->numElements * sizeof(uint32_t), mesh);

  RenderMeshVertex *vertex = mesh->vertexes;
  uint32_t *elements = mesh->elements;

  {
    RenderMeshFace *face = mesh->faces;
    for (int32_t i = 0; i < mesh->numFaces; i++, face++) {

      memcpy(vertex, face->vertexes, face->numVertexes * mesh->numFrames * sizeof(RenderMeshVertex));
      Mem_Free(face->vertexes);

      face->vertexes = vertex;
      vertex += face->numVertexes * mesh->numFrames;

      memcpy(elements, face->elements, face->numElements * sizeof(uint32_t));
      Mem_Free(face->elements);

      face->elements = elements;
      elements += face->numElements;
    }
  }

  R_LoadMeshTangents(mod);

  {
    RenderMeshFace *face = mesh->faces;
    for (int32_t i = 0; i < mesh->numFaces; i++, face++) {
      face->baseVertex = (int32_t) (face->vertexes - mesh->vertexes);
      face->indices = (void *) ((face->elements - mesh->elements) * sizeof(uint32_t));
    }
  }

  mesh->vertexBuffer = $(rContext.device, createBufferWithConstMem,
      SDL_GPU_BUFFERUSAGE_VERTEX,
      mesh->vertexes,
      mesh->numVertexes * mesh->numFrames * sizeof(RenderMeshVertex));

  mesh->elementsBuffer = $(rContext.device, createBufferWithConstMem,
      SDL_GPU_BUFFERUSAGE_INDEX,
      mesh->elements,
      mesh->numElements * sizeof(uint32_t));
}

/**
 * @brief Registers the mesh model's material dependencies with the media system.
 */
void R_RegisterMeshModel(RenderMedia *self) {
  RenderModel *mod = (RenderModel *) self;

  const RenderMeshFace *face = mod->mesh->faces;
  for (int32_t i = 0; i < mod->mesh->numFaces; i++, face++) {
    if (face->material) {
      R_RegisterDependency(self, (RenderMedia *) face->material);
    }
  }
}

/**
 * @brief Releases the mesh model's GPU buffers.
 */
void R_FreeMeshModel(RenderMedia *self) {
  RenderModel *mod = (RenderModel *) self;

  if (mod->mesh) {
    mod->mesh->vertexBuffer = release(mod->mesh->vertexBuffer);
    mod->mesh->elementsBuffer = release(mod->mesh->elementsBuffer);
  }
}
