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

/*
 * G_ProjectSpawn
 */
void G_ProjectSpawn(g_edict_t *ent) {
	vec3_t mins, maxs, delta, forward;
	float up, len0, len1, fwd;

	// up
	up = ceilf(fabs(PM_SCALE * PM_MINS[2] - PM_MINS[2]));
	ent->s.origin[2] += up;

	// forward, find the old x/y size
	VectorCopy(PM_MINS, mins);
	VectorCopy(PM_MAXS, maxs);
	mins[2] = maxs[2] = 0.0;

	VectorSubtract(maxs, mins, delta);
	len0 = VectorLength(delta);

	// and the new x/y size
	VectorScale(delta, PM_SCALE, delta);
	len1 = VectorLength(delta);

	fwd = ceilf(len1 - len0);

	AngleVectors(ent->s.angles, forward, NULL, NULL);
	VectorMA(ent->s.origin, fwd, forward, ent->s.origin);
}

/*
 * G_SetupProjectile
 *
 * Determines the initial position and directional vectors of a projectile.
 */
void G_InitProjectile(g_edict_t *ent, vec3_t forward, vec3_t right, vec3_t up,
		vec3_t org) {

	AngleVectors(ent->client->angles, forward, right, up);

	VectorCopy(ent->s.origin, org);

	VectorMA(org, ent->view_height - 10.0, up, org);
	VectorMA(org, 6.0, right, org);
	VectorMA(org, 30.0, forward, org);
}

/*
 * G_Find
 *
 * Searches all active entities for the next one that holds the matching string
 * at field offset (use the FOFS() macro) in the structure.
 *
 * Searches beginning at the edict after from, or the beginning if NULL
 * NULL will be returned if the end of the list is reached.
 *
 * Example:
 *   G_Find(NULL, FOFS(class_name), "info_player_deathmatch");
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
		if (!strcasecmp(s, match))
			return from;
	}

	return NULL;
}

/*
 * G_FindRadius
 *
 * Returns entities that have origins within a spherical area
 *
 * G_FindRadius(origin, radius)
 */
g_edict_t *G_FindRadius(g_edict_t *from, vec3_t org, float rad) {
	vec3_t eorg;
	int j;

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
			eorg[j] = org[j] - (from->s.origin[j] + (from->mins[j]
					+ from->maxs[j]) * 0.5);

		if (VectorLength(eorg) > rad)
			continue;

		return from;
	}

	return NULL;
}

/*
 * G_PickTarget
 *
 * Searches all active entities for the next one that holds
 * the matching string at fieldofs(use the FOFS() macro) in the structure.
 *
 * Searches beginning at the edict after from, or the beginning if NULL
 * NULL will be returned if the end of the list is reached.
 *
 */
#define MAXCHOICES	8

g_edict_t *G_PickTarget(char *target_name) {
	g_edict_t *ent = NULL;
	int num_choices = 0;
	g_edict_t *choice[MAXCHOICES];

	if (!target_name) {
		gi.Debug("G_PickTarget called with NULL targetname\n");
		return NULL;
	}

	while (true) {
		ent = G_Find(ent, FOFS(target_name), target_name);
		if (!ent)
			break;
		choice[num_choices++] = ent;
		if (num_choices == MAXCHOICES)
			break;
	}

	if (!num_choices) {
		gi.Debug("G_PickTarget: target %s not found\n", target_name);
		return NULL;
	}

	return choice[rand() % num_choices];
}

/*
 * G_UseTargets_Delay
 */
static void G_UseTargets_Delay(g_edict_t *ent) {
	G_UseTargets(ent, ent->activator);
	G_FreeEdict(ent);
}

/*
 * G_UseTargets
 *
 * Search for all entities that the specified entity targets, and call their
 * use functions. Set their activator to our activator. Print our message,
 * if set, to the activator.
 */
