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

#include "sv_local.h"

/**
 * @brief Sends the first message from the server to a connected client.
 * This will be sent on the initial connection and upon each server load.
 */
static void Sv_New_f(void) {

	Com_Debug(DEBUG_SERVER, "%s\n", Sv_NetaddrToString(sv_client));

	if (sv_client->state != SV_CLIENT_CONNECTED) {
		Com_Warn("%s issued new from %d\n", Sv_NetaddrToString(sv_client), sv_client->state);
		return;
	}

	// demo servers will send the demo file's server info packet
	if (sv.state == SV_ACTIVE_DEMO) {
		return;
	}

	// send the server data
	Net_WriteByte(&sv_client->net_chan.message, SV_CMD_SERVER_DATA);
	Net_WriteShort(&sv_client->net_chan.message, PROTOCOL_MAJOR);
	Net_WriteShort(&sv_client->net_chan.message, svs.game->protocol);
	Net_WriteByte(&sv_client->net_chan.message, 0);
	Net_WriteString(&sv_client->net_chan.message, Cvar_GetString("game"));

	const int32_t client_num = (int32_t) (ptrdiff_t) (sv_client - svs.clients);
	Net_WriteShort(&sv_client->net_chan.message, client_num);

	// send full level name
	Net_WriteString(&sv_client->net_chan.message, sv.config_strings[CS_NAME]);

	// begin fetching config_strings
	Net_WriteByte(&sv_client->net_chan.message, SV_CMD_CBUF_TEXT);
	Net_WriteString(&sv_client->net_chan.message, va("config_strings %i 0\n", svs.spawn_count));
}

/**
 * @brief
 */
static void Sv_ConfigStrings_f(void) {
	uint32_t start;

	Com_Debug(DEBUG_SERVER, "%s\n", Sv_NetaddrToString(sv_client));

	if (sv_client->state != SV_CLIENT_CONNECTED) {
		Com_Warn("%s already spawned\n", Sv_NetaddrToString(sv_client));
		return;
	}

	// handle the case of a level changing while a client was connecting
	if (strtoul(Cmd_Argv(1), NULL, 0) != svs.spawn_count) {
		Com_Debug(DEBUG_SERVER, "Stale spawn count from %s\n", Sv_NetaddrToString(sv_client));
		Sv_New_f();
		return;
	}

	start = (uint32_t) strtoul(Cmd_Argv(2), NULL, 0);

	if (start >= MAX_CONFIG_STRINGS) { // catch bad offsets
		Com_Warn("Bad offset from %s\n", Sv_NetaddrToString(sv_client));
		Sv_KickClient(sv_client, NULL);
		return;
	}

	// write a packet full of data

	net_chan_t *ch = &sv_client->net_chan;

	while (start < MAX_CONFIG_STRINGS) {
		const size_t len = strlen(sv.config_strings[start]);
		if (len) {
			if (ch->message.size + len >= ch->message.max_size - 48) {
				break;
			}
			Net_WriteByte(&sv_client->net_chan.message, SV_CMD_CONFIG_STRING);
			Net_WriteShort(&sv_client->net_chan.message, start);
			Net_WriteString(&sv_client->net_chan.message, sv.config_strings[start]);
		}
		start++;
	}

	// send next command
	if (start == MAX_CONFIG_STRINGS) {
		Net_WriteByte(&sv_client->net_chan.message, SV_CMD_CBUF_TEXT);
		Net_WriteString(&sv_client->net_chan.message, va("baselines %i 0\n", svs.spawn_count));
	} else {
		Net_WriteByte(&sv_client->net_chan.message, SV_CMD_CBUF_TEXT);
		Net_WriteString(&sv_client->net_chan.message,
		                va("config_strings %i %i\n", svs.spawn_count, start));
	}
}

/**
 * @brief
 */
