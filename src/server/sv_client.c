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

#include "sv_local.h"

/*
 * Sv_New_f
 *
 * Sends the first message from the server to a connected client.
 * This will be sent on the initial connection and upon each server load.
 */
static void Sv_New_f(void) {
	int player_num;

	Com_Debug("New() from %s\n", Sv_NetaddrToString(sv_client));

	if (sv_client->state != SV_CLIENT_CONNECTED) {
		Com_Warn("Sv_New_f: %s already spawned\n",
				Sv_NetaddrToString(sv_client));
		return;
	}

	// demo servers will send the demo file's server info packet
	if (sv.state == SV_ACTIVE_DEMO) {
		return;
	}

	// send the server data
	Msg_WriteByte(&sv_client->netchan.message, SV_CMD_SERVER_DATA);
	Msg_WriteLong(&sv_client->netchan.message, PROTOCOL);
	Msg_WriteLong(&sv_client->netchan.message, svs.spawn_count);
	Msg_WriteLong(&sv_client->netchan.message, svs.frame_rate);
	Msg_WriteByte(&sv_client->netchan.message, 0);
	Msg_WriteString(&sv_client->netchan.message, Cvar_GetString("game"));

	player_num = sv_client - svs.clients;
	Msg_WriteShort(&sv_client->netchan.message, player_num);

	// send full level name
	Msg_WriteString(&sv_client->netchan.message, sv.config_strings[CS_NAME]);

	// begin fetching config_strings
	Msg_WriteByte(&sv_client->netchan.message, SV_CMD_CBUF_TEXT);
	Msg_WriteString(&sv_client->netchan.message,
			va("config_strings %i 0\n", svs.spawn_count));
}

/*
 * Sv_ConfigStrings_f
 */
static void Sv_ConfigStrings_f(void) {
	unsigned int start;

	Com_Debug("ConfigStrings() from %s\n", Sv_NetaddrToString(sv_client));

	if (sv_client->state != SV_CLIENT_CONNECTED) {
		Com_Warn("Sv_ConfigStrings_f: %s already spawned\n",
				Sv_NetaddrToString(sv_client));
		return;
	}

	// handle the case of a level changing while a client was connecting
	if (strtoul(Cmd_Argv(1), NULL, 0) != svs.spawn_count) {
		Com_Debug("Sv_ConfigStrings_f: Stale spawn count from %s\n",
				Sv_NetaddrToString(sv_client));
		Sv_New_f();
		return;
	}

	start = strtoul(Cmd_Argv(2), NULL, 0);

	if (start >= MAX_CONFIG_STRINGS) { // catch bad offsets
		Com_Warn("Sv_ConfigStrings_f: Bad config_string offset from %s\n",
				Sv_NetaddrToString(sv_client));
		Sv_KickClient(sv_client, NULL);
		return;
	}

	// write a packet full of data
	while (sv_client->netchan.message.size < MAX_MSG_SIZE / 2 && start
			< MAX_CONFIG_STRINGS) {
		if (sv.config_strings[start][0]) {
			Msg_WriteByte(&sv_client->netchan.message, SV_CMD_CONFIG_STRING);
			Msg_WriteShort(&sv_client->netchan.message, start);
			Msg_WriteString(&sv_client->netchan.message,
					sv.config_strings[start]);
		}
		start++;
	}

	// send next command
	if (start == MAX_CONFIG_STRINGS) {
		Msg_WriteByte(&sv_client->netchan.message, SV_CMD_CBUF_TEXT);
		Msg_WriteString(&sv_client->netchan.message,
				va("baselines %i 0\n", svs.spawn_count));
	} else {
		Msg_WriteByte(&sv_client->netchan.message, SV_CMD_CBUF_TEXT);
		Msg_WriteString(&sv_client->netchan.message,
				va("config_strings %i %i\n", svs.spawn_count, start));
	}
}

/*
 * Sv_Baselines_f
 */
