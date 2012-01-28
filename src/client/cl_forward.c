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

// a copy of the last item we dropped, for %d
static char last_dropped_item[MAX_TOKEN_CHARS];

/*
 * Cl_ExpandVariable
 *
 * This is the client-specific sibling to Cvar_VariableString.
 */
static const char *Cl_ExpandVariable(char v) {
	int i;

	switch (v) {

	case 'l': // client's location
		return Cl_LocationHere();
	case 'L': // client's line of sight
		return Cl_LocationThere();

	case 'd': // last dropped item
		return last_dropped_item;

	case 'h': // health
		if (!cl.frame.valid)
			return "";
		i = cl.frame.ps.stats[STAT_HEALTH];
		return va("%d", i);

	case 'a': // armor
		if (!cl.frame.valid)
			return "";
		i = cl.frame.ps.stats[STAT_ARMOR];
		return va("%d", i);

	default:
		return "";
	}
}

/*
 * Cl_ExpandVariables
 */
static char *Cl_ExpandVariables(const char *text) {
	static char expanded[MAX_STRING_CHARS];
	int i, j, len;

	if (!text || !text[0])
		return "";

	memset(expanded, 0, sizeof(expanded));
	len = strlen(text);

	for (i = j = 0; i < len; i++) {
		if (text[i] == '%' && i < len - 1) { // expand %variables
			const char *c = Cl_ExpandVariable(text[i + 1]);
			strcat(expanded, c);
			j += strlen(c);
			i++;
		} else
			// or just append normal chars
			expanded[j++] = text[i];
	}

	return expanded;
}

/*
 * CL_ForwardCmdToServer
 *
 * Client implementation of Cmd_ForwardToServer. Any commands not recognized
 * locally by the client will be sent to the server. Some will undergo parameter
 * expansion so that players can use macros for locations, weapons, etc.
 */
void Cl_ForwardCmdToServer(void) {
	const char *cmd, *args;

	if (cls.state <= CL_DISCONNECTED) {
		Com_Print("Not connected.\n");
		return;
	}

	cmd = Cmd_Argv(0);

	if (*cmd == '-' || *cmd == '+') {
		Com_Print("Unknown command \"%s\"\n", cmd);
		return;
	}

	args = Cmd_Args();

	if (!strcmp(cmd, "drop")) // maintain last item dropped for 'say %d'
		strncpy(last_dropped_item, args, sizeof(last_dropped_item) - 1);

	if (!strcmp(cmd, "say") || !strcmp(cmd, "say_team"))
		args = Cl_ExpandVariables(args);

	Msg_WriteByte(&cls.netchan.message, CL_CMD_STRING);
	Sb_Print(&cls.netchan.message, cmd);
	if (Cmd_Argc() > 1) {
		Sb_Print(&cls.netchan.message, " ");
		Sb_Print(&cls.netchan.message, args);
	}
}
