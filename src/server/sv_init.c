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
#include "common/demo.h"

/**
 * @brief Searches `sv.`config_strings` from the specified start, searching for the
 * desired name. If not found, the name can be optionally created and sent to
 * all connected clients. This allows the game to lazily load assets.
 */
static int32_t Sv_FindIndex(const char *name, int32_t start, int32_t max, bool create) {
  int32_t i;

  if (!name || !name[0]) {
    return 0;
  }

  for (i = 0; i < max && sv.config_strings[start + i][0]; i++)
    if (!q_strcmp(sv.config_strings[start + i], name)) {
      return i;
    }

  if (!create) {
    return 0;
  }

  if (i == max) {
    Com_Warn("Max index for %s\n", name);
    return 0;
  }

  q_strlcpy(sv.config_strings[start + i], name, sizeof(sv.config_strings[i]));

  if (svs.state != SV_LOADING) { // send the update to everyone
    Mem_ClearBuffer(&sv.multicast);
    Net_WriteByte(&sv.multicast, SV_CMD_CONFIG_STRING);
    Net_WriteShort(&sv.multicast, start + i);
    Net_WriteString(&sv.multicast, name);
    Sv_Multicast(Vec3_Zero(), MULTICAST_ALL_R);
  }

  return i;
}

int32_t Sv_ModelIndex(const char *name) {
  return Sv_FindIndex(name, CS_MODELS, MAX_MODELS, true);
}

int32_t Sv_SoundIndex(const char *name) {
  return Sv_FindIndex(name, CS_SOUNDS, MAX_SOUNDS, true);
}

int32_t Sv_ImageIndex(const char *name) {
  return Sv_FindIndex(name, CS_IMAGES, MAX_IMAGES, true);
}

/**
 * @brief Entity baselines are used to compress the update messages
 * to the clients -- only the fields that differ from the
 * baseline will be transmitted
 */
static int32_t Sv_CreateBaseline(void) {

  int32_t count = 0;

  for (int32_t i = 0; i < sv_max_entities->integer; i++) {

    g_entity_t *ent = sv.entities[i].gent;

    if (!ent || !ent->in_use) {
      continue;
    }

    if (!ent->s.model1 && !ent->s.sound && !ent->s.effects) {
      continue;
    }

    ent->s.number = i;

    // take current state as baseline
    sv.entities[i].baseline = ent->s;

    count++;
  }

  return count;
}

/**
 * @brief Sends the shutdown message message to all connected clients. The message
 * is sent immediately, because the server could completely terminate after
 * returning from this function.
 */
static void Sv_ShutdownMessage(const char *msg, bool reconnect) {

  if (svs.state == SV_UNINITIALIZED) {
    return;
  }

  Mem_ClearBuffer(&net_message);

  if (msg) { // send message
    Net_WriteByte(&net_message, SV_CMD_PRINT);
    Net_WriteByte(&net_message, PRINT_HIGH);
    Net_WriteString(&net_message, msg);
  }

  if (reconnect) { // send reconnect
    Net_WriteByte(&net_message, SV_CMD_RECONNECT);
  } else { // or just disconnect
    Net_WriteByte(&net_message, SV_CMD_DISCONNECT);
  }

  sv_client_t *cl = svs.clients;
  for (int32_t i = 0; i < sv_max_clients->integer; i++, cl++)
    if (cl->state >= SV_CLIENT_CONNECTED) {
      Netchan_Transmit(&cl->net_chan, net_message.data, net_message.size);
    }
}

/**
 * @brief Wipes the `sv_server_t` structure after freeing any references it holds.
 */
static void Sv_ClearState(void) {

  if (svs.state == SV_UNINITIALIZED) {
    return;
  }

  Sv_StopServerRecord();

  if (sv.demo_file) {
    Fs_Close(sv.demo_file);
  }

  Sv_DemoSeekCleanup();

  Demo_FreeIndex(&sv.demo_index);

  memset(&sv, 0, sizeof(sv));
  Com_QuitSubsystem(QUETOO_SERVER);

  svs.next_heartbeat = 0;
}

/**
 * @brief Applies any pending variable changes and clamps ones we really care about.
 */
