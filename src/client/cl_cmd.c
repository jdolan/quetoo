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
 * @brief
 */
static void Cl_InitMovementCommand(void) {

	cl_cmd_t *cmd = &cl.cmds[cls.net_chan.outgoing_sequence & CMD_MASK];

	memset(cmd, 0, sizeof(*cmd));
}

/**
 * @brief Gather movement that is eligible for client-side prediction (i.e. view offset, angles).
 */
void Cl_UpdateMovementCommand(uint32_t msec) {

	cl_cmd_t *cmd = &cl.cmds[cls.net_chan.outgoing_sequence & CMD_MASK];

	cmd->cmd.msec = Min(msec, 255u);

	Cl_Look(&cmd->cmd);

	cmd->time = cl.time;
	cmd->timestamp = cl.unclamped_time;
}

/**
 * @brief Gather remaining movement (i.e. forward, back, etc.) so that it may be sent off.
 */
static void Cl_FinalizeMovementCommand(void) {

	cl_cmd_t *cmd = &cl.cmds[cls.net_chan.outgoing_sequence & CMD_MASK];

	cl_cmd_t *prev = &cl.cmds[(cls.net_chan.outgoing_sequence - 1) & CMD_MASK];

	const uint32_t msec = cl.unclamped_time - prev->timestamp;

	cmd->cmd.msec = Min(msec, 255u);

	Cl_Move(&cmd->cmd);

	cmd->time = cl.time;
	cmd->timestamp = cl.unclamped_time;
}

/**
 * @brief Writes the most recent movement command(s) using delta-compression if available.
 */
static void Cl_WriteMovementCommand(mem_buf_t *buf) {
	static cl_cmd_t null_cmd;

	Net_WriteByte(buf, CL_CMD_MOVE);

	if (!cl.frame.valid || (cls.demo_file && Fs_Tell(cls.demo_file) == 0)) {
		Net_WriteLong(buf, -1);
	} else {
		Net_WriteLong(buf, cl.frame.frame_num);
	}

	cl_cmd_t *from = &null_cmd, *to = &cl.cmds[(cls.net_chan.outgoing_sequence - 2) & CMD_MASK];
	Net_WriteDeltaMoveCmd(buf, &from->cmd, &to->cmd);

	from = to; to = &cl.cmds[(cls.net_chan.outgoing_sequence - 1) & CMD_MASK];
	Net_WriteDeltaMoveCmd(buf, &from->cmd, &to->cmd);

	from = to;  to = &cl.cmds[(cls.net_chan.outgoing_sequence) & CMD_MASK];
	Net_WriteDeltaMoveCmd(buf, &from->cmd, &to->cmd);
}

/**
 * @brief Sends the user info string to the server over the reliable channel.
 */
static void Cl_WriteUserInfoCommand(void) {

	if (cvar_user_info_modified) {
		cvar_user_info_modified = false;

		Net_WriteByte(&cls.net_chan.message, CL_CMD_USER_INFO);
		Net_WriteString(&cls.net_chan.message, Cvar_UserInfo());
	}
}

/**
 * @brief Pumps the command cycle, sending the most recently gathered movement to the server.
 * @details Commands must meet a certain duration, in milliseconds, in order to be sent. This
 * prevents saturating the network channel with very small movement commands, which are also
 * problematic for the physics and prediction code.
 */
void Cl_SendCommands(void) {

	const uint32_t delta = quetoo.ticks - cls.net_chan.last_sent;

	if (delta < 8 && !r_swap_interval->value) {
		return;
	}

	switch (cls.state) {
		case CL_CONNECTED:
		case CL_LOADING:

			if (cls.net_chan.message.size || delta > 1000) {
				Netchan_Transmit(&cls.net_chan, NULL, 0);
				cl.packet_counter++;
			}

			break;

		case CL_ACTIVE:

			Cl_WriteUserInfoCommand();

			Cl_FinalizeMovementCommand();

			mem_buf_t buf;
			byte data[sizeof(cl_cmd_t) * 3];

			Mem_InitBuffer(&buf, data, sizeof(data));

			Cl_WriteMovementCommand(&buf);

			Netchan_Transmit(&cls.net_chan, buf.data, buf.size);
			cl.packet_counter++;

			Cl_InitMovementCommand();

			break;

		default:
			break;
	}
}
