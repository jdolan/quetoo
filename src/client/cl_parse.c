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

char *svc_strings[256] = { "svc_bad", "svc_nop", "svc_muzzle_flash",
		"svc_temp_entity", "svc_layout", "svc_disconnect", "svc_reconnect",
		"svc_sound", "svc_print", "svc_stuff_text", "svc_server_data",
		"svc_config_string", "svc_spawn_baseline", "svc_center_print",
		"svc_download", "svc_frame" };

/*
 * Cl_CheckOrDownloadFile
 *
 * Returns true if the file exists, otherwise it attempts
 * to start a download from the server.
 */
boolean_t Cl_CheckOrDownloadFile(const char *file_name) {
	FILE *fp;
	char name[MAX_OSPATH];
	char cmd[MAX_STRING_CHARS];

	if (cls.state == CL_DISCONNECTED) {
		Com_Print("Not connected.\n");
		return true;
	}

	if (file_name[0] == '/') {
		Com_Warn("Refusing to download a path starting with /.\n");
		return true;
	}
	if (strstr(file_name, "..")) {
		Com_Warn("Refusing to download a path with .. .\n");
		return true;
	}
	if (strchr(file_name, ' ')) {
		Com_Warn("Refusing to download a path with whitespace.\n");
		return true;
	}

	Com_Debug("Checking for %s\n", file_name);

	if (Fs_LoadFile(file_name, NULL) != -1) { // it exists, no need to download
		return true;
	}

	Com_Debug("Attempting to download %s\n", file_name);

	strcpy(cls.download.name, file_name);

	// udp downloads to a temp name, and only renames when done
	StripExtension(cls.download.name, cls.download.tempname);
	strcat(cls.download.tempname, ".tmp");

	// attempt an http download if available
	if (cls.download_url[0] && Cl_HttpDownload())
		return false;

	// check to see if we already have a tmp for this file, if so, try to resume
	// open the file if not opened yet
	snprintf(name, sizeof(name), "%s/%s", Fs_Gamedir(), cls.download.tempname);

	fp = fopen(name, "r+b");
	if (fp) { // a temp file exists, resume download
		int len;
		fseek(fp, 0, SEEK_END);
		len = ftell(fp);

		cls.download.file = fp;

		// give the server the offset to start the download
		Com_Debug("Resuming %s...\n", cls.download.name);

		snprintf(cmd, sizeof(cmd), "download %s %i", cls.download.name, len);
		Msg_WriteByte(&cls.netchan.message, CL_CMD_STRING);
		Msg_WriteString(&cls.netchan.message, cmd);
	} else {
		// or start if from the beginning
		Com_Debug("Downloading %s...\n", cls.download.name);

		snprintf(cmd, sizeof(cmd), "download %s", cls.download.name);
		Msg_WriteByte(&cls.netchan.message, CL_CMD_STRING);
		Msg_WriteString(&cls.netchan.message, cmd);
	}

	return false;
}

/*
 * Cl_Download_f
 *
 * Manually request a download from the server.
 */
void Cl_Download_f(void) {

	if (Cmd_Argc() != 2) {
		Com_Print("Usage: %s <file_name>\n", Cmd_Argv(0));
		return;
	}

	Cl_CheckOrDownloadFile(Cmd_Argv(1));
}

/*
 * Cl_ParseDownload
 *
 * A download message has been received from the server
 */
static void Cl_ParseDownload(void) {
	int size, percent;
	char name[MAX_OSPATH];

	// read the data
	size = Msg_ReadShort(&net_message);
	percent = Msg_ReadByte(&net_message);
	if (size < 0) {
		Com_Debug("Server does not have this file.\n");
		if (cls.download.file) {
			// if here, we tried to resume a file but the server said no
			Fs_CloseFile(cls.download.file);
			cls.download.file = NULL;
		}
		Cl_RequestNextDownload();
		return;
	}

	// open the file if not opened yet
	if (!cls.download.file) {
		snprintf(name, sizeof(name), "%s/%s", Fs_Gamedir(), cls.download.tempname);

		Fs_CreatePath(name);

		if (!(cls.download.file = fopen(name, "wb"))) {
			net_message.read += size;
			Com_Warn("Failed to open %s.\n", name);
			Cl_RequestNextDownload();
			return;
		}
	}

	Fs_Write(net_message.data + net_message.read, 1, size, cls.download.file);

	net_message.read += size;

	if (percent != 100) {
		Msg_WriteByte(&cls.netchan.message, CL_CMD_STRING);
		Sb_Print(&cls.netchan.message, "nextdl");
	} else {
		char oldn[MAX_OSPATH];
		char newn[MAX_OSPATH];

		Fs_CloseFile(cls.download.file);

		// rename the temp file to it's final name
		snprintf(oldn, sizeof(oldn), "%s/%s", Fs_Gamedir(), cls.download.tempname);
		snprintf(newn, sizeof(newn), "%s/%s", Fs_Gamedir(), cls.download.name);

		if (rename(oldn, newn))
			Com_Warn("Failed to rename %s to %s.\n", oldn, newn);

		cls.download.file = NULL;

		if (strstr(newn, ".pak")) // append paks to searchpaths
			Fs_AddPakfile(newn);

		// get another file if needed
		Cl_RequestNextDownload();
	}
}

