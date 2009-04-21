/*
 * Copyright(c) 1997-2001 Id Software, Inc.
 * Copyright(c) 2002 The Quakeforge Project.
 * Copyright(c) 2006 Quake2World.
 *
 * This program is free software; you can redistribute it and/or
 * modify it under the terms of the GNU General Public License
 * as published by the Free Software Foundation; either version 2
 * of the License, or(at your option) any later version.
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

#include <dirent.h>

#include "g_local.h"

game_locals_t game;
level_locals_t level;
game_import_t gi;
game_export_t globals;
spawn_temp_t st;

int meansOfDeath;

edict_t *g_edicts;

cvar_t *g_autojoin;
cvar_t *g_capturelimit;
cvar_t *g_chatlog;
cvar_t *g_cheats;
cvar_t *g_ctf;
cvar_t *g_fraglimit;
cvar_t *g_fraglog;
cvar_t *g_friendlyfire;
cvar_t *g_gameplay;
cvar_t *g_match;
cvar_t *g_maxentities;
cvar_t *g_mysql;
cvar_t *g_mysqldb;
cvar_t *g_mysqlhost;
cvar_t *g_mysqlpassword;
cvar_t *g_mysqluser;
cvar_t *g_playerprojectile;
cvar_t *g_randommap;
cvar_t *g_roundlimit;
cvar_t *g_rounds;
cvar_t *g_spawnfarthest;
cvar_t *g_teams;
cvar_t *g_timelimit;
cvar_t *g_voting;

cvar_t *password;

cvar_t *sv_maxclients;
cvar_t *dedicated;

team_t good, evil;

maplist_t maplist;

#ifdef HAVE_MYSQL
MYSQL *mysql;
char sql[512];
#endif

FILE *fraglog, *chatlog, *f;


/*
 * G_ResetTeams
 */
void G_ResetTeams(void){

	memset(&good, 0, sizeof(good));
	memset(&evil, 0, sizeof(evil));

	strcpy(good.name, "Good");
	gi.Configstring(CS_TEAMGOOD, va("%15s", good.name));

	strcpy(evil.name, "Evil");
	gi.Configstring(CS_TEAMEVIL, va("%15s", evil.name));

	strcpy(good.skin, "ogro/freedom");
	strcpy(evil.skin, "ichabod/ichabod");
}


/*
 * G_ResetVote
 */
void G_ResetVote(void){
	int i;

	for(i = 0; i < sv_maxclients->value; i++){  //reset vote flags
		if(!g_edicts[i + 1].inuse)
			continue;
		g_edicts[i + 1].client->locals.vote = VOTE_NOOP;
	}

	gi.Configstring(CS_VOTE, NULL);

	level.votes[0] = level.votes[1] = level.votes[2] = 0;
	level.vote_cmd[0] = 0;

	level.votetime = 0;
}

/*
 * G_ResetItems
 *
 * Reset all items in the level based on gameplay, ctf, etc.
 */
static void G_ResetItems(void){
	edict_t *ent;
	int i;

	for(i = 1; i < globals.num_edicts; i++){  // reset items

		ent = &g_edicts[i];

		if(!ent->inuse)
			continue;

		if(!ent->item)
			continue;

		if(ent->spawnflags & SF_ITEM_DROPPED){  // free dropped ones
			G_FreeEdict(ent);
			continue;
		}

		if(ent->item->flags & IT_FLAG){  // flags only appear for ctf

			if(level.ctf){
				ent->svflags &= ~SVF_NOCLIENT;
				ent->solid = SOLID_TRIGGER;
				ent->nextthink = level.time + 0.2;
			}
			else {
				ent->svflags |= SVF_NOCLIENT;
				ent->solid = SOLID_NOT;
				ent->nextthink = 0;
			}
		}
		else {  // everything else honors gameplay

			if(level.gameplay){  // hide items
				ent->svflags |= SVF_NOCLIENT;
				ent->solid = SOLID_NOT;
				ent->nextthink = 0.0;
			}
			else {  // or unhide them
				ent->svflags &= ~SVF_NOCLIENT;
				ent->solid = SOLID_TRIGGER;
				ent->nextthink = level.time + 2.0 * gi.serverframe;
			}
		}
	}
}


/*
 * G_RestartGame
 *
 * For normal games, this just means reset scores and respawn.
 * For match games, this means cancel the match and force everyone
 * to ready again.  Teams are only reset when teamz is true.
 */
