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

#include <SDL3/SDL_timer.h>

#include "sv_local.h"
#include "common/sys.h"

#define INSTALLER_UPDATE_INTERVAL (4 * 60 * 60 * 1000u) // 4 hours in milliseconds

ServerStatic svs; // persistent server info
Server sv; // per-level server info

ServerClient *svClient; // current client

Cvar *sv_demoList;
Cvar *sv_enforceTime;
Cvar *sv_guid;
Cvar *sv_hostname;
Cvar *sv_map;
Cvar *sv_mapList;
Cvar *sv_mapListShuffle;
Cvar *sv_maxClients;
Cvar *sv_maxEntities;
Cvar *sv_minClients;
Cvar *sv_master;
Cvar *sv_public;
Cvar *sv_statsUrl;
Cvar *sv_timeout;

/**
 * @brief Called when the player is totally leaving the server, either willingly
 * or unwillingly. This is NOT called if the entire server is quitting
 * or crashing.
 */
void Sv_DropClient(ServerClient *client) {

  Sv_ClearVoiceMutes(client);

  if (client->state > SV_CLIENT_FREE) { // send the disconnect

    GameClient *cl = client->gclient;

    if (!cl->ai) { // bots have no network connection
      Mem_ClearBuffer(&client->netChan.message);
      Net_WriteByte(&client->netChan.message, SV_CMD_DROP);
      Netchan_Transmit(&client->netChan, client->netChan.message.data, client->netChan.message.size);
    }

    if (cl->inUse) { // inform the game module
      svs.game->ClientDisconnect(cl);
    }
  }

  Sv_HttpClientDisconnect(&client->http);

  Mem_ClearBuffer(&client->netChan.message);
  Mem_ClearBuffer(&client->datagram.buffer);

  client->datagram.messages = release(client->datagram.messages);

  GameClient *gclient = client->gclient;
  memset(client, 0, sizeof(*client));

  client->lastFrame = -1;
  client->gclient = gclient;
}

/**
 * @brief Returns a string fit for heartbeats and status replies.
 */
const char *Sv_StatusString(void) {
  static char status[MAX_MSG_SIZE - 16];

  q_snprintf(status, sizeof(status), "%s\n", Cvar_ServerInfo());
  size_t statusLen = q_strlen(status);

  for (int32_t i = 0; i < sv_maxClients->integer; i++) {

    const ServerClient *cl = &svs.clients[i];

    if ((cl->state == SV_CLIENT_CONNECTED || cl->state == SV_CLIENT_ACTIVE) && cl->gclient->inUse) {
      char player[MAX_TOKEN_CHARS];

      char name[sizeof(cl->name)];
      q_strcolorstrip(cl->name, name);

      const int16_t score = cl->state == SV_CLIENT_ACTIVE ? cl->gclient->score : 0;
      const bool isBot = cl->gclient->ai != NULL;

      if (isBot) {
        q_snprintf(player, sizeof(player), "\\score\\%d\\ping\\%u\\name\\%s\\ai\\1\n",
                   score, cl->ping, name);
      } else {
        q_snprintf(player, sizeof(player), "\\score\\%d\\ping\\%u\\name\\%s\n",
                   score, cl->ping, name);
      }

      const size_t playerLen = q_strlen(player);

      if (statusLen + playerLen + 1 >= sizeof(status)) {
        break;
      }

      strcat(status, player);
      statusLen += playerLen;
    }
  }

  return status;
}

/**
 * @brief Responds with all the info that qplug or qspy can see.
 */
static void Sv_Status_f(void) {
  Netchan_OutOfBandPrint(NS_UDP_SERVER, &netFrom, "status\n%s", Sv_StatusString());
}

/**
 * @brief Returns a challenge number that can be used in a subsequent `client_connect`
 * command.
 *
 * We do this to prevent denial of service attacks that flood the server with
 * invalid connection IPs. With a challenge, they must give a valid address.
 */
static void Sv_GetChallenge_f(void) {
  uint16_t i, oldest;
  uint32_t oldestTime;

  oldest = 0;
  oldestTime = UINT32_MAX;

  // see if we already have a challenge for this ip
  for (i = 0; i < MAX_CHALLENGES; i++) {

    if (Net_CompareClientNetaddr(&netFrom, &svs.challenges[i].addr)) {
      break;
    }

    if (svs.challenges[i].time < oldestTime) {
      oldestTime = svs.challenges[i].time;
      oldest = i;
    }
  }

  if (i == MAX_CHALLENGES) { // overwrite the oldest
    svs.challenges[oldest].challenge = Randomu();
    svs.challenges[oldest].addr = netFrom;
    svs.challenges[oldest].time = quetoo.ticks;
    i = oldest;
  }

  // send it back
  Netchan_OutOfBandPrint(NS_UDP_SERVER, &netFrom, "challenge %i", svs.challenges[i].challenge);
}

