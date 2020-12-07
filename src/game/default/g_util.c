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

#include "g_local.h"
#include "bg_pmove.h"

/**
 * @brief
 */
void G_InitPlayerSpawn(g_entity_t *ent) {
	vec3_t mins, maxs, delta, forward;

	// up
	const float up = ceilf(fabs(PM_SCALE * PM_MINS.z - PM_MINS.z));
	ent->s.origin.z += up;

	// forward, find the old x/y size
	mins = PM_MINS;
	maxs = PM_MAXS;
	mins.z = maxs.z = 0.0;

	delta = Vec3_Subtract(maxs, mins);
	const float len0 = Vec3_Length(delta);

	// and the new x/y size
	delta = Vec3_Scale(delta, PM_SCALE);
	const float len1 = Vec3_Length(delta);

	const float fwd = ceilf(len1 - len0);

	Vec3_Vectors(ent->s.angles, &forward, NULL, NULL);
	ent->s.origin = Vec3_Add(ent->s.origin, Vec3_Scale(forward, fwd));
	
	if (!g_strcmp0(ent->class_name, "info_player_intermission")) {
		G_Ai_DropItemLikeNode(ent);
	}
}

/**
 * @brief Determines the initial position and directional vectors of a projectile.
 */
void G_InitProjectile(const g_entity_t *ent, vec3_t *forward, vec3_t *right, vec3_t *up, vec3_t *org, float hand) {

	// resolve the projectile destination
	const vec3_t start = Vec3_Add(ent->s.origin, ent->client->ps.pm_state.view_offset);
	const vec3_t end = Vec3_Add(start, Vec3_Scale(ent->client->locals.forward, MAX_WORLD_DIST));
	const cm_trace_t tr = gi.Trace(start, end, Vec3_Zero(), Vec3_Zero(), ent, CONTENTS_MASK_CLIP_PROJECTILE);

	// resolve the projectile origin
	vec3_t ent_forward, ent_right, ent_up;
	Vec3_Vectors(ent->client->locals.angles, &ent_forward, &ent_right, &ent_up);

	*org = Vec3_Add(start, Vec3_Scale(ent_forward, 12.0));

	switch (ent->client->locals.persistent.hand) {
		case HAND_RIGHT:
			*org = Vec3_Add(*org, Vec3_Scale(ent_right, 6.0 * hand));
			break;
		case HAND_LEFT:
			*org = Vec3_Add(*org, Vec3_Scale(ent_right, -6.0 * hand));
			break;
		default:
			break;
	}

	if ((ent->client->ps.pm_state.flags & PMF_DUCKED)) {
		*org = Vec3_Add(*org, Vec3_Scale(ent_up, -6.0));
	} else {
		*org = Vec3_Add(*org, Vec3_Scale(ent_up, -12.0));
	}

	// if the projected origin is invalid, use the entity's origin
	if (gi.Trace(*org, *org, Vec3_Zero(), Vec3_Zero(), ent, CONTENTS_MASK_CLIP_PROJECTILE).start_solid) {
		*org = ent->s.origin;
	}

	if (forward) {
		// return the projectile's directional vectors
		*forward = Vec3_Subtract(tr.end, *org);
		*forward = Vec3_Normalize(*forward);

		const vec3_t euler = Vec3_Euler(*forward);
		Vec3_Vectors(euler, NULL, right, up);
	}
}

/**
 * @brief Searches all active entities for the next one that holds the matching string
 * at field offset (use the ELOFS() macro) in the structure.
 *
 * Searches beginning at the entity after from, or the beginning if NULL
 * NULL will be returned if the end of the list is reached.
 *
 * Example:
 *   G_Find(NULL, EOFS(class_name), "info_player_deathmatch");
 *
 */
g_entity_t *G_Find(g_entity_t *from, ptrdiff_t field, const char *match) {
	char *s;

	if (!from) {
		from = g_game.entities;
	} else {
		from++;
	}

	for (; from < &g_game.entities[ge.num_entities]; from++) {
		if (!from->in_use) {
			continue;
		}
		s = *(char **) ((byte *) from + field);
		if (!s) {
			continue;
		}
		if (!g_ascii_strcasecmp(s, match)) {
			return from;
		}
	}

	return NULL;
}

/**
 * @brief Searches all active entities for the next one that holds the matching pointer
 * at field offset (use the ELOFS() macro) in the structure.
 *
 * Searches beginning at the entity after from, or the beginning if NULL
 * NULL will be returned if the end of the list is reached.
 *
 * Example:
 *   G_Find(NULL, LOFS(ptr), 0x1234);
 *
 */