static void G_RestartGame(qboolean teamz){
	int i;
	gclient_t *cl;

	if(level.matchtime)
		level.matchnum++;

	if(level.roundtime)
		level.roundnum++;

	for(i = 0; i < sv_maxclients->value; i++){  // reset clients
		if(!g_edicts[i + 1].inuse)
			continue;

		cl = g_edicts[i + 1].client;

		cl->locals.ready = false;  // back to warmup
		cl->locals.score = 0;
		cl->locals.captures = 0;

		if(teamz)  // reset teams
			cl->locals.team = NULL;

		// respawn players from the last match or round
		if(cl->locals.matchnum == level.matchnum)
			cl->locals.spectator = false;

		if(cl->locals.roundnum == level.roundnum)
			cl->locals.spectator = false;

		// as long as they have a team
		if((level.teams || level.ctf) && !cl->locals.team)
			cl->locals.spectator = true;

		P_Respawn(&g_edicts[i + 1], false);
	}

	G_ResetItems();

	level.matchtime = level.roundtime = 0;
	good.score = evil.score = 0;
	good.captures = evil.captures = 0;

	gi.Bprintf(PRINT_HIGH, "Game restarted\n");
	gi.Sound(&g_edicts[0], gi.SoundIndex("world/teleport"), ATTN_NONE);
}


/*
 * G_MuteClient
 */
static void G_MuteClient(char *name, qboolean mute){
	gclient_t *cl;

	if(!(cl = G_ClientByName(name)))
		return;

	cl->muted = mute;
}


/*
 * G_BeginIntermission
 */
static void G_BeginIntermission(const char *map){
	int i;
	edict_t *ent, *client;

	if(level.intermissiontime)
		return;  // already activated

	level.intermissiontime = level.time;

	// respawn any dead clients
	for(i = 0; i < sv_maxclients->value; i++){

		client = g_edicts + 1 + i;

		if(!client->inuse)
			continue;

		if(client->health <= 0)
			P_Respawn(client, false);
	}

	// find an intermission spot
	ent = G_Find(NULL, FOFS(classname), "info_player_intermission");
	if(!ent){  // map does not have an intermission point
		ent = G_Find(NULL, FOFS(classname), "info_player_start");
		if(!ent)
			ent = G_Find(NULL, FOFS(classname), "info_player_deathmatch");
	}

	VectorCopy(ent->s.origin, level.intermission_origin);
	VectorCopy(ent->s.angles, level.intermission_angle);

	// move all clients to the intermission point
	for(i = 0; i < sv_maxclients->value; i++){

		client = g_edicts + 1 + i;

		if(!client->inuse)
			continue;

		P_MoveToIntermission(client);
	}

	// play a dramatic sound effect
	gi.PositionedSound(level.intermission_origin, g_edicts,
			gi.SoundIndex("weapons/bfg/hit"), ATTN_NORM);

	// stay on same level if not provided
	level.changemap = map && *map ? map : level.name;
}


/*
 * G_CheckVote
 */
static void G_CheckVote(void){
	int i, count = 0;

	if(!g_voting->value)
		return;

	if(level.votetime == 0)
		return;

	if(level.time - level.votetime > MAX_VOTE_TIME){
		gi.Bprintf(PRINT_HIGH, "Vote \"%s\" expired\n", level.vote_cmd);
		G_ResetVote();
		return;
	}

	for(i = 0; i < sv_maxclients->value; i++){
		if(!g_edicts[i + 1].inuse)
			continue;
		count++;
	}

	if(level.votes[VOTE_YES] >= count * VOTE_MAJORITY){  // vote passed

		gi.Bprintf(PRINT_HIGH, "Vote \"%s\" passed\n", level.vote_cmd);

		if(!strncmp(level.vote_cmd, "map ", 4)){  // special case for map
			G_BeginIntermission(level.vote_cmd + 4);
		}
		else if(!strcmp(level.vote_cmd, "restart")){  // and restart
			G_RestartGame(false);
		}
		else if(!strncmp(level.vote_cmd, "mute ", 5)){  // and mute
			G_MuteClient(level.vote_cmd + 5, true);
		}
		else if(!strncmp(level.vote_cmd, "unmute ", 7)){
			G_MuteClient(level.vote_cmd + 7, false);
		}
		else {  // general case, just execute the command
			gi.AddCommandString(level.vote_cmd);
		}
		G_ResetVote();
	} else if(level.votes[VOTE_NO] >= count * VOTE_MAJORITY){  // vote failed
		gi.Bprintf(PRINT_HIGH, "Vote \"%s\" failed\n", level.vote_cmd);
		G_ResetVote();
	}
}


