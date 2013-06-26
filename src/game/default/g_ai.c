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
static void G_Ai_ClientThink(g_edict_t *self) {
	user_cmd_t cmd;

	memset(&cmd, 0, sizeof(cmd));
	cmd.msec = gi.frame_millis;

	G_ClientThink(self, &cmd);

	self->locals.next_think = g_level.time + gi.frame_millis;
}

/*
 * @brief
 */
static void G_Ai_Add_f(void) {
	int32_t i;

	g_edict_t *ent = &g_game.edicts[1];
	for (i = 1; i <= sv_max_clients->integer; i++, ent++) {
		if (!g_game.edicts[i].in_use) {
			break;
		}
	}

	if (i > sv_max_clients->integer) {
		gi.Print("No client slots available, increase sv_max_clients\n");
		return;
	}

	ent->ai = true; // and away we go!

	G_ClientConnect(ent, "\\name\\newbie\\skin\\qforcer/enforcer");
	G_ClientBegin(ent);

	gi.Debug("Spawned %s at %s", ent->client->locals.persistent.net_name, vtos(ent->s.origin));

	ent->locals.Think = G_Ai_ClientThink;
}

/*
 * @brief
 */
void G_Ai_Init(void) {

	gi.Cmd("g_ai_add", G_Ai_Add_f, CMD_GAME, NULL);
}

/*
 * @brief
 */
void G_Ai_Shutdown(void) {

}
