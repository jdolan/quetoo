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

typedef struct {
  uint32_t v;
  uint32_t vt;
  uint32_t vn;
  uint32_t el;
} r_obj_face_vertex_t;

typedef struct {
  r_obj_face_vertex_t fv[4];
} r_obj_face_t;

typedef struct {
  char name[MAX_QPATH];
  Vector *f;
} r_obj_group_t;

typedef struct {
  Vector *v;
  Vector *vt;
  Vector *vn;
  Vector *g;
} r_obj_t;

/**
 * @brief Finds an existing vertex in the face or appends a new one, returning its index.
 */
static GLuint R_FindOrAppendObjVertex(r_mesh_face_t *face, const r_mesh_vertex_t *v) {

  for (int32_t i = 0; i < face->num_vertexes; i++) {
    if (!memcmp(v, face->vertexes + i, sizeof(*v))) {
      return i;
    }
  }

  face->num_vertexes++;
  face->vertexes = Mem_Realloc(face->vertexes, face->num_vertexes * sizeof(r_mesh_vertex_t));

  face->vertexes[face->num_vertexes - 1] = *v;
  return face->num_vertexes - 1;
}

/**
 * @brief Appends three element indices forming a triangle to the face's element list.
 */
static void R_AppendObjElements(r_mesh_face_t *face, GLuint a, GLuint b, GLuint c) {

  face->num_elements += 3;
  face->elements = Mem_Realloc(face->elements, face->num_elements * sizeof(GLuint));

  GLuint *elements = ((GLuint *) face->elements) + face->num_elements - 3;

  elements[0] = a;
  elements[1] = b;
  elements[2] = c;
}

/**
 * @brief Parses and loads an OBJ model from the given buffer into the render model.
 */