/*
 * G_EndLevel
 *
 * The timelimit, fraglimit, etc.. has been exceeded.
 */
static void G_EndLevel(void){
	int i;

	// try maplist
	if(maplist.count > 0){

		if(g_randommap->value){  // random weighted selection
			maplist.index = maplist.weighted_index[rand() % MAPLIST_WEIGHT];
		}
		else { // incremental, so long as wieght is not 0

			i = maplist.index;

			while(true){
				maplist.index = (maplist.index + 1) % maplist.count;

				if(!maplist.maps[maplist.index].weight)
					continue;

				if(maplist.index == i)  // wrapped around, all weights were 0
					break;

				break;
			}
		}

		G_BeginIntermission(maplist.maps[maplist.index].name);
		return;
	}

	// or stay on current level
	G_BeginIntermission(level.name);
}


/*
 * G_CheckRoundStart
 */
static void G_CheckRoundStart(void){
	int i, g, e, clients;
	gclient_t *cl;

	if(!level.rounds)
		return;

	if(level.roundtime)
		return;

	clients = g = e = 0;

	for(i = 0; i < sv_maxclients->value; i++){
		if(!g_edicts[i + 1].inuse)
			continue;

		cl = g_edicts[i + 1].client;

		if(cl->locals.spectator)
			continue;

		clients++;

		if(level.teams)
			cl->locals.team == &good ? g++ : e++;
	}

	if(clients < 2)  // need at least 2 clients to trigger countdown
		return;

	if(level.teams && (!g || !e))  // need at least 1 player per team
		return;

	if((int)level.teams == 2 && (g != e)){  // balanced teams required
		if(level.framenum % 100 == 0)
			gi.Bprintf(PRINT_HIGH, "Teams must be balanced for round to start\n");
		return;
	}

	gi.Bprintf(PRINT_HIGH, "Round starting in 10 seconds..\n");
	level.roundtime = level.time + 10.0;

	level.start_round = true;
}


/*
 * G_CheckRoundLimit
 */
static void G_CheckRoundLimit(){
	int i;
	edict_t *ent;
	gclient_t *cl;

	if(level.roundnum >= level.roundlimit){  // enforce roundlimit
		gi.Bprintf(PRINT_HIGH, "Roundlimit hit\n");
		G_EndLevel();
		return;
	}

	// or attempt to re-join previously active players
	for(i = 0; i < sv_maxclients->value; i++){
		if(!g_edicts[i + 1].inuse)
			continue;

		ent = &g_edicts[i + 1];
		cl = ent->client;

		if(cl->locals.roundnum != level.roundnum)
			continue;  // they were intentionally spectating, skip them

		if(level.teams || level.ctf){  // rejoin a team
			if(cl->locals.team)
				G_AddClientToTeam(ent, cl->locals.team->name);
			else
				G_AddClientToTeam(ent, G_SmallestTeam()->name);
		}
		else  // just rejoin the game
			cl->locals.spectator = false;

		P_Respawn(ent, false);
	}
}


/*
 * G_CheckRoundEnd
 */
static void G_CheckRoundEnd(void){
	int i, g, e, clients;
	edict_t *winner;
	gclient_t *cl;

	if(!level.rounds)
		return;

	if(!level.roundtime || level.roundtime > level.time)
		return;  // no round currently running

	winner = NULL;
	g = e = clients = 0;
	for(i = 0; i < sv_maxclients->value; i++){
		if(!g_edicts[i + 1].inuse)
			continue;

		cl = g_edicts[i + 1].client;

		if(cl->locals.spectator)  // true spectator, or dead
			continue;

		winner = &g_edicts[i + 1];

		if(level.teams)
			cl->locals.team == &good ? g++ : e++;

		clients++;
	}

	if(clients == 0){  // corner case where everyone was fragged
		gi.Bprintf(PRINT_HIGH, "Tie!\n");
		level.roundtime = 0;
		G_CheckRoundLimit();
		return;
	}

	if(level.teams || level.ctf){  // teams rounds continue if each team has a player
		if(g > 0 && e > 0)
			return;
	}
	else if(clients > 1)  // ffa continues if two players are alive
		return;

	// allow enemy projectiles to expire before declaring a winner
	for(i = 0; i < globals.num_edicts; i++){
		if(!g_edicts[i + 1].inuse)
			continue;

		if(!g_edicts[i + 1].owner)
			continue;

		if(!(cl = g_edicts[i + 1].owner->client))
			continue;

		if(level.teams || level.ctf){
			if(cl->locals.team != winner->client->locals.team)
				return;
		}
		else {
			if(g_edicts[i + 1].owner != winner)
				return;
		}
	}

	// we have a winner
	gi.Bprintf(PRINT_HIGH, "%s wins!\n", (level.teams || level.ctf ?
				winner->client->locals.team->name : winner->client->locals.netname));

	level.roundtime = 0;

	G_CheckRoundLimit();
}


