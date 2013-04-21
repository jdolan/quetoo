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

/*
 * @brief Writes server_data, config_strings, and baselines once a non-delta
 * compressed frame arrives from the server.
 */
static void Cl_WriteDemoHeader(void) {
	byte buffer[MAX_MSG_SIZE];
	size_buf_t msg;
	int32_t i;
	int32_t len;
	entity_state_t null_state;

	// write out messages to hold the startup information
	Sb_Init(&msg, buffer, sizeof(buffer));

	// write the server data
	Msg_WriteByte(&msg, SV_CMD_SERVER_DATA);
	Msg_WriteLong(&msg, PROTOCOL);
	Msg_WriteLong(&msg, cl.server_count);
	Msg_WriteLong(&msg, cl.server_hz);
	Msg_WriteByte(&msg, 1); // demo_server byte
	Msg_WriteString(&msg, Cvar_GetString("game"));
	Msg_WriteShort(&msg, cl.player_num);
	Msg_WriteString(&msg, cl.config_strings[CS_NAME]);

	// and config_strings
	for (i = 0; i < MAX_CONFIG_STRINGS; i++) {
		if (*cl.config_strings[i] != '\0') {
			if (msg.size + strlen(cl.config_strings[i]) + 32 > msg.max_size) { // write it out
				len = LittleLong(msg.size);
				Fs_Write(cls.demo_file, &len, sizeof(len), 1);
				Fs_Write(cls.demo_file, msg.data, msg.size, 1);
				msg.size = 0;
			}

			Msg_WriteByte(&msg, SV_CMD_CONFIG_STRING);
			Msg_WriteShort(&msg, i);
			Msg_WriteString(&msg, cl.config_strings[i]);
		}
	}

	// and baselines
	for (i = 0; i < MAX_EDICTS; i++) {
		entity_state_t *ent = &cl.entities[i].baseline;
		if (!ent->number)
			continue;

		if (msg.size + 64 > msg.max_size) { // write it out
			len = LittleLong(msg.size);
			Fs_Write(cls.demo_file, &len, sizeof(len), 1);
			Fs_Write(cls.demo_file, msg.data, msg.size, 1);
			msg.size = 0;
		}

		memset(&null_state, 0, sizeof(null_state));

		Msg_WriteByte(&msg, SV_CMD_BASELINE);
		Msg_WriteDeltaEntity(&null_state, &cl.entities[i].baseline, &msg, true, true);
	}

	Msg_WriteByte(&msg, SV_CMD_CBUF_TEXT);
	Msg_WriteString(&msg, "precache 0\n");

	// write it to the demo file

	len = LittleLong(msg.size);
	Fs_Write(cls.demo_file, &len, sizeof(len), 1);
	Fs_Write(cls.demo_file, msg.data, msg.size, 1);

	Com_Debug("Cl_WriteDemoHeader: demo started\n");
	// the rest of the demo file will be individual frames
}

/*
 * @brief Dumps the current net message, prefixed by the length.
 */
void Cl_WriteDemoMessage(void) {
	int32_t len;

	if (!cls.demo_file)
		return;

	// if we received an uncompressed frame, write the demo header
	if (cl.frame.delta_frame < 1 && !Fs_Tell(cls.demo_file)) {
		Cl_WriteDemoHeader();
	}

	// the first eight bytes are just packet sequencing stuff
	len = LittleLong(net_message.size - 8);
	Fs_Write(cls.demo_file, &len, sizeof(len), 1);
	Fs_Write(cls.demo_file, net_message.data + 8, len, 1);
}

/*
 * @brief Stop recording a demo
 */
void Cl_Stop_f(void) {
	int32_t len = -1;

	if (!cls.demo_file) {
		Com_Print("Not recording a demo.\n");
		return;
	}

	// finish up
	Fs_Write(cls.demo_file, &len, sizeof(len), 1);
	Fs_Close(cls.demo_file);

	cls.demo_file = NULL;
	Com_Print("Stopped demo.\n");
}

/*
 * @brief record <demo name>
 *
 * Begin recording a demo from the current frame until `stop` is issued.
 */
void Cl_Record_f(void) {

	if (Cmd_Argc() != 2) {
		Com_Print("Usage: %s <demo name>\n", Cmd_Argv(0));
		return;
	}

	if (cls.demo_file) {
		Com_Print("Already recording.\n");
		return;
	}

	if (cls.state != CL_ACTIVE) {
		Com_Print("You must be in a level to record.\n");
		return;
	}

	g_snprintf(cls.demo_filename, sizeof(cls.demo_filename), "demos/%s.dem", Cmd_Argv(1));

	// open the demo file
	if (!(cls.demo_file = Fs_OpenWrite(cls.demo_filename))) {
		Com_Warn("Cl_Record_f: couldn't open %s.\n", cls.demo_filename);
		return;
	}

	Com_Print("Recording to %s\n", cls.demo_filename);
}

#define DEMO_PLAYBACK_STEP 1

/*
 * @brief Adjusts time scale by delta, clamping to reasonable limits.
 */
static void Cl_AdjustDemoPlayback(float delta) {
	float f;

	if (!cl.demo_server) {
		return;
	}

	f = Clamp(time_scale->value + delta, DEMO_PLAYBACK_STEP, 4.0);
	Cvar_Set("time_scale", va("%f", f));

	Com_Print("Demo playback rate %d%%\n", (int) (time_scale->value * 100));
}

/*
 * @brief
 */
void Cl_FastForward_f(void) {
	Cl_AdjustDemoPlayback(DEMO_PLAYBACK_STEP);
}

/*
 * @brief
 */
void Cl_SlowMotion_f(void) {
	Cl_AdjustDemoPlayback(-DEMO_PLAYBACK_STEP);
}