static void Sv_UpdateLatchedVars(void) {

  Cvar_UpdateLatched();

  sv_max_clients->integer = Clampf(sv_max_clients->integer, 1, MAX_CLIENTS);
}

/**
 * @brief Allocates the client array for the current server session.
 */
static void Sv_InitClients(void) {

  svs.clients = Mem_TagMalloc(sizeof(sv_client_t) * sv_max_clients->integer, MEM_TAG_SERVER);

  for (int32_t i = 0; i < sv_max_clients->integer; i++) {
    svs.clients[i].gclient = svs.game->clients[i];
  }
}

/**
 * @brief Allocates the entity state ring buffer used for delta compression.
 */
static void Sv_InitEntityState(void) {
  svs.num_entity_states = PACKET_BACKUP * MAX_ENTITIES;
  svs.entity_states = Mem_TagMalloc(sizeof(entity_state_t) * svs.num_entity_states, MEM_TAG_SERVER);
}

/**
 * @brief Gracefully frees all resources allocated to `svs.clients`.
 */
static void Sv_ShutdownClients(void) {

  if (svs.state == SV_UNINITIALIZED) {
    return;
  }

  sv_client_t *cl = svs.clients;
  for (int32_t i = 0; i < sv_max_clients->integer; i++, cl++) {
    Sv_HttpClientDisconnect(&cl->http);
  }

  Mem_Free(svs.clients);
  svs.clients = NULL;

  Mem_Free(svs.entity_states);
  svs.entity_states = NULL;
}

/**
 * @brief Resets all spawned clients back to the connected state for a map change.
 */
static void Sv_ReconnectClients(void) {

  for (int32_t i = 0; i < sv_max_clients->integer; i++) {

    // re-bind the game client pointer (may have been cleared on bot disconnect)
    svs.clients[i].gclient = svs.game->clients[i];

    // reset state of spawned clients back to connected
    if (svs.clients[i].state > SV_CLIENT_CONNECTED) {
      svs.clients[i].state = SV_CLIENT_CONNECTED;
    }

    // invalidate last frame to force a baseline
    svs.clients[i].last_frame = -1;
    svs.clients[i].last_message = quetoo.ticks;
  }
}

/**
 * @brief Reloads `svs.clients`, `svs.client_entities`, the game programs, etc. Because
 * we must allocate clients and edicts based on sizes the game module requests,
 * we refresh the game module.
 */
static void Sv_InitEntities(sv_state_t state) {

  if (svs.state == SV_UNINITIALIZED || Cvar_PendingLatched()) {

    if (svs.state != SV_UNINITIALIZED) {
      svs.state = SV_LOADING;
    }

    Sv_ShutdownGame();

    Sv_ShutdownClients();

    Sv_UpdateLatchedVars();

    Sv_InitGame();

    Sv_InitClients();

    Sv_InitEntityState();
  } else {
    Sv_ReconnectClients();
  }

  svs.spawn_count++;
}

/**
 * @brief Loads the map or demo file and populates the server-controlled "config
 * strings."  We hand off the entity string to the game module, which will
 * load the rest.
 */
