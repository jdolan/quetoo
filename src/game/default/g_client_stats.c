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

/**
 * @brief
 */
void G_ClientToIntermission(g_entity_t *ent) {

	ent->s.origin = g_level.intermission_origin;
	ent->client->ps.pm_state.origin = g_level.intermission_origin;

	ent->client->ps.pm_state.view_angles = vec3_zero();
	ent->client->ps.pm_state.delta_angles = g_level.intermission_angle;

	ent->client->ps.pm_state.view_offset = vec3_zero();

	ent->client->ps.pm_state.type = PM_FREEZE;

	ent->s.model1 = 0;
	ent->s.model2 = 0;
	ent->s.model3 = 0;
	ent->s.model4 = 0;
	ent->s.effects = 0;
	ent->s.sound = 0;
	ent->solid = SOLID_NOT;
	ent->locals.dead = true;

	// show scores
	ent->client->locals.show_scores = true;

	// hide the HUD
	memset(ent->client->locals.inventory, 0, sizeof(ent->client->locals.inventory));
	ent->client->locals.weapon = NULL;

	ent->client->locals.ammo_index = 0;
	ent->client->locals.pickup_msg_time = 0;

	// take a screenshot if we're supposed to
	if (g_force_screenshot->integer == 1) {
		G_ClientStuff(ent, "r_screenshot\n");
	}
}

/**
 * @brief Write the scores information for the specified client.
 */
static void G_UpdateScore(const g_entity_t *ent, g_score_t *s) {

	memset(s, 0, sizeof(*s));

	s->client = ent - g_game.entities - 1;
	s->ping = ent->client->ping < 999 ? ent->client->ping : 999;

	if (ent->client->locals.persistent.spectator) {
		s->color = -1;
		s->flags |= SCORE_SPECTATOR;
	} else {
		if (g_level.match) {
			if (!ent->client->locals.persistent.ready) {
				s->flags |= SCORE_NOT_READY;
			}
		}
		if (g_level.ctf) {
			if (ent->s.effects & EF_CTF_MASK) {
				s->flags |= SCORE_CTF_FLAG;
			}
		}
		if (ent->client->locals.persistent.team) {
			s->color = ent->client->locals.persistent.team->color;
			s->team = ent->client->locals.persistent.team->id + 1;
		} else {
			s->color = ent->client->locals.persistent.color;
		}
	}

	s->score = ent->client->locals.persistent.score;
	s->deaths = ent->client->locals.persistent.deaths;
	s->captures = ent->client->locals.persistent.captures;
}

/**
 * @brief Returns the number of scores written to the buffer.
 */
static size_t G_UpdateScores(g_score_t *scores) {
	g_score_t *s = scores;
	int32_t i;

	// assemble the client scores
	for (i = 0; i < sv_max_clients->integer; i++) {
		const g_entity_t *e = &g_game.entities[i + 1];

		if (!e->in_use) {
			continue;
		}

		G_UpdateScore(e, s++);
	}

	// and optionally concatenate the team scores
	if (g_level.teams || g_level.ctf) {
		memset(s, 0, sizeof(*s) * MAX_TEAMS);

		for (i = 0; i < MAX_TEAMS; i++) {
			g_team_t *team = &g_teamlist[i];

			s->client = MAX_CLIENTS;
			s->score = team->score;
			s->captures = team->captures;
			s->flags = team->id | SCORE_AGGREGATE;
			s++;
		}
	}

	return (size_t) (s - scores);
}

/**
 * @brief Assemble the binary scores data for the client. Scores are sent in
 * chunks to overcome the 1400 byte UDP packet limitation.
 */