/**
 * @brief A client connection.
 */
static void Sv_Connect_f(void) {

  Com_Debug(DEBUG_SERVER, "Sv_Connect_f()\n");

  NetAddr *addr = &netFrom;

  const int32_t version = (int32_t) strtol(Cmd_Argv(1), NULL, 0);

  // resolve protocol
  if (version != PROTOCOL_MAJOR) {
    Netchan_OutOfBandPrint(NS_UDP_SERVER, addr, "print\nServer is version %d.\n", PROTOCOL_MAJOR);
    return;
  }

  const uint8_t qport = (uint8_t) strtoul(Cmd_Argv(2), NULL, 0);
  const uint32_t challenge = (uint32_t) strtoul(Cmd_Argv(3), NULL, 0);

  // copy userInfo, leave room for ip stuffing
  char userInfo[MAX_INFO_STRING_STRING];
  q_strlcpy(userInfo, Cmd_Argv(4), sizeof(userInfo) - 25);

  if (*userInfo == '\0') { // catch empty userInfo
    Com_Print("Empty user_info from %s\n", Net_NetaddrToString(addr));
    Netchan_OutOfBandPrint(NS_UDP_SERVER, addr, "print\nConnection refused\n");
    return;
  }

  if (q_strchr(userInfo, '\xFF')) { // catch end of message in string exploit
    Com_Print("Illegal user_info contained xFF from %s\n", Net_NetaddrToString(addr));
    Netchan_OutOfBandPrint(NS_UDP_SERVER, addr, "print\nConnection refused\n");
    return;
  }

  char val[MAX_INFO_STRING_VALUE];

  if (InfoString_Get(userInfo, "ip", val, sizeof(val)) > 0) { // catch spoofed ips
    Com_Print("Illegal user_info contained ip from %s\n", Net_NetaddrToString(addr));
    Netchan_OutOfBandPrint(NS_UDP_SERVER, addr, "print\nConnection refused\n");
    return;
  }

  if (!InfoString_Validate(userInfo)) { // catch otherwise invalid userInfo
    Com_Print("Invalid user_info from %s\n", Net_NetaddrToString(addr));
    Netchan_OutOfBandPrint(NS_UDP_SERVER, addr, "print\nConnection refused\n");
    return;
  }

  // force the ip so the game can filter on it
  InfoString_Set(userInfo, "ip", Net_NetaddrToString(addr));

  // enforce a valid challenge to avoid denial of service attack
  int32_t i;
  for (i = 0; i < MAX_CHALLENGES; i++) {
    if (Net_CompareClientNetaddr(addr, &svs.challenges[i].addr)) {
      if (challenge == svs.challenges[i].challenge) {
        svs.challenges[i].challenge = 0;
        break; // good
      }
      Netchan_OutOfBandPrint(NS_UDP_SERVER, addr, "print\nBad challenge\n");
      return;
    }
  }
  if (i == MAX_CHALLENGES) {
    Com_Print("Connection without challenge from %s\n", Net_NetaddrToString(addr));
    Netchan_OutOfBandPrint(NS_UDP_SERVER, addr, "print\nNo challenge for address\n");
    return;
  }

  // resolve the client slot
  ServerClient *client = NULL;

  // first check for an ungraceful reconnect (client crashed, perhaps)
  ServerClient *cl = svs.clients;
  for (int32_t i = 0; i < sv_maxClients->integer; i++, cl++) {

    if (cl->state == SV_CLIENT_FREE) { // not in use, not interested
      continue;
    }

    const NetChan *ch = &cl->netChan;

    // the base address and either the qport or real port must match
    if (Net_CompareClientNetaddr(addr, &ch->remoteAddress)) {
      if (addr->port == ch->remoteAddress.port || qport == ch->qport) {
        client = cl;
        break;
      }
    }
  }

  // otherwise, treat as a fresh connect to a new slot
  if (!client) {
    ServerClient *cl = svs.clients;
    for (int32_t i = 0; i < sv_maxClients->integer; i++, cl++) {
      if (cl->state == SV_CLIENT_FREE) { // we have a free one
        client = cl;
        break;
      }
    }
  }

  // no free slots, see if there's an AI slot ready to go and boot them.
  if (!client) {
    ServerClient *cl = svs.clients;
    for (int32_t i = 0; i < sv_maxClients->integer; i++, cl++) {
      if (cl->gclient->ai) {
        client = cl;
        svs.game->ClientDisconnect(cl->gclient);
        break;
      }
    }
  }

  if (!client) {
    Netchan_OutOfBandPrint(NS_UDP_SERVER, addr, "print\nServer is full\n");
    Com_Debug(DEBUG_SERVER, "Rejected a connection\n");
    return;
  }

  // give the game a chance to reject this connection or modify the userInfo
  if (!(svs.game->ClientConnect(client->gclient, userInfo))) {
    char rejmsg[MAX_INFO_STRING_VALUE];

    if (InfoString_Get(userInfo, "rejmsg", rejmsg, sizeof(rejmsg)) > 0) {
      Netchan_OutOfBandPrint(NS_UDP_SERVER, addr, "print\n%s\nConnection refused\n", rejmsg);
    } else {
      Netchan_OutOfBandPrint(NS_UDP_SERVER, addr, "print\nConnection refused\n");
    }

    Com_Debug(DEBUG_SERVER, "Game rejected a connection\n");
    return;
  }

  Netchan_Setup(NS_UDP_SERVER, &client->netChan, addr, qport);

  Mem_InitBuffer(&client->datagram.buffer, client->datagram.data, sizeof(client->datagram.data));

  client->lastMessage = quetoo.ticks;

  client->state = SV_CLIENT_CONNECTED;

  // Sv_UserInfoChanged refuses an ip and forces the client's own, so drop ours
  q_strlcpy(client->userInfo, userInfo, sizeof(client->userInfo));
  InfoString_Delete(client->userInfo, "ip");

  if (!Sv_UserInfoChanged(client)) {
    Netchan_OutOfBandPrint(NS_UDP_SERVER, addr, "print\nConnection refused\n");
    return;
  }

  // send the connect packet to the client
  Netchan_OutOfBandPrint(NS_UDP_SERVER, addr, "client_connect");
}

