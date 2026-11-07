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
static bool R_LoadMeshConfig(RenderMeshConfig *config, Parser *parser) {
  char token[MAX_STRING_CHARS];

  memset(config, 0, sizeof(*config));

  config->transform = Mat4_Identity();
  config->scale = 1.f;

  while (true) {

    if (!Parse_Token(parser, PARSE_DEFAULT, token, sizeof(token))) {
      return false;
    }
    if (!Str_Compare(token, "}")) {
      return true;
    }

    if (!Str_Compare(token, "translate")) {

      Vec3 v;
      if (Parse_Primitive(parser, PARSE_DEFAULT | PARSE_WITHIN_QUOTES | PARSE_NO_WRAP, PARSE_FLOAT, v.xyz, 3) != 3) {
        return false;
      }

      config->translate = v;
      config->transform = Mat4_ConcatTranslation(config->transform, v);
      continue;
    }

    if (!Str_Compare(token, "rotate")) {

      Vec3 v;
      if (Parse_Primitive(parser, PARSE_DEFAULT | PARSE_WITHIN_QUOTES | PARSE_NO_WRAP, PARSE_FLOAT, v.xyz, 3) != 3) {
        return false;
      }

      config->rotate = v;
      config->transform = Mat4_ConcatRotation3(config->transform, v);
      continue;
    }

    if (!Str_Compare(token, "scale")) {

      float v;
      if (!Parse_Primitive(parser, PARSE_DEFAULT | PARSE_WITHIN_QUOTES | PARSE_NO_WRAP, PARSE_FLOAT, &v, 1)) {
        return false;
      }

      config->scale = v;
      config->transform = Mat4_ConcatScale(config->transform, v);
      continue;
    }

    return false;
  }
}

/**
 * @brief Loads the mesh configs for a model.
 */
void R_LoadMeshConfigs(RenderModel *mod) {
  char path[MAX_QPATH];

  Dirname(mod->media.name, path);

  RenderMeshConfig *configs[] = {
    &mod->mesh->config.world, &mod->mesh->config.link, &mod->mesh->config.view
  };
  const char *names[] = { "world", "link", "view", "muzzle" };
  for (size_t i = 0; i < lengthof(configs); i++) {
    memset(configs[i], 0, sizeof(*configs[i]));
    configs[i]->transform = Mat4_Identity();
    configs[i]->scale = 1.f;
  }
  void *buf;
  const char *file = va("%s/model.cfg", path);
  if (Fs_Load(file, &buf) == -1) {
    return;
  }
  Parser parser = Parse_Init(buf, PARSER_ALL_COMMENTS);
  char token[MAX_STRING_CHARS];
  bool seen[4] = { false };
  bool valid = true;
  Vec3 muzzle = Vec3_Zero();
  while (Parse_Token(&parser, PARSE_DEFAULT, token, sizeof(token))) {
    if (Str_Compare(token, "{") ||
        !Parse_Token(&parser, PARSE_DEFAULT, token, sizeof(token)) || Str_Compare(token, "type") ||
        !Parse_Token(&parser, PARSE_DEFAULT, token, sizeof(token))) {
      valid = false;
      break;
    }
    size_t i;
    for (i = 0; i < lengthof(names); i++) {
      if (!Str_Compare(token, names[i])) {
        break;
      }
    }
    if (i == lengthof(names) || seen[i]) {
      valid = false;
      break;
    }
    seen[i] = true;
    if (i < lengthof(configs)) {
      valid = R_LoadMeshConfig(configs[i], &parser);
    } else {
      valid = Parse_Token(&parser, PARSE_DEFAULT, token, sizeof(token)) && !Str_Compare(token, "origin") &&
              Parse_Primitive(&parser, PARSE_DEFAULT | PARSE_WITHIN_QUOTES | PARSE_NO_WRAP,
                              PARSE_FLOAT, muzzle.xyz, 3) == 3 &&
              Parse_Token(&parser, PARSE_DEFAULT, token, sizeof(token)) && !Str_Compare(token, "}");
    }
    if (!valid) {
      break;
    }
  }
  if (!valid) {
    Com_Error(ERROR_DROP, "Invalid model config %s at line %u\n", file, parser.position.row);
  }
  for (size_t i = 0; i < lengthof(configs); i++) {
    configs[i]->muzzle = muzzle;
  }
  Fs_Free(buf);
}

