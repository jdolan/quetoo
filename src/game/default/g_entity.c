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

typedef struct {
	char *name;
	void (*Spawn)(g_entity_t *ent);
} g_entity_spawn_t;

static void G_worldspawn(g_entity_t *ent);

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
	{ "misc_emit", G_FreeEntity },
	{ "misc_model", G_FreeEntity },

	{ NULL, NULL }
};

/**
 * @brief Finds the spawn function for the entity and calls it.
 */
static void G_SpawnEntity(g_entity_t *ent) {
	g_entity_spawn_t *s;
	int32_t i;

	if (!ent->class_name) {
		gi.Debug("NULL classname\n");
		return;
	}

	// check item spawn functions
	const g_item_t *item = g_items;
	for (i = 0; i < g_num_items; i++, item++) {

		if (!item->class_name)
			continue;

		if (!g_strcmp0(item->class_name, ent->class_name)) { // found it
			G_SpawnItem(ent, item);
			return;
		}
	}

	// check normal spawn functions
	for (s = g_entity_spawns; s->name; s++) {
		if (!g_strcmp0(s->name, ent->class_name)) { // found it
			s->Spawn(ent);
			return;
		}
	}

	gi.Debug("%s doesn't have a spawn function\n", ent->class_name);
}

/**
 * @brief
 */
