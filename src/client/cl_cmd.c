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
 * Foundation, Inc., 59 Temple Place - Suite 330, Boston, MA 02111-1307, USA.
 */

#include "cl_local.h"

/**
 * @brief Initializes the current movement command for the outgoing sequence slot.
 */
static void Cl_InitMovementCommand(void) {

  ClientCmd *cmd = &cl.cmds[cls.netChan.outgoingSequence & CMD_MASK];

  memset(cmd, 0, sizeof(*cmd));
}

/**
 * @brief Gather movement that is eligible for client-side prediction (i.e. view offset, angles).
 */
void Cl_UpdateMovementCommand(uint32_t msec) {

  ClientCmd *cmd = &cl.cmds[cls.netChan.outgoingSequence & CMD_MASK];

  cmd->cmd.msec = Minf(msec, 255u);

  Cl_Look(&cmd->cmd);

  cmd->time = cl.time;
  cmd->timestamp = cl.unclampedTime;
}

/**
 * @brief Gather remaining movement (i.e. forward, back, etc.) so that it may be sent off.
 */
static void Cl_FinalizeMovementCommand(void) {

  ClientCmd *cmd = &cl.cmds[cls.netChan.outgoingSequence & CMD_MASK];

  ClientCmd *prev = &cl.cmds[(cls.netChan.outgoingSequence - 1) & CMD_MASK];

  const uint32_t msec = cl.unclampedTime - prev->timestamp;

  cmd->cmd.msec = Minf(msec, 255u);

  Cl_Move(&cmd->cmd);

  cmd->time = cl.time;
  cmd->timestamp = cl.unclampedTime;
}

/**
 * @brief Writes the most recent movement command(s) using delta-compression if available.
 */
static void Cl_WriteMovementCommand(MemBuf *buf) {
  static ClientCmd null_cmd;

  Net_WriteByte(buf, CL_CMD_MOVE);

  if (!cl.frame.valid) {
    Net_WriteLong(buf, -1);
  } else {
    Net_WriteLong(buf, cl.frame.frameNum);
  }

  ClientCmd *from = &null_cmd, *to = &cl.cmds[(cls.netChan.outgoingSequence - 2) & CMD_MASK];
  Net_WriteDeltaMoveCmd(buf, &from->cmd, &to->cmd);

  from = to; to = &cl.cmds[(cls.netChan.outgoingSequence - 1) & CMD_MASK];
  Net_WriteDeltaMoveCmd(buf, &from->cmd, &to->cmd);

  from = to;  to = &cl.cmds[(cls.netChan.outgoingSequence) & CMD_MASK];
  Net_WriteDeltaMoveCmd(buf, &from->cmd, &to->cmd);
}

/**
 * @brief Sends the user info string to the server over the reliable channel.
 */
static void Cl_WriteUserInfoCommand(void) {

  if (cvarUserInfoModified) {
    cvarUserInfoModified = false;

    Net_WriteByte(&cls.netChan.message, CL_CMD_USER_INFO);
    Net_WriteString(&cls.netChan.message, Cvar_UserInfo());
  }
}

/**
 * @brief Sends the entity info string tot he server over the reliable channel.
 */
void Cl_WriteEntityInfoCommand(int16_t number, const CmEntity *entity) {

  Net_WriteByte(&cls.netChan.message, CL_CMD_ENTITY_INFO);
  Net_WriteShort(&cls.netChan.message, number);

  char *info = Cm_EntityToInfoString(entity);

  Com_Debug(DEBUG_EDITOR, "%d: %s\n", number, info);
  Net_WriteString(&cls.netChan.message, info);

  Mem_Free(info);
}

/**
 * @brief Writes one pending voice frame, if any, to the outgoing packet.
 * @details The channel is carried opaquely: the server asks the game who may hear it, exactly as
 * the game decides who receives a say or a sayTeam.
 */
static void Cl_WriteVoiceCommand(MemBuf *buf) {

  byte voice[VOICE_MAX_PAYLOAD];
  uint8_t seq, flags, channel;

  const int32_t len = S_ReadVoice(voice, &seq, &flags, &channel);

  if (len <= 0) {
    return;
  }

  Net_WriteByte(buf, CL_CMD_VOICE);
  Net_WriteByte(buf, channel);
  Net_WriteByte(buf, seq);
  Net_WriteByte(buf, flags);
  Net_WriteByte(buf, len);
  Net_WriteData(buf, voice, len);

  // light our own indicator, so holding the key is visible without anyone to hear it
  cl.voiceTime[cl.frame.ps.client] = cl.unclampedTime;
}

/**
 * @brief Pumps the command cycle, sending the most recently gathered movement to the server.
 * @details Commands must meet a certain duration, in milliseconds, in order to be sent. This
 * prevents saturating the network channel with very small movement commands, which are also
 * problematic for the physics and prediction code. This holds regardless of vsync (#866).
 */
void Cl_SendCommands(void) {

  const uint32_t delta = quetoo.ticks - cls.netChan.lastSent;

  if (delta < 4) {
    return;
  }

  switch (cls.state) {
    case CL_CONNECTED:
    case CL_LOADING:

      if (cls.netChan.message.size || delta > 1000) {
        Netchan_Transmit(&cls.netChan, NULL, 0);
        cl.packets++;
      }

      break;

    case CL_ACTIVE:

      Cl_WriteUserInfoCommand();

      Cl_FinalizeMovementCommand();

      MemBuf buf;
      byte data[sizeof(ClientCmd) * 3 + VOICE_MAX_PAYLOAD + 16];

      Mem_InitBuffer(&buf, data, sizeof(data));

      Cl_WriteMovementCommand(&buf);

      Cl_WriteVoiceCommand(&buf);

      Netchan_Transmit(&cls.netChan, buf.data, buf.size);
      cl.packets++;

      Cl_InitMovementCommand();

      break;

    default:
      break;
  }
}