/**
 * @brief Authenticates a remote console request by checking the rcon password.
 * @return True if the password is set and matches, false otherwise.
 */
static bool Sv_RconAuthenticate(void) {

  // a password must be set for rcon to be available
  if (*rconPassword->string == '\0') {
    return false;
  }

  // and of course the passwords must match
  if (q_strcmp(Cmd_Argv(1), rconPassword->string)) {
    return false;
  }

  return true;
}

static char svRconBuffer[MAX_PRINT_MSG];

/**
 * @brief Console appender for remote console.
 */
static void Sv_Rcon_Print(const ConsoleString *str) {

  q_strlcat(svRconBuffer, str->chars, sizeof(svRconBuffer));
}

/**
 * @brief A client issued an rcon command. Shift down the remaining args and
 * redirect all output to the invoking client.
 */
static void Sv_Rcon_f(void) {

  const bool auth = Sv_RconAuthenticate();

  const char *addr = Net_NetaddrToString(&netFrom);

  // first print to the server console
  if (auth) {
    Com_Print("Rcon from %s:\n%s\n", addr, netMessage.data + 4);
  } else {
    Com_Print("Bad rcon from %s:\n%s\n", addr, netMessage.data + 4);
  }

  // then redirect the remaining output back to the client

  Console rcon = { .Append = Sv_Rcon_Print };
  svRconBuffer[0] = '\0';

  Con_AddConsole(&rcon);

  if (auth) {
    char cmd[MAX_STRING_CHARS];
    cmd[0] = '\0';

    for (int32_t i = 2; i < Cmd_Argc(); i++) {
      q_strlcat(cmd, Cmd_Argv(i), sizeof(cmd));
      q_strlcat(cmd, " ", sizeof(cmd));
    }

    Cmd_ExecuteString(cmd);
  } else {
    Com_Print("Bad rconPassword\n");
  }

  Netchan_OutOfBandPrint(NS_UDP_SERVER, &netFrom, "print\n%s", svRconBuffer);

  Con_RemoveConsole(&rcon);
}

/**
 * @brief A connection-less packet has four leading 0xff bytes to distinguish
 * it from a game channel. Clients that are in the game can still send these,
 * and they will be handled here.
 */
