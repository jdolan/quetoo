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
#include "parse.h"

typedef struct {
	char *name;
	void (*Spawn)(g_entity_t *ent);
} g_entity_spawn_t;

static void G_worldspawn(g_entity_t *ent);

/**
 * @brief Actually does the magic
 */
static void G_weapon_chaingun_Think(g_entity_t *ent) {

	g_entity_t *cg = NULL;
	
	while ((cg = G_Find(cg, EOFS(class_name), "weapon_chaingun"))) {

		// spawn a lightning gun where we are
		g_entity_t *lg = G_AllocEntity_(g_media.items.weapons[WEAPON_LIGHTNING]->class_name);
		VectorCopy(cg->s.origin, lg->s.origin);
		VectorCopy(cg->s.angles, lg->s.angles);
		lg->locals.spawn_flags = cg->locals.spawn_flags;

		G_SpawnItem(lg, g_media.items.weapons[WEAPON_LIGHTNING]);

		// replace nearby bullets with bolts
		g_entity_t *ammo = NULL;

		while ((ammo = G_FindRadius(ammo, lg->s.origin, 128.0))) {
			if (ammo->locals.item && ammo->locals.item == g_media.items.ammo[AMMO_BULLETS]) {

				// hello bolts
				g_entity_t *bolts = G_AllocEntity_(g_media.items.ammo[AMMO_BOLTS]->class_name);
				VectorCopy(ammo->s.origin, bolts->s.origin);
				VectorCopy(ammo->s.angles, bolts->s.angles);
				bolts->locals.spawn_flags = ammo->locals.spawn_flags;

				G_SpawnItem(bolts, g_media.items.ammo[AMMO_BOLTS]);

				// byebye bullets
				G_FreeEntity(ammo);
			}
		}

		// byebye chaingun
		G_FreeEntity(cg);
	}
}

/**
 * @brief Support function to allow chaingun maps to work nicely
 */
static void G_weapon_chaingun(g_entity_t *ent) {

	// see if we already have one ready, this is just to keep this BS self-contained
	g_entity_t *cg = NULL;
	
	while ((cg = G_Find(cg, EOFS(class_name), "weapon_chaingun"))) {
		if (cg->locals.Think) {
			return; // think will destroy us later
		}
	}

	ent->locals.Think = G_weapon_chaingun_Think;
	ent->locals.next_think = g_level.time + 1; // do it after spawnentities
	ent->locals.move_type = MOVE_TYPE_THINK;
}

static g_entity_spawn_t g_entity_spawns[] = { // entity class names -> spawn functions
	{ "func_areaportal", G_func_areaportal },
	{ "func_button", G_func_button },
	{ "func_conveyor", G_func_conveyor },
	{ "func_door", G_func_door },
	{ "func_door_rotating", G_func_door_rotating },
	{ "func_door_secret", G_func_door_secret },
	{ "func_plat", G_func_plat },
	{ "func_rotating", G_func_rotating },
	{ "func_timer", G_func_timer },
	{ "func_train", G_func_train },
	{ "func_wall", G_func_wall },
	{ "func_water", G_func_water },

	{ "info_notnull", G_info_notnull },
	{ "info_player_intermission", G_info_player_intermission },
	{ "info_player_deathmatch", G_info_player_deathmatch },
	{ "info_player_start", G_info_player_start },
	{ "info_player_team1", G_info_player_team1 },
	{ "info_player_team2", G_info_player_team2 },
	{ "info_player_team3", G_info_player_team3 },
	{ "info_player_team4", G_info_player_team4 },
	{ "info_player_team_any", G_info_player_team_any },

	{ "misc_teleporter", G_misc_teleporter },
	{ "misc_teleporter_dest", G_misc_teleporter_dest },
	{ "misc_fireball", G_misc_fireball },

	{ "path_corner", G_info_notnull },

	{ "target_light", G_target_light },
	{ "target_speaker", G_target_speaker },
	{ "target_string", G_target_string },

	{ "trigger_always", G_trigger_always },
	{ "trigger_exec", G_trigger_exec },
	{ "trigger_hurt", G_trigger_hurt },
	{ "trigger_multiple", G_trigger_multiple },
	{ "trigger_once", G_trigger_once },
	{ "trigger_push", G_trigger_push },
	{ "trigger_relay", G_trigger_relay },
	{ "trigger_teleporter", G_misc_teleporter },

	{ "worldspawn", G_worldspawn },

	// compatibility-only entities
	{ "weapon_chaingun", G_weapon_chaingun },

	// lastly, these are entities which we intentionally suppress

	{ "func_group", G_FreeEntity },
	{ "info_null", G_FreeEntity },
	{ "light", G_FreeEntity },
	{ "light_spot", G_FreeEntity },
	{ "misc_emit", G_FreeEntity },
	{ "misc_model", G_FreeEntity },

	{ NULL, NULL }
};

/**
 * @brief Finds the spawn function for the entity and calls it.
 */
static _Bool G_SpawnEntity(g_entity_t *ent) {

	if (!ent->class_name) {
		gi.Debug("NULL classname\n");
		return false;
	}

	// check item spawn functions
	for (int32_t i = 0; i < g_num_items; i++) {

		const g_item_t *item = G_ItemByIndex(i);

		if (!item->class_name) {
			continue;
		}

		if (!g_strcmp0(item->class_name, ent->class_name)) { // found it
			G_SpawnItem(ent, item);
			return true;
		}
	}

	// check normal spawn functions
	for (g_entity_spawn_t *s = g_entity_spawns; s->name; s++) {
		if (!g_strcmp0(s->name, ent->class_name)) { // found it
			s->Spawn(ent);
			return true;
		}
	}

	gi.Warn("%s doesn't have a spawn function\n", ent->class_name);
	return false;
}