void G_ClientScores(g_entity_t *ent) {
	static g_score_t scores[MAX_CLIENTS + MAX_TEAMS];
	static size_t count;

	if (!ent->client->locals.show_scores || (ent->client->locals.scores_time > g_level.time)) {
		return;
	}

	ent->client->locals.scores_time = g_level.time + 500;

	// update the scoreboard if it's stale; this is shared to all clients
	if (g_level.scores_time <= g_level.time) {
		count = G_UpdateScores(scores);
		g_level.scores_time = g_level.time + 500;
	}

	// send the scores over in chunks
	size_t i = 0, j = 0;
	while (++i < count) {
		const size_t len = (i - j) * sizeof(g_score_t);
		if (len > 512) {
			gi.WriteByte(SV_CMD_SCORES);
			gi.WriteShort((int32_t) j);
			gi.WriteShort((int32_t) i);
			gi.WriteData((const void *) (scores + j), len);
			gi.WriteByte(0); // sequence is incomplete
			gi.Unicast(ent, false);

			j = i;
		}
	}

	// send any remaining scores, and indicate that the sequence is complete
	const size_t len = (i - j) * sizeof(g_score_t);

	gi.WriteByte(SV_CMD_SCORES);
	gi.WriteShort((int32_t) j);
	gi.WriteShort((int32_t) i);
	gi.WriteData((const void *) (scores + j), len);
	gi.WriteByte(1); // sequence is complete
	gi.Unicast(ent, false);
}

/**
 * @brief Writes the stats array of the player state structure. The client's HUD is
 * largely derived from this information.
 */
