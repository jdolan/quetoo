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
 * @brief Loads `animation.cfg` for an MD3 model.
 */
static void R_LoadMd3Animations(RenderModel *mod) {
  char path[MAX_QPATH];
  void *buf;
  int32_t skip = 0;
  char token[MAX_TOKEN_CHARS];

  Dirname(mod->media.name, path);
  strcat(path, "animation.cfg");

  if (Fs_Load(path, &buf) == -1) {
    Com_Warn("No animation.cfg for %s\n", mod->media.name);
    return;
  }

  mod->mesh->animations = Mem_LinkMalloc(sizeof(RenderMeshAnimation) * MD3_MAX_ANIMATIONS, mod->mesh);

  q_strlcpy(mod->mesh->sounds, "male", sizeof(mod->mesh->sounds));

  Parser parser = Parse_Init((const char *) buf, PARSER_DEFAULT);

  while (true) {

    if (!Parse_PeekToken(&parser, PARSE_DEFAULT, token, sizeof(token))) {
      break;
    }

    if (!q_strcmp(token, "footsteps")) {
      Parse_SkipToken(&parser, PARSE_DEFAULT);
      Parse_SkipToken(&parser, PARSE_DEFAULT | PARSE_NO_WRAP);
      continue;
    }

    if (!q_strcmp(token, "headoffset")) {
      Parse_SkipToken(&parser, PARSE_DEFAULT);
      Parse_SkipPrimitive(&parser, PARSE_DEFAULT | PARSE_NO_WRAP, PARSE_FLOAT, 3);
      continue;
    }

    // legacy Quake III directive; maps to a sound set name for backwards compatibility
    if (!q_strcmp(token, "sex")) {
      Parse_SkipToken(&parser, PARSE_DEFAULT);
      Parse_SkipToken(&parser, PARSE_DEFAULT | PARSE_NO_WRAP);
      continue;
    }

    // names the directory under players/common to fall back to for samples the
    // model does not provide its own version of, e.g. "female", "cyborg", "demon"
    if (!q_strcmp(token, "sounds")) {
      Parse_SkipToken(&parser, PARSE_DEFAULT);
      if (!Parse_Token(&parser, PARSE_DEFAULT | PARSE_NO_WRAP, mod->mesh->sounds, sizeof(mod->mesh->sounds))) {
        break;
      }
      continue;
    }

    if (!q_strcmp(token, "fixedlegs")) {
      Parse_SkipToken(&parser, PARSE_DEFAULT);
      mod->mesh->flags |= MESH_MODEL_FIXED_LEGS;
      continue;
    }

    if (!q_strcmp(token, "fixedtorso")) {
      Parse_SkipToken(&parser, PARSE_DEFAULT);
      mod->mesh->flags |= MESH_MODEL_FIXED_TORSO;
      continue;
    }

    if (*token >= '0' && *token <= '9') {
      RenderMeshAnimation *a = &mod->mesh->animations[mod->mesh->numAnimations];

      if (!Parse_Primitive(&parser, PARSE_DEFAULT, PARSE_INT32, &a->firstFrame, 1)) {
        break;
      }

      if (!Parse_Primitive(&parser, PARSE_DEFAULT | PARSE_NO_WRAP, PARSE_INT32, &a->numFrames, 1)) {
        break;
      }

      if (!Parse_Primitive(&parser, PARSE_DEFAULT | PARSE_NO_WRAP, PARSE_INT32, &a->loopedFrames, 1)) {
        break;
      }

      if (!Parse_Primitive(&parser, PARSE_DEFAULT | PARSE_NO_WRAP, PARSE_INT32, &a->hz, 1)) {
        break;
      }

      if (mod->mesh->numAnimations == ANIM_LEGS_WALKCR) {
        skip = a->firstFrame - mod->mesh->animations[ANIM_TORSO_GESTURE].firstFrame;
      }

      if (mod->mesh->numAnimations >= ANIM_LEGS_WALKCR) {
        a->firstFrame -= skip;
      }

      if (!a->numFrames) {
        Com_Warn("%s: No frames for %d\n", mod->media.name, mod->mesh->numAnimations);
      }

      if (!a->hz) {
        Com_Warn("%s: No hz for %d\n", mod->media.name, mod->mesh->numAnimations);
      }

      Com_Debug(DEBUG_RENDERER, "Parsed %d: %d %d %d %d\n", mod->mesh->numAnimations,
                a->firstFrame, a->numFrames, a->loopedFrames, a->hz);

      mod->mesh->numAnimations++;

      if (mod->mesh->numAnimations == MD3_MAX_ANIMATIONS) {
        Com_Warn("MD3_MAX_ANIMATIONS reached: %s\n", mod->media.name);
        break;
      }

      continue;
    }

    Parse_SkipToken(&parser, PARSE_DEFAULT);

    while (true) {
      if (!Parse_SkipToken(&parser, PARSE_DEFAULT | PARSE_NO_WRAP)) {
        break;
      }
    }
  }

  Fs_Free(buf);

  Com_Debug(DEBUG_RENDERER, "Loaded %d animations: %s\n", mod->mesh->numAnimations, mod->media.name);
}