/**
 * @brief
 */
static char *G_NewString(const char *string) {
	char *newb, *new_p;
	size_t i, l;

	l = strlen(string) + 1;

	newb = gi.Malloc(l, MEM_TAG_GAME_LEVEL);

	new_p = newb;

	for (i = 0; i < l; i++) {
		if (string[i] == '\\' && i < l - 1) {
			i++;
			if (string[i] == 'n') {
				*new_p++ = '\n';
			} else {
				*new_p++ = '\\';
			}
		} else {
			*new_p++ = string[i];
		}
	}

	return newb;
}

// fields are needed for spawning from the entity string
#define FFL_SPAWN_TEMP		1
#define FFL_NO_SPAWN		2

typedef enum g_field_type_s {
	F_SHORT,
	F_INT,
	F_FLOAT,
	F_STRING, // string on disk, pointer in memory, TAG_LEVEL
	F_VECTOR,
	F_ANGLE
} g_field_type_t;

typedef struct g_field_s {
	char *name;
	ptrdiff_t ofs;
	g_field_type_t type;
	int32_t flags;
} g_field_t;

static const g_field_t fields[] = {
	{ "classname", EOFS(class_name), F_STRING, 0 },
	{ "model", EOFS(model), F_STRING, 0 },
	{ "spawnflags", LOFS(spawn_flags), F_INT, 0 },
	{ "speed", LOFS(speed), F_FLOAT, 0 },
	{ "accel", LOFS(accel), F_FLOAT, 0 },
	{ "decel", LOFS(decel), F_FLOAT, 0 },
	{ "target", LOFS(target), F_STRING, 0 },
	{ "targetname", LOFS(target_name), F_STRING, 0 },
	{ "pathtarget", LOFS(path_target), F_STRING, 0 },
	{ "killtarget", LOFS(kill_target), F_STRING, 0 },
	{ "message", LOFS(message), F_STRING, 0 },
	{ "team", LOFS(team), F_STRING, 0 },
	{ "command", LOFS(command), F_STRING, 0 },
	{ "script", LOFS(script), F_STRING, 0 },
	{ "wait", LOFS(wait), F_FLOAT, 0 },
	{ "delay", LOFS(delay), F_FLOAT, 0 },
	{ "random", LOFS(random), F_FLOAT, 0 },
	{ "style", LOFS(area_portal), F_INT, 0 }, // HACK, this was always overloaded
	{ "areaportal", LOFS(area_portal), F_INT, 0 },
	{ "count", LOFS(count), F_INT, 0 },
	{ "health", LOFS(health), F_SHORT, 0 },
	{ "dmg", LOFS(damage), F_SHORT, 0 },
	{ "mass", LOFS(mass), F_FLOAT, 0 },
	{ "attenuation", LOFS(attenuation), F_SHORT, 0 },
	{ "origin", EOFS(s.origin), F_VECTOR, 0 },
	{ "angles", EOFS(s.angles), F_VECTOR, 0 },
	{ "angle", EOFS(s.angles), F_ANGLE, 0 },

	// temp spawn vars -- only valid when the spawn function is called
	{ "lip", SOFS(lip), F_INT, FFL_SPAWN_TEMP },
	{ "distance", SOFS(distance), F_INT, FFL_SPAWN_TEMP },
	{ "height", SOFS(height), F_INT, FFL_SPAWN_TEMP },
	{ "sounds", SOFS(sounds), F_INT, FFL_SPAWN_TEMP },
	{ "noise", SOFS(noise), F_STRING, FFL_SPAWN_TEMP },
	{ "item", SOFS(item), F_STRING, FFL_SPAWN_TEMP },
	{ "colors", SOFS(colors), F_STRING, FFL_SPAWN_TEMP },

	// world vars, we use strings to differentiate between 0 and unset
	{ "sky", SOFS(sky), F_STRING, FFL_SPAWN_TEMP },
	{ "weather", SOFS(weather), F_STRING, FFL_SPAWN_TEMP },
	{ "gravity", SOFS(gravity), F_STRING, FFL_SPAWN_TEMP },
	{ "gameplay", SOFS(gameplay), F_STRING, FFL_SPAWN_TEMP },
	{ "hook", SOFS(hook), F_STRING, FFL_SPAWN_TEMP },
	{ "teams", SOFS(teams), F_STRING, FFL_SPAWN_TEMP },
	{ "num_teams", SOFS(num_teams), F_STRING, FFL_SPAWN_TEMP },
	{ "ctf", SOFS(ctf), F_STRING, FFL_SPAWN_TEMP },
	{ "match", SOFS(match), F_STRING, FFL_SPAWN_TEMP },
	{ "frag_limit", SOFS(frag_limit), F_STRING, FFL_SPAWN_TEMP },
	{ "round_limit", SOFS(round_limit), F_STRING, FFL_SPAWN_TEMP },
	{ "capture_limit", SOFS(capture_limit), F_STRING, FFL_SPAWN_TEMP },
	{ "time_limit", SOFS(time_limit), F_STRING, FFL_SPAWN_TEMP },
	{ "give", SOFS(give), F_STRING, FFL_SPAWN_TEMP },

	{ 0, 0, 0, 0 }
};

/**
 * @brief Takes a key-value pair and sets the binary values in an entity.
 */
