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
void G_ClientToIntermission(g_edict_t *ent) {

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
	ent->client->persistent.weapon = NULL;
	ent->client->ammo_index = 0;
	ent->client->persistent.armor = 0;
	ent->client->pickup_msg_time = 0;
}

/*
 * G_UpdateScores_
 *
 * Write the scores information for the specified client.
 */
static void G_UpdateScores_(const g_edict_t *ent, char **buf) {
	player_score_t s;

	memset(&s, 0, sizeof(s));

	s.player_num = ent - g_game.edicts - 1;
	s.ping = ent->client->ping < 999 ? ent->client->ping : 999;

	if (ent->client->persistent.spectator) {
		s.team = 0xff;
		s.color = 0;
	} else if (ent->client->persistent.team) {
		if (ent->client->persistent.team == &g_team_good) {
			s.team = CS_TEAM_GOOD;
			s.color = ColorByName("blue", 0);
		} else {
			s.team = CS_TEAM_EVIL;
			s.color = ColorByName("red", 0);
		}
	} else {
		s.team = 0;
		s.color = ent->client->persistent.color;
	}

	s.score = ent->client->persistent.score;
	s.captures = ent->client->persistent.captures;

	memcpy(*buf, &s, sizeof(s));
	*buf += sizeof(s);
}

// all scores are dumped into this buffer several times per second
static char scores_buffer[MAX_STRING_CHARS];

/*
 * G_UpdateScores
 *
 * Returns the size of the resulting scores buffer.
 *
 * FIXME: Because we can only send the first 32 or so scores, we should sort
 * the clients here before serializing them.
 */
static unsigned int G_UpdateScores(void) {
	char *buf = scores_buffer;
	int i, j = 0;

	memset(buf, 0, sizeof(buf));

	// assemble the client scores
	for (i = 0; i < sv_max_clients->integer; i++) {
		const g_edict_t *e = &g_game.edicts[i + 1];

		if (!e->in_use)
			continue;

		G_UpdateScores_(e, &buf);

		if (j++ == 32)
			break;
	}

	// and optionally concatenate the team scores
	if (g_teams->integer || g_ctf->integer) {
		player_score_t s[2];

		memset(s, 0, sizeof(s));

		s[0].player_num = MAX_CLIENTS;
		s[0].team = CS_TEAM_GOOD;
		s[0].score = g_team_good.score;
		s[0].captures = g_team_good.captures;

		s[1].player_num = MAX_CLIENTS;
		s[1].team = CS_TEAM_EVIL;
		s[1].score = g_team_evil.score;
		s[1].captures = g_team_evil.captures;

		memcpy(buf, s, sizeof(s));
		j += 2;
	}

	return j * sizeof(player_score_t);
}

/*
 * G_ClientScores
 *
 * Assemble the binary scores data for the client.
 */
void G_ClientScores(g_edict_t *ent) {
	unsigned short length;

	if (ent->client->scores_time && (ent->client->scores_time < g_level.time))
		return;

	length = G_UpdateScores();

	gi.WriteByte(SV_CMD_SCORES);
	gi.WriteShort(length);
	gi.WriteData(scores_buffer, length);
	gi.Unicast(ent, false);

	ent->client->scores_time = g_level.time + 0.5;
}

/*
 * G_ClientStats
 */
