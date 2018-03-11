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

/**
 * @brief Writes server_data, config_strings, and baselines once a non-delta
 * compressed frame arrives from the server.
 */
static void Cl_WriteDemoHeader(void) {
	static entity_state_t null_state;
	mem_buf_t msg;
	byte buffer[MAX_MSG_SIZE];

	// write out messages to hold the startup information
	Mem_InitBuffer(&msg, buffer, sizeof(buffer));

	// write the server data
	Net_WriteByte(&msg, SV_CMD_SERVER_DATA);
	Net_WriteShort(&msg, PROTOCOL_MAJOR);
	Net_WriteShort(&msg, cls.cgame->protocol);
	Net_WriteByte(&msg, 1); // demo_server byte
	Net_WriteString(&msg, Cvar_GetString("game"));
	Net_WriteShort(&msg, cl.client_num);
	Net_WriteString(&msg, cl.config_strings[CS_NAME]);

	// and config_strings
	for (int32_t i = 0; i < MAX_CONFIG_STRINGS; i++) {
		if (*cl.config_strings[i] != '\0') {
			if (msg.size + strlen(cl.config_strings[i]) + 32 > msg.max_size) { // write it out
				const int32_t len = LittleLong((int32_t) msg.size);
				Fs_Write(cls.demo_file, &len, sizeof(len), 1);
				Fs_Write(cls.demo_file, msg.data, msg.size, 1);
				msg.size = 0;
			}

			Net_WriteByte(&msg, SV_CMD_CONFIG_STRING);
			Net_WriteShort(&msg, i);
			Net_WriteString(&msg, cl.config_strings[i]);
		}
	}

	// and baselines
	for (size_t i = 0; i < lengthof(cl.entities); i++) {
		entity_state_t *ent = &cl.entities[i].baseline;
		if (!ent->number) {
			continue;
		}

		if (msg.size + 64 > msg.max_size) { // write it out
			const int32_t len = LittleLong((int32_t) msg.size);
			Fs_Write(cls.demo_file, &len, sizeof(len), 1);
			Fs_Write(cls.demo_file, msg.data, msg.size, 1);
			msg.size = 0;
		}

		Net_WriteByte(&msg, SV_CMD_BASELINE);
		Net_WriteDeltaEntity(&msg, &null_state, &cl.entities[i].baseline, true);
	}

	Net_WriteByte(&msg, SV_CMD_CBUF_TEXT);
	Net_WriteString(&msg, "precache 0\n");

	// write it to the demo file

	const int32_t len = LittleLong((int32_t) msg.size);

	Fs_Write(cls.demo_file, &len, sizeof(len), 1);
	Fs_Write(cls.demo_file, msg.data, msg.size, 1);

	Com_Debug(DEBUG_CLIENT, "Demo started\n");
	// the rest of the demo file will be individual frames
}

/**
 * @brief Dumps the current net message, prefixed by the length.
 */
void Cl_WriteDemoMessage(void) {

	if (!cls.demo_file) {
		return;
	}

	if (!Fs_Tell(cls.demo_file)) {
		if (cl.frame.delta_frame_num < 0) {
			Com_Debug(DEBUG_CLIENT, "Received uncompressed frame, writing demo header..\n");
			Cl_WriteDemoHeader();
		} else {
			return; // wait for an uncompressed packet
		}
	}

	// the first eight bytes are just packet sequencing stuff
	const int32_t len = LittleLong((int32_t) (net_message.size - 8));

	Fs_Write(cls.demo_file, &len, sizeof(len), 1);
	Fs_Write(cls.demo_file, net_message.data + 8, len, 1);
}

/**
 * @brief Stop recording a demo
 */
void Cl_Stop_f(void) {
	int32_t len = -1;

	if (!cls.demo_file) {
		Com_Print("Not recording a demo\n");
		return;
	}

	// finish up
	Fs_Write(cls.demo_file, &len, sizeof(len), 1);
	Fs_Close(cls.demo_file);

	cls.demo_file = NULL;
	Com_Print("Stopped demo\n");
}

/**
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
		Com_Print("Already recording\n");
		return;
	}

	if (cls.state != CL_ACTIVE) {
		Com_Print("You must be in a level to record\n");
		return;
	}

	g_snprintf(cls.demo_filename, sizeof(cls.demo_filename), "demos/%s.demo", Cmd_Argv(1));

	// open the demo file
	if (!(cls.demo_file = Fs_OpenWrite(cls.demo_filename))) {
		Com_Warn("Couldn't open %s\n", cls.demo_filename);
		return;
	}

	Com_Print("Recording to %s\n", cls.demo_filename);
}

#define DEMO_PLAYBACK_STEP 1

/**
 * @brief Adjusts time scale by delta, clamping to reasonable limits.
 */
static void Cl_AdjustDemoPlayback(vec_t delta) {

	if (!cl.demo_server) {
		return;
	}

	Cvar_ForceSetValue(time_scale->name, Clamp(time_scale->value + delta, DEMO_PLAYBACK_STEP, 4.0));

	Com_Print("Demo playback rate %d%%\n", (int32_t) (time_scale->value * 100));
}

/**
 * @brief
 */
void Cl_FastForward_f(void) {
	Cl_AdjustDemoPlayback(DEMO_PLAYBACK_STEP);
}

/**
 * @brief
 */
void Cl_SlowMotion_f(void) {
	Cl_AdjustDemoPlayback(-DEMO_PLAYBACK_STEP);
}
