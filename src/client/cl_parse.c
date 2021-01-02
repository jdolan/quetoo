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

#include "cl_local.h"
#include "parse.h"

static char *sv_cmd_names[32] = {
	"SV_CMD_BAD",
	"SV_CMD_BASELINE",
	"SV_CMD_CBUF_TEXT",
	"SV_CMD_CONFIG_STRING",
	"SV_CMD_DISCONNECT",
	"SV_CMD_DOWNLOAD",
	"SV_CMD_DROP",
	"SV_CMD_FRAME",
	"SV_CMD_PRINT",
	"SV_CMD_RECONNECT",
	"SV_CMD_SERVER_DATA",
	"SV_CMD_SOUND"
};

/**
 * @brief Returns true if the file exists, otherwise it attempts to start a download
 * from the server.
 */
_Bool Cl_CheckOrDownloadFile(const char *filename) {
	char cmd[MAX_STRING_CHARS];

	if (cls.state == CL_DISCONNECTED) {
		Com_Print("Not connected\n");
		return true;
	}

	if (IS_INVALID_DOWNLOAD(filename)) {
		Com_Warn("Refusing to download \"%s\"\n", filename);
		return true;
	}

	Com_Debug(DEBUG_CLIENT, "Checking for %s\n", filename);

	if (Fs_Exists(filename)) { // it exists, no need to download
		return true;
	}

	Com_Debug(DEBUG_CLIENT, "Attempting to download %s\n", filename);

	g_strlcpy(cls.download.name, filename, sizeof(cls.download.name));

	// UDP downloads to a temp name, and only renames when done
	StripExtension(cls.download.name, cls.download.tempname);
	g_strlcat(cls.download.tempname, ".tmp", sizeof(cls.download.tempname));

	// attempt an HTTP download if available
	if (cls.download_url[0] && Cl_HttpDownload()) {
		return false;
	}

	// check to see if we already have a temp for this file, if so, try to resume
	// open the file if not opened yet

	if (Fs_Exists(cls.download.tempname)) { // a temp file exists, resume download
		int64_t len = Fs_Load(cls.download.tempname, NULL);

		if ((cls.download.file = Fs_OpenAppend(cls.download.tempname))) {

			if (Fs_Seek(cls.download.file, len - 1)) {
				// give the server the offset to start the download
				Com_Debug(DEBUG_CLIENT, "Resuming %s...\n", cls.download.name);

				g_snprintf(cmd, sizeof(cmd), "download %s %u", cls.download.name, (uint32_t) len);
				Net_WriteByte(&cls.net_chan.message, CL_CMD_STRING);
				Net_WriteString(&cls.net_chan.message, cmd);

				return false;
			}
		}
	}

	// or start if from the beginning
	Com_Debug(DEBUG_CLIENT, "Downloading %s...\n", cls.download.name);

	g_snprintf(cmd, sizeof(cmd), "download %s", cls.download.name);
	Net_WriteByte(&cls.net_chan.message, CL_CMD_STRING);
	Net_WriteString(&cls.net_chan.message, cmd);

	return false;
}

/**
 * @brief Manually request a download from the server.
 */
void Cl_Download_f(void) {

	if (Cmd_Argc() != 2) {
		Com_Print("Usage: %s <filename>\n", Cmd_Argv(0));
		return;
	}

	Cl_CheckOrDownloadFile(Cmd_Argv(1));
}

/**
 * @brief The server sends this command just after server_data. Hang onto the spawn
 * count and check for the media we'll need to enter the game.
 */
void Cl_Precache_f(void) {

	if (Cmd_Argc() != 2) {
		Com_Print("Usage: %s <spawn_count>\n", Cmd_Argv(0));
		return;
	}

	cls.spawn_count = (uint32_t) strtoul(Cmd_Argv(1), NULL, 0);

	cl.precache_check = CS_ZIP;

	Cl_RequestNextDownload();
}

/**
 * @brief
 */
static void Cl_ParseBaseline(void) {
	static entity_state_t null_state;

	const uint16_t number = Net_ReadShort(&net_message);
	const uint16_t bits = Net_ReadShort(&net_message);

	cl_entity_t *ent = &cl.entities[number];

	Net_ReadDeltaEntity(&net_message, &null_state, &ent->baseline, number, bits);

	// initialize clipping matrices
	if (ent->baseline.solid) {
		if (ent->baseline.solid == SOLID_BSP) {
			Matrix4x4_CreateFromEntity(&ent->matrix, ent->baseline.origin, ent->baseline.angles, 1.0);
			Matrix4x4_Invert_Simple(&ent->inverse_matrix, &ent->matrix);
		} else { // bounding-box entities
			Matrix4x4_CreateFromEntity(&ent->matrix, ent->baseline.origin, Vec3_Zero(), 1.0);
			Matrix4x4_Invert_Simple(&ent->inverse_matrix, &ent->matrix);
		}
	}
}

/**
 * @brief
 */
static void Cl_ParseCbufText(void) {

	const char *text = Net_ReadString(&net_message);

	Cbuf_AddText(text);
}