static void Sv_ConnectionlessPacket(void) {

  Net_BeginReading(&netMessage);
  Net_ReadLong(&netMessage); // skip the -1 marker

  const char *s = Net_ReadStringLine(&netMessage);

  Cmd_TokenizeString(s);

  const char *c = Cmd_Argv(0);
  const char *a = Net_NetaddrToString(&netFrom);

  Com_Debug(DEBUG_SERVER, "Packet from %s: %s\n", a, c);

  if (!q_strcmp(c, "challenge")) {
    Sv_Challenge(&netFrom, (uint32_t) strtoul(Cmd_Argv(1), NULL, 10));
  } else if (!q_strcmp(c, "status")) {
    Sv_Status_f();
  } else if (!q_strcmp(c, "get_challenge")) {
    Sv_GetChallenge_f();
  } else if (!q_strcmp(c, "connect")) {
    Sv_Connect_f();
  } else if (!q_strcmp(c, "rcon")) {
    Sv_Rcon_f();
  } else {
    Com_Print("Bad connectionless packet from %s:\n%s\n", a, s);
  }
}

/**
 * @brief Updates the "ping" times for all spawned clients.
 * @details @ref Sv_WaitForPackets stamps each acknowledgement at its true arrival time, so the
 * recorded samples are accurate to the millisecond rather than quantized to the frame interval.
 * They are still averaged: this figure reaches every client each frame as `STAT_PING`, and an
 * unsmoothed one would both read as jitter and defeat the delta compression of the player state.
 */
static void Sv_UpdatePings(void) {
  static uint32_t lastUpdateTime;

  if (quetoo.ticks - lastUpdateTime < SV_CLIENT_PING_INTERVAL) {
    return;
  }

  lastUpdateTime = quetoo.ticks;

  for (int32_t i = 0; i < sv_maxClients->integer; i++) {

    ServerClient *cl = &svs.clients[i];

    if (cl->state != SV_CLIENT_ACTIVE) {
      continue;
    }

    uint64_t total = 0;
    for (uint32_t j = 0; j < cl->frameLatencyCount; j++) {
      total += cl->frameLatency[j];
    }

    if (cl->frameLatencyCount) {
      cl->ping = (int32_t) roundf(total / (float) cl->frameLatencyCount);
    } else {
      cl->ping = 0;
    }

    cl->gclient->ping = Clampf(cl->ping, 0, 999);
  }
}

/**
 * @brief Once per second, gives all clients an allotment of 1000 milliseconds
 * for their movement commands which will be decremented as we receive
 * new information from them. If they drift by a significant margin
 * over the next interval, assume they are trying to cheat.
 */
static void Sv_CheckCommandTimes(void) {
  static uint32_t lastCheckTime;

  // see if its time to check the movements
  if (quetoo.ticks - lastCheckTime < CMD_MSEC_CHECK_INTERVAL) {
    return;
  }

  lastCheckTime = quetoo.ticks;

  // inspect each client, ensuring they are reasonably in sync with us
  for (int32_t i = 0; i < sv_maxClients->integer; i++) {
    ServerClient *cl = &svs.clients[i];

    if (cl->state < SV_CLIENT_ACTIVE) {
      continue;
    }

    if (sv_enforceTime->value) { // check them

      if (cl->cmdMsec > CMD_MSEC_ALLOWABLE_DRIFT) { // irregular movement
        cl->cmdMsecErrors++;

        Com_Debug(DEBUG_SERVER, "%s drifted %dms\n", Sv_NetaddrToString(cl), cl->cmdMsec);

        if (cl->cmdMsecErrors >= sv_enforceTime->value) {
          Com_Warn("Too many errors from %s\n", Sv_NetaddrToString(cl));
          Sv_KickClient(cl, "Irregular movement");
          continue;
        }
      } else { // normal movement

        if (cl->cmdMsecErrors) {
          cl->cmdMsecErrors--;
        }
      }
    }

    cl->cmdMsec = 0; // reset for next cycle
  }
}

/**
 * @brief Reads and dispatches all pending network packets from connected clients.
 */
