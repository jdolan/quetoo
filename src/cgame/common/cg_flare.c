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

#include "cg_local.h"

/**
 * @brief The flare type.
 */
typedef struct {

  /**
   * @brief The face this flare is anchored to.
   */
  const RenderBspFace *face;

  /**
   * @brief The material stage defining this flare.
   */
  const RenderStage *stage;

  /**
   * @brief The bounds of all faces represented by this flare.
   */
  Box3 bounds;

  /**
   * @brief The sprite input and output instances.
   */
  RenderSprite in, out;

  /**
   * @brief The entity referencing the model containin this flare, if any.
   */
  const ClientEntity *entity;
} ClientGameFlare;

static Vector *cg_flares;

#define FLARE_ALPHA_RAMP 0.01

/**
 * @brief Adds all loaded flare sprites to the view, attenuated by their surface angle to the camera.
 */
void Cg_AddFlares(void) {

  if (!cg_add_flares->value) {
    return;
  }

  for (size_t i = 0; i < cg_flares->count; i++) {
    ClientGameFlare *flare = VectorValue(cg_flares, ClientGameFlare *, i);

    Mat4 matrix = Mat4_Identity();
    flare->entity = NULL;

    const RenderBspInlineModel *in = flare->face->node->model;

    if (in != cgi.WorldModel()->bsp->inlineModels && !editor->value) {

      const ClientEntity *e = cgi.client->entities;
      for (int32_t j = 0; j < MAX_ENTITIES; j++, e++) {

        if (!e->current.model1) {
          continue;
        }

        if (e->current.model1 == MODEL_CLIENT) {
          continue;
        }

        const RenderModel *mod = cgi.client->models[e->current.model1];

        if (mod && mod->type == MODEL_BSP_INLINE) {
          if (in == mod->bspInline) {
            matrix = Mat4_FromRotationTranslationScale(e->angles, e->origin, 1.f);
            flare->entity = e;
            break;
          }
        }
      }

      assert(flare->entity);
    }

    CmBspPlane plane = *(flare->face->plane->cm);

    if (flare->entity) {
      flare->out.origin = Mat4_Transform(matrix, flare->in.origin);

      const Vec4 out = Mat4_TransformPlane(matrix, plane.normal, plane.dist);

      plane.normal = out.xyz;
      plane.dist = out.w;
    }

    // Dot product gives us facing: positive=front, negative=back
    const float dot = Vec3_Dot(Vec3_Direction(cgi.view->origin, flare->out.origin), plane.normal);
    // Use absolute value to allow sprites from behind, but abs(dot) reduces visibility for grazing angles
    const float alpha = Clampf01(Maxf(fabsf(dot), 0.25f) * cg_add_flares->value);

    if (alpha == 0.f) {
      continue;
    }

    flare->out.color = Vec3_Scale(flare->in.color, alpha);

    cgi.AddSprite(cgi.view, &flare->out);
  }
}

/**
 * @brief Creates a flare from the specified face and stage.
 */
ClientGameFlare *Cg_LoadFlare(const RenderBspFace *face, const RenderStage *stage) {

  ClientGameFlare *flare = cgi.Malloc(sizeof(*flare), MEM_TAG_CGAME_LEVEL);

  flare->face = face;
  flare->stage = stage;

  flare->bounds = Box3_Null();
  for (int32_t i = 0; i < face->numVertexes; i++) {
    flare->bounds = Box3_Append(flare->bounds, face->vertexes[i].position);
  }

  flare->bounds = Box3_Expand(flare->bounds, Box3_Distance(flare->bounds) * .1f);

  if (stage->cm->flags & STAGE_COLOR) {
    flare->in.color = stage->cm->color.vec3;
  } else {
    flare->in.color = color_white.vec3;
  }

  flare->in.media = stage->media;
  flare->in.lighting = 1.f;
  // Use frustum-aligned billboard without the two perpendicular axial quads

  return flare;
}

/**
 * @brief Returns true if two faces share a vertex position, indicating geometric adjacency.
 */
static _Bool Cg_FacesShareVertex(const RenderBspFace *a, const RenderBspFace *b) {

  const float epsilon = 1.f;

  for (int32_t i = 0; i < a->numVertexes; i++) {
    for (int32_t j = 0; j < b->numVertexes; j++) {
      if (Vec3_Distance(a->vertexes[i].position, b->vertexes[j].position) < epsilon) {
        return true;
      }
    }
  }

  return false;
}

/**
 * @brief Merges adjacent flares on the same brush side into unified flares by union of bounds.
 */
static void Cg_MergeFlares(void) {

  for (size_t i = 0; i < cg_flares->count; i++) {
    ClientGameFlare *a = VectorValue(cg_flares, ClientGameFlare *, i);

    for (size_t j = i + 1; j < cg_flares->count; j++) {
      ClientGameFlare *b = VectorValue(cg_flares, ClientGameFlare *, j);

      if (a->face->brushSide == b->face->brushSide &&
          Cg_FacesShareVertex(a->face, b->face)) {
        a->bounds = Box3_Union(a->bounds, b->bounds);

        $(cg_flares, removeAt, j);
        cgi.Free(b);

        j--;
      }
    }

    a->in.origin = Box3_Center(a->bounds);
    a->in.size = Box3_Distance(a->bounds);

    if (a->stage->cm->flags & (STAGE_SCALE_S | STAGE_SCALE_T)) {
      a->in.size *= (a->stage->cm->scale.s ? a->stage->cm->scale.s : a->stage->cm->scale.t);
    }

    a->out = a->in;
  }
}

/**
 * @brief Loads all flare stages from BSP faces and merges adjacent flares on the same brush side.
 */
void Cg_LoadFlares(void) {

  cg_flares = $(alloc(Vector), initWithSize, sizeof(ClientGameFlare *));

  const RenderBspModel *bsp = cgi.WorldModel()->bsp;

  const RenderBspFace *face = bsp->faces;
  for (int32_t i = 0; i < bsp->numFaces; i++, face++) {

    if (!face->brushSide) {
      continue;
    }

    const RenderMaterial *material = face->brushSide->material;
    if (material->cm->stageFlags & STAGE_FLARE) {

      const RenderStage *stage = material->stages;
      while (stage) {
        if (stage->cm->flags & STAGE_FLARE) {
          break;
        }
        stage = stage->next;
      }
      assert(stage);

      if (stage->media == NULL) {
        continue;
      }

      ClientGameFlare *flare = Cg_LoadFlare(face, stage);
      $(cg_flares, add, &flare);
    }
  }

  Cg_MergeFlares();

  Cg_Debug("Loaded %zu flares\n", cg_flares->count);
}

/**
 * @brief Frees the flare array and sets the pointer to `NULL`.
 */
void Cg_FreeFlares(void) {

  if (cg_flares) {
    release(cg_flares);
    cg_flares = NULL;
  }
}