static char *G_NewString(const char *string) {
	char *newb, *new_p;
	int32_t i, l;

	l = strlen(string) + 1;

	newb = gi.Malloc(l, MEM_TAG_GAME_LEVEL);

	new_p = newb;

	for (i = 0; i < l; i++) {
		if (string[i] == '\\' && i < l - 1) {
			i++;
			if (string[i] == 'n')
				*new_p++ = '\n';
			else
				*new_p++ = '\\';
		} else
			*new_p++ = string[i];
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
	{ "teams", SOFS(teams), F_STRING, FFL_SPAWN_TEMP },
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

			if (f->flags & FFL_SPAWN_TEMP)
				b = (byte *) &g_game.spawn;
			else
				b = (byte *) ent;

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
 * @brief Parses an entity out of the given string, returning the new position
 * in said string. The entity should be a properly initialized free entity.
 */
static const char *G_ParseEntity(const char *data, g_entity_t *ent) {
	_Bool init;
	char key[MAX_QPATH];
	const char *tok;

	init = false;
	memset(&g_game.spawn, 0, sizeof(g_game.spawn));

	// go through all the dictionary pairs
	while (true) {
		// parse key
		tok = ParseToken(&data);
		if (tok[0] == '}')
			break;

		if (!data)
			gi.Error("EOF without closing brace\n");

		g_strlcpy(key, tok, sizeof(key));

		// parse value
		tok = ParseToken(&data);
		if (!data)
			gi.Error("EOF in entity definition\n");

		if (tok[0] == '}')
			gi.Error("No entity definition\n");

		init = true;

		// keys with a leading underscore are used for utility comments,
		// and are immediately discarded by quake
		if (key[0] == '_')
			continue;

		G_ParseField(key, tok, ent);
	}

	if (!init)
		memset(ent, 0, sizeof(*ent));

	return data;
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

		if (!e->in_use)
			continue;

		if (!e->locals.team)
			continue;

		if (e->locals.flags & FL_TEAM_SLAVE)
			continue;

		g_entity_t *chain = e;
		e->locals.team_master = e;

		teams++;
		team_entities++;

		for (j = i + 1, e2 = e + 1; j < ge.num_entities; j++, e2++) {

			if (!e2->in_use)
				continue;

			if (!e2->locals.team)
				continue;

			if (e2->locals.flags & FL_TEAM_SLAVE)
				continue;

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

	g_media.items.jacket_armor = ITEM_INDEX(G_FindItemByClassName("item_armor_jacket"));
	g_media.items.combat_armor = ITEM_INDEX(G_FindItemByClassName("item_armor_combat"));
	g_media.items.body_armor = ITEM_INDEX(G_FindItemByClassName("item_armor_body"));
	g_media.items.quad_damage = ITEM_INDEX(G_FindItemByClassName("item_quad"));

	g_media.models.grenade = gi.ModelIndex("models/objects/grenade/tris");
	g_media.models.rocket = gi.ModelIndex("models/objects/rocket/tris");

	g_media.sounds.bfg_hit = gi.SoundIndex("weapons/bfg/hit");
	g_media.sounds.bfg_prime = gi.SoundIndex("weapons/bfg/prime");
	g_media.sounds.grenade_hit = gi.SoundIndex("objects/grenade/hit");
	g_media.sounds.rocket_fly = gi.SoundIndex("objects/rocket/fly");
	g_media.sounds.lightning_fly = gi.SoundIndex("weapons/lightning/fly");
	g_media.sounds.quad_attack = gi.SoundIndex("quad/attack");

	g_media.sounds.teleport = gi.SoundIndex("world/teleport");

	g_media.sounds.water_in = gi.SoundIndex("world/water_in");
	g_media.sounds.water_out = gi.SoundIndex("world/water_out");

	g_media.sounds.weapon_no_ammo = gi.SoundIndex("weapons/common/no_ammo");
	g_media.sounds.weapon_switch = gi.SoundIndex("weapons/common/switch");

	for (i = 0; i < NUM_GIB_MODELS; i++) {
		g_media.models.gibs[i] = gi.ModelIndex(va("models/gibs/gib_%i/tris", i + 1));
		g_media.sounds.gib_hits[i] = gi.SoundIndex(va("gibs/gib_%i/hit", i + 1));
	}

	for (i = 1; i < lengthof(g_media.sounds.countdown); i++) {
		g_media.sounds.countdown[i] = gi.SoundIndex(va("world/countdown_%d", i));
	}
	
	g_media.sounds.roar = gi.SoundIndex("world/ominous_bwah");
	
	
	// precache all weapons, even if the map doesn't contain them
	G_PrecacheItem(G_FindItem("Blaster"));
	G_PrecacheItem(G_FindItem("Shotgun"));
	G_PrecacheItem(G_FindItem("Super Shotgun"));
	G_PrecacheItem(G_FindItem("Machinegun"));
	G_PrecacheItem(G_FindItem("Grenade Launcher"));
	G_PrecacheItem(G_FindItem("Rocket Launcher"));
	G_PrecacheItem(G_FindItem("Hyperblaster"));
	G_PrecacheItem(G_FindItem("Lightning"));
	G_PrecacheItem(G_FindItem("Railgun"));
	G_PrecacheItem(G_FindItem("BFG10K"));

	// precache these so that the HUD icons are available
	G_PrecacheItem(G_FindItem("Medium Health"));
	G_PrecacheItem(G_FindItem("Body Armor"));
}

/**
 * @brief Creates a server's entity / program execution context by
 * parsing textual entity definitions out of an ent file.
 */
void G_SpawnEntities(const char *name, const char *entities) {

	gi.FreeTag(MEM_TAG_GAME_LEVEL);

	memset(&g_level, 0, sizeof(g_level));

	g_strlcpy(g_level.name, name, sizeof(g_level.name));

	memset(g_game.entities, 0, g_max_entities->value * sizeof(g_entity_t));

	for (int32_t i = 0; i < sv_max_clients->integer; i++) {
		g_game.entities[i + 1].client = g_game.clients + i;
	}

	ge.num_entities = sv_max_clients->integer + 1;

	g_entity_t *ent = NULL;
	uint16_t inhibit = 0;

	// parse the entity definition string
	while (true) {

		const char *tok = ParseToken(&entities);

		if (!entities)
			break;

		if (tok[0] != '{')
			gi.Error("Found \"%s\" when expecting \"{\"", tok);

		if (ent == NULL)
			ent = g_game.entities;
		else
			ent = G_AllocEntity(__func__);

		entities = G_ParseEntity(entities, ent);

		// handle legacy spawn flags
		if (ent != g_game.entities) {

			if (ent->locals.spawn_flags & SF_NOT_DEATHMATCH) {
				G_FreeEntity(ent);
				inhibit++;
				continue;
			}

			// strip away unsupported flags
			ent->locals.spawn_flags &= ~(SF_NOT_EASY | SF_NOT_MEDIUM | SF_NOT_HARD | SF_NOT_COOP);
		}

		G_SpawnEntity(ent);
	}

	gi.Debug("%i entities inhibited\n", inhibit);

	G_InitEntityTeams();

	G_ResetItems();

	G_ResetTeams();

	G_ResetVote();

	G_InitMedia();
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
			gi.ConfigString(CS_MUSICS + i, va("track%d", ((i + shuffle) % MAX_MUSICS) + 1));
		}
		return;
	}

	g_strlcpy(buf, g_level.music, sizeof(buf));

	i = 0;
	t = strtok(buf, ",");

	while (true) {

		if (!t)
			break;

		if (i == MAX_MUSICS)
			break;

		if (*t != '\0')
			gi.ConfigString(CS_MUSICS + i++, g_strstrip(t));

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
 weather : Weather effects, one of "none, rain, snow" followed optionally by "fog r g b."
 gravity : Gravity for the level (default 800).
 gameplay : The gameplay mode, one of "deathmatch, instagib, arena."
 teams : Enables and enforces teams play (enabled = 1, auto-balance = 2).
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

	const g_map_list_map_t *map = G_MapList_Find(g_level.name);

	if (ent->locals.message && *ent->locals.message)
		g_strlcpy(g_level.title, ent->locals.message, sizeof(g_level.title));
	else
		// or just the level name
		g_strlcpy(g_level.title, g_level.name, sizeof(g_level.title));
	gi.ConfigString(CS_NAME, g_level.title);

	if (map && *map->sky) // prefer maps.lst sky
		gi.ConfigString(CS_SKY, map->sky);
	else { // or fall back on worldspawn
		if (g_game.spawn.sky && *g_game.spawn.sky)
			gi.ConfigString(CS_SKY, g_game.spawn.sky);
		else
			// or default to unit1_
			gi.ConfigString(CS_SKY, "unit1_");
	}

	if (map && *map->weather) // prefer maps.lst weather
		gi.ConfigString(CS_WEATHER, map->weather);
	else { // or fall back on worldspawn
		if (g_game.spawn.weather && *g_game.spawn.weather)
			gi.ConfigString(CS_WEATHER, g_game.spawn.weather);
		else
			// or default to none
			gi.ConfigString(CS_WEATHER, "none");
	}

	if (map && map->gravity > 0) // prefer maps.lst gravity
		g_level.gravity = map->gravity;
	else { // or fall back on worldspawn
		if (g_game.spawn.gravity && *g_game.spawn.gravity)
			g_level.gravity = atoi(g_game.spawn.gravity);
		else
			// or default to 800
			g_level.gravity = 800;
	}

	if (g_strcmp0(g_gameplay->string, "default")) { // perfer g_gameplay
		g_level.gameplay = G_GameplayByName(g_gameplay->string);
	} else if (map && map->gameplay > -1) { // then maps.lst gameplay
		g_level.gameplay = map->gameplay;
	} else { // or fall back on worldspawn
		if (g_game.spawn.gameplay && *g_game.spawn.gameplay)
			g_level.gameplay = G_GameplayByName(g_game.spawn.gameplay);
		else
			// or default to deathmatch
			g_level.gameplay = GAME_DEATHMATCH;
	}
	gi.ConfigString(CS_GAMEPLAY, va("%d", g_level.gameplay));

	if (map && map->teams > -1) // prefer maps.lst teams
		g_level.teams = map->teams;
	else { // or fall back on worldspawn
		if (g_game.spawn.teams && *g_game.spawn.teams)
			g_level.teams = atoi(g_game.spawn.teams);
		else
			// or default to cvar
			g_level.teams = g_teams->integer;
	}

	if (map && map->ctf > -1) // prefer maps.lst ctf
		g_level.ctf = map->ctf;
	else { // or fall back on worldspawn
		if (g_game.spawn.ctf && *g_game.spawn.ctf)
			g_level.ctf = atoi(g_game.spawn.ctf);
		else
			// or default to cvar
			g_level.ctf = g_ctf->integer;
	}

	if (g_level.teams && g_level.ctf) // ctf overrides teams
		g_level.teams = 0;

	gi.ConfigString(CS_TEAMS, va("%d", g_level.teams));
	gi.ConfigString(CS_CTF, va("%d", g_level.ctf));

	if (map && map->match > -1) // prefer maps.lst match
		g_level.match = map->match;
	else { // or fall back on worldspawn
		if (g_game.spawn.match && *g_game.spawn.match)
			g_level.match = atoi(g_game.spawn.match);
		else
			// or default to cvar
			g_level.match = g_match->integer;
	}

	if (map && map->rounds > -1) // prefer maps.lst rounds
		g_level.rounds = map->rounds;
	else { // or fall back on worldspawn
		if (g_game.spawn.rounds && *g_game.spawn.rounds)
			g_level.rounds = atoi(g_game.spawn.rounds);
		else
			// or default to cvar
			g_level.rounds = g_rounds->integer;
	}

	if (g_level.match && g_level.rounds) // rounds overrides match
		g_level.match = 0;

	gi.ConfigString(CS_MATCH, va("%d", g_level.match));
	gi.ConfigString(CS_ROUNDS, va("%d", g_level.rounds));

	if (map && map->frag_limit > -1) // prefer maps.lst frag_limit
		g_level.frag_limit = map->frag_limit;
	else { // or fall back on worldspawn
		if (g_game.spawn.frag_limit && *g_game.spawn.frag_limit)
			g_level.frag_limit = atoi(g_game.spawn.frag_limit);
		else
			// or default to cvar
			g_level.frag_limit = g_frag_limit->integer;
	}

	if (map && map->round_limit > -1) // prefer maps.lst round_limit
		g_level.round_limit = map->round_limit;
	else { // or fall back on worldspawn
		if (g_game.spawn.round_limit && *g_game.spawn.round_limit)
			g_level.round_limit = atoi(g_game.spawn.round_limit);
		else
			// or default to cvar
			g_level.round_limit = g_round_limit->integer;
	}

	if (map && map->capture_limit > -1) // prefer maps.lst capture_limit
		g_level.capture_limit = map->capture_limit;
	else { // or fall back on worldspawn
		if (g_game.spawn.capture_limit && *g_game.spawn.capture_limit)
			g_level.capture_limit = atoi(g_game.spawn.capture_limit);
		else
			// or default to cvar
			g_level.capture_limit = g_capture_limit->integer;
	}

	vec_t time_limit;
	if (map && map->time_limit > -1) // prefer maps.lst time_limit
		time_limit = map->time_limit;
	else { // or fall back on worldspawn
		if (g_game.spawn.time_limit && *g_game.spawn.time_limit)
			time_limit = atof(g_game.spawn.time_limit);
		else
			// or default to cvar
			time_limit = g_time_limit->value;
	}
	g_level.time_limit = time_limit * 60 * 1000;

	if (map && *map->give) // prefer maps.lst give
		g_strlcpy(g_level.give, map->give, sizeof(g_level.give));
	else { // or fall back on worldspawn
		if (g_game.spawn.give && *g_game.spawn.give)
			g_strlcpy(g_level.give, g_game.spawn.give, sizeof(g_level.give));
		else
			// or clean it
			g_level.give[0] = '\0';
	}

	if (map && *map->music) // prefer maps.lst music
		g_strlcpy(g_level.music, map->music, sizeof(g_level.music));
	else { // or fall back on worldspawn
		if (g_game.spawn.music && *g_game.spawn.music)
			g_strlcpy(g_level.music, g_game.spawn.music, sizeof(g_level.music));
		else
			g_level.music[0] = '\0';
	}

	G_WorldspawnMusic();

	gi.ConfigString(CS_VOTE, "");
	gi.ConfigString(CS_TEAM_GOOD, g_team_good.name);
	gi.ConfigString(CS_TEAM_EVIL, g_team_evil.name);
}