static void Sv_ReadPackets(void) {

  while (Net_ReceiveDatagram(NS_UDP_SERVER, &netFrom, &netMessage)) {

    // check for connectionless packet (0xffffffff) first
    if (*(uint32_t *) netMessage.data == 0xffffffff) {
      Sv_ConnectionlessPacket();
      continue;
    }

    // read the qport out of the message so we can fix up
    // stupid address translating routers
    Net_BeginReading(&netMessage);

    Net_ReadLong(&netMessage); // sequence number
    Net_ReadLong(&netMessage); // sequence number

    const byte qport = Net_ReadByte(&netMessage) & 0xff;

    // check for packets from connected clients
    ServerClient *cl = svs.clients;
    for (int32_t i = 0; i < sv_maxClients->integer; i++, cl++) {

      if (cl->state == SV_CLIENT_FREE) {
        continue;
      }

      if (!Net_CompareClientNetaddr(&netFrom, &cl->netChan.remoteAddress)) {
        continue;
      }

      if (cl->netChan.qport != qport) {
        continue;
      }

      if (cl->netChan.remoteAddress.port != netFrom.port) {
        cl->netChan.remoteAddress.port = netFrom.port;
        Com_Warn("Fixed translated port for %s\n", Net_NetaddrToString(&netFrom));
      }

      // this is a valid, sequenced packet, so process it
      if (Netchan_Process(&cl->netChan, &netMessage)) {
        cl->lastMessage = quetoo.ticks; // nudge timeout
        Sv_ParseClientMessage(cl);
      }

      // we've processed the packet for the correct client, so break
      break;
    }
  }
}

/**
 * @brief Disconnects clients that have exceeded the idle timeout threshold.
 */
static void Sv_CheckTimeouts(void) {

  const uint32_t timeout = 1000 * sv_timeout->value;

  if (timeout > quetoo.ticks) {
    return;
  }

  ServerClient *cl = svs.clients;
  for (int32_t i = 0; i < sv_maxClients->integer; i++, cl++) {

    if (cl->state == SV_CLIENT_FREE) {
      continue;
    }

    uint64_t grace = timeout;
    if (cl->state == SV_CLIENT_CONNECTED) {
      grace += timeout;
    }

    if (quetoo.ticks <= grace) {
      continue;
    }

    const uint32_t whence = (uint32_t) (quetoo.ticks - grace);

    if (cl->lastMessage < whence) {
      Sv_BroadcastPrint(PRINT_MEDIUM, "%s timed out\n", cl->name);
      Sv_DropClient(cl);
    }
  }
}

/**
 * @brief Resets entity flags and other state which should only last one frame.
 */
static void Sv_ResetEntities(void) {

  if (svs.state != SV_ACTIVE_GAME) {
    return;
  }

  for (int32_t i = 0; i < sv_maxEntities->integer; i++) {
    GameEntity *ent = sv.entities[i].gent;
    if (ent) {
      ent->s.event = 0;
      ent->s.eventData = 0;
    }
  }
}

/**
 * @brief Syncs `ServerClient` state with the game module's `GameClient` for bot clients.
 * Called after each game frame to reflect bot connects and disconnects.
 */
static void Sv_SyncGameClients(void) {

  for (int32_t i = 0; i < sv_maxClients->integer; i++) {
    ServerClient *client = &svs.clients[i];
    const GameClient *cl = client->gclient;

    if (client->state == SV_CLIENT_FREE) {
      if (cl->inUse && cl->ai) { // ai client has just connected
        q_strlcpy(client->userInfo, cl->userInfo, sizeof(client->userInfo));
        client->lastMessage = UINT32_MAX; // ai clients never time out
        client->state = SV_CLIENT_ACTIVE;
        if (!Sv_UserInfoChanged(client)) {
          Com_Warn("Rejected user_info from ai client %d\n", i);
        }
      }
    } else {
      if (!cl->inUse) { // ai client has just disconnected
        GameClient *gclient = client->gclient;
        memset(client, 0, sizeof(*client));
        client->lastFrame = -1;
        client->gclient = gclient;
      }
    }
  }
}

/**
 * @brief Updates the game module's time and runs its frame function.
 */
static void Sv_RunGameFrame(void) {

  sv.frameNum++;
  sv.time = sv.frameNum * QUETOO_TICK_MILLIS;

  if (svs.state == SV_ACTIVE_GAME) {
    svs.game->Frame();
    Sv_SyncGameClients();
  }
}

/**
 * @brief Kicks the specified client from the server with an optional message.
 */
