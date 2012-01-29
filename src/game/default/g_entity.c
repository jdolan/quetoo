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

typedef struct {
	char *name;
	void (*spawn)(g_edict_t *ent);
} spawn_t;

static void G_worldspawn(g_edict_t *ent);

static spawn_t g_spawns[] = {
	{"info_player_start", G_info_player_start},
	{"info_player_deathmatch", G_info_player_deathmatch},
	{"info_player_team1", G_info_player_team1},
	{"info_player_team2", G_info_player_team2},
	{"info_player_intermission", G_info_player_intermission},
	{"info_null", G_info_null},
	{"info_notnull", G_info_notnull},

	{"func_plat", G_func_plat},
	{"func_button", G_func_button},
	{"func_door", G_func_door},
	{"func_rotating", G_func_rotating},
	{"func_train", G_func_train},
	{"func_conveyor", G_func_conveyor},
	{"func_areaportal", G_func_areaportal},
	{"func_wall", G_func_wall},
	{"func_water", G_func_water},
	{"func_timer", G_func_timer},
	{"func_killbox", G_func_killbox},

	{"trigger_always", G_trigger_always},
	{"trigger_once", G_trigger_once},
	{"trigger_multiple", G_trigger_multiple},
	{"trigger_relay", G_trigger_relay},
	{"trigger_push", G_trigger_push},
	{"trigger_hurt", G_trigger_hurt},
	{"trigger_exec", G_trigger_exec},
	{"trigger_teleporter", G_misc_teleporter},

	{"target_speaker", G_target_speaker},
	{"target_explosion", G_target_explosion},
	{"target_splash", G_target_splash},
	{"target_string", G_target_string},

	{"worldspawn", G_worldspawn},

	{"misc_teleporter", G_misc_teleporter},
	{"misc_teleporter_dest", G_misc_teleporter_dest},

	{NULL, NULL}
};

/*
 * G_SpawnEntity
 *
 * Finds the spawn function for the entity and calls it.
 */
static void G_SpawnEntity(g_edict_t *ent) {
	spawn_t *s;
	g_item_t *item;
	g_override_t *over;
	int i;

	if (!ent->class_name) {
		gi.Debug("G_SpawnEntity: NULL classname\n");
		return;
	}

	// check overrides
	for (i = 0; i < g_game.num_overrides; i++) {

		over = &g_overrides[i];

		if (!strcmp(ent->class_name, over->old)) { // found it
			item = G_FindItemByClassname(over->new);
			G_SpawnItem(ent, item);
			return;
		}
	}

	// check item spawn functions
	for (i = 0, item = g_items; i < g_game.num_items; i++, item++) {

		if (!item->class_name)
			continue;

		if (!strcmp(item->class_name, ent->class_name)) { // found it
			G_SpawnItem(ent, item);
			return;
		}
	}

	// check normal spawn functions
	for (s = g_spawns; s->name; s++) {
		if (!strcmp(s->name, ent->class_name)) { // found it
			s->spawn(ent);
			return;
		}
	}

	gi.Debug("%s doesn't have a spawn function\n", ent->class_name);
}

/*
 * G_NewString
 */
static char *G_NewString(const char *string) {
	char *newb, *new_p;
	int i, l;

	l = strlen(string) + 1;

	newb = gi.TagMalloc(l, TAG_LEVEL);

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
	F_STRING,     // string on disk, pointer in memory, TAG_LEVEL
	F_VECTOR,
	F_ANGLE
} g_field_type_t;

typedef struct g_field_s {
	char *name;
	ptrdiff_t ofs;
	g_field_type_t type;
	int flags;
} g_field_t;