/*
 * G_CheckMatchEnd
 */
static void G_CheckMatchEnd(void){
	int i, g, e, clients;
	gclient_t *cl;

	if(!level.match)
		return;

	if(!level.matchtime || level.matchtime > level.time)
		return;  // no match currently running

	g = e = clients = 0;
	for(i = 0; i < sv_maxclients->value; i++){
		if(!g_edicts[i + 1].inuse)
			continue;

		cl = g_edicts[i + 1].client;

		if(cl->locals.spectator)
			continue;

		if(level.teams || level.ctf)
			cl->locals.team == &good ?  g++ : e++;

		clients++;
	}

	if(clients == 0){  // everyone left
		gi.Bprintf(PRINT_HIGH, "No players left\n");
		level.matchtime = 0;
		return;
	}

	if((level.teams || level.ctf) && (!g || !e)){
		gi.Bprintf(PRINT_HIGH, "Not enough players left\n");
		level.matchtime = 0;
		return;
	}
}


/*
 * G_FormatTime
 */
static char formatted_time[16];
static int last_secs = 999999;
static char *G_FormatTime(int secs){
	int i;
	const int m = secs / 60;
	const int s = secs % 60;

	snprintf(formatted_time, sizeof(formatted_time), " %2d:%02d", m, s);

	// highlight for countdowns
	if(m == 0 && s < 30 && secs < last_secs && s & 1){
		for(i = 0; i < 6; i++)
			formatted_time[i] += 128;
	}

	last_secs = secs;

	return formatted_time;
}


/*
 * G_CheckRules
 */