static void Sv_Baselines_f(void) {
	unsigned int start;
	entity_state_t nullstate;
	entity_state_t *base;

	Com_Debug("Baselines() from %s\n", Sv_NetaddrToString(sv_client));

	if (sv_client->state != SV_CLIENT_CONNECTED) {
		Com_Warn("Sv_Baselines_f: %s already spawned\n",
				Sv_NetaddrToString(sv_client));
		return;
	}

	// handle the case of a level changing while a client was connecting
	if (strtoul(Cmd_Argv(1), NULL, 0) != svs.spawn_count) {
		Com_Debug("Sv_Baselines_f: Stale spawn count from %s\n",
				Sv_NetaddrToString(sv_client));
		Sv_New_f();
		return;
	}

	start = strtoul(Cmd_Argv(2), NULL, 0);

	memset(&nullstate, 0, sizeof(nullstate));

	// write a packet full of data
	while (sv_client->netchan.message.size < MAX_MSG_SIZE / 2 && start
			< MAX_EDICTS) {
		base = &sv.baselines[start];
		if (base->model1 || base->sound || base->effects) {
			Msg_WriteByte(&sv_client->netchan.message, SV_CMD_ENTITY_BASELINE);
			Msg_WriteDeltaEntity(&nullstate, base, &sv_client->netchan.message,
					true, true);
		}
		start++;
	}

	// send next command
	if (start == MAX_EDICTS) {
		Msg_WriteByte(&sv_client->netchan.message, SV_CMD_CBUF_TEXT);
		Msg_WriteString(&sv_client->netchan.message,
				va("precache %i\n", svs.spawn_count));
	} else {
		Msg_WriteByte(&sv_client->netchan.message, SV_CMD_CBUF_TEXT);
		Msg_WriteString(&sv_client->netchan.message,
				va("baselines %i %i\n", svs.spawn_count, start));
	}
}

/*
 * Sv_Begin_f
 */
static void Sv_Begin_f(void) {

	Com_Debug("Begin() from %s\n", Sv_NetaddrToString(sv_client));

	if (sv_client->state != SV_CLIENT_CONNECTED) { // catch duplicate spawns
		Com_Warn("Sv_Begin_f: Invalid Begin() from %s\n",
				Sv_NetaddrToString(sv_client));
		Sv_KickClient(sv_client, NULL);
		return;
	}

	if (sv.state == SV_ACTIVE_DEMO)
		return;

	// handle the case of a level changing while a client was connecting
	if (strtoul(Cmd_Argv(1), NULL, 0) != svs.spawn_count) {
		Com_Debug("Sv_Begin_f: Stale spawn count from %s\n",
				Sv_NetaddrToString(sv_client));
		Sv_New_f();
		return;
	}

	sv_client->state = SV_CLIENT_ACTIVE;

	// call the game begin function
	svs.game->ClientBegin(sv_player);

	Cbuf_InsertFromDefer();
}

/*
 * Sv_NextDownload_f
 */
static void Sv_NextDownload_f(void) {
	int r, size, percent;
	byte buf[MAX_MSG_SIZE];
	size_buf_t msg;

	if (!sv_client->download)
		return;

	r = sv_client->download_size - sv_client->download_count;

	if (r > 1024) // cap the download chunk at 1024 bytes
		r = 1024;

	Sb_Init(&msg, buf, MAX_MSG_SIZE);

	Msg_WriteByte(&msg, SV_CMD_DOWNLOAD);
	Msg_WriteShort(&msg, r);

	sv_client->download_count += r;
	size = sv_client->download_size;
	if (!size)
		size = 1;

	percent = sv_client->download_count * 100 / size;
	Msg_WriteByte(&msg, percent);

	Sb_Write(&msg, sv_client->download + sv_client->download_count - r, r);
	Sb_Write(&sv_client->netchan.message, msg.data, msg.size);

	if (sv_client->download_count != sv_client->download_size)
		return;

	Fs_FreeFile(sv_client->download);
	sv_client->download = NULL;
}

// only these prefixes are valid downloads, all else are denied
static const char *downloadable[] = {
	"*.pak", "maps/*", "sounds/*", "env/*", "textures/*", NULL
};

/*
 * Sv_Download_f
 */