/*
 *
 *   SERVER CONNECTING MESSAGES
 *
 */

/*
 * Cl_ParseServerData
 */
static void Cl_ParseServerData(void) {
	char *str;
	int i;

	// wipe the cl_client_t struct
	Cl_ClearState();

	cls.state = CL_CONNECTED;
	cls.key_state.dest = KEY_CONSOLE;

	// parse protocol version number
	i = Msg_ReadLong(&net_message);

	// ensure protocol matches
	if (i != PROTOCOL) {
		Com_Error(ERR_DROP,
				"Cl_ParseServerData: Server is using unknown protocol %d.\n", i);
	}

	// retrieve spawn count and packet rate
	cl.server_count = Msg_ReadLong(&net_message);
	cl.server_frame_rate = Msg_ReadLong(&net_message);

	// determine if we're viewing a demo
	cl.demo_server = Msg_ReadByte(&net_message);

	// game directory
	str = Msg_ReadString(&net_message);
	if (strcmp(Cvar_GetString("game"), str)) {

		Fs_SetGame(str);

		// reload the client game
		Cl_InitCgame();
	}

	// parse player entity number
	cl.player_num = Msg_ReadShort(&net_message);

	// get the full level name
	str = Msg_ReadString(&net_message);
	Com_Print("\n");
	Com_Print("%c%s\n", 2, str);
}

/*
 * Cl_ParseBaseline
 */
static void Cl_ParseBaseline(void) {
	entity_state_t *state;
	entity_state_t null_state;

	const unsigned short number = Msg_ReadShort(&net_message);
	const unsigned short bits = Msg_ReadShort(&net_message);

	memset(&null_state, 0, sizeof(null_state));
	state = &cl.entities[number].baseline;

	Msg_ReadDeltaEntity(&null_state, state, &net_message, number, bits);
}

/*
 * Cl_ParseGravity
 */
static void Cl_ParseGravity(const char *gravity) {
	int g;

	if ((g = atoi(gravity)) > -1)
		cl_gravity = g;
}

/*
 * Cl_ParseConfigString
 */
void Cl_ParseConfigString(void) {
	const unsigned short i = (unsigned short) Msg_ReadShort(&net_message);

	if (i >= MAX_CONFIG_STRINGS) {
		Com_Error(ERR_DROP, "Cl_ParseConfigString: Invalid index %i.\n", i);
	}

	strcpy(cl.config_strings[i], Msg_ReadString(&net_message));
	const char *s = cl.config_strings[i];

	if (i == CS_GRAVITY)
		Cl_ParseGravity(s);
	else if (i >= CS_MODELS && i < CS_MODELS + MAX_MODELS) {
		if (cls.state == CL_ACTIVE) {
			cl.model_draw[i - CS_MODELS] = R_LoadModel(s);
			if (cl.config_strings[i][0] == '*')
				cl.model_clip[i - CS_MODELS] = Cm_Model(s);
			else
				cl.model_clip[i - CS_MODELS] = NULL;
		}
	} else if (i >= CS_SOUNDS && i < CS_SOUNDS + MAX_SOUNDS) {
		if (cls.state == CL_ACTIVE)
			cl.sound_precache[i - CS_SOUNDS] = S_LoadSample(s);
	} else if (i >= CS_IMAGES && i < CS_IMAGES + MAX_IMAGES) {
		if (cls.state == CL_ACTIVE)
			cl.image_precache[i - CS_IMAGES] = R_LoadPic(s);
	} else if (i >= CS_CLIENT_INFO && i < CS_CLIENT_INFO + MAX_CLIENTS) {
		if (cls.state == CL_ACTIVE)
			Cl_LoadClient(&cl.client_info[i - CS_CLIENT_INFO], s);
	}
}

/*
 * Cl_ParseSound
 */
static void Cl_ParseSound(void) {
	vec3_t origin;
	float *org;
	int soundindex;
	int ent_num;
	int atten;
	int flags;

	flags = Msg_ReadByte(&net_message);

	soundindex = Msg_ReadByte(&net_message);

	if (flags & S_ATTEN)
		atten = Msg_ReadByte(&net_message);
	else
		atten = DEFAULT_SOUND_ATTENUATION;

	if (flags & S_ENTNUM) { // entity relative
		ent_num = Msg_ReadShort(&net_message);

		if (ent_num > MAX_EDICTS)
			Com_Error(ERR_DROP, "Cl_ParseSound: ent_num = %d.\n", ent_num);
	} else {
		ent_num = -1;
	}

	if (flags & S_ORIGIN) { // positioned in space
		Msg_ReadPos(&net_message, origin);

		org = origin;
	} else
		// use ent_num
		org = NULL;

	if (!cl.sound_precache[soundindex])
		return;

	S_PlaySample(org, ent_num, cl.sound_precache[soundindex], atten);
}

