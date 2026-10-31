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

#if defined(_WIN32)
 #include <winsock2.h>
#endif

#include "cl_local.h"

/**
 * @brief Allocates and prepends a new server info entry for the given network address.
 */
static ClientServerInfo *Cl_AddServer(const NetAddr *addr) {
  ClientServerInfo *s;

  s = (ClientServerInfo *) Mem_TagMalloc(sizeof(*s), MEM_TAG_CLIENT);

  s->addr = *addr;
  Str_Copy(s->hostname, Net_NetaddrToString(&s->addr), sizeof(s->hostname));

  if (!cls.servers) {
    cls.servers = $(alloc(PointerArray), initWithDestroy, Mem_Free);
  }

  $(cls.servers, add, s);

  return s;
}

/**
 * @brief Finds the server info entry matching the given network address.
 */
static NetAddr Cl_ServerNetaddr(const NetAddr *addr) {

  NetAddr a = *addr;

  // every loopback datagram is received stamped with no port, so a listen server has to be
  // keyed that way too, or the entry that asked can never be matched to its own answer
  if (a.type == NA_LOOP) {
    a.port = 0;
  }

  return a;
}

/**
 * @brief Returns the known server for the given address, or `NULL`.
 */
static ClientServerInfo *Cl_ServerForNetaddr(const NetAddr *addr) {

  for (size_t i = 0; i < (cls.servers ? cls.servers->count : 0); i++) {
    ClientServerInfo *s = (ClientServerInfo *) $(cls.servers, get, i);

    if (Net_CompareNetaddr(addr, &s->addr)) {
      return s;
    }
  }

  return NULL;
}

/**
 * @brief Returns the known server that reports the given identity, or `NULL`. An empty identity
 * matches no server.
 * @remarks An entry keeps the identity it last reported even after it stops answering, so a
 * caller that wants a live server checks that it answered its current flurry.
 */
static ClientServerInfo *Cl_ServerForGuid(const char *guid) {

  if (!guid[0]) {
    return NULL;
  }

  for (size_t i = 0; i < (cls.servers ? cls.servers->count : 0); i++) {
    ClientServerInfo *s = (ClientServerInfo *) $(cls.servers, get, i);

    if (!Str_Compare(guid, s->guid)) {
      return s;
    }
  }

  return NULL;
}

/**
 * @brief Discards the server's smoothed ping, so that the next reply starts it again.
 * @remarks The displayed `ping` is kept until a new sample replaces it.
 */
static void Cl_ClearPings(ClientServerInfo *server) {
  server->pingSmoothed = 0;
  server->pingsLost = 0;
}

/**
 * @brief Sends a status request to the server.
 */
static void Cl_SendPing(ClientServerInfo *server) {

  server->pingTime = quetoo.ticks;
  server->pingPending = true;

  NetChan_OutOfBandPrint(NS_UDP_CLIENT, &server->addr, "status");
}

/**
 * @brief Smooths a round-trip sample into the server's ping.
 */
static void Cl_AddPingSample(ClientServerInfo *server, const int32_t sample) {

  if (server->pingSmoothed == 0) {
    server->pingSmoothed = sample;
  } else {
    server->pingSmoothed = (server->pingSmoothed * 3 + sample) / 4;
  }

  server->pingsLost = 0;

  server->ping = server->pingSmoothed;
}

/**
 * @brief True if the server has a ping that the server browser shows as answered.
 */
static bool Cl_PingAnswered(const ClientServerInfo *server) {
  return server->ping > 0 && server->ping < 999;
}

/**
 * @brief Notifies the user interface that a server changed.
 * @param code `NOTIFICATION_SERVER_PARSED` if the list, or the order it sorts in, may have
 * changed, or `NOTIFICATION_SERVER_PINGED` if only the server's values changed.
 */