/**
 * @brief Swaps MD3 texcoords to host endianness.
 */
static Md3Texcoord R_SwapMd3Texcoord(const Md3Texcoord *in) {

  Md3Texcoord out = *in;

#if SDL_BYTEORDER != SDL_LIL_ENDIAN
  out.st = LittleVec2(out.st);
#endif

  return out;
}

/**
 * @brief Swaps an MD3 vertex to host endianness.
 */
static Md3Vertex R_SwapMd3Vertex(const Md3Vertex *in) {

  Md3Vertex out = *in;

#if SDL_BYTEORDER != SDL_LIL_ENDIAN
  out.point = LittleVec3s(out.point);
  out.norm = LittleShort(out.norm);
#endif

  return out;
}

/**
 * @brief Swaps an MD3 triangle to host endianness.
 */
static Md3Triangle R_SwapMd3Triangle(const Md3Triangle *in) {

  Md3Triangle out = *in;

#if SDL_BYTEORDER != SDL_LIL_ENDIAN
  for (int32_t i = 0; i < 3; i++) {
    out.indexes[i] = LittleLong(out.indexes[i]);
  }
#endif

  return out;
}

/**
 * @brief Swaps an MD3 frame to host endianness.
 */
static Md3Frame R_SwapMd3Frame(const Md3Frame *in) {

  Md3Frame out = *in;

#if SDL_BYTEORDER != SDL_LIL_ENDIAN
  out.bounds = LittleBounds(out.bounds);
  out.translate = LittleVec3(out.translate);
  out.radius = LittleFloat(out.radius);
#endif

  return out;
}

/**
 * @brief Swaps an MD3 tag to host endianness.
 */
static Md3Tag R_SwapMd3Tag(const Md3Tag *in) {

  Md3Tag out = *in;

#if SDL_BYTEORDER != SDL_LIL_ENDIAN
  out.origin = LittleVec3(out.origin);
  out.axis[0] = LittleVec3(out.axis[0]);
  out.axis[1] = LittleVec3(out.axis[1]);
  out.axis[2] = LittleVec3(out.axis[2]);
#endif

  return out;
}

/**
 * @brief Swaps an MD3 surface to host endianness.
 */
static Md3Surface R_SwapMd3Surface(const Md3Surface *in) {

  Md3Surface out = *in;

#if SDL_BYTEORDER != SDL_LIL_ENDIAN
  out.id = LittleLong(out.id);
  out.flags = LittleLong(out.flags);

  out.numFrames = LittleLong(out.numFrames);
  out.numShaders = LittleLong(out.numShaders);
  out.numVertexes = LittleLong(out.numVertexes);
  out.numTriangles = LittleLong(out.numTriangles);

  out.ofsTriangles = LittleLong(out.ofsTriangles);
  out.ofsShaders = LittleLong(out.ofsShaders);
  out.ofsTexcoords = LittleLong(out.ofsTexcoords);
  out.ofsVertexes = LittleLong(out.ofsVertexes);
  out.ofsEnd = LittleLong(out.ofsEnd);
#endif

  return out;
}

/**
 * @brief Swaps an MD3 header to host endianness.
 */
static Md3 R_SwapMd3(const Md3 *in) {

  Md3 out = *in;

#if SDL_BYTEORDER != SDL_LIL_ENDIAN
  out.id = LittleLong(out.id);
  out.version = LittleLong(out.version);
  out.flags = LittleLong(out.flags);

  out.numFrames = LittleLong(out.numFrames);
  out.numTags = LittleLong(out.numTags);
  out.num_meshes = LittleLong(out.num_meshes);
  out.numShaders = LittleLong(out.numShaders);

  out.ofsFrames = LittleLong(out.ofsFrames);
  out.ofsTags = LittleLong(out.ofsTags);
  out.ofs_meshes = LittleLong(out.ofs_meshes);
  out.ofsEnd = LittleLong(out.ofsEnd);
#endif

  return out;
}

