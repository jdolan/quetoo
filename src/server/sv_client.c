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

  Com_Debug(DEBUG_SERVER, "%s\n", Sv_NetaddrToString(svClient));

  if (svClient->state != SV_CLIENT_CONNECTED) {
    Com_Warn("%s issued new from %d\n", Sv_NetaddrToString(svClient), svClient->state);
    Sv_DropClient(svClient);
    return;
  }

  // demo servers have no per-map baselines to send: Sv_SendDemoSetup replays the demo's own
  // recorded server data, config strings and baselines instead, whatever point the shared
  // playback cursor has already reached. Duration and pause state aren't part of that recording,
  // so Sv_SendDemoInfo covers those separately, here and on every later change
  if (svs.state == SV_ACTIVE_DEMO) {
    Sv_SendDemoInfo();
    Sv_SendDemoSetup(svClient);
    return;
  }

  // send the server data
  Net_WriteByte(&svClient->netChan.message, SV_CMD_SERVER_DATA);
  Net_WriteLong(&svClient->netChan.message, PROTOCOL_MAJOR);
  Net_WriteLong(&svClient->netChan.message, svs.game->protocol);
  Net_WriteByte(&svClient->netChan.message, 0);
  Net_WriteString(&svClient->netChan.message, Com_Game());
  Net_WriteString(&svClient->netChan.message, svs.game->cgame ? : Com_Game());

  // send level title
  Net_WriteString(&svClient->netChan.message, sv.configStrings[CS_MESSAGE]);

  // begin fetching configStrings
  Net_WriteByte(&svClient->netChan.message, SV_CMD_CBUF_TEXT);
  Net_WriteString(&svClient->netChan.message, va("config_strings %i 0\n", svs.spawnCount));
}

/**
 * @brief Sends config strings to a connecting client in batches.
 */
static void Sv_ConfigStrings_f(void) {
  uint32_t start;

  Com_Debug(DEBUG_SERVER, "%s\n", Sv_NetaddrToString(svClient));

  if (svClient->state != SV_CLIENT_CONNECTED) {
    Com_Warn("%s already spawned\n", Sv_NetaddrToString(svClient));
    return;
  }

  // handle the case of a level changing while a client was connecting
  if (strtoul(Cmd_Argv(1), NULL, 0) != svs.spawnCount) {
    Com_Debug(DEBUG_SERVER, "Stale spawn count from %s\n", Sv_NetaddrToString(svClient));
    Sv_New_f();
    return;
  }

  start = (uint32_t) strtoul(Cmd_Argv(2), NULL, 0);

  if (start >= MAX_CONFIG_STRINGS) { // catch bad offsets
    Com_Warn("Bad offset from %s\n", Sv_NetaddrToString(svClient));
    Sv_KickClient(svClient, NULL);
    return;
  }

  // write a packet full of data

  NetChan *ch = &svClient->netChan;

  while (start < MAX_CONFIG_STRINGS) {
    const size_t len = q_strlen(sv.configStrings[start]);
    if (len) {
      if (ch->message.size + len >= ch->message.maxSize - 48) {
        break;
      }
      Net_WriteByte(&svClient->netChan.message, SV_CMD_CONFIG_STRING);
      Net_WriteShort(&svClient->netChan.message, start);
      Net_WriteString(&svClient->netChan.message, sv.configStrings[start]);
    }
    start++;
  }

  // send next command
  if (start == MAX_CONFIG_STRINGS) {
    Net_WriteByte(&svClient->netChan.message, SV_CMD_CBUF_TEXT);
    Net_WriteString(&svClient->netChan.message, va("baselines %i 0\n", svs.spawnCount));
  } else {
    Net_WriteByte(&svClient->netChan.message, SV_CMD_CBUF_TEXT);
    Net_WriteString(&svClient->netChan.message,
                    va("config_strings %i %i\n", svs.spawnCount, start));
  }
}

