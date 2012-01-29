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

#include "cg_local.h"

/*
 * Cg_ThirdPerson
 *
 * Return the 3rd person offset. Negative values imply that the camera is in
 * front of the player.
 */
static float Cg_ThirdPerson(const player_state_t *ps) {

	if (!ps->stats[STAT_CHASE]) { // chasing uses client side 3rd person

		/*
		 * Don't bother translating the origin when spectating, since we have
		 * no visible player model to begin with.
		 */
		if (ps->pmove.pm_type == PM_SPECTATOR && !ps->stats[STAT_HEALTH])
			return 0.0;
	}

	return cg_third_person->value;
}

/*
 * Cg_UpdateView
 */
void Cg_UpdateView(const player_state_t *ps) {

}