/**
 * @brief Updates the world block without rewriting the other model config blocks.
 */
static bool R_SaveMeshConfig(const RenderMeshConfig *cfg, const char *path) {
  void *buf = NULL;
  Fs_Load(path, &buf);
  const char *start = buf;
  const char *worldStart = NULL, *worldEnd = NULL;
  if (buf) {
    Parser parser = Parse_Init(buf, PARSER_ALL_COMMENTS);
    char token[MAX_STRING_CHARS];
    while (true) {
      const char *block = parser.position.ptr;
      if (!Parse_Token(&parser, PARSE_DEFAULT, token, sizeof(token))) {
        break;
      }
      if (Str_Compare(token, "{") ||
          !Parse_Token(&parser, PARSE_DEFAULT, token, sizeof(token)) || Str_Compare(token, "type") ||
          !Parse_Token(&parser, PARSE_DEFAULT, token, sizeof(token))) {
        Com_Warn("Cannot update invalid model config %s\n", path);
        Fs_Free(buf);
        return false;
      }
      const bool world = !Str_Compare(token, "world");
      bool closed = false;
      while (Parse_Token(&parser, PARSE_DEFAULT, token, sizeof(token))) {
        if (!Str_Compare(token, "}")) {
          closed = true;
          break;
        }
      }
      if (!closed || (world && worldStart)) {
        Com_Warn("Cannot update invalid model config %s\n", path);
        Fs_Free(buf);
        return false;
      }
      if (world) {
        worldStart = block;
        worldEnd = parser.position.ptr;
      }
    }
  }

  File *file = Fs_OpenWrite(path);
  if (!file) {
    Com_Warn("Failed to write %s\n", path);
    if (buf) {
      Fs_Free(buf);
    }
    return false;
  }

  char line[MAX_STRING_CHARS];
  size_t len;
  if (buf) {
    Fs_Write(file, start, 1, worldStart ? (size_t) (worldStart - start) : strlen(start));
  }
  const char *header = "\n{\n\"type\" \"world\"\n";
  Fs_Write(file, header, 1, strlen(header));

  if (!Vec3_Equal(cfg->translate, Vec3_Zero())) {
    len = (size_t) Str_Format(line, sizeof(line), "\"translate\" \"%g %g %g\"\n", cfg->translate.x, cfg->translate.y, cfg->translate.z);
    Fs_Write(file, line, 1, len);
  }

  if (!Vec3_Equal(cfg->rotate, Vec3_Zero())) {
    len = (size_t) Str_Format(line, sizeof(line), "\"rotate\" \"%g %g %g\"\n", cfg->rotate.x, cfg->rotate.y, cfg->rotate.z);
    Fs_Write(file, line, 1, len);
  }

  if (cfg->scale != 1.f) {
    len = (size_t) Str_Format(line, sizeof(line), "\"scale\" \"%g\"\n", cfg->scale);
    Fs_Write(file, line, 1, len);
  }

  Fs_Write(file, "}\n", 1, 2);
  if (worldEnd) {
    Fs_Write(file, worldEnd, 1, strlen(worldEnd));
  }
  if (buf) {
    Fs_Free(buf);
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

  if (R_SaveMeshConfig(&mod->mesh->config.world, va("%s/model.cfg", path))) {
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

    WindingVertex *vertexes = Mem_Malloc(sizeof(WindingVertex) * face->numVertexes);

    for (int32_t j = 0; j < mod->mesh->numFrames; j++) {

      RenderMeshVertex *v = face->vertexes + face->numVertexes * j;
      for (int32_t k = 0; k < face->numVertexes; k++, v++) {
        vertexes[k] = (WindingVertex) {
          .position = &v->position,
          .normal = &v->normal,
          .tangent = &v->tangent,
          .bitangent = &v->bitangent,
          .st = &v->diffusemap
        };
      }

      Winding_Tangents(vertexes, 0, face->numVertexes, (int32_t *) face->elements, face->numElements);
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

  mesh->vertexBuffer = $(renderContext.device, createBufferWithConstMem,
      SDL_GPU_BUFFERUSAGE_VERTEX,
      mesh->vertexes,
      mesh->numVertexes * mesh->numFrames * sizeof(RenderMeshVertex));

  mesh->elementsBuffer = $(renderContext.device, createBufferWithConstMem,
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
