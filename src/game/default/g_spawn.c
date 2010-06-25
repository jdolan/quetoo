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
	void (*spawn)(edict_t *ent);
} spawn_t;

static void G_worldspawn(edict_t *ent);

spawn_t spawns[] = {
	{"info_player_start", G_info_player_start},
	{"info_player_deathmatch", G_info_player_deathmatch},
	{"info_player_team1", G_info_player_team1},
	{"info_player_team2", G_info_player_team2},
	{"info_player_intermission", G_info_player_intermission},

	{"func_plat", G_func_plat},
	{"func_button", G_func_button},
	{"func_door", G_func_door},
	{"func_door_secret", G_func_door_secret},
	{"func_door_rotating", G_func_door_rotating},
	{"func_rotating", G_func_rotating},
	{"func_train", G_func_train},
	{"func_water", G_func_water},
	{"func_conveyor", G_func_conveyor},
	{"func_areaportal", G_func_areaportal},
	{"func_wall", G_func_wall},
	{"func_object", G_func_object},
	{"func_timer", G_func_timer},
	{"func_killbox", G_func_killbox},

	{"trigger_always", G_trigger_always},
	{"trigger_once", G_trigger_once},
	{"trigger_multiple", G_trigger_multiple},
	{"trigger_relay", G_trigger_relay},
	{"trigger_push", G_trigger_push},
	{"trigger_hurt", G_trigger_hurt},
	{"trigger_teleporter", G_misc_teleporter},

	{"target_speaker", G_target_speaker},
	{"target_explosion", G_target_explosion},
	{"target_splash", G_target_splash},
	{"target_string", G_target_string},

	{"worldspawn", G_worldspawn},

	{"info_null", G_info_null},
	{"func_group", G_info_null},
	{"info_notnull", G_info_notnull},

	{"misc_teleporter", G_misc_teleporter},
	{"misc_teleporter_dest", G_misc_teleporter_dest},

	{NULL, NULL}
};


/*
 * G_CallSpawn
 *
 * Finds the spawn function for the entity and calls it
 */
static void G_CallSpawn(edict_t *ent){
	spawn_t *s;
	gitem_t *item;
	override_t *over;
	int i;

	if(!ent->classname){
		gi.Dprintf("G_CallSpawn: NULL classname\n");
		return;
	}

	// check overrides
	for(i = 0; i < game.num_overrides; i++){
		over = &overrides[i];
		if(!strcmp(ent->classname, over->old)){  // found it
			item = G_FindItemByClassname(over->new);
			G_SpawnItem(ent, item);
			return;
		}
	}

	// check item spawn functions
	for(i = 0, item = itemlist; i < game.num_items; i++, item++){
		if(!item->classname)
			continue;
		if(!strcmp(item->classname, ent->classname)){  // found it
			G_SpawnItem(ent, item);
			return;
		}
	}

	// check normal spawn functions
	for(s = spawns; s->name; s++){
		if(!strcmp(s->name, ent->classname)){  // found it
			s->spawn(ent);
			return;
		}
	}
	gi.Dprintf("%s doesn't have a spawn function\n", ent->classname);
}


/*
 * G_NewString
 */
static char *G_NewString(const char *string){
	char *newb, *new_p;
	int i, l;

	l = strlen(string) + 1;

	newb = gi.TagMalloc(l, TAG_LEVEL);

	new_p = newb;

	for(i = 0; i < l; i++){
		if(string[i] == '\\' && i < l - 1){
			i++;
			if(string[i] == 'n')
				*new_p++ = '\n';
			else
				*new_p++ = '\\';
		} else
			*new_p++ = string[i];
	}

	return newb;
}


// fields are needed for spawning from the entity string
#define FFL_SPAWNTEMP		1
#define FFL_NOSPAWN			2

typedef enum {
	F_INT,
	F_FLOAT,
	F_LSTRING,     // string on disk, pointer in memory, TAG_LEVEL
	F_GSTRING,     // string on disk, pointer in memory, TAG_GAME
	F_VECTOR,
	F_ANGLEHACK,
	F_EDICT,     // index on disk, pointer in memory
	F_ITEM,     // index on disk, pointer in memory
	F_CLIENT,     // index on disk, pointer in memory
	F_FUNCTION,
	F_MMOVE,
	F_IGNORE
} fieldtype_t;

typedef struct {
	char *name;
	int ofs;
	fieldtype_t type;
	int flags;
} field_t;