static void Cl_NotifyServer(const ClientServerInfo *server, const ClientNotification code) {

  SDL_PushEvent(&(SDL_Event) {
    .user.type = MVC_NOTIFICATION_EVENT,
    .user.code = code,
    .user.data1 = (void *) server
  });
}

/**
 * @brief Starts or stops polling the known servers, while the server browser is open.
 */
void Cl_PollServers(bool poll) {

  if (poll && !cls.pollServers) {
    for (size_t i = 0; i < (cls.servers ? cls.servers->count : 0); i++) {
      Cl_ClearPings((ClientServerInfo *) $(cls.servers, get, i));
    }
  }

  cls.pollServers = poll;
}

/**
 * @brief Counts unanswered status requests as lost and, while polling, sends each server its
 * next request.
 * @remarks A status reply carries no request identifier, so one request at most is in flight per
 * server. That way each reply is matched to the request sent at `pingTime`.
 * @remarks Polling also requires the menus to have the keys. A level load announces the menus'
 * view controllers as appearing, and the switch to the game that follows does not announce them
 * as disappearing.
 */
void Cl_SendServerPings(void) {

  const bool poll = cls.pollServers && cls.keyState.dest == KEY_UI;

  for (size_t i = 0; i < (cls.servers ? cls.servers->count : 0); i++) {
    ClientServerInfo *s = (ClientServerInfo *) $(cls.servers, get, i);

    if (s->pingPending) {
      if (quetoo.ticks - s->pingTime < SERVER_PING_INTERVAL) {
        continue;
      }

      s->pingPending = false;

      if (++s->pingsLost == SERVER_PING_LOST) {
        Cl_ClearPings(s);
        s->ping = 999;
        Cl_NotifyServer(s, NOTIFICATION_SERVER_PARSED);
      }
    }

    if (poll && quetoo.ticks - s->pingTime >= SERVER_PING_INTERVAL) {
      Cl_SendPing(s);
    }
  }
}

/**
 * @brief Frees the list of known servers and clears the pointer.
 */
void Cl_FreeServers(void) {
  cls.servers = release(cls.servers);
}

/**
 * @brief Drops any other known entry for the same server reached a different way.
 * @details A server broadcasting on the LAN and also listed by the master
 * answers under two different addresses, so it shows twice. `Cl_ServerForNetaddr`
 * already merges entries that share one address; this catches what that
 * cannot - one server reported under two. `server` itself is never dropped: it is
 * the one that just proved it is reachable and answering right now, so any
 * duplicate loses to it rather than the reverse.
 * @remarks Matching is on the identity the server reports, not on how it looks. A server
 * that reports none is left alone rather than guessed at.
 * @return True if a duplicate was dropped.
 */
static bool Cl_MergeDuplicateServers(const ClientServerInfo *server) {

  for (size_t i = 0; i < (cls.servers ? cls.servers->count : 0); i++) {
    const ClientServerInfo *other = (ClientServerInfo *) $(cls.servers, get, i);

    if (other == server || Net_CompareNetaddr(&other->addr, &server->addr)) {
      continue;
    }

    if (!server->guid[0] || Str_Compare(other->guid, server->guid)) {
      continue;
    }

    $(cls.servers, removeAt, i);
    return true;
  }

  return false;
}

/**
 * @brief Parses a server status response and updates or creates the matching server entry.
 */