/**
 * @brief
 */
void Cl_ParseConfigString(void) {
	const uint16_t i = (uint16_t) Net_ReadShort(&net_message);

	if (i >= MAX_CONFIG_STRINGS) {
		Com_Error(ERROR_DROP, "Invalid index %i\n", i);
	}

	strcpy(cl.config_strings[i], Net_ReadString(&net_message));
	const char *s = cl.config_strings[i];

	if (i > CS_MODELS && i < CS_MODELS + MAX_MODELS) {
		if (cls.state == CL_ACTIVE) {
			cl.models[i - CS_MODELS] = R_LoadModel(s);
			if (*s == '*') {
				cl.cm_models[i - CS_MODELS] = Cm_Model(s);
			} else {
				cl.cm_models[i - CS_MODELS] = NULL;
			}
		}
	} else if (i >= CS_SOUNDS && i < CS_SOUNDS + MAX_SOUNDS) {
		if (cls.state == CL_ACTIVE) {
			cl.sounds[i - CS_SOUNDS] = S_LoadSample(s);
		}
	} else if (i >= CS_IMAGES && i < CS_IMAGES + MAX_IMAGES) {
		if (cls.state == CL_ACTIVE) {
			cl.images[i - CS_IMAGES] = R_LoadImage(s, IT_PIC);
		}
	}

	cls.cgame->UpdateConfigString(i);
}

/**
 * @brief A download message has been received from the server.
 */
static void Cl_ParseDownload(void) {
	int32_t size, percent;

	// read the data
	size = Net_ReadShort(&net_message);
	percent = Net_ReadByte(&net_message);
	if (size < 0) {
		Com_Debug(DEBUG_CLIENT, "Server does not have this file\n");
		if (cls.download.file) {
			// if here, we tried to resume a file but the server said no
			Fs_Close(cls.download.file);
			cls.download.file = NULL;
		}
		Cl_RequestNextDownload();
		return;
	}

	// open the file if not opened yet
	if (!cls.download.file) {

		if (!(cls.download.file = Fs_OpenWrite(cls.download.tempname))) {
			net_message.read += size;
			Com_Warn("Failed to open %s\n", cls.download.tempname);
			Cl_RequestNextDownload();
			return;
		}
	}

	Fs_Write(cls.download.file, net_message.data + net_message.read, 1, size);

	net_message.read += size;

	if (percent != 100) {
		Net_WriteByte(&cls.net_chan.message, CL_CMD_STRING);
		Net_WriteString(&cls.net_chan.message, "next_download");
	} else {
		Fs_Close(cls.download.file);
		cls.download.file = NULL;

		// add new archives to the search path
		if (Fs_Rename(cls.download.tempname, cls.download.name)) {
			if (strstr(cls.download.name, ".pk3")) {
				Fs_AddToSearchPath(cls.download.name);
			}
		} else {
			Com_Error(ERROR_DROP, "Failed to rename %s\n", cls.download.name);
		}

		// get another file if needed
		Cl_RequestNextDownload();
	}
}

/**
 * @brief
 */
static void Cl_ParseServerData(void) {

	// wipe the cl_client_t struct
	Cl_ClearState();

	Cl_SetKeyDest(KEY_CONSOLE);

	// parse protocol version number
	const int32_t major = Net_ReadLong(&net_message);
	const int32_t minor = Net_ReadLong(&net_message);

	// ensure protocol major matches
	if (major != PROTOCOL_MAJOR) {
		Com_Error(ERROR_DROP, "Server is using protocol major %d, you have %d\n", major, PROTOCOL_MAJOR);
	}

	// determine if we're viewing a demo
	cl.demo_server = Net_ReadByte(&net_message);

	// game directory
	char *str = Net_ReadString(&net_message);
	if (g_strcmp0(Cvar_GetString("game"), str)) {

		Fs_SetGame(str);

		// reload the client game
		Cl_InitCgame();
	}

	// ensure protocol minor matches
	if (minor != cls.cgame->protocol) {
		Com_Error(ERROR_DROP, "Server is using protocol minor %d, you have %d\n", minor, cls.cgame->protocol);
	}

	// parse client slot number, which is our entity number + 1
	cl.client_num = Net_ReadShort(&net_message);

	// get the full level name
	str = Net_ReadString(&net_message);
	Com_Print("\n");
	Com_Print("^2%s^7\n", str);
}

/**
 * @brief Parses an incoming SVC_PRINT message.
 */