static const field_t fields[] = {
	{"classname", FOFS(classname), F_LSTRING},
	{"model", FOFS(model), F_LSTRING},
	{"spawnflags", FOFS(spawnflags), F_INT},
	{"speed", FOFS(speed), F_FLOAT},
	{"accel", FOFS(accel), F_FLOAT},
	{"decel", FOFS(decel), F_FLOAT},
	{"target", FOFS(target), F_LSTRING},
	{"targetname", FOFS(targetname), F_LSTRING},
	{"pathtarget", FOFS(pathtarget), F_LSTRING},
	{"killtarget", FOFS(killtarget), F_LSTRING},
	{"message", FOFS(message), F_LSTRING},
	{"team", FOFS(team), F_LSTRING},
	{"wait", FOFS(wait), F_FLOAT},
	{"delay", FOFS(delay), F_FLOAT},
	{"random", FOFS(random), F_FLOAT},
	{"style", FOFS(style), F_INT},
	{"count", FOFS(count), F_INT},
	{"health", FOFS(health), F_INT},
	{"sounds", FOFS(sounds), F_INT},
	{"dmg", FOFS(dmg), F_INT},
	{"mass", FOFS(mass), F_INT},
	{"attenuation", FOFS(attenuation), F_INT},
	{"origin", FOFS(s.origin), F_VECTOR},
	{"angles", FOFS(s.angles), F_VECTOR},
	{"angle", FOFS(s.angles), F_ANGLEHACK},

	{"enemy", FOFS(enemy), F_EDICT, FFL_NOSPAWN},
	{"activator", FOFS(activator), F_EDICT, FFL_NOSPAWN},
	{"groundentity", FOFS(groundentity), F_EDICT, FFL_NOSPAWN},
	{"teamchain", FOFS(teamchain), F_EDICT, FFL_NOSPAWN},
	{"teammaster", FOFS(teammaster), F_EDICT, FFL_NOSPAWN},
	{"owner", FOFS(owner), F_EDICT, FFL_NOSPAWN},
	{"target_ent", FOFS(target_ent), F_EDICT, FFL_NOSPAWN},
	{"chain", FOFS(chain), F_EDICT, FFL_NOSPAWN},

	{"prethink", FOFS(prethink), F_FUNCTION, FFL_NOSPAWN},
	{"think", FOFS(think), F_FUNCTION, FFL_NOSPAWN},
	{"blocked", FOFS(blocked), F_FUNCTION, FFL_NOSPAWN},
	{"touch", FOFS(touch), F_FUNCTION, FFL_NOSPAWN},
	{"use", FOFS(use), F_FUNCTION, FFL_NOSPAWN},
	{"pain", FOFS(pain), F_FUNCTION, FFL_NOSPAWN},
	{"die", FOFS(die), F_FUNCTION, FFL_NOSPAWN},

	{"endfunc", FOFS(moveinfo.endfunc), F_FUNCTION, FFL_NOSPAWN},

	// temp spawn vars -- only valid when the spawn function is called
	{"lip", STOFS(lip), F_INT, FFL_SPAWNTEMP},
	{"distance", STOFS(distance), F_INT, FFL_SPAWNTEMP},
	{"height", STOFS(height), F_INT, FFL_SPAWNTEMP},
	{"noise", STOFS(noise), F_LSTRING, FFL_SPAWNTEMP},
	{"pausetime", STOFS(pausetime), F_FLOAT, FFL_SPAWNTEMP},
	{"item", STOFS(item), F_LSTRING, FFL_SPAWNTEMP},

	// need for item field in edict struct, FFL_SPAWNTEMP item will be skipped on saves
	{"item", FOFS(item), F_ITEM},

	// world vars, we use strings to differentiate between 0 and unset
	{"sky", STOFS(sky), F_LSTRING, FFL_SPAWNTEMP},
	{"weather", STOFS(weather), F_LSTRING, FFL_SPAWNTEMP},
	{"gravity", STOFS(gravity), F_LSTRING, FFL_SPAWNTEMP},
	{"gameplay", STOFS(gameplay), F_LSTRING, FFL_SPAWNTEMP},
	{"teams", STOFS(teams), F_LSTRING, FFL_SPAWNTEMP},
	{"ctf", STOFS(ctf), F_LSTRING, FFL_SPAWNTEMP},
	{"match", STOFS(match), F_LSTRING, FFL_SPAWNTEMP},
	{"fraglimit", STOFS(fraglimit), F_LSTRING, FFL_SPAWNTEMP},
	{"roundlimit", STOFS(roundlimit), F_LSTRING, FFL_SPAWNTEMP},
	{"capturelimit", STOFS(capturelimit), F_LSTRING, FFL_SPAWNTEMP},
	{"timelimit", STOFS(timelimit), F_LSTRING, FFL_SPAWNTEMP},
	{"give", STOFS(give), F_LSTRING, FFL_SPAWNTEMP},

	{0, 0, 0, 0}
};


