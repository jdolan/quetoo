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

#include "g_types.h"

#if defined(__G_LOCAL_H__)

  extern GameItem *g_items;

  /**
   * @brief Item bounding box scaling.
   */
  #define ITEM_SCALE 1.f

  /**
   * @brief Quad Damage scaling factors
   */
  #define QUAD_DAMAGE_FACTOR 2.5f
  #define QUAD_KNOCKBACK_FACTOR 2.f

  extern const Box3 ITEM_BOUNDS;

  bool G_AddAmmo(GameClient *cl, const GameItem *item, int16_t count);
  void G_CheckItemHazard(GameEntity *ent);
  GameEntity *G_DropItem(GameClient *cl, const GameItem *item);
  void G_DropInventoryItem(GameClient *cl, const GameItem *item);
  bool G_ItemAvailable(const GameItem *item);
  const GameItem *G_FindItem(const char *name);
  const GameItem *G_FindItemByClassName(const char *classname);
  const GameItem *G_MappedWeapon(const GameItem *weapon);
  const GameItem *G_ClientArmor(const GameClient *cl);
  const GameArmorInfo *G_ArmorInfo(const GameItem *armor);
  void G_PrecacheItem(const GameItem *it);
  void G_SetItemRespawn(GameEntity *ent, uint32_t delay);
  void G_SpawnItem(GameEntity *ent, const GameItem *item);
  bool G_SetAmmo(GameClient *cl, const GameItem *item, int16_t count);
  GameEntity *G_TossQuadDamage(GameClient *cl);
  GameEntity *G_TossInvisibility(GameClient *cl);
  GameEntity *G_TossInvulnerability(GameClient *cl);
  void G_TouchItem(GameEntity *ent, GameEntity *other, const CmTrace *trace);
  void G_InitItems(void);

#endif