static void G_ParseField(const char *key, const char *value, g_entity_t *ent) {
	const g_field_t *f;
	byte *b;
	vec_t v;
	vec3_t vec;

	for (f = fields; f->name; f++) {

		if (!(f->flags & FFL_NO_SPAWN) && !g_ascii_strcasecmp(f->name, key)) { // found it

			if (f->flags & FFL_SPAWN_TEMP) {
				b = (byte *) &g_game.spawn;
			} else {
				b = (byte *) ent;
			}

			switch (f->type) {
				case F_SHORT:
					*(int16_t *) (b + f->ofs) = (int16_t) atoi(value);
					break;
				case F_INT:
					*(int32_t *) (b + f->ofs) = atoi(value);
					break;
				case F_FLOAT:
					*(vec_t *) (b + f->ofs) = atof(value);
					break;
				case F_STRING:
					*(char **) (b + f->ofs) = G_NewString(value);
					break;
				case F_VECTOR:
					sscanf(value, "%f %f %f", &vec[0], &vec[1], &vec[2]);
					((vec_t *) (b + f->ofs))[0] = vec[0];
					((vec_t *) (b + f->ofs))[1] = vec[1];
					((vec_t *) (b + f->ofs))[2] = vec[2];
					break;
				case F_ANGLE:
					v = atof(value);
					((vec_t *) (b + f->ofs))[0] = 0;
					((vec_t *) (b + f->ofs))[1] = v;
					((vec_t *) (b + f->ofs))[2] = 0;
					break;
				default:
					break;
			}
			return;
		}
	}

	//gi.Debug("%s is not a field\n", key);
}

/**
 * @brief Parses an entity out of the given string. 
 * The entity should be a properly initialized free entity.
 */
static void G_ParseEntity(parser_t *parser, g_entity_t *ent) {
	char key[MAX_BSP_ENTITY_KEY], value[MAX_BSP_ENTITY_VALUE];

	_Bool init = false;
	memset(&g_game.spawn, 0, sizeof(g_game.spawn));

	// go through all the dictionary pairs
	while (true) {

		// parse key
		if (!Parse_Token(parser, PARSE_DEFAULT, key, sizeof(key))) {
			gi.Error("EOF without closing brace\n");
		}

		if (key[0] == '}') {
			break;
		}

		// parse value
		if (!Parse_Token(parser, PARSE_DEFAULT | PARSE_NO_WRAP, value, sizeof(value))) {
			gi.Error("EOF in entity definition\n");
		}

		if (value[0] == '}') {
			gi.Error("No entity definition\n");
		}

		init = true;

		// keys with a leading underscore are used for utility comments,
		// and are immediately discarded by quake
		if (key[0] == '_') {
			continue;
		}

		G_ParseField(key, value, ent);
	}

	if (!init) {
		G_ClearEntity(ent);
	}
}

/**
 * @brief Chain together all entities with a matching team field.
 *
 * All but the first will have the FL_TEAM_SLAVE flag set.
 * All but the last will have the team_chain field set to the next one.
 */
static void G_InitEntityTeams(void) {
	g_entity_t *e, *e2;
	uint16_t i, j;

	uint16_t teams = 0, team_entities = 0;
	for (i = 1, e = g_game.entities + i; i < ge.num_entities; i++, e++) {

		if (!e->in_use) {
			continue;
		}

		if (!e->locals.team) {
			continue;
		}

		if (e->locals.flags & FL_TEAM_SLAVE) {
			continue;
		}

		g_entity_t *chain = e;
		e->locals.team_master = e;

		teams++;
		team_entities++;

		for (j = i + 1, e2 = e + 1; j < ge.num_entities; j++, e2++) {

			if (!e2->in_use) {
				continue;
			}

			if (!e2->locals.team) {
				continue;
			}

			if (e2->locals.flags & FL_TEAM_SLAVE) {
				continue;
			}

			if (!g_strcmp0(e->locals.team, e2->locals.team)) {

				e2->locals.team_master = e;
				e2->locals.flags |= FL_TEAM_SLAVE;

				chain->locals.team_chain = e2;
				chain = e2;

				team_entities++;
			}
		}
	}

	gi.Debug("%i teams with %i entities\n", teams, team_entities);
}

/**
 * @brief Resolves references to frequently accessed media.
 */
