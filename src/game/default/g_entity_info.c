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

/*QUAKED info_notnull (0 .5 0) (-4 -4 -4) (4 4 4)
 A positional target for other entities. Unlike info_null, these are available in the game and can be targeted by other entities (e.g. info_player_intermission).

 -------- Keys --------
 targetname : The target name of this entity.
*/
void G_info_notnull(g_entity_t *self) {
	gi.LinkEntity(self);
}

/*QUAKED info_player_start (1 0 0) (-16 -16 -24) (16 16 32)
 Single player spawn point.

 -------- Keys --------
 angle : The angle at which the player will face when spawned.
*/
void G_info_player_start(g_entity_t *self) {
	G_InitPlayerSpawn(self);
}

/*QUAKED info_player_intermission (1 0 1) (-16 -16 -24) (16 16 32)
 Camera for intermission screen between matches.

 -------- Keys --------
 angles : The "pitch yaw roll" angles for the camera (e.g. 20 270 0).
 target : The target name of an info_notnull as an alternate way to set the camera angles.
*/
void G_info_player_intermission(g_entity_t *self) {
	G_InitPlayerSpawn(self);
}

/*QUAKED info_player_deathmatch (1 0 1) (-16 -16 -24) (16 16 32)
 Deathmatch spawn point.

 -------- Keys --------
 angle : The angle at which the player will face when spawned.
*/
void G_info_player_deathmatch(g_entity_t *self) {
	G_InitPlayerSpawn(self);
}

/*QUAKED info_player_team1 (0 0 1) (-16 -16 -24) (16 16 32)
 Player spawn point for red team in teams or CTF gameplay.

 -------- Keys --------
 angle : The angle at which the player will face when spawned.
*/
void G_info_player_team1(g_entity_t *self) {
	G_InitPlayerSpawn(self);
}

/*QUAKED info_player_team2 (1 0 0) (-16 -16 -24) (16 16 32)
 Player spawn point for blue team in teams or CTF gameplay.

 -------- Keys --------
 angle : The angle at which the player will face when spawned.
*/
void G_info_player_team2(g_entity_t *self) {
	G_InitPlayerSpawn(self);
}

/*QUAKED info_player_team3 (1 0 0) (-16 -16 -24) (16 16 32)
 Player spawn point for green team in teams or CTF gameplay.

 -------- Keys --------
 angle : The angle at which the player will face when spawned.
*/
void G_info_player_team3(g_entity_t *self) {
	G_InitPlayerSpawn(self);
}

/*QUAKED info_player_team4 (1 0 0) (-16 -16 -24) (16 16 32)
 Player spawn point for orange team in teams or CTF gameplay.

 -------- Keys --------
 angle : The angle at which the player will face when spawned.
*/
void G_info_player_team4(g_entity_t *self) {
	G_InitPlayerSpawn(self);
}

/*QUAKED G_info_player_team_any (1 0 0) (-16 -16 -24) (16 16 32)
 Player spawn point for any team in teams or CTF gameplay.

 -------- Keys --------
 angle : The angle at which the player will face when spawned.
*/
void G_info_player_team_any(g_entity_t *self) {
	G_InitPlayerSpawn(self);
}