g_entity_t *G_FindPtr(g_entity_t *from, ptrdiff_t field, const void *match) {
	void *s;

	if (!from) {
		from = g_game.entities;
	} else {
		from++;
	}

	for (; from < &g_game.entities[ge.num_entities]; from++) {
		if (!from->in_use) {
			continue;
		}
		s = *(void **) ((byte *) from + field);
		if (!s) {
			continue;
		}
		if (s == match) {
			return from;
		}
	}

	return NULL;
}

/**
 * @brief Returns entities that have origins within a spherical radius.
 */
g_entity_t *G_FindRadius(g_entity_t *from, const vec3_t org, float rad) {

	if (!from) {
		from = g_game.entities;
	} else {
		from++;
	}

	for (; from < &g_game.entities[ge.num_entities]; from++) {

		if (!from->in_use) {
			continue;
		}

		if (from->solid == SOLID_NOT) {
			continue;
		}

		if (Vec3_Distance(org, from->abs_mins) < rad) {
			return from;
		}

		if (Vec3_Distance(org, from->abs_maxs) < rad) {
			return from;
		}

		return from;
	}

	return NULL;
}

#define MAX_TARGETS	8

/**
 * @brief Searches all active entities for the next targeted one.
 *
 * Searches beginning at the entity after from, or the beginning if NULL
 * NULL will be returned if the end of the list is reached.
 */
g_entity_t *G_PickTarget(const char *target_name) {
	g_entity_t *choice[MAX_TARGETS];
	uint32_t num_choices = 0;

	if (!target_name) {
		G_Debug("NULL target_name\n");
		return NULL;
	}

	g_entity_t *ent = NULL;
	while (true) {

		ent = G_Find(ent, LOFS(target_name), target_name);

		if (!ent) {
			break;
		}

		choice[num_choices++] = ent;

		if (num_choices == MAX_TARGETS) {
			break;
		}
	}

	if (!num_choices) {
		G_Debug("Target %s not found\n", target_name);
		return NULL;
	}

	return choice[RandomRangeu(0, num_choices)];
}

/**
 * @brief
 */
static void G_UseTargets_Delay(g_entity_t *ent) {
	G_UseTargets(ent, ent->locals.activator);
	G_FreeEntity(ent);
}

/**
 * @brief Search for all entities that the specified entity targets, and call their
 * use functions. Set their activator to our activator. Print our message,
 * if set, to the activator.
 */
void G_UseTargets(g_entity_t *ent, g_entity_t *activator) {

	// check for a delay
	if (ent->locals.delay) {
		// create a temp entity to fire at a later time
		g_entity_t *temp = G_AllocEntity();
		temp->locals.next_think = g_level.time + ent->locals.delay * 1000;
		temp->locals.Think = G_UseTargets_Delay;
		temp->locals.activator = activator;
		if (!activator) {
			G_Debug("No activator for %s\n", etos(ent));
		}
		temp->locals.message = ent->locals.message;
		temp->locals.target = ent->locals.target;
		temp->locals.kill_target = ent->locals.kill_target;
		return;
	}

	// print the message
	if ((ent->locals.message) && activator->client) {
		gi.WriteByte(SV_CMD_CENTER_PRINT);
		gi.WriteString(ent->locals.message);
		gi.Unicast(activator, true);

		if (ent->locals.sound) {
			gi.Sound(activator, ent->locals.sound, SOUND_ATTEN_LINEAR, 0);
		} else {
			gi.Sound(activator, gi.SoundIndex("misc/chat"), SOUND_ATTEN_LINEAR, 0);
		}
	}

	// kill kill_targets
	if (ent->locals.kill_target) {
		g_entity_t *target = NULL;
		while ((target = G_Find(target, LOFS(target_name), ent->locals.kill_target))) {
			G_FreeEntity(target);
			if (!ent->in_use) {
				G_Debug("%s was removed while using kill_targets\n", etos(ent));
				return;
			}
		}
	}

	// fire targets
	if (ent->locals.target) {
		g_entity_t *target = NULL;
		while ((target = G_Find(target, LOFS(target_name), ent->locals.target))) {

			if (target == ent) {
				G_Debug("%s tried to use itself\n", etos(ent));
				continue;
			}

			if (target->locals.Use) {
				target->locals.Use(target, ent, activator);
				if (!ent->in_use) { // see if our target freed us
					G_Debug("%s was removed while using targets\n", etos(ent));
					break;
				}
			}
		}
	}
}

/**
 * @brief
 */