/**
 * @brief Sends entity baseline states to a connecting client in batches.
 */
static void Sv_Baselines_f(void) {
  uint32_t start;
  EntityState nullState;
  EntityState *base;

  Com_Debug(DEBUG_SERVER, "%s\n", Sv_NetaddrToString(svClient));

  if (svClient->state != SV_CLIENT_CONNECTED) {
    Com_Warn("%s already spawned\n", Sv_NetaddrToString(svClient));
    return;
  }

  // handle the case of a level changing while a client was connecting
  if (strtoul(Cmd_Argv(1), NULL, 0) != svs.spawnCount) {
    Com_Debug(DEBUG_SERVER, "Stale spawn count from %s\n", Sv_NetaddrToString(svClient));
    Sv_New_f();
    return;
  }

  start = (uint32_t) strtoul(Cmd_Argv(2), NULL, 0);

  memset(&nullState, 0, sizeof(nullState));

  // write a packet full of data
  while (svClient->netChan.message.size < (MAX_MSG_SIZE >> 1) && start < MAX_ENTITIES) {
    base = &sv.entities[start].baseline;
    if (base->model1 || base->sound || base->effects) {
      Net_WriteByte(&svClient->netChan.message, SV_CMD_BASELINE);
      Net_WriteDeltaEntity(&svClient->netChan.message, &nullState, base, true);
    }
    start++;
  }

  // send next command
  if (start == MAX_ENTITIES) {
    Net_WriteByte(&svClient->netChan.message, SV_CMD_CBUF_TEXT);
    Net_WriteString(&svClient->netChan.message, va("precache %i\n", svs.spawnCount));
  } else {
    Net_WriteByte(&svClient->netChan.message, SV_CMD_CBUF_TEXT);
    Net_WriteString(&svClient->netChan.message, va("baselines %i %i\n", svs.spawnCount, start));
  }
}

/**
 * @brief Transitions a connected client to the active state and spawns them in-game.
 */
static void Sv_Begin_f(void) {

  Com_Debug(DEBUG_SERVER, "%s\n", Sv_NetaddrToString(svClient));

  if (svClient->state != SV_CLIENT_CONNECTED) { // catch duplicate spawns
    Com_Warn("Invalid begin from %s\n", Sv_NetaddrToString(svClient));
    Sv_DropClient(svClient);
    return;
  }

  if (svs.state == SV_ACTIVE_DEMO) {
    return;
  }

  // handle the case of a level changing while a client was connecting
  if (strtoul(Cmd_Argv(1), NULL, 0) != svs.spawnCount) {
    Com_Debug(DEBUG_SERVER, "Stale spawn count from %s\n", Sv_NetaddrToString(svClient));
    Sv_New_f();
    return;
  }

  svClient->state = SV_CLIENT_ACTIVE;

  GameClient *cl = svClient->gclient;

  svs.game->ClientBegin(cl);

  Cbuf_InsertFromDefer();
}

/**
 * @brief The client is going to disconnect, so remove the connection immediately
 */
static void Sv_Disconnect_f(void) {
  Sv_DropClient(svClient);
}

/**
 * @brief Enumeration helper for `Sv_Info_f`.
 */
static void Sv_Info_f_enumerate(Cvar *var, void *data) {

  if (var->flags & CVAR_SERVER_INFO) {

    const ServerClient *client = (ServerClient *) data;
    const GameClient *cl = client->gclient;

    Sv_ClientPrint(cl, PRINT_MEDIUM, "%s %s\n", var->name, var->string);
  }
}

/**
 * @brief Dumps the serverinfo info string
 */
static void Sv_Info_f(void) {

  if (!svClient) { // print to server console
    Com_PrintInfo(Cvar_ServerInfo());
    return;
  }

  Cvar_Enumerate(Sv_Info_f_enumerate, (void *) svClient);
}

typedef struct ServerUserStringCmd {
  char *name;
  void (*func)(void);
} ServerUserStringCmd;

