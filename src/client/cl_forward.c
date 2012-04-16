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

#include "cl_local.h"

/**
 * CL_ForwardCmdToServer
 *
 * Client implementation of Cmd_ForwardToServer. Any commands not recognized
 * locally by the client will be sent to the server. Some will undergo parameter
 * expansion so that players can use macros for locations, weapons, etc.
 */
void Cl_ForwardCmdToServer(void) {

	if (cls.state <= CL_DISCONNECTED) {
		Com_Print("Not connected.\n");
		return;
	}

	const char *cmd = Cmd_Argv(0);
	char *args = Cmd_Args();

	Msg_WriteByte(&cls.netchan.message, CL_CMD_STRING);
	Sb_Print(&cls.netchan.message, va("%s %s", cmd, args));

	//Com_Debug("Forwarding '%s %s'\n", cmd, args);
}
