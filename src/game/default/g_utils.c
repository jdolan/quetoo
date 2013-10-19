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

#include "g_local.h"
#include "bg_pmove.h"

/*
 * @brief
 */
void G_ProjectSpawn(g_edict_t *ent) {
	vec3_t mins, maxs, delta, forward;

	// up
	const vec_t up = ceilf(fabs(PM_SCALE * PM_MINS[2] - PM_MINS[2]));
	ent->s.origin[2] += up;

	// forward, find the old x/y size
	VectorCopy(PM_MINS, mins);
	VectorCopy(PM_MAXS, maxs);
	mins[2] = maxs[2] = 0.0;

	VectorSubtract(maxs, mins, delta);
	const vec_t len0 = VectorLength(delta);

	// and the new x/y size
	VectorScale(delta, PM_SCALE, delta);
	const vec_t len1 = VectorLength(delta);

	const vec_t fwd = ceilf(len1 - len0);

	AngleVectors(ent->s.angles, forward, NULL, NULL);
	VectorMA(ent->s.origin, fwd, forward, ent->s.origin);
}

/*
 * @brief Determines the initial position and directional vectors of a projectile.
 */
void G_InitProjectile(g_edict_t *ent, vec3_t forward, vec3_t right, vec3_t up, vec3_t org) {
	c_trace_t tr;
	vec3_t view, end;

	// use the client's view angles to project the origin flash
	VectorCopy(ent->client->locals.forward, forward);
	VectorCopy(ent->client->locals.right, right);
	VectorCopy(ent->client->locals.up, up);
	VectorCopy(ent->s.origin, org);

	const vec_t up_offset = ent->client->ps.pm_state.flags & PMF_DUCKED ? 0.0 : 14.0;
	const vec_t up_scale = (30.0 - fabs(ent->s.angles[0])) / 30.0;

	VectorMA(org, up_offset * up_scale, up, org);
	VectorMA(org, 8.0, right, org);
	VectorMA(org, 28.0, forward, org);

	// check that we're at a valid origin
	if (gi.PointContents(org) & MASK_PLAYER_SOLID)
		return;

	// correct the destination of the shot for the offset produced above
	UnpackVector(ent->client->ps.pm_state.view_offset, view);
	VectorAdd(ent->s.origin, view, view);

	VectorMA(view, 8192.0, forward, end);
	tr = gi.Trace(view, end, NULL, NULL, ent, MASK_SHOT);

	VectorSubtract(tr.end, org, forward);
	VectorNormalize(forward);

	VectorAngles(forward, view);
	AngleVectors(view, NULL, right, up);
}

/*
 * @brief Searches all active entities for the next one that holds the matching string
 * at field offset (use the ELOFS() macro) in the structure.
 *
 * Searches beginning at the edict after from, or the beginning if NULL
 * NULL will be returned if the end of the list is reached.
 *
 * Example:
 *   G_Find(NULL, EOFS(class_name), "info_player_deathmatch");
 *
 */
g_edict_t *G_Find(g_edict_t *from, ptrdiff_t field, const char *match) {
	char *s;

	if (!from)
		from = g_game.edicts;
	else
		from++;

	for (; from < &g_game.edicts[ge.num_edicts]; from++) {
		if (!from->in_use)
			continue;
		s = *(char **) ((byte *) from + field);
		if (!s)
			continue;
		if (!g_ascii_strcasecmp(s, match))
			return from;
	}

	return NULL;
}

/*
 * @brief Returns entities that have origins within a spherical area
 *
 * G_FindRadius(origin, radius)
 */
g_edict_t *G_FindRadius(g_edict_t *from, vec3_t org, vec_t rad) {
	vec3_t eorg;
	int32_t j;

	if (!from)
		from = g_game.edicts;
	else
		from++;

	for (; from < &g_game.edicts[ge.num_edicts]; from++) {

		if (!from->in_use)
			continue;

		if (from->solid == SOLID_NOT)
			continue;

		for (j = 0; j < 3; j++)
			eorg[j] = org[j] - (from->s.origin[j] + (from->mins[j] + from->maxs[j]) * 0.5);

		if (VectorLength(eorg) > rad)
			continue;

		return from;
	}

	return NULL;
}

/*
 * @brief Searches all active entities for the next one that holds
 * the matching string at fieldofs(use the ELOFS() macro) in the structure.
 *
 * Searches beginning at the edict after from, or the beginning if NULL
 * NULL will be returned if the end of the list is reached.
 *
 */
#define MAXCHOICES	8

