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
 * G_ClientToIntermission
 */
void G_ClientToIntermission(edict_t *ent){
	VectorCopy(g_level.intermission_origin, ent->s.origin);
	ent->client->ps.pmove.origin[0] = g_level.intermission_origin[0] * 8;
	ent->client->ps.pmove.origin[1] = g_level.intermission_origin[1] * 8;
	ent->client->ps.pmove.origin[2] = g_level.intermission_origin[2] * 8;
	VectorCopy(g_level.intermission_angle, ent->client->ps.angles);
	ent->client->ps.pmove.pm_type = PM_FREEZE;

	ent->view_height = 0;
	ent->s.model1 = 0;
	ent->s.model2 = 0;
	ent->s.model3 = 0;
	ent->s.model4 = 0;
	ent->s.effects = 0;
	ent->s.sound = 0;
	ent->solid = SOLID_NOT;
	ent->dead = true;

	// show scores
	ent->client->show_scores = true;

	// hide the hud
	ent->client->locals.weapon = NULL;
	ent->client->ammo_index = 0;
	ent->client->locals.armor = 0;
	ent->client->pickup_msg_time = 0;

	// add the layout
	if(g_level.teams || g_level.ctf)
		G_ClientTeamsScoreboard(ent);
	else
		G_ClientScoreboard(ent);

	gi.Unicast(ent, true);
}


/*
 * G_ColoredName
 *
 * Copies src to dest, padding it to the specified maximum length.
 * Color escape sequences do not contribute to length.
 */
static void G_ColoredName(char *dst, const char *src, int max_len, int max_size){
	int c, l;
	const char *s;

	c = l = 0;
	s = src;

	while(*s){

		if(c > max_size - 2)  // prevent overflows
			break;

		if(IS_COLOR(s)){  // copy the color code, omitting length
			dst[c++] = *s;
			dst[c++] = *(s + 1);
			s += 2;
		}
		else {  // copy the char, incrementing length
			dst[c++] = *s;
			l++;
			s++;
		}
	}

	while(l < max_len){  // pad with spaces
		dst[c++] = ' ';
		l++;
	}

	dst[c] = 0;
}


/*
 * G_ClientTeamsScoreboard
 */
void G_ClientTeamsScoreboard(edict_t *ent){
	char entry[512];
	char string[1300];
	char net_name[MAX_NET_NAME];
	char net_name2[MAX_NET_NAME];
	int stringlength;
	int i, j, k, l;
	int sorted[MAX_CLIENTS];
	int sortedscores[MAX_CLIENTS];
	char good_name[64], evil_name[64];
	int good_count, evil_count, spec_count, total;
	int good_ping, evil_ping;
	int good_time, evil_time;
	int minutes;
	int x, y;
	g_client_t *cl;
	edict_t *cl_ent;
	char *c;

	good_count = evil_count = spec_count = total = 0;
	good_ping = evil_ping = 0;
	good_time = evil_time = 0;

	for(i = 0; i < sv_max_clients->integer; i++){  // sort the clients by score

		cl_ent = g_game.edicts + 1 + i;
		cl = cl_ent->client;

		if(!cl_ent->in_use)
			continue;

		if(cl->locals.team == &good){  // head and score count each team
			good_ping += cl->ping;
			good_time += (g_level.frame_num - cl->locals.first_frame);
			good_count++;
		} else if(cl->locals.team == &evil){
			evil_ping += cl->ping;
			evil_time += (g_level.frame_num - cl->locals.first_frame);
			evil_count++;
		} else spec_count++;

		for(j = 0; j < total; j++){
			if(cl->locals.score > sortedscores[j])
				break;
		}

		for(k = total; k > j; k--){
			sorted[k] = sorted[k - 1];
			sortedscores[k] = sortedscores[k - 1];
		}

		sorted[j] = i;
		sortedscores[j] = cl->locals.score;
		total++;
	}

	// normalize times to minutes
	good_time = good_time / gi.frame_rate / 60;
	evil_time = evil_time / gi.frame_rate / 60;

	string[0] = stringlength = 0;
	j = k = l = 0;

	// build the scoreboard, resolve coordinates based on team
	for(i = 0; i < total; i++){
		cl = &g_game.clients[sorted[i]];
		cl_ent = g_game.edicts + 1 + sorted[i];

		if(cl->locals.team == &good){  // good up top, evil below
			x = 64; y = 32 * j++ + 64;
		} else if(cl->locals.team == &evil){
			x = 64; y = 32 * k++ + 128 + (good_count * 32);
		} else {
			x = 128; y = 32 * l++ + 192 + ((good_count + evil_count) * 32);
		}

		minutes = (g_level.frame_num - cl->locals.first_frame) / gi.frame_rate / 60;

		G_ColoredName(net_name, cl->locals.net_name, 16, MAX_NET_NAME);

		if(!cl->locals.team){  // spectators
			// name[ping]
			snprintf(entry, sizeof(entry),
					"xv %i yv %i string \"%s[%i]\" ", x, y, cl->locals.net_name, cl->ping);
		} else {  // teamed players
			if(g_level.ctf){ // name        captures score ping time
				x = 0;
				snprintf(entry, sizeof(entry),
						"xv %i yv %i string \"%s    %4i  %4i %4i %4i\" ",
						x, y, net_name, cl->locals.captures,
						cl->locals.score, cl->ping, minutes
				);
			}
			else { // name        score ping time
				snprintf(entry, sizeof(entry),
						"xv %i yv %i string \"%s %4i %4i %4i\" ",
						x, y, net_name, cl->locals.score, cl->ping, minutes
				);
			}
		}

		if(strlen(string) + strlen(entry) > 1024)  // leave room for header
			break;

		strcat(string, entry);
	}

	y = (good_count * 32) + 96;  // draw evil beneath good

	good_count = good_count ? good_count : 1;  // avoid divide by zero
	evil_count = evil_count ? evil_count : 1;

	snprintf(good_name, sizeof(good_name), "%s (%s", good.name, good.skin);
	if((c = strchr(good_name, '/')))  // trim to model name
		*c = 0;
	strcat(good_name, ")");
	if(strlen(good_name) > 15)
		good_name[15] = 0;

	snprintf(evil_name, sizeof(evil_name), "%s (%s", evil.name, evil.skin);
	if((c = strchr(evil_name, '/')))  // trim to model name
		*c = 0;
	strcat(evil_name, ")");
	if(strlen(evil_name) > 15)
		evil_name[15] = 0;

	// headers, team names, and team scores
	G_ColoredName(net_name, good_name, 16, MAX_NET_NAME);
	G_ColoredName(net_name2, evil_name, 16, MAX_NET_NAME);

	if(g_level.ctf){
		snprintf(entry, sizeof(entry),
				"xv 0 yv  0 string2 \"Name            Captures Frags Ping Time\" "
				"xv 0 yv 32 string2 \"%s    %4i  %4i %4i %4i\" "
				"xv 0 yv %i string2 \"%s    %4i  %4i %4i %4i\" ",

				net_name, good.captures, good.score, good_ping / good_count, good_time / good_count,

				y, net_name2, evil.captures, evil.score, evil_ping / evil_count, evil_time / evil_count
		);
	}
	else {
		snprintf(entry, sizeof(entry),
				"xv 64 yv  0 string2 \"Name            Frags Ping Time\" "
				"xv 64 yv 32 string2 \"%s %4i %4i %4i\" "
				"xv 64 yv %i string2 \"%s %4i %4i %4i\" ",

				net_name, good.score, good_ping / good_count, good_time / good_count,

				y, net_name2, evil.score, evil_ping / evil_count, evil_time / evil_count
		);
	}

	strcat(string, entry);

	gi.WriteByte(svc_layout);
	gi.WriteString(string);
}