void Cl_ParseServerInfo(void) {
  char string[MAX_MSG_SIZE];

  const size_t length = netMessage.read < netMessage.size
                        ? Minz(netMessage.size - netMessage.read, sizeof(string) - 1)
                        : 0;
  Net_ReadData(&netMessage, string, length);
  string[length] = '\0';

  Com_Debug(DEBUG_CLIENT, "Status from %s: %" PRIuPTR " bytes\n",
            Net_NetaddrToString(&netFrom), (uintptr_t) length);

  // First line is the server infostring; subsequent lines are player entries.
  char *playerStart = Str_FindChar(string, '\n');
  if (playerStart) {
    *playerStart++ = '\0';
  }

  ClientServerInfo *server = Cl_ServerForNetaddr(&netFrom);
  ClientNotification code = NOTIFICATION_SERVER_PINGED;

  char hostname[sizeof(server->hostname)];
  char name[sizeof(server->name)];
  char gameplay[sizeof(server->gameplay)];
  char movement[sizeof(server->movement)];
  char guid[sizeof(server->guid)];
  char game[sizeof(server->game)];

  InfoString_Get(string, "sv_hostname", hostname, sizeof(hostname));
  InfoString_Get(string, "sv_map", name, sizeof(name));
  InfoString_Get(string, "sv_guid", guid, sizeof(guid));
  InfoString_Get(string, "gameName", game, sizeof(game));

  if (!server) { // unknown server, assumed response to broadcast

    const ClientServerInfo *known = Cl_ServerForGuid(guid);
    if (known && known->pingSmoothed > 0) {
      Com_Debug(DEBUG_CLIENT, "Status from %s ignored: answering under another address\n",
                Net_NetaddrToString(&netFrom));
      return;
    }

    server = Cl_AddServer(&netFrom);

    server->source = SERVER_SOURCE_BCAST;

    if (quetoo.ticks - cls.broadcastTime < SERVER_PING_INTERVAL) {
      server->pingTime = cls.broadcastTime;
      server->pingPending = true;
    }

    code = NOTIFICATION_SERVER_PARSED;
  }

  const bool answered = server->pingPending;
  server->pingPending = false;

  const bool wasAnswered = Cl_PingAnswered(server);

  // the mode keys are what the level resolved to, and fall back to what was asked for
  if (InfoString_Get(string, "g_gameplayMode", gameplay, sizeof(gameplay)) <= 0) {
    InfoString_Get(string, "g_gameplay", gameplay, sizeof(gameplay));
  }

  if (InfoString_Get(string, "g_movementMode", movement, sizeof(movement)) <= 0) {
    InfoString_Get(string, "g_movement", movement, sizeof(movement));
  }

  char val[MAX_INFO_STRING_VALUE];
  InfoString_Get(string, "sv_maxClients", val, sizeof(val));
  const int32_t maxClients = atoi(val);

  if (hostname[0] && name[0]) {
    Str_Copy(server->hostname, hostname, sizeof(server->hostname));
    Str_Copy(server->name, name, sizeof(server->name));
    Str_Copy(server->guid, guid, sizeof(server->guid));
    Str_Copy(server->gameplay, gameplay, sizeof(server->gameplay));
    Str_Copy(server->movement, movement, sizeof(server->movement));
    Str_Copy(server->game, game, sizeof(server->game));
    server->maxClients = maxClients;

    server->clients = 0;
    server->bots = 0;

    const char *line = playerStart;
    while (line && *line) {
      const char *end = Str_FindChar(line, '\n');
      if (!end) {
        break;
      }

      char player[MAX_TOKEN_CHARS];
      Str_Copy(player, line, Minz((size_t) (end - line) + 1, sizeof(player)));

      if (player[0]) {
        server->clients++;
        InfoString_Get(player, "ai", val, sizeof(val));
        if (atoi(val)) {
          server->bots++;
        }
      }

      line = end + 1;
    }

    if (answered) {
      Cl_AddPingSample(server, Clampf(quetoo.ticks - server->pingTime, 1u, 999u));
    }

    if (Cl_PingAnswered(server) != wasAnswered) {
      code = NOTIFICATION_SERVER_PARSED;
    }

    server->error[0] = '\0';

    Com_Debug(DEBUG_CLIENT, "Status from %s: \"%s\" map %s, gameplay %s, %d/%d clients (%d bots), %dms\n",
              Net_NetaddrToString(&netFrom), server->hostname, server->name, server->gameplay,
              server->clients, server->maxClients, server->bots, server->ping);

    if (Cl_MergeDuplicateServers(server)) {
      code = NOTIFICATION_SERVER_PARSED;
    }

  } else {
    code = NOTIFICATION_SERVER_PARSED;

    server->hostname[0] = '\0';
    server->name[0] = '\0';
    server->gameplay[0] = '\0';
    server->movement[0] = '\0';
    server->game[0] = '\0';

    server->clients = 0;
    server->maxClients = 0;
    server->bots = 0;

    Str_Format(server->error, sizeof(server->error), "Invalid response from %s\n", Net_NetaddrToString(&server->addr));

    Com_Debug(DEBUG_CLIENT, "Status from %s rejected: sv_hostname %s, sv_map %s\n",
              Net_NetaddrToString(&netFrom),
              hostname[0] ? "present" : "MISSING", name[0] ? "present" : "MISSING");
  }

  Cl_NotifyServer(server, code);
}