static const g_field_t fields[] = {
	{"classname", FOFS(class_name), F_STRING, 0},
	{"model", FOFS(model), F_STRING, 0},
	{"spawnflags", FOFS(spawn_flags), F_INT, 0},
	{"speed", FOFS(speed), F_FLOAT, 0},
	{"accel", FOFS(accel), F_FLOAT, 0},
	{"decel", FOFS(decel), F_FLOAT, 0},
	{"target", FOFS(target), F_STRING, 0},
	{"targetname", FOFS(target_name), F_STRING, 0},
	{"pathtarget", FOFS(path_target), F_STRING, 0},
	{"killtarget", FOFS(kill_target), F_STRING, 0},
	{"message", FOFS(message), F_STRING, 0},
	{"team", FOFS(team), F_STRING, 0},
	{"command", FOFS(command), F_STRING, 0},
	{"script", FOFS(script), F_STRING, 0},
	{"wait", FOFS(wait), F_FLOAT, 0},
	{"delay", FOFS(delay), F_FLOAT, 0},
	{"random", FOFS(random), F_FLOAT, 0},
	{"areaportal", FOFS(area_portal), F_INT, 0},
	{"count", FOFS(count), F_INT, 0},
	{"health", FOFS(health), F_SHORT, 0},
	{"sounds", FOFS(sounds), F_SHORT, 0},
	{"dmg", FOFS(dmg), F_SHORT, 0},
	{"mass", FOFS(mass), F_FLOAT, 0},
	{"attenuation", FOFS(attenuation), F_SHORT, 0},
	{"origin", FOFS(s.origin), F_VECTOR, 0},
	{"angles", FOFS(s.angles), F_VECTOR, 0},
	{"angle", FOFS(s.angles), F_ANGLE, 0},

	// temp spawn vars -- only valid when the spawn function is called
	{"lip", SOFS(lip), F_INT, FFL_SPAWN_TEMP},
	{"distance", SOFS(distance), F_INT, FFL_SPAWN_TEMP},
	{"height", SOFS(height), F_INT, FFL_SPAWN_TEMP},
	{"noise", SOFS(noise), F_STRING, FFL_SPAWN_TEMP},
	{"pausetime", SOFS(pause_time), F_FLOAT, FFL_SPAWN_TEMP},
	{"item", SOFS(item), F_STRING, FFL_SPAWN_TEMP},

	// world vars, we use strings to differentiate between 0 and unset
	{"sky", SOFS(sky), F_STRING, FFL_SPAWN_TEMP},
	{"weather", SOFS(weather), F_STRING, FFL_SPAWN_TEMP},
	{"gravity", SOFS(gravity), F_STRING, FFL_SPAWN_TEMP},
	{"gameplay", SOFS(gameplay), F_STRING, FFL_SPAWN_TEMP},
	{"teams", SOFS(teams), F_STRING, FFL_SPAWN_TEMP},
	{"ctf", SOFS(ctf), F_STRING, FFL_SPAWN_TEMP},
	{"match", SOFS(match), F_STRING, FFL_SPAWN_TEMP},
	{"frag_limit", SOFS(frag_limit), F_STRING, FFL_SPAWN_TEMP},
	{"round_limit", SOFS(round_limit), F_STRING, FFL_SPAWN_TEMP},
	{"capture_limit", SOFS(capture_limit), F_STRING, FFL_SPAWN_TEMP},
	{"time_limit", SOFS(time_limit), F_STRING, FFL_SPAWN_TEMP},
	{"give", SOFS(give), F_STRING, FFL_SPAWN_TEMP},

	{0, 0, 0, 0}
};


/*
 * G_ParseField
 *
 * Takes a key-value pair and sets the binary values in an edict.
 */
static void G_ParseField(const char *key, const char *value, g_edict_t *ent) {
	const g_field_t *f;
	byte *b;
	float v;
	vec3_t vec;

	for (f = fields; f->name; f++) {

		if (!(f->flags & FFL_NO_SPAWN) && !strcasecmp(f->name, key)) { // found it

			if (f->flags & FFL_SPAWN_TEMP)
				b = (byte *) &g_game.spawn;
			else
				b = (byte *) ent;

			switch (f->type) {
			case F_SHORT:
				*(short *) (b + f->ofs) = (short) atoi(value);
				break;
			case F_INT:
				*(int *) (b + f->ofs) = atoi(value);
				break;
			case F_FLOAT:
				*(float *) (b + f->ofs) = atof(value);
				break;
			case F_STRING:
				*(char **) (b + f->ofs) = G_NewString(value);
				break;
			case F_VECTOR:
				sscanf(value, "%f %f %f", &vec[0], &vec[1], &vec[2]);
				((float *) (b + f->ofs))[0] = vec[0];
				((float *) (b + f->ofs))[1] = vec[1];
				((float *) (b + f->ofs))[2] = vec[2];
				break;
			case F_ANGLE:
				v = atof(value);
				((float *) (b + f->ofs))[0] = 0;
				((float *) (b + f->ofs))[1] = v;
				((float *) (b + f->ofs))[2] = 0;
				break;
			default:
				break;
			}
			return;
		}
	}

	//gi.Debug("%s is not a field\n", key);
}

/*
 * G_ParseEntity
 *
 * Parses an edict out of the given string, returning the new position
 * in said string.  The edict parameter should be a properly initialized
 * free edict.
 */