void G_SetMoveDir(g_entity_t *ent) {

	const vec3_t angles_up = Vec3(0.0, -1.0, 0.0);
	const vec3_t dir_up = Vec3(0.0, 0.0, 1.0 );
	const vec3_t angles_down = Vec3(0.0, -2.0, 0.0);
	const vec3_t dir_down = Vec3(0.0, 0.0, -1.0);

	if (Vec3_Equal(ent->s.angles, angles_up)) {
		ent->locals.move_dir = dir_up;
	} else if (Vec3_Equal(ent->s.angles, angles_down)) {
		ent->locals.move_dir = dir_down;
	} else {
		Vec3_Vectors(ent->s.angles, &ent->locals.move_dir, NULL, NULL);
	}

	ent->s.angles = Vec3_Zero();
}

/**
 * @brief Clear an entity's local data to empty. This function
 * retains any data that the entity client should always keep.
 */
void G_ClearEntity(g_entity_t *ent) {

	g_client_t *client = ent->client;

	memset(ent, 0, sizeof(*ent));

	ent->client = client;
}

/**
 * @brief Initialize an entity. This clears the entity memory, sets up the members
 * that need to be there for entity system to work (number, etc) and marks it as in_use.
 */
void G_InitEntity(g_entity_t *ent, const char *class_name) {
	static uint32_t g_spawn_id;

	G_ClearEntity(ent);

	ent->class_name = class_name;
	ent->in_use = true;

	ent->locals.water_level = WATER_UNKNOWN;
	ent->locals.timestamp = g_level.time;
	ent->s.number = ent - g_game.entities;
	ent->spawn_id = g_spawn_id++;
}

/**
 * @brief Allocates an entity for use.
 */
g_entity_t *G_AllocEntity_(const char *class_name) {
	uint16_t i;

	g_entity_t *e = &g_game.entities[sv_max_clients->integer + 1];
	for (i = sv_max_clients->integer + 1; i < ge.num_entities; i++, e++) {
		if (!e->in_use) {
			G_InitEntity(e, class_name);
			return e;
		}
	}

	if (i >= g_max_entities->value) {
		gi.Error("No free entities for %s\n", class_name);
	}

	ge.num_entities++;
	G_InitEntity(e, class_name);
	return e;
}

/**
 * @brief Frees the specified entity.
 */
void G_FreeEntity(g_entity_t *ent) {

	gi.UnlinkEntity(ent);

	if ((ent - g_game.entities) <= sv_max_clients->integer) {
		return;
	}

	G_ClearEntity(ent);
	ent->class_name = "free";
}

/**
 * @brief Kills all entities that would touch the proposed new positioning of the entity.
 * FIXME gibs randomly, need to fix this
 * @remarks This doesn't work correctly for rotating BSP entities.
 */
void G_KillBox(g_entity_t *ent) {
	g_entity_t *ents[MAX_ENTITIES];

	const vec3_t mins = Vec3_Add(ent->s.origin, ent->mins);
	const vec3_t maxs = Vec3_Add(ent->s.origin, ent->maxs);

	size_t i, len = gi.BoxEntities(mins, maxs, ents, lengthof(ents), BOX_COLLIDE);
	for (i = 0; i < len; i++) {

		if (ents[i] == ent) {
			continue;
		}

		if (G_IsMeat(ents[i])) {

			G_Damage(ents[i], NULL, ent, Vec3_Zero(), ents[i]->s.origin, Vec3_Zero(), 999, 0, DMG_NO_GOD, MOD_TELEFRAG);

			if (ents[i]->in_use && !ents[i]->locals.dead) {
				break;
			}
		} else {
			break;
		}
	}

	if (i < len) {
		if (G_IsMeat(ent)) {
			G_Damage(ent, NULL, ents[i], Vec3_Zero(), ent->s.origin, Vec3_Zero(), 999, 0, DMG_NO_GOD, MOD_ACT_OF_GOD);
		}
	}
}

/**
 * @brief Kills the specified entity via explosion, potentially taking nearby
 * entities with it. Certain pickup items are reset after exploding.
 */
void G_Explode(g_entity_t *ent, int16_t damage, int16_t knockback, float radius, uint32_t mod) {

	gi.WriteByte(SV_CMD_TEMP_ENTITY);
	gi.WriteByte(TE_EXPLOSION);
	gi.WritePosition(ent->s.origin);
	gi.WriteDir(Vec3_Up());
	gi.Multicast(ent->s.origin, MULTICAST_PHS, NULL);

	G_RadiusDamage(ent, ent, NULL, damage, knockback, radius, mod ?: MOD_EXPLOSIVE);

	const g_item_t *item = ent->locals.item;
	if (item) {
		switch (item->type) {
			case ITEM_TECH:
				G_ResetDroppedTech(ent);
				break;
			case ITEM_FLAG:
				G_ResetDroppedFlag(ent);
				break;
			default:
				G_FreeEntity(ent);
				break;
		}
	} else {
		G_FreeEntity(ent);
	}
}