static void Sv_Baselines_f(void) {
	uint32_t start;
	entity_state_t null_state;
	entity_state_t *base;

	Com_Debug(DEBUG_SERVER, "%s\n", Sv_NetaddrToString(sv_client));

	if (sv_client->state != SV_CLIENT_CONNECTED) {
		Com_Warn("%s already spawned\n", Sv_NetaddrToString(sv_client));
		return;
	}

	// handle the case of a level changing while a client was connecting
	if (strtoul(Cmd_Argv(1), NULL, 0) != svs.spawn_count) {
		Com_Debug(DEBUG_SERVER, "Stale spawn count from %s\n", Sv_NetaddrToString(sv_client));
		Sv_New_f();
		return;
	}

	start = (uint32_t) strtoul(Cmd_Argv(2), NULL, 0);

	memset(&null_state, 0, sizeof(null_state));

	// write a packet full of data
	while (sv_client->net_chan.message.size < (MAX_MSG_SIZE >> 1) && start < MAX_ENTITIES) {
		base = &sv.baselines[start];
		if (base->model1 || base->sound || base->effects) {
			Net_WriteByte(&sv_client->net_chan.message, SV_CMD_BASELINE);
			Net_WriteDeltaEntity(&sv_client->net_chan.message, &null_state, base, true);
		}
		start++;
	}

	// send next command
	if (start == MAX_ENTITIES) {
		Net_WriteByte(&sv_client->net_chan.message, SV_CMD_CBUF_TEXT);
		Net_WriteString(&sv_client->net_chan.message, va("precache %i\n", svs.spawn_count));
	} else {
		Net_WriteByte(&sv_client->net_chan.message, SV_CMD_CBUF_TEXT);
		Net_WriteString(&sv_client->net_chan.message,
		                va("baselines %i %i\n", svs.spawn_count, start));
	}
}

/**
 * @brief
 */
static void Sv_Begin_f(void) {

	Com_Debug(DEBUG_SERVER, "%s\n", Sv_NetaddrToString(sv_client));

	if (sv_client->state != SV_CLIENT_CONNECTED) { // catch duplicate spawns
		Com_Warn("Invalid begin from %s\n", Sv_NetaddrToString(sv_client));
		Sv_KickClient(sv_client, NULL);
		return;
	}

	if (sv.state == SV_ACTIVE_DEMO) {
		return;
	}

	// handle the case of a level changing while a client was connecting
	if (strtoul(Cmd_Argv(1), NULL, 0) != svs.spawn_count) {
		Com_Debug(DEBUG_SERVER, "Stale spawn count from %s\n", Sv_NetaddrToString(sv_client));
		Sv_New_f();
		return;
	}

	sv_client->state = SV_CLIENT_ACTIVE;

	// call the game begin function
	svs.game->ClientBegin(sv_client->entity);

	Cbuf_InsertFromDefer();
}

/**
 * @brief
 */
static void Sv_NextDownload_f(void) {
	byte buf[MAX_MSG_SIZE];
	mem_buf_t msg;

	sv_client_download_t *download = &sv_client->download;

	if (!download->buffer) {
		return;
	}

	Mem_InitBuffer(&msg, buf, sizeof(buf));

	int32_t len = clampf(download->size - download->count, 0, 1024);

	Net_WriteByte(&msg, SV_CMD_DOWNLOAD);
	Net_WriteShort(&msg, len);

	int32_t percent = download->count * 100 / (clampf(download->size, 1, download->size));
	Net_WriteByte(&msg, percent);

	Mem_WriteBuffer(&msg, download->buffer + download->count, len);
	Mem_WriteBuffer(&sv_client->net_chan.message, msg.data, msg.size);

	download->count += len;

	if (download->count == download->size) {
		Com_Debug(DEBUG_SERVER, "Finished download to %s\n", Sv_NetaddrToString(sv_client));

		Fs_Free(download->buffer);
		download->buffer = NULL;
	}
}

/**
 * @brief
 */
