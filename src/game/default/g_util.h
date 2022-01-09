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

void G_KillBox(g_entity_t *ent);
void G_Explode(g_entity_t *ent, int16_t damage, int16_t knockback, float radius, uint32_t mod);
void G_Gib(g_entity_t *ent);
void G_InitPlayerSpawn(g_entity_t *ent);
void G_InitProjectile(const g_entity_t *ent, vec3_t *forward, vec3_t *right, vec3_t *up, vec3_t *org, float hand);
g_entity_t *G_Find(g_entity_t *from, ptrdiff_t field, const char *match);
g_entity_t *G_FindPtr(g_entity_t *from, ptrdiff_t field, const void *match);
g_entity_t *G_FindRadius(g_entity_t *from, const vec3_t org, float rad);
g_entity_t *G_PickTarget(const char *target_name);
void G_UseTargets(g_entity_t *ent, g_entity_t *activator);
void G_SetMoveDir(g_entity_t *ent);
char *G_GameplayName(int32_t g);
g_gameplay_t G_GameplayByName(const char *c);
g_team_t *G_TeamByName(const char *c);
const g_item_t *G_IsFlagBearer(const g_entity_t *ent);
g_team_t *G_TeamForFlag(const g_entity_t *ent);
g_entity_t *G_FlagForTeam(const g_team_t *t);
uint32_t G_EffectForTeam(const g_team_t *t);
size_t G_TeamSize(const g_team_t *team);
g_team_t *G_SmallestTeam(void);
g_entity_t *G_EntityByName(char *name);
g_client_t *G_ClientByName(char *name);
g_hook_style_t G_HookStyleByName(const char *s);
_Bool G_IsMeat(const g_entity_t *ent);
_Bool G_IsStationary(const g_entity_t *ent);
_Bool G_IsStructural(const cm_trace_t *trace);
_Bool G_IsSky(const cm_trace_t *trace);
void G_SetAnimation(g_entity_t *ent, entity_animation_t anim, _Bool restart);
_Bool G_IsAnimation(g_entity_t *ent, entity_animation_t anim);
g_entity_t *G_AllocEntity_(const char *class_name);
void G_InitEntity(g_entity_t *ent, const char *class_name);
void G_ClearEntity(g_entity_t *ent);
void G_FreeEntity(g_entity_t *ent);
void G_TeamCenterPrint(const g_team_t *team, const char *fmt, ...) __attribute__((format(printf, 2, 3)));

#define G_AllocEntity() G_AllocEntity_(__func__)

#endif /* __GAME_LOCAL_H__ */
