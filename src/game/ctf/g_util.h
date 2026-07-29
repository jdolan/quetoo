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

void G_KillBox(g_entity_t *ent);
void G_Explode(g_entity_t *ent, int16_t damage, int16_t knockback, float radius, uint32_t mod);
void G_Gib(g_entity_t *ent);
void G_InitPlayerSpawn(g_entity_t *ent);
void G_ClientProjectile(const g_client_t *cl, vec3_t *forward, vec3_t *right, vec3_t *up, vec3_t *org, float hand);
g_entity_t *G_Find(g_entity_t *from, ptrdiff_t field, const char *match);
g_entity_t *G_PickTarget(const char *target_name);
void G_UseTargets(g_entity_t *ent, g_entity_t *activator);
void G_SetMoveDir(g_entity_t *ent);
char *G_GameplayName(int32_t g);
g_gameplay_t G_GameplayByName(const char *c);
g_team_t *G_TeamByName(const char *c);
const g_item_t *G_GetFlag(const g_client_t *cl);
g_team_t *G_TeamForFlag(const g_entity_t *ent);
g_entity_t *G_FlagForTeam(const g_team_t *team);
int32_t G_EffectForTeam(const g_team_t *team);
size_t G_TeamSize(const g_team_t *team);
g_team_t *G_SmallestTeam(void);
g_client_t *G_ClientByName(char *name);
g_hook_style_t G_HookStyleByName(const char *s);
bool G_IsMeat(const g_entity_t *ent);
bool G_IsStationary(const g_entity_t *ent);
bool G_IsStructural(const cm_trace_t *trace);
bool G_IsSky(const cm_trace_t *trace);
void G_SetAnimation(g_client_t *cl, entity_animation_t anim, bool restart);
bool G_IsAnimation(g_client_t *cl, entity_animation_t anim);
g_entity_t *G_AllocEntity(const char *classname);
void G_FreeEntity(g_entity_t *ent);
void G_TeamCenterPrint(const g_team_t *team, const char *fmt, ...) __attribute__((format(printf, 2, 3)));

#define G_ForEachClient(var, block) \
{ \
  for (int32_t i = 0; i < sv_max_clients->integer; i++) { \
    g_client_t *var = ge.clients[i]; \
    if (var->in_use) { \
      block; \
    } \
  } \
}

#define G_ForEachFreeClient(var, block) \
{ \
  for (int32_t i = 0; i < sv_max_clients->integer; i++) { \
    g_client_t *var = ge.clients[i]; \
    if (!var->in_use) { \
      block; \
    } \
  } \
}

#define G_ForEachEntity(var, block) \
{ \
  for (int32_t i = 0; i < sv_max_entities->integer; i++) { \
    g_entity_t *var = ge.entities[i]; \
    if (var->in_use) { \
      block; \
    } \
  } \
}

#define G_ForEachFreeEntity(var, block) \
{ \
  for (int32_t i = 0; i < sv_max_entities->integer; i++) { \
    g_entity_t *var = ge.entities[i]; \
    if (!var->in_use) { \
      block; \
    } \
  } \
}

#endif /* __GAME_LOCAL_H__ */
