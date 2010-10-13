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

#include "client.h"


/*
 * Cl_UpdateCmd
 *
 * Accumulate all movement for the current packet frame in a command.
 */
void Cl_UpdateCmd(void){
	usercmd_t *cmd;

	if(cls.state != ca_active)
		return;

	cmd = &cl.cmds[cls.netchan.outgoing_sequence & CMD_MASK];
	cmd->msec = cls.packet_delta;

	// get movement from input devices
	Cl_Move(cmd);
}


/*
 * Cl_InitCmd
 *
 * Initializes the next outgoing command so that it may accumulate movement
 * over the next packet frame.
 */
static void Cl_InitCmd(void){
	usercmd_t *cmd;

	// the outgoing sequence has just been incremented, mask it off and
	// clear the command which will now accumulate movement
	cmd = &cl.cmds[cls.netchan.outgoing_sequence & CMD_MASK];
	memset(cmd, 0, sizeof(usercmd_t));
}


/*
 * Cl_FinalizeCmd
 *
 * Calculate the true command duration and clamp it so that it may be sent.
 */
static void Cl_FinalizeCmd(void){
	usercmd_t *cmd;

	// resolve the cumulative command duration
	cmd = &cl.cmds[cls.netchan.outgoing_sequence & CMD_MASK];
	cmd->msec = cls.packet_delta;

	if(cmd->msec > 250)  // clamp it to server max
		cmd->msec = 250;
}


/*
 * Cl_SendCmd
 *
 * Pumps the command cycle, sending the most recently gathered movement
 * to the server.
 */
void Cl_SendCmd(void){
	extern int packets_this_second;
	sizebuf_t buf;
	byte data[128];
	usercmd_t *cmd, *oldcmd;
	usercmd_t nullcmd;

	if(cls.state <= ca_connecting)
		return;

	Sb_Init(&buf, data, sizeof(data));

	if(cls.state == ca_connected){
		// send anything we have pending, or just don't timeout
		if(cls.netchan.message.cursize || cls.realtime - cls.netchan.last_sent > 1000)
			Netchan_Transmit(&cls.netchan, 0, buf.data);
		return;
	}

	// send a userinfo update if needed
	if(userinfo_modified){
		Msg_WriteByte(&cls.netchan.message, clc_userinfo);
		Msg_WriteString(&cls.netchan.message, Cvar_Userinfo());

		userinfo_modified = false;
	}

	// finalize the current command
	Cl_FinalizeCmd();

	// write it out
	Msg_WriteByte(&buf, clc_move);

	// let the server know what the last frame we got was, so the next
	// message can be delta compressed
	if(!cl.frame.valid || cls.demowaiting)
		Msg_WriteLong(&buf, -1);  // no compression
	else
		Msg_WriteLong(&buf, cl.frame.serverframe);

	// send this and the previous two cmds in the message, so
	// if the last packet was dropped, it can be recovered
	memset(&nullcmd, 0, sizeof(nullcmd));

	cmd = &cl.cmds[(cls.netchan.outgoing_sequence - 2) & CMD_MASK];
	Msg_WriteDeltaUsercmd(&buf, &nullcmd, cmd);

	oldcmd = cmd;
	cmd = &cl.cmds[(cls.netchan.outgoing_sequence - 1) & CMD_MASK];
	Msg_WriteDeltaUsercmd(&buf, oldcmd, cmd);

	oldcmd = cmd;
	cmd = &cl.cmds[(cls.netchan.outgoing_sequence) & CMD_MASK];
	Msg_WriteDeltaUsercmd(&buf, oldcmd, cmd);

	// record a timestamp for netgraph calculations
	cl.cmd_time[(cls.netchan.outgoing_sequence) & CMD_MASK] = cls.realtime;

	// deliver the message
	Netchan_Transmit(&cls.netchan, buf.cursize, buf.data);

	packets_this_second++;

	// initialize the next command
	Cl_InitCmd();
}