void Sv_KickClient(ServerClient *cl, const char *msg) {
  char buf[MAX_STRING_CHARS], name[32];

  if (!cl) {
    return;
  }

  if (cl->state < SV_CLIENT_CONNECTED) {
    return;
  }

  if (*cl->name == '\0') { // force a name to kick
    strcpy(name, "player");
  } else {
    q_strlcpy(name, cl->name, sizeof(name));
  }

  memset(buf, 0, sizeof(buf));

  if (msg && *msg != '\0') {
    q_snprintf(buf, sizeof(buf), ": %s", msg);
  }

  Sv_ClientPrint(cl->gclient, PRINT_HIGH, "You were kicked%s\n", buf);

  Sv_DropClient(cl);

  Sv_BroadcastPrint(PRINT_MEDIUM, "%s was kicked%s\n", name, buf);
}

/**
 * @brief A convenience function for printing out client addresses.
 */
const char *Sv_NetaddrToString(const ServerClient *cl) {
  return Net_NetaddrToString(&cl->netChan.remoteAddress);
}

/**
 * @brief Enforces safe `userInfo` data before passing onto game module.
 * @return False if the client was kicked for its `userInfo`, in which case the slot is free
 * again and the caller MUST NOT touch it further.
 */
bool Sv_UserInfoChanged(ServerClient *cl) {
  char val[MAX_INFO_STRING_VALUE];
  size_t i;

  if (*cl->userInfo == '\0') { // catch empty userInfo
    Com_Print("Empty user_info from %s\n", Sv_NetaddrToString(cl));
    Sv_KickClient(cl, "Bad user info");
    return false;
  }

  if (q_strchr(cl->userInfo, '\xFF')) { // catch end of message exploit
    Com_Print("Illegal user_info contained xFF from %s\n", Sv_NetaddrToString(cl));
    Sv_KickClient(cl, "Bad user info");
    return false;
  }

  if (!InfoString_Validate(cl->userInfo)) { // catch otherwise invalid userInfo
    Com_Print("Invalid user_info from %s\n", Sv_NetaddrToString(cl));
    Sv_KickClient(cl, "Bad user info");
    return false;
  }

  if (InfoString_Get(cl->userInfo, "ip", val, sizeof(val)) > 0) { // catch spoofed ips, as the connect does
    Com_Print("Illegal user_info contained ip from %s\n", Sv_NetaddrToString(cl));
    Sv_KickClient(cl, "Bad user info");
    return false;
  }

  // force the ip so the game can filter on it, as the connect did: a client's
  // update replaces the whole string, and the client never sends one
  InfoString_Set(cl->userInfo, "ip", Sv_NetaddrToString(cl));

  // call game code to allow overrides
  svs.game->ClientUserInfoChanged(cl->gclient, cl->userInfo);

  // name for C code, mask off high bit
  InfoString_Get(cl->userInfo, "name", cl->name, sizeof(cl->name));
  for (i = 0; i < sizeof(cl->name); i++) {
    cl->name[i] &= 127;
  }

  // limit the print messages the client receives
  if (InfoString_Get(cl->userInfo, "messageLevel", val, sizeof(val)) > 0) {
    cl->messageLevel = (int32_t) strtol(val, NULL, 10);
  }

  return true;
}

/**
 * @brief Installer frame callback for dedicated servers. Delays 100 ms and
 * logs state transitions to the console.
 */
int32_t Sv_InstallerFrame(const InstallerStatus *in) {
  static InstallerStatus last;

  if (in->state != last.state || q_strcmp(in->currentFile, last.currentFile)) {
    switch (in->state) {
      case INSTALLER_CHECKING_BIN:
        Com_Print("Checking for updates\u2026\n");
        break;
      case INSTALLER_BIN_AVAILABLE:
        Com_Warn("A new version of Quetoo is available.\n"
                 "Run quetoo-update to install it.\n"
                 "Your server will not be public until you do.\n");
        Cvar_ForceSetInteger("sv_public", 0);
        Installer_Consent(false);
        break;
      case INSTALLER_DOWNLOADING_BIN:
        Com_Print("Downloading update %s\u2026\n", in->currentFile);
        break;
      case INSTALLER_STAGING_BIN:
        Com_Print("Unpacking update\u2026\n");
        break;
      case INSTALLER_BIN_STAGED:
        Com_Print("Update staged; it will be applied when this server exits.\n");
        Installer_Consent(false);
        break;
      case INSTALLER_INSTALLING_DATA:
        Com_Print("Installing game data\u2026\n");
        break;
      case INSTALLER_CHECKING_DATA:
        Com_Print("Checking game data\u2026\n");
        break;
      case INSTALLER_DOWNLOADING_DATA:
        Com_Print("Downloading game data %s\u2026\n", in->currentFile);
        break;
      case INSTALLER_COMMITTING_DATA:
        Com_Print("Committing game data\u2026\n");
        break;
      case INSTALLER_CANCELLED:
        Com_Print("Update cancelled.\n");
        break;
      case INSTALLER_DONE:
        Com_Print("Game data is up to date.\n");
        break;
      case INSTALLER_ERROR:
        Com_Warn("Update failed: %s\n", in->error);
        break;
      default:
        break;
    }

    last = *in;
  }

  Sv_DrawConsole();

  SDL_Delay(100);

  return in->state >= INSTALLER_DONE || sys_signal_received;
}