/*
 * G_ClientScoreboard
 */
void G_ClientScoreboard(edict_t *ent){
	char entry[512];
	char string[1300];
	char net_name[MAX_NET_NAME];
	int stringlength;
	int i, j, k, l;
	int sorted[MAX_CLIENTS];
	int sortedscores[MAX_CLIENTS];
	int playercount, speccount, total;
	int minutes;
	int x, y;
	g_client_t *cl;
	edict_t *cl_ent;

	playercount = speccount = total = 0;

	for(i = 0; i < sv_max_clients->integer; i++){  // sort the clients by score

		cl_ent = g_game.edicts + 1 + i;
		cl = cl_ent->client;

		if(!cl_ent->in_use)
			continue;

		if(cl->locals.spectator)
			speccount++;
		else playercount++;

		for(j = 0; j < total; j++){
			if(cl->locals.score > sortedscores[j])
				break;
		}

		for(k = total; k > j; k--){
			sorted[k] = sorted[k - 1];
			sortedscores[k] = sortedscores[k - 1];
		}

		sorted[j] = i;
		sortedscores[j] = cl->locals.score;
		total++;
	}

	string[0] = stringlength = 0;
	j = k = l = 0;

	// build the scoreboard, resolve coords based on team
	for(i = 0; i < total; i++){
		cl = &g_game.clients[sorted[i]];
		cl_ent = g_game.edicts + 1 + sorted[i];

		if(cl->locals.spectator){  // spectators below players
			x = 128; y = 32 * l++ + 64 + (playercount * 32);
		}
		else {
			x = 64; y = 32 * j++ + 32;
		}

		minutes = (g_level.frame_num - cl->locals.first_frame) / gi.frame_rate / 60;

		G_ColoredName(net_name, cl->locals.net_name, 16, MAX_NET_NAME);

		if(cl->locals.spectator){  // spectators
			// name[ping]
			snprintf(entry, sizeof(entry),
					"xv %i yv %i string \"%s[%i]\" ", x, y, cl->locals.net_name, cl->ping);
		} else {  //players
			// name        score ping time
			snprintf(entry, sizeof(entry),
					"xv %i yv %i string \"%s %4i %4i %4i\" ",
					x, y, net_name, cl->locals.score, cl->ping, minutes
			);
		}

		if(strlen(string) + strlen(entry) > 1024)  // leave room for header
			break;

		strcat(string, entry);
	}

	// append header
	snprintf(entry, sizeof(entry),
			"xv 64 yv 0 string2 \"Name            Frags Ping Time\" ");
	strcat(string, entry);

	gi.WriteByte(svc_layout);
	gi.WriteString(string);
}