static ServerUserStringCmd svUserStringCmds[] = { // mapping command names to their functions
  { "new", Sv_New_f },
  { "config_strings", Sv_ConfigStrings_f },
  { "baselines", Sv_Baselines_f },
  { "begin", Sv_Begin_f },
  { "disconnect", Sv_Disconnect_f },
  { "info", Sv_Info_f },
  { "demo_seek", Sv_DemoSeek_f },
  { "demo_seek_relative", Sv_DemoSeekRelative_f },
  { "demo_pause", Sv_DemoPause_f },
  { NULL, NULL }
};

/**
 * @brief Invoke the specified user string command. If we don't have a function for
 * it, pass it off to the game module.
 */
static void Sv_UserStringCommand(const char *s) {
  ServerUserStringCmd *c;

  Cmd_TokenizeString(s);

  if (q_strchr(s, '\xFF')) { // catch end of message exploit
    Com_Warn("Illegal command from %s\n", Sv_NetaddrToString(svClient));
    Sv_KickClient(svClient, NULL);
    return;
  }

  for (c = svUserStringCmds; c->name; c++) {

    if (!q_strcmp(Cmd_Argv(0), c->name)) {
      c->func();
      break;
    }
  }

  if (!c->name) { // unmatched command
    if (svs.state == SV_ACTIVE_GAME) { // maybe the game knows what to do with it
      svs.game->ClientCommand(svClient->gclient);
    }
  }
}

/**
 * @brief Account for command time and pass the command to game module.
 */
static void Sv_ClientThink(ServerClient *cl, PlayerMoveCmd *cmd) {

  cl->cmdMsec += cmd->msec;

  svs.game->ClientThink(cl->gclient, cmd);
}

#define CMD_MAX_MOVES 1
#define CMD_MAX_STRINGS 8
#define CMD_MAX_VOICE 8

/**
 * @brief The current `netMessage` is parsed for the given client.
 */