static void G_CheckRules(void){
	int i, seconds;
	gclient_t *cl;

	if(level.intermissiontime)
		return;

	// match mode, no match, or countdown underway
	level.warmup = level.match && (!level.matchtime || level.matchtime > level.time);

	// arena mode, no round, or countdown underway
	level.warmup |= level.rounds && (!level.roundtime || level.roundtime > level.time);

	if(level.start_match && level.time >= level.matchtime){  // players have readied, begin match
		level.start_match = false;
		level.warmup = false;

		for(i = 0; i < sv_maxclients->value; i++){
			if(!g_edicts[i + 1].inuse)
				continue;
			P_Respawn(&g_edicts[i + 1], false);
		}

		gi.Sound(&g_edicts[0], gi.SoundIndex("world/teleport"), ATTN_NONE);
		gi.Bprintf(PRINT_HIGH, "Match has started\n");
	}

	if(level.start_round && level.time >= level.roundtime){  // pre-game expired, begin round
		level.start_round = false;
		level.warmup = false;

		for(i = 0; i < sv_maxclients->value; i++){
			if(!g_edicts[i + 1].inuse)
				continue;
			P_Respawn(&g_edicts[i + 1], false);
		}

		gi.Sound(&g_edicts[0], gi.SoundIndex("world/teleport"), ATTN_NONE);
		gi.Bprintf(PRINT_HIGH, "Round has started\n");
	}

	seconds = level.time;

	if(level.rounds){
		if(level.roundtime > level.time)  // round about to start, show pre-game countdown
			seconds = level.roundtime - level.time;
		else if(level.roundtime)
			seconds = level.time - level.roundtime;  // round started, count up
		else seconds = -1;
	}
	else if(level.match){
		if(level.matchtime > level.time)  // match about to start, show pre-game countdown
			seconds = level.matchtime - level.time;
		else if(level.matchtime){
			if(level.timelimit)  // count down to timelimit
				seconds = level.matchtime + level.timelimit * 60 - level.time;
			else seconds = level.time - level.matchtime;  // count up
		}
		else seconds = -1;
	}

	if(level.timelimit){  // check timelimit
		float t = level.time;

		if(level.match)  // for matches
			t = level.time - level.matchtime;
		else if(level.rounds)  // and for rounds
			t = level.time - level.roundtime;

		if(t >= level.timelimit * 60){
			gi.Bprintf(PRINT_HIGH, "Timelimit hit\n");
			G_EndLevel();
			return;
		}
		seconds = level.timelimit * 60 - t;  // count down
	}

	if(level.framenum % gi.serverrate == 0)  // send time updates once per second
		gi.Configstring(CS_TIME, (level.warmup ? "Warmup" : G_FormatTime(seconds)));

	if(!level.ctf && level.fraglimit){  // check fraglimit

		if(level.teams){  // check team scores
 			if(good.score >= level.fraglimit || evil.score >= level.fraglimit){
				gi.Bprintf(PRINT_HIGH, "Fraglimit hit\n");
				G_EndLevel();
				return;
			}
		}
		else {  // or individual scores
			for(i = 0; i < sv_maxclients->value; i++){
				cl = game.clients + i;
				if(!g_edicts[i + 1].inuse)
					continue;

				if(cl->locals.score >= level.fraglimit){
					gi.Bprintf(PRINT_HIGH, "Fraglimit hit\n");
					G_EndLevel();
					return;
				}
			}
		}
	}

	if(level.ctf && level.capturelimit){  // check capture limit

		if(good.captures >= level.capturelimit || evil.captures >= level.capturelimit){
			gi.Bprintf(PRINT_HIGH, "Capturelimit hit\n");
			G_EndLevel();
			return;
		}
	}

	if(g_gameplay->modified){  // change gameplay, fix items, respaw clients
		g_gameplay->modified = false;
		level.gameplay = G_GameplayByName(g_gameplay->string);

		G_RestartGame(false);  // reset all clients

		gi.Bprintf(PRINT_HIGH, "Gameplay has changed to %s\n",
				G_GameplayName(level.gameplay));
	}

	if(g_teams->modified){  // reset teams, scores
		g_teams->modified = false;
		level.teams = g_teams->value;

		gi.Bprintf(PRINT_HIGH, "Teams have been %s\n",
				level.teams ? "enabled" : "disabled");

		G_RestartGame(true);
	}

	if(g_ctf->modified){  // reset teams, scores
		g_ctf->modified = false;
		level.ctf = g_ctf->value;

		gi.Bprintf(PRINT_HIGH, "CTF has been %s\n",
				level.ctf ? "enabled" : "disabled");

		G_RestartGame(true);
	}

	if(g_match->modified){  // reset scores
		g_match->modified = false;
		level.match = g_match->value;

		level.warmup = level.match;  // toggle warmup

		gi.Bprintf(PRINT_HIGH, "Match has been %s\n",
				level.match ? "enabled" : "disabled");

		G_RestartGame(false);
	}

	if(g_rounds->modified){  // reset scores
		g_rounds->modified = false;
		level.rounds = g_rounds->value;

		level.warmup = level.rounds;  // toggle warmup

		gi.Bprintf(PRINT_HIGH, "Rounds have been %s\n",
				level.rounds ? "enabled" : "disabled");

		G_RestartGame(false);
	}

	if(g_cheats->modified){  // notify when cheats changes
		g_cheats->modified = false;

		gi.Bprintf(PRINT_HIGH, "Cheats have been %s\n",
				g_cheats->value ? "enabled" : "disabled");
	}

	if(g_fraglimit->modified){
		g_fraglimit->modified = false;
		level.fraglimit = g_fraglimit->value;

		gi.Bprintf(PRINT_HIGH, "Fraglimit has been changed to %d\n",
				level.fraglimit);
	}

	if(g_roundlimit->modified){
		g_roundlimit->modified = false;
		level.roundlimit = g_roundlimit->value;

		gi.Bprintf(PRINT_HIGH, "Roundlimit has been changed to %d\n",
				level.roundlimit);
	}

	if(g_capturelimit->modified){
		g_capturelimit->modified = false;
		level.capturelimit = g_capturelimit->value;

		gi.Bprintf(PRINT_HIGH, "Capturelimit has been changed to %d\n",
				level.capturelimit);
	}

	if(g_timelimit->modified){
		g_timelimit->modified = false;
		level.timelimit = g_timelimit->value;

		gi.Bprintf(PRINT_HIGH, "Timelimit has been changed to %d\n",
				(int)level.timelimit);
	}
}