static void G_InitMedia(void) {
	uint16_t i;

	memset(&g_media, 0, sizeof(g_media));

	// preload items/set item media ptrs
	G_InitItems();

	// precache sexed sounds; clients will load these when a new player model
	// gets loaded.
	gi.SoundIndex("*gurp_1");
	gi.SoundIndex("*drown_1");
	gi.SoundIndex("*fall_1");
	gi.SoundIndex("*fall_2");
	gi.SoundIndex("*land_1");
	gi.SoundIndex("*jump_1");
	gi.SoundIndex("*jump_2");
	gi.SoundIndex("*jump_3");
	gi.SoundIndex("*jump_4");
	gi.SoundIndex("*jump_5");
	gi.SoundIndex("*sizzle_1");
	gi.SoundIndex("*death_1");
	gi.SoundIndex("*gasp_1");
	gi.SoundIndex("*pain25_1");
	gi.SoundIndex("*pain50_1");
	gi.SoundIndex("*pain75_1");
	gi.SoundIndex("*pain100_1");

	g_media.models.grenade = gi.ModelIndex("models/objects/grenade/tris");
	g_media.models.rocket = gi.ModelIndex("models/objects/rocket/tris");
	g_media.models.hook = gi.ModelIndex("models/objects/grapplehook/tris");
	g_media.models.fireball = gi.ModelIndex("models/objects/fireball/tris");

	g_media.sounds.bfg_hit = gi.SoundIndex("weapons/bfg/hit");
	g_media.sounds.bfg_prime = gi.SoundIndex("weapons/bfg/prime");
	g_media.sounds.grenade_hit = gi.SoundIndex("objects/grenade/hit");
	g_media.sounds.grenade_throw = gi.SoundIndex("weapons/handgrenades/hg_throw");
	g_media.sounds.rocket_fly = gi.SoundIndex("objects/rocket/fly");
	g_media.sounds.lightning_fly = gi.SoundIndex("weapons/lightning/fly");
	g_media.sounds.quad_attack = gi.SoundIndex("quad/attack");
	g_media.sounds.quad_expire = gi.SoundIndex("quad/expire");

	g_media.sounds.hook_fire = gi.SoundIndex("objects/hook/fire");
	g_media.sounds.hook_fly = gi.SoundIndex("objects/hook/fly");
	g_media.sounds.hook_hit = gi.SoundIndex("objects/hook/hit");
	g_media.sounds.hook_pull = gi.SoundIndex("objects/hook/pull");
	g_media.sounds.hook_detach = gi.SoundIndex("objects/hook/detach");
	g_media.sounds.hook_gibhit = gi.SoundIndex("objects/hook/gibhit");

	g_media.sounds.teleport = gi.SoundIndex("world/teleport");

	g_media.sounds.water_in = gi.SoundIndex("world/water_in");
	g_media.sounds.water_out = gi.SoundIndex("world/water_out");

	g_media.sounds.weapon_no_ammo = gi.SoundIndex("weapons/common/no_ammo");
	g_media.sounds.weapon_switch = gi.SoundIndex("weapons/common/switch");

	for (i = 0; i < NUM_GIB_MODELS; i++) {
		g_media.models.gibs[i] = gi.ModelIndex(va("models/gibs/gib_%i/tris", i + 1));
	}

	for (i = 0; i < NUM_GIB_SOUNDS; i++) {
		g_media.sounds.gib_hits[i] = gi.SoundIndex(va("gibs/gib_%i/hit", i + 1));
	}

	for (i = 1; i < lengthof(g_media.sounds.countdown); i++) {
		g_media.sounds.countdown[i] = gi.SoundIndex(va("world/countdown_%d", i));
	}

	g_media.sounds.roar = gi.SoundIndex("world/ominous_bwah");

	g_media.sounds.techs[TECH_HASTE] = gi.SoundIndex("tech/haste");
	g_media.sounds.techs[TECH_REGEN] = gi.SoundIndex("tech/regen");
	g_media.sounds.techs[TECH_RESIST] = gi.SoundIndex("tech/resist");
	g_media.sounds.techs[TECH_STRENGTH] = gi.SoundIndex("tech/strength");
	g_media.sounds.techs[TECH_VAMPIRE] = gi.SoundIndex("tech/vampire");

	g_media.images.health = gi.ImageIndex("pics/i_health");
}

/**
 * @brief Sorts the spawns so that the furthest from the flag are at the beginning.
 */
static int32_t G_CreateTeamSpawnPoints_CompareFunc(gconstpointer a, gconstpointer b, gpointer user_data) {

	g_entity_t *flag = (g_entity_t *) user_data;
	
	const g_entity_t *ap = (const g_entity_t *) a;
	const g_entity_t *bp = (const g_entity_t *) b;

	return Sign(VectorDistanceSquared(flag->s.origin, bp->s.origin) - VectorDistanceSquared(flag->s.origin, ap->s.origin));
}

/**
 * @brief Creates team spawns if the map doesn't have one. Also creates flags, although
 * chances are they will be in crap positions.
 */
