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

#ifdef __GAME_LOCAL_H__

	extern const size_t g_num_items;
	
	const g_item_t *G_ItemList(void);

	/**
	* @brief Item bounding box scaling.
	*/
	#define ITEM_SCALE 1.0
	
	/**
	 * @brief Quad DAmage scaling factor
	 */
	#define QUAD_DAMAGE_FACTOR 2.5
	#define QUAD_KNOCKBACK_FACTOR 2.0

	/**
	 * @brief Haste scaling factor
	 */
	#define TECH_HASTE_FACTOR 0.75
	
	/**
	 * @brief Resist scaling factors
	 */
	#define TECH_RESIST_DAMAGE_FACTOR 0.5
	#define TECH_RESIST_KNOCKBACK_FACTOR 0.75
	
	/**
	 * @brief Strength scaling factor
	 */
	#define TECH_STRENGTH_DAMAGE_FACTOR 1.5
	#define TECH_STRENGTH_KNOCKBACK_FACTOR 1.25

	/**
	 * @brief Regen constants
	 */
	#define TECH_REGEN_TICK_TIME 750
	#define TECH_REGEN_HEALTH 2
	
	/**
	 * @brief Vampire scaling factor
	 */
	#define TECH_VAMPIRE_DAMAGE_FACTOR 0.25

	extern const box_t ITEM_BOUNDS;

	const g_item_t *G_CarryingTech(const g_entity_t *ent);
	_Bool G_HasTech(const g_entity_t *player, const g_tech_t tech);
	_Bool G_AddAmmo(g_entity_t *ent, const g_item_t *item, int16_t count);
	g_entity_t *G_DropItem(g_entity_t *ent, const g_item_t *item);
	const g_item_t *G_FindItem(const char *name);
	const g_item_t *G_FindItemByClassName(const char *class_name);
	const g_item_t *G_ItemByIndex(uint16_t index);
	const g_item_t *G_ClientArmor(const g_entity_t *ent);
	const g_armor_info_t *G_ArmorInfo(const g_item_t *armor);
	void G_PrecacheItem(const g_item_t *it);
	void G_ResetDroppedFlag(g_entity_t *ent);
	void G_ResetDroppedTech(g_entity_t *ent);
	g_entity_t *G_SelectTechSpawnPoint(void);
	void G_ResetItem(g_entity_t *ent);
	void G_SetItemRespawn(g_entity_t *ent, uint32_t delay);
	void G_SpawnItem(g_entity_t *ent, const g_item_t *item);
	_Bool G_SetAmmo(g_entity_t *ent, const g_item_t *item, int16_t count);
	g_entity_t *G_TossFlag(g_entity_t *self);
	g_entity_t *G_TossTech(g_entity_t *self);
	g_entity_t *G_TossQuadDamage(g_entity_t *self);
	void G_TouchItem(g_entity_t *ent, g_entity_t *other, const cm_bsp_plane_t *plane, const cm_bsp_texinfo_t *texinfo);
	void G_InitItems(void);

#endif /* __GAME_LOCAL_H__ */