/**
 * @brief Kills the specified entity via gib effect.
 */
void G_Gib(g_entity_t *ent) {

	gi.WriteByte(SV_CMD_TEMP_ENTITY);
	gi.WriteByte(TE_GIB);
	gi.WritePosition(ent->s.origin);
	gi.Multicast(ent->s.origin, MULTICAST_PVS, NULL);

	G_FreeEntity(ent);
}

/**
 * @brief
 */
char *G_GameplayName(int32_t g) {
	switch (g) {
		case GAME_DEATHMATCH:
			return "DM";
		case GAME_INSTAGIB:
			return "Instagib";
		case GAME_ARENA:
			return "Arena";
		case GAME_DUEL:
			return "Duel";
		default:
			return "DM";
	}
}

/**
 * @brief
 */
g_gameplay_t G_GameplayByName(const char *c) {
	g_gameplay_t gameplay = GAME_DEATHMATCH;

	if (!c || *c == '\0') {
		return gameplay;
	}

	char *lower = g_strchug(g_ascii_strdown(c, -1));

	if (g_str_has_prefix(lower, "insta")) {
		gameplay = GAME_INSTAGIB;
	} else if (g_str_has_prefix(lower, "arena")) {
		gameplay = GAME_ARENA;
	} else if (g_str_has_prefix(lower, "duel")) {
		gameplay = GAME_DUEL;
	}

	g_free(lower);
	return gameplay;
}

/**
 * @brief
 */
g_team_t *G_TeamByName(const char *c) {

	if (!c || !*c) {
		return NULL;
	}

	for (int32_t i = 0; i < g_level.num_teams; i++) {

		if (!StrColorCmp(g_teamlist[i].name, c)) {
			return &g_teamlist[i];
		}
	}

	return NULL;
}

/**
 * @brief
 */
g_team_t *G_TeamForFlag(const g_entity_t *ent) {

	if (!g_level.ctf) {
		return NULL;
	}

	if (!ent->locals.item || ent->locals.item->type != ITEM_FLAG) {
		return NULL;
	}

	for (int32_t i = 0; i < g_level.num_teams; i++) {

		if (!g_strcmp0(ent->class_name, g_teamlist[i].flag)) {
			return &g_teamlist[i];
		}
	}

	return NULL;
}

/**
 * @brief
 */
g_entity_t *G_FlagForTeam(const g_team_t *t) {

	if (!g_level.ctf) {
		return NULL;
	}

	return t->flag_entity;
}

/**
 * @brief
 */
uint32_t G_EffectForTeam(const g_team_t *t) {

	if (!t) {
		return 0;
	}

	return EF_CTF_CARRY | t->effect;
}

/**
 * @brief Get the flag a player is holding, or NULL if we're not a flag-bearer.
 */
const g_item_t *G_IsFlagBearer(const g_entity_t *ent) {

	for (int32_t i = 0; i < g_level.num_teams; i++) {

		if (&g_teamlist[i] == ent->client->locals.persistent.team) {
			continue;
		}

		g_entity_t *f = G_FlagForTeam(&g_teamlist[i]);

		if (f && ent->client->locals.inventory[f->locals.item->index]) {
			return f->locals.item;
		}
	}

	return NULL;
}

/*
 *	Return the number of players on the given team.
 */
size_t G_TeamSize(const g_team_t *team) {
	size_t count = 0;

	for (int32_t i = 0; i < sv_max_clients->integer; i++) {
		if (!g_game.entities[i + 1].in_use) {
			continue;
		}

		const g_client_t *cl = g_game.entities[i + 1].client;
		if (cl->locals.persistent.team == team) {
			count++;
		}
	}
	return count;
}

/**
 * @brief
 */
g_team_t *G_SmallestTeam(void) {
	g_client_t *cl;
	uint8_t num_clients[MAX_TEAMS];

	memset(num_clients, 0, sizeof(num_clients));

	for (int32_t i = 0; i < sv_max_clients->integer; i++) {
		if (!g_game.entities[i + 1].in_use) {
			continue;
		}

		cl = g_game.entities[i + 1].client;

		if (!cl->locals.persistent.team) {
			continue;
		}

		num_clients[cl->locals.persistent.team->id]++;
	}

	g_team_t *smallest = NULL;

	for (int32_t i = 0; i < g_level.num_teams; i++) {
		if (!smallest || num_clients[i] < num_clients[smallest->id]) {
			smallest = &g_teamlist[i];
		}
	}

	return smallest;
}