/*
 * G_ClientStats
 */
void G_ClientStats(edict_t *ent){
	g_item_t *item;

	// health
	if(ent->client->locals.spectator || ent->dead){
		ent->client->ps.stats[STAT_HEALTH_ICON] = 0;
		ent->client->ps.stats[STAT_HEALTH] = 0;
	}
	else {
		ent->client->ps.stats[STAT_HEALTH_ICON] = gi.ImageIndex("i_health");
		ent->client->ps.stats[STAT_HEALTH] = ent->health;
	}

	// ammo
	if(!ent->client->ammo_index){
		ent->client->ps.stats[STAT_AMMO_ICON] = 0;
		ent->client->ps.stats[STAT_AMMO] = 0;
	} else {
		item = &g_items[ent->client->ammo_index];
		ent->client->ps.stats[STAT_AMMO_ICON] = gi.ImageIndex(item->icon);
		ent->client->ps.stats[STAT_AMMO] = ent->client->locals.inventory[ent->client->ammo_index];
		ent->client->ps.stats[STAT_AMMO_LOW] = item->quantity;
	}

	// armor
	if(ent->client->locals.armor >= 200)
		ent->client->ps.stats[STAT_ARMOR_ICON] = gi.ImageIndex("i_bodyarmor");
	else if(ent->client->locals.armor >= 100)
		ent->client->ps.stats[STAT_ARMOR_ICON] = gi.ImageIndex("i_combatarmor");
	else if(ent->client->locals.armor >= 50)
		ent->client->ps.stats[STAT_ARMOR_ICON] = gi.ImageIndex("i_jacketarmor");
	else if(ent->client->locals.armor > 0)
		ent->client->ps.stats[STAT_ARMOR_ICON] = gi.ImageIndex("i_shard");
	else
		ent->client->ps.stats[STAT_ARMOR_ICON] = 0;
	ent->client->ps.stats[STAT_ARMOR] = ent->client->locals.armor;

	// pickup message
	if(g_level.time > ent->client->pickup_msg_time){
		ent->client->ps.stats[STAT_PICKUP_ICON] = 0;
		ent->client->ps.stats[STAT_PICKUP_STRING] = 0;
	}

	// weapon
	if(ent->client->locals.weapon){
		ent->client->ps.stats[STAT_WEAPICON] =
			gi.ImageIndex(ent->client->locals.weapon->icon);
		ent->client->ps.stats[STAT_WEAPON] =
			gi.ModelIndex(ent->client->locals.weapon->model);
	}
	else {
		ent->client->ps.stats[STAT_WEAPICON] = 0;
		ent->client->ps.stats[STAT_WEAPON] = 0;
	}

	// layouts
	ent->client->ps.stats[STAT_LAYOUTS] = 0;

	if(ent->client->locals.health <= 0 || g_level.intermission_time || ent->client->show_scores)
		ent->client->ps.stats[STAT_LAYOUTS] |= 1;

	// frags
	ent->client->ps.stats[STAT_FRAGS] = ent->client->locals.score;

	ent->client->ps.stats[STAT_SPECTATOR] = 0;

	if(g_level.votetime)  //send vote
		ent->client->ps.stats[STAT_VOTE] = CS_VOTE;
	else ent->client->ps.stats[STAT_VOTE] = 0;

	if((g_level.teams || g_level.ctf) && ent->client->locals.team){  // send team_name
		if(ent->client->locals.team == &good)
			ent->client->ps.stats[STAT_TEAMNAME] = CS_TEAM_GOOD;
		else ent->client->ps.stats[STAT_TEAMNAME] = CS_TEAM_EVIL;
	}
	else ent->client->ps.stats[STAT_TEAMNAME] = 0;

	ent->client->ps.stats[STAT_TIME] = CS_TIME;

	ent->client->ps.stats[STAT_READY] = 0;

	if(g_level.match && g_level.match_time)  // match enabled but not started
		ent->client->ps.stats[STAT_READY] = ent->client->locals.ready;
}


/*
 * G_ClientSpectatorStats
 */
void G_ClientSpectatorStats(edict_t *ent){
	g_client_t *cl = ent->client;

	if(!cl->chase_target){
		G_ClientStats(ent);
		cl->ps.stats[STAT_SPECTATOR] = 1;
	}

	// layouts are independent in spectator
	cl->ps.stats[STAT_LAYOUTS] = 0;

	if(cl->locals.health <= 0 || g_level.intermission_time || cl->show_scores)
		cl->ps.stats[STAT_LAYOUTS] |= 1;

	if(cl->chase_target && cl->chase_target->in_use){
		cl->ps.stats[STAT_CHASE] = CS_PLAYER_SKINS + (cl->chase_target - g_game.edicts) - 1;
		memcpy(cl->ps.stats, cl->chase_target->client->ps.stats, sizeof(cl->ps.stats));
	}
	else
		cl->ps.stats[STAT_CHASE] = 0;
}