/*
 * G_ExitLevel
 */
static void G_ExitLevel(void){

	gi.AddCommandString(va("map %s\n", level.changemap));

	level.changemap = NULL;
	level.intermissiontime = 0;

	P_EndServerFrames();
}

/*
 * G_RunFrame
 *
 * Advances the world by 0.1 seconds
 */
static void G_RunFrame(void){
	int i;
	edict_t *ent;

	level.framenum++;
	level.time = level.framenum * gi.serverframe;

	// exit intermissions
	if(level.intermissiontime && level.time > level.intermissiontime + 10.0){
		G_ExitLevel();
		return;
	}

	// treat each object in turn
	// even the world gets a chance to think
	ent = &g_edicts[0];
	for(i = 0; i < globals.num_edicts; i++, ent++){

		if(!ent->inuse)
			continue;

		level.current_entity = ent;

		if(!(ent->s.effects & EF_LIGHTNING))  // update old origin for lerps
			VectorCopy(ent->s.origin, ent->s.old_origin);

		// if the ground entity moved, make sure we are still on it
		if((ent->groundentity) && (ent->groundentity->linkcount != ent->groundentity_linkcount)){
			ent->groundentity = NULL;
		}

		if(i > 0 && i <= sv_maxclients->value){
			P_BeginServerFrame(ent);
			continue;
		}

		G_RunEntity(ent);
	}

	// see if a vote has passed
	G_CheckVote();

	// inspect and enforce gameplay rules
	G_CheckRules();

	// see if a match should end
	G_CheckMatchEnd();

	// see if an arena round should start
	G_CheckRoundStart();

	// see if an arena round should end
	G_CheckRoundEnd();

	// build the playerstate_t structures for all players
	P_EndServerFrames();
}


/*
 * G_InitMaplist
 */
static void G_InitMaplist(void){
	int i;

	memset(&maplist, 0, sizeof(maplist));

	for(i = 0; i < MAX_MAPLIST_ELTS; i++){
		maplistelt_t *map = &maplist.maps[i];
		map->gravity = map->gameplay = -1;
		map->teams = map->ctf = map->match = -1;
		map->fraglimit = map->roundlimit = map->capturelimit = -1;
		map->timelimit = -1;
		map->weight = 1.0;
	}
}


/*
 * G_ParseMaplist
 *
 * Populates a maplist_t from a text file.  See default/maps.lst
 */
