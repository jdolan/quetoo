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

	PackPosition(g_level.intermission_origin, ent->client->ps.pm_state.origin);

	VectorClear(ent->client->ps.pm_state.view_angles);
	PackAngles(g_level.intermission_angle, ent->client->ps.pm_state.delta_angles);

	VectorClear(ent->client->ps.pm_state.view_offset);

	ent->client->ps.pm_state.pm_type = PM_FREEZE;

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

/**
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
	} else {
		if (g_level.match) {
			if (!ent->client->persistent.ready)
				s.flags |= SCORES_NOTREADY;
		}
		if (g_level.ctf) {
			if (ent->s.effects & (EF_CTF_BLUE | EF_CTF_RED))
				s.flags |= SCORES_FLAG;
		}
		if (ent->client->persistent.team) {
			if (ent->client->persistent.team == &g_team_good) {
				s.team = 1;
				s.color = ColorByName("blue", 0);
			} else {
				s.team = 2;
				s.color = ColorByName("red", 0);
			}
		} else {
			s.team = 0;
			s.color = ent->client->persistent.color;
		}
	}

	s.score = ent->client->persistent.score;
	s.captures = ent->client->persistent.captures;

	memcpy(*buf, &s, sizeof(s));
	*buf += sizeof(s);
}

// all scores are dumped into this buffer several times per second
static char scores_buffer[MAX_STRING_CHARS];

/**
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

		if (++j == 64)
			break;
	}

	// and optionally concatenate the team scores
	if (g_level.teams || g_level.ctf) {
		player_score_t s[2];

		memset(s, 0, sizeof(s));

		s[0].player_num = MAX_CLIENTS;
		s[0].team = 1;
		s[0].score = g_team_good.score;
		s[0].captures = g_team_good.captures;

		s[1].player_num = MAX_CLIENTS;
		s[1].team = 2;
		s[1].score = g_team_evil.score;
		s[1].captures = g_team_evil.captures;

		memcpy(buf, s, sizeof(s));
		j += 2;
	}

	return j * sizeof(player_score_t);
}

/**
 * G_ClientScores
 *
 * Assemble the binary scores data for the client.
 */
void G_ClientScores(g_edict_t *ent) {
	unsigned short length;

	if (!ent->client->show_scores || (ent->client->scores_time > g_level.time))
		return;

	length = G_UpdateScores();

	gi.WriteByte(SV_CMD_SCORES);
	gi.WriteShort(length);
	gi.WriteData(scores_buffer, length);
	gi.Unicast(ent, false);

	ent->client->scores_time = g_level.time + 500;
}

/**
 * G_ClientStats
 *
 * Writes the stats array of the player state structure. The client's HUD is
 * largely derived from this information.
 */
void G_ClientStats(g_edict_t *ent) {

	g_client_t *client = ent->client;
	const g_client_persistent_t *persistent = &client->persistent;

	// ammo
	if (!client->ammo_index) {
		client->ps.stats[STAT_AMMO_ICON] = 0;
		client->ps.stats[STAT_AMMO] = 0;
	} else {
		const g_item_t *item = &g_items[client->ammo_index];
		client->ps.stats[STAT_AMMO_ICON] = gi.ImageIndex(item->icon);
		client->ps.stats[STAT_AMMO] = persistent->inventory[client->ammo_index];
		client->ps.stats[STAT_AMMO_LOW] = item->quantity;
	}

	// armor
	if (persistent->armor >= 200)
		client->ps.stats[STAT_ARMOR_ICON] = gi.ImageIndex("i_bodyarmor");
	else if (persistent->armor >= 100)
		client->ps.stats[STAT_ARMOR_ICON] = gi.ImageIndex("i_combatarmor");
	else if (persistent->armor >= 50)
		client->ps.stats[STAT_ARMOR_ICON] = gi.ImageIndex("i_jacketarmor");
	else if (persistent->armor > 0)
		client->ps.stats[STAT_ARMOR_ICON] = gi.ImageIndex("i_shard");
	else
		client->ps.stats[STAT_ARMOR_ICON] = 0;
	client->ps.stats[STAT_ARMOR] = persistent->armor;

	// captures
	client->ps.stats[STAT_CAPTURES] = persistent->captures;

	// damage received and inflicted
	client->ps.stats[STAT_DAMAGE_ARMOR] = client->damage_armor;
	client->ps.stats[STAT_DAMAGE_HEALTH] = client->damage_health;
	client->ps.stats[STAT_DAMAGE_INFLICT] = client->damage_inflicted;

	// frags
	client->ps.stats[STAT_FRAGS] = persistent->score;

	// health
	if (persistent->spectator || ent->dead) {
		client->ps.stats[STAT_HEALTH_ICON] = 0;
		client->ps.stats[STAT_HEALTH] = 0;
	} else {
		client->ps.stats[STAT_HEALTH_ICON] = gi.ImageIndex("i_health");
		client->ps.stats[STAT_HEALTH] = ent->health;
	}

	// pickup message
	if (g_level.time > client->pickup_msg_time) {
		client->ps.stats[STAT_PICKUP_ICON] = 0;
		client->ps.stats[STAT_PICKUP_STRING] = 0;
	}

	// ready
	client->ps.stats[STAT_READY] = 0;
	if (g_level.match && g_level.match_time)
		client->ps.stats[STAT_READY] = persistent->ready;

	// rounds
	if (g_level.rounds)
		client->ps.stats[STAT_ROUND] = g_level.round_num + 1;

	// scores
	client->ps.stats[STAT_SCORES] = 0;
	if (g_level.intermission_time || client->show_scores)
		client->ps.stats[STAT_SCORES] |= 1;

	if ((g_level.teams || g_level.ctf) && persistent->team) { // send team_name
		if (persistent->team == &g_team_good)
			client->ps.stats[STAT_TEAM] = CS_TEAM_GOOD;
		else
			client->ps.stats[STAT_TEAM] = CS_TEAM_EVIL;
	} else
		client->ps.stats[STAT_TEAM] = 0;

	// time
	if (g_level.intermission_time)
		client->ps.stats[STAT_TIME] = 0;
	else
		client->ps.stats[STAT_TIME] = CS_TIME;

	// vote
	if (g_level.vote_time)
		client->ps.stats[STAT_VOTE] = CS_VOTE;
	else
		client->ps.stats[STAT_VOTE] = 0;

	// weapon
	if (persistent->weapon) {
		client->ps.stats[STAT_WEAPON_ICON] = gi.ImageIndex(persistent->weapon->icon);
		client->ps.stats[STAT_WEAPON] = gi.ModelIndex(persistent->weapon->model);
	} else {
		client->ps.stats[STAT_WEAPON_ICON] = 0;
		client->ps.stats[STAT_WEAPON] = 0;
	}

}

/*
 * G_ClientSpectatorStats
 */
void G_ClientSpectatorStats(g_edict_t *ent) {
	g_client_t *client = ent->client;

	client->ps.stats[STAT_SPECTATOR] = 1;

	// chase camera inherits stats from their chase target
	if (client->chase_target && client->chase_target->in_use) {
		client->ps.stats[STAT_CHASE] = CS_CLIENTS + (client->chase_target - g_game.edicts) - 1;

		// scores are independent of chase camera target
		if (g_level.intermission_time || client->show_scores)
			client->ps.stats[STAT_SCORES] = 1;
		else {
			client->ps.stats[STAT_SCORES] = 0;
		}
	} else {
		G_ClientStats(ent);
		client->ps.stats[STAT_CHASE] = 0;
	}
}

