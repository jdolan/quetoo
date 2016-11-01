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
 * @brief Accumulates all movement for the current packet frame in a command. This
 * may be called multiple times per packet frame.
 */
static void Cl_UpdateCommand(void) {

	cl_cmd_t *cmd = &cl.cmds[cls.net_chan.outgoing_sequence & CMD_MASK];

	memset(cmd, 0, sizeof(*cmd));

	cmd->cmd.msec = Clamp(cls.packet_delta, QUETOO_TICK_MILLIS, 255);

	Cl_Move(&cmd->cmd);

	//Com_Debug("%3dms: %4d forward %4d right %4d up\n", cmd->cmd.msec, cmd->cmd.forward, cmd->cmd.right, cmd->cmd.up);

	// store timestamp for netgraph and prediction calculations
	cmd->time = cl.time;
	cmd->timestamp = cl.systime;
}

/**
 * @brief Pumps the command cycle, sending the most recently gathered movement
 * to the server.
 */
void Cl_SendCommand(void) {
	mem_buf_t buf;
	byte data[sizeof(cl_cmd_t) * 3];

	if (cls.state <= CL_CONNECTING)
		return;

	if (cls.state == CL_CONNECTED) {
		// send any reliable messages and / or don't timeout
		if (cls.net_chan.message.size || quetoo.time - cls.net_chan.last_sent > 1000)
			Netchan_Transmit(&cls.net_chan, NULL, 0);
		return;
	}

	// gather the current command
	Cl_UpdateCommand();

	// and write it out
	Mem_InitBuffer(&buf, data, sizeof(data));

	Net_WriteByte(&buf, CL_CMD_MOVE);

	// let the server know the last frame we received, so the next message can be delta compressed
	if (!cl.frame.valid || (cls.demo_file && Fs_Tell(cls.demo_file) == 0))
		Net_WriteLong(&buf, -1); // no compression
	else
		Net_WriteLong(&buf, cl.frame.frame_num);

	// send this and the previous two cmds, so if the last packet was dropped, it can be recovered
	static pm_cmd_t null_cmd;

	cl_cmd_t *cmd = &cl.cmds[(cls.net_chan.outgoing_sequence - 2) & CMD_MASK];
	Net_WriteDeltaMoveCmd(&buf, &null_cmd, &cmd->cmd);

	pm_cmd_t *old_cmd = &cmd->cmd;
	cmd = &cl.cmds[(cls.net_chan.outgoing_sequence - 1) & CMD_MASK];
	Net_WriteDeltaMoveCmd(&buf, old_cmd, &cmd->cmd);

	old_cmd = &cmd->cmd;
	cmd = &cl.cmds[(cls.net_chan.outgoing_sequence) & CMD_MASK];
	Net_WriteDeltaMoveCmd(&buf, old_cmd, &cmd->cmd);

	// send a user info update if needed
	if (cvar_user_info_modified) {
		Net_WriteByte(&cls.net_chan.message, CL_CMD_USER_INFO);
		Net_WriteString(&cls.net_chan.message, Cvar_UserInfo());

		cvar_user_info_modified = false;
	}

	// deliver the message
	Netchan_Transmit(&cls.net_chan, buf.data, buf.size);

	cl.packet_counter++;
}

