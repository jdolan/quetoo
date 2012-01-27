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

#ifndef __G_UTILS_H__
#define __G_UITLS_H__

#ifdef __G_LOCAL_H__

boolean_t G_KillBox(g_edict_t *ent);
void G_ProjectSpawn(g_edict_t *ent);
void G_InitProjectile(g_edict_t *ent, vec3_t forward, vec3_t right, vec3_t up, vec3_t org);
g_edict_t *G_Find(g_edict_t *from, ptrdiff_t field, const char *match);
g_edict_t *G_FindRadius(g_edict_t *from, vec3_t org, float rad);
g_edict_t *G_PickTarget(char *target_name);
void G_UseTargets(g_edict_t *ent, g_edict_t *activator);
void G_SetMoveDir(vec3_t angles, vec3_t movedir);
char *G_GameplayName(int g);
int G_GameplayByName(char *c);
g_team_t *G_TeamByName(char *c);
g_team_t *G_OtherTeam(g_team_t *t);
g_team_t *G_TeamForFlag(g_edict_t *ent);
g_edict_t *G_FlagForTeam(g_team_t *t);
unsigned int G_EffectForTeam(g_team_t *t);
g_team_t *G_SmallestTeam(void);
g_client_t *G_ClientByName(char *name);
boolean_t G_IsStationary(g_edict_t *ent);
void G_SetAnimation(g_edict_t *ent, entity_animation_t anim, boolean_t restart);
boolean_t G_IsAnimation(g_edict_t *ent, entity_animation_t anim);
g_edict_t *G_Spawn(void);
void G_InitEdict(g_edict_t *e);
void G_FreeEdict(g_edict_t *e);
void G_TouchTriggers(g_edict_t *ent);
void G_TouchSolids(g_edict_t *ent);
c_trace_t G_PushEntity(g_edict_t *ent, vec3_t push);
char *G_CopyString(char *in);
float *tv(float x, float y, float z);
char *vtos(vec3_t v);

#endif /* __G_LOCAL_H__ */

#endif /* __G_UTILS_H__ */