static void G_CreateTeamSpawnPoints(GSList **dm_spawns, GSList **team_red_spawns, GSList **team_blue_spawns) {

	// find our flags
	g_entity_t *red_flag, *blue_flag;
	g_entity_t *reused_spawns[2] = { NULL, NULL };
	
	red_flag = g_team_red->flag_entity;
	blue_flag = g_team_blue->flag_entity;

	if (!!red_flag != !!blue_flag) {
		gi.Error("Make sure you have both flags in your map!\n");
	} else if (!red_flag) {
		// no flag in map, so let's make one by repurposing the furthest spawn points

		if (g_slist_length(*dm_spawns) < 4) {
			return; // not enough points to make a flag
		}

		vec_t furthest_dist = 0;

		for (GSList *pa = *dm_spawns; pa; pa = pa->next) {
			for (GSList *pb = *dm_spawns; pb; pb = pb->next) {

				if (pa == pb) {
					continue;
				}
				
				g_entity_t *pae = (g_entity_t *) pa->data;
				g_entity_t *pab = (g_entity_t *) pb->data;

				if ((reused_spawns[0] == pae && reused_spawns[1] == pab) ||
					(reused_spawns[0] == pab && reused_spawns[1] == pae)) {
					continue;
				}

				vec3_t line;
				VectorSubtract(pae->s.origin, pab->s.origin, line);
				line[2] /= 10.0; // don't consider Z as heavily as X/Y

				const vec_t dist = VectorLengthSquared(line);

				if (dist > furthest_dist) {

					reused_spawns[0] = pae;
					reused_spawns[1] = pab;
					furthest_dist = dist;
				}
			}
		}

		if (!reused_spawns[0] || !reused_spawns[1] || !furthest_dist) {
			return; // error in finding furthest points
		}
		
		red_flag = G_AllocEntity_(g_team_red->flag);
		blue_flag = G_AllocEntity_(g_team_blue->flag);

		const uint8_t r = Randomr(0, 2);

		VectorCopy(reused_spawns[r]->s.origin, red_flag->s.origin);
		VectorCopy(reused_spawns[r ^ 1]->s.origin, blue_flag->s.origin);
		
		G_SpawnItem(red_flag, g_media.items.flags[TEAM_RED]);
		G_SpawnItem(blue_flag, g_media.items.flags[TEAM_BLUE]);
		
		g_team_red->flag_entity = red_flag;
		g_team_blue->flag_entity = blue_flag;
	}

	for (GSList *point = *dm_spawns; point; point = point->next) {

		g_entity_t *p = (g_entity_t *) point->data;

		if (p == reused_spawns[0] || p == reused_spawns[1]) {
			continue;
		}

		const vec_t dist_to_red = VectorDistanceSquared(red_flag->s.origin, p->s.origin);
		const vec_t dist_to_blue = VectorDistanceSquared(blue_flag->s.origin, p->s.origin);

		if (dist_to_red < dist_to_blue) {
			*team_red_spawns = g_slist_prepend(*team_red_spawns, p);
		} else {
			*team_blue_spawns = g_slist_prepend(*team_blue_spawns, p);
		}
	}

	if (g_slist_length(*team_red_spawns) == g_slist_length(*team_blue_spawns)) {
		return; // best case scenario
	}
	
	// unmatched spawns, we need to move some
	*team_red_spawns = g_slist_sort_with_data(*team_red_spawns, G_CreateTeamSpawnPoints_CompareFunc, red_flag);
	*team_blue_spawns = g_slist_sort_with_data(*team_blue_spawns, G_CreateTeamSpawnPoints_CompareFunc, blue_flag);
		
	int32_t num_red_spawns = (int32_t) g_slist_length(*team_red_spawns);
	int32_t num_blue_spawns = (int32_t) g_slist_length(*team_blue_spawns);
	int32_t diff = abs(num_red_spawns - num_blue_spawns);

	GSList **from, **to;

	if (num_red_spawns > num_blue_spawns) {
		from = team_red_spawns;
		to = team_blue_spawns;
	} else {
		from = team_blue_spawns;
		to = team_red_spawns;
	}

	// odd number of points, make one neutral
	if (diff & 1) {
		g_entity_t *point = (g_entity_t *) ((*from)->data);

		*to = g_slist_prepend(*to, point);
	}

	int32_t num_move = diff - 1;
	
	// move spawns to the other team
	while (num_move) {

		g_entity_t *point = (g_entity_t *) ((*from)->data);

		*from = g_slist_remove(*from, point);
		*to = g_slist_prepend(*to, point);

		num_move--;
	}
}

/**
 * @brief Set up the list of spawn points.
 */
static void G_InitSpawnPoints(void) {

	// first, set up all of the deathmatch points
	GSList *dm_spawns = NULL;
	g_entity_t *spot = NULL;
	
	while ((spot = G_Find(spot, EOFS(class_name), "info_player_deathmatch")) != NULL) {
		dm_spawns = g_slist_prepend(dm_spawns, spot);
	}

	spot = NULL;

	// for legacy maps
	if (!g_slist_length(dm_spawns)) {

		while ((spot = G_Find(spot, EOFS(class_name), "info_player_start")) != NULL) {
			dm_spawns = g_slist_prepend(dm_spawns, spot);
		}
	}
	
	// find the team points, if we have any explicit ones in the map.
	// start by finding the flags
	for (int32_t t = 0; t < MAX_TEAMS; t++) {
		g_teamlist[t].flag_entity = G_Find(NULL, EOFS(class_name), g_teamlist[t].flag);
	}

	GSList *team_spawns[MAX_TEAMS];

	memset(team_spawns, 0, sizeof(team_spawns));

	spot = NULL;

	while ((spot = G_Find(spot, EOFS(class_name), "info_player_team_any")) != NULL) {
		for (int32_t t = 0; t < MAX_TEAMS; t++) {
			team_spawns[t] = g_slist_prepend(team_spawns[t], spot);
		}
	}

	for (int32_t t = 0; t < MAX_TEAMS; t++) {
		spot = NULL;
		g_team_t *team = &g_teamlist[t];

		while ((spot = G_Find(spot, EOFS(class_name), team->spawn)) != NULL) {
			team_spawns[t] = g_slist_prepend(team_spawns[t], spot);
		}

		team->spawn_points.count = g_slist_length(team_spawns[t]);
	}

	// only one team
	if (!!g_team_red->spawn_points.count != !!g_team_blue->spawn_points.count) {
		gi.Error("Map has spawns for only a single team. Use info_player_deathmatch for these!\n");
	}

	g_level.spawn_points.count = g_slist_length(dm_spawns);

	GSList *point = NULL;

	// in the odd case that the map only has team spawns, we'll use them
	if (!g_level.spawn_points.count) {
		for (int32_t t = 0; t < MAX_TEAMS; t++) {
			for (point = team_spawns[t]; point; point = point->next) {
				dm_spawns = g_slist_prepend(dm_spawns, (g_entity_t *) point->data);
			}
		}
		
		g_level.spawn_points.count = g_slist_length(dm_spawns);

		if (!g_level.spawn_points.count) {
			gi.Error("Map has no spawn points! You need some info_player_deathmatch's (or info_player_team1/2/3/4/_any for teamplay maps).\n");
		}
	}

	// if we have team spawns, copy them over
	if (!g_team_red->spawn_points.count) {

		// none in the map, let's make some!
		G_CreateTeamSpawnPoints(&dm_spawns, &team_spawns[TEAM_RED], &team_spawns[TEAM_BLUE]);

		// re-calculate final values since the above may change them
		for (int32_t t = 0; t < MAX_TEAMS; t++) {
			g_teamlist[t].spawn_points.count = g_slist_length(team_spawns[t]);
		}

		g_level.spawn_points.count = g_slist_length(dm_spawns);
	}

	// copy all the data in!
	size_t i;

	for (int32_t t = 0; t < MAX_TEAMS; t++) {
		g_teamlist[t].spawn_points.spots = gi.Malloc(sizeof(g_entity_t *) * g_teamlist[t].spawn_points.count, MEM_TAG_GAME_LEVEL);
	
		for (i = 0, point = team_spawns[t]; point; point = point->next, i++) {
			g_teamlist[t].spawn_points.spots[i] = (g_entity_t *) point->data;
		}
	
		g_slist_free(team_spawns[t]);
	}
	
	g_level.spawn_points.spots = gi.Malloc(sizeof(g_entity_t *) * g_level.spawn_points.count, MEM_TAG_GAME_LEVEL);

	for (i = 0, point = dm_spawns; point; point = point->next, i++) {
		g_level.spawn_points.spots[i] = (g_entity_t *) point->data;
	}

	g_slist_free(dm_spawns);

	G_InitNumTeams();
}

