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
void G_ClientChaseThink(g_entity_t *ent) {
	g_entity_t *targ = ent->client->locals.chase_target;

	if (targ) {
		vec3_t new_delta;

		// calculate delta angles if switching targets
		if (targ != ent->client->locals.old_chase_target) {
			new_delta = Vec3_Subtract(ent->client->locals.angles, targ->client->locals.angles);
			ent->client->locals.old_chase_target = targ;
		} else {
			new_delta = Vec3_Zero();
		}

		// copy origin
		ent->s.origin = targ->s.origin;

		// velocity
		ent->locals.velocity = targ->locals.velocity;

		// and angles
		ent->client->locals.angles = targ->client->locals.angles;

		// and player state
		memcpy(&ent->client->ps, &targ->client->ps, sizeof(player_state_t));

		// add in delta angles in case we've switched targets
		if (!Vec3_Equal(new_delta, Vec3_Zero())) {
			ent->client->ps.pm_state.delta_angles = Vec3_Add(ent->client->ps.pm_state.delta_angles, new_delta);
		}

		// disable the spectator's input
		ent->client->ps.pm_state.type = PM_FREEZE;
	} else {
		ent->client->ps.pm_state.delta_angles.z = -ent->client->ps.pm_state.delta_angles.z;

		// enable the spectator's input
		ent->client->ps.pm_state.type = PM_SPECTATOR;
	}

	gi.LinkEntity(ent);
}

/**
 * @brief
 */
void G_ClientChaseNext(g_entity_t *ent) {
	g_entity_t *e;

	if (!ent->client->locals.chase_target) {
		return;
	}

	int32_t i = (int32_t) (ptrdiff_t) (ent->client->locals.chase_target - g_game.entities);
	do {
		i++;

		if (i > sv_max_clients->integer) {
			i = 1;
		}

		e = g_game.entities + i;

		if (G_IsMeat(e)) {
			break;
		}

	} while (e != ent->client->locals.chase_target);

	ent->client->locals.chase_target = e;
}

/**
 * @brief
 */
void G_ClientChasePrevious(g_entity_t *ent) {
	g_entity_t *e;

	if (!ent->client->locals.chase_target) {
		return;
	}

	int32_t i = (int32_t) (ptrdiff_t) (ent->client->locals.chase_target - g_game.entities);
	do {
		i--;

		if (i < 1) {
			i = sv_max_clients->integer;
		}

		e = g_game.entities + i;

		if (G_IsMeat(e)) {
			break;
		}

	} while (e != ent->client->locals.chase_target);

	ent->client->locals.chase_target = e;
}

/**
 * @brief Finds the first available chase target and assigns it to the specified ent.
 */
void G_ClientChaseTarget(g_entity_t *ent) {

	for (int32_t i = 0; i < sv_max_clients->integer; i++) {
		g_entity_t *other = g_game.entities + i + 1;
		if (G_IsMeat(other)) {
			ent->client->locals.chase_target = other;
			return;
		}
	}
}

