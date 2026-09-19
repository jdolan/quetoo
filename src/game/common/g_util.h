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

void G_KillBox(GameEntity *ent);
void G_Explode(GameEntity *ent, int16_t damage, int16_t knockback, float radius, uint32_t mod);
void G_Gib(GameEntity *ent);
void G_InitPlayerSpawn(GameEntity *ent);
Box3 G_PlayerBounds(void);
void G_ClientProjectile(const GameClient *cl, Vec3 *forward, Vec3 *right, Vec3 *up, Vec3 *org, float hand);
GameEntity *G_Find(GameEntity *from, ptrdiff_t field, const char *match);

void G_AddSpawn(Vector **spawns, GameEntity *spot);
void G_CollectSpawns(const char *class_name, Vector **spawns);
void G_SetSpawnPoints(GameSpawnPoints *points, const Vector *spawns);
GameEntity *G_PickTarget(const char *target_name);
void G_UseTargets(GameEntity *ent, GameEntity *activator);
void G_SetMoveDir(GameEntity *ent);
const Gameplay *G_GameplayByName(const char *c);
const Gameplay *G_GameplayById(GameplayId id);
GameTeam *G_TeamByName(const char *c);
size_t G_TeamSize(const GameTeam *team);
GameTeam *G_SmallestTeam(void);
GameClient *G_ClientByName(char *name);
bool G_IsMeat(const GameEntity *ent);
bool G_IsStationary(const GameEntity *ent);
bool G_IsStructural(const CmTrace *trace);
bool G_IsSky(const CmTrace *trace);
void G_SetAnimation(GameClient *cl, EntityAnimation anim, bool restart);
bool G_IsAnimation(GameClient *cl, EntityAnimation anim);
GameEntity *G_AllocEntity(const char *classname);
GameEntity *G_AllocEntityAt(int32_t number, const char *classname);
void G_InvalidateEntityReferences(const GameEntity *ent);
void G_FreeEntity(GameEntity *ent);
void G_TeamCenterPrint(const GameTeam *team, const char *fmt, ...) __attribute__((format(printf, 2, 3)));

#define G_ForEachClient(var, block) \
{ \
  for (int32_t i = 0; i < sv_max_clients->integer; i++) { \
    GameClient *var = ge.clients[i]; \
    if (var->in_use) { \
      block; \
    } \
  } \
}

#define G_ForEachFreeClient(var, block) \
{ \
  for (int32_t i = 0; i < sv_max_clients->integer; i++) { \
    GameClient *var = ge.clients[i]; \
    if (!var->in_use) { \
      block; \
    } \
  } \
}

#define G_ForEachEntity(var, block) \
{ \
  for (int32_t i = 0; i < sv_max_entities->integer; i++) { \
    GameEntity *var = ge.entities[i]; \
    if (var->in_use) { \
      block; \
    } \
  } \
}

#define G_ForEachFreeEntity(var, block) \
{ \
  for (int32_t i = 0; i < sv_max_entities->integer; i++) { \
    GameEntity *var = ge.entities[i]; \
    if (!var->in_use) { \
      block; \
    } \
  } \
}

#endif