/**
 * @brief
 */
void G_SpawnTech(const g_item_t *item) {

	g_entity_t *spawn = G_SelectTechSpawnPoint();
	g_entity_t *ent = G_DropItem(spawn, item);

	VectorSet(ent->locals.velocity, Randomc() * 250, Randomc() * 250, 200 + (Randomf() * 200));
}

/**
 * @brief Spawn all of the techs
 */
void G_SpawnTechs(void) {

	if (!g_level.techs) {
		return;
	}

	for (g_tech_t i = 0; i < TECH_TOTAL; i++) {
		G_SpawnTech(g_media.items.techs[i]);
	}
}

/**
 * @brief Creates a server's entity / program execution context by
 * parsing textual entity definitions out of an ent file.
 */
void G_SpawnEntities(const char *name, const char *entities) {

	gi.FreeTag(MEM_TAG_GAME_LEVEL);

	memset(&g_level, 0, sizeof(g_level));

	g_strlcpy(g_level.name, name, sizeof(g_level.name));

	// see if we have bots to keep
	if (aix) {
		g_game.ai_fill_slots = 0;
		g_game.ai_left_to_spawn = 0;

		if (g_ai_max_clients->integer) {
			if (g_ai_max_clients->integer == -1) {
				g_game.ai_fill_slots = sv_max_clients->integer;
			} else {
				g_game.ai_fill_slots = Clamp(g_ai_max_clients->integer, 0, sv_max_clients->integer);
			}

			g_game.ai_left_to_spawn = g_game.ai_fill_slots;
			g_ai_max_clients->modified = false;
		} else {
			for (int32_t i = 0; i < sv_max_clients->integer; i++) {
				if (g_game.entities[i + 1].client && g_game.entities[i + 1].client->ai) {
					g_game.ai_left_to_spawn++;
				}
			}
		}
	}
	
	memset(g_game.entities, 0, g_max_entities->value * sizeof(g_entity_t));
	memset(g_game.clients, 0, sv_max_clients->value * sizeof(g_client_t));

	for (int32_t i = 0; i < sv_max_clients->integer; i++) {
		g_game.entities[i + 1].client = g_game.clients + i;
	}

	ge.num_entities = sv_max_clients->integer + 1;

	g_entity_t *ent = NULL;

	gchar **inhibit = g_strsplit(g_inhibit->string, " ", -1);
	int32_t inhibited = 0;
	
	parser_t parser;
	char tok[MAX_QPATH];
	Parse_Init(&parser, entities, PARSER_NO_COMMENTS);

	// parse the entity definition string
	while (true) {

		if (!Parse_Token(&parser, PARSE_DEFAULT, tok, sizeof(tok))) {
			break;
		}

		if (tok[0] != '{') {
			gi.Error("Found \"%s\" when expecting \"{\"", tok);
		}

		if (ent == NULL) {
			ent = g_game.entities;
		} else {
			ent = G_AllocEntity();
		}

		G_ParseEntity(&parser, ent);

		if (ent != g_game.entities) {

			// enforce entity inhibiting
			for (size_t i = 0; i < g_strv_length(inhibit); i++) {
				if (g_strcmp0(ent->class_name, inhibit[i]) == 0) {
					G_FreeEntity(ent);
					inhibited++;
					continue;
				}
			}

			// handle legacy spawn flags
			if (ent->locals.spawn_flags & SF_NOT_DEATHMATCH) {
				G_FreeEntity(ent);
				inhibited++;
				continue;
			}

			// strip away unsupported flags
			ent->locals.spawn_flags &= ~(SF_NOT_EASY | SF_NOT_MEDIUM | SF_NOT_HARD | SF_NOT_COOP);
		}

		if (!G_SpawnEntity(ent)) {
			G_FreeEntity(ent);
			inhibited++;
		}
	}

	g_strfreev(inhibit);

	gi.Debug("%i entities inhibited\n", inhibited);

	G_InitMedia();

	G_InitEntityTeams();

	G_CheckHook();

	G_CheckTechs();

	G_ResetTeams();

	G_InitSpawnPoints();

	G_ResetSpawnPoints();

	G_ResetVote();

	G_ResetItems();
}

