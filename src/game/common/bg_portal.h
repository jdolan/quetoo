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

#include "shared/shared.h"

/**
 * @brief The basis a trigger_portal's compiler-baked `angles` describe. The compiler bakes the
 * direction of travel *into* the tagged face; on arrival, a toucher should instead face and move
 * along that face's outward normal, so the arrival basis has forward reversed, and right reversed
 * with it (right is forward × up, and flipping forward alone would mirror left and right). Up is
 * left alone: gravity is the one thing a portal must never turn over.
 */
static inline void Bg_PortalBasis(const vec3_t angles, bool arrival, vec3_t *right, vec3_t *up, vec3_t *forward) {

  Vec3_Vectors(angles, forward, right, up);

  if (arrival) {
    *forward = Vec3_Negate(*forward);
    *right = Vec3_Negate(*right);
  }
}

/**
 * @brief Re-expresses `v` in the departure frame `(right_a, up_a, fwd_a)`, then rebuilds it in the
 * arrival frame `(right_b, up_b, fwd_b)`. This is the entire trick to a portal: a position or
 * direction is only ever meaningful relative to the face it was measured from, so carrying it
 * across as those same three relative numbers - however the two faces happen to be oriented in
 * the world - is what makes the crossing read as one continuous surface. The game uses it to
 * teleport, and the client game uses it to place the camera that renders the view through.
 */
static inline vec3_t Bg_PortalCarry(const vec3_t v,
                                    const vec3_t right_a, const vec3_t up_a, const vec3_t fwd_a,
                                    const vec3_t right_b, const vec3_t up_b, const vec3_t fwd_b) {

  const vec3_t local = Vec3(Vec3_Dot(v, right_a), Vec3_Dot(v, up_a), Vec3_Dot(v, fwd_a));

  return Vec3_Add(Vec3_Scale(right_b, local.x),
                  Vec3_Add(Vec3_Scale(up_b, local.y), Vec3_Scale(fwd_b, local.z)));
}