void G_ClientStats(g_entity_t *ent) {
	g_client_t *client = ent->client;

	// ammo
	if (client->locals.weapon && client->locals.ammo_index) {
		const g_item_t *ammo = G_ItemByIndex(client->locals.ammo_index);
		const g_item_t *weap = client->locals.weapon;
		client->ps.stats[STAT_AMMO_ICON] = weap->icon_index;
		client->ps.stats[STAT_AMMO] = client->locals.inventory[client->locals.ammo_index];
		client->ps.stats[STAT_AMMO_LOW] = ammo->quantity;
	} else {
		client->ps.stats[STAT_AMMO_ICON] = 0;
		client->ps.stats[STAT_AMMO] = 0;
	}

	// armor
	const g_item_t *armor = G_ClientArmor(ent);
	if (armor) {
		client->ps.stats[STAT_ARMOR_ICON] = armor->icon_index;
		client->ps.stats[STAT_ARMOR] = client->locals.inventory[armor->index];
	} else {
		client->ps.stats[STAT_ARMOR_ICON] = 0;
		client->ps.stats[STAT_ARMOR] = 0;
	}

	// tech
	const g_item_t *tech = G_CarryingTech(ent);
	if (tech) {
		client->ps.stats[STAT_TECH_ICON] = tech->icon_index;
	} else {
		client->ps.stats[STAT_TECH_ICON] = -1;
	}

	// captures
	client->ps.stats[STAT_CAPTURES] = client->locals.persistent.captures;

	// damage received and inflicted
	client->ps.stats[STAT_DAMAGE_ARMOR] = client->locals.damage_armor;
	client->ps.stats[STAT_DAMAGE_HEALTH] = client->locals.damage_health;
	client->ps.stats[STAT_DAMAGE_INFLICT] = client->locals.damage_inflicted;

	// held flag
	client->ps.stats[STAT_CARRYING_FLAG] = 0;

	for (int32_t i = 0; i < g_level.num_teams; i++) {

		if (client->locals.inventory[g_media.items.flags[i]->index]) {
			client->ps.stats[STAT_CARRYING_FLAG] = i + 1;
			break;
		}
	}

	// frags
	client->ps.stats[STAT_FRAGS] = client->locals.persistent.score;
	client->ps.stats[STAT_DEATHS] = client->locals.persistent.deaths;

	// health
	if (client->locals.persistent.spectator || ent->locals.dead) {
		client->ps.stats[STAT_HEALTH_ICON] = 0;
		client->ps.stats[STAT_HEALTH] = 0;
	} else {
		if (ent->locals.health > 100) {
			client->ps.stats[STAT_HEALTH_ICON] = g_media.items.health[HEALTH_MEGA]->icon_index;
		} else if (ent->locals.health > 75) {
			client->ps.stats[STAT_HEALTH_ICON] = g_media.images.health;
		} else if (ent->locals.health > 25) {
			client->ps.stats[STAT_HEALTH_ICON] = g_media.items.health[HEALTH_MEDIUM]->icon_index;
		} else {
			client->ps.stats[STAT_HEALTH_ICON] = g_media.items.health[HEALTH_LARGE]->icon_index;
		}
		client->ps.stats[STAT_HEALTH] = ent->locals.health;
	}

	// pickup message
	if (g_level.time > client->locals.pickup_msg_time) {
		client->ps.stats[STAT_PICKUP_ICON] = -1;
		client->ps.stats[STAT_PICKUP_STRING] = 0;
	}

	// ready
	client->ps.stats[STAT_READY] = 0;
	if (g_level.match && g_level.match_time) {
		client->ps.stats[STAT_READY] = client->locals.persistent.ready;
	}

	// rounds
	if (g_level.rounds) {
		client->ps.stats[STAT_ROUND] = g_level.round_num + 1;
	}

	// scores
	client->ps.stats[STAT_SCORES] = 0;
	if (g_level.intermission_time || client->locals.show_scores) {
		client->ps.stats[STAT_SCORES] |= 1;
	}

	if ((g_level.teams || g_level.ctf) && client->locals.persistent.team) { // send team ID, -1 is no team
		client->ps.stats[STAT_TEAM] = client->locals.persistent.team->id;
	} else {
		client->ps.stats[STAT_TEAM] = -1;
	}

	// time
	if (g_level.intermission_time) {
		client->ps.stats[STAT_TIME] = 0;
	} else {
		client->ps.stats[STAT_TIME] = CS_TIME;
	}

	// vote
	if (g_level.vote_time) {
		client->ps.stats[STAT_VOTE] = CS_VOTE;
	} else {
		client->ps.stats[STAT_VOTE] = 0;
	}

	// weapon
	const g_item_t *weapon = client->locals.weapon;

	if (weapon) {
		client->ps.stats[STAT_WEAPON] = client->locals.weapon->model_index;
		client->ps.stats[STAT_WEAPON_ICON] = weapon->icon_index;
		client->ps.stats[STAT_WEAPON_TAG] = weapon->tag;
	} else {
		client->ps.stats[STAT_WEAPON] = 0;
		client->ps.stats[STAT_WEAPON_ICON] = 0;
		client->ps.stats[STAT_WEAPON_TAG] = 0;
	}

	if (client->locals.next_weapon) {
		client->ps.stats[STAT_WEAPON_TAG] |= (client->locals.next_weapon->tag << 8);
	}

	if (g_level.time <= client->locals.quad_damage_time) {
		client->ps.stats[STAT_QUAD_TIME] = ceil((client->locals.quad_damage_time - g_level.time) / 1000.0);
	} else {
		client->ps.stats[STAT_QUAD_TIME] = 0;
	}

	// change-able weapons
	client->ps.stats[STAT_WEAPONS] = 0;

	if (!client->locals.persistent.spectator && !ent->locals.dead) {
		for (int32_t i = WEAPON_NONE + 1; i < WEAPON_TOTAL; i++) {
			const g_item_t *weapon = g_media.items.weapons[i];
			const g_item_t *ammo = weapon->ammo_item;

			if (client->locals.inventory[weapon->index] &&
				(!ammo || client->locals.inventory[ammo->index] >= weapon->quantity)) {
				client->ps.stats[STAT_WEAPONS] |= 1 << (i - 1);
			}
		}
	}
}

/**
 * @brief
 */
void G_ClientSpectatorStats(g_entity_t *ent) {
	g_client_t *client = ent->client;

	client->ps.stats[STAT_SPECTATOR] = 1;

	// chase camera inherits stats from their chase target
	if (client->locals.chase_target && client->locals.chase_target->in_use) {
		client->ps.stats[STAT_CHASE] = CS_CLIENTS + (client->locals.chase_target - g_game.entities)
		                               - 1;

		// scores are independent of chase camera target
		if (g_level.intermission_time || client->locals.show_scores) {
			client->ps.stats[STAT_SCORES] = 1;
		} else {
			client->ps.stats[STAT_SCORES] = 0;
		}
	} else {
		G_ClientStats(ent);
		client->ps.stats[STAT_CHASE] = 0;
	}
}
