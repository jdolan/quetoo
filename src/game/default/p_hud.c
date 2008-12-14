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

#include "g_local.h"


/*
P_MoveToIntermission
*/
void P_MoveToIntermission(edict_t *ent){
	VectorCopy(level.intermission_origin, ent->s.origin);
	ent->client->ps.pmove.origin[0] = level.intermission_origin[0] * 8;
	ent->client->ps.pmove.origin[1] = level.intermission_origin[1] * 8;
	ent->client->ps.pmove.origin[2] = level.intermission_origin[2] * 8;
	VectorCopy(level.intermission_angle, ent->client->ps.angles);
	ent->client->ps.pmove.pm_type = PM_FREEZE;

	ent->viewheight = 0;
	ent->s.modelindex = 0;
	ent->s.modelindex2 = 0;
	ent->s.modelindex3 = 0;
	ent->s.modelindex = 0;
	ent->s.effects = 0;
	ent->s.sound = 0;
	ent->solid = SOLID_NOT;
	ent->dead = true;

	// show scores
	ent->client->showscores = true;

	// hide the hud
	ent->client->locals.weapon = NULL;
	ent->client->ammo_index = 0;
	ent->client->locals.armor = 0;
	ent->client->pickup_msg_time = 0;

	// add the layout
	if(level.teams || level.ctf)
		P_TeamsScoreboard(ent);
	else
		P_Scoreboard(ent);

	gi.Unicast(ent, true);
}


