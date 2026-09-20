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
 * @brief Searches `sv.`configStrings` from the specified start, searching for the
 * desired name. If not found, the name can be optionally created and sent to
 * all connected clients. This allows the game to lazily load assets.
 */
static int32_t Sv_FindIndex(const char *name, int32_t start, int32_t max, bool create) {
  int32_t i;

  if (!name || !name[0]) {
    return 0;
  }

  for (i = 0; i < max && sv.configStrings[start + i][0]; i++)
    if (!q_strcmp(sv.configStrings[start + i], name)) {
      return i;
    }

  if (!create) {
    return 0;
  }

  if (i == max) {
    Com_Warn("Max index for %s\n", name);
    return 0;
  }

  q_strlcpy(sv.configStrings[start + i], name, sizeof(sv.configStrings[i]));

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

  for (int32_t i = 0; i < sv_maxEntities->integer; i++) {

    GameEntity *ent = sv.entities[i].gent;

    if (!ent || !ent->inUse) {
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

  Mem_ClearBuffer(&netMessage);

  if (msg) { // send message
    Net_WriteByte(&netMessage, SV_CMD_PRINT);
    Net_WriteByte(&netMessage, PRINT_HIGH);
    Net_WriteString(&netMessage, msg);
  }

  if (reconnect) { // send reconnect
    Net_WriteByte(&netMessage, SV_CMD_RECONNECT);
  } else { // or just disconnect
    Net_WriteByte(&netMessage, SV_CMD_DISCONNECT);
  }

  ServerClient *cl = svs.clients;
  for (int32_t i = 0; i < sv_maxClients->integer; i++, cl++)
    if (cl->state >= SV_CLIENT_CONNECTED) {
      Netchan_Transmit(&cl->netChan, netMessage.data, netMessage.size);
    }
}

/**
 * @brief Wipes the `Server` structure after freeing any references it holds.
 */
static void Sv_ClearState(void) {

  if (svs.state == SV_UNINITIALIZED) {
    return;
  }

  Sv_FreeDemo();

  memset(&sv, 0, sizeof(sv));
  Com_QuitSubsystem(QUETOO_SERVER);

  svs.nextHeartbeat = 0;
}

/**
 * @brief Applies any pending variable changes and clamps ones we really care about.
 */
static void Sv_UpdateLatchedVars(void) {

  Cvar_UpdateLatched();

  sv_maxClients->integer = Clampf(sv_maxClients->integer, 1, MAX_CLIENTS);
  sv_maxEntities->integer = Clampf(sv_maxEntities->integer, MAX_CLIENTS + 1, MAX_ENTITIES);
}

/**
 * @brief Allocates the client array for the current server session.
 */
static void Sv_InitClients(void) {

  svs.clients = Mem_TagMalloc(sizeof(ServerClient) * sv_maxClients->integer, MEM_TAG_SERVER);

  for (int32_t i = 0; i < sv_maxClients->integer; i++) {
    svs.clients[i].gclient = svs.game->clients[i];
  }
}

/**
 * @brief Allocates the entity state ring buffer used for delta compression.
 */
static void Sv_InitEntityState(void) {
  svs.numEntityStates = PACKET_BACKUP * MAX_ENTITIES;
  svs.entityStates = Mem_TagMalloc(sizeof(EntityState) * svs.numEntityStates, MEM_TAG_SERVER);
}

/**
 * @brief Gracefully frees all resources allocated to `svs.clients`.
 */
static void Sv_ShutdownClients(void) {

  if (svs.state == SV_UNINITIALIZED) {
    return;
  }

  ServerClient *cl = svs.clients;
  for (int32_t i = 0; i < sv_maxClients->integer; i++, cl++) {
    Sv_HttpClientDisconnect(&cl->http);
  }

  Mem_Free(svs.clients);
  svs.clients = NULL;

  Mem_Free(svs.entityStates);
  svs.entityStates = NULL;
}

/**
 * @brief Resets all spawned clients back to the connected state for a map change.
 */
static void Sv_ReconnectClients(void) {

  for (int32_t i = 0; i < sv_maxClients->integer; i++) {

    // re-bind the game client pointer (may have been cleared on bot disconnect)
    svs.clients[i].gclient = svs.game->clients[i];

    // reset state of spawned clients back to connected
    if (svs.clients[i].state > SV_CLIENT_CONNECTED) {
      svs.clients[i].state = SV_CLIENT_CONNECTED;
    }

    // invalidate last frame to force a baseline
    svs.clients[i].lastFrame = -1;
    svs.clients[i].lastMessage = quetoo.ticks;

    // and discard the previous map's latency samples, which this map's frames do not answer
    // for, along with the frames themselves: their send times outlive the frame numbering that
    // reaches back for them, and would otherwise answer an early acknowledgement on the new map
    memset(svs.clients[i].frames, 0, sizeof(svs.clients[i].frames));
    svs.clients[i].frameLatencyIndex = 0;
    svs.clients[i].frameLatencyCount = 0;
  }
}

/**
 * @brief Reloads `svs.clients`, `svs.client_entities`, the game programs, etc. Because
 * we must allocate clients and edicts based on sizes the game module requests,
 * we refresh the game module.
 */
static void Sv_InitEntities(ServerState state) {

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

  svs.spawnCount++;
}

/**
 * @brief Loads the map or demo file and populates the server-controlled "config
 * strings."  We hand off the entity string to the game module, which will
 * load the rest.
 */
static void Sv_LoadMedia(const char *name, const CmEntity *props, ServerState state) {

  strcpy(sv.name, name);
  strcpy(sv.configStrings[CS_MESSAGE], name);

  if (state == SV_ACTIVE_DEMO) { // loading a demo
    Cvar_ForceSetString(sv_map->name, "");

    svs.spawnCount = 0;

    Sv_LoadDemo();

    Com_Print("  Loaded demo %s.\n", sv.name);
  } else { // loading a map
    Cvar_ForceSetString(sv_map->name, sv.name);

    q_snprintf(sv.configStrings[CS_BSP], MAX_STRING_CHARS, "maps/%s.bsp", sv.name);

    sv.cmModels[0] = Cm_LoadBspModel(sv.configStrings[CS_BSP], NULL);

    // advertise the bsp we actually loaded, so that a client can prove it loaded
    // the same one. Hashing the file rather than trusting our own manifest: a
    // stale .mf would otherwise have us reject correct clients
    if (!Cm_HashFile(sv.configStrings[CS_BSP], sv.configStrings[CS_BSP_HASH], MAX_STRING_CHARS)) {
      Com_Error(ERROR_DROP, "Failed to hash %s\n", sv.configStrings[CS_BSP]);
    }

    const char *dir = Fs_RealDir(sv.configStrings[CS_BSP]);
    const size_t dirLen = q_strlen(dir);
    if (dirLen >= 4 && !q_strcmp(dir + dirLen - 4, ".pk3")) {
      q_strlcpy(sv.configStrings[CS_PK3], Basename(dir), MAX_STRING_CHARS);
    } else {
      sv.configStrings[CS_PK3][0] = '\0';
    }

    q_snprintf(sv.configStrings[CS_MANIFEST], MAX_STRING_CHARS, "maps/%s.mf", sv.name);

    for (int32_t i = 0; i < Cm_NumModels(); i++) {

      if (i == MAX_MODELS) {
        Com_Error(ERROR_DROP, "Sub-model count exceeds protocol limits\n");
      }

      char *s = sv.configStrings[CS_MODELS + i];
      q_snprintf(s, MAX_STRING_CHARS, "*%d", i);

      sv.cmModels[i] = Cm_Model(s);
    }

    svs.state = SV_LOADING;

    Sv_SpawnEntities(name, props);

    const int32_t numEntities = Sv_CreateBaseline();

    Com_Print("  Loaded map %s, %d entities.\n", sv.name, numEntities);
  }

}

/**
 * @brief Entry point for spawning a new server or changing maps / demos. Brings any
 * connected clients along for the ride by broadcasting a reconnect before
 * clearing state. Special effort is made to ensure that a locally connected
 * client sees the reconnect message immediately.
 */
void Sv_InitServer(const char *name, const CmEntity *props, ServerState state) {
  extern void Cl_Disconnect(void);

  Com_Debug(DEBUG_SERVER, "Sv_InitServer: %s (%d)\n", name, state);

  // any override belongs to the map change that consumed it; whatever brought us here,
  // it must not survive to decide the next one. A level served from outside the
  // rotation has no position in it to resume from.
  svs.maps.next = -1;

  if (props == NULL) {
    svs.maps.current = -1;
  }

  Cbuf_CopyToDefer();

  // inform any connected clients to reconnect to us
  Sv_ShutdownMessage("Server restarting...\n", true);

  // disconnect any local client, they'll immediately reconnect
  Cl_Disconnect();

  // clear the Server structure
  Sv_ClearState();

  Com_Print("Server initialization...\n");

  Net_Config(NS_UDP_SERVER, true);

  Mem_InitBuffer(&sv.multicast, sv.multicastBuffer, sizeof(sv.multicastBuffer));

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

  Sv_ShutdownMaster();

  Sv_ClearState();

  Net_Config(NS_UDP_SERVER, false);

  Com_Print("Server down\n");

  svs.state = SV_UNINITIALIZED;
}