g_edict_t *G_PickTarget(char *target_name) {
	g_edict_t *ent = NULL;
	int32_t num_choices = 0;
	g_edict_t *choice[MAXCHOICES];

	if (!target_name) {
		gi.Debug("NULL target_name\n");
		return NULL;
	}

	while (true) {
		ent = G_Find(ent, LOFS(target_name), target_name);
		if (!ent)
			break;
		choice[num_choices++] = ent;
		if (num_choices == MAXCHOICES)
			break;
	}

	if (!num_choices) {
		gi.Debug("Target %s not found\n", target_name);
		return NULL;
	}

	return choice[Random() % num_choices];
}

/*
 * @brief
 */
static void G_UseTargets_Delay(g_edict_t *ent) {
	G_UseTargets(ent, ent->locals.activator);
	G_FreeEdict(ent);
}

/*
 * @brief Search for all entities that the specified entity targets, and call their
 * use functions. Set their activator to our activator. Print our message,
 * if set, to the activator.
 */
void G_UseTargets(g_edict_t *ent, g_edict_t *activator) {
	g_edict_t *t;

	// check for a delay
	if (ent->locals.delay) {
		// create a temp object to fire at a later time
		t = G_Spawn(__func__);
		t->locals.next_think = g_level.time + ent->locals.delay * 1000;
		t->locals.Think = G_UseTargets_Delay;
		t->locals.activator = activator;
		if (!activator)
			gi.Debug("No activator for %s\n", etos(ent));
		t->locals.message = ent->locals.message;
		t->locals.target = ent->locals.target;
		t->locals.kill_target = ent->locals.kill_target;
		return;
	}

	// print the message
	if ((ent->locals.message) && activator->client) {
		gi.WriteByte(SV_CMD_CENTER_PRINT);
		gi.WriteString(ent->locals.message);
		gi.Unicast(activator, true);

		if (ent->locals.noise_index)
			gi.Sound(activator, ent->locals.noise_index, ATTN_NORM);
		else
			gi.Sound(activator, gi.SoundIndex("misc/chat"), ATTN_NORM);
	}

	// kill kill_targets
	if (ent->locals.kill_target) {
		t = NULL;
		while ((t = G_Find(t, LOFS(target_name), ent->locals.kill_target))) {
			G_FreeEdict(t);
			if (!ent->in_use) {
				gi.Debug("%s was removed while using kill_targets\n", etos(ent));
				return;
			}
		}
	}

	// doors fire area portals in a specific way
	const _Bool is_door = g_str_has_prefix(ent->class_name, "func_door");

	// fire targets
	if (ent->locals.target) {
		t = NULL;
		while ((t = G_Find(t, LOFS(target_name), ent->locals.target))) {

			if (is_door && !g_strcmp0(t->class_name, "func_areaportal")) {
				continue;
			}

			if (t == ent) {
				gi.Debug("%s tried to use itself\n", etos(ent));
				continue;
			}

			if (t->locals.Use) {
				t->locals.Use(t, ent, activator);
				if (!ent->in_use) { // see if our target freed us
					gi.Debug("%s was removed while using targets\n", etos(ent));
					break;
				}
			}
		}
	}
}

/*
 * @brief
 */
void G_SetMoveDir(vec3_t angles, vec3_t move_dir) {
	static vec3_t VEC_UP = { 0, -1, 0 };
	static vec3_t MOVE_DIR_UP = { 0, 0, 1 };
	static vec3_t VEC_DOWN = { 0, -2, 0 };
	static vec3_t MOVE_DIR_DOWN = { 0, 0, -1 };

	if (VectorCompare(angles, VEC_UP)) {
		VectorCopy(MOVE_DIR_UP, move_dir);
	} else if (VectorCompare(angles, VEC_DOWN)) {
		VectorCopy(MOVE_DIR_DOWN, move_dir);
	} else {
		AngleVectors(angles, move_dir, NULL, NULL);
	}

	VectorClear(angles);
}

/*
 * @brief
 */
char *G_CopyString(char *in) {
	char *out;

	out = gi.Malloc(strlen(in) + 1, MEM_TAG_GAME_LEVEL);
	strcpy(out, in);
	return out;
}

/*
 * @brief
 */
void G_InitEdict(g_edict_t *e, const char *class_name) {

	e->class_name = class_name;
	e->in_use = true;

	e->locals.timestamp = g_level.time;
	e->s.number = e - g_game.edicts;
}

/*
 * @brief Either finds a free edict, or allocates a new one.
 * Try to avoid reusing an entity that was recently freed, because it
 * can cause the client to think the entity morphed into something else
 * instead of being removed and recreated, which can cause interpolated
 * angles and bad trails.
 */