/*
P_ColoredName

Copies src to dest, padding it to the specified maximum length.
Color escape sequences do not contribute to length.
*/
static void P_ColoredName(char *dst, const char *src, int maxlen, int maxsize){
	int c, l;
	const char *s;

	c = l = 0;
	s = src;

	while(*s){

		if(c > maxsize - 2)  // prevent overflows
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

	while(l < maxlen){  // pad with spaces
		dst[c++] = ' ';
		l++;
	}

	dst[c] = 0;
}


/*
P_TeamsScoreboard
*/
void P_TeamsScoreboard(edict_t *ent){
	char entry[512];
	char string[1300];
	char netname[MAX_NETNAME];
	char netname2[MAX_NETNAME];
	int stringlength;
	int i, j, k, l;
	int sorted[MAX_CLIENTS];
	int sortedscores[MAX_CLIENTS];
	char goodname[64], evilname[64];
	int goodcount, evilcount, speccount, total;
	int goodping, evilping;
	int goodtime, eviltime;
	int minutes;
	int x, y;
	gclient_t *cl;
	edict_t *cl_ent;
	char *c;

	goodcount = evilcount = speccount = total = 0;
	goodping = evilping = 0;
	goodtime = eviltime = 0;

	for(i = 0; i < sv_maxclients->value; i++){  // sort the clients by score

		cl_ent = g_edicts + 1 + i;
		cl = cl_ent->client;

		if(!cl_ent->inuse)
			continue;

		if(cl->locals.team == &good){  // head and score count each team
			goodping += cl->ping;
			goodtime += (level.framenum - cl->locals.enterframe);
			goodcount++;
		} else if(cl->locals.team == &evil){
			evilping += cl->ping;
			eviltime += (level.framenum - cl->locals.enterframe);
			evilcount++;
		} else speccount++;

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
	goodtime = goodtime / gi.serverrate / 60;
	eviltime = eviltime / gi.serverrate / 60;

	string[0] = stringlength = 0;
	j = k = l = 0;

	// build the scoreboard, resolve coords based on team
	for(i = 0; i < total; i++){
		cl = &game.clients[sorted[i]];
		cl_ent = g_edicts + 1 + sorted[i];

		if(cl->locals.team == &good){  // good up top, evil below
			x = 64; y = 32 * j++ + 64;
		} else if(cl->locals.team == &evil){
			x = 64; y = 32 * k++ + 128 + (goodcount * 32);
		} else {
			x = 128; y = 32 * l++ + 192 + ((goodcount + evilcount) * 32);
		}

		minutes = (level.framenum - cl->locals.enterframe) / gi.serverrate / 60;

		P_ColoredName(netname, cl->locals.netname, 16, MAX_NETNAME);

		if(!cl->locals.team){  // spectators
			// name[ping]
			snprintf(entry, sizeof(entry),
					"xv %i yv %i string \"%s[%i]\" ", x, y, cl->locals.netname, cl->ping);
		} else {  // teamed players
			if(level.ctf){ // name        captures score ping time
				x = 0;
				snprintf(entry, sizeof(entry),
						"xv %i yv %i string \"%s    %4i  %4i %4i %4i\" ",
						x, y, netname, cl->locals.captures,
						cl->locals.score, cl->ping, minutes
				);
			}
			else { // name        score ping time
				snprintf(entry, sizeof(entry),
						"xv %i yv %i string \"%s %4i %4i %4i\" ",
						x, y, netname, cl->locals.score, cl->ping, minutes
				);
			}
		}

		if(strlen(string) + strlen(entry) > 1024)  // leave room for header
			break;

		strcat(string, entry);
	}

	y = (goodcount * 32) + 96;  // draw evil beneath good

	goodcount = goodcount ? goodcount : 1;  // avoid divide by zero
	evilcount = evilcount ? evilcount : 1;

	snprintf(goodname, sizeof(goodname), "%s (%s", good.name, good.skin);
	if((c = strchr(goodname, '/')))  // trim to modelname
		*c = 0;
	strcat(goodname, ")");
	if(strlen(goodname) > 15)
		goodname[15] = 0;

	snprintf(evilname, sizeof(evilname), "%s (%s", evil.name, evil.skin);
	if((c = strchr(evilname, '/')))  // trim to modelname
		*c = 0;
	strcat(evilname, ")");
	if(strlen(evilname) > 15)
		evilname[15] = 0;

	// headers, team names, and team scores
	P_ColoredName(netname, goodname, 16, MAX_NETNAME);
	P_ColoredName(netname2, evilname, 16, MAX_NETNAME);

	if(level.ctf){
		snprintf(entry, sizeof(entry),
				"xv 0 yv  0 string2 \"Name            Captures Frags Ping Time\" "
				"xv 0 yv 32 string2 \"%s    %4i  %4i %4i %4i\" "
				"xv 0 yv %i string2 \"%s    %4i  %4i %4i %4i\" ",

				netname, good.captures, good.score, goodping / goodcount, goodtime / goodcount,

				y, netname2, evil.captures, evil.score, evilping / evilcount, eviltime / evilcount
		);
	}
	else {
		snprintf(entry, sizeof(entry),
				"xv 64 yv  0 string2 \"Name            Frags Ping Time\" "
				"xv 64 yv 32 string2 \"%s %4i %4i %4i\" "
				"xv 64 yv %i string2 \"%s %4i %4i %4i\" ",

				netname, good.score, goodping / goodcount, goodtime / goodcount,

				y, netname2, evil.score, evilping / evilcount, eviltime / evilcount
		);
	}

	strcat(string, entry);

	gi.WriteByte(svc_layout);
	gi.WriteString(string);
}


/*
P_Scoreboard
*/
void P_Scoreboard(edict_t *ent){
	char entry[512];
	char string[1300];
	char netname[MAX_NETNAME];
	int stringlength;
	int i, j, k, l;
	int sorted[MAX_CLIENTS];
	int sortedscores[MAX_CLIENTS];
	int playercount, speccount, total;
	int minutes;
	int x, y;
	gclient_t *cl;
	edict_t *cl_ent;

	playercount = speccount = total = 0;

	for(i = 0; i < sv_maxclients->value; i++){  // sort the clients by score

		cl_ent = g_edicts + 1 + i;
		cl = cl_ent->client;

		if(!cl_ent->inuse)
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
		cl = &game.clients[sorted[i]];
		cl_ent = g_edicts + 1 + sorted[i];

		if(cl->locals.spectator){  // spectators below players
			x = 128; y = 32 * l++ + 64 + (playercount * 32);
		}
		else {
			x = 64; y = 32 * j++ + 32;
		}

		minutes = (level.framenum - cl->locals.enterframe) / gi.serverrate / 60;

		P_ColoredName(netname, cl->locals.netname, 16, MAX_NETNAME);

		if(cl->locals.spectator){  // spectators
			// name[ping]
			snprintf(entry, sizeof(entry),
					"xv %i yv %i string \"%s[%i]\" ", x, y, cl->locals.netname, cl->ping);
		} else {  //players
			// name        score ping time
			snprintf(entry, sizeof(entry),
					"xv %i yv %i string \"%s %4i %4i %4i\" ",
					x, y, netname, cl->locals.score, cl->ping, minutes
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
P_SetStats
*/
void P_SetStats(edict_t *ent){
	gitem_t *item;

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
		item = &itemlist[ent->client->ammo_index];
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
	if(level.time > ent->client->pickup_msg_time){
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

	if(ent->client->locals.health <= 0 || level.intermissiontime || ent->client->showscores)
		ent->client->ps.stats[STAT_LAYOUTS] |= 1;

	// frags
	ent->client->ps.stats[STAT_FRAGS] = ent->client->locals.score;

	ent->client->ps.stats[STAT_SPECTATOR] = 0;

	if(level.votetime)  //send vote
		ent->client->ps.stats[STAT_VOTE] = CS_VOTE;
	else ent->client->ps.stats[STAT_VOTE] = 0;

	if((level.teams || level.ctf) && ent->client->locals.team){  // send teamname
		if(ent->client->locals.team == &good)
			ent->client->ps.stats[STAT_TEAMNAME] = CS_TEAMGOOD;
		else ent->client->ps.stats[STAT_TEAMNAME] = CS_TEAMEVIL;
	}
	else ent->client->ps.stats[STAT_TEAMNAME] = 0;

	ent->client->ps.stats[STAT_TIME] = CS_TIME;

	ent->client->ps.stats[STAT_READY] = 0;

	if(level.match && level.matchtime)  // match enabled but not started
		ent->client->ps.stats[STAT_READY] = ent->client->locals.ready;
}


/*
P_CheckChaseStats
*/
void P_CheckChaseStats(edict_t *ent){
	int i;
	gclient_t *cl;

	for(i = 1; i <= sv_maxclients->value; i++){

		cl = g_edicts[i].client;

		if(!g_edicts[i].inuse || cl->chase_target != ent)
			continue;

		memcpy(cl->ps.stats, ent->client->ps.stats, sizeof(cl->ps.stats));
		P_SetSpectatorStats(g_edicts + i);
	}
}


/*
P_SetSpectatorStats
*/
void P_SetSpectatorStats(edict_t *ent){
	gclient_t *cl = ent->client;

	if(!cl->chase_target){
		P_SetStats(ent);
		cl->ps.stats[STAT_SPECTATOR] = 1;
	}

	// layouts are independent in spectator
	cl->ps.stats[STAT_LAYOUTS] = 0;

	if(cl->locals.health <= 0 || level.intermissiontime || cl->showscores)
		cl->ps.stats[STAT_LAYOUTS] |= 1;

	if(cl->chase_target && cl->chase_target->inuse)
		cl->ps.stats[STAT_CHASE] = CS_PLAYERSKINS + (cl->chase_target - g_edicts) - 1;
	else
		cl->ps.stats[STAT_CHASE] = 0;
}