/**
 * @brief
 */
static void G_WorldspawnMusic(void) {
	char *t, buf[MAX_STRING_CHARS];
	int32_t i;

	if (*g_level.music == '\0') {
		int32_t shuffle = Random();
		for (i = 0; i < MAX_MUSICS; i++) {
			gi.SetConfigString(CS_MUSICS + i, va("track%d", ((i + shuffle) % MAX_MUSICS) + 1));
		}
		return;
	}

	g_strlcpy(buf, g_level.music, sizeof(buf));

	i = 0;
	t = strtok(buf, ",");

	while (true) {

		if (!t) {
			break;
		}

		if (i == MAX_MUSICS) {
			break;
		}

		if (*t != '\0') {
			gi.SetConfigString(CS_MUSICS + i++, g_strstrip(t));
		}

		t = strtok(NULL, ",");
	}
}

/*QUAKED worldspawn (0 0 0) ?
 The worldspawn entity defines global conditions and behavior for the entire level. All brushes not belonging to an explicit entity implicitly belong to worldspawn.
 -------- KEYS --------
 message : The map title.
 sky : The sky environment map (default unit1_).
 ambient_light : The ambient light level (e.g. 0.14 0.11 0.12).
 sun_light : Sun light intensity, a single scalar value 0 - 255.
 sun_color : Sun light color (e.g. 0.8 0.4 0.7).
 sun_angles : Sun light angles as "pitch yaw roll" (e.g. 85 225 0).
 brightness : Global light scale, a single positive scalar value (e.g. 1.125).
 saturation : Global light saturation, a single positive scalar value (e.g. 0.9).
 contrast : Global light contrast, a single positive scalar value (e.g. 1.17).
 patch_size : Surface light patch size (default 64).
 weather : Weather effects, one of "none, rain, snow" followed optionally by "fog r g b."
 gravity : Gravity for the level (default 800).
 gameplay : The gameplay mode, one of "deathmatch, instagib, arena."
 hook : Enables the grappling hook (unset for gameplay default, 0 = disabled, 1 = enabled)."
 teams : Enables and enforces teams play (enabled = 1, auto-balance = 2).
 num_teams : Enforces number of teams (disabled = -1, must be between 2 and 4)
 ctf : Enables CTF play (enabled = 1, auto-balance = 2).
 match : Enables match play (round-based elimination with warmup) (enabled = 1).
 fraglimit : The frag limit (default 20).
 roundlimit : The round limit (default 20).
 capturelimit : The capture limit (default 8).
 timelimit : The time limit in minutes (default 20).
 give : A comma-delimited item string to give each player on spawn.
 */