g_edict_t *G_Spawn(const char *class_name) {
	uint32_t i;
	g_edict_t *e;

	e = &g_game.edicts[sv_max_clients->integer + 1];
	for (i = sv_max_clients->integer + 1; i < ge.num_edicts; i++, e++) {
		if (!e->in_use) {
			G_InitEdict(e, class_name);
			return e;
		}
	}

	if (i >= g_max_entities->value)
		gi.Error("No free edicts for %s\n", class_name);

	ge.num_edicts++;
	G_InitEdict(e, class_name);
	return e;
}

/*
 * @brief Marks the edict as free
 */
void G_FreeEdict(g_edict_t *ed) {
	gi.UnlinkEdict(ed); // unlink from world

	if ((ed - g_game.edicts) <= sv_max_clients->integer)
		return;

	memset(ed, 0, sizeof(*ed));
	ed->class_name = "freed";
}

/*
 * @brief
 */
void G_TouchTriggers(g_edict_t *ent) {
	int32_t i, num;
	g_edict_t *touch[MAX_EDICTS], *hit;

	num = gi.AreaEdicts(ent->abs_mins, ent->abs_maxs, touch, MAX_EDICTS, AREA_TRIGGERS);

	// be careful, it is possible to have an entity in this
	// list removed before we get to it (kill-triggered)
	for (i = 0; i < num; i++) {

		hit = touch[i];

		if (!hit->in_use)
			continue;

		if (!hit->locals.Touch)
			continue;

		hit->locals.Touch(hit, ent, NULL, NULL);
	}
}

/*
 * @brief Call after linking a new trigger in during gameplay
 * to force all entities it covers to immediately touch it
 */
void G_TouchSolids(g_edict_t *ent) {
	int32_t i, num;
	g_edict_t *touch[MAX_EDICTS], *hit;

	num = gi.AreaEdicts(ent->abs_mins, ent->abs_maxs, touch, MAX_EDICTS, AREA_SOLID);

	// be careful, it is possible to have an entity in this
	// list removed before we get to it (kill triggered)
	for (i = 0; i < num; i++) {
		hit = touch[i];

		if (!hit->in_use)
			continue;

		if (ent->locals.Touch)
			ent->locals.Touch(hit, ent, NULL, NULL);

		if (!ent->in_use)
			break;
	}
}

/*
 * @brief Kills all entities that would touch the proposed new positioning
 * of ent. Ent should be unlinked before calling this!
 */
_Bool G_KillBox(g_edict_t *ent) {
	c_trace_t tr;

	while (true) {
		tr = gi.Trace(ent->s.origin, ent->s.origin, ent->mins, ent->maxs, NULL, MASK_PLAYER_SOLID);
		if (!tr.ent)
			break;

		// nail it
		G_Damage(tr.ent, ent, ent, vec3_origin, ent->s.origin, vec3_origin, 9999, 0,
				DAMAGE_NO_PROTECTION, MOD_TELEFRAG);

		// if we didn't kill it, fail
		if (tr.ent->solid)
			return false;
	}

	return true; // all clear
}

/*
 * @brief
 */
char *G_GameplayName(int32_t g) {
	switch (g) {
		case DEATHMATCH:
			return "DEATHMATCH";
		case INSTAGIB:
			return "INSTAGIB";
		case ARENA:
			return "ARENA";
		default:
			return "DEATHMATCH";
	}
}

/*
 * @brief
 */
int32_t G_GameplayByName(const char *c) {

	if (!c || *c == '\0')
		return DEATHMATCH;

	switch (*c) { // hack for numeric matches, atoi wont cut it
		case '0':
			return DEFAULT;
		case '1':
			return DEATHMATCH;
		case '2':
			return INSTAGIB;
		case '3':
			return ARENA;
		default:
			break;
	}

	char name[MAX_TOKEN_CHARS];
	Lowercase(c, name);

	if (strstr(name, "insta"))
		return INSTAGIB;
	if (strstr(name, "arena"))
		return ARENA;
	if (strstr(name, "deathmatch") || strstr(name, "dm"))
		return DEATHMATCH;

	return DEFAULT;
}

/*
 * @brief
 */
g_team_t *G_TeamByName(const char *c) {

	if (!c || !*c)
		return NULL;

	if (!StrColorCmp(g_team_good.name, c))
		return &g_team_good;

	if (!StrColorCmp(g_team_evil.name, c))
		return &g_team_evil;

	return NULL;
}

/*
 * @brief
 */
g_team_t *G_TeamForFlag(g_edict_t *ent) {

	if (!g_level.ctf)
		return NULL;

	if (!ent->locals.item || ent->locals.item->type != ITEM_FLAG)
		return NULL;

	if (!g_strcmp0(ent->class_name, "item_flag_team1"))
		return &g_team_good;

	if (!g_strcmp0(ent->class_name, "item_flag_team2"))
		return &g_team_evil;

	return NULL;
}

