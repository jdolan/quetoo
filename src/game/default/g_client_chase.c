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
 * @brief
 */
void G_ClientChaseThink(g_edict_t *ent) {
	int16_t delta[3];

	g_edict_t *targ = ent->client->locals.chase_target;

	// calculate delta angles if switching targets
	if (targ != ent->client->locals.old_chase_target) {
		vec3_t d;

		VectorSubtract(ent->client->locals.angles, targ->client->locals.angles, d);
		PackAngles(d, delta);

		ent->client->locals.old_chase_target = targ;
	} else {
		VectorClear(delta);
	}

	// copy origin
	VectorCopy(targ->s.origin, ent->s.origin);

	// velocity
	VectorCopy(targ->locals.velocity, ent->locals.velocity);

	// and angles
	VectorCopy(targ->client->locals.angles, ent->client->locals.angles);

	// and player state
	memcpy(&ent->client->ps, &targ->client->ps, sizeof(player_state_t));

	// add in delta angles in case we've switched targets
	VectorAdd(ent->client->ps.pm_state.delta_angles, delta, ent->client->ps.pm_state.delta_angles);

	// disable the spectator's input
	ent->client->ps.pm_state.type = PM_FREEZE;

	// disable client prediction
	ent->client->ps.pm_state.flags |= PMF_NO_PREDICTION;

	gi.LinkEdict(ent);
}

/*
 * @brief
 */
void G_ClientChaseNext(g_edict_t *ent) {
	int32_t i;
	g_edict_t *e;

	if (!ent->client->locals.chase_target)
		return;

	i = ent->client->locals.chase_target - g_game.edicts;
	do {
		i++;

		if (i > sv_max_clients->integer)
			i = 1;

		e = g_game.edicts + i;

		if (!e->in_use)
			continue;

		if (!e->client->locals.persistent.spectator)
			break;

	} while (e != ent->client->locals.chase_target);

	ent->client->locals.chase_target = e;
}

/*
 * @brief
 */
void G_ClientChasePrevious(g_edict_t *ent) {
	int32_t i;
	g_edict_t *e;

	if (!ent->client->locals.chase_target)
		return;

	i = ent->client->locals.chase_target - g_game.edicts;
	do {
		i--;

		if (i < 1)
			i = sv_max_clients->integer;

		e = g_game.edicts + i;

		if (!e->in_use)
			continue;

		if (!e->client->locals.persistent.spectator)
			break;

	} while (e != ent->client->locals.chase_target);

	ent->client->locals.chase_target = e;
}

/*
 * @brief Finds the first available chase target and assigns it to the specified ent.
 */
void G_ClientChaseTarget(g_edict_t *ent) {
	int32_t i;
	g_edict_t *other;

	for (i = 1; i <= sv_max_clients->integer; i++) {
		other = g_game.edicts + i;
		if (other->in_use && !other->client->locals.persistent.spectator) {
			ent->client->locals.chase_target = other;
			G_ClientChaseThink(ent);
			return;
		}
	}
}