static void Sv_Download_f(void) {
	const char *name;
	void *buf;
	unsigned int i = 0, offset = 0;

	name = Cmd_Argv(1);

	if (Cmd_Argc() > 2)
		offset = strtoul(Cmd_Argv(2), NULL, 0); // downloaded offset

	// catch illegal offset or file_names
	if (*name == '.' || *name == '/' || *name == '\\' || strstr(
			name, "..")) {
		Com_Warn("Sv_Download_f: Malicious download (%s:%d) from %s\n", name,
				offset, Sv_NetaddrToString(sv_client));
		Sv_KickClient(sv_client, NULL);
		return;
	}

	while (downloadable[i]) { // ensure download name is allowed
		if (GlobMatch(downloadable[i], name))
			break;
		i++;
	}

	if (!downloadable[i]) { // it wasn't
		Com_Warn("Sv_Download_f: Illegal download (%s) from %s\n", name,
				Sv_NetaddrToString(sv_client));
		Sv_KickClient(sv_client, NULL);
		return;
	}

	if (!sv_udp_download->value) { // lastly, ensure server wishes to allow
		Msg_WriteByte(&sv_client->netchan.message, SV_CMD_DOWNLOAD);
		Msg_WriteShort(&sv_client->netchan.message, -1);
		Msg_WriteByte(&sv_client->netchan.message, 0);
		return;
	}

	if (sv_client->download) // free last download
		Fs_FreeFile(sv_client->download);

	sv_client->download_size = Fs_LoadFile(name, &buf);
	sv_client->download = (byte *) buf;
	sv_client->download_count = offset;

	if (offset > sv_client->download_size)
		sv_client->download_count = sv_client->download_size;

	if (!sv_client->download) { // legal file name, but missing file
		Com_Warn("Sv_Download_f: Couldn't download %s to %s\n", name,
				Sv_NetaddrToString(sv_client));
		Msg_WriteByte(&sv_client->netchan.message, SV_CMD_DOWNLOAD);
		Msg_WriteShort(&sv_client->netchan.message, -1);
		Msg_WriteByte(&sv_client->netchan.message, 0);
		return;
	}

	Sv_NextDownload_f();
	Com_Debug("Downloading %s to %s\n", name, sv_client->name);
}

/*
 * Sv_Disconnect_f
 *
 * The client is going to disconnect, so remove the connection immediately
 */
static void Sv_Disconnect_f(void) {
	Sv_DropClient(sv_client);
}

/*
 * Sv_Serverinfo_f
 *
 * Dumps the serverinfo info string
 */
static void Sv_Info_f(void) {
	const cvar_t *cvar;
	char line[MAX_STRING_CHARS];

	if (!sv_client) { // print to server console
		Com_PrintInfo(Cvar_ServerInfo());
		return;
	}

	for (cvar = cvar_vars; cvar; cvar = cvar->next) {

		if (!(cvar->flags & CVAR_SERVER_INFO))
			continue; //only print serverinfo cvars

		snprintf(line, sizeof(line), "%s %s\n", cvar->name, cvar->string);
		Sv_ClientPrint(sv_client->edict, PRINT_MEDIUM, "%s", line);
	}
}

typedef struct sv_user_string_cmd_s {
	char *name;
	void (*func)(void);
} sv_user_string_cmd_t;

sv_user_string_cmd_t sv_user_string_cmds[] = { { "new", Sv_New_f }, {
		"config_strings", Sv_ConfigStrings_f },
		{ "baselines", Sv_Baselines_f }, { "begin", Sv_Begin_f }, {
				"disconnect", Sv_Disconnect_f }, { "info", Sv_Info_f }, {
				"download", Sv_Download_f }, { "nextdl", Sv_NextDownload_f }, {
				NULL, NULL } };

/*
 * Sv_UserStringCommand
 *
 * Invoke the specified user string command.  If we don't have a function for
 * it, pass it off to the game module.
 */
static void Sv_UserStringCommand(const char *s) {
	sv_user_string_cmd_t *c;

	Cmd_TokenizeString(s);

	if (strchr(s, '\xFF')) { // catch end of message exploit
		Com_Warn("Sv_ExecuteUserCommand: Illegal command from %s\n",
				Sv_NetaddrToString(sv_client));
		Sv_KickClient(sv_client, NULL);
		return;
	}

	for (c = sv_user_string_cmds; c->name; c++) {

		if (!strcmp(Cmd_Argv(0), c->name)) {
			c->func();
			break;
		}
	}

	if (!c->name) { // unmatched command

		if (sv.state == SV_ACTIVE_GAME) // maybe the game knows what to do with it
			svs.game->ClientCommand(sv_player);
	}
}

