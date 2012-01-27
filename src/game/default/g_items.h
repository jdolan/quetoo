/*
 * Copyright(c) 1997-2001 Id Software, Inc.
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

#ifndef __G_ITEM_H__
#define __G_ITEM_H__

#ifdef __G_LOCAL_H__

extern g_item_t g_items[];

#define ITEM_INDEX(x) ((x) - g_items)

boolean_t G_AddAmmo(g_edict_t *ent, g_item_t *item, short count);
g_edict_t *G_DropItem(g_edict_t *ent, g_item_t *item);
g_item_t *G_FindItem(const char *pickup_name);
g_item_t *G_FindItemByClassname(const char *class_name);
void G_InitItems(void);
g_item_t *G_ItemByIndex(unsigned short index);
void G_PrecacheItem(g_item_t *it);
void G_ResetFlag(g_edict_t *ent);
void G_SetItemNames(void);
void G_SetItemRespawn(g_edict_t *ent, float delay);
void G_SpawnItem(g_edict_t *ent, g_item_t *item);
void G_TossFlag(g_edict_t *self);
void G_TossQuadDamage(g_edict_t *self);
void G_TouchItem(g_edict_t *ent, g_edict_t *other, c_bsp_plane_t *plane, c_bsp_surface_t *surf);

#endif /* __G_LOCAL_H__ */

#endif /* __G_ITEM_H__ */