void Sv_ParseClientMessage(ServerClient *cl) {
  int32_t stringsIssued;
  int32_t movesIssued;
  int32_t voiceIssued;

  svClient = cl;

  // allow a finite number of moves and strings
  movesIssued = stringsIssued = voiceIssued = 0;

  while (true) {

    if (netMessage.read > netMessage.size) {
      Com_Warn("Bad read from %s\n", Sv_NetaddrToString(svClient));
      Sv_DropClient(cl);
      return;
    }

    const int32_t c = Net_ReadByte(&netMessage);
    if (c == -1) {
      break;
    }

    switch (c) {

      case CL_CMD_USER_INFO: {
        const char *userInfo = Net_ReadString(&netMessage);

        // leave room for ip stuffing, as the connect does; truncating instead
        // could leave a dangling key for the ip to complete
        if (q_strlen(userInfo) >= sizeof(cl->userInfo) - 25) {
          Com_Print("Oversized user_info from %s\n", Sv_NetaddrToString(cl));
          Sv_KickClient(cl, "Bad user info");
          return;
        }

        q_strlcpy(cl->userInfo, userInfo, sizeof(cl->userInfo));
        if (!Sv_UserInfoChanged(cl)) {
          return;
        }
      }
        break;

      case CL_CMD_ENTITY_INFO: {
        const int16_t number = Net_ReadShort(&netMessage);
        const char *info = Net_ReadString(&netMessage);
        if (!editor->value) {
          Com_Warn("CL_CMD_ENTITY_INFO from %s but editor is disabled\n", Sv_NetaddrToString(cl));
          break;
        }
        if (q_strlen(info)) {
          if (number != -1 && (number < 0 || number >= sv_maxEntities->integer)) {
            Com_Warn("CL_CMD_ENTITY_INFO from %s: bad entity number %d\n", Sv_NetaddrToString(cl), number);
            break;
          }
          Sv_EditEditorEntity(number, info);
        } else {
          if (number < 0 || number >= sv_maxEntities->integer) {
            Com_Warn("CL_CMD_ENTITY_INFO from %s: bad entity number %d\n", Sv_NetaddrToString(cl), number);
            break;
          }
          Sv_FreeEditorEntity(number);
        }
        break;
      }

      case CL_CMD_MOVE: {

        if (cl->state != SV_CLIENT_ACTIVE) {
          // Stale in-flight packets from the previous map arrive during level transitions
          // before the client has processed our reconnect message. Ignore them quietly.
          Com_Debug(DEBUG_SERVER, "CL_CMD_MOVE from inactive client %s\n", Sv_NetaddrToString(cl));
          return;
        }

        if (++movesIssued > CMD_MAX_MOVES) {
          Com_Warn("CMD_MAX_MOVES exceeded for %s\n", Sv_NetaddrToString(cl));
          Sv_DropClient(cl);
          return; // someone is trying to cheat
        }

        const int32_t lastFrame = Net_ReadLong(&netMessage);
        if (lastFrame != cl->lastFrame) {
          cl->lastFrame = lastFrame;

          // the frame number is the client's to choose, so believe it only if we really sent
          // that frame and still hold it; otherwise sentTime is zero and the latency comes
          // out as the server's entire uptime, poisoning the average it feeds
          if (lastFrame > -1 && (uint32_t) lastFrame <= sv.frameNum &&
              sv.frameNum - (uint32_t) lastFrame < PACKET_BACKUP) {

            const uint32_t sentTime = cl->frames[lastFrame & PACKET_MASK].sentTime;

            // the tick counter wraps, so measure the elapsed delta rather than ordering the
            // timestamps, and take it only if it could have come from a frame we still hold
            const uint32_t latency = quetoo.ticks - sentTime;

            if (sentTime && latency <= PACKET_BACKUP * QUETOO_TICK_MILLIS) {

              cl->frameLatency[cl->frameLatencyIndex] = latency;
              cl->frameLatencyIndex = (cl->frameLatencyIndex + 1) % SV_CLIENT_LATENCY_COUNT;

              if (cl->frameLatencyCount < SV_CLIENT_LATENCY_COUNT) {
                cl->frameLatencyCount++;
              }
            }
          }
        }

        // the client sends their 3 most recent movement commands every frame to combat packet loss
        static PlayerMoveCmd null_cmd;
        PlayerMoveCmd cmd[3];
        Net_ReadDeltaMoveCmd(&netMessage, &null_cmd, &cmd[0]);
        Net_ReadDeltaMoveCmd(&netMessage, &cmd[0], &cmd[1]);
        Net_ReadDeltaMoveCmd(&netMessage, &cmd[1], &cmd[2]);

        uint32_t netDrop = cl->netChan.dropped;

        if (netDrop > 1) {
          Sv_ClientThink(cl, &cmd[0]);
        }

        if (netDrop > 0) {
          Sv_ClientThink(cl, &cmd[1]);
        }

        Sv_ClientThink(cl, &cmd[2]);
        break;
      }

      case CL_CMD_VOICE:

        if (++voiceIssued == CMD_MAX_VOICE) {
          Com_Warn("CMD_MAX_VOICE exceeded for %s\n", Sv_NetaddrToString(cl));
          Sv_KickClient(cl, "Too many voice frames.");
          return;
        }

        Sv_ParseVoice(cl);

        if (cl->state == SV_CLIENT_FREE) {
          return; // the frame was malformed, and the client is gone
        }

        break;

      case CL_CMD_STRING:

        // malicious users may try using too many string commands
        if (++stringsIssued == CMD_MAX_STRINGS) {
          Com_Warn("CMD_MAX_STRINGS exceeded for %s\n", Sv_NetaddrToString(cl));
          Sv_KickClient(cl, "Too many commands.");
          return;
        }

        Sv_UserStringCommand(Net_ReadString(&netMessage));

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