/**
 * @brief On a dedicated server, sleeps out the remainder of the frame interval while
 * waking on socket activity to read client packets at their true arrival time.
 * @details The simulation only advances on @ref QUETOO_TICK_MILLIS boundaries, but
 * the socket was historically serviced only at those same boundaries -- so a packet
 * could sit in the kernel buffer for most of a tick before being timestamped,
 * quantizing every ping estimate to the frame interval. Blocking on the socket
 * instead of sleeping lets us stamp and ingest each datagram within a main-loop
 * iteration, giving millisecond-resolution ping and answering connectionless queries
 * (e.g. server-browser pings) without frame-boundary latency. Movement commands are
 * command-driven and already processed on receipt, so this shifts only wall-clock
 * timing, not the deterministic simulation result.
 */
static void Sv_WaitForPackets(const uint32_t msec) {

  const uint32_t enter = quetoo.ticks;

  // the tick counter wraps, so spend the budget by elapsed time rather than against a deadline
  // that can land behind us
  uint32_t elapsed = 0;

  while (elapsed < msec) {

    // block until a packet arrives or the budget elapses
    Net_Sleep(msec - elapsed);

    // timestamp and ingest whatever arrived at its true receive time
    quetoo.ticks = (uint32_t) SDL_GetTicks();
    Sv_ReadPackets();

    elapsed = (uint32_t) SDL_GetTicks() - enter;
  }

  quetoo.ticks = enter;
}

/**
 * @brief Main server frame entry point; advances the simulation and services all clients.
 */
void Sv_Frame(const uint32_t msec) {
  static uint32_t frameDelta;

  if (svs.state == SV_UNINITIALIZED) {
    Sv_DrawConsole();
    if (dedicated->value) {
      SDL_Delay(QUETOO_TICK_MILLIS);
    }
    return;
  }

  if (timeDemo->value) { // always run a frame
    frameDelta = QUETOO_TICK_MILLIS;
  } else { // keep simulation time in sync with reality

    frameDelta += msec;

    if (frameDelta < QUETOO_TICK_MILLIS) {
      if (dedicated->value) {
        Sv_WaitForPackets(QUETOO_TICK_MILLIS - frameDelta);
      } else {
        // a listen server is already called once per rendered frame, with the clock freshly
        // read, so it has only to look at the socket rather than block on it
        Sv_ReadPackets();
      }

      return;
    }
  }

  // clamp the frame interval to 4 ticks to prevent physics tunneling under heavy load
  frameDelta = Minf(frameDelta, (uint32_t) (QUETOO_TICK_MILLIS * 4));

  // read any pending packets from clients
  Sv_ReadPackets();

  // check timeouts
  Sv_CheckTimeouts();

  // check command times for attempted cheating
  Sv_CheckCommandTimes();

  // update ping based on the last known frame from all clients
  Sv_UpdatePings();

  // send a heartbeat to the master if needed
  Sv_HeartbeatMaster();

  // let everything in the world think and move
  const uint64_t simStart = SDL_GetTicks();
  int32_t ticksRun = 0;

  while (frameDelta >= QUETOO_TICK_MILLIS) {

    const uint64_t tickStart = SDL_GetTicks();

    // run the simulation
    Sv_RunGameFrame();

    // send the resulting frame to connected clients
    Sv_SendClientPackets();

    // decrement the simulation time
    frameDelta -= QUETOO_TICK_MILLIS;
    ticksRun++;

    const uint32_t tickMs = (uint32_t) (SDL_GetTicks() - tickStart);
    if (tickMs > QUETOO_TICK_MILLIS && sv.frameNum > QUETOO_TICK_RATE) {
      Com_Debug(DEBUG_SERVER, "Slow game tick: %ums (frame %u)\n", tickMs, sv.frameNum);
    }
  }

  const uint32_t simMs = (uint32_t) (SDL_GetTicks() - simStart);
  if ((simMs > 100 || ticksRun > 2) && sv.frameNum > QUETOO_TICK_RATE) {
    Com_Debug(DEBUG_SERVER, "Server frame overrun: %ums wall time, %d ticks\n", simMs, ticksRun);
  }

  // clear entity flags, etc for next frame
  Sv_ResetEntities();

  // service HTTP file downloads
  Sv_HttpThink();

  // redraw the console
  Sv_DrawConsole();
}

