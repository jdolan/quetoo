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
#include "net/net_http.h"

#include <SDL3/SDL_atomic.h>
#include <SDL3/SDL_timer.h>

/**
 * @brief State for an in-progress async HTTP download.
 */
static struct {
	SDL_AtomicInt complete;
	int32_t status;
	void *data;
	size_t length;
} cl_download;

/**
 * @brief Net_HttpCallback for Cl_CheckOrDownloadFile.
 */
static void Cl_DownloadComplete(int32_t status, void *body, size_t length, void *user_data) {

	if (body && length) {
		cl_download.data = Mem_Malloc(length);
		memcpy(cl_download.data, body, length);
		cl_download.length = length;
	} else {
		cl_download.data = NULL;
		cl_download.length = 0;
	}

	cl_download.status = status;
	SDL_SetAtomicInt(&cl_download.complete, 1);
}

static char *sv_cmd_names[32] = {
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

  if (IS_INVALID_DOWNLOAD(filename)) {
    Com_Warn("Refusing to download \"%s\"\n", filename);
    return;
  }

  Com_Debug(DEBUG_CLIENT, "Checking for %s\n", filename);

  if (Fs_Exists(filename)) {
    return;
  }

  // derive the download URL from the server address we're connected to
  char url[MAX_OS_PATH];
  Net_HttpUrl(&cls.net_chan.remote_address, filename, url, sizeof(url));

  Com_Print("Downloading %s...\n", filename);

  SDL_SetAtomicInt(&cl_download.complete, 0);
  cl_download.status = 0;
  cl_download.data = NULL;
  cl_download.length = 0;

  Net_HttpGetAsync(url, Cl_DownloadComplete, NULL);

  const char *base = Basename(filename);
  while (!SDL_GetAtomicInt(&cl_download.complete)) {

    if (cls.state == CL_DISCONNECTED) {
      Com_Warn("Disconnected during download of %s\n", filename);
      return;
    }

    Cl_LoadingProgress(-1, va("Downloading %s", base));
    SDL_Delay(16);
  }

  if (cl_download.status != 200 || !cl_download.data || !cl_download.length) {
    Com_Warn("Failed to download %s (HTTP %d)\n", filename, cl_download.status);
    if (cl_download.data) {
      Mem_Free(cl_download.data);
    }
    return;
  }

  if (cl_download.length > MAX_DOWNLOAD_SIZE) {
    Com_Warn("Download %s exceeds maximum size (%zu bytes)\n", filename, cl_download.length);
    Mem_Free(cl_download.data);
    return;
  }

  // write to a temp file, then rename
  char tempname[MAX_OS_PATH];
  StripExtension(filename, tempname);
  g_strlcat(tempname, ".tmp", sizeof(tempname));

  file_t *file = Fs_OpenWrite(tempname);
  if (!file) {
    Com_Warn("Failed to open %s for writing\n", tempname);
    Mem_Free(cl_download.data);
    return;
  }

  Fs_Write(file, cl_download.data, 1, cl_download.length);
  Fs_Close(file);
  Mem_Free(cl_download.data);

  if (Fs_Rename(tempname, filename)) {
    Com_Print("Downloaded %s (%zu bytes)\n", filename, cl_download.length);

    if (strstr(filename, ".pk3")) {
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
 * @brief The server sends this command just after server_data. Hang onto the spawn
 * count and check for the media we'll need to enter the game.
 */
void Cl_Precache_f(void) {

  if (Cmd_Argc() != 2) {
    Com_Print("Usage: %s <spawn_count>\n", Cmd_Argv(0));
    return;
  }

  cls.spawn_count = (uint32_t) strtoul(Cmd_Argv(1), NULL, 0);

  cl.precache_check = CS_PK3;

  Cl_RequestNextDownload();
}

/**
 * @brief Parses the baseline entity state for the given entity number.
 */
static void Cl_ParseBaseline(void) {
  static entity_state_t null_state;

  const int16_t number = Net_ReadShort(&net_message);
  const uint16_t bits = Net_ReadShort(&net_message);

  cl_entity_t *ent = &cl.entities[number];

  Net_ReadDeltaEntity(&net_message, &null_state, &ent->baseline, number, bits);
}

/**
 * @brief Reads a command buffer text string from the server and appends it to the client cbuf.
 */
static const char *Cl_ParseCbufText(void) {

  const char *text = Net_ReadString(&net_message);

  Cbuf_AddText(text);

  return text;
}

/**
 * @brief Parses a config string update from the server message and applies it to the client.
 */
int32_t Cl_ParseConfigString(void) {
  const int32_t i = Net_ReadShort(&net_message);

  if (i >= MAX_CONFIG_STRINGS) {
    Com_Error(ERROR_DROP, "Invalid index %i\n", i);
  }

  strcpy(cl.config_strings[i], Net_ReadString(&net_message));
  const char *s = cl.config_strings[i];

  if (i >= CS_MODELS && i < CS_MODELS + MAX_MODELS) {
    if (cls.state == CL_ACTIVE) {
      cl.models[i - CS_MODELS] = R_LoadModel(s);
      if (*s == '*') {
        cl.cm_models[i - CS_MODELS] = Cm_Model(s);
      } else {
        cl.cm_models[i - CS_MODELS] = NULL;
      }
    }
  } else if (i >= CS_SOUNDS && i < CS_SOUNDS + MAX_SOUNDS) {
    if (cls.state == CL_ACTIVE) {
      cl.sounds[i - CS_SOUNDS] = S_LoadSample(s);
    }
  } else if (i >= CS_IMAGES && i < CS_IMAGES + MAX_IMAGES) {
    if (cls.state == CL_ACTIVE) {
      cl.images[i - CS_IMAGES] = R_LoadImage(s, IMG_PIC);
    }
  } else if (i >= CS_ENTITIES && i < CS_ENTITIES + MAX_ENTITIES) {
    cls.cgame->ParseEditorEntity(i - CS_ENTITIES, s);
  }

  return i;
}

/**
 * @brief Parses the initial server data packet, resetting client state and loading the game.
 */
static void Cl_ParseServerData(void) {

  // wipe the cl_client_t struct
  Cl_ClearState();

  Cl_SetKeyDest(KEY_CONSOLE);

  // parse protocol version number
  const int32_t major = Net_ReadLong(&net_message);
  const int32_t minor = Net_ReadLong(&net_message);

  // ensure protocol major matches
  if (major != PROTOCOL_MAJOR) {
    Com_Error(ERROR_DROP, "Server is using protocol major %d, you have %d\n", major, PROTOCOL_MAJOR);
  }

  // determine if we're viewing a demo
  cl.demo_server = Net_ReadByte(&net_message);

  // game directory
  char *str = Net_ReadString(&net_message);
  if (g_strcmp0(Cvar_GetString("game"), str)) {

    Fs_SetGame(str);

    // reload the client game
    Cl_InitCgame();
  }

  // ensure protocol minor matches
  if (minor != cls.cgame->protocol) {
    Com_Error(ERROR_DROP, "Server is using protocol minor %d, you have %d\n", minor, cls.cgame->protocol);
  }

  // get the full level name
  str = Net_ReadString(&net_message);
  Com_Print("\n");
  Com_Print("^2%s^7\n", str);
}

/**
 * @brief Parses an incoming SVC_PRINT message.
 */
static void Cl_ParsePrint(void) {

  const byte level = Net_ReadByte(&net_message);
  const char *string = Net_ReadString(&net_message);

  // the server shouldn't have sent us anything below our level anyway
  if (level >= message_level->integer) {

    // check to see if we should ignore the message
    if (*cl_ignore->string) {
      parser_t parser = Parse_Init(cl_ignore->string, PARSER_DEFAULT);
      char pattern[MAX_STRING_CHARS];

      while (true) {

        if (!Parse_Token(&parser, PARSE_DEFAULT, pattern, sizeof(pattern))) {
          break;
        }

        if (GlobMatch(pattern, string, GLOB_FLAGS_NONE)) {
          return;
        }
      }
    }

    char *sample = NULL;
    switch (level) {
      case PRINT_CHAT:
      case PRINT_TEAM_CHAT:
        if (level == PRINT_CHAT && *cl_chat_sound->string) {
          sample = cl_chat_sound->string;
        } else if (level == PRINT_TEAM_CHAT && *cl_team_chat_sound->string) {
          sample = cl_team_chat_sound->string;
        }
        break;
      default:
        break;
    }

    if (sample) {
      S_AddSample(&cl_stage, &(s_play_sample_t) {
        .sample = S_LoadSample(sample),
        .flags = S_PLAY_UI
      });
    }

    Con_Append(level, string);
  }
}

/**
 * @brief Prints a network message label when `cl_draw_net_messages` >= 2.
 */
static void Cl_ShowNet(const char *s) {
  if (cl_draw_net_messages->integer >= 2) {
    Com_Print("%3u: %s\n", (uint32_t) (net_message.read - 1), s);
  }
}

/**
 * @brief Parses a complete server message, dispatching each command to its handler.
 */
void Cl_ParseServerMessage(void) {
  int32_t cmd, old_cmd;

  if (cl_draw_net_messages->integer == 1) {
    Com_Print("%u ", (uint32_t) net_message.size);
  } else if (cl_draw_net_messages->integer >= 2) {
    Com_Print("------------------\n");
  }

  cmd = SV_CMD_BAD;

  // parse the message
  while (true) {
    if (net_message.read > net_message.size) {
      Com_Error(ERROR_DROP, "Bad server message\n");
    }

    old_cmd = cmd;
    cmd = Net_ReadByte(&net_message);

    if (cmd == -1) {
      Cl_ShowNet("END OF MESSAGE");
      break;
    }

    if (cl_draw_net_messages->integer >= 2 && sv_cmd_names[cmd]) {
      Cl_ShowNet(sv_cmd_names[cmd]);
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
        cls.connect_time = 0; // fire immediately
        break;

      case SV_CMD_SERVER_DATA:
        Cl_ParseServerData();
        break;

      default:
        // delegate to the client game module before failing
        if (!cls.cgame->ParseMessage(cmd)) {
          Com_Error(ERROR_DROP, "Illegible server message:\n"
                    " %d: last command was %s\n", cmd, sv_cmd_names[old_cmd]);
        }
        break;
    }

    cls.cgame->ParsedMessage(cmd, data);
  }

  Cl_AddNetGraph();

  Cl_WriteDemoMessage();
}