static void G_worldspawn(g_entity_t *ent) {

	ent->solid = SOLID_BSP;
	ent->locals.move_type = MOVE_TYPE_NONE;
	ent->in_use = true; // since the world doesn't use G_Spawn()
	ent->s.model1 = 0; // world model is always index 1

	const g_map_list_map_t *map = G_MapList_Find(NULL, g_level.name);

	if (ent->locals.message && *ent->locals.message) {
		g_strlcpy(g_level.title, ent->locals.message, sizeof(g_level.title));
	} else
		// or just the level name
	{
		g_strlcpy(g_level.title, g_level.name, sizeof(g_level.title));
	}

	gi.SetConfigString(CS_NAME, g_level.title);
	gi.SetConfigString(CS_MAXCLIENTS, va("%d", sv_max_clients->integer));

	if (map && *map->sky) { // prefer maps.lst sky
		gi.SetConfigString(CS_SKY, map->sky);
	} else { // or fall back on worldspawn
		if (g_game.spawn.sky && *g_game.spawn.sky) {
			gi.SetConfigString(CS_SKY, g_game.spawn.sky);
		} else
			// or default to unit1_
		{
			gi.SetConfigString(CS_SKY, "unit1_");
		}
	}

	if (map && *map->weather) { // prefer maps.lst weather
		gi.SetConfigString(CS_WEATHER, map->weather);
	} else { // or fall back on worldspawn
		if (g_game.spawn.weather && *g_game.spawn.weather) {
			gi.SetConfigString(CS_WEATHER, g_game.spawn.weather);
		} else
			// or default to none
		{
			gi.SetConfigString(CS_WEATHER, "none");
		}
	}

	if (map && map->gravity > 0) { // prefer maps.lst gravity
		g_level.gravity = map->gravity;
	} else { // or fall back on worldspawn
		if (g_game.spawn.gravity && *g_game.spawn.gravity) {
			g_level.gravity = atoi(g_game.spawn.gravity);
		} else {
			g_level.gravity = DEFAULT_GRAVITY;
		}
	}

	if (g_strcmp0(g_gameplay->string, "default")) { // perfer g_gameplay
		g_level.gameplay = G_GameplayByName(g_gameplay->string);
	} else if (map && map->gameplay > -1) { // then maps.lst gameplay
		g_level.gameplay = map->gameplay;
	} else { // or fall back on worldspawn
		if (g_game.spawn.gameplay && *g_game.spawn.gameplay) {
			g_level.gameplay = G_GameplayByName(g_game.spawn.gameplay);
		} else {
			g_level.gameplay = GAME_DEATHMATCH;
		}
	}

	gi.SetConfigString(CS_GAMEPLAY, va("%d", g_level.gameplay));

	if (map && map->teams > -1) { // prefer maps.lst teams
		g_level.teams = map->teams;
	} else { // or fall back on worldspawn
		if (g_game.spawn.teams && *g_game.spawn.teams) {
			g_level.teams = atoi(g_game.spawn.teams);
		} else {
			g_level.teams = g_teams->integer;
		}
	}

	if (map && map->num_teams > -1) { // prefer maps.lst teams
		g_level.num_teams = map->num_teams;
	} else { // or fall back on worldspawn
		if (g_game.spawn.num_teams && *g_game.spawn.num_teams) {
			g_level.num_teams = atoi(g_game.spawn.num_teams);
		} else {
			if (g_strcmp0(g_num_teams->string, "default")) {
				g_level.num_teams = g_num_teams->integer;
			} else {
				g_level.num_teams = -1; // spawn point function will do this
			}
		}
	}

	if (g_level.num_teams != -1) {
		g_level.num_teams = Clamp(g_level.num_teams, 2, MAX_TEAMS);
	}

	if (map && map->ctf > -1) { // prefer maps.lst ctf
		g_level.ctf = map->ctf;
	} else { // or fall back on worldspawn
		if (g_game.spawn.ctf && *g_game.spawn.ctf) {
			g_level.ctf = atoi(g_game.spawn.ctf);
		} else {
			g_level.ctf = g_ctf->integer;
		}
	}

	if (map && map->hook > -1) {
		g_level.hook_map = map->hook;
	} else {
		g_level.hook_map = -1;
	}

	if (map && map->techs > -1) {
		g_level.techs_map = map->techs;
	} else {
		g_level.techs_map = -1;
	}

	if (g_level.teams && g_level.ctf) { // ctf overrides teams
		g_level.teams = 0;
	}

	gi.SetConfigString(CS_CTF, va("%d", g_level.ctf));
	gi.SetConfigString(CS_HOOK_PULL_SPEED, g_hook_pull_speed->string);

	if (map && map->match > -1) { // prefer maps.lst match
		g_level.match = map->match;
	} else { // or fall back on worldspawn
		if (g_game.spawn.match && *g_game.spawn.match) {
			g_level.match = atoi(g_game.spawn.match);
		} else {
			g_level.match = g_match->integer;
		}
	}

	if (map && map->rounds > -1) { // prefer maps.lst rounds
		g_level.rounds = map->rounds;
	} else { // or fall back on worldspawn
		if (g_game.spawn.rounds && *g_game.spawn.rounds) {
			g_level.rounds = atoi(g_game.spawn.rounds);
		} else {
			g_level.rounds = g_rounds->integer;
		}
	}

	if (g_level.match && g_level.rounds) { // rounds overrides match
		g_level.match = 0;
	}

	gi.SetConfigString(CS_MATCH, va("%d", g_level.match));
	gi.SetConfigString(CS_ROUNDS, va("%d", g_level.rounds));

	if (map && map->frag_limit > -1) { // prefer maps.lst frag_limit
		g_level.frag_limit = map->frag_limit;
	} else { // or fall back on worldspawn
		if (g_game.spawn.frag_limit && *g_game.spawn.frag_limit) {
			g_level.frag_limit = atoi(g_game.spawn.frag_limit);
		} else {
			g_level.frag_limit = g_frag_limit->integer;
		}
	}

	if (map && map->round_limit > -1) { // prefer maps.lst round_limit
		g_level.round_limit = map->round_limit;
	} else { // or fall back on worldspawn
		if (g_game.spawn.round_limit && *g_game.spawn.round_limit) {
			g_level.round_limit = atoi(g_game.spawn.round_limit);
		} else {
			g_level.round_limit = g_round_limit->integer;
		}
	}

	if (map && map->capture_limit > -1) { // prefer maps.lst capture_limit
		g_level.capture_limit = map->capture_limit;
	} else { // or fall back on worldspawn
		if (g_game.spawn.capture_limit && *g_game.spawn.capture_limit) {
			g_level.capture_limit = atoi(g_game.spawn.capture_limit);
		} else {
			g_level.capture_limit = g_capture_limit->integer;
		}
	}

	vec_t time_limit;
	if (map && map->time_limit > -1) { // prefer maps.lst time_limit
		time_limit = map->time_limit;
	} else { // or fall back on worldspawn
		if (g_game.spawn.time_limit && *g_game.spawn.time_limit) {
			time_limit = atof(g_game.spawn.time_limit);
		} else {
			time_limit = g_time_limit->value;
		}
	}
	g_level.time_limit = time_limit * 60 * 1000;

	if (map && *map->give) { // prefer maps.lst give
		g_strlcpy(g_level.give, map->give, sizeof(g_level.give));
	} else { // or fall back on worldspawn
		if (g_game.spawn.give && *g_game.spawn.give) {
			g_strlcpy(g_level.give, g_game.spawn.give, sizeof(g_level.give));
		} else {
			g_level.give[0] = '\0';
		}
	}

	if (map && *map->music) { // prefer maps.lst music
		g_strlcpy(g_level.music, map->music, sizeof(g_level.music));
	} else { // or fall back on worldspawn
		if (g_game.spawn.music && *g_game.spawn.music) {
			g_strlcpy(g_level.music, g_game.spawn.music, sizeof(g_level.music));
		} else {
			g_level.music[0] = '\0';
		}
	}

	G_WorldspawnMusic();

	gi.SetConfigString(CS_VOTE, "");
}