/*
 * @brief
 */
g_edict_t *G_FlagForTeam(g_team_t *t) {
	g_edict_t *ent;
	char class_name[32];
	uint32_t i;

	if (!g_level.ctf)
		return NULL;

	if (t == &g_team_good) {
		g_strlcpy(class_name, "item_flag_team1", sizeof(class_name));
	} else if (t == &g_team_evil) {
		g_strlcpy(class_name, "item_flag_team2", sizeof(class_name));
	} else {
		return NULL;
	}

	i = sv_max_clients->integer + 1;
	while (i < ge.num_edicts) {

		ent = &ge.edicts[i++];

		if (!ent->locals.item || ent->locals.item->type != ITEM_FLAG)
			continue;

		// when a carrier is killed, we spawn a new temporary flag
		// where they died. we are generally not interested in these.
		if (ent->locals.spawn_flags & SF_ITEM_DROPPED)
			continue;

		if (!g_strcmp0(ent->class_name, class_name))
			return ent;
	}

	return NULL;
}

/*
 * @brief
 */
uint32_t G_EffectForTeam(g_team_t *t) {

	if (!t)
		return 0;

	return (t == &g_team_good ? EF_CTF_BLUE : EF_CTF_RED);
}

/*
 * @brief
 */
g_team_t *G_OtherTeam(g_team_t *t) {

	if (!t)
		return NULL;

	if (t == &g_team_good)
		return &g_team_evil;

	if (t == &g_team_evil)
		return &g_team_good;

	return NULL;
}

/*
 * @brief
 */
g_team_t *G_SmallestTeam(void) {
	int32_t i, g, e;
	g_client_t *cl;

	g = e = 0;

	for (i = 0; i < sv_max_clients->integer; i++) {
		if (!g_game.edicts[i + 1].in_use)
			continue;

		cl = g_game.edicts[i + 1].client;

		if (cl->locals.persistent.team == &g_team_good)
			g++;
		else if (cl->locals.persistent.team == &g_team_evil)
			e++;
	}

	return g < e ? &g_team_good : &g_team_evil;
}

/*
 * @brief
 */
g_client_t *G_ClientByName(char *name) {
	int32_t i, j, min;
	g_client_t *cl, *ret;

	if (!name)
		return NULL;

	ret = NULL;
	min = 9999;

	for (i = 0; i < sv_max_clients->integer; i++) {
		if (!g_game.edicts[i + 1].in_use)
			continue;

		cl = g_game.edicts[i + 1].client;
		if ((j = g_strcmp0(name, cl->locals.persistent.net_name)) < min) {
			ret = cl;
			min = j;
		}
	}

	return ret;
}

/*
 * @brief
 */
_Bool G_IsStationary(g_edict_t *ent) {

	if (!ent)
		return false;

	return VectorCompare(vec3_origin, ent->locals.velocity);
}

/*
 * @brief Writes the specified animation byte, toggling the high bit to restart the
 * sequence if desired and necessary.
 */
static void G_SetAnimation_(byte *dest, entity_animation_t anim, _Bool restart) {

	if (restart) {
		if (*dest == anim) {
			anim |= ANIM_TOGGLE_BIT;
		}
	}

	*dest = anim;
}

/*
 * @brief Assigns the specified animation to the correct member(s) on the specified
 * entity. If requested, the current animation will be restarted.
 */
void G_SetAnimation(g_edict_t *ent, entity_animation_t anim, _Bool restart) {

	// certain sequences go to both torso and leg animations

	if (anim < ANIM_TORSO_GESTURE) {
		G_SetAnimation_(&ent->s.animation1, anim, restart);
		G_SetAnimation_(&ent->s.animation2, anim, restart);
		return;
	}

	// while most go to one or the other, and are throttled

	if (anim < ANIM_LEGS_WALKCR) {
		if (restart || ent->client->locals.animation1_time <= g_level.time) {
			G_SetAnimation_(&ent->s.animation1, anim, restart);
			ent->client->locals.animation1_time = g_level.time + 50;
		}
	} else {
		if (restart || ent->client->locals.animation2_time <= g_level.time) {
			G_SetAnimation_(&ent->s.animation2, anim, restart);
			ent->client->locals.animation2_time = g_level.time + 50;
		}
	}
}

/*
 * @brief Returns true if the entity is currently using the specified animation.
 */
_Bool G_IsAnimation(g_edict_t *ent, entity_animation_t anim) {
	byte a;

	if (anim < ANIM_LEGS_WALK)
		a = ent->s.animation1;
	else
		a = ent->s.animation2;

	return (a & ~ANIM_TOGGLE_BIT) == anim;
}