/**
 * @brief Loads an MD3 model into the mesh renderer format.
 */
static void R_LoadMd3Model(RenderModel *mod, void *buffer) {

  const byte *base = buffer;

  const Md3 md3 = R_SwapMd3((Md3 *) base);

  if (md3.id != MD3_IDENT) {
    Com_Error(ERROR_DROP, "%s MD3_IDENT is %d\n", mod->media.name, md3.id);
  }

  if (md3.version != MD3_VERSION) {
    Com_Error(ERROR_DROP, "%s MD3_VERSION is %d\n", mod->media.name, md3.version);
  }

  if (md3.numFrames < MD3_MIN_FRAMES) {
    Com_Error(ERROR_DROP, "%s MD3_MIN_FRAMES %d\n", mod->media.name, md3.numFrames);
  }

  if (md3.numFrames > MD3_MAX_FRAMES) {
    Com_Error(ERROR_DROP, "%s MD3_MAX_FRAMES %d\n", mod->media.name, md3.numFrames);
  }

  if (md3.numTags > MD3_MAX_TAGS) {
    Com_Error(ERROR_DROP, "%s MD3_MAX_TAGS %d\n", mod->media.name, md3.numTags);
  }

  if (md3.numSurfaces > MD3_MAX_SURFACES) {
    Com_Error(ERROR_DROP, "%s MD3_MAX_SURFACES %d\n", mod->media.name, md3.numSurfaces);
  }

  if (q_strncmp(mod->media.name, "players/", 8)) {
    Com_Warn("%s: MD3 is only supported for player models; use OBJ instead\n", mod->media.name);
    return;
  }

  mod->mesh = Mem_LinkMalloc(sizeof(RenderMeshModel), mod);

  {
    mod->mesh->numFrames = md3.numFrames;
    mod->mesh->frames = Mem_LinkMalloc(mod->mesh->numFrames * sizeof(RenderMeshFrame), mod->mesh);

    const Md3Frame *in = (Md3Frame *) (base + md3.ofsFrames);
    RenderMeshFrame *out = mod->mesh->frames;

    for (int32_t i = 0; i < mod->mesh->numFrames; i++, in++, out++) {

      const Md3Frame frame = R_SwapMd3Frame(in);

      out->bounds = frame.bounds;
      
      out->translate = frame.translate;
    }
  }

  {
    mod->mesh->numTags = md3.numTags;
    mod->mesh->tags = Mem_LinkMalloc(mod->mesh->numTags * mod->mesh->numFrames * sizeof(RenderMeshTag), mod->mesh);

    const Md3Tag *in = (Md3Tag *) (base + md3.ofsTags);
    RenderMeshTag *out = mod->mesh->tags;

    for (int32_t i = 0; i < mod->mesh->numFrames; i++) {
      for (int32_t j = 0; j < mod->mesh->numTags; j++, in++, out++) {

        const Md3Tag tag = R_SwapMd3Tag(in);

        q_strlcpy(out->name, tag.name, MD3_MAX_PATH);
        out->matrix = Mat4_FromVectors(tag.axis[0], tag.axis[1], tag.axis[2], tag.origin);
      }
    }
  }

  {
    mod->mesh->numFaces = md3.numSurfaces;
    mod->mesh->faces = Mem_LinkMalloc(mod->mesh->numFaces * sizeof(RenderMeshFace), mod->mesh);

    const Md3Surface *in = (Md3Surface *) (base + md3.ofsSurfaces);
    RenderMeshFace *out = mod->mesh->faces;

    for (int32_t i = 0; i < mod->mesh->numFaces; i++, out++) {

      const Md3Surface surface = R_SwapMd3Surface(in);

      if (surface.id != MD3_IDENT) {
        Com_Error(ERROR_DROP, "%s: %s: MD3_IDENT %d\n", mod->media.name, surface.name, surface.id);
      }

      if (surface.numShaders > MD3_MAX_SHADERS) {
        Com_Error(ERROR_DROP, "%s: %s: MD3_MAX_SHADERS %d\n", mod->media.name, surface.name, surface.numShaders);
      }

      if (in->numTriangles > MD3_MAX_TRIANGLES) {
        Com_Error(ERROR_DROP, "%s: %s: MD3_MAX_TRIANGLES %d\n", mod->media.name, surface.name, surface.numTriangles);
      }

      if (in->numVertexes > MD3_MAX_VERTEXES) {
        Com_Error(ERROR_DROP, "%s: %s: MD3_MAX_VERTEXES %d\n", mod->media.name, surface.name, surface.numVertexes);
      }

      q_strlcpy(out->name, surface.name, MD3_MAX_PATH);

      const byte *surfaceBase = (byte *) in;

      if (*out->name) {
        out->material = R_LoadMaterial(out->name, ASSET_CONTEXT_PLAYERS);
      } else {
        Com_Warn("%s: surface %d has no name, it will not be drawn\n", mod->media.name, i);
      }

      {
        out->numVertexes = surface.numVertexes;
        out->vertexes = Mem_LinkMalloc(out->numVertexes * mod->mesh->numFrames * sizeof(RenderMeshVertex), mod->mesh);

        const Md3Vertex *inVertex = (Md3Vertex *) (surfaceBase + surface.ofsVertexes);
        RenderMeshVertex *outVertex = out->vertexes;

        for (int32_t j = 0; j < mod->mesh->numFrames; j++) {

          const Md3Texcoord *inTexcoord = (Md3Texcoord *) (surfaceBase + surface.ofsTexcoords);

          for (int32_t k = 0; k < out->numVertexes; k++, inVertex++, inTexcoord++, outVertex++) {

            const Md3Vertex vertex = R_SwapMd3Vertex(inVertex);

            outVertex->position = Vec3_Scale(Vec3s_CastVec3(vertex.point), MD3_XYZ_SCALE);

            mod->bounds = Box3_Append(mod->bounds, outVertex->position);

            float lat = (vertex.norm >> 8) & 0xff;
            float lon = (vertex.norm & 0xff);

            lat *= M_PI / 128.0;
            lon *= M_PI / 128.0;

            outVertex->normal.x = cos(lat) * sin(lon);
            outVertex->normal.y = sin(lat) * sin(lon);
            outVertex->normal.z = cos(lon);

            outVertex->normal = Vec3_Normalize(outVertex->normal);

            const Md3Texcoord texcoord = R_SwapMd3Texcoord(inTexcoord);

            outVertex->diffusemap = texcoord.st;
          }
        }
      }

      {
        out->numElements = surface.numTriangles * 3;
        out->elements = Mem_LinkMalloc(out->numElements * sizeof(uint32_t), mod->mesh);

        const Md3Triangle *inTriangle = (Md3Triangle *) (surfaceBase + surface.ofsTriangles);
        uint32_t *outTriangle = out->elements;

        for (int32_t j = 0; j < surface.numTriangles; j++, inTriangle++, outTriangle += 3) {

          const Md3Triangle tri = R_SwapMd3Triangle(inTriangle);

          outTriangle[0] = tri.indexes[0];
          outTriangle[1] = tri.indexes[1];
          outTriangle[2] = tri.indexes[2];
        }
      }

      in = (Md3Surface *) (surfaceBase + in->ofsEnd);
    }
  }

  if (q_strstr(mod->media.name, "/upper")) {
    R_LoadMd3Animations(mod);
  }

  R_LoadMeshConfigs(mod);

  R_LoadMeshVertexArray(mod);

  Com_Debug(DEBUG_RENDERER, "!================================\n");
  Com_Debug(DEBUG_RENDERER, "!R_LoadMd3Model:   %s\n", mod->media.name);
  Com_Debug(DEBUG_RENDERER, "!  Vertexes:       %d\n", mod->mesh->numVertexes);
  Com_Debug(DEBUG_RENDERER, "!  Elements:       %d\n", mod->mesh->numElements);
  Com_Debug(DEBUG_RENDERER, "!  Frames:         %d\n", mod->mesh->numFrames);
  Com_Debug(DEBUG_RENDERER, "!  Tags:           %d\n", mod->mesh->numTags);
  Com_Debug(DEBUG_RENDERER, "!  Faces:          %d\n", mod->mesh->numFaces);
  Com_Debug(DEBUG_RENDERER, "!  Animations:     %d\n", mod->mesh->numAnimations);
  Com_Debug(DEBUG_RENDERER, "!================================\n");
}

/**
 * @brief The MD3 model format descriptor.
 */
const RenderModelFormat rMd3ModelFormat = {
  .extension = "md3",
  .type = MODEL_MESH,
  .Load = R_LoadMd3Model,
  .Register = R_RegisterMeshModel,
  .Free = R_FreeMeshModel,
};