static void G_ParseMaplist(const char *filename){
	void *buf;
	const char *buffer;
	int i, j, k, l;
	const char *c;
	qboolean inmap;
	maplistelt_t *map;

	G_InitMaplist();

	if(!filename || !filename[0]){
		maplist.count = 0;
		return;
	}

	if((i = gi.LoadFile(filename, &buf)) < 1){
		gi.Printf("Couldn't open %s\n", filename);
		maplist.count = 0;
		return;
	}

	buffer = (char *)buf;

	i = 0;
	inmap = false;
	while(true){

		c = Com_Parse(&buffer);

		if(!strlen(c))
			break;

		if(*c == '{')
			inmap = true;

		if(!inmap)  // skip any whitespace between maps
			continue;

		map = &maplist.maps[i];

		if(!strcmp(c, "name")){
			strncpy(map->name, Com_Parse(&buffer), 31);
			continue;
		}

		if(!strcmp(c, "title")){
			strncpy(map->title, Com_Parse(&buffer), 127);
			continue;
		}

		if(!strcmp(c, "sky")){
			strncpy(map->sky, Com_Parse(&buffer), 31);
			continue;
		}

		if(!strcmp(c, "weather")){
			strncpy(map->weather, Com_Parse(&buffer), 63);
			continue;
		}

		if(!strcmp(c, "gravity")){
			map->gravity = atoi(Com_Parse(&buffer));
			continue;
		}

		if(!strcmp(c, "gameplay")){
			map->gameplay = G_GameplayByName(Com_Parse(&buffer));
			continue;
		}

		if(!strcmp(c, "teams")){
			map->teams = atoi(Com_Parse(&buffer));
			continue;
		}

		if(!strcmp(c, "ctf")){
			map->ctf = atoi(Com_Parse(&buffer));
			continue;
		}

		if(!strcmp(c, "match")){
			map->match = atoi(Com_Parse(&buffer));
			continue;
		}

		if(!strcmp(c, "rounds")){
			map->rounds = atoi(Com_Parse(&buffer));
			continue;
		}

		if(!strcmp(c, "fraglimit")){
			map->fraglimit = atoi(Com_Parse(&buffer));
			continue;
		}

		if(!strcmp(c, "roundlimit")){
			map->roundlimit = atoi(Com_Parse(&buffer));
			continue;
		}

		if(!strcmp(c, "capturelimit")){
			map->capturelimit = atoi(Com_Parse(&buffer));
			continue;
		}

		if(!strcmp(c, "timelimit")){
			map->timelimit = atof(Com_Parse(&buffer));
			continue;
		}

		if(!strcmp(c, "give")){
			strncpy(map->give, Com_Parse(&buffer), 1023);
			continue;
		}

		if(!strcmp(c, "weight")){
			map->weight = atof(Com_Parse(&buffer));
			continue;
		}

		if(*c == '}'){  // close it out
			inmap = false;

			/*printf("Loaded map %s:\n"
					"title: %s\n"
					"sky: %s\n"
					"weather: %d\n"
					"gravity: %d\n"
					"gameplay: %d\n"
					"teams: %d\n"
					"ctf: %d\n"
					"match: %d\n"
					"rounds: %d\n"
					"fraglimit: %d\n"
					"roundlimit: %d\n"
					"capturelimit: %d\n"
					"timelimit: %f\n"
					"give: %s\n"
					"weight: %f\n",
					map->name, map->title, map->sky, map->weather, map->gravity,
					map->gameplay, map->teams, map->ctf, map->match, map->rounds, map->fraglimit,
					map->roundlimit, map->capturelimit, map->timelimit, map->give, map->weight);*/

			// accumulate total weight
			maplist.total_weight += map->weight;

			if(++i == MAX_MAPLIST_ELTS)
				break;
		}
	}

	maplist.count = i;
	maplist.index = 0;

	gi.TagFree(buf);

	// tho shalt not divide by zero
	if(!maplist.total_weight)
		maplist.total_weight = 1.0;

	// compute the weighted index list
	for(i = 0, l = 0; i < maplist.count; i++){

		map = &maplist.maps[i];
		k = (map->weight / maplist.total_weight) * MAPLIST_WEIGHT;

		for(j = 0; j < k; j++)
			maplist.weighted_index[l++] = i;
	}
}


/*
 * G_Init
 *
 * This will be called when the game module is first loaded.
 */
