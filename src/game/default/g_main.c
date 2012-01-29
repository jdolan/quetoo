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

g_import_t gi;
g_export_t ge;

g_game_t g_game;
g_level_t g_level;

unsigned int means_of_death;

cvar_t *g_auto_join;
cvar_t *g_capture_limit;
cvar_t *g_chat_log;
cvar_t *g_cheats;
cvar_t *g_ctf;
cvar_t *g_frag_limit;
cvar_t *g_frag_log;
cvar_t *g_friendly_fire;
cvar_t *g_gameplay;
cvar_t *g_gravity;
cvar_t *g_match;
cvar_t *g_max_entities;
cvar_t *g_mysql;
cvar_t *g_mysql_db;
cvar_t *g_mysql_host;
cvar_t *g_mysql_password;
cvar_t *g_mysql_user;
cvar_t *g_player_projectile;
cvar_t *g_random_map;
cvar_t *g_round_limit;
cvar_t *g_rounds;
cvar_t *g_spawn_farthest;
cvar_t *g_teams;
cvar_t *g_time_limit;
cvar_t *g_voting;

cvar_t *password;

cvar_t *sv_max_clients;
cvar_t *dedicated;

g_team_t g_team_good, g_team_evil;

g_map_list_t g_map_list;

#ifdef HAVE_MYSQL
MYSQL *mysql;
char sql[512];
#endif

FILE *frag_log, *chat_log, *f;

/*
 * G_ResetTeams
 */
void G_ResetTeams(void) {

	memset(&g_team_good, 0, sizeof(g_team_good));
	memset(&g_team_evil, 0, sizeof(g_team_evil));

	strcpy(g_team_good.name, "Good");
	gi.ConfigString(CS_TEAM_GOOD, g_team_good.name);

	strcpy(g_team_evil.name, "Evil");
	gi.ConfigString(CS_TEAM_EVIL, g_team_evil.name);

	strcpy(g_team_good.skin, "qforcer/blue");
	strcpy(g_team_evil.skin, "qforcer/red");
}

/*
 * G_ResetVote
 */
void G_ResetVote(void) {
	int i;

	for (i = 0; i < sv_max_clients->integer; i++) { //reset vote flags
		if (!g_game.edicts[i + 1].in_use)
			continue;
		g_game.edicts[i + 1].client->persistent.vote = VOTE_NO_OP;
	}

	gi.ConfigString(CS_VOTE, NULL);

	g_level.votes[0] = g_level.votes[1] = g_level.votes[2] = 0;
	g_level.vote_cmd[0] = 0;

	g_level.vote_time = 0;
}

/*
 * G_ResetItems
 *
 * Reset all items in the level based on gameplay, ctf, etc.
 */