void G_ClientStats(g_edict_t *ent) {
	g_item_t *item;

	// health
	if (ent->client->persistent.spectator || ent->dead) {
		ent->client->ps.stats[STAT_HEALTH_ICON] = 0;
		ent->client->ps.stats[STAT_HEALTH] = 0;
	} else {
		ent->client->ps.stats[STAT_HEALTH_ICON] = gi.ImageIndex("i_health");
		ent->client->ps.stats[STAT_HEALTH] = ent->health;
	}

	// ammo
	if (!ent->client->ammo_index) {
		ent->client->ps.stats[STAT_AMMO_ICON] = 0;
		ent->client->ps.stats[STAT_AMMO] = 0;
	} else {
		item = &g_items[ent->client->ammo_index];
		ent->client->ps.stats[STAT_AMMO_ICON] = gi.ImageIndex(item->icon);
		ent->client->ps.stats[STAT_AMMO]
				= ent->client->persistent.inventory[ent->client->ammo_index];
		ent->client->ps.stats[STAT_AMMO_LOW] = item->quantity;
	}

	// armor
	if (ent->client->persistent.armor >= 200)
		ent->client->ps.stats[STAT_ARMOR_ICON] = gi.ImageIndex("i_bodyarmor");
	else if (ent->client->persistent.armor >= 100)
		ent->client->ps.stats[STAT_ARMOR_ICON] = gi.ImageIndex("i_combatarmor");
	else if (ent->client->persistent.armor >= 50)
		ent->client->ps.stats[STAT_ARMOR_ICON] = gi.ImageIndex("i_jacketarmor");
	else if (ent->client->persistent.armor > 0)
		ent->client->ps.stats[STAT_ARMOR_ICON] = gi.ImageIndex("i_shard");
	else
		ent->client->ps.stats[STAT_ARMOR_ICON] = 0;
	ent->client->ps.stats[STAT_ARMOR] = ent->client->persistent.armor;

	// pickup message
	if (g_level.time > ent->client->pickup_msg_time) {
		ent->client->ps.stats[STAT_PICKUP_ICON] = 0;
		ent->client->ps.stats[STAT_PICKUP_STRING] = 0;
	}

	// weapon
	if (ent->client->persistent.weapon) {
		ent->client->ps.stats[STAT_WEAPON_ICON] = gi.ImageIndex(
				ent->client->persistent.weapon->icon);
		ent->client->ps.stats[STAT_WEAPON] = gi.ModelIndex(
				ent->client->persistent.weapon->model);
	} else {
		ent->client->ps.stats[STAT_WEAPON_ICON] = 0;
		ent->client->ps.stats[STAT_WEAPON] = 0;
	}

	// scoreboard
	ent->client->ps.stats[STAT_SCORES] = 0;

	if (ent->client->persistent.health <= 0 || g_level.intermission_time
			|| ent->client->show_scores)
		ent->client->ps.stats[STAT_SCORES] |= 1;

	// frags and captures
	ent->client->ps.stats[STAT_FRAGS] = ent->client->persistent.score;
	ent->client->ps.stats[STAT_CAPTURES] = ent->client->persistent.captures;

	ent->client->ps.stats[STAT_SPECTATOR] = 0;

	if (g_level.vote_time) //send vote
		ent->client->ps.stats[STAT_VOTE] = CS_VOTE;
	else
		ent->client->ps.stats[STAT_VOTE] = 0;

	if ((g_level.teams || g_level.ctf) && ent->client->persistent.team) { // send team_name
		if (ent->client->persistent.team == &g_team_good)
			ent->client->ps.stats[STAT_TEAM] = CS_TEAM_GOOD;
		else
			ent->client->ps.stats[STAT_TEAM] = CS_TEAM_EVIL;
	} else
		ent->client->ps.stats[STAT_TEAM] = 0;

	ent->client->ps.stats[STAT_TIME] = CS_TIME;

	ent->client->ps.stats[STAT_READY] = 0;

	if (g_level.match && g_level.match_time) // match enabled but not started
		ent->client->ps.stats[STAT_READY] = ent->client->persistent.ready;

	if (g_level.rounds) // rounds enabled, show the round number
		ent->client->ps.stats[STAT_ROUND] = g_level.round_num + 1;
}

/*
 * G_ClientSpectatorStats
 */
void G_ClientSpectatorStats(g_edict_t *ent) {
	g_client_t *cl = ent->client;

	if (!cl->chase_target) {
		G_ClientStats(ent);
		cl->ps.stats[STAT_SPECTATOR] = 1;
	}

	// layouts are independent in spectator
	cl->ps.stats[STAT_SCORES] = 0;

	if (cl->persistent.health <= 0 || g_level.intermission_time
			|| cl->show_scores)
		cl->ps.stats[STAT_SCORES] |= 1;

	if (cl->chase_target && cl->chase_target->in_use) {
		memcpy(cl->ps.stats, cl->chase_target->client->ps.stats, sizeof(cl->ps.stats));
		cl->ps.stats[STAT_CHASE] = CS_CLIENT_INFO + (cl->chase_target
				- g_game.edicts) - 1;
	} else
		cl->ps.stats[STAT_CHASE] = 0;
}