/**
 * @brief Initializes server-local console variables.
 */
static void Sv_InitLocal(void) {

  sv_demoList = Cvar_Add("sv_demoList", "", CVAR_SERVER_INFO, "A list of demo names to cycle through");
  sv_enforceTime = Cvar_Add("sv_enforceTime", va("%d", CMD_MSEC_MAX_DRIFT_ERRORS), 0, "Prevents the most blatant form of speed cheating, disable at your own risk");
  sv_hostname = Cvar_Add("sv_hostname", "Quetoo", CVAR_SERVER_INFO | CVAR_ARCHIVE, "The server hostname, visible in the server browser");
  sv_map = Cvar_Add("sv_map", "", CVAR_SERVER_INFO | CVAR_NO_SET, "The name of the current map.");
  sv_mapList = Cvar_Add("sv_mapList", "maps.lst", 0, "The map list filename.");
  sv_mapListShuffle = Cvar_Add("sv_mapListShuffle", "0", 0, "Enables map shuffling.");
  sv_master = Cvar_Add("sv_master", HOST_MASTER, CVAR_NO_SET, "The master server to advertise on, or \"\" to advertise nowhere");
  sv_maxClients = Cvar_Add("sv_maxClients", va("%d", MAX_CLIENTS), CVAR_SERVER_INFO | CVAR_LATCH, "The maximum number of clients the server will allow");
  sv_maxEntities = Cvar_Add("sv_maxEntities", va("%d", MAX_ENTITIES), CVAR_SERVER_INFO | CVAR_LATCH, "The maximum number of entities the server will allow");
  sv_minClients = Cvar_Add("sv_minClients", "0", CVAR_SERVER_INFO, "The minimum number of clients the server will allow");
  sv_public = Cvar_Add("sv_public", "0", CVAR_SERVER_INFO, "Set to 1 to to advertise this server via the master server");
  sv_statsUrl = Cvar_Add("sv_statsUrl", "https://giblets.quetoo.org", CVAR_ARCHIVE, "URL to POST per-match stats to. Requires sv_public 1. Set to 0 to disable.");
  char uuid[37];
  Com_Uuid(uuid, sizeof(uuid));
  sv_guid = Cvar_Add("sv_guid", uuid, CVAR_SERVER_INFO | CVAR_NO_SET,
                     "This server's identity for the lifetime of the process, so that clients "
                     "reaching it by more than one address can tell it is one server");

  sv_timeout = Cvar_Add("sv_timeout", va("%d", SV_TIMEOUT), 0, "The client connection timeout threshold in seconds");

  sv_maxClients->integer = Mini(sv_maxClients->integer, MAX_CLIENTS);
  sv_maxEntities->integer = Mini(sv_maxEntities->integer, MAX_ENTITIES);

  if (dedicated->value) {
    Cvar_SetInteger(sv_public->name, 1);
  }

  // set this so clients and server browsers can see it
  Cvar_Add("sv_protocol", va("%i", PROTOCOL_MAJOR), CVAR_SERVER_INFO | CVAR_NO_SET, NULL);

  Sv_InitVoice();
}

/**
 * @brief Only called at Quetoo startup, not for each game.
 */
void Sv_Init(void) {

  memset(&svs, 0, sizeof(svs));

  Cm_LoadBspModel(NULL, NULL);

  Sv_InitConsole();

  Sv_InitLocal();

  Sv_InitAdmin();

  Sv_InitMaster();

  Sv_InitHttp();

  Sv_InitMapList();

  Sv_InitGame();
}

/**
 * @brief Called when server is shutting down due to error or an explicit `quit`.
 */
void Sv_Shutdown(const char *msg) {

  Sv_ShutdownHttp();

  Sv_ShutdownServer(msg);

  Sv_ShutdownMapList();

  Sv_ShutdownConsole();

  memset(&svs, 0, sizeof(svs));

  Cmd_RemoveAll(CMD_SERVER);

  Mem_FreeTag(MEM_TAG_SERVER);
}
