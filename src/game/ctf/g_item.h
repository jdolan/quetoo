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

#if defined(__GAME_LOCAL_H__)

  extern g_item_t *g_items;

  /**
   * @brief Item bounding box scaling.
   */
  #define ITEM_SCALE 1.f

  /**
   * @brief Quad Damage scaling factors
   */
  #define QUAD_DAMAGE_FACTOR 2.5f
  #define QUAD_KNOCKBACK_FACTOR 2.f

  /**
   * @brief Haste scaling factor
   */
  #define TECH_HASTE_FACTOR 0.75f

  /**
   * @brief Resist scaling factors
   */
  #define TECH_RESIST_DAMAGE_FACTOR 0.5f
  #define TECH_RESIST_KNOCKBACK_FACTOR 0.75f

  /**
   * @brief Strength scaling factor
   */
  #define TECH_STRENGTH_DAMAGE_FACTOR 1.5f
  #define TECH_STRENGTH_KNOCKBACK_FACTOR 1.25f

  /**
   * @brief Regen constants
   */
  #define TECH_REGEN_TICK_TIME 500
  #define TECH_REGEN_HEALTH 1
  
  /**
   * @brief Vampire scaling factor
   */
  #define TECH_VAMPIRE_DAMAGE_FACTOR 0.25f

  extern const box3_t ITEM_BOUNDS;

  const g_item_t *G_GetTech(const g_client_t *cl);
  bool G_HasTech(const g_client_t *cl, g_item_tag_t tech);
  bool G_AddAmmo(g_client_t *cl, const g_item_t *item, int16_t count);
  g_entity_t *G_DropItem(g_client_t *cl, const g_item_t *item);
  bool G_ItemAvailable(const g_item_t *item);
  const g_item_t *G_FindItem(const char *name);
  const g_item_t *G_FindItemByClassName(const char *classname);
  const g_item_t *G_MappedWeapon(const g_item_t *weapon);
  const g_item_t *G_ClientArmor(const g_client_t *cl);
  const g_armor_info_t *G_ArmorInfo(const g_item_t *armor);
  void G_PrecacheItem(const g_item_t *it);
  void G_ResetDroppedFlag(g_entity_t *ent);
  void G_ResetDroppedTech(g_entity_t *ent);
  g_entity_t *G_SelectTechSpawnPoint(void);
  void G_ResetItem(g_entity_t *ent);
  void G_SetItemRespawn(g_entity_t *ent, uint32_t delay);
  void G_SpawnItem(g_entity_t *ent, const g_item_t *item);
  bool G_SetAmmo(g_client_t *cl, const g_item_t *item, int16_t count);
  g_entity_t *G_TossFlag(g_client_t *cl);
  g_entity_t *G_TossTech(g_client_t *cl);
  g_entity_t *G_TossQuadDamage(g_client_t *cl);
  g_entity_t *G_TossInvisibility(g_client_t *cl);
  g_entity_t *G_TossInvulnerability(g_client_t *cl);
  void G_TouchItem(g_entity_t *ent, g_entity_t *other, const cm_trace_t *trace);
  void G_InitItems(void);

#endif /* __GAME_LOCAL_H__ */