static const char *G_ParseEntity(const char *data, g_edict_t *ent) {
	boolean_t init;
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
			gi.Error("G_ParseEntity: EOF without closing brace.");

		strncpy(key, tok, sizeof(key) - 1);

		// parse value
		tok = ParseToken(&data);
		if (!data)
			gi.Error("G_ParseEntity: EOF in edict definition.");

		if (tok[0] == '}')
			gi.Error("G_ParseEntity: No edict definition.");

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

/*
 * G_InitEntityTeams
 *
 * Chain together all entities with a matching team field.
 *
 * All but the first will have the FL_TEAMSLAVE flag set.
 * All but the last will have the teamchain field set to the next one
 */
static void G_InitEntityTeams(void) {
	g_edict_t *e, *e2, *chain;
	unsigned int i, j;
	int c, c2;

	c = 0;
	c2 = 0;
	for (i = 1, e = g_game.edicts + i; i < ge.num_edicts; i++, e++) {

		if (!e->in_use)
			continue;

		if (!e->team)
			continue;

		if (e->flags & FL_TEAM_SLAVE)
			continue;

		chain = e;
		e->team_master = e;
		c++;
		c2++;

		for (j = i + 1, e2 = e + 1; j < ge.num_edicts; j++, e2++) {

			if (!e2->in_use)
				continue;

			if (!e2->team)
				continue;

			if (e2->flags & FL_TEAM_SLAVE)
				continue;

			if (!strcmp(e->team, e2->team)) {
				c2++;
				chain->team_chain = e2;
				e2->team_master = e;
				chain = e2;
				e2->flags |= FL_TEAM_SLAVE;
			}
		}
	}

	gi.Debug("%i teams with %i entities\n", c, c2);
}

/*
 * G_SpawnEntities
 *
 * Creates a server's entity / program execution context by
 * parsing textual entity definitions out of an ent file.
 */
void G_SpawnEntities(const char *name, const char *entities) {
	g_edict_t *ent;
	int inhibit;
	char *com_token;
	int i;

	gi.FreeTags(TAG_LEVEL);

	memset(&g_level, 0, sizeof(g_level));
	memset(g_game.edicts, 0, g_max_entities->value * sizeof(g_game.edicts[0]));

	strncpy(g_level.name, name, sizeof(g_level.name) - 1);

	// set client fields on player ents
	for (i = 0; i < sv_max_clients->integer; i++)
		g_game.edicts[i + 1].client = g_game.clients + i;

	ent = NULL;
	inhibit = 0;

	// parse ents
	while (true) {
		// parse the opening brace
		com_token = ParseToken(&entities);

		if (!entities)
			break;

		if (com_token[0] != '{')
			gi.Error("SpawnEntities: Found %s when expecting {.", com_token);

		if (!ent)
			ent = g_game.edicts;
		else
			ent = G_Spawn();

		entities = G_ParseEntity(entities, ent);

		// some ents don't belong in deathmatch
		if (ent != g_game.edicts) {

			// legacy levels may require this
			if (ent->spawn_flags & SF_NOT_DEATHMATCH) {
				G_FreeEdict(ent);
				inhibit++;
				continue;
			}

			// emits and models are client sided
			if (!strcmp(ent->class_name, "misc_emit") || !strcmp(
					ent->class_name, "misc_model")) {
				G_FreeEdict(ent);
				inhibit++;
				continue;
			}

			// lights aren't even used
			if (!strcmp(ent->class_name, "light") || !strcmp(ent->class_name,
					"light_spot")) {
				G_FreeEdict(ent);
				inhibit++;
				continue;
			}

			// strip away unsupported flags
			ent->spawn_flags &= ~(SF_NOT_EASY | SF_NOT_MEDIUM | SF_NOT_HARD
					| SF_NOT_COOP | SF_NOT_DEATHMATCH);
		}

		// retain the map-specified origin for respawns
		VectorCopy(ent->s.origin, ent->map_origin);

		G_SpawnEntity(ent);

		if (g_level.gameplay && ent->item) { // now that we've spawned them, hide them
			ent->sv_flags |= SVF_NO_CLIENT;
			ent->solid = SOLID_NOT;
			ent->next_think = 0.0;
		}
	}

	gi.Debug("%i entities inhibited\n", inhibit);

	G_InitEntityTeams();

	// reset teams and votes
	G_ResetTeams();

	G_ResetVote();
}

/*
 * G_WorldspawnMusic
 */
static void G_WorldspawnMusic(void) {
	char *t, buf[MAX_STRING_CHARS];
	int i;

	if (*g_level.music == '\0')
		return;

	strncpy(buf, g_level.music, sizeof(buf));

	i = 1;
	t = strtok(buf, ",");

	while (true) {

		if (!t)
			break;

		if (i == MAX_MUSICS)
			break;

		if (*t != '\0')
			gi.ConfigString(CS_MUSICS + i++, Trim(t));

		t = strtok(NULL, ",");
	}
}

/*QUAKED worldspawn(0 0 0) ?

 Only used for the world.
 "message"		"Stress Fractures by Jester"
 "sky"			unit1_
 "weather"		none, rain, snow, fog 0.5 0.8 0.7
 "gravity"		800
 "gameplay"		deathmatch, instagib, rocket arena
 "teams"			0 off, 1 on, 2 balanced
 "ctf"			0 off, 1 on, 2 balanced
 "match"			0 off, 1 on
 "round"			0 off, 1 on
 "frag_limit" 	20 frags
 "round_limit" 	20 rounds
 "capture_limit" 	8 captures
 "time_limit"		20 minutes
 "give"			comma-delimited items list
 "music"			comma-delimited track list
 */
static void G_worldspawn(g_edict_t *ent) {
	unsigned int i;
	g_map_list_elt_t *map;

	ent->move_type = MOVE_TYPE_PUSH;
	ent->solid = SOLID_BSP;
	ent->in_use = true; // since the world doesn't use G_Spawn()
	ent->s.model1 = 1; // world model is always index 1

	// set config_strings for items
	G_SetItemNames();

	map = NULL; // resolve the maps.lst entry for this level
	for (i = 0; i < g_map_list.count; i++) {
		if (!strcmp(g_level.name, g_map_list.maps[i].name)) {
			map = &g_map_list.maps[i];
			break;
		}
	}

	if (ent->message && *ent->message)
		strncpy(g_level.title, ent->message, sizeof(g_level.title));
	else
		// or just the level name
		strncpy(g_level.title, g_level.name, sizeof(g_level.title));
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
	gi.ConfigString(CS_GRAVITY, va("%d", g_level.gravity));

	if (map && map->gameplay > -1) // prefer maps.lst gameplay
		g_level.gameplay = map->gameplay;
	else { // or fall back on worldspawn
		if (g_game.spawn.gameplay && *g_game.spawn.gameplay)
			g_level.gameplay = G_GameplayByName(g_game.spawn.gameplay);
		else
			// or default to deathmatch
			g_level.gameplay = DEATHMATCH;
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

	if (map && map->time_limit > -1) // prefer maps.lst time_limit
		g_level.time_limit = map->time_limit;
	else { // or fall back on worldspawn
		if (g_game.spawn.time_limit && *g_game.spawn.time_limit)
			g_level.time_limit = atof(g_game.spawn.time_limit);
		else
			// or default to cvar
			g_level.time_limit = g_time_limit->value;
	}

	if (map && *map->give) // prefer maps.lst give
		strncpy(g_level.give, map->give, sizeof(g_level.give));
	else { // or fall back on worldspawn
		if (g_game.spawn.give && *g_game.spawn.give)
			strncpy(g_level.give, g_game.spawn.give, sizeof(g_level.give));
		else
			// or clean it
			g_level.give[0] = 0;
	}

	if (map && *map->music) // prefer maps.lst music
		strncpy(g_level.music, map->music, sizeof(g_level.music));
	else { // or fall back on worldspawn
		if (g_game.spawn.music && *g_game.spawn.music)
			strncpy(g_level.music, g_game.spawn.music, sizeof(g_level.music));
		else
			g_level.music[0] = 0;
	}

	G_WorldspawnMusic();

	G_PrecacheItem(G_FindItem("Shotgun"));
	G_PrecacheItem(G_FindItem("Shuper Shotgun"));
	G_PrecacheItem(G_FindItem("Machinegun"));
	G_PrecacheItem(G_FindItem("Chaingun"));
	G_PrecacheItem(G_FindItem("Grenade Launcher"));
	G_PrecacheItem(G_FindItem("Rocket Launcher"));
	G_PrecacheItem(G_FindItem("Hyperblaster"));
	G_PrecacheItem(G_FindItem("Lightning"));
	G_PrecacheItem(G_FindItem("Railgun"));
	G_PrecacheItem(G_FindItem("BFG10K"));

	gi.SoundIndex("world/water_in");
	gi.SoundIndex("world/water_out");

	gi.SoundIndex("weapons/common/no_ammo");
	gi.SoundIndex("weapons/common/pickup");
	gi.SoundIndex("weapons/common/switch");

	gi.ConfigString(CS_VOTE, "");
	gi.ConfigString(CS_TEAM_GOOD, g_team_good.name);
	gi.ConfigString(CS_TEAM_EVIL, g_team_evil.name);
}

