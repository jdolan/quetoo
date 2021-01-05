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

/**
 * @brief The entity class structure.
 */
typedef struct {
	/**
	 * @brief The entity class name.
	 */
	char *class_name;

	/**
	 * @brief The instance initializer.
	 */
	void (*Init)(g_entity_t *ent);
} g_entity_class_t;

static void G_worldspawn(g_entity_t *ent);

/**
 * @brief The entity classes.
 */
static const g_entity_class_t g_entity_classes[] = {

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

	// lastly, these are entities which we intentionally suppress

	{ "func_group", G_FreeEntity },
	{ "info_null", G_FreeEntity },

	{ "light", G_FreeEntity },
	{ "light_spot", G_FreeEntity },
	{ "light_sun", G_FreeEntity },

	{ "misc_flame", G_FreeEntity },
	{ "misc_fog", G_FreeEntity },
	{ "misc_light", G_FreeEntity },
	{ "misc_model", G_FreeEntity },
	{ "misc_sound", G_FreeEntity },
	{ "misc_sparks", G_FreeEntity },
	{ "misc_sprite", G_FreeEntity },
	{ "misc_steam", G_FreeEntity },
};

/**
 * @brief Populates common entity fields and then dispatches the class initializer.
 */
static _Bool G_SpawnEntity(g_entity_t *ent) {

	ent->class_name = gi.EntityValue(ent->def, "classname")->string;
	ent->s.origin = gi.EntityValue(ent->def, "origin")->vec3;
	ent->s.angles = gi.EntityValue(ent->def, "angles")->vec3;

	const cm_entity_t *angle = gi.EntityValue(ent->def, "angle");
	if (angle->parsed & ENTITY_FLOAT) {
		ent->s.angles = Vec3(0.f, angle->value, 0.f);
	}

	ent->model = gi.EntityValue(ent->def, "model")->nullable_string;

	// TODO: Trim down entity_locals_t, move all of these "one offs" to custom data
	// structs, allocated through each spawn function. See cgame cg_entity_t.

	ent->locals.spawn_flags = gi.EntityValue(ent->def, "spawnflags")->integer;

	ent->locals.speed = gi.EntityValue(ent->def, "speed")->value;
	ent->locals.accel = gi.EntityValue(ent->def, "accel")->value;
	ent->locals.decel = gi.EntityValue(ent->def, "decel")->value;
	ent->locals.lip = gi.EntityValue(ent->def, "lip")->value;

	ent->locals.target = gi.EntityValue(ent->def, "target")->nullable_string;
	ent->locals.target_name = gi.EntityValue(ent->def, "targetname")->nullable_string;
	ent->locals.path_target = gi.EntityValue(ent->def, "pathtarget")->nullable_string;
	ent->locals.kill_target = gi.EntityValue(ent->def, "killtarget")->nullable_string;
	ent->locals.message = gi.EntityValue(ent->def, "message")->nullable_string;
	ent->locals.team = gi.EntityValue(ent->def, "team")->nullable_string;
	ent->locals.command = gi.EntityValue(ent->def, "command")->nullable_string;
	ent->locals.script = gi.EntityValue(ent->def, "script")->nullable_string;

	ent->locals.wait = gi.EntityValue(ent->def, "wait")->value;
	ent->locals.delay = gi.EntityValue(ent->def, "delay")->value;
	ent->locals.random = gi.EntityValue(ent->def, "random")->value;
	ent->locals.count = gi.EntityValue(ent->def, "count")->integer;

	ent->locals.health = gi.EntityValue(ent->def, "health")->integer;
	ent->locals.damage = gi.EntityValue(ent->def, "dmg")->integer;
	ent->locals.mass = gi.EntityValue(ent->def, "mass")->value;

	ent->locals.atten = gi.EntityValue(ent->def, "atten")->integer;
	ent->locals.color = gi.EntityValue(ent->def, "_color")->vec3;
	ent->locals.light = gi.EntityValue(ent->def, "light")->value;

	// check item spawn functions
	for (size_t i = 0; i < g_num_items; i++) {

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

	for (size_t i = 0; i < lengthof(g_entity_classes); i++) {
		const g_entity_class_t *clazz = g_entity_classes + i;

		if (!g_strcmp0(clazz->class_name, ent->class_name)) {
			clazz->Init(ent);
			return true;
		}
	}

	gi.Warn("%s doesn't have a spawn function\n", etos(ent));
	return false;
}

/**
 * @brief Chain together all entities with a matching team field.
 *
 * All but the first will have the FL_TEAM_SLAVE flag set.
 * All but the last will have the team_next field set to the next one.
 */
static void G_InitEntityTeams(void) {

	int32_t teams = 0, team_entities = 0;

	g_entity_t *m = g_game.entities;
	for (int32_t i = 0; i < ge.num_entities; i++, m++) {

		if (!m->in_use) {
			continue;
		}

		if (!m->locals.team) {
			continue;
		}

		if (m->locals.flags & FL_TEAM_SLAVE) {
			continue;
		}

		g_entity_t *team = m;
		m->locals.team_master = m;

		teams++;
		team_entities++;

		g_entity_t *n = m + 1;
		for (int32_t j = i + 1; j < ge.num_entities; j++, n++) {

			if (!n->in_use) {
				continue;
			}

			if (!n->locals.team) {
				continue;
			}

			if (n->locals.flags & FL_TEAM_SLAVE) {
				continue;
			}

			if (!g_strcmp0(m->locals.team, n->locals.team)) {

				n->locals.team_master = m;
				n->locals.flags |= FL_TEAM_SLAVE;

				team->locals.team_next = n;
				team = n;

				team_entities++;
			}
		}
	}

	G_Debug("%i teams with %i entities\n", teams, team_entities);
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

	const int32_t a_dist = Vec3_DistanceSquared(flag->s.origin, ap->s.origin);
	const int32_t b_dist = Vec3_DistanceSquared(flag->s.origin, bp->s.origin);

	return SignOf(b_dist - a_dist);
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

		float furthest_dist = 0;

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
				line = Vec3_Subtract(pae->s.origin, pab->s.origin);
				line.z /= 10.0; // don't consider Z as heavily as X/Y

				const float dist = Vec3_LengthSquared(line);

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

		const uint8_t r = RandomRangeu(0, 2);

		red_flag->s.origin = reused_spawns[r]->s.origin;
		blue_flag->s.origin = reused_spawns[r ^ 1]->s.origin;
		
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

		const float dist_to_red = Vec3_DistanceSquared(red_flag->s.origin, p->s.origin);
		const float dist_to_blue = Vec3_DistanceSquared(blue_flag->s.origin, p->s.origin);

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

	while ((spot = G_Find(spot, EOFS(class_name), "info_player_start")) != NULL) {
		dm_spawns = g_slist_prepend(dm_spawns, spot);
	}

	spot = NULL;

	while ((spot = G_Find(spot, EOFS(class_name), "info_player_deathmatch")) != NULL) {
		dm_spawns = g_slist_prepend(dm_spawns, spot);
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

	ent->locals.velocity = Vec3(RandomRangef(-250.f, 250.f), RandomRangef(-250.f, 250.f), RandomRangef(200.f, 400.f));
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
void G_SpawnEntities(const char *name, cm_entity_t *const *entities, size_t num_entities) {

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
				g_game.ai_fill_slots = Clampf(g_ai_max_clients->integer, 0, sv_max_clients->integer);
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

		// load nav data
		G_Ai_Load(name);
	}
	
	memset(g_game.entities, 0, g_max_entities->value * sizeof(g_entity_t));
	memset(g_game.clients, 0, sv_max_clients->value * sizeof(g_client_t));

	for (int32_t i = 0; i < sv_max_clients->integer; i++) {
		g_game.entities[i + 1].client = g_game.clients + i;
	}

	ge.num_entities = sv_max_clients->integer + 1;

	gchar **inhibit = g_strsplit(g_inhibit->string, " ", -1);
	int32_t num_inhibited = 0;

	for (size_t i = 0; i < num_entities; i++) {

		g_entity_t *ent = i == 0 ? g_game.entities : G_AllocEntity();
		ent->def = entities[i];

		if (!G_SpawnEntity(ent)) {
			G_FreeEntity(ent);
			continue;
		}

		// enforce entity inhibiting
		for (size_t i = 0; i < g_strv_length(inhibit); i++) {
			if (!g_strcmp0(ent->class_name, inhibit[i])) {
				G_FreeEntity(ent);
				num_inhibited++;
				continue;
			}
		}
	}

	g_strfreev(inhibit);

	G_Debug("%i entities inhibited\n", num_inhibited);

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
 * @brief CompareFunc to shuffle tracks.
 */
gint G_WorldspawnMusic_shuffle(gconstpointer a, gconstpointer b) {
	return RandomRangei(-1, 2);
}

/**
 * @brief
 */
static void G_WorldspawnMusic(void) {

	if (*g_level.music == '\0') {

		int32_t num_tracks = 0;
		while (gi.FileExists(va("music/track%d.ogg", num_tracks + 1))) {
			num_tracks++;
		}

		GArray *tracks = g_array_new(0, 0, sizeof(int32_t));
		for (int32_t i = 1; i <= num_tracks; i++) {
			g_array_append_val(tracks, i);
		}
		g_array_sort(tracks, G_WorldspawnMusic_shuffle);

		for (int32_t i = 0; i < MAX_MUSICS; i++) {
			gi.SetConfigString(CS_MUSICS + i, va("track%d", g_array_index(tracks, int32_t, i)));
		}

		g_array_free(tracks, 1);
		return;
	}

	char buf[MAX_STRING_CHARS];
	g_strlcpy(buf, g_level.music, sizeof(buf));

	int32_t i = 0;
	char *t = strtok(buf, ",");

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
 ambient : The ambient light level (e.g. 0.14 0.11 0.12).
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

	if (map && *map->sky) { // prefer maps.lst sky
		gi.SetConfigString(CS_SKY, map->sky);
	} else { // or fall back on worldspawn
		const cm_entity_t *sky = gi.EntityValue(ent->def, "sky");
		if (*sky->string) {
			gi.SetConfigString(CS_SKY, sky->string);
		} else { // or default to unit1_
			gi.SetConfigString(CS_SKY, "unit1_");
		}
	}

	if (map && *map->weather) { // prefer maps.lst weather
		gi.SetConfigString(CS_WEATHER, map->weather);
	} else { // or fall back on worldspawn
		const cm_entity_t *weather = gi.EntityValue(ent->def, "weather");
		if (*weather->string) {
			gi.SetConfigString(CS_WEATHER, weather->string);
		} else { // or default to none
			gi.SetConfigString(CS_WEATHER, "none");
		}
	}

	if (map && map->gravity > 0) { // prefer maps.lst gravity
		g_level.gravity = map->gravity;
	} else { // or fall back on worldspawn
		const cm_entity_t *gravity = gi.EntityValue(ent->def, "gravity");
		if (gravity->parsed & ENTITY_INTEGER) {
			g_level.gravity = gravity->integer;
		} else {
			g_level.gravity = DEFAULT_GRAVITY;
		}
	}

	if (g_strcmp0(g_gameplay->string, "default")) { // perfer g_gameplay
		g_level.gameplay = G_GameplayByName(g_gameplay->string);
	} else if (map && map->gameplay > -1) { // then maps.lst gameplay
		g_level.gameplay = map->gameplay;
	} else { // or fall back on worldspawn
		const cm_entity_t *gameplay = gi.EntityValue(ent->def, "gameplay");
		if (*gameplay->string) {
			g_level.gameplay = G_GameplayByName(gameplay->string);
		} else {
			g_level.gameplay = GAME_DEATHMATCH;
		}
	}

	gi.SetConfigString(CS_GAMEPLAY, va("%d", g_level.gameplay));

	if (map && map->teams > -1) { // prefer maps.lst teams
		g_level.teams = map->teams;
	} else { // or fall back on worldspawn
		const cm_entity_t *teams = gi.EntityValue(ent->def, "teams");
		if (teams->parsed & ENTITY_INTEGER) {
			g_level.teams = teams->integer;
		} else {
			g_level.teams = g_teams->integer;
		}
	}

	if (map && map->num_teams > -1) { // prefer maps.lst teams
		g_level.num_teams = map->num_teams;
	} else { // or fall back on worldspawn
		const cm_entity_t *num_teams = gi.EntityValue(ent->def, "num_teams");
		if (num_teams->parsed & ENTITY_INTEGER) {
			g_level.num_teams = num_teams->integer;
		} else {
			if (g_strcmp0(g_num_teams->string, "default")) {
				g_level.num_teams = g_num_teams->integer;
			} else {
				g_level.num_teams = -1; // spawn point function will do this
			}
		}
	}

	if (g_level.num_teams != -1) {
		g_level.num_teams = Clampf(g_level.num_teams, 2, MAX_TEAMS);
	}

	if (map && map->ctf > -1) { // prefer maps.lst ctf
		g_level.ctf = map->ctf;
	} else { // or fall back on worldspawn
		const cm_entity_t *ctf = gi.EntityValue(ent->def, "ctf");
		if (ctf->parsed & ENTITY_INTEGER) {
			g_level.ctf = ctf->integer;
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
		const cm_entity_t *match = gi.EntityValue(ent->def, "match");
		if (match->parsed & ENTITY_INTEGER) {
			g_level.match = match->integer;
		} else {
			g_level.match = g_match->integer;
		}
	}

	if (map && map->rounds > -1) { // prefer maps.lst rounds
		g_level.rounds = map->rounds;
	} else { // or fall back on worldspawn
		const cm_entity_t *rounds = gi.EntityValue(ent->def, "rounds");
		if (rounds->parsed & ENTITY_INTEGER) {
			g_level.rounds = rounds->integer;
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
		const cm_entity_t *frag_limit = gi.EntityValue(ent->def, "frag_limit");
		if (frag_limit->parsed & ENTITY_INTEGER) {
			g_level.frag_limit = frag_limit->integer;
		} else {
			g_level.frag_limit = g_frag_limit->integer;
		}
	}

	if (map && map->round_limit > -1) { // prefer maps.lst round_limit
		g_level.round_limit = map->round_limit;
	} else { // or fall back on worldspawn
		const cm_entity_t *round_limit = gi.EntityValue(ent->def, "round_limit");
		if (round_limit->parsed & ENTITY_INTEGER) {
			g_level.round_limit = round_limit->integer;
		} else {
			g_level.round_limit = g_round_limit->integer;
		}
	}

	if (map && map->capture_limit > -1) { // prefer maps.lst capture_limit
		g_level.capture_limit = map->capture_limit;
	} else { // or fall back on worldspawn
		const cm_entity_t *capture_limit = gi.EntityValue(ent->def, "capture_limit");
		if (capture_limit->parsed & ENTITY_INTEGER) {
			g_level.capture_limit = capture_limit->integer;
		} else {
			g_level.capture_limit = g_capture_limit->integer;
		}
	}

	float minutes;
	if (map && map->time_limit > -1) { // prefer maps.lst time_limit
		minutes = map->time_limit;
	} else { // or fall back on worldspawn
		const cm_entity_t *time_limit = gi.EntityValue(ent->def, "time_limit");
		if (time_limit->parsed & ENTITY_FLOAT) {
			minutes = time_limit->value;
		} else {
			minutes = g_time_limit->value;
		}
	}
	g_level.time_limit = minutes * 60 * 1000;

	if (map && *map->give) { // prefer maps.lst give
		g_strlcpy(g_level.give, map->give, sizeof(g_level.give));
	} else { // or fall back on worldspawn
		const cm_entity_t *give = gi.EntityValue(ent->def, "give");
		if (*give->string) {
			g_strlcpy(g_level.give, give->string, sizeof(g_level.give));
		} else {
			g_level.give[0] = '\0';
		}
	}

	if (map && *map->music) { // prefer maps.lst music
		g_strlcpy(g_level.music, map->music, sizeof(g_level.music));
	} else { // or fall back on worldspawn
		const cm_entity_t *music = gi.EntityValue(ent->def, "music");
		if (*music->string) {
			g_strlcpy(g_level.music, music->string, sizeof(g_level.music));
		} else {
			g_level.music[0] = '\0';
		}
	}

	G_WorldspawnMusic();

	gi.SetConfigString(CS_VOTE, "");
}