/*
 * Cl_IgnoreChatMessage
 */
static boolean_t Cl_IgnoreChatMessage(const char *msg) {

	const char *s = strtok(cl_ignore->string, " ");

	if (*cl_ignore->string == '\0')
		return false; // nothing currently filtered

	while (s) {
		if (strstr(msg, s))
			return true;
		s = strtok(NULL, " ");
	}
	return false; // msg is okay
}

/*
 * Cl_ShowNet
 */
static void Cl_ShowNet(const char *s) {
	if (cl_show_net_messages->integer >= 2)
		Com_Print("%3zd: %s\n", net_message.read - 1, s);
}

/*
 * Cl_ParseServerMessage
 */
void Cl_ParseServerMessage(void) {
	extern int bytes_this_second;
	int cmd, old_cmd;
	char *s;
	int i;

	if (cl_show_net_messages->integer == 1)
		Com_Print(Q2W_SIZE_T" ", net_message.size);
	else if (cl_show_net_messages->integer >= 2)
		Com_Print("------------------\n");

	bytes_this_second += net_message.size;
	cmd = 0;

	// parse the message
	while (true) {
		if (net_message.read > net_message.size) {
			Com_Error(ERR_DROP, "Cl_ParseServerMessage: Bad server message.\n");
		}

		old_cmd = cmd;
		cmd = Msg_ReadByte(&net_message);

		if (cmd == -1) {
			Cl_ShowNet("END OF MESSAGE");
			break;
		}

		if (cl_show_net_messages->integer >= 2 && svc_strings[cmd])
			Cl_ShowNet(svc_strings[cmd]);

		switch (cmd) {

		case SV_CMD_DISCONNECT:
			Com_Error(ERR_DROP, "Server disconnected.\n");
			break;

		case SV_CMD_RECONNECT:
			Com_Print("Server disconnected, reconnecting...\n");
			// stop download
			if (cls.download.file) {
				if (cls.download.http) // clean up http downloads
					Cl_HttpDownloadCleanup();
				else
					// or just stop legacy ones
					Fs_CloseFile(cls.download.file);
				cls.download.file = NULL;
			}
			cls.state = CL_CONNECTING;
			cls.connect_time = -99999; // fire immediately
			break;

		case SV_CMD_PRINT:
			i = Msg_ReadByte(&net_message);
			s = Msg_ReadString(&net_message);
			if (i == PRINT_CHAT) {
				if (Cl_IgnoreChatMessage(s)) // filter /ignore'd chatters
					break;
				if (*cl_chat_sound->string) // trigger chat sound
					S_StartLocalSample(cl_chat_sound->string);
			} else if (i == PRINT_TEAMCHAT) {
				if (Cl_IgnoreChatMessage(s)) // filter /ignore'd chatters
					break;
				if (*cl_team_chat_sound->string) // trigger chat sound
					S_StartLocalSample(cl_team_chat_sound->string);
			}
			Com_Print("%s", s);
			break;

		case SV_CMD_CENTER_PRINT:
			Cl_CenterPrint(Msg_ReadString(&net_message));
			break;

		case SV_CMD_CBUF_TEXT:
			s = Msg_ReadString(&net_message);
			Cbuf_AddText(s);
			break;

		case SV_CMD_SERVER_DATA:
			Cl_ParseServerData();
			break;

		case SV_CMD_CONFIG_STRING:
			Cl_ParseConfigString();
			break;

		case SV_CMD_SOUND:
			Cl_ParseSound();
			break;

		case SV_CMD_ENTITY_BASELINE:
			Cl_ParseBaseline();
			break;

		case SV_CMD_TEMP_ENTITY:
			Cl_ParseTempEntity();
			break;

		case SV_CMD_MUZZLE_FLASH:
			Cl_ParseMuzzleFlash();
			break;

		case SV_CMD_DOWNLOAD:
			Cl_ParseDownload();
			break;

		case SV_CMD_FRAME:
			Cl_ParseFrame();
			break;

		default:
			// delegate to the client game module before failing
			if (!cls.cgame->ParseMessage(cmd)) {
				Com_Error(ERR_DROP,
						"Cl_ParseServerMessage: Illegible server message:\n"
							"  %d: last command was %s\n", cmd,
						svc_strings[old_cmd]);
			}
			break;
		}
	}

	Cl_AddNetgraph();

	Cl_WriteDemoMessage();
}
