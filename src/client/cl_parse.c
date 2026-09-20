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

#include <Objectively/RESTClient.h>
#include "net/net_http_server.h"

#include <SDL3/SDL_atomic.h>
#include <SDL3/SDL_timer.h>

/**
 * @brief State for an in-progress async HTTP download.
 */
static struct {
	SDL_AtomicInt complete;
	int32_t status;
	Data *data;
} module;

/**
 * @brief `RESTClientCompletion` for `Cl_CheckOrDownloadFile`.
 */
static void Cl_DownloadComplete(int32_t status, Data *data, void *userData) {

	release(module.data);
	module.data = (status == 200 && data) ? retain(data) : NULL;

	module.status = status;
	SDL_SetAtomicInt(&module.complete, 1);
}

static char *svCmdNames[32] = {
  "SV_CMD_BAD",
  "SV_CMD_BASELINE",
  "SV_CMD_CBUF_TEXT",
  "SV_CMD_CONFIG_STRING",
  "SV_CMD_DISCONNECT",
  "SV_CMD_DROP",
  "SV_CMD_FRAME",
  "SV_CMD_PRINT",
  "SV_CMD_RECONNECT",
  "SV_CMD_SERVER_DATA",
  "SV_CMD_DEMO_INFO",
  "SV_CMD_SOUND"
};

/**
 * @brief Maximum download size (128 MB).
 */
#define MAX_DOWNLOAD_SIZE (128 * 1024 * 1024)

/**
 * @brief If the file does not exist locally, download it from the server via HTTP.
 */