static void G_ResetItems(void) {
	g_edict_t *ent;
	unsigned int i;

	for (i = 1; i < ge.num_edicts; i++) { // reset items

		ent = &g_game.edicts[i];

		if (!ent->in_use)
			continue;

		if (!ent->item)
			continue;

		if (ent->spawn_flags & SF_ITEM_DROPPED) { // free dropped ones
			G_FreeEdict(ent);
			continue;
		}

		if (ent->item->type == ITEM_FLAG) { // flags only appear for ctf

			if (g_level.ctf) {
				ent->sv_flags &= ~SVF_NO_CLIENT;
				ent->solid = SOLID_TRIGGER;
				ent->next_think = g_level.time + 0.2;
			} else {
				ent->sv_flags |= SVF_NO_CLIENT;
				ent->solid = SOLID_NOT;
				ent->next_think = 0;
			}
		} else { // everything else honors gameplay

			if (g_level.gameplay) { // hide items
				ent->sv_flags |= SVF_NO_CLIENT;
				ent->solid = SOLID_NOT;
				ent->next_think = 0.0;
			} else { // or unhide them
				ent->sv_flags &= ~SVF_NO_CLIENT;
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
static void G_RestartGame(boolean_t teamz) {
	int i;
	g_edict_t *ent;
	g_client_t *cl;

	if (g_level.match_time)
		g_level.match_num++;

	if (g_level.round_time)
		g_level.round_num++;

	for (i = 0; i < sv_max_clients->integer; i++) { // reset clients

		if (!g_game.edicts[i + 1].in_use)
			continue;

		ent = &g_game.edicts[i + 1];
		cl = ent->client;

		cl->persistent.ready = false; // back to warmup
		cl->persistent.score = 0;
		cl->persistent.captures = 0;

		if (teamz) // reset teams
			cl->persistent.team = NULL;

		// determine spectator or team affiliations

		if (g_level.match) {
			if (cl->persistent.match_num == g_level.match_num)
				cl->persistent.spectator = false;
			else
				cl->persistent.spectator = true;
		}

		else if (g_level.rounds) {
			if (cl->persistent.round_num == g_level.round_num)
				cl->persistent.spectator = false;
			else
				cl->persistent.spectator = true;
		}

		if (g_level.teams || g_level.ctf) {

			if (!cl->persistent.team) {
				if (g_auto_join->value)
					G_AddClientToTeam(ent, G_SmallestTeam()->name);
				else
					cl->persistent.spectator = true;
			}
		}

		G_ClientRespawn(ent, false);
	}

	G_ResetItems();

	g_level.match_time = g_level.round_time = 0;
	g_team_good.score = g_team_evil.score = 0;
	g_team_good.captures = g_team_evil.captures = 0;

	gi.BroadcastPrint(PRINT_HIGH, "Game restarted\n");
	gi.Sound(&g_game.edicts[0], gi.SoundIndex("world/teleport"), ATTN_NONE);
}

/*
 * G_MuteClient
 */
static void G_MuteClient(char *name, boolean_t mute) {
	g_client_t *cl;

	if (!(cl = G_ClientByName(name)))
		return;

	cl->muted = mute;
}

/*
 * G_BeginIntermission
 */
static void G_BeginIntermission(const char *map) {
	int i;
	g_edict_t *ent, *client;

	if (g_level.intermission_time)
		return; // already activated

	g_level.intermission_time = g_level.time;

	// respawn any dead clients
	for (i = 0; i < sv_max_clients->integer; i++) {

		client = g_game.edicts + 1 + i;

		if (!client->in_use)
			continue;

		if (client->health <= 0)
			G_ClientRespawn(client, false);
	}

	// find an intermission spot
	ent = G_Find(NULL, FOFS(class_name), "info_player_intermission");
	if (!ent) { // map does not have an intermission point
		ent = G_Find(NULL, FOFS(class_name), "info_player_start");
		if (!ent)
			ent = G_Find(NULL, FOFS(class_name), "info_player_deathmatch");
	}

	VectorCopy(ent->s.origin, g_level.intermission_origin);
	VectorCopy(ent->s.angles, g_level.intermission_angle);

	// move all clients to the intermission point
	for (i = 0; i < sv_max_clients->integer; i++) {

		client = g_game.edicts + 1 + i;

		if (!client->in_use)
			continue;

		G_ClientToIntermission(client);
	}

	// play a dramatic sound effect
	gi.PositionedSound(g_level.intermission_origin, g_game.edicts,
			gi.SoundIndex("weapons/bfg/hit"), ATTN_NORM);

	// stay on same level if not provided
	g_level.changemap = map && *map ? map : g_level.name;
}

/*
 * G_CheckVote
 */
static void G_CheckVote(void) {
	int i, count = 0;

	if (!g_voting->value)
		return;

	if (g_level.vote_time == 0)
		return;

	if (g_level.time - g_level.vote_time > MAX_VOTE_TIME) {
		gi.BroadcastPrint(PRINT_HIGH, "Vote \"%s\" expired\n", g_level.vote_cmd);
		G_ResetVote();
		return;
	}

	for (i = 0; i < sv_max_clients->integer; i++) {
		if (!g_game.edicts[i + 1].in_use)
			continue;
		count++;
	}

	if (g_level.votes[VOTE_YES] >= count * VOTE_MAJORITY) { // vote passed

		gi.BroadcastPrint(PRINT_HIGH, "Vote \"%s\" passed\n", g_level.vote_cmd);

		if (!strncmp(g_level.vote_cmd, "map ", 4)) { // special case for map
			G_BeginIntermission(g_level.vote_cmd + 4);
		} else if (!strcmp(g_level.vote_cmd, "restart")) { // and restart
			G_RestartGame(false);
		} else if (!strncmp(g_level.vote_cmd, "mute ", 5)) { // and mute
			G_MuteClient(g_level.vote_cmd + 5, true);
		} else if (!strncmp(g_level.vote_cmd, "unmute ", 7)) {
			G_MuteClient(g_level.vote_cmd + 7, false);
		} else { // general case, just execute the command
			gi.AddCommandString(g_level.vote_cmd);
		}
		G_ResetVote();
	} else if (g_level.votes[VOTE_NO] >= count * VOTE_MAJORITY) { // vote failed
		gi.BroadcastPrint(PRINT_HIGH, "Vote \"%s\" failed\n", g_level.vote_cmd);
		G_ResetVote();
	}
}

/*
 * G_EndLevel
 *
 * The time limit, frag limit, etc.. has been exceeded.
 */
static void G_EndLevel(void) {
	unsigned int i;

	// try maplist
	if (g_map_list.count > 0) {

		if (g_random_map->value) { // random weighted selection
			g_map_list.index = g_map_list.weighted_index[rand()
					% MAP_LIST_WEIGHT];
		} else { // incremental, so long as wieght is not 0

			i = g_map_list.index;

			while (true) {
				g_map_list.index = (g_map_list.index + 1) % g_map_list.count;

				if (!g_map_list.maps[g_map_list.index].weight)
					continue;

				if (g_map_list.index == i) // wrapped around, all weights were 0
					break;

				break;
			}
		}

		G_BeginIntermission(g_map_list.maps[g_map_list.index].name);
		return;
	}

	// or stay on current level
	G_BeginIntermission(g_level.name);
}

/*
 * G_CheckRoundStart
 */
static void G_CheckRoundStart(void) {
	int i, g, e, clients;
	g_client_t *cl;

	if (!g_level.rounds)
		return;

	if (g_level.round_time)
		return;

	clients = g = e = 0;

	for (i = 0; i < sv_max_clients->integer; i++) {
		if (!g_game.edicts[i + 1].in_use)
			continue;

		cl = g_game.edicts[i + 1].client;

		if (cl->persistent.spectator)
			continue;

		clients++;

		if (g_level.teams)
			cl->persistent.team == &g_team_good ? g++ : e++;
	}

	if (clients < 2) // need at least 2 clients to trigger countdown
		return;

	if (g_level.teams && (!g || !e)) // need at least 1 player per team
		return;

	if ((int) g_level.teams == 2 && (g != e)) { // balanced teams required
		if (g_level.frame_num % 100 == 0)
			gi.BroadcastPrint(PRINT_HIGH,
					"Teams must be balanced for round to start\n");
		return;
	}

	gi.BroadcastPrint(PRINT_HIGH, "Round starting in 10 seconds...\n");
	g_level.round_time = g_level.time + 10.0;

	g_level.start_round = true;
}

/*
 * G_CheckRoundLimit
 */
static void G_CheckRoundLimit() {
	int i;
	g_edict_t *ent;
	g_client_t *cl;

	if (g_level.round_num >= (unsigned int) g_level.round_limit) { // enforce round_limit
		gi.BroadcastPrint(PRINT_HIGH, "Roundlimit hit\n");
		G_EndLevel();
		return;
	}

	// or attempt to re-join previously active players
	for (i = 0; i < sv_max_clients->integer; i++) {
		if (!g_game.edicts[i + 1].in_use)
			continue;

		ent = &g_game.edicts[i + 1];
		cl = ent->client;

		if (cl->persistent.round_num != g_level.round_num)
			continue; // they were intentionally spectating, skip them

		if (g_level.teams || g_level.ctf) { // rejoin a team
			if (cl->persistent.team)
				G_AddClientToTeam(ent, cl->persistent.team->name);
			else
				G_AddClientToTeam(ent, G_SmallestTeam()->name);
		} else
			// just rejoin the game
			cl->persistent.spectator = false;

		G_ClientRespawn(ent, false);
	}
}

/*
 * G_CheckRoundEnd
 */
static void G_CheckRoundEnd(void) {
	unsigned int i, g, e, clients;
	int j;
	g_edict_t *winner;
	g_client_t *cl;

	if (!g_level.rounds)
		return;

	if (!g_level.round_time || g_level.round_time > g_level.time)
		return; // no round currently running

	winner = NULL;
	g = e = clients = 0;
	for (j = 0; j < sv_max_clients->integer; j++) {
		if (!g_game.edicts[j + 1].in_use)
			continue;

		cl = g_game.edicts[j + 1].client;

		if (cl->persistent.spectator) // true spectator, or dead
			continue;

		winner = &g_game.edicts[j + 1];

		if (g_level.teams)
			cl->persistent.team == &g_team_good ? g++ : e++;

		clients++;
	}

	if (clients == 0) { // corner case where everyone was fragged
		gi.BroadcastPrint(PRINT_HIGH, "Tie!\n");
		g_level.round_time = 0;
		G_CheckRoundLimit();
		return;
	}

	if (g_level.teams || g_level.ctf) { // teams rounds continue if each team has a player
		if (g > 0 && e > 0)
			return;
	} else if (clients > 1) // ffa continues if two players are alive
		return;

	// allow enemy projectiles to expire before declaring a winner
	for (i = 0; i < ge.num_edicts; i++) {
		if (!g_game.edicts[i + 1].in_use)
			continue;

		if (!g_game.edicts[i + 1].owner)
			continue;

		if (!(cl = g_game.edicts[i + 1].owner->client))
			continue;

		if (g_level.teams || g_level.ctf) {
			if (cl->persistent.team != winner->client->persistent.team)
				return;
		} else {
			if (g_game.edicts[i + 1].owner != winner)
				return;
		}
	}

	// we have a winner
	gi.BroadcastPrint(
			PRINT_HIGH,
			"%s wins!\n",
			(g_level.teams || g_level.ctf ? winner->client->persistent.team->name
					: winner->client->persistent.net_name));

	g_level.round_time = 0;

	G_CheckRoundLimit();
}

/*
 * G_CheckMatchEnd
 */
static void G_CheckMatchEnd(void) {
	int i, g, e, clients;
	g_client_t *cl;

	if (!g_level.match)
		return;

	if (!g_level.match_time || g_level.match_time > g_level.time)
		return; // no match currently running

	g = e = clients = 0;
	for (i = 0; i < sv_max_clients->integer; i++) {
		if (!g_game.edicts[i + 1].in_use)
			continue;

		cl = g_game.edicts[i + 1].client;

		if (cl->persistent.spectator)
			continue;

		if (g_level.teams || g_level.ctf)
			cl->persistent.team == &g_team_good ? g++ : e++;

		clients++;
	}

	if (clients == 0) { // everyone left
		gi.BroadcastPrint(PRINT_HIGH, "No players left\n");
		g_level.match_time = 0;
		return;
	}

	if ((g_level.teams || g_level.ctf) && (!g || !e)) {
		gi.BroadcastPrint(PRINT_HIGH, "Not enough players left\n");
		g_level.match_time = 0;
		return;
	}
}

/*
 * G_FormatTime
 */
static char *G_FormatTime(int secs) {
	static char formatted_time[16];
	static int last_secs = 999999;
	int i;
	const int m = secs / 60;
	const int s = secs % 60;

	snprintf(formatted_time, sizeof(formatted_time), " %2d:%02d", m, s);

	// highlight for countdowns
	if (m == 0 && s < 30 && secs < last_secs && (s & 1)) {
		for (i = 0; i < 6; i++)
			formatted_time[i] += 128;
	}

	last_secs = secs;

	return formatted_time;
}

/*
 * G_CheckRules
 */
static void G_CheckRules(void) {
	int i, seconds;
	g_client_t *cl;

	if (g_level.intermission_time)
		return;

	// match mode, no match, or countdown underway
	g_level.warmup = g_level.match && (!g_level.match_time
			|| g_level.match_time > g_level.time);

	// arena mode, no round, or countdown underway
	g_level.warmup |= g_level.rounds && (!g_level.round_time
			|| g_level.round_time > g_level.time);

	if (g_level.start_match && g_level.time >= g_level.match_time) { // players have readied, begin match
		g_level.start_match = false;
		g_level.warmup = false;

		for (i = 0; i < sv_max_clients->integer; i++) {
			if (!g_game.edicts[i + 1].in_use)
				continue;
			G_ClientRespawn(&g_game.edicts[i + 1], false);
		}

		gi.Sound(&g_game.edicts[0], gi.SoundIndex("world/teleport"), ATTN_NONE);
		gi.BroadcastPrint(PRINT_HIGH, "Match has started\n");
	}

	if (g_level.start_round && g_level.time >= g_level.round_time) { // pre-game expired, begin round
		g_level.start_round = false;
		g_level.warmup = false;

		for (i = 0; i < sv_max_clients->integer; i++) {
			if (!g_game.edicts[i + 1].in_use)
				continue;
			G_ClientRespawn(&g_game.edicts[i + 1], false);
		}

		gi.Sound(&g_game.edicts[0], gi.SoundIndex("world/teleport"), ATTN_NONE);
		gi.BroadcastPrint(PRINT_HIGH, "Round has started\n");
	}

	seconds = g_level.time;

	if (g_level.rounds) {
		if (g_level.round_time > g_level.time) // round about to start, show pre-game countdown
			seconds = g_level.round_time - g_level.time;
		else if (g_level.round_time)
			seconds = g_level.time - g_level.round_time; // round started, count up
		else
			seconds = -1;
	} else if (g_level.match) {
		if (g_level.match_time > g_level.time) // match about to start, show pre-game countdown
			seconds = g_level.match_time - g_level.time;
		else if (g_level.match_time) {
			if (g_level.time_limit) // count down to time_limit
				seconds = g_level.match_time + g_level.time_limit * 60
						- g_level.time;
			else
				seconds = g_level.time - g_level.match_time; // count up
		} else
			seconds = -1;
	}

	if (g_level.time_limit) { // check time_limit
		float t = g_level.time;

		if (g_level.match) // for matches
			t = g_level.time - g_level.match_time;
		else if (g_level.rounds) // and for rounds
			t = g_level.time - g_level.round_time;

		if (t >= g_level.time_limit * 60) {
			gi.BroadcastPrint(PRINT_HIGH, "Timelimit hit\n");
			G_EndLevel();
			return;
		}
		seconds = g_level.time_limit * 60 - t; // count down
	}

	if (g_level.frame_num % gi.frame_rate == 0) // send time updates once per second
		gi.ConfigString(CS_TIME,
				(g_level.warmup ? "Warmup" : G_FormatTime(seconds)));

	if (!g_level.ctf && g_level.frag_limit) { // check frag_limit

		if (g_level.teams) { // check team scores
			if (g_team_good.score >= g_level.frag_limit ||
					g_team_evil.score >= g_level.frag_limit) {
				gi.BroadcastPrint(PRINT_HIGH, "Fraglimit hit\n");
				G_EndLevel();
				return;
			}
		} else { // or individual scores
			for (i = 0; i < sv_max_clients->integer; i++) {
				cl = g_game.clients + i;
				if (!g_game.edicts[i + 1].in_use)
					continue;

				if (cl->persistent.score >= g_level.frag_limit) {
					gi.BroadcastPrint(PRINT_HIGH, "Fraglimit hit\n");
					G_EndLevel();
					return;
				}
			}
		}
	}

	if (g_level.ctf && g_level.capture_limit) { // check capture limit

		if (g_team_good.captures >= g_level.capture_limit || g_team_evil.captures
				>= g_level.capture_limit) {
			gi.BroadcastPrint(PRINT_HIGH, "Capturelimit hit\n");
			G_EndLevel();
			return;
		}
	}

	if (g_gameplay->modified) { // change gameplay, fix items, respawn clients
		g_gameplay->modified = false;

		g_level.gameplay = G_GameplayByName(g_gameplay->string);
		gi.ConfigString(CS_GAMEPLAY, va("%d", g_level.gameplay));

		G_RestartGame(false); // reset all clients

		gi.BroadcastPrint(PRINT_HIGH, "Gameplay has changed to %s\n",
				G_GameplayName(g_level.gameplay));
	}

	if (g_gravity->modified) { // send gravity config string
		g_gravity->modified = false;

		g_level.gravity = g_gravity->integer;
		gi.ConfigString(CS_GRAVITY, va("%d", g_level.gravity));
	}

	if (g_teams->modified) { // reset teams, scores
		g_teams->modified = false;

		g_level.teams = g_teams->integer;
		gi.ConfigString(CS_TEAMS, va("%d", g_level.teams));

		gi.BroadcastPrint(PRINT_HIGH, "Teams have been %s\n",
				g_level.teams ? "enabled" : "disabled");

		G_RestartGame(true);
	}

	if (g_ctf->modified) { // reset teams, scores
		g_ctf->modified = false;

		g_level.ctf = g_ctf->integer;
		gi.ConfigString(CS_CTF, va("%d", g_level.ctf));

		gi.BroadcastPrint(PRINT_HIGH, "CTF has been %s\n",
				g_level.ctf ? "enabled" : "disabled");

		G_RestartGame(true);
	}

	if (g_match->modified) { // reset scores
		g_match->modified = false;

		g_level.match = g_match->integer;
		gi.ConfigString(CS_MATCH, va("%d", g_level.match));

		g_level.warmup = g_level.match; // toggle warmup

		gi.BroadcastPrint(PRINT_HIGH, "Match has been %s\n",
				g_level.match ? "enabled" : "disabled");

		G_RestartGame(false);
	}

	if (g_rounds->modified) { // reset scores
		g_rounds->modified = false;

		g_level.rounds = g_rounds->integer;
		gi.ConfigString(CS_ROUNDS, va("%d", g_level.rounds));

		g_level.warmup = g_level.rounds; // toggle warmup

		gi.BroadcastPrint(PRINT_HIGH, "Rounds have been %s\n",
				g_level.rounds ? "enabled" : "disabled");

		G_RestartGame(false);
	}

	if (g_cheats->modified) { // notify when cheats changes
		g_cheats->modified = false;

		gi.BroadcastPrint(PRINT_HIGH, "Cheats have been %s\n",
				g_cheats->integer ? "enabled" : "disabled");
	}

	if (g_frag_limit->modified) {
		g_frag_limit->modified = false;
		g_level.frag_limit = g_frag_limit->integer;

		gi.BroadcastPrint(PRINT_HIGH, "Fraglimit has been changed to %d\n",
				g_level.frag_limit);
	}

	if (g_round_limit->modified) {
		g_round_limit->modified = false;
		g_level.round_limit = g_round_limit->integer;

		gi.BroadcastPrint(PRINT_HIGH, "Roundlimit has been changed to %d\n",
				g_level.round_limit);
	}

	if (g_capture_limit->modified) {
		g_capture_limit->modified = false;
		g_level.capture_limit = g_capture_limit->integer;

		gi.BroadcastPrint(PRINT_HIGH, "Capturelimit has been changed to %d\n",
				g_level.capture_limit);
	}

	if (g_time_limit->modified) {
		g_time_limit->modified = false;
		g_level.time_limit = g_time_limit->value;

		gi.BroadcastPrint(PRINT_HIGH, "Timelimit has been changed to %d\n",
				(int) g_level.time_limit);
	}
}

/*
 * G_ExitLevel
 */
static void G_ExitLevel(void) {

	gi.AddCommandString(va("map %s\n", g_level.changemap));

	g_level.changemap = NULL;
	g_level.intermission_time = 0;

	G_EndClientFrames();
}

#define INTERMISSION 10.0  // intermission duration
/*
 * G_Frame
 *
 * The main game module "think" function, called once per server frame.
 * Nothing would happen in Quake land if this weren't called.
 */
static void G_Frame(void) {
	int i;
	g_edict_t *ent;

	g_level.frame_num++;
	g_level.time = g_level.frame_num * gi.server_frame;

	// check for level change after running intermission
	if (g_level.intermission_time) {
		if (g_level.time > g_level.intermission_time + INTERMISSION) {
			G_ExitLevel();
			return;
		}
	}

	// treat each object in turn
	// even the world gets a chance to think
	ent = &g_game.edicts[0];
	for (i = 0; i < ge.num_edicts; i++, ent++) {

		if (!ent->in_use)
			continue;

		g_level.current_entity = ent;

		// update old origin for interpolation
		if (!(ent->s.effects & EF_LIGHTNING))
			VectorCopy(ent->s.origin, ent->s.old_origin);

		if (ent->ground_entity) {

			// check for ground entities going away
			if (ent->ground_entity->link_count != ent->ground_entity_link_count)
				ent->ground_entity = NULL;
		}

		if (i > 0 && i <= sv_max_clients->integer)
			G_ClientBeginFrame(ent);
		else
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

	// build the player_state_t structures for all players
	G_EndClientFrames();
}

/*
 * G_InitMapList
 */
static void G_InitMapList(void) {
	int i;

	memset(&g_map_list, 0, sizeof(g_map_list));

	for (i = 0; i < MAX_MAP_LIST_ELTS; i++) {
		g_map_list_elt_t *map = &g_map_list.maps[i];
		map->gravity = map->gameplay = -1;
		map->teams = map->ctf = map->match = -1;
		map->frag_limit = map->round_limit = map->capture_limit = -1;
		map->time_limit = -1;
		map->weight = 1.0;
	}
}

/*
 * G_ParseMapList
 *
 * Populates a g_map_list_t from a text file.  See default/maps.lst
 */
static void G_ParseMapList(const char *file_name) {
	void *buf;
	const char *buffer;
	unsigned int i, j, k, l;
	const char *c;
	boolean_t map;
	g_map_list_elt_t *elt;

	G_InitMapList();

	if (gi.LoadFile(file_name, &buf) == -1) {
		gi.Print("Couldn't open %s\n", file_name);
		return;
	}

	buffer = (char *) buf;

	i = 0;
	map = false;
	while (true) {

		c = ParseToken(&buffer);

		if (*c == '\0')
			break;

		if (*c == '{')
			map = true;

		if (!map) // skip any whitespace between maps
			continue;

		elt = &g_map_list.maps[i];

		if (!strcmp(c, "name")) {
			strncpy(elt->name, ParseToken(&buffer), sizeof(elt->name) - 1);
			continue;
		}

		if (!strcmp(c, "title")) {
			strncpy(elt->title, ParseToken(&buffer), sizeof(elt->title) - 1);
			continue;
		}

		if (!strcmp(c, "sky")) {
			strncpy(elt->sky, ParseToken(&buffer), sizeof(elt->sky) - 1);
			continue;
		}

		if (!strcmp(c, "weather")) {
			strncpy(elt->weather, ParseToken(&buffer), sizeof(elt->weather) - 1);
			continue;
		}

		if (!strcmp(c, "gravity")) {
			elt->gravity = atoi(ParseToken(&buffer));
			continue;
		}

		if (!strcmp(c, "gameplay")) {
			elt->gameplay = G_GameplayByName(ParseToken(&buffer));
			continue;
		}

		if (!strcmp(c, "teams")) {
			elt->teams = strtoul(ParseToken(&buffer), NULL, 0);
			continue;
		}

		if (!strcmp(c, "ctf")) {
			elt->ctf = strtoul(ParseToken(&buffer), NULL, 0);
			continue;
		}

		if (!strcmp(c, "match")) {
			elt->match = strtoul(ParseToken(&buffer), NULL, 0);
			continue;
		}

		if (!strcmp(c, "rounds")) {
			elt->rounds = strtoul(ParseToken(&buffer), NULL, 0);
			continue;
		}

		if (!strcmp(c, "frag_limit")) {
			elt->frag_limit = strtoul(ParseToken(&buffer), NULL, 0);
			continue;
		}

		if (!strcmp(c, "round_limit")) {
			elt->round_limit = strtoul(ParseToken(&buffer), NULL, 0);
			continue;
		}

		if (!strcmp(c, "capture_limit")) {
			elt->capture_limit = strtoul(ParseToken(&buffer), NULL, 0);
			continue;
		}

		if (!strcmp(c, "time_limit")) {
			elt->time_limit = atof(ParseToken(&buffer));
			continue;
		}

		if (!strcmp(c, "give")) {
			strncpy(elt->give, ParseToken(&buffer), sizeof(elt->give) - 1);
			continue;
		}

		if (!strcmp(c, "music")) {
			strncpy(elt->music, ParseToken(&buffer), sizeof(elt->music) - 1);
			continue;
		}

		if (!strcmp(c, "weight")) {
			elt->weight = atof(ParseToken(&buffer));
			continue;
		}

		if (*c == '}') { // close it out
			map = false;

			/*printf("Loaded map %s:\n"
			 "title: %s\n"
			 "sky: %s\n"
			 "weather: %s\n"
			 "gravity: %d\n"
			 "gameplay: %ud\n"
			 "teams: %ud\n"
			 "ctf: %ud\n"
			 "match: %ud\n"
			 "rounds: %ud\n"
			 "frag_limit: %ud\n"
			 "round_limit: %ud\n"
			 "capture_limit: %ud\n"
			 "time_limit: %f\n"
			 "give: %s\n"
			 "music: %s\n"
			 "weight: %f\n",
			 map->name, map->title, map->sky, map->weather, map->gravity,
			 map->gameplay, map->teams, map->ctf, map->match, map->rounds,
			 map->frag_limit, map->round_limit, map->capture_limit,
			 map->time_limit, map->give, map->music, map->weight);*/

			// accumulate total weight
			g_map_list.total_weight += elt->weight;

			if (++i == MAX_MAP_LIST_ELTS)
				break;
		}
	}

	g_map_list.count = i;
	g_map_list.index = 0;

	gi.TagFree(buf);

	// thou shalt not divide by zero
	if (!g_map_list.total_weight)
		g_map_list.total_weight = 1.0;

	// compute the weighted index list
	for (i = 0, l = 0; i < g_map_list.count; i++) {

		elt = &g_map_list.maps[i];
		k = (elt->weight / g_map_list.total_weight) * MAP_LIST_WEIGHT;

		for (j = 0; j < k; j++)
			g_map_list.weighted_index[l++] = i;
	}
}

/*
 * G_Init
 *
 * This will be called when the game module is first loaded.
 */
void G_Init(void) {

	gi.Print("  Game initialization...\n");

	memset(&g_game, 0, sizeof(g_game));

	gi.Cvar("game_name", GAME_NAME, CVAR_SERVER_INFO | CVAR_NO_SET, NULL);
	gi.Cvar("game_date", __DATE__, CVAR_SERVER_INFO | CVAR_NO_SET, NULL);

	g_auto_join = gi.Cvar("g_auto_join", "1", CVAR_SERVER_INFO, NULL);
	g_capture_limit = gi.Cvar("g_capture_limit", "8", CVAR_SERVER_INFO, NULL);
	g_chat_log = gi.Cvar("g_chat_log", "0", 0, NULL);
	g_cheats = gi.Cvar("g_cheats", "0", CVAR_SERVER_INFO, NULL);
	g_ctf = gi.Cvar("g_ctf", "0", CVAR_SERVER_INFO, NULL);
	g_frag_limit = gi.Cvar("g_frag_limit", "30", CVAR_SERVER_INFO, NULL);
	g_frag_log = gi.Cvar("g_frag_log", "0", 0, NULL);
	g_friendly_fire = gi.Cvar("g_friendly_fire", "1", CVAR_SERVER_INFO, NULL);
	g_gameplay = gi.Cvar("g_gameplay", "0", CVAR_SERVER_INFO, NULL);
	g_gravity = gi.Cvar("g_gravity", "800", CVAR_SERVER_INFO, NULL);
	g_match = gi.Cvar("g_match", "0", CVAR_SERVER_INFO, NULL);
	g_max_entities = gi.Cvar("g_max_entities", "1024", CVAR_LATCH, NULL);
	g_mysql = gi.Cvar("g_mysql", "0", 0, NULL);
	g_mysql_db = gi.Cvar("g_mysql_db", "quake2world", 0, NULL);
	g_mysql_host = gi.Cvar("g_mysql_host", "localhost", 0, NULL);
	g_mysql_password = gi.Cvar("g_mysql_password", "", 0, NULL);
	g_mysql_user = gi.Cvar("g_mysql_user", "quake2world", 0, NULL);
	g_player_projectile = gi.Cvar("g_player_projectile", "1", CVAR_SERVER_INFO,
			NULL);
	g_random_map = gi.Cvar("g_random_map", "0", 0, NULL);
	g_round_limit = gi.Cvar("g_round_limit", "30", CVAR_SERVER_INFO, NULL);
	g_rounds = gi.Cvar("g_rounds", "0", CVAR_SERVER_INFO, NULL);
	g_spawn_farthest = gi.Cvar("g_spawn_farthest", "0", CVAR_SERVER_INFO, NULL);
	g_teams = gi.Cvar("g_teams", "0", CVAR_SERVER_INFO, NULL);
	g_time_limit = gi.Cvar("g_time_limit", "20", CVAR_SERVER_INFO, NULL);
	g_voting = gi.Cvar("g_voting", "1", CVAR_SERVER_INFO, "Activates voting");

	password = gi.Cvar("password", "", CVAR_USER_INFO, NULL);

	sv_max_clients = gi.Cvar("sv_max_clients", "8",
			CVAR_SERVER_INFO | CVAR_LATCH, NULL);
	dedicated = gi.Cvar("dedicated", "0", CVAR_NO_SET, NULL);

	if (g_frag_log->value)
		gi.OpenFile("frag_log.log", &frag_log, FILE_APPEND);

	if (g_chat_log->value)
		gi.OpenFile("chat_log.log", &chat_log, FILE_APPEND);

#ifdef HAVE_MYSQL
	if(g_mysql->value) { //init database

		mysql = mysql_init(NULL);

		mysql_real_connect(mysql, g_mysql_host->string,
				g_mysql_user->string, g_mysql_password->string,
				g_mysql_db->string, 0, NULL, 0
		);

		if(mysql != NULL)
		gi.Print("    MySQL connection to %s/%s", g_mysql_host->string,
				g_mysql_user->string);
	}
#endif

	G_ParseMapList("maps.lst");

	G_InitItems();

	// initialize entities and clients for this game
	g_game.edicts = gi.TagMalloc(g_max_entities->integer * sizeof(g_edict_t),
			TAG_GAME);
	g_game.clients = gi.TagMalloc(sv_max_clients->integer * sizeof(g_client_t),
			TAG_GAME);

	ge.edicts = g_game.edicts;
	ge.max_edicts = g_max_entities->integer;
	ge.num_edicts = sv_max_clients->integer + 1;

	// set these to false to avoid spurious game restarts and alerts on init
	g_gameplay->modified = g_teams->modified = g_match->modified
			= g_rounds->modified = g_ctf->modified = g_cheats->modified
					= g_frag_limit->modified = g_round_limit->modified
							= g_capture_limit->modified
									= g_time_limit->modified = false;

	gi.Print("  Game initialized.\n");
}

/*
 * Frees tags and closes frag_log.  This is called when the game is unloaded
 * (complements G_Init).
 */
void G_Shutdown(void) {

	gi.Print("  Game shutdown...\n");

	if (frag_log != NULL)
		gi.CloseFile(frag_log); // close frag_log

	if (chat_log != NULL)
		gi.CloseFile(chat_log); // and chat_log

#ifdef HAVE_MYSQL
	if(mysql != NULL)
		mysql_close(mysql); // and db
#endif

	gi.FreeTags(TAG_LEVEL);
	gi.FreeTags(TAG_GAME);
}

/*
 * G_LoadGame
 *
 * This is the entry point responsible for aligning the server and game module.
 * The server resolves this symbol upon successfully loading the game library,
 * and invokes it.  We're responsible for copying the import structure so that
 * we can call back into the server, and returning a populated game export
 * structure.
 */
g_export_t *G_LoadGame(g_import_t *import) {

	gi = *import;

	memset(&ge, 0, sizeof(ge));

	ge.api_version = GAME_API_VERSION;

	ge.Init = G_Init;
	ge.Shutdown = G_Shutdown;
	ge.SpawnEntities = G_SpawnEntities;

	ge.ClientThink = G_ClientThink;
	ge.ClientConnect = G_ClientConnect;
	ge.ClientUserInfoChanged = G_ClientUserInfoChanged;
	ge.ClientDisconnect = G_ClientDisconnect;
	ge.ClientBegin = G_ClientBegin;
	ge.ClientCommand = G_ClientCommand;

	ge.Frame = G_Frame;

	ge.edict_size = sizeof(g_edict_t);

	return &ge;
}