/**
 * @brief Handles the `ping` console command, pinging a specific server address.
 */
void Cl_Ping_f(void) {
  NetAddr addr;
  ClientServerInfo *server;

  if (Cmd_Argc() != 2) {
    Com_Print("Usage: %s <address>\n", Cmd_Argv(0));
    return;
  }

  server = NULL;

  if (!Net_StringToNetaddr(Cmd_Argv(1), &addr)) {
    Com_Print("Invalid address\n");
    return;
  }

  if (!addr.port) { // use default
    addr.port = (uint16_t) htons(PORT_SERVER);
  }

  server = Cl_ServerForNetaddr(&addr);

  if (!server) { // add it
    server = Cl_AddServer(&addr);
    server->source = SERVER_SOURCE_USER;
  }

  server->ping = 999;

  Com_Print("Pinging %s\n", Net_NetaddrToString(&server->addr));

  Cl_ClearPings(server);

  if (!server->pingPending) {
    Cl_SendPing(server);
  }
}

/**
 * @brief Queries the status of the server at `addr`, adding it to the list if it is unknown, so
 * that its hostname is at hand for the connection in progress. A local server is not listed.
 */
void Cl_QueryServer(const NetAddr *addr) {

  const NetAddr to = Cl_ServerNetaddr(addr);

  ClientServerInfo *server = Cl_ServerForNetaddr(&to);

  if (!server) {
    server = Cl_AddServer(&to);
    server->source = SERVER_SOURCE_USER;
  }

  if (!server->pingPending) {
    Cl_SendPing(server);
  }
}

/**
 * @brief Returns the known status of the server being connected to or played on, or `NULL` if
 * there is none or it has never been queried.
 */
const ClientServerInfo *Cl_ServerInfo(void) {

  if (cls.server.address[0] == '\0') {
    return NULL;
  }

  const NetAddr addr = Cl_ServerNetaddr(&cls.server.addr);

  return Cl_ServerForNetaddr(&addr);
}

/**
 * @brief Sends a LAN broadcast to discover servers on the local network.
 * @remarks The broadcast is the next request of each known LAN server with none in flight.
 */
static void Cl_SendBroadcast(void) {
  for (size_t i = 0; i < (cls.servers ? cls.servers->count : 0); i++) {
    ClientServerInfo *s = (ClientServerInfo *) $(cls.servers, get, i);

    if (s->source == SERVER_SOURCE_BCAST && !s->pingPending) {
      s->pingTime = quetoo.ticks;
      s->pingPending = true;
    }
  }

  NetAddr addr;
  memset(&addr, 0, sizeof(addr));

  addr.type = NA_BROADCAST;
  addr.port = htons(PORT_SERVER);

  Com_Debug(DEBUG_CLIENT, "Broadcasting status to %s\n", Net_NetaddrToString(&addr));

  NetChan_OutOfBandPrint(NS_UDP_CLIENT, &addr, "status");

  cls.broadcastTime = quetoo.ticks;
}

/**
 * @brief Handles the `servers` console command, querying the master server and sending a LAN broadcast.
 */
