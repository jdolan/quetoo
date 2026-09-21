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
 * @brief Applies the mesh configuration transform to the entity's matrix.
 */
void R_ApplyMeshConfig(RenderEntity *e) {

  assert(IS_MESH_MODEL(e->model));

  const RenderMeshConfig *c;
  
  if (e->parent) {
    c = &e->model->mesh->config.link;
  } else if (e->effects & EF_WEAPON) {
    c = &e->model->mesh->config.view;
  } else {
    c = &e->model->mesh->config.world;
  }

  e->matrix = Mat4_Concat(e->matrix, c->transform);
}

/**
 * @brief Returns the named mesh tag for the specified frame.
 */
static const RenderMeshTag *R_MeshTag(const RenderModel *mod, const char *name, const int32_t frame) {

  // a negative frame is not merely out of range: it indexes behind the tags, so the scan below
  // finds no match and blames the tag, on a model that has it
  if (frame < 0 || frame >= mod->mesh->numFrames) {
    Com_Warn("%s: Invalid frame: %d of %d\n", mod->media.name, frame, mod->mesh->numFrames);
    return NULL;
  }

  const RenderMeshModel *model = mod->mesh;
  const RenderMeshTag *tag = &model->tags[frame * model->numTags];

  for (int32_t i = 0; i < model->numTags; i++, tag++) {
    if (!q_strcmp(name, tag->name)) {
      return tag;
    }
  }

  Com_Warn("%s: Tag not found: %s (frame %d of %d, %d tags)\n", mod->media.name, name, frame,
           model->numFrames, model->numTags);
  return NULL;
}

/**
 * @brief Applies a parent mesh tag transform to a linked entity.
 */
void R_ApplyMeshTag(RenderEntity *e) {

  const RenderMeshTag *t1 = R_MeshTag(e->parent->model, e->tag, e->parent->oldFrame);
  const RenderMeshTag *t2 = R_MeshTag(e->parent->model, e->tag, e->parent->frame);

  if (!t1 || !t2) {
    Com_Warn("%s: Invalid tag %s: frames %d, %d\n", e->parent->model->media.name, e->tag,
             e->parent->oldFrame, e->parent->frame);
    return;
  }

  Mat4 tagTransform = Mat4_Mix(t2->matrix, t1->matrix, e->parent->backLerp);
  tagTransform = Mat4_Concat(tagTransform, e->matrix);
  e->matrix = Mat4_Concat(e->parent->matrix, tagTransform);
  Vec3 forward;
  Mat4_Vectors(e->matrix, &forward, NULL, NULL, &e->origin);

  e->angles = Vec3_Euler(forward);
  e->scale = Mat4_ToScale(e->matrix);
  e->absBounds = Mat4_TransformBounds(e->matrix, e->bounds);
}