/*
 * G_ParseField
 *
 * Takes a key/value pair and sets the binary values
 * in an edict
 */
static void G_ParseField(const char *key, const char *value, edict_t *ent){
	const field_t *f;
	byte *b;
	float v;
	vec3_t vec;

	for(f = fields; f->name; f++){
		if(!(f->flags & FFL_NOSPAWN) && !strcasecmp(f->name, key)){  // found it
			if(f->flags & FFL_SPAWNTEMP)
				b = (byte *)&st;
			else
				b = (byte *)ent;

			switch(f->type){
				case F_LSTRING:
					*(char **)(b + f->ofs) = G_NewString(value);
					break;
				case F_VECTOR:
					sscanf(value, "%f %f %f", &vec[0], &vec[1], &vec[2]);
					((float *)(b + f->ofs))[0] = vec[0];
					((float *)(b + f->ofs))[1] = vec[1];
					((float *)(b + f->ofs))[2] = vec[2];
					break;
				case F_INT:
					*(int *)(b + f->ofs) = atoi(value);
					break;
				case F_FLOAT:
					*(float *)(b + f->ofs) = atof(value);
					break;
				case F_ANGLEHACK:
					v = atof(value);
					((float *)(b + f->ofs))[0] = 0;
					((float *)(b + f->ofs))[1] = v;
					((float *)(b + f->ofs))[2] = 0;
					break;
				case F_IGNORE:
					break;
				default:
					break;
			}
			return;
		}
	}
	gi.Dprintf("%s is not a field\n", key);
}


/*
 * G_ParseEdict
 *
 * Parses an edict out of the given string, returning the new position
 * ed should be a properly initialized empty edict.
 */
static const char *G_ParseEdict(const char *data, edict_t *ent){
	qboolean init;
	char keyname[256];
	const char *com_token;

	init = false;
	memset(&st, 0, sizeof(st));

	// go through all the dictionary pairs
	while(true){
		// parse key
		com_token = Com_Parse(&data);
		if(com_token[0] == '}')
			break;
		if(!data)
			gi.Error("G_ParseEdict: EOF without closing brace.");

		strncpy(keyname, com_token, sizeof(keyname) - 1);

		// parse value
		com_token = Com_Parse(&data);
		if(!data)
			gi.Error("G_ParseEdict: EOF in edict definition.");

		if(com_token[0] == '}')
			gi.Error("G_ParseEdict: No edict definition.");

		init = true;

		// keynames with a leading underscore are used for utility comments,
		// and are immediately discarded by quake
		if(keyname[0] == '_')
			continue;

		G_ParseField(keyname, com_token, ent);
	}

	if(!init)
		memset(ent, 0, sizeof(*ent));

	return data;
}


/*
 * G_FindEdictTeams
 *
 * Chain together all entities with a matching team field.
 *
 * All but the first will have the FL_TEAMSLAVE flag set.
 * All but the last will have the teamchain field set to the next one
 */
static void G_FindEdictTeams(void){
	edict_t *e, *e2, *chain;
	int i, j;
	int c, c2;

	c = 0;
	c2 = 0;
	for(i = 1, e = g_edicts + i; i < globals.num_edicts; i++, e++){
		if(!e->inuse)
			continue;
		if(!e->team)
			continue;
		if(e->flags & FL_TEAMSLAVE)
			continue;
		chain = e;
		e->teammaster = e;
		c++;
		c2++;
		for(j = i + 1, e2 = e + 1; j < globals.num_edicts; j++, e2++){
			if(!e2->inuse)
				continue;
			if(!e2->team)
				continue;
			if(e2->flags & FL_TEAMSLAVE)
				continue;
			if(!strcmp(e->team, e2->team)){
				c2++;
				chain->teamchain = e2;
				e2->teammaster = e;
				chain = e2;
				e2->flags |= FL_TEAMSLAVE;
			}
		}
	}

	gi.Dprintf("%i teams with %i entities\n", c, c2);
}