void Cl_Servers_f(void) {
  NetAddr addr;

  if (!Net_StringToNetaddr(HOST_MASTER, &addr)) {
    Com_Print("Failed to resolve %s\n", HOST_MASTER);
    return;
  }

  Com_Print("Refreshing servers\n");

  addr.type = NA_DATAGRAM;
  addr.port = htons(PORT_MASTER);

  Com_Debug(DEBUG_CLIENT, "Requesting servers from %s (%s) for protocol %d\n",
            HOST_MASTER, Net_NetaddrToString(&addr), PROTOCOL_MAJOR);

  NetChan_OutOfBandPrint(NS_UDP_CLIENT, &addr, "getservers %d", PROTOCOL_MAJOR);

  Cl_SendBroadcast();
}

/**
 * @brief Parses the server list from a master server response and pings each entry.
 */
void Cl_ParseServers(void) {
  ClientServerInfo *server;

  Com_Debug(DEBUG_CLIENT, "Servers list from %s: %" PRIuPTR " bytes\n",
            Net_NetaddrToString(&netFrom), (uintptr_t) netMessage.size);

  if (netMessage.size <= 12) {
    Com_Debug(DEBUG_CLIENT, "Servers list is empty (the master knows of no servers "
              "for protocol %d)\n", PROTOCOL_MAJOR);
    return;
  }

  byte *buffptr = netMessage.data + 12;
  byte *buffend = buffptr + netMessage.size - 12;

  uint32_t parsed = 0;

  // parse the list
  while (buffptr + 1 < buffend) {
    NetAddr addr;
    byte ip[4];

    ip[0] = *buffptr++; // parse the address
    ip[1] = *buffptr++;
    ip[2] = *buffptr++;
    ip[3] = *buffptr++;

    uint16_t port = (*buffptr++) << 8; // and the port
    port += *buffptr++;

    char s[32];
    Str_Format(s, sizeof(s), "%d.%d.%d.%d:%d", ip[0], ip[1], ip[2], ip[3], port);

    Com_Debug(DEBUG_CLIENT, "Parsed %s\n", s);

    if (!Net_StringToNetaddr(s, &addr)) { // make sure it's valid
      Com_Warn("Invalid address: %s\n", s);
      break;
    }

    if (!addr.port) { // 0's mean we're done
      Com_Debug(DEBUG_CLIENT, "Zero port terminates the list after %u entries\n", parsed);
      break;
    }

    server = Cl_ServerForNetaddr(&addr);

    if (!server) {
      server = Cl_AddServer(&addr);
    }

    server->source = SERVER_SOURCE_INTERNET;
    parsed++;
  }

  netMessage.read = netMessage.size;

  // then ping them

  uint32_t queried = 0;

  for (size_t i = 0; i < (cls.servers ? cls.servers->count : 0); i++) {
    server = (ClientServerInfo *) $(cls.servers, get, i);

    if (server->source == SERVER_SOURCE_INTERNET && !server->pingPending) {
      Cl_SendPing(server);
      queried++;
    }
  }

  Com_Debug(DEBUG_CLIENT, "Parsed %u servers, queried %u for status\n", parsed, queried);

  // and inform the user interface

  Cl_NotifyServer(NULL, NOTIFICATION_SERVER_PARSED);
}

/**
 * @brief Handles the `serverList` console command, printing all known servers to the console.
 */
void Cl_ServerList_f(void) {
  char string[256];

  for (size_t i = 0; i < (cls.servers ? cls.servers->count : 0); i++) {
    const ClientServerInfo *s = (const ClientServerInfo *) $(cls.servers, get, i);

    Str_Format(string, sizeof(string), "%-40.40s %-20.20s %-16.16s %-24.24s %02d/%02d %5dms",
               s->hostname, Net_NetaddrToString(&s->addr), s->name, s->gameplay, s->clients,
               s->maxClients, s->ping);
    Com_Print("%s\n", string);
  }
}
