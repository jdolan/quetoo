/*
 * Copyright(c) 1997-2001 id Software, Inc.
 * Copyright(c) 2002 The Quakeforge Project.
 * Copyright(c) 2006 Quake2World.
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

#ifndef __GAME_ITEM_H__
#define __GAME_ITEM_H__

#ifdef __GAME_LOCAL_H__

#include "g_types.h"

extern const g_item_t g_items[];
extern const uint16_t g_num_items;

#define ITEM_INDEX(x) ((x) - g_items)

/*
 * @brief Item bounding box scaling.
 */
#define ITEM_SCALE 1.0

extern const vec3_t ITEM_MINS;
extern const vec3_t ITEM_MAXS;

_Bool G_AddAmmo(g_edict_t *ent, const g_item_t *item, int16_t count);
g_edict_t *G_DropItem(g_edict_t *ent, const g_item_t *item);
const g_item_t *G_FindItem(const char *name);
const g_item_t *G_FindItemByClassName(const char *class_name);
const g_item_t *G_ItemByIndex(uint16_t index);
const g_item_t *G_ClientArmor(const g_edict_t *ent);
void G_PrecacheItem(const g_item_t *it);
void G_ResetFlag(g_edict_t *ent);
void G_SetItemRespawn(g_edict_t *ent, uint32_t delay);
void G_SpawnItem(g_edict_t *ent, const g_item_t *item);
_Bool G_SetAmmo(g_edict_t *ent, const g_item_t *item, int16_t count);
g_edict_t *G_TossFlag(g_edict_t *self);
g_edict_t *G_TossQuadDamage(g_edict_t *self);
void G_TouchItem(g_edict_t *ent, g_edict_t *other, cm_bsp_plane_t *plane, cm_bsp_surface_t *surf);

#endif /* __GAME_LOCAL_H__ */

#endif /* __GAME_ITEM_H__ */