/**
 * @brief
 */
g_entity_t *G_EntityByName(char *name) {
	int32_t i, j, min;
	g_client_t *cl;
	g_entity_t *ret;

	if (!name) {
		return NULL;
	}

	ret = NULL;
	min = 9999;

	for (i = 0; i < sv_max_clients->integer; i++) {
		if (!g_game.entities[i + 1].in_use) {
			continue;
		}

		cl = g_game.entities[i + 1].client;
		if ((j = g_strcmp0(name, cl->locals.persistent.net_name)) < min) {
			ret = &g_game.entities[i + 1];
			min = j;
		}
	}

	return ret;
}


/**
 * @brief
 */
g_client_t *G_ClientByName(char *name) {

	const g_entity_t *ent = G_EntityByName(name);
	return ent->client;
}

/**
 * @return Get the g_hook_style_t this string describes.
 */
g_hook_style_t G_HookStyleByName(const char *s) {

	if (!g_strcmp0(s, "swing")) {
		return HOOK_SWING;
	}

	return HOOK_PULL;
}

/**
 * @return True if the specified entity should bleed when damaged.
 */
_Bool G_IsMeat(const g_entity_t *ent) {

	if (!ent || !ent->in_use) {
		return false;
	}

	if (ent->solid == SOLID_BOX && ent->client) {
		return true;
	}

	if (ent->solid == SOLID_DEAD) {
		return true;
	}

	return false;
}

/**
 * @return True if the specified entity is likely stationary.
 */
_Bool G_IsStationary(const g_entity_t *ent) {

	if (!ent || !ent->in_use) {
		return false;
	}

	if (ent->locals.move_type) {
		return false;
	}

	if (!Vec3_Equal(Vec3_Zero(), ent->locals.velocity)) {
		return false;
	}

	if (!Vec3_Equal(Vec3_Zero(), ent->locals.avelocity)) {
		return false;
	}

	return true;
}

/**
 * @return True if the specified entity and surface are structural.
 */
_Bool G_IsStructural(const g_entity_t *ent, const cm_bsp_texinfo_t *texinfo) {

	if (ent) {
		if (ent->solid == SOLID_BSP) {

			if (!texinfo || (texinfo->flags & SURF_SKY)) {
				return false;
			}

			return true;
		}
	}

	return false;
}

/**
 * @return True if the specified entity and surface are sky.
 */
_Bool G_IsSky(const cm_bsp_texinfo_t *texinfo) {

	if (texinfo && (texinfo->flags & SURF_SKY)) {
		return true;
	}

	return false;
}

/**
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

/**
 * @brief Assigns the specified animation to the correct member(s) on the specified
 * entity. If requested, the current animation will be restarted.
 */
void G_SetAnimation(g_entity_t *ent, entity_animation_t anim, _Bool restart) {

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

/**
 * @brief Returns true if the entity is currently using the specified animation.
 */
_Bool G_IsAnimation(g_entity_t *ent, entity_animation_t anim) {
	byte a;

	if (anim < ANIM_LEGS_WALK) {
		a = ent->s.animation1;
	} else {
		a = ent->s.animation2;
	}

	return (a & ANIM_MASK_VALUE) == anim;
}

/**
 * @brief forcefully suggest client adds given command to its console buffer
 */
void G_ClientStuff(const g_entity_t *ent, const char *s) {
	gi.WriteByte(SV_CMD_CBUF_TEXT);
	gi.WriteString(s);
	gi.Unicast(ent, true);
}

/**
 * @brief Send a centerprint to everyone on the supplied team
 */
void G_TeamCenterPrint(g_team_t *team, const char *fmt, ...) {
	char string[MAX_STRING_CHARS];
	va_list args;
	const g_entity_t *ent;

	va_start(args, fmt);
	vsprintf(string, fmt, args);
	va_end(args);

	// look through all players
	for (int32_t i = 0; i < sv_max_clients->integer; i++) {
		if (!g_game.entities[i + 1].in_use) {
			continue;
		}

		ent = &g_game.entities[i + 1];

		// member of supplied team? send it
		if (ent->client->locals.persistent.team == team) {
			gi.WriteByte(SV_CMD_CENTER_PRINT);
			gi.WriteString(string);
			gi.Unicast(ent, false);
		}
	}
}