/*
 * G_SpawnEntities
 *
 * Creates a server's entity / program execution context by
 * parsing textual entity definitions out of an ent file.
 */
void G_SpawnEntities(const char *name, const char *entities){
	edict_t *ent;
	int inhibit;
	char *com_token;
	int i;

	gi.FreeTags(TAG_LEVEL);

	memset(&level, 0, sizeof(level));
	memset(g_edicts, 0, g_maxentities->value * sizeof(g_edicts[0]));

	strncpy(level.name, name, sizeof(level.name) - 1);

	// set client fields on player ents
	for(i = 0; i < sv_maxclients->value; i++)
		g_edicts[i + 1].client = game.clients + i;

	ent = NULL;
	inhibit = 0;

	// parse ents
	while(true){
		// parse the opening brace
		com_token = Com_Parse(&entities);

		if(!entities)
			break;

		if(com_token[0] != '{')
			gi.Error("SpawnEntities: Found %s when expecting {.", com_token);

		if(!ent)
			ent = g_edicts;
		else
			ent = G_Spawn();

		entities = G_ParseEdict(entities, ent);

		// some ents don't belong in deathmatch
		if(ent != g_edicts){

			// legacy levels may require this
			if(ent->spawnflags & SF_NOT_DEATHMATCH){
				G_FreeEdict(ent);
				inhibit++;
				continue;
			}

			// emits and models are client sided
			if(!strcmp(ent->classname, "misc_emit") ||
					!strcmp(ent->classname, "misc_model")){
				G_FreeEdict(ent);
				inhibit++;
				continue;
			}

			// lights aren't even used
			if(!strcmp(ent->classname, "light")){
				G_FreeEdict(ent);
				inhibit++;
				continue;
			}

			// strip away unsupported flags
			ent->spawnflags &= ~(SF_NOT_EASY | SF_NOT_MEDIUM |
			SF_NOT_HARD | SF_NOT_COOP | SF_NOT_DEATHMATCH);
		}

		// retain the map-specified origin for respawns
		VectorCopy(ent->s.origin, ent->map_origin);

		G_CallSpawn(ent);

		if(level.gameplay && ent->item){  // now that we've spawned them, hide them
			ent->svflags |= SVF_NOCLIENT;
			ent->solid = SOLID_NOT;
			ent->nextthink = 0.0;
		}
	}

	gi.Dprintf("%i entities inhibited\n", inhibit);

	// reset teams and votes
	G_ResetTeams();

	G_ResetVote();

	G_FindEdictTeams();
}


/*
 * // cursor positioning
 * xl <value>
 * xr <value>
 * yb <value>
 * yt <value>
 * xv <value>
 * yv <value>
 *
 * // drawing
 * statpic <name>
 * pic <stat>
 * num <fieldwidth> <stat>
 * string <stat>
 *
 * // control
 * if <stat>
 * ifeq <stat> <value>
 * ifbit <stat> <value>
 * endif
 *
 */

const char *dm_statusbar =
	// health
	"if 0 "
	"xv	32 "
	"yb	-40 "
	"health "
	"yb -64 "
	"xv	96 "
	"pic 0 "
	"endif "

	// ammo and weapon
	"if 2 "
	"xv	256 "
	"yb -40 "
	"ammo "
	"yb -64 "
	"xv	320 "
	"pic 8 "
	"endif "

	// armor
	"if 4 "
	"xv	480 "
	"yb -40 "
	"armor "
	"yb -64 "
	"xv	544 "
	"pic 4 "
	"endif "

	// picked up item
	"if 6 "
	"xr -320 "
	"yt	2 "
	"pic 6 "
	"xr -256 "
	"yt 18 "
	"stat_string 7 "
	"endif "

	// spectator
	"if 12 "
	"xv 240 "
	"yb -128 "
	"string2 \"Spectating\" "
	"endif "

	// chasecam
	"if 13 "
	"xv 160 "
	"yb -128 "
	"string2 \"Chasing\" "
	"xv 288 "
	"stat_string 13 "
	"endif "

	// vote
	"if 14 "
	"xl 0 "
	"yb -160 "
	"string \"Vote: \" "
	"xl 96 "
	"stat_string 14 "
	"endif "

	// team
	"if 15 "
	"xr -240 "
	"yb -256 "
	"stat_string 15 "
	"endif "

	// match time / warmup
	"if 16 "
	"xr -96 "
	"yb -288 "
	"stat_string 16"
	"endif "

	// ready
	"if 17 "
	"xr -224 "
	"yb -288 "
	"string2 \"[Ready]\" "
	"endif "
	;