void G_Init(void){
	gi.Printf("  Game initialization..\n");

	gi.Cvar("gamename", GAMEVERSION , CVAR_SERVERINFO | CVAR_NOSET, NULL);
	gi.Cvar("gamedate", __DATE__ , CVAR_SERVERINFO | CVAR_NOSET, NULL);

	g_autojoin = gi.Cvar("g_autojoin", "1", CVAR_SERVERINFO, NULL);
	g_capturelimit = gi.Cvar("g_capturelimit", "8", CVAR_SERVERINFO, NULL);
	g_chatlog = gi.Cvar("g_chatlog", "0", 0, NULL);
	g_cheats = gi.Cvar("g_cheats", "0", CVAR_SERVERINFO, NULL);
	g_ctf = gi.Cvar("g_ctf", "0", CVAR_SERVERINFO, NULL);
	g_fraglimit = gi.Cvar("g_fraglimit", "30", CVAR_SERVERINFO, NULL);
	g_fraglog = gi.Cvar("g_fraglog", "0", 0, NULL);
	g_friendlyfire = gi.Cvar("g_friendlyfire", "1", CVAR_SERVERINFO, NULL);
	g_gameplay = gi.Cvar("g_gameplay", "0", CVAR_SERVERINFO, NULL);
	g_match = gi.Cvar("g_match", "0", CVAR_SERVERINFO, NULL);
	g_maxentities = gi.Cvar("g_maxentities", "1024", CVAR_LATCH, NULL);
	g_mysql = gi.Cvar("g_mysql", "0", 0, NULL);
	g_mysqldb = gi.Cvar("g_mysqldb", "quake2world", 0, NULL);
	g_mysqlhost = gi.Cvar("g_mysqlhost", "localhost", 0, NULL);
	g_mysqlpassword = gi.Cvar("g_mysqlpassword", "", 0, NULL);
	g_mysqluser = gi.Cvar("g_mysqluser", "quake2world", 0, NULL);
	g_playerprojectile = gi.Cvar("g_playerprojectile", "1", CVAR_SERVERINFO, NULL);
	g_randommap = gi.Cvar("g_randommap", "0", 0, NULL);
	g_roundlimit = gi.Cvar("g_roundlimit", "30", CVAR_SERVERINFO, NULL);
	g_rounds = gi.Cvar("g_rounds", "0", CVAR_SERVERINFO, NULL);
	g_spawnfarthest = gi.Cvar("g_spawnfarthest", "0", CVAR_SERVERINFO, NULL);
	g_teams = gi.Cvar("g_teams", "0", CVAR_SERVERINFO, NULL);
	g_timelimit = gi.Cvar("g_timelimit", "20", CVAR_SERVERINFO, NULL);
	g_voting = gi.Cvar("g_voting", "1", CVAR_SERVERINFO, "Activates voting");

	password = gi.Cvar("password", "", CVAR_USERINFO, NULL);

	sv_maxclients = gi.Cvar("sv_maxclients", "8", CVAR_SERVERINFO | CVAR_LATCH, NULL);
	dedicated = gi.Cvar("dedicated", "0", CVAR_NOSET, NULL);

	if(g_fraglog->value)
		gi.OpenFile("fraglog.log", &fraglog, FILE_APPEND);

	if(g_chatlog->value)
		gi.OpenFile("chatlog.log", &chatlog, FILE_APPEND);

#ifdef HAVE_MYSQL
	if(g_mysql->value){  //init database

		mysql = mysql_init(NULL);

		mysql_real_connect(mysql, g_mysqlhost->string,
				g_mysqluser->string, g_mysqlpassword->string,
				g_mysqldb->string, 0, NULL, 0
		);

		if(mysql != NULL)
			gi.Printf("    MySQL connection to %s/%s", g_mysqlhost->string,
					g_mysqluser->string);
	}
#endif

	G_ParseMaplist("maps.lst");

	G_InitItems();

	// initialize all entities for this game
	g_edicts = gi.TagMalloc(g_maxentities->value * sizeof(g_edicts[0]), TAG_GAME);
	globals.edicts = g_edicts;
	globals.max_edicts = g_maxentities->value;

	// initialize all clients for this game
	sv_maxclients->value = sv_maxclients->value;
	game.clients = gi.TagMalloc(sv_maxclients->value * sizeof(game.clients[0]), TAG_GAME);
	globals.num_edicts = sv_maxclients->value + 1;

	// set these to false to avoid spurious game restarts and alerts on init
	g_gameplay->modified = g_teams->modified = g_match->modified = g_rounds->modified =
		g_ctf->modified = g_cheats->modified = g_fraglimit->modified =
		g_roundlimit->modified = g_capturelimit->modified = g_timelimit->modified = false;

	gi.Printf("  Game initialized.\n");
}


/*
 *  Frees tags and closes fraglog.  This is called when the
 *  game is unloaded (complements G_Init).
 */
void G_Shutdown(void){
	gi.Printf("Game shutdown..\n");

	if(fraglog != NULL)
		gi.CloseFile(fraglog);  // close fraglog

	if(chatlog != NULL)
		gi.CloseFile(chatlog);  // and chatlog

#ifdef HAVE_MYSQL
	if(mysql != NULL)
		mysql_close(mysql);  // and db
#endif

	gi.FreeTags(TAG_LEVEL);
	gi.FreeTags(TAG_GAME);
}


/*
 * LoadGame
 *
 * Returns a pointer to the structure with all entry points
 * and global variables
 */
game_export_t *LoadGame(game_import_t *import){
	gi = *import;

	globals.apiversion = GAME_API_VERSION;
	globals.Init = G_Init;
	globals.Shutdown = G_Shutdown;
	globals.SpawnEntities = G_SpawnEntities;

	globals.ClientThink = P_Think;
	globals.ClientConnect = P_Connect;
	globals.ClientUserinfoChanged = P_UserinfoChanged;
	globals.ClientDisconnect = P_Disconnect;
	globals.ClientBegin = P_Begin;
	globals.ClientCommand = P_Command;

	globals.RunFrame = G_RunFrame;

	globals.edict_size = sizeof(edict_t);

	return &globals;
}

/*
 * Com_Printf
 *
 * Redefined here so functions in shared.c can link.
 */
void Com_Printf(const char *msg, ...){
	va_list	argptr;
	char text[1024];

	va_start(argptr, msg);
	vsprintf(text, msg, argptr);
	va_end(argptr);

	gi.Bprintf(PRINT_HIGH, "%s", text);
}