void G_UseTargets(g_edict_t *ent, g_edict_t *activator) {
	g_edict_t *t;

	// check for a delay
	if (ent->delay) {
		// create a temp object to fire at a later time
		t = G_Spawn();
		t->class_name = "DelayedUse";
		t->next_think = g_level.time + ent->delay;
		t->think = G_UseTargets_Delay;
		t->activator = activator;
		if (!activator)
			gi.Debug("G_UseTargets: No activator\n");
		t->message = ent->message;
		t->target = ent->target;
		t->kill_target = ent->kill_target;
		return;
	}

	// print the message
	if ((ent->message) && activator->client) {
		gi.ClientCenterPrint(activator, "%s", ent->message);
		if (ent->noise_index)
			gi.Sound(activator, ent->noise_index, ATTN_NORM);
		else
			gi.Sound(activator, gi.SoundIndex("misc/chat"), ATTN_NORM);
	}

	// kill kill_targets
	if (ent->kill_target) {
		t = NULL;
		while ((t = G_Find(t, FOFS(target_name), ent->kill_target))) {
			G_FreeEdict(t);
			if (!ent->in_use) {
				gi.Debug("entity was removed while using killtargets\n");
				return;
			}
		}
	}

	// fire targets
	if (ent->target) {
		t = NULL;
		while ((t = G_Find(t, FOFS(target_name), ent->target))) {
			// doors fire area portals in a specific way
			if (!strcasecmp(t->class_name, "func_areaportal") && (!strcasecmp(
					ent->class_name, "func_door") || !strcasecmp(
					ent->class_name, "func_door_rotating")))
				continue;

			if (t == ent) {
				gi.Debug("entity asked to use itself\n");
			} else {
				if (t->use)
					t->use(t, ent, activator);
			}
			if (!ent->in_use) {
				gi.Debug("entity was removed while using targets\n");
				return;
			}
		}
	}
}

/*
 * tv
 *
 * This is just a convenience function
 * for making temporary vectors for function calls
 */
float *tv(float x, float y, float z) {
	static int index;
	static vec3_t vecs[8];
	float *v;

	// use an array so that multiple tempvectors won't collide
	// for a while
	v = vecs[index];
	index = (index + 1) & 7;

	v[0] = x;
	v[1] = y;
	v[2] = z;

	return v;
}

/*
 * vtos
 *
 * A convenience function for printing vectors.
 */
char *vtos(vec3_t v) {
	static int index;
	static char str[8][32];
	char *s;

	// use an array so that multiple vtos won't collide
	s = str[index];
	index = (index + 1) & 7;

	snprintf(s, 32, "(%i %i %i)", (int)v[0],(int)v[1],(int)v[2]);

	return s;
}

/*
 * G_SetMoveDir
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
 * G_CopyString
 */
char *G_CopyString(char *in) {
	char *out;

	out = gi.TagMalloc(strlen(in) + 1, TAG_LEVEL);
	strcpy(out, in);
	return out;
}

/*
 * G_InitEdict
 */
void G_InitEdict(g_edict_t *e) {
	e->in_use = true;
	e->class_name = "noclass";
	e->gravity = 1.0;
	e->timestamp = g_level.time;
	e->s.number = e - g_game.edicts;
}

/*
 * G_Spawn
 *
 * Either finds a free edict, or allocates a new one.
 * Try to avoid reusing an entity that was recently freed, because it
 * can cause the client to think the entity morphed into something else
 * instead of being removed and recreated, which can cause interpolated
 * angles and bad trails.
 */
g_edict_t *G_Spawn(void) {
	unsigned int i;
	g_edict_t *e;

	e = &g_game.edicts[sv_max_clients->integer + 1];
	for (i = sv_max_clients->integer + 1; i < ge.num_edicts; i++, e++) {
		if (!e->in_use) {
			G_InitEdict(e);
			return e;
		}
	}

	if (i >= g_max_entities->value)
		gi.Error("G_Spawn: No free edicts.");

	ge.num_edicts++;
	G_InitEdict(e);
	return e;
}

/*
 * G_FreeEdict
 *
 * Marks the edict as free
 */