static void Sv_Download_f(void) {
	const char *allowed_patterns[] = {
		"*.pk3",
		"maps/*",
		"models/*",
		"sounds/*",
		"env/*",
		"textures/*",
		NULL
	};

	const char *filename = Cmd_Argv(1);

	// catch illegal offset or filenames
	if (IS_INVALID_DOWNLOAD(filename)) {
		Com_Warn("Malicious download (%s) from %s\n", filename, Sv_NetaddrToString(sv_client));
		Sv_KickClient(sv_client, NULL);
		return;
	}

	const char **pattern = allowed_patterns;
	while (pattern) { // ensure download name is allowed
		if (GlobMatch(*pattern, filename, GLOB_FLAGS_NONE)) {
			break;
		}
		pattern++;
	}

	if (!pattern) { // it wasn't
		Com_Warn("Illegal download (%s) from %s\n", filename, Sv_NetaddrToString(sv_client));
		Sv_KickClient(sv_client, NULL);
		return;
	}

	if (!sv_udp_download->value) { // ensure server wishes to allow
		Net_WriteByte(&sv_client->net_chan.message, SV_CMD_DOWNLOAD);
		Net_WriteShort(&sv_client->net_chan.message, -1);
		Net_WriteByte(&sv_client->net_chan.message, 0);
		return;
	}

	sv_client_download_t *download = &sv_client->download;

	if (download->buffer) { // free last download
		Fs_Free(download->buffer);
	}

	memset(download, 0, sizeof(*download));

	// try to load the file
	download->size = (int32_t) Fs_Load(filename, (void *) &download->buffer);

	if (download->size == -1) {
		Com_Warn("Couldn't download %s to %s\n", filename, Sv_NetaddrToString(sv_client));
		Net_WriteByte(&sv_client->net_chan.message, SV_CMD_DOWNLOAD);
		Net_WriteShort(&sv_client->net_chan.message, -1);
		Net_WriteByte(&sv_client->net_chan.message, 0);
		return;
	}

	if (Cmd_Argc() > 2) {
		download->count = (int32_t) strtol(Cmd_Argv(2), NULL, 0);
		if (download->count < 0 || download->count > download->size) {
			Com_Warn("Invalid offset (%d) from %s\n", download->count,
			         Sv_NetaddrToString(sv_client));
			download->count = download->size;
		}
	}

	Sv_NextDownload_f();
	Com_Debug(DEBUG_SERVER, "Downloading %s to %s\n", filename, sv_client->name);
}

/**
 * @brief The client is going to disconnect, so remove the connection immediately
 */
static void Sv_Disconnect_f(void) {
	Sv_DropClient(sv_client);
}

/**
 * @brief Enumeration helper for Sv_Info_f.
 */
static void Sv_Info_f_enumerate(cvar_t *var, void *data) {
	sv_client_t *client = (sv_client_t *) data;

	if (var->flags & CVAR_SERVER_INFO) {
		Sv_ClientPrint(client->entity, PRINT_MEDIUM, "%s %s\n", var->name, var->string);
	}
}

/**
 * @brief Dumps the serverinfo info string
 */
static void Sv_Info_f(void) {

	if (!sv_client) { // print to server console
		Com_PrintInfo(Cvar_ServerInfo());
		return;
	}

	Cvar_Enumerate(Sv_Info_f_enumerate, (void *) sv_client);
}

typedef struct sv_user_string_cmd_s {
	char *name;
	void (*func)(void);
} sv_user_string_cmd_t;

static sv_user_string_cmd_t sv_user_string_cmds[] = { // mapping command names to their functions
	{ "new", Sv_New_f },
	{ "config_strings", Sv_ConfigStrings_f },
	{ "baselines", Sv_Baselines_f },
	{ "begin", Sv_Begin_f },
	{ "disconnect", Sv_Disconnect_f },
	{ "info", Sv_Info_f },
	{ "download", Sv_Download_f },
	{ "next_download", Sv_NextDownload_f },
	{ NULL, NULL }
};

/**
 * @brief Invoke the specified user string command. If we don't have a function for
 * it, pass it off to the game module.
 */
