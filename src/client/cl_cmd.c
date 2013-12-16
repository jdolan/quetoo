/*
 * Copyright(c) 1997-2001 id Software, Inc.
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
 * @brief Accumulates all movement for the current packet frame in a command. This
 * may be called multiple times per packet frame.
 */
void Cl_UpdateCmd(void) {
	static uint32_t last_move;
	pm_cmd_t c;

	if (cls.state != CL_ACTIVE)
		return;

	memset(&c, 0, sizeof(c));

	// determine the interval for just this move
	c.msec = Clamp((cls.real_time - last_move) * time_scale->value, 1, 255);

	// get movement from input devices
	Cl_Move(&c);
	last_move = cls.real_time;

	// now update the pending command
	cl_cmd_t *cmd = &cl.cmds[cls.net_chan.outgoing_sequence & CMD_MASK];

	cmd->cmd.msec += c.msec;

	cmd->cmd.buttons |= c.buttons;

	VectorCopy(c.angles, cmd->cmd.angles);

	cmd->cmd.forward += c.forward;
	cmd->cmd.right += c.right;
	cmd->cmd.up += c.up;

	// store timestamps for netgraph and prediction calculations
	cmd->time = cl.time;
	cmd->real_time = cls.real_time;
}

/*
 * @brief Initializes the next outgoing command so that it may accumulate movement
 * over the next packet frame.
 */
static void Cl_InitCmd(void) {
	cl_cmd_t *cmd = &cl.cmds[cls.net_chan.outgoing_sequence & CMD_MASK];

	memset(cmd, 0, sizeof(cl_cmd_t));
}

/*
 * @brief Calculate the true command duration and clamp it so that it may be sent.
 */
static void Cl_FinalizeCmd(void) {
	cl_cmd_t *cmd = &cl.cmds[cls.net_chan.outgoing_sequence & CMD_MASK];

	cmd->cmd.msec = Clamp(cls.packet_delta, 1, 255);
	//Com_Debug("%3dms: %4d forward %4d right %4d up\n", cmd->cmd.msec, cmd->cmd.forward, cmd->cmd.right, cmd->cmd.up);
}

/*
 * @brief Pumps the command cycle, sending the most recently gathered movement
 * to the server.
 */
void Cl_SendCmd(void) {
	mem_buf_t buf;
	byte data[128];

	if (cls.state <= CL_CONNECTING)
		return;

	if (cls.state == CL_CONNECTED) {
		// send any reliable messages and / or don't timeout
		if (cls.net_chan.message.size || cls.real_time - cls.net_chan.last_sent > 1000)
			Netchan_Transmit(&cls.net_chan, NULL, 0);
		return;
	}

	// send a user info update if needed
	if (cvar_user_info_modified) {
		Net_WriteByte(&cls.net_chan.message, CL_CMD_USER_INFO);
		Net_WriteString(&cls.net_chan.message, Cvar_UserInfo());

		cvar_user_info_modified = false;
	}

	// finalize the current command
	Cl_FinalizeCmd();

	// and write it out
	Mem_InitBuffer(&buf, data, sizeof(data));

	Net_WriteByte(&buf, CL_CMD_MOVE);

	// let the server know what the last frame we got was, so the next
	// message can be delta compressed
	if (!cl.frame.valid || (cls.demo_file && Fs_Tell(cls.demo_file) == 0))
		Net_WriteLong(&buf, -1); // no compression
	else
		Net_WriteLong(&buf, cl.frame.frame_num);

	// send this and the previous two cmds in the message, so
	// if the last packet was dropped, it can be recovered
	static pm_cmd_t null_cmd;

	cl_cmd_t *cmd = &cl.cmds[(cls.net_chan.outgoing_sequence - 2) & CMD_MASK];
	Net_WriteDeltaMoveCmd(&buf, &null_cmd, &cmd->cmd);

	pm_cmd_t *old_cmd = &cmd->cmd;
	cmd = &cl.cmds[(cls.net_chan.outgoing_sequence - 1) & CMD_MASK];
	Net_WriteDeltaMoveCmd(&buf, old_cmd, &cmd->cmd);

	old_cmd = &cmd->cmd;
	cmd = &cl.cmds[(cls.net_chan.outgoing_sequence) & CMD_MASK];
	Net_WriteDeltaMoveCmd(&buf, old_cmd, &cmd->cmd);

	// deliver the message
	Netchan_Transmit(&cls.net_chan, buf.data, buf.size);

	cl.packet_counter++;

	// initialize the next command
	Cl_InitCmd();
}

