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

#include <dirent.h>

#include "g_local.h"

game_import_t gi;
game_export_t ge;

g_locals_t g_locals;
g_level_t g_level;
g_spawn_temp_t st;

int means_of_death;

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
cvar_t *g_gravity;
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

g_team_t good, evil;

g_maplist_t g_maplist;

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

	g_level.votes[0] = g_level.votes[1] = g_level.votes[2] = 0;
	g_level.vote_cmd[0] = 0;

	g_level.votetime = 0;
}

/*
 * G_ResetItems
 *
 * Reset all items in the level based on gameplay, ctf, etc.
 */
static void G_ResetItems(void){
	edict_t *ent;
	int i;

	for(i = 1; i < ge.num_edicts; i++){  // reset items

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

			if(g_level.ctf){
				ent->svflags &= ~SVF_NOCLIENT;
				ent->solid = SOLID_TRIGGER;
				ent->next_think = g_level.time + 0.2;
			}
			else {
				ent->svflags |= SVF_NOCLIENT;
				ent->solid = SOLID_NOT;
				ent->next_think = 0;
			}
		}
		else {  // everything else honors gameplay

			if(g_level.gameplay){  // hide items
				ent->svflags |= SVF_NOCLIENT;
				ent->solid = SOLID_NOT;
				ent->next_think = 0.0;
			}
			else {  // or unhide them
				ent->svflags &= ~SVF_NOCLIENT;
				ent->solid = SOLID_TRIGGER;
				ent->next_think = g_level.time + 2.0 * gi.server_frame;
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
	edict_t *ent;
	g_client_t *cl;

	if(g_level.match_time)
		g_level.match_num++;

	if(g_level.round_time)
		g_level.round_num++;

	for(i = 0; i < sv_maxclients->value; i++){  // reset clients

		if(!g_edicts[i + 1].inuse)
			continue;

		ent = &g_edicts[i + 1];
		cl = ent->client;

		cl->locals.ready = false;  // back to warmup
		cl->locals.score = 0;
		cl->locals.captures = 0;

		if(teamz)  // reset teams
			cl->locals.team = NULL;

		// determine spectator or team affiliations

		if(g_level.match){
			if(cl->locals.match_num == g_level.match_num)
				cl->locals.spectator = false;
			else
				cl->locals.spectator = true;
		}

		else if(g_level.rounds){
			if(cl->locals.round_num == g_level.round_num)
				cl->locals.spectator = false;
			else
				cl->locals.spectator = true;
		}

		if(g_level.teams || g_level.ctf){
			
			if(!cl->locals.team){
				if(g_autojoin->value)
					G_AddClientToTeam(ent, G_SmallestTeam()->name);
				else
					cl->locals.spectator = true;
			}
		}

		P_Respawn(ent, false);
	}

	G_ResetItems();

	g_level.match_time = g_level.round_time = 0;
	good.score = evil.score = 0;
	good.captures = evil.captures = 0;

	gi.BroadcastPrint(PRINT_HIGH, "Game restarted\n");
	gi.Sound(&g_edicts[0], gi.SoundIndex("world/teleport"), ATTN_NONE);
}


/*
 * G_MuteClient
 */
static void G_MuteClient(char *name, qboolean mute){
	g_client_t *cl;

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

	if(g_level.intermission_time)
		return;  // already activated

	g_level.intermission_time = g_level.time;

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

	VectorCopy(ent->s.origin, g_level.intermission_origin);
	VectorCopy(ent->s.angles, g_level.intermission_angle);

	// move all clients to the intermission point
	for(i = 0; i < sv_maxclients->value; i++){

		client = g_edicts + 1 + i;

		if(!client->inuse)
			continue;

		P_MoveToIntermission(client);
	}

	// play a dramatic sound effect
	gi.PositionedSound(g_level.intermission_origin, g_edicts,
			gi.SoundIndex("weapons/bfg/hit"), ATTN_NORM);

	// stay on same level if not provided
	g_level.changemap = map && *map ? map : g_level.name;
}


/*
 * G_CheckVote
 */
static void G_CheckVote(void){
	int i, count = 0;

	if(!g_voting->value)
		return;

	if(g_level.votetime == 0)
		return;

	if(g_level.time - g_level.votetime > MAX_VOTE_TIME){
		gi.BroadcastPrint(PRINT_HIGH, "Vote \"%s\" expired\n", g_level.vote_cmd);
		G_ResetVote();
		return;
	}

	for(i = 0; i < sv_maxclients->value; i++){
		if(!g_edicts[i + 1].inuse)
			continue;
		count++;
	}

	if(g_level.votes[VOTE_YES] >= count * VOTE_MAJORITY){  // vote passed

		gi.BroadcastPrint(PRINT_HIGH, "Vote \"%s\" passed\n", g_level.vote_cmd);

		if(!strncmp(g_level.vote_cmd, "map ", 4)){  // special case for map
			G_BeginIntermission(g_level.vote_cmd + 4);
		}
		else if(!strcmp(g_level.vote_cmd, "restart")){  // and restart
			G_RestartGame(false);
		}
		else if(!strncmp(g_level.vote_cmd, "mute ", 5)){  // and mute
			G_MuteClient(g_level.vote_cmd + 5, true);
		}
		else if(!strncmp(g_level.vote_cmd, "unmute ", 7)){
			G_MuteClient(g_level.vote_cmd + 7, false);
		}
		else {  // general case, just execute the command
			gi.AddCommandString(g_level.vote_cmd);
		}
		G_ResetVote();
	} else if(g_level.votes[VOTE_NO] >= count * VOTE_MAJORITY){  // vote failed
		gi.BroadcastPrint(PRINT_HIGH, "Vote \"%s\" failed\n", g_level.vote_cmd);
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
	if(g_maplist.count > 0){

		if(g_randommap->value){  // random weighted selection
			g_maplist.index = g_maplist.weighted_index[rand() % MAPLIST_WEIGHT];
		}
		else { // incremental, so long as wieght is not 0

			i = g_maplist.index;

			while(true){
				g_maplist.index = (g_maplist.index + 1) % g_maplist.count;

				if(!g_maplist.maps[g_maplist.index].weight)
					continue;

				if(g_maplist.index == i)  // wrapped around, all weights were 0
					break;

				break;
			}
		}

		G_BeginIntermission(g_maplist.maps[g_maplist.index].name);
		return;
	}

	// or stay on current level
	G_BeginIntermission(g_level.name);
}


/*
 * G_CheckRoundStart
 */
static void G_CheckRoundStart(void){
	int i, g, e, clients;
	g_client_t *cl;

	if(!g_level.rounds)
		return;

	if(g_level.round_time)
		return;

	clients = g = e = 0;

	for(i = 0; i < sv_maxclients->value; i++){
		if(!g_edicts[i + 1].inuse)
			continue;

		cl = g_edicts[i + 1].client;

		if(cl->locals.spectator)
			continue;

		clients++;

		if(g_level.teams)
			cl->locals.team == &good ? g++ : e++;
	}

	if(clients < 2)  // need at least 2 clients to trigger countdown
		return;

	if(g_level.teams && (!g || !e))  // need at least 1 player per team
		return;

	if((int)g_level.teams == 2 && (g != e)){  // balanced teams required
		if(g_level.frame_num % 100 == 0)
			gi.BroadcastPrint(PRINT_HIGH, "Teams must be balanced for round to start\n");
		return;
	}

	gi.BroadcastPrint(PRINT_HIGH, "Round starting in 10 seconds..\n");
	g_level.round_time = g_level.time + 10.0;

	g_level.start_round = true;
}


/*
 * G_CheckRoundLimit
 */
static void G_CheckRoundLimit(){
	int i;
	edict_t *ent;
	g_client_t *cl;

	if(g_level.round_num >= g_level.round_limit){  // enforce roundlimit
		gi.BroadcastPrint(PRINT_HIGH, "Roundlimit hit\n");
		G_EndLevel();
		return;
	}

	// or attempt to re-join previously active players
	for(i = 0; i < sv_maxclients->value; i++){
		if(!g_edicts[i + 1].inuse)
			continue;

		ent = &g_edicts[i + 1];
		cl = ent->client;

		if(cl->locals.round_num != g_level.round_num)
			continue;  // they were intentionally spectating, skip them

		if(g_level.teams || g_level.ctf){  // rejoin a team
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
	g_client_t *cl;

	if(!g_level.rounds)
		return;

	if(!g_level.round_time || g_level.round_time > g_level.time)
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

		if(g_level.teams)
			cl->locals.team == &good ? g++ : e++;

		clients++;
	}

	if(clients == 0){  // corner case where everyone was fragged
		gi.BroadcastPrint(PRINT_HIGH, "Tie!\n");
		g_level.round_time = 0;
		G_CheckRoundLimit();
		return;
	}

	if(g_level.teams || g_level.ctf){  // teams rounds continue if each team has a player
		if(g > 0 && e > 0)
			return;
	}
	else if(clients > 1)  // ffa continues if two players are alive
		return;

	// allow enemy projectiles to expire before declaring a winner
	for(i = 0; i < ge.num_edicts; i++){
		if(!g_edicts[i + 1].inuse)
			continue;

		if(!g_edicts[i + 1].owner)
			continue;

		if(!(cl = g_edicts[i + 1].owner->client))
			continue;

		if(g_level.teams || g_level.ctf){
			if(cl->locals.team != winner->client->locals.team)
				return;
		}
		else {
			if(g_edicts[i + 1].owner != winner)
				return;
		}
	}

	// we have a winner
	gi.BroadcastPrint(PRINT_HIGH, "%s wins!\n", (g_level.teams || g_level.ctf ?
				winner->client->locals.team->name : winner->client->locals.netname));

	g_level.round_time = 0;

	G_CheckRoundLimit();
}


/*
 * G_CheckMatchEnd
 */
static void G_CheckMatchEnd(void){
	int i, g, e, clients;
	g_client_t *cl;

	if(!g_level.match)
		return;

	if(!g_level.match_time || g_level.match_time > g_level.time)
		return;  // no match currently running

	g = e = clients = 0;
	for(i = 0; i < sv_maxclients->value; i++){
		if(!g_edicts[i + 1].inuse)
			continue;

		cl = g_edicts[i + 1].client;

		if(cl->locals.spectator)
			continue;

		if(g_level.teams || g_level.ctf)
			cl->locals.team == &good ?  g++ : e++;

		clients++;
	}

	if(clients == 0){  // everyone left
		gi.BroadcastPrint(PRINT_HIGH, "No players left\n");
		g_level.match_time = 0;
		return;
	}

	if((g_level.teams || g_level.ctf) && (!g || !e)){
		gi.BroadcastPrint(PRINT_HIGH, "Not enough players left\n");
		g_level.match_time = 0;
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
	g_client_t *cl;

	if(g_level.intermission_time)
		return;

	// match mode, no match, or countdown underway
	g_level.warmup = g_level.match && (!g_level.match_time || g_level.match_time > g_level.time);

	// arena mode, no round, or countdown underway
	g_level.warmup |= g_level.rounds && (!g_level.round_time || g_level.round_time > g_level.time);

	if(g_level.start_match && g_level.time >= g_level.match_time){  // players have readied, begin match
		g_level.start_match = false;
		g_level.warmup = false;

		for(i = 0; i < sv_maxclients->value; i++){
			if(!g_edicts[i + 1].inuse)
				continue;
			P_Respawn(&g_edicts[i + 1], false);
		}

		gi.Sound(&g_edicts[0], gi.SoundIndex("world/teleport"), ATTN_NONE);
		gi.BroadcastPrint(PRINT_HIGH, "Match has started\n");
	}

	if(g_level.start_round && g_level.time >= g_level.round_time){  // pre-game expired, begin round
		g_level.start_round = false;
		g_level.warmup = false;

		for(i = 0; i < sv_maxclients->value; i++){
			if(!g_edicts[i + 1].inuse)
				continue;
			P_Respawn(&g_edicts[i + 1], false);
		}

		gi.Sound(&g_edicts[0], gi.SoundIndex("world/teleport"), ATTN_NONE);
		gi.BroadcastPrint(PRINT_HIGH, "Round has started\n");
	}

	seconds = g_level.time;

	if(g_level.rounds){
		if(g_level.round_time > g_level.time)  // round about to start, show pre-game countdown
			seconds = g_level.round_time - g_level.time;
		else if(g_level.round_time)
			seconds = g_level.time - g_level.round_time;  // round started, count up
		else seconds = -1;
	}
	else if(g_level.match){
		if(g_level.match_time > g_level.time)  // match about to start, show pre-game countdown
			seconds = g_level.match_time - g_level.time;
		else if(g_level.match_time){
			if(g_level.time_limit)  // count down to timelimit
				seconds = g_level.match_time + g_level.time_limit * 60 - g_level.time;
			else seconds = g_level.time - g_level.match_time;  // count up
		}
		else seconds = -1;
	}

	if(g_level.time_limit){  // check timelimit
		float t = g_level.time;

		if(g_level.match)  // for matches
			t = g_level.time - g_level.match_time;
		else if(g_level.rounds)  // and for rounds
			t = g_level.time - g_level.round_time;

		if(t >= g_level.time_limit * 60){
			gi.BroadcastPrint(PRINT_HIGH, "Timelimit hit\n");
			G_EndLevel();
			return;
		}
		seconds = g_level.time_limit * 60 - t;  // count down
	}

	if(g_level.frame_num % gi.server_hz == 0)  // send time updates once per second
		gi.Configstring(CS_TIME, (g_level.warmup ? "Warmup" : G_FormatTime(seconds)));

	if(!g_level.ctf && g_level.frag_limit){  // check fraglimit

		if(g_level.teams){  // check team scores
 			if(good.score >= g_level.frag_limit || evil.score >= g_level.frag_limit){
				gi.BroadcastPrint(PRINT_HIGH, "Fraglimit hit\n");
				G_EndLevel();
				return;
			}
		}
		else {  // or individual scores
			for(i = 0; i < sv_maxclients->value; i++){
				cl = g_locals.clients + i;
				if(!g_edicts[i + 1].inuse)
					continue;

				if(cl->locals.score >= g_level.frag_limit){
					gi.BroadcastPrint(PRINT_HIGH, "Fraglimit hit\n");
					G_EndLevel();
					return;
				}
			}
		}
	}

	if(g_level.ctf && g_level.capture_limit){  // check capture limit

		if(good.captures >= g_level.capture_limit || evil.captures >= g_level.capture_limit){
			gi.BroadcastPrint(PRINT_HIGH, "Capturelimit hit\n");
			G_EndLevel();
			return;
		}
	}

	if(g_gameplay->modified){  // change gameplay, fix items, respawn clients
		g_gameplay->modified = false;
		g_level.gameplay = G_GameplayByName(g_gameplay->string);

		G_RestartGame(false);  // reset all clients

		gi.BroadcastPrint(PRINT_HIGH, "Gameplay has changed to %s\n",
				G_GameplayName(g_level.gameplay));
	}

	if(g_gravity->modified){  // send gravity config string
		g_gravity->modified = false;

		gi.Configstring(CS_GRAVITY, va("%d", (int)g_gravity->value));
		g_level.gravity = g_gravity->value;
	}

	if(g_teams->modified){  // reset teams, scores
		g_teams->modified = false;
		g_level.teams = g_teams->value;

		gi.BroadcastPrint(PRINT_HIGH, "Teams have been %s\n",
				g_level.teams ? "enabled" : "disabled");

		G_RestartGame(true);
	}

	if(g_ctf->modified){  // reset teams, scores
		g_ctf->modified = false;
		g_level.ctf = g_ctf->value;

		gi.BroadcastPrint(PRINT_HIGH, "CTF has been %s\n",
				g_level.ctf ? "enabled" : "disabled");

		G_RestartGame(true);
	}

	if(g_match->modified){  // reset scores
		g_match->modified = false;
		g_level.match = g_match->value;

		g_level.warmup = g_level.match;  // toggle warmup

		gi.BroadcastPrint(PRINT_HIGH, "Match has been %s\n",
				g_level.match ? "enabled" : "disabled");

		G_RestartGame(false);
	}

	if(g_rounds->modified){  // reset scores
		g_rounds->modified = false;
		g_level.rounds = g_rounds->value;

		g_level.warmup = g_level.rounds;  // toggle warmup

		gi.BroadcastPrint(PRINT_HIGH, "Rounds have been %s\n",
				g_level.rounds ? "enabled" : "disabled");

		G_RestartGame(false);
	}

	if(g_cheats->modified){  // notify when cheats changes
		g_cheats->modified = false;

		gi.BroadcastPrint(PRINT_HIGH, "Cheats have been %s\n",
				g_cheats->value ? "enabled" : "disabled");
	}

	if(g_fraglimit->modified){
		g_fraglimit->modified = false;
		g_level.frag_limit = g_fraglimit->value;

		gi.BroadcastPrint(PRINT_HIGH, "Fraglimit has been changed to %d\n",
				g_level.frag_limit);
	}

	if(g_roundlimit->modified){
		g_roundlimit->modified = false;
		g_level.round_limit = g_roundlimit->value;

		gi.BroadcastPrint(PRINT_HIGH, "Roundlimit has been changed to %d\n",
				g_level.round_limit);
	}

	if(g_capturelimit->modified){
		g_capturelimit->modified = false;
		g_level.capture_limit = g_capturelimit->value;

		gi.BroadcastPrint(PRINT_HIGH, "Capturelimit has been changed to %d\n",
				g_level.capture_limit);
	}

	if(g_timelimit->modified){
		g_timelimit->modified = false;
		g_level.time_limit = g_timelimit->value;

		gi.BroadcastPrint(PRINT_HIGH, "Timelimit has been changed to %d\n",
				(int)g_level.time_limit);
	}
}


/*
 * G_ExitLevel
 */
static void G_ExitLevel(void){

	gi.AddCommandString(va("map %s\n", g_level.changemap));

	g_level.changemap = NULL;
	g_level.intermission_time = 0;

	P_EndServerFrames();
}


#define INTERMISSION 10.0  // intermission duration

/*
 * G_RunFrame
 *
 * Advances the world by 0.1 seconds
 */
static void G_RunFrame(void){
	int i;
	edict_t *ent;

	g_level.frame_num++;
	g_level.time = g_level.frame_num * gi.server_frame;

	// check for level change after running intermission
	if(g_level.intermission_time){
		if(g_level.time > g_level.intermission_time + INTERMISSION){
			G_ExitLevel();
			return;
		}
	}

	// treat each object in turn
	// even the world gets a chance to think
	ent = &g_edicts[0];
	for(i = 0; i < ge.num_edicts; i++, ent++){

		if(!ent->inuse)
			continue;

		g_level.current_entity = ent;

		if(!(ent->s.effects & EF_LIGHTNING))  // update old origin for lerps
			VectorCopy(ent->s.origin, ent->s.old_origin);

		if(ent->ground_entity){
			// if the ground entity moved, make sure we are still on it
			if(ent->ground_entity->linkcount != ent->ground_entity_linkcount)
				ent->ground_entity = NULL;
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

	memset(&g_maplist, 0, sizeof(g_maplist));

	for(i = 0; i < MAX_MAPLIST_ELTS; i++){
		g_maplist_elt_t *map = &g_maplist.maps[i];
		map->gravity = map->gameplay = -1;
		map->teams = map->ctf = map->match = -1;
		map->frag_limit = map->round_limit = map->capture_limit = -1;
		map->time_limit = -1;
		map->weight = 1.0;
	}
}


/*
 * G_ParseMaplist
 *
 * Populates a maplist_t from a text file.  See default/maps.lst
 */
static void G_ParseMaplist(const char *file_name){
	void *buf;
	const char *buffer;
	int i, j, k, l;
	const char *c;
	qboolean inmap;
	g_maplist_elt_t *map;

	G_InitMaplist();

	if(!file_name || !file_name[0]){
		g_maplist.count = 0;
		return;
	}

	if((i = gi.LoadFile(file_name, &buf)) < 1){
		gi.Print("Couldn't open %s\n", file_name);
		g_maplist.count = 0;
		return;
	}

	buffer = (char *)buf;

	i = 0;
	inmap = false;
	while(true){

		c = Com_Parse(&buffer);

		if(*c == '\0')
			break;

		if(*c == '{')
			inmap = true;

		if(!inmap)  // skip any whitespace between maps
			continue;

		map = &g_maplist.maps[i];

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
			map->frag_limit = atoi(Com_Parse(&buffer));
			continue;
		}

		if(!strcmp(c, "roundlimit")){
			map->round_limit = atoi(Com_Parse(&buffer));
			continue;
		}

		if(!strcmp(c, "capturelimit")){
			map->capture_limit = atoi(Com_Parse(&buffer));
			continue;
		}

		if(!strcmp(c, "timelimit")){
			map->time_limit = atof(Com_Parse(&buffer));
			continue;
		}

		if(!strcmp(c, "give")){
			strncpy(map->give, Com_Parse(&buffer), 1023);
			continue;
		}

		if(!strcmp(c, "music")){
			strncpy(map->music, Com_Parse(&buffer), 1023);
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
					"music: %s\n"
					"weight: %f\n",
					map->name, map->title, map->sky, map->weather, map->gravity,
					map->gameplay, map->teams, map->ctf, map->match, map->rounds,
					map->fraglimit, map->roundlimit, map->capturelimit,
					map->timelimit, map->give, map->music, map->weight);*/

			// accumulate total weight
			g_maplist.total_weight += map->weight;

			if(++i == MAX_MAPLIST_ELTS)
				break;
		}
	}

	g_maplist.count = i;
	g_maplist.index = 0;

	gi.TagFree(buf);

	// thou shalt not divide by zero
	if(!g_maplist.total_weight)
		g_maplist.total_weight = 1.0;

	// compute the weighted index list
	for(i = 0, l = 0; i < g_maplist.count; i++){

		map = &g_maplist.maps[i];
		k = (map->weight / g_maplist.total_weight) * MAPLIST_WEIGHT;

		for(j = 0; j < k; j++)
			g_maplist.weighted_index[l++] = i;
	}
}


/*
 * G_Init
 *
 * This will be called when the game module is first loaded.
 */
void G_Init(void){
	gi.Print("  Game initialization..\n");

	gi.Cvar("gamename", GAMEVERSION , CVAR_SERVER_INFO | CVAR_NOSET, NULL);
	gi.Cvar("gamedate", __DATE__ , CVAR_SERVER_INFO | CVAR_NOSET, NULL);

	g_autojoin = gi.Cvar("g_autojoin", "1", CVAR_SERVER_INFO, NULL);
	g_capturelimit = gi.Cvar("g_capturelimit", "8", CVAR_SERVER_INFO, NULL);
	g_chatlog = gi.Cvar("g_chatlog", "0", 0, NULL);
	g_cheats = gi.Cvar("g_cheats", "0", CVAR_SERVER_INFO, NULL);
	g_ctf = gi.Cvar("g_ctf", "0", CVAR_SERVER_INFO, NULL);
	g_fraglimit = gi.Cvar("g_fraglimit", "30", CVAR_SERVER_INFO, NULL);
	g_fraglog = gi.Cvar("g_fraglog", "0", 0, NULL);
	g_friendlyfire = gi.Cvar("g_friendlyfire", "1", CVAR_SERVER_INFO, NULL);
	g_gameplay = gi.Cvar("g_gameplay", "0", CVAR_SERVER_INFO, NULL);
	g_gravity = gi.Cvar("g_gravity", "800", CVAR_SERVER_INFO, NULL);
	g_match = gi.Cvar("g_match", "0", CVAR_SERVER_INFO, NULL);
	g_maxentities = gi.Cvar("g_maxentities", "1024", CVAR_LATCH, NULL);
	g_mysql = gi.Cvar("g_mysql", "0", 0, NULL);
	g_mysqldb = gi.Cvar("g_mysqldb", "quake2world", 0, NULL);
	g_mysqlhost = gi.Cvar("g_mysqlhost", "localhost", 0, NULL);
	g_mysqlpassword = gi.Cvar("g_mysqlpassword", "", 0, NULL);
	g_mysqluser = gi.Cvar("g_mysqluser", "quake2world", 0, NULL);
	g_playerprojectile = gi.Cvar("g_playerprojectile", "1", CVAR_SERVER_INFO, NULL);
	g_randommap = gi.Cvar("g_randommap", "0", 0, NULL);
	g_roundlimit = gi.Cvar("g_roundlimit", "30", CVAR_SERVER_INFO, NULL);
	g_rounds = gi.Cvar("g_rounds", "0", CVAR_SERVER_INFO, NULL);
	g_spawnfarthest = gi.Cvar("g_spawnfarthest", "0", CVAR_SERVER_INFO, NULL);
	g_teams = gi.Cvar("g_teams", "0", CVAR_SERVER_INFO, NULL);
	g_timelimit = gi.Cvar("g_timelimit", "20", CVAR_SERVER_INFO, NULL);
	g_voting = gi.Cvar("g_voting", "1", CVAR_SERVER_INFO, "Activates voting");

	password = gi.Cvar("password", "", CVAR_USER_INFO, NULL);

	sv_maxclients = gi.Cvar("sv_maxclients", "8", CVAR_SERVER_INFO | CVAR_LATCH, NULL);
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
			gi.Print("    MySQL connection to %s/%s", g_mysqlhost->string,
					g_mysqluser->string);
	}
#endif

	G_ParseMaplist("maps.lst");

	G_InitItems();

	// initialize all entities for this game
	g_edicts = gi.TagMalloc(g_maxentities->value * sizeof(g_edicts[0]), TAG_GAME);
	ge.edicts = g_edicts;
	ge.max_edicts = g_maxentities->value;

	// initialize all clients for this game
	sv_maxclients->value = sv_maxclients->value;
	g_locals.clients = gi.TagMalloc(sv_maxclients->value * sizeof(g_locals.clients[0]), TAG_GAME);
	ge.num_edicts = sv_maxclients->value + 1;

	// set these to false to avoid spurious game restarts and alerts on init
	g_gameplay->modified = g_teams->modified = g_match->modified = g_rounds->modified =
		g_ctf->modified = g_cheats->modified = g_fraglimit->modified =
		g_roundlimit->modified = g_capturelimit->modified = g_timelimit->modified = false;

	gi.Print("  Game initialized.\n");
}


/*
 *  Frees tags and closes fraglog.  This is called when the
 *  game is unloaded (complements G_Init).
 */
void G_Shutdown(void){
	gi.Print("Game shutdown..\n");

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

	ge.apiversion = GAME_API_VERSION;
	ge.Init = G_Init;
	ge.Shutdown = G_Shutdown;
	ge.SpawnEntities = G_SpawnEntities;

	ge.ClientThink = P_Think;
	ge.ClientConnect = P_Connect;
	ge.ClientUserinfoChanged = P_UserinfoChanged;
	ge.ClientDisconnect = P_Disconnect;
	ge.ClientBegin = P_Begin;
	ge.ClientCommand = P_Command;

	ge.RunFrame = G_RunFrame;

	ge.edict_size = sizeof(edict_t);

	return &ge;
}

/*
 * Com_Printf
 *
 * Redefined here so functions in shared.c can link.
 */
void Com_Print(const char *msg, ...){
	va_list	args;
	char text[1024];

	va_start(args, msg);
	vsprintf(text, msg, args);
	va_end(args);

	gi.BroadcastPrint(PRINT_HIGH, "%s", text);
}