void G_FreeEdict(g_edict_t *ed) {
	gi.UnlinkEntity(ed); // unlink from world

	if ((ed - g_game.edicts) <= sv_max_clients->integer)
		return;

	memset(ed, 0, sizeof(*ed));
	ed->class_name = "freed";
	ed->in_use = false;
}

/*
 * G_TouchTriggers
 */
void G_TouchTriggers(g_edict_t *ent) {
	int i, num;
	g_edict_t *touch[MAX_EDICTS], *hit;

	num = gi.BoxEdicts(ent->abs_mins, ent->abs_maxs, touch, MAX_EDICTS,
			AREA_TRIGGERS);

	// be careful, it is possible to have an entity in this
	// list removed before we get to it (killtriggered)
	for (i = 0; i < num; i++) {

		hit = touch[i];

		if (!hit->in_use)
			continue;

		if (!hit->touch)
			continue;

		hit->touch(hit, ent, NULL, NULL);
	}
}

/*
 * G_TouchSolids
 *
 * Call after linking a new trigger in during gameplay
 * to force all entities it covers to immediately touch it
 */
void G_TouchSolids(g_edict_t *ent) {
	int i, num;
	g_edict_t *touch[MAX_EDICTS], *hit;

	num = gi.BoxEdicts(ent->abs_mins, ent->abs_maxs, touch, MAX_EDICTS,
			AREA_SOLID);

	// be careful, it is possible to have an entity in this
	// list removed before we get to it(killtriggered)
	for (i = 0; i < num; i++) {
		hit = touch[i];
		if (!hit->in_use)
			continue;
		if (ent->touch)
			ent->touch(hit, ent, NULL, NULL);
		if (!ent->in_use)
			break;
	}
}

/*
 * G_KillBox
 *
 * Kills all entities that would touch the proposed new positioning
 * of ent.  Ent should be unlinked before calling this!
 */
boolean_t G_KillBox(g_edict_t *ent) {
	c_trace_t tr;

	while (true) {
		tr = gi.Trace(ent->s.origin, ent->mins, ent->maxs, ent->s.origin, NULL,
				MASK_PLAYER_SOLID);
		if (!tr.ent)
			break;

		// nail it
		G_Damage(tr.ent, ent, ent, vec3_origin, ent->s.origin, vec3_origin,
				100000, 0, DAMAGE_NO_PROTECTION, MOD_TELEFRAG);

		// if we didn't kill it, fail
		if (tr.ent->solid)
			return false;
	}

	return true; // all clear
}

/*
 * G_GameplayName
 */
char *G_GameplayName(int g) {
	switch (g) {
	case INSTAGIB:
		return "INSTAGIB";
	case ARENA:
		return "ARENA";
	default:
		return "DEATHMATCH";
	}
}

/*
 * G_GameplayByName
 */
int G_GameplayByName(char *c) {

	if (!c || *c == '\0')
		return DEATHMATCH;

	switch (*c) { // hack for numeric matches, atoi wont cut it
	case '0':
		return DEATHMATCH;
	case '1':
		return INSTAGIB;
	case '2':
		return ARENA;
	default:
		break;
	}

	if (strstr(Lowercase(c), "insta"))
		return INSTAGIB;
	if (strstr(Lowercase(c), "arena"))
		return ARENA;
	return DEATHMATCH;
}

/*
 * G_TeamByName
 */
g_team_t *G_TeamByName(char *c) {

	if (!c || !*c)
		return NULL;

	if (!StrColorCmp(g_team_good.name, c))
		return &g_team_good;

	if (!StrColorCmp(g_team_evil.name, c))
		return &g_team_evil;

	return NULL;
}

/*
 * G_TeamForFlag
 */
g_team_t *G_TeamForFlag(g_edict_t *ent) {

	if (!g_level.ctf)
		return NULL;

	if (!ent->item || ent->item->type != ITEM_FLAG)
		return NULL;

	if (!strcmp(ent->class_name, "item_flag_team1"))
		return &g_team_good;

	if (!strcmp(ent->class_name, "item_flag_team2"))
		return &g_team_evil;

	return NULL;
}

/*
 * G_FlagForTeam
 */