static void Sv_UserStringCommand(const char *s) {
	sv_user_string_cmd_t *c;

	Cmd_TokenizeString(s);

	if (strchr(s, '\xFF')) { // catch end of message exploit
		Com_Warn("Illegal command from %s\n", Sv_NetaddrToString(sv_client));
		Sv_KickClient(sv_client, NULL);
		return;
	}

	for (c = sv_user_string_cmds; c->name; c++) {

		if (!g_strcmp0(Cmd_Argv(0), c->name)) {
			c->func();
			break;
		}
	}

	if (!c->name) { // unmatched command
		if (sv.state == SV_ACTIVE_GAME) { // maybe the game knows what to do with it
			svs.game->ClientCommand(sv_client->entity);
		}
	}
}

/**
 * @brief Account for command time and pass the command to game module.
 */
static void Sv_ClientThink(sv_client_t *cl, pm_cmd_t *cmd) {

	cl->cmd_msec += cmd->msec;

	svs.game->ClientThink(cl->entity, cmd);
}

#define CMD_MAX_MOVES 1
#define CMD_MAX_STRINGS 8

/**
 * @brief The current net_message is parsed for the given client.
 */
void Sv_ParseClientMessage(sv_client_t *cl) {
	int32_t strings_issued;
	int32_t moves_issued;

	sv_client = cl;

	// allow a finite number of moves and strings
	moves_issued = strings_issued = 0;

	while (true) {

		if (net_message.read > net_message.size) {
			Com_Warn("Bad read from %s\n", Sv_NetaddrToString(sv_client));
			Sv_DropClient(cl);
			return;
		}

		const int32_t c = Net_ReadByte(&net_message);
		if (c == -1) {
			break;
		}

		switch (c) {

			case CL_CMD_USER_INFO:
				g_strlcpy(cl->user_info, Net_ReadString(&net_message), sizeof(cl->user_info));
				Sv_UserInfoChanged(cl);
				break;

			case CL_CMD_MOVE:

				if (++moves_issued > CMD_MAX_MOVES) {
					return; // someone is trying to cheat
				}

				const int32_t last_frame = Net_ReadLong(&net_message);
				if (last_frame != cl->last_frame) {
					cl->last_frame = last_frame;
					if (cl->last_frame > -1) {
						cl->frame_latency[cl->last_frame & (SV_CLIENT_LATENCY_COUNT - 1)] =
						    quetoo.ticks - cl->frames[cl->last_frame & PACKET_MASK].sent_time;
					}
				}

				static pm_cmd_t null_cmd;
				pm_cmd_t oldest_cmd, old_cmd, new_cmd;
				Net_ReadDeltaMoveCmd(&net_message, &null_cmd, &oldest_cmd);
				Net_ReadDeltaMoveCmd(&net_message, &oldest_cmd, &old_cmd);
				Net_ReadDeltaMoveCmd(&net_message, &old_cmd, &new_cmd);

				// don't start delta compression until the client is spawned
				// TODO: should this be a little higher up?
				if (cl->state != SV_CLIENT_ACTIVE) {
					cl->last_frame = -1;
					break;
				}

				uint32_t net_drop = cl->net_chan.dropped;
				if (net_drop < 20) {
					while (net_drop > 2) {
						Sv_ClientThink(cl, &cl->last_cmd);
						net_drop--;
					}
					if (net_drop > 1) {
						Sv_ClientThink(cl, &oldest_cmd);
					}
					if (net_drop > 0) {
						Sv_ClientThink(cl, &old_cmd);
					}
				}
				Sv_ClientThink(cl, &new_cmd);
				cl->last_cmd = new_cmd;
				break;

			case CL_CMD_STRING:

				// malicious users may try using too many string commands
				if (++strings_issued == CMD_MAX_STRINGS) {
					Com_Warn("CMD_MAX_STRINGS exceeded for %s\n", Sv_NetaddrToString(cl));
					Sv_KickClient(cl, "Too many commands.");
					return;
				}

				Sv_UserStringCommand(Net_ReadString(&net_message));

				if (cl->state == SV_CLIENT_FREE) {
					return; // disconnect command
				}

				break;

			default:
				Com_Print("Sv_ParseClientMessage: unknown command %d\n", c);
				Sv_DropClient(cl);
				return;
		}
	}
}
