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

#ifndef __G_ENTITY_FUNC_H__
#define __G_ENTITY_FUNC_H__

#ifdef __G_LOCAL_H__

void G_func_areaportal(g_edict_t *ent);
void G_func_button(g_edict_t *ent);
void G_func_conveyor(g_edict_t *self);
void G_func_door(g_edict_t *ent);
void G_func_door_secret(g_edict_t *ent);
void G_func_killbox(g_edict_t *ent);
void G_func_plat(g_edict_t *ent);
void G_func_rotating(g_edict_t *ent);
void G_func_timer(g_edict_t *self);
void G_func_train(g_edict_t *ent);
void G_func_wall(g_edict_t *self);
void G_func_water(g_edict_t *self);

#endif /* __G_LOCAL_H__ */

#endif /* __G_ENTITY_FUNC_H__ */
