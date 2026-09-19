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

#include "g_local.h"

/**
 * @brief Emit a water ripple and optional splash effect.
 * @details The caller must pass either an entity, or two points to trace between.
 * @param ent The entity entering the water, or `NULL`.
 * @param pos1 The start position to trace for liquid from.
 * @param pos2 The end position to trace for liquid to.
 * @param size The ripple size, or 0.0 to use the entity's size.
 * @param splash True to emit a splash effect, false otherwise.
 */
void G_Ripple(GameEntity *ent, const Vec3 pos1, const Vec3 pos2, float size, bool splash) {

  CmTrace tr = gi.Trace(pos1, pos2, Box3_Zero(), ent, CONTENTS_MASK_LIQUID);
  if (!tr.brushSide) {
    tr = gi.Trace(pos2, pos1, Box3_Zero(), ent, CONTENTS_MASK_LIQUID);
  }
  if (!tr.brushSide) {
    return;
  }

  const Vec3 pos = Vec3_Add(tr.end, Vec3_Up());
  const Vec3 dir = tr.plane.normal;

  if (ent) {
    if (g_level.time - ent->rippleTime < 400) {
      return;
    }
    
    ent->rippleTime = g_level.time;

    if (size == 0.f) {
      if (ent->rippleSize) {
        size = ent->rippleSize;
      } else {
        size = Clampf(Box3_Distance(ent->bounds), 12.0, 64.0);
      }
    }
  }

  gi.WriteByte(SV_CMD_TEMP_ENTITY);
  gi.WriteByte(TE_RIPPLE);
  gi.WritePosition(pos);
  gi.WriteDir(dir);
  gi.WriteLong((int32_t) (ptrdiff_t) (tr.brushSide - gi.Bsp()->brushSides));
  gi.WriteByte((uint8_t) size);
  gi.WriteByte((uint8_t) splash);

  gi.Multicast(pos, MULTICAST_PVS);

  if (!(tr.contents & CONTENTS_TRANSLUCENT)) {

    gi.WriteByte(SV_CMD_TEMP_ENTITY);
    gi.WriteByte(TE_RIPPLE);
    gi.WritePosition(Vec3_Add(pos, Vec3_Down()));
    gi.WriteDir(Vec3_Negate(dir));
    gi.WriteLong((int32_t) (ptrdiff_t) (tr.brushSide - gi.Bsp()->brushSides));
    gi.WriteByte((uint8_t) size);
    gi.WriteByte((uint8_t) false);

    gi.Multicast(pos, MULTICAST_PVS);
  }
}

/**
 * @brief Returns true if the entity is facing a wall at too close proximity
 * for the specified projectile.
 */
bool G_ImmediateWall(GameEntity *ent, GameEntity *projectile) {

  const CmTrace tr = gi.Trace(ent->s.origin, projectile->s.origin, projectile->bounds,
                                 ent, CONTENTS_MASK_SOLID);

  return tr.fraction < 1.0;
}
