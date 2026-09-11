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

/**
 * @return True if a portal facing along `forward` is a floor or ceiling, entered vertically and
 * feet or head first, rather than a wall walked into center first.
 */
static inline bool Bg_PortalIsHorizontal(const vec3_t forward) {
  return fabsf(forward.z) > 0.7071f;
}

/**
 * @return How far `bounds` reach along `dir` from their origin: the leading edge of a body
 * moving that way.
 */
static inline float Bg_PortalExtent(const box3_t bounds, const vec3_t dir) {

  float extent = 0.f;

  for (int32_t i = 0; i < 3; i++) {
    extent += dir.xyz[i] >= 0.f ? bounds.maxs.xyz[i] * dir.xyz[i] : bounds.mins.xyz[i] * dir.xyz[i];
  }

  return extent;
}

/**
 * @brief How far short of a portal's face a player moving into it transits, in units, so their
 * view never reaches the face itself, where the near plane would cut it open onto the recess
 * behind. The game and the client game's prediction must agree on this, or the client runs on
 * ahead. Must be less than PORTAL_TRANSIT_OFFSET, or an arrival sits inside the far portal's
 * lead already.
 */
#define PORTAL_TRANSIT_LEAD 3.f

/**
 * @return How far behind a portal's face a `body` may be centered while remaining within the
 * portal's `volume`: as deep as the volume is, less the body's own reach behind its center.
 */
static inline float Bg_PortalRecess(const box3_t volume, const vec3_t origin, const vec3_t outward, const box3_t body) {
  return Bg_PortalExtent(Box3_Translate(volume, Vec3_Negate(origin)), Vec3_Negate(outward)) -
         Bg_PortalExtent(body, Vec3_Negate(outward));
}

/**
 * @brief How far past the far face a transit lands, in units: clear of the face's own touch
 * field, of the lead above, and of the near plane's corners on a wide display should the view
 * turn straight back to it. A player arriving at a wall does not use it: they land as far into
 * the far portal as they were into the near one, as deep as its recess allows, and walk on out
 * of it. At a floor or ceiling they land this far inside with their center, to be held by it.
 */
#define PORTAL_TRANSIT_OFFSET 4.f

/**
 * @brief How far beyond a portal's volume players are still crossing it, in units, and so still
 * pass through each other: an arrival at a wall lands just outside the volume, and must not be
 * stuck in whoever is standing there.
 */
#define PORTAL_PLAYER_CLEARANCE 24.f