static void R_LoadObjModel(r_model_t *mod, void *buffer) {
  r_mesh_model_t *out;

  mod->mesh = out = Mem_LinkMalloc(sizeof(r_mesh_model_t), mod);
  out->num_frames = 1;

  r_obj_t obj = {
    .v = $(alloc(Vector), initWithSize, sizeof(vec3_t)),
    .vt = $(alloc(Vector), initWithSize, sizeof(vec2_t)),
    .vn = $(alloc(Vector), initWithSize, sizeof(vec3_t)),
    .g = $(alloc(Vector), initWithSize, sizeof(r_obj_group_t)),
  };

  r_obj_group_t group = {
    .name = "default",
    .f = $(alloc(Vector), initWithSize, sizeof(r_obj_face_t))
  };

  char *file = buffer;

  for (char *line = strtok(file, "\r\n"); line; line = strtok(NULL, "\r\n")) {

    vec3_t vec;
    if (q_strncmp("v ", line, q_strlen("v ")) == 0) {
      if (Parse_QuickPrimitive(line + q_strlen("v "), PARSER_NO_COMMENTS, PARSE_DEFAULT, PARSE_FLOAT, &vec, 3) == 3) {
        // swap ordering to match Quetoo
        vec = Vec3(vec.x, vec.z, vec.y);
        mod->bounds = Box3_Append(mod->bounds, vec);
        $(obj.v, addElement, &vec);
      }
    } else if (q_strncmp("vt ", line, q_strlen("vt ")) == 0) {
      if (Parse_QuickPrimitive(line + q_strlen("vt "), PARSER_NO_COMMENTS, PARSE_DEFAULT, PARSE_FLOAT, &vec, 2) == 2) {
        vec.y = -vec.y;
        $(obj.vt, addElement, &vec);
      }
    } else if (q_strncmp("vn ", line, q_strlen("vn ")) == 0) {
      if (Parse_QuickPrimitive(line + q_strlen("vn "), PARSER_NO_COMMENTS, PARSE_DEFAULT, PARSE_FLOAT, &vec, 3) == 3) {
        // swap ordering to match Quetoo
        vec = Vec3_Normalize(Vec3(vec.x, vec.z, vec.y));
        $(obj.vn, addElement, &vec);
      }
    } else if (q_strncmp("usemtl ", line, q_strlen("usemtl ")) == 0) {
      if (group.f->count) {
        $(obj.g, addElement, &group);
      } else {
        release(group.f);
      }
      q_strlcpy(group.name, line + q_strlen("usemtl "), sizeof(group.name));
      group.f = $(alloc(Vector), initWithSize, sizeof(r_obj_face_t));
    } else if (q_strncmp("g ", line, q_strlen("g ")) == 0) {
      if (group.f->count) {
        $(obj.g, addElement, &group);
      } else {
        release(group.f);
      }
      q_strlcpy(group.name, line + q_strlen("g "), sizeof(group.name));
      group.f = $(alloc(Vector), initWithSize, sizeof(r_obj_face_t));
    } else if (q_strncmp("f ", line, q_strlen("f ")) == 0) {

      r_obj_face_t face;
      memset(&face, 0, sizeof(face));

      int32_t i = 0;

      char *token = line + 1;
      while (*token) {

        if (i == lengthof(face.fv)) {
          Com_Error(ERROR_DROP, "%s uses complex faces, try triangles\n", mod->media.name);
        }

        r_obj_face_vertex_t *fv = &face.fv[i++];
        fv->v = (uint32_t) strtoul(token + 1, &token, 10);
        if (*token == '/') {
          fv->vt = (uint32_t) strtoul(token + 1, &token, 10);
          if (*token == '/') {
            fv->vn = (uint32_t) strtoul(token + 1, &token, 10);
          }
        } else if (fv->v == 0) {
          break;
        }
      }

      $(group.f, addElement, &face);
    }
  }

  if (group.f->count) {
    $(obj.g, addElement, &group);
  } else {
    release(group.f);
  }

  out->num_faces = (int32_t) obj.g->count;
  assert(out->num_faces <= MAX_MESH_FACES);

  out->faces = Mem_LinkMalloc(out->num_faces * sizeof(r_mesh_face_t), out);

  for (int32_t i = 0; i < out->num_faces; i++) {
    const r_obj_group_t *group = VectorElement(obj.g, r_obj_group_t, i);
    r_mesh_face_t *face = out->faces + i;

    q_strlcpy(face->name, group->name, sizeof(face->name));
    face->material = R_LoadMaterial(face->name, ASSET_CONTEXT_MODELS);
    R_RegisterDependency((r_media_t *) mod, (r_media_t *) face->material);

    for (size_t j = 0; j < group->f->count; j++) {
      r_obj_face_t *f = VectorElement(group->f, r_obj_face_t, j);

      for (size_t k = 0; k < lengthof(f->fv); k++) {
        r_obj_face_vertex_t *fv = f->fv + k;

        if (fv->v == 0) {
          break;
        }

        if (fv->v > obj.v->count) {
          Com_Error(ERROR_DROP, "%s has out-of-range vertex index %u\n", mod->media.name, fv->v);
        }

        if (fv->vt > obj.vt->count) {
          Com_Error(ERROR_DROP, "%s has out-of-range texcoord index %u\n", mod->media.name, fv->vt);
        }

        if (fv->vn == 0 || fv->vn > obj.vn->count) {
          Com_Error(ERROR_DROP, "%s is missing vertex normals\n", mod->media.name);
        }

        const r_mesh_vertex_t v = {
          .position = *VectorElement(obj.v, vec3_t, fv->v - 1),
          .diffusemap = fv->vt ? *VectorElement(obj.vt, vec2_t, fv->vt - 1) : Vec2_Zero(),
          .normal = *VectorElement(obj.vn, vec3_t, fv->vn - 1),
        };

        fv->el = R_FindOrAppendObjVertex(face, &v);
      }

      for (size_t k = 2; k < lengthof(f->fv); k++) {

        const r_obj_face_vertex_t *a = &f->fv[0];
        const r_obj_face_vertex_t *b = &f->fv[k - 1];
        const r_obj_face_vertex_t *c = &f->fv[k];

        if (c->v == 0) {
          break;
        }

        R_AppendObjElements(face, a->el, b->el, c->el);
      }
    }

    release(group->f);
  }

  // and configs
  R_LoadMeshConfigs(mod);

  // and finally the arrays
  R_LoadMeshVertexArray(mod);

  release(obj.v);
  release(obj.vt);
  release(obj.vn);
  release(obj.g);

  Com_Debug(DEBUG_RENDERER, "!================================\n");
  Com_Debug(DEBUG_RENDERER, "!R_LoadObjModel:   %s\n", mod->media.name);
  Com_Debug(DEBUG_RENDERER, "!  Vertexes:       %d\n", mod->mesh->num_vertexes);
  Com_Debug(DEBUG_RENDERER, "!  Elements:       %d\n", mod->mesh->num_elements);
  Com_Debug(DEBUG_RENDERER, "!  Frames:         %d\n", mod->mesh->num_frames);
  Com_Debug(DEBUG_RENDERER, "!  Tags:           %d\n", mod->mesh->num_tags);
  Com_Debug(DEBUG_RENDERER, "!  Faces:          %d\n", mod->mesh->num_faces);
  Com_Debug(DEBUG_RENDERER, "!  Animations:     %d\n", mod->mesh->num_animations);
  Com_Debug(DEBUG_RENDERER, "!================================\n");
}

/**
 * @brief The OBJ model format descriptor.
 */
const r_model_format_t r_obj_model_format = {
  .extension = "obj",
  .type = MODEL_MESH,
  .Load = R_LoadObjModel,
  .Register = R_RegisterMeshModel,
};