static void Sv_LoadMedia(const char *name, const cm_entity_t *props, sv_state_t state) {
  int64_t bsp_size = -1;

  strcpy(sv.name, name);
  strcpy(sv.config_strings[CS_MESSAGE], name);

  if (state == SV_ACTIVE_DEMO) { // loading a demo
    Cvar_ForceSetString(sv_map->name, "");

    sv.demo_file = Fs_OpenRead(va("demos/%s.demo", sv.name));
    svs.spawn_count = 0;

    // detect the v2 container; if present, consume its header so the read
    // cursor is left at the first record. Otherwise fall back to legacy v1.
    sv.demo_v2 = false;
    sv.demo_time = 0;

    if (sv.demo_file && Demo_IsV2(sv.demo_file)) {
      demo_header_t header;
      if (Demo_ReadHeader(sv.demo_file, &header, NULL)) {
        sv.demo_v2 = true;
        sv.demo_speed = 1.0;
        sv.demo_paused = false;

        // pre-decode the whole demo into a clean, fully seekable stream (periodic
        // keyframes + sequential deltas) so scrubbing can't break the delta chain
        if (!Sv_DemoMakeSeekable(&header)) {
          Com_Warn("  Seek re-encode failed; forward playback only.\n");
        }

        // build the keyframe seek index; this leaves the cursor at the first record
        Demo_ScanIndex(sv.demo_file, &sv.demo_index);
        Com_Print("  %u keyframe(s), %.1fs.\n", sv.demo_index.count, sv.demo_index.duration / 1000.0);
      } else {
        Com_Warn("Corrupt v2 demo header in %s\n", sv.name);
      }
    }

    Com_Print("  Loaded demo %s (%s).\n", sv.name, sv.demo_v2 ? "v2" : "v1");
  } else { // loading a map
    Cvar_ForceSetString(sv_map->name, sv.name);

    q_snprintf(sv.config_strings[CS_BSP], MAX_STRING_CHARS, "maps/%s.bsp", sv.name);

    sv.cm_models[0] = Cm_LoadBspModel(sv.config_strings[CS_BSP], &bsp_size);

    const char *dir = Fs_RealDir(sv.config_strings[CS_BSP]);
    const size_t dir_len = q_strlen(dir);
    if (dir_len >= 4 && !q_strcmp(dir + dir_len - 4, ".pk3")) {
      q_strlcpy(sv.config_strings[CS_PK3], Basename(dir), MAX_STRING_CHARS);
    } else {
      sv.config_strings[CS_PK3][0] = '\0';
    }

    q_snprintf(sv.config_strings[CS_MANIFEST], MAX_STRING_CHARS, "maps/%s.mf", sv.name);

    for (int32_t i = 0; i < Cm_NumModels(); i++) {

      if (i == MAX_MODELS) {
        Com_Error(ERROR_DROP, "Sub-model count exceeds protocol limits\n");
      }

      char *s = sv.config_strings[CS_MODELS + i];
      q_snprintf(s, MAX_STRING_CHARS, "*%d", i);

      sv.cm_models[i] = Cm_Model(s);
    }

    svs.state = SV_LOADING;

    Sv_SpawnEntities(name, props);

    const int32_t num_entities = Sv_CreateBaseline();

    Com_Print("  Loaded map %s, %d entities.\n", sv.name, num_entities);
  }

  q_snprintf(sv.config_strings[CS_BSP_SIZE], MAX_STRING_CHARS, "%" PRId64, bsp_size);
}

/**
 * @brief Entry point for spawning a new server or changing maps / demos. Brings any
 * connected clients along for the ride by broadcasting a reconnect before
 * clearing state. Special effort is made to ensure that a locally connected
 * client sees the reconnect message immediately.
 */
void Sv_InitServer(const char *name, const cm_entity_t *props, sv_state_t state) {
  extern void Cl_Disconnect(void);

  Com_Debug(DEBUG_SERVER, "Sv_InitServer: %s (%d)\n", name, state);

  Cbuf_CopyToDefer();

  // inform any connected clients to reconnect to us
  Sv_ShutdownMessage("Server restarting...\n", true);

  // disconnect any local client, they'll immediately reconnect
  Cl_Disconnect();

  // clear the sv_server_t structure
  Sv_ClearState();

  Com_Print("Server initialization...\n");

  Net_Config(NS_UDP_SERVER, true);

  Mem_InitBuffer(&sv.multicast, sv.multicast_buffer, sizeof(sv.multicast_buffer));

  // initialize entities, reloading the game module if necessary
  Sv_InitEntities(state);

  // load the map or demo and related media
  Sv_LoadMedia(name, props, state);
  svs.state = state;

  Com_Print("Server initialized\n");
  Com_InitSubsystem(QUETOO_SERVER);
}

/**
 * @brief Called when the game is shutting down.
 */
void Sv_ShutdownServer(const char *msg) {

  Com_Debug(DEBUG_SERVER, "Sv_ShutdownServer: %s\n", msg);

  Com_Print("Server shutdown...\n");

  Sv_ShutdownMessage(msg, false);

  Sv_ShutdownGame();

  Sv_ShutdownClients();

  Sv_ShutdownMasters();

  Sv_ClearState();

  Net_Config(NS_UDP_SERVER, false);

  Com_Print("Server down\n");

  svs.state = SV_UNINITIALIZED;
}
