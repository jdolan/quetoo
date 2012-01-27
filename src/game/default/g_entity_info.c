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

/*QUAKED info_null (0 0.5 0) (-4 -4 -4) (4 4 4)
 Used as a positional target for spotlights, etc.
 */
void G_info_null(g_edict_t *self) {
	G_FreeEdict(self);
}

/*QUAKED info_notnull (0 0.5 0) (-4 -4 -4) (4 4 4)
 Used as a positional target for lightning.
 */
void G_info_notnull(g_edict_t *self) {
	VectorCopy(self->s.origin, self->abs_mins);
	VectorCopy(self->s.origin, self->abs_maxs);
}

/*QUAKED info_player_start (1 0 0) (-16 -16 -24) (16 16 32)
 The normal starting point for a level.
 */
void G_info_player_start(g_edict_t *self) {
	G_ProjectSpawn(self);
}

/*QUAKED info_player_intermission (1 0 1) (-16 -16 -24) (16 16 32)
 Level intermission point will be at one of these
 Use 'angles' instead of 'angle', so you can set pitch or roll as well as yaw.
 'pitch yaw roll'
 */
void G_info_player_intermission(g_edict_t *self) {
	G_ProjectSpawn(self);
}

/*QUAKED info_player_deathmatch (1 0 1) (-16 -16 -24) (16 16 32)
 potential spawning position for deathmatch games
 */
void G_info_player_deathmatch(g_edict_t *self) {
	G_ProjectSpawn(self);
}

/*QUAKED info_player_team1 (1 0 1) (-16 -16 -24) (16 16 32)
 potential spawning position for team games
 */
void G_info_player_team1(g_edict_t *self) {
	G_ProjectSpawn(self);
}

/*QUAKED info_player_team2 (1 0 1) (-16 -16 -24) (16 16 32)
 potential spawning position for team games
 */
void G_info_player_team2(g_edict_t *self) {
	G_ProjectSpawn(self);
}