/**
 * G_WorldspawnMusic
 */
static void G_WorldspawnMusic(void){
	char *t, buf[MAX_STRING_CHARS];
	int i;

	if(!strlen(level.music))
		return;

	strncpy(buf, level.music, sizeof(buf));

	i = 1;
	t = strtok(buf, ",");

	while(true){

		if(!t)
			break;

		if(i == MAX_MUSICS)
			break;

		if(strlen(t))
			gi.Configstring(CS_MUSICS + i++, Com_TrimString(t));

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
"fraglimit" 	20 frags
"roundlimit" 	20 rounds
"capturelimit" 	8 captures
"timelimit"		20 minutes
"give"			comma-delimited items list
"music"			comma-delimited track list
*/
static void G_worldspawn(edict_t *ent){
	int i;
	maplistelt_t *map;

	ent->movetype = MOVETYPE_PUSH;
	ent->solid = SOLID_BSP;
	ent->inuse = true;  // since the world doesn't use G_Spawn()
	ent->s.modelindex = 1;  // world model is always index 1

	// set configstrings for items
	G_SetItemNames();

	map = NULL;  // resolve the maps.lst entry for this level
	for(i = 0; i < maplist.count; i++){
		if(!strcmp(level.name, maplist.maps[i].name)){
			map = &maplist.maps[i];
			break;
		}
	}

	if(ent->message && *ent->message)
		strncpy(level.title, ent->message, sizeof(level.title));
	else  // or just the level name
		strncpy(level.title, level.name, sizeof(level.title));
	gi.Configstring(CS_NAME, level.title);

	if(map && *map->sky)  // prefer maps.lst sky
		gi.Configstring(CS_SKY, map->sky);
	else {  // or fall back on worldspawn
		if(st.sky && *st.sky)
			gi.Configstring(CS_SKY, st.sky);
		else  // or default to unit1_
			gi.Configstring(CS_SKY, "unit1_");
	}

	if(map && *map->weather)  // prefer maps.lst weather
		gi.Configstring(CS_WEATHER, map->weather);
	else {  // or fall back on worldspawn
		if(st.weather && *st.weather)
			gi.Configstring(CS_WEATHER, st.weather);
		else  // or default to none
			gi.Configstring(CS_WEATHER, "none");
	}

	if(map && map->gravity > 0)  // prefer maps.lst gravity
		level.gravity = map->gravity;
	else {  // or fall back on worldspawn
		if(st.gravity && *st.gravity)
			level.gravity = atoi(st.gravity);
		else  // or default to 800
			level.gravity = 800;
	}
	gi.Configstring(CS_GRAVITY, va("%d", level.gravity));

	if(map && map->gameplay > -1)  // prefer maps.lst gameplay
		level.gameplay = map->gameplay;
	else {  // or fall back on worldspawn
		if(st.gameplay && *st.gameplay)
			level.gameplay = G_GameplayByName(st.gameplay);
		else  // or default to deathmatch
			level.gameplay = DEATHMATCH;
	}

	if(map && map->teams > -1)  // prefer maps.lst teams
		level.teams = map->teams;
	else {  // or fall back on worldspawn
		if(st.teams && *st.teams)
			level.teams = atoi(st.teams);
		else  // or default to cvar
			level.teams = g_teams->value;
	}

	if(map && map->ctf > -1)  // prefer maps.lst ctf
		level.ctf = map->ctf;
	else {  // or fall back on worldspawn
		if(st.ctf && *st.ctf)
			level.ctf = atoi(st.ctf);
		else  // or default to cvar
			level.ctf = g_ctf->value;
	}

	if(level.teams && level.ctf)  // ctf overrides teams
		level.teams = 0;

	if(map && map->match > -1)  // prefer maps.lst match
		level.match = map->match;
	else {  // or fall back on worldspawn
		if(st.match && *st.match)
			level.match = atoi(st.match);
		else  // or default to cvar
			level.match = g_match->value;
	}

	if(map && map->rounds > -1)  // prefer maps.lst rounds
		level.rounds = map->rounds;
	else {  // or fall back on worldspawn
		if(st.rounds && *st.rounds)
			level.rounds = atoi(st.rounds);
		else  // or default to cvar
			level.rounds = g_rounds->value;
	}

	if(level.match && level.rounds)  // rounds overrides match
		level.match = 0;

	if(map && map->fraglimit > -1)  // prefer maps.lst fraglimit
		level.fraglimit = map->fraglimit;
	else {  // or fall back on worldspawn
		if(st.fraglimit && *st.fraglimit)
			level.fraglimit = atoi(st.fraglimit);
		else  // or default to cvar
			level.fraglimit = g_fraglimit->value;
	}

	if(map && map->roundlimit > -1)  // prefer maps.lst roundlimit
		level.roundlimit = map->roundlimit;
	else {  // or fall back on worldspawn
		if(st.roundlimit && *st.roundlimit)
			level.roundlimit = atoi(st.roundlimit);
		else  // or default to cvar
			level.roundlimit = g_roundlimit->value;
	}

	if(map && map->capturelimit > -1)  // prefer maps.lst capturelimit
		level.capturelimit = map->capturelimit;
	else {  // or fall back on worldspawn
		if(st.capturelimit && *st.capturelimit)
			level.capturelimit = atoi(st.capturelimit);
		else  // or default to cvar
			level.capturelimit = g_capturelimit->value;
	}

	if(map && map->timelimit > -1)  // prefer maps.lst timelimit
		level.timelimit = map->timelimit;
	else {  // or fall back on worldspawn
		if(st.timelimit && *st.timelimit)
			level.timelimit = atof(st.timelimit);
		else  // or default to cvar
			level.timelimit = g_timelimit->value;
	}

	if(map && *map->give)  // prefer maps.lst give
		strncpy(level.give, map->give, sizeof(level.give));
	else {  // or fall back on worldspawn
		if(st.give && *st.give)
			strncpy(level.give, st.give, sizeof(level.give));
		else  // or clean it
			level.give[0] = 0;
	}

	if(map && *map->music)  // prefer maps.lst music
		strncpy(level.music, map->music, sizeof(level.music));
	else {  // or fall back on worldspawn
		if(st.music && *st.music)
			strncpy(level.music, st.music, sizeof(level.music));
		else
			level.music[0] = 0;
	}

	G_WorldspawnMusic();

	// send sv_maxclients to clients
	gi.Configstring(CS_MAXCLIENTS, va("%i", (int)(sv_maxclients->value)));

	// status bar program
	gi.Configstring(CS_STATUSBAR, dm_statusbar);

	G_PrecacheItem(G_FindItem("Shotgun"));
	G_PrecacheItem(G_FindItem("Shuper Shotgun"));
	G_PrecacheItem(G_FindItem("Machinegun"));
	G_PrecacheItem(G_FindItem("Chaingun"));
	G_PrecacheItem(G_FindItem("Grenade Launcher"));
	G_PrecacheItem(G_FindItem("Rocket Launcher"));
	G_PrecacheItem(G_FindItem("Hyperblaster"));
	G_PrecacheItem(G_FindItem("Railgun"));
	G_PrecacheItem(G_FindItem("BFG10K"));

	// THIS ORDER MUST MATCH THE DEFINES IN g_locals.h
	// you can add more, max 15
	gi.ModelIndex("#w_shotgun.md2");
	gi.ModelIndex("#w_sshotgun.md2");
	gi.ModelIndex("#w_machinegun.md2");
	gi.ModelIndex("#w_glauncher.md2");
	gi.ModelIndex("#w_rlauncher.md2");
	gi.ModelIndex("#w_hyperblaster.md2");
	gi.ModelIndex("#w_chaingun.md2");  //TODO: replace with lightning
	gi.ModelIndex("#w_railgun.md2");
	gi.ModelIndex("#w_bfg.md2");

	gi.SoundIndex("world/water_in");
	gi.SoundIndex("world/water_out");

	gi.SoundIndex("weapons/common/no_ammo");
	gi.SoundIndex("weapons/common/pickup");
	gi.SoundIndex("weapons/common/switch");

	gi.Configstring(CS_VOTE, "");
	gi.Configstring(CS_TEAMGOOD, va("%15s", good.name));
	gi.Configstring(CS_TEAMEVIL, va("%15s", evil.name));
}

