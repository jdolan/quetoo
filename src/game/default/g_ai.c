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
static void G_AiThink(g_edict_t *self) {
	user_cmd_t cmd;

	memset(&cmd, 0, sizeof(cmd));

	G_ClientThink(self, &cmd);

	self->locals.next_think = g_level.time + gi.frame_millis;
}

/*
 * @brief
 */
static void G_AiAdd_f(void) {
	int32_t i;

	g_edict_t *ent = &g_game.edicts[1];
	for (i = 1; i <= sv_max_clients->integer; i++, ent++) {
		if (!g_game.edicts[i].in_use) {
			break;
		}
	}

	if (i > sv_max_clients->integer) {
		gi.Print("No client slots available");
		return;
	}

	ent->ai = true; // and away we go!

	G_ClientConnect(ent, "\\name\\newbie\\skin\\qforcer/enforcer");
	G_ClientBegin(ent);

	ent->locals.Think = G_AiThink;
}

/*
 * @brief
 */
void G_InitAi(void) {

	gi.Cmd("g_ai_add", G_AiAdd_f, CMD_GAME, NULL);
}