static void Cl_ParsePrint(void) {

	const byte level = Net_ReadByte(&net_message);
	const char *string = Net_ReadString(&net_message);

	// the server shouldn't have sent us anything below our level anyway
	if (level >= message_level->integer) {

		// check to see if we should ignore the message
		if (*cl_ignore->string) {
			parser_t parser;
			char pattern[MAX_STRING_CHARS];

			Parse_Init(&parser, cl_ignore->string, PARSER_DEFAULT);

			while (true) {

				if (!Parse_Token(&parser, PARSE_DEFAULT, pattern, sizeof(pattern))) {
					break;
				}

				if (GlobMatch(pattern, string, GLOB_FLAGS_NONE)) {
					return;
				}
			}
		}

		char *sample = NULL;
		switch (level) {
			case PRINT_CHAT:
			case PRINT_TEAM_CHAT:
				if (level == PRINT_CHAT && *cl_chat_sound->string) {
					sample = cl_chat_sound->string;
				} else if (level == PRINT_TEAM_CHAT && *cl_team_chat_sound->string) {
					sample = cl_team_chat_sound->string;
				}
				break;
			default:
				break;
		}

		if (sample) {
			S_AddSample(&(const s_play_sample_t) {
				.sample = S_LoadSample(sample)
			});
		}

		Con_Append(level, string);
	}
}

/**
 * @brief
 */
static void Cl_ParseSound(void) {

	s_play_sample_t play = {
		.flags = 0,
		.atten = SOUND_ATTEN_LINEAR
	};

	const byte flags = Net_ReadByte(&net_message);

	const int32_t sound = Net_ReadByte(&net_message);
	if (sound >= MAX_SOUNDS) {
		Com_Error(ERROR_DROP, "Bad sound (%d)\n", sound);
	}

	play.sample = cl.sounds[sound];
	play.atten = Net_ReadByte(&net_message);

	if (flags & S_ENTITY) {
		play.entity = Net_ReadShort(&net_message);

		const cl_entity_t *ent = &cl.entities[play.entity];
		if (ent->current.solid == SOLID_BSP) {
			play.origin = Vec3_Scale(Vec3_Add(ent->abs_mins, ent->abs_maxs), .5f);
		} else {
			play.origin = cl.entities[play.entity].current.origin;
		}
	} else {
		play.entity = -1;
	}

	if (flags & S_ORIGIN) { // positioned in space
		play.origin = Net_ReadPosition(&net_message);
	}

	if (flags & S_PITCH) {
		play.pitch = Net_ReadChar(&net_message) * 2;
	}

	S_AddSample(&play);
}

/**
 * @brief
 */
static void Cl_ShowNet(const char *s) {
	if (cl_draw_net_messages->integer >= 2) {
		Com_Print("%3u: %s\n", (uint32_t) (net_message.read - 1), s);
	}
}

/**
 * @brief
 */
void Cl_ParseServerMessage(void) {
	int32_t cmd, old_cmd;

	if (cl_draw_net_messages->integer == 1) {
		Com_Print("%u ", (uint32_t) net_message.size);
	} else if (cl_draw_net_messages->integer >= 2) {
		Com_Print("------------------\n");
	}

	cl.suppress_count = 0;

	cmd = SV_CMD_BAD;

	// parse the message
	while (true) {
		if (net_message.read > net_message.size) {
			Com_Error(ERROR_DROP, "Bad server message\n");
		}

		old_cmd = cmd;
		cmd = Net_ReadByte(&net_message);

		if (cmd == -1) {
			Cl_ShowNet("END OF MESSAGE");
			break;
		}

		if (cl_draw_net_messages->integer >= 2 && sv_cmd_names[cmd]) {
			Cl_ShowNet(sv_cmd_names[cmd]);
		}

		switch (cmd) {

			case SV_CMD_BASELINE:
				Cl_ParseBaseline();
				break;

			case SV_CMD_CBUF_TEXT:
				Cl_ParseCbufText();
				break;

			case SV_CMD_CONFIG_STRING:
				Cl_ParseConfigString();
				break;

			case SV_CMD_DISCONNECT:
				Cl_Disconnect();
				break;

			case SV_CMD_DOWNLOAD:
				Cl_ParseDownload();
				break;

			case SV_CMD_DROP:
				Com_Error(ERROR_DROP, "Server dropped connection\n");

			case SV_CMD_FRAME:
				Cl_ParseFrame();
				break;

			case SV_CMD_PRINT:
				Cl_ParsePrint();
				break;

			case SV_CMD_RECONNECT:
				Com_Print("Server disconnected, reconnecting...\n");
				// stop download
				if (cls.download.file) {
					if (cls.download.http) { // clean up http downloads
						Cl_HttpDownload_Complete();
					} else { // or just stop UDP ones
						Fs_Close(cls.download.file);
					}
					cls.download.name[0] = '\0';
					cls.download.file = NULL;
				}
				cls.state = CL_CONNECTING;
				cls.connect_time = 0; // fire immediately
				break;

			case SV_CMD_SERVER_DATA:
				Cl_ParseServerData();
				break;

			case SV_CMD_SOUND:
				Cl_ParseSound();
				break;

			default:
				// delegate to the client game module before failing
				if (!cls.cgame->ParseMessage(cmd)) {
					Com_Error(ERROR_DROP, "Illegible server message:\n"
					          " %d: last command was %s\n", cmd, sv_cmd_names[old_cmd]);
				}
				break;
		}
	}

	Cl_AddNetGraph();

	Cl_WriteDemoMessage();
}