g_edict_t *G_FlagForTeam(g_team_t *t) {
	g_edict_t *ent;
	char class[32];
	unsigned int i;

	if (!g_level.ctf)
		return NULL;

	if (t != &g_team_good && t != &g_team_evil)
		return NULL;

	strcpy(class, (t == &g_team_good ? "item_flag_team1" : "item_flag_team2"));

	i = sv_max_clients->integer + 1;
	while (i < ge.num_edicts) {

		ent = &ge.edicts[i++];

		if (!ent->item || ent->item->type != ITEM_FLAG)
			continue;

		// when a carrier is killed, we spawn a new temporary flag
		// where they died.  we are generally not interested in these.
		if (ent->spawn_flags & SF_ITEM_DROPPED)
			continue;

		if (!strcmp(ent->class_name, class))
			return ent;
	}

	return NULL;
}

/*
 * G_EffectForTeam
 */
unsigned int G_EffectForTeam(g_team_t *t) {

	if (!t)
		return 0;

	return (t == &g_team_good ? EF_CTF_BLUE : EF_CTF_RED);
}

/*
 * G_OtherTeam
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
 * G_SmallestTeam
 */
g_team_t *G_SmallestTeam(void) {
	int i, g, e;
	g_client_t *cl;

	g = e = 0;

	for (i = 0; i < sv_max_clients->integer; i++) {
		if (!g_game.edicts[i + 1].in_use)
			continue;

		cl = g_game.edicts[i + 1].client;

		if (cl->persistent.team == &g_team_good)
			g++;
		else if (cl->persistent.team == &g_team_evil)
			e++;
	}

	return g < e ? &g_team_good : &g_team_evil;
}

/*
 * G_ClientByName
 */
g_client_t *G_ClientByName(char *name) {
	int i, j, min;
	g_client_t *cl, *ret;

	if (!name)
		return NULL;

	ret = NULL;
	min = 9999;

	for (i = 0; i < sv_max_clients->integer; i++) {
		if (!g_game.edicts[i + 1].in_use)
			continue;

		cl = g_game.edicts[i + 1].client;
		if ((j = strcmp(name, cl->persistent.net_name)) < min) {
			ret = cl;
			min = j;
		}
	}

	return ret;
}

/*
 * G_IsStationary
 */
boolean_t G_IsStationary(g_edict_t *ent) {

	if (!ent)
		return false;

	return VectorCompare(vec3_origin, ent->velocity);
}

/*
 * G_SetAnimation_
 *
 * Writes the specified animation byte, toggling the high bit to restart the
 * sequence if desired and necessary.
 */
static void G_SetAnimation_(byte *dest, entity_animation_t anim,
		boolean_t restart) {

	if (restart) {
		if (*dest == anim) {
			anim |= ANIM_TOGGLE_BIT;
		}
	}

	*dest = anim;
}

/*
 * G_SetAnimation
 *
 * Assigns the specified animation to the correct member(s) on the specified
 * entity. If requested, the current animation will be restarted.
 */
void G_SetAnimation(g_edict_t *ent, entity_animation_t anim, boolean_t restart) {

	// certain sequences go to both torso and leg animations

	if (anim < ANIM_TORSO_GESTURE) {
		G_SetAnimation_(&ent->s.animation1, anim, restart);
		G_SetAnimation_(&ent->s.animation2, anim, restart);
		return;
	}

	// while most go to one or the other

	if (anim < ANIM_LEGS_WALKCR)
		G_SetAnimation_(&ent->s.animation1, anim, restart);
	else
		G_SetAnimation_(&ent->s.animation2, anim, restart);
}

/*
 * G_IsAnimation
 *
 * Returns true if the entity is currently using the specified animation.
 */
boolean_t G_IsAnimation(g_edict_t *ent, entity_animation_t anim) {
	byte a;

	if (anim < ANIM_LEGS_WALK)
		a = ent->s.animation1;
	else
		a = ent->s.animation2;

	return (a & ~ANIM_TOGGLE_BIT) == anim;
}