/*
 * Sv_ClientThink
 *
 * Account for command time and pass the command to game module.
 */
static void Sv_ClientThink(sv_client_t *cl, user_cmd_t *cmd) {

	cl->cmd_msec -= cmd->msec;

	svs.game->ClientThink(cl->edict, cmd);
}

#define CMD_MAX_MSEC 250
#define CMD_MAX_MOVES 1
#define CMD_MAX_STRINGS 8

/*
 * Sv_ParseClientMessage
 *
 * The current net_message is parsed for the given client.
 */
void Sv_ParseClientMessage(sv_client_t *cl) {
	user_cmd_t null_cmd, oldest_cmd, old_cmd, new_cmd;
	int net_drop;
	int strings_issued;
	int moves_issued;
	int last_frame;
	int c;
	char *s;

	sv_client = cl;
	sv_player = sv_client->edict;

	// allow a finite number of moves and strings
	moves_issued = strings_issued = 0;

	while (true) {

		if (net_message.read > net_message.size) {
			Com_Warn("Sv_ParseClientMessage: Bad read from %s\n",
					Sv_NetaddrToString(sv_client));
			Sv_DropClient(cl);
			return;
		}

		c = Msg_ReadByte(&net_message);
		if (c == -1)
			break;

		switch (c) {

		case CL_CMD_USER_INFO:
			strncpy(cl->user_info, Msg_ReadString(&net_message), sizeof(cl->user_info) - 1);
			Sv_UserInfoChanged(cl);
			break;

		case CL_CMD_MOVE:
			if (++moves_issued > CMD_MAX_MOVES) {
				return; // someone is trying to cheat
			}

			last_frame = Msg_ReadLong(&net_message);
			if (last_frame != cl->last_frame) {
				cl->last_frame = last_frame;
				if (cl->last_frame > -1) {
					cl->frame_latency[cl->last_frame & (CLIENT_LATENCY_COUNTS
							- 1)] = svs.real_time - cl->frames[cl->last_frame
							& UPDATE_MASK].sent_time;
				}
			}

			memset(&null_cmd, 0, sizeof(null_cmd));
			Msg_ReadDeltaUsercmd(&net_message, &null_cmd, &oldest_cmd);
			Msg_ReadDeltaUsercmd(&net_message, &oldest_cmd, &old_cmd);
			Msg_ReadDeltaUsercmd(&net_message, &old_cmd, &new_cmd);

			// don't start delta compression until the client is spawned
			// TODO: should this be a little higher up?
			if (cl->state != SV_CLIENT_ACTIVE) {
				cl->last_frame = -1;
				break;
			}

			// catch extremely high msec movements
			if (null_cmd.msec > CMD_MAX_MSEC || oldest_cmd.msec > CMD_MAX_MSEC
					|| old_cmd.msec > CMD_MAX_MSEC || new_cmd.msec
					> CMD_MAX_MSEC) {
				Com_Warn("Sv_ParseClientMessage: Illegal msec from %s\n",
						Sv_NetaddrToString(cl));
				Sv_KickClient(cl, "Illegal movement");
				return;
			}

			net_drop = cl->netchan.dropped;
			if (net_drop < 20) {
				while (net_drop > 2) {
					Sv_ClientThink(cl, &cl->last_cmd);
					net_drop--;
				}
				if (net_drop > 1)
					Sv_ClientThink(cl, &oldest_cmd);
				if (net_drop > 0)
					Sv_ClientThink(cl, &old_cmd);
			}
			Sv_ClientThink(cl, &new_cmd);
			cl->last_cmd = new_cmd;
			break;

		case CL_CMD_STRING:
			s = Msg_ReadString(&net_message);

			// malicious users may try using too many string commands
			if (++strings_issued < CMD_MAX_STRINGS)
				Sv_UserStringCommand(s);
			else {
				Com_Warn(
						"Sv_ParseClientMessage: CMD_MAX_STRINGS exceeded for %s\n",
						Sv_NetaddrToString(cl));
				Sv_KickClient(cl, "Too many commands.");
				return;
			}

			if (cl->state == SV_CLIENT_FREE)
				return; // disconnect command
			break;

		default:
			Com_Print("Sv_ParseClientMessage: unknown command %d\n", c);
			Sv_DropClient(cl);
			return;
		}
	}
}