void Cl_CheckOrDownloadFile(const char *filename) {

  if (cls.state == CL_DISCONNECTED) {
    Com_Print("Not connected\n");
    return;
  }

  if (!Com_IsValidDownload(filename)) {
    Com_Warn("Refusing to download \"%s\"\n", filename);
    return;
  }

  Com_Debug(DEBUG_CLIENT, "Checking for %s\n", filename);

  if (Fs_Exists(filename)) {
    return;
  }

  // derive the download URL from the server address we're connected to
  char url[MAX_OS_PATH];
  Net_HttpUrl(&cls.netChan.remoteAddress, filename, url, sizeof(url));

  Com_Print("Downloading %s...\n", filename);

  SDL_SetAtomicInt(&module.complete, 0);
  module.status = 0;
  module.data = release(module.data);

  $($$(RESTClient, sharedInstance), getAsync, url, NULL, Cl_DownloadComplete, NULL);

  const char *base = Basename(filename);
  while (!SDL_GetAtomicInt(&module.complete)) {

    if (cls.state == CL_DISCONNECTED) {
      Com_Warn("Disconnected during download of %s\n", filename);
      return;
    }

    Cl_LoadingProgress(-1, va("Downloading %s", base));
    SDL_Delay(16);
  }

  if (module.status != 200 || !module.data) {
    Com_Warn("Failed to download %s (HTTP %d)\n", filename, module.status);
    module.data = release(module.data);
    return;
  }

  if (module.data->length > MAX_DOWNLOAD_SIZE) {
    Com_Warn("Download %s exceeds maximum size (%zu bytes)\n", filename, module.data->length);
    module.data = release(module.data);
    return;
  }

  // write to a temp file, then rename
  char tempname[MAX_OS_PATH];
  StripExtension(filename, tempname);
  q_strlcat(tempname, ".tmp", sizeof(tempname));

  File *file = Fs_OpenWrite(tempname);
  if (!file) {
    Com_Warn("Failed to open %s for writing\n", tempname);
    module.data = release(module.data);
    return;
  }

  Fs_Write(file, module.data->bytes, 1, module.data->length);
  Fs_Close(file);

  const size_t downloadedLength = module.data->length;

  module.data = release(module.data);

  if (Fs_Rename(tempname, filename)) {
    Com_Print("Downloaded %s (%zu bytes)\n", filename, downloadedLength);

    if (q_strstr(filename, ".pk3")) {
      Fs_AddToSearchPath(filename);
    }
  } else {
    Com_Error(ERROR_DROP, "Failed to rename %s\n", tempname);
  }
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
 * @brief The server sends this command just after `server_data`. Hang onto the spawn
 * count and check for the media we'll need to enter the game.
 */
void Cl_Precache_f(void) {

  if (Cmd_Argc() != 2) {
    Com_Print("Usage: %s <spawn_count>\n", Cmd_Argv(0));
    return;
  }

  cls.server.spawnCount = (uint32_t) strtoul(Cmd_Argv(1), NULL, 0);

  cl.precacheCheck = CS_PK3;

  Cl_RequestNextDownload();
}

/**
 * @brief Parses the baseline entity state for the given entity number.
 */
static void Cl_ParseBaseline(void) {
  static EntityState null_state;

  const int16_t number = Net_ReadShort(&netMessage);
  const uint16_t bits = Net_ReadShort(&netMessage);

  if (number < 0 || number >= MAX_ENTITIES) {
    Com_Error(ERROR_DROP, "Invalid entity number: %d\n", number);
  }

  ClientEntity *ent = &cl.entities[number];

  Net_ReadDeltaEntity(&netMessage, &null_state, &ent->baseline, number, bits);
}

/**
 * @brief Reads a command buffer text string from the server and appends it to the client cbuf.
 */
static const char *Cl_ParseCbufText(void) {

  const char *text = Net_ReadString(&netMessage);

  Cbuf_AddText(text);

  return text;
}

/**
 * @brief Parses a config string update from the server message and applies it to the client.
 */
int32_t Cl_ParseConfigString(void) {
  const int32_t i = Net_ReadShort(&netMessage);

  if (i < 0 || i >= MAX_CONFIG_STRINGS) {
    Com_Error(ERROR_DROP, "Invalid index %i\n", i);
  }

  q_strlcpy(cl.configStrings[i], Net_ReadString(&netMessage), MAX_STRING_CHARS);

  const char *s = cl.configStrings[i];

  if (i >= CS_MODELS && i < CS_MODELS + MAX_MODELS) {
    if (cls.state == CL_ACTIVE) {
      cl.models[i - CS_MODELS] = R_LoadModel(s);
      if (*s == '*') {
        cl.cmModels[i - CS_MODELS] = Cm_Model(s);
      } else {
        cl.cmModels[i - CS_MODELS] = NULL;
      }
    }
  } else if (i >= CS_SOUNDS && i < CS_SOUNDS + MAX_SOUNDS) {
    if (cls.state == CL_ACTIVE) {
      cl.sounds[i - CS_SOUNDS] = S_LoadSample(s, ASSET_CONTEXT_SOUNDS);
    }
  } else if (i >= CS_ENTITIES && i < CS_ENTITIES + MAX_ENTITIES) {
    cls.cgame->ParseEditorEntity(i - CS_ENTITIES, s);
  }

  return i;
}

/**
 * @brief Parses the demo duration sent once when connecting to a demo relay. Arrives just ahead
 * of, in the same packet as, the relayed SV_CMD_SERVER_DATA - stored on cls.demo rather than cl
 * so Cl_ClearState's memset of cl (triggered by that very next command) doesn't wipe it back out.
 */
static void Cl_ParseDemoInfo(void) {

  cls.demo.duration = Net_ReadLong(&netMessage);
  cls.demo.paused = Net_ReadByte(&netMessage);

  Com_Debug(DEBUG_CLIENT, "Demo duration %d ms, paused %d\n", cls.demo.duration, cls.demo.paused);
}

/**
 * @brief Parses a chat message and hands it to the client game to render.
 * @details Chat carries its sender rather than arriving pre-formatted, so the module decides how it
 * reads and whether it is shown at all. The engine prints nothing here.
 */
static void Cl_ParseChat(void) {

  const int32_t client = Net_ReadByte(&netMessage);
  const uint8_t flags = Net_ReadByte(&netMessage);
  const char *message = Net_ReadString(&netMessage);

  if (client < 0 || client >= MAX_CLIENTS) {
    Com_Debug(DEBUG_CLIENT, "Rejecting chat from client %d\n", client);
    return;
  }

  if (cls.cgame) {
    cls.cgame->Chat(client, flags, message);
  }
}

/**
 * @brief Parses a relayed voice frame and hands it to the sound subsystem.
 */
static void Cl_ParseVoice(void) {

  const int32_t client = Net_ReadByte(&netMessage);
  const uint8_t seq = Net_ReadByte(&netMessage);
  const uint8_t flags = Net_ReadByte(&netMessage);

  const int32_t len = Net_ReadByte(&netMessage);

  if (len <= 0 || len > VOICE_MAX_PAYLOAD) {
    Com_Error(ERROR_DROP, "Illegible voice frame of %d bytes\n", len);
  }

  if (client < 0 || client >= MAX_CLIENTS) {
    Com_Error(ERROR_DROP, "Illegible voice frame from client %d\n", client);
  }

  byte data[VOICE_MAX_PAYLOAD];
  Net_ReadData(&netMessage, data, len);

  if (cls.cgame && !cls.cgame->Voice(client, flags)) {
    return;
  }

  cl.voiceTime[client] = cl.unclampedTime;

  S_AddVoice(client, seq, flags, data, len);
}

/**
 * @brief Parses the initial server data packet, resetting client state and loading the game.
 */
static void Cl_ParseServerData(void) {

  Cl_ClearState();

  Cl_SetKeyDest(KEY_CONSOLE);

  // parse protocol version number
  const int32_t major = Net_ReadLong(&netMessage);
  const int32_t minor = Net_ReadLong(&netMessage);

  // ensure protocol major matches
  if (major != PROTOCOL_MAJOR) {
    Com_Error(ERROR_DROP, "Server is using protocol major %d, you have %d\n", major, PROTOCOL_MAJOR);
  }

  // determine if we're viewing a demo
  cl.demoServer = Net_ReadByte(&netMessage);

  if (cl.demoServer) {
    Com_Print("^3Demo playback controls:^7\n"
              "  Pause/resume:  ^2SPACE^7\n"
              "  Prev frame:    ^2LEFT^7\n"
              "  Next frame:    ^2RIGHT^7\n"
              "  Slower/faster: ^2, / .^7\n");
  }

  // the game and client game directories, validated before being copied off:
  // truncating first would turn an over-long name into a legal one
  const char *s = Net_ReadString(&netMessage);

  if (!Com_IsValidGame(s)) {
    Com_Error(ERROR_DROP, "Server sent an invalid game directory: %s\n", s);
  }

  char game[MAX_QPATH];
  q_strlcpy(game, s, sizeof(game));

  s = Net_ReadString(&netMessage);

  if (!Com_IsValidGame(s)) {
    Com_Error(ERROR_DROP, "Server sent an invalid cgame directory: %s\n", s);
  }

  char cgame[MAX_QPATH];
  q_strlcpy(cgame, s, sizeof(cgame));

  // ensure we have the required cgame installed
  if (!Sys_HasLibrary(cgame, "cgame")) {
    Com_Error(ERROR_DROP, "Server requires uninstalled client game: %s\n", cgame);
  }

  // set the game directory
  if (!Com_SetGame(game, cgame)) {
    Com_Error(ERROR_DROP, "Failed to set game: %s\n", game);
  }

  // only the module we hold can say which one it is, and it says so only once a
  // load has succeeded, so a load that failed is retried rather than remembered
  if (!cls.cgame || q_strcmp(cls.cgame->name, cgame)) {
    Cl_InitCgame();
  }

  // ensure the module we loaded is the module the server expects
  if (q_strcmp(cls.cgame->name, cgame)) {
    Com_Error(ERROR_DROP, "Server requires client game %s, you loaded %s\n", cgame, cls.cgame->name);
  }

  // ensure protocol minor matches
  if (minor != cls.cgame->protocol) {
    Com_Error(ERROR_DROP, "Server is using protocol minor %d, you have %d\n", minor, cls.cgame->protocol);
  }

  // get the full level name
  const char *name = Net_ReadString(&netMessage);
  Com_Print("\n");
  Com_Print("^2%s^7\n", name);
}

/**
 * @brief Parses an incoming `SVC_PRINT` message.
 */
static void Cl_ParsePrint(void) {

  const byte level = Net_ReadByte(&netMessage);
  const char *string = Net_ReadString(&netMessage);

  // the server shouldn't have sent us anything below our level anyway
  if (level >= messageLevel->integer) {

    // chat from a player arrives as SV_CMD_CHAT and is sounded by the client game, which is the
    // only side that knows what kind of message it is; this remains for console originated chat
    char *sample = NULL;
    if (level == PRINT_CHAT && *cl_chatSound->string) {
      sample = cl_chatSound->string;
    }

    if (sample) {
      S_AddSample(&clStage, &(SoundPlaySample) {
        .sample = S_LoadSample(sample, ASSET_CONTEXT_SOUNDS),
        .flags = S_PLAY_UI
      });
    }

    Con_Append(level, string);
  }
}

/**
 * @brief Prints a network message label when `cl_drawNetMessages` >= 2.
 */
static void Cl_ShowNet(const char *s) {
  if (cl_drawNetMessages->integer >= 2) {
    Com_Print("%3u: %s\n", (uint32_t) (netMessage.read - 1), s);
  }
}

/**
 * @brief Folds any dropped packets into the client's drop counter.
 * @remarks Round trip time is not measured here. The client can only sample it as far apart as
 * it reads its socket, which is once per rendered frame, so its figure was quantized to the
 * frame interval and disagreed with the server's. The server stamps packets at their arrival
 * time and reports the result in `STAT_PING`, which is what the HUD and the scoreboard read.
 */
static void Cl_UpdateNetStats(void) {

  if (cls.state != CL_ACTIVE) {
    return;
  }

  cl.dropped += cls.netChan.dropped;
}

/**
 * @brief Parses a complete server message, dispatching each command to its handler.
 */
void Cl_ParseServerMessage(void) {
  int32_t cmd, oldCmd;

  if (cl_drawNetMessages->integer == 1) {
    Com_Print("%u ", (uint32_t) netMessage.size);
  } else if (cl_drawNetMessages->integer >= 2) {
    Com_Print("------------------\n");
  }

  cmd = SV_CMD_BAD;

  // parse the message
  while (true) {
    if (netMessage.read > netMessage.size) {
      Com_Error(ERROR_DROP, "Bad server message\n");
    }

    const size_t cmdStart = netMessage.read;

    oldCmd = cmd;
    cmd = Net_ReadByte(&netMessage);

    if (cmd == -1) {
      Cl_ShowNet("END OF MESSAGE");
      break;
    }

    if (cl_drawNetMessages->integer >= 2 && cmd < (int32_t) lengthof(svCmdNames) && svCmdNames[cmd]) {
      Cl_ShowNet(svCmdNames[cmd]);
    }

    void *data = NULL;

    switch (cmd) {

      case SV_CMD_BASELINE:
        Cl_ParseBaseline();
        break;

      case SV_CMD_CBUF_TEXT:
        Cl_ParseCbufText();
        break;

      case SV_CMD_CONFIG_STRING:
        data = (void *) (intptr_t) Cl_ParseConfigString();
        break;

      case SV_CMD_DISCONNECT:
        Cl_Disconnect();
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
          Fs_Close(cls.download.file);
          memset(&cls.download, 0, sizeof(cls.download));
        }
        cls.state = CL_CONNECTING;
        cls.server.connectTime = 0; // fire immediately
        break;

      case SV_CMD_SERVER_DATA:
        Cl_ParseServerData();
        break;

      case SV_CMD_DEMO_INFO:
        Cl_ParseDemoInfo();
        break;

      case SV_CMD_CHAT:
        Cl_ParseChat();
        break;

      case SV_CMD_VOICE:
        Cl_ParseVoice();
        break;

      default:
        // delegate to the client game module before failing
        if (!cls.cgame->ParseMessage(cmd)) {
          Com_Error(ERROR_DROP, "Illegible server message:\n"
                    " %d: last command was %s\n", cmd,
                    oldCmd < (int32_t) lengthof(svCmdNames) ? svCmdNames[oldCmd] : "unknown");
        }
        break;
    }

    cls.cgame->ParsedMessage(cmd, data);

    // capture every non-frame command verbatim so it rides along with the next recorded demo
    // frame: chat, centerprint, temp entities, sounds, etc. are one-shot events, not part of the
    // continuous entity/player state Cl_WriteDemoMessage re-synthesizes from cl.frame.
    // Voice is the exception: recording speech to disk is a consent question, it would swamp the
    // event buffer and start dropping the events above, and the demo relay re-broadcasts to every
    // spectator with none of the sender's recipient filtering.
    // Deliberately NOT reset at the top of this function: Sv_SendClientDatagram fragments a
    // tick's datagram into a second packet (its own Cl_ParseServerMessage call) when queued
    // messages overflow one packet, and that second packet carries no SV_CMD_FRAME at all -
    // resetting here would silently drop whatever events landed in it. Cl_WriteDemoMessage
    // clears this once it actually flushes the accumulated bytes into a recorded frame.
    if (cls.demo.file && cmd != SV_CMD_FRAME && cmd != SV_CMD_VOICE) {
      const size_t len = netMessage.read - cmdStart;
      if (cls.demo.eventSize + len <= sizeof(cls.demo.eventBuffer)) {
        memcpy(cls.demo.eventBuffer + cls.demo.eventSize, netMessage.data + cmdStart, len);
        cls.demo.eventSize += len;
      } else {
        Com_Warn("Demo event buffer full, dropping command %d\n", cmd);
      }
    }
  }

  Cl_UpdateNetStats();

  Cl_WriteDemoMessage();
}
