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

#pragma once

#include "r_types.h"

void R_AddDecal(r_view_t *view, const r_decal_t *decal);

#if defined(__R_LOCAL_H__)

#define DECAL_TEXTURE_SIZE 256
#define DECAL_TEXTURE_LAYERS 64

/**
 * @brief The capacity of the decal instance ring shared by all blocks.
 * @remarks Instances are immutable once written and the ring is only appended
 * to, so each frame's new instances upload without cycling. That is only safe
 * while the ring cannot wrap onto instances a frame still in flight may read,
 * hence a capacity far beyond what any one frame can allocate.
 */
#define MAX_DECAL_INSTANCES 0x8000

/**
 * @brief Vec4-aligned instance of a decal: the decal as clipped to a single
 * face, holding everything it contributes to that face's triangles which does
 * not vary per vertex.
 * @remarks Written once when the decal is clipped and never modified. Must
 * match `decal_instance_t` in decal_vs.glsl.
 */
typedef struct {

  /**
   * @brief The origin projected onto the face, and the projected radius in `w`.
   */
  alignas(16) vec4_t origin;

  /**
   * @brief The face normal.
   */
  vec4_t normal;

  /**
   * @brief The rotated face tangent.
   */
  vec4_t tangent;

  /**
   * @brief The rotated face bitangent.
   */
  vec4_t bitangent;

  /**
   * @brief The atlas rect of the decal image: `xy` min, `zw` max.
   */
  vec4_t texcoords;

  /**
   * @brief The decal color.
   */
  vec4_t color;

  /**
   * @brief Decal creation time.
   */
  uint32_t time;

  /**
   * @brief Decal lifetime.
   */
  uint32_t lifetime;

  /**
   * @brief The ring generation this instance was written in, so that a triangle
   * that outlived its instance is discarded rather than adopting whichever
   * decal reused the slot.
   */
  uint32_t generation;
} r_decal_instance_t;

static_assert(sizeof(r_decal_instance_t) == 112, "r_decal_instance_t must match decal_instance_t in decal_vs.glsl");
static_assert(MAX_DECAL_INSTANCES <= 0x1000000, "MAX_DECAL_INSTANCES exceeds the 24 bit instance index");

/**
 * @brief Decal vertex.
 */
typedef struct {

  /**
   * @brief Vertex position.
   */
  vec3_t position;

  /**
   * @brief The instance this vertex draws its decal from: generation in the
   * high 8 bits, instance index in the low 24.
   */
  uint32_t instance;
} r_decal_vertex_t;

/**
 * @brief Decal triangle.
 */
typedef struct {

  /**
   * @brief Triangle vertices.
   */
  r_decal_vertex_t vertexes[3];
} r_decal_triangle_t;

void R_UpdateDecals(const r_view_t *view, CopyPass *pass);
void R_DrawDecals(const r_view_t *view, RenderPass *pass);
void R_InitDecals(void);
void R_ShutdownDecals(void);
void R_UpdateDecalPipeline(void);
#endif
