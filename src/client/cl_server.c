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

#if defined(_WIN32)
 #include <winsock2.h>
#endif

#include "cl_local.h"

/**
 * @brief Allocates and prepends a new server info entry for the given network address.
 */
static cl_server_info_t *Cl_AddServer(const net_addr_t *addr) {
  cl_server_info_t *s;

  s = (cl_server_info_t *) Mem_TagMalloc(sizeof(*s), MEM_TAG_CLIENT);

  s->addr = *addr;
  q_strlcpy(s->hostname, Net_NetaddrToString(&s->addr), sizeof(s->hostname));

  if (!cls.servers) {
    cls.servers = $(alloc(PointerArray), initWithDestroy, Mem_Free);
  }

  $(cls.servers, add, s);

  return s;
}

/**
 * @brief Finds the server info entry matching the given network address.
 */
static cl_server_info_t *Cl_ServerForNetaddr(const net_addr_t *addr) {

  for (size_t i = 0; i < (cls.servers ? cls.servers->count : 0); i++) {
    cl_server_info_t *s = (cl_server_info_t *) $(cls.servers, get, i);

    if (Net_CompareNetaddr(addr, &s->addr)) {
      return s;
    }
  }

  return NULL;
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
 * cannot - the same hostname, map and occupancy reported under two. `server`
 * itself is never dropped: it is the one that just proved it is reachable and
 * answering right now, so any duplicate loses to it rather than the reverse.
 */
static void Cl_MergeDuplicateServers(const cl_server_info_t *server) {

  for (size_t i = 0; i < (cls.servers ? cls.servers->count : 0); i++) {
    const cl_server_info_t *other = (cl_server_info_t *) $(cls.servers, get, i);

    if (other == server || Net_CompareNetaddr(&other->addr, &server->addr)) {
      continue;
    }

    if (!server->hostname[0] || q_strcmp(other->hostname, server->hostname) ||
        q_strcmp(other->name, server->name) ||
        other->max_clients != server->max_clients ||
        other->clients != server->clients) {
      continue;
    }

    $(cls.servers, removeAt, i);
    break;
  }
}

/**
 * @brief Parses a server status response and updates or creates the matching server entry.
 */
void Cl_ParseServerInfo(void) {
  char string[MAX_MSG_SIZE];

  cl_server_info_t *server = Cl_ServerForNetaddr(&net_from);
  if (!server) { // unknown server, assumed response to broadcast

    server = Cl_AddServer(&net_from);

    server->source = SERVER_SOURCE_BCAST;
    server->ping_time = cls.broadcast_time;
  }

  const size_t length = net_message.read < net_message.size
                        ? Minz(net_message.size - net_message.read, sizeof(string) - 1)
                        : 0;
  Net_ReadData(&net_message, string, length);
  string[length] = '\0';

  Com_Debug(DEBUG_CLIENT, "Status from %s: %" PRIuPTR " bytes\n",
            Net_NetaddrToString(&net_from), (uintptr_t) length);

  // First line is the server infostring; subsequent lines are player entries.
  char *player_start = q_strchr(string, '\n');
  if (player_start) {
    *player_start++ = '\0';
  }

  char hostname[sizeof(server->hostname)];
  char name[sizeof(server->name)];
  char gameplay[sizeof(server->gameplay)];
  char movement[sizeof(server->movement)];

  q_strlcpy(hostname, InfoString_Get(string, "sv_hostname"), sizeof(hostname));
  q_strlcpy(name, InfoString_Get(string, "sv_map"), sizeof(name));
  const char *mode = InfoString_Get(string, "g_gameplay_mode");
  q_strlcpy(gameplay, *mode ? mode : InfoString_Get(string, "g_gameplay"), sizeof(gameplay));
  const char *move = InfoString_Get(string, "g_movement_mode");
  q_strlcpy(movement, *move ? move : InfoString_Get(string, "g_movement"), sizeof(movement));
  const int32_t max_clients = atoi(InfoString_Get(string, "sv_max_clients"));

  if (hostname[0] && name[0]) {
    q_strlcpy(server->hostname, hostname, sizeof(server->hostname));
    q_strlcpy(server->name, name, sizeof(server->name));
    q_strlcpy(server->gameplay, gameplay, sizeof(server->gameplay));
    q_strlcpy(server->movement, movement, sizeof(server->movement));
    server->max_clients = max_clients;

    server->clients = 0;
    server->bots = 0;

    const char *line = player_start;
    while (line && *line) {
      const char *end = q_strchr(line, '\n');
      if (!end) {
        break;
      }

      char player[MAX_TOKEN_CHARS];
      q_strlcpy(player, line, Minz((size_t) (end - line) + 1, sizeof(player)));

      if (player[0]) {
        server->clients++;
        if (atoi(InfoString_Get(player, "ai"))) {
          server->bots++;
        }
      }

      line = end + 1;
    }

    server->ping = Clampf(quetoo.ticks - server->ping_time, 1u, 999u);
    server->error[0] = '\0';

    Com_Debug(DEBUG_CLIENT, "Status from %s: \"%s\" map %s, gameplay %s, %d/%d clients (%d bots), %dms\n",
              Net_NetaddrToString(&net_from), server->hostname, server->name, server->gameplay,
              server->clients, server->max_clients, server->bots, server->ping);

    Cl_MergeDuplicateServers(server);

  } else {
    server->hostname[0] = '\0';
    server->name[0] = '\0';
    server->gameplay[0] = '\0';
    server->movement[0] = '\0';

    server->clients = 0;
    server->max_clients = 0;
    server->bots = 0;

    q_snprintf(server->error, sizeof(server->error), "Invalid response from %s\n", Net_NetaddrToString(&server->addr));

    Com_Debug(DEBUG_CLIENT, "Status from %s rejected: sv_hostname %s, sv_map %s\n",
              Net_NetaddrToString(&net_from),
              hostname[0] ? "present" : "MISSING", name[0] ? "present" : "MISSING");
  }

  SDL_PushEvent(&(SDL_Event) {
    .user.type = MVC_NOTIFICATION_EVENT,
    .user.code = NOTIFICATION_SERVER_PARSED,
    .user.data1 = server
  });
}

/**
 * @brief Handles the `ping` console command, pinging a specific server address.
 */
void Cl_Ping_f(void) {
  net_addr_t addr;
  cl_server_info_t *server;

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

  server->ping_time = quetoo.ticks;
  server->ping = 999;

  Com_Print("Pinging %s\n", Net_NetaddrToString(&server->addr));

  Netchan_OutOfBandPrint(NS_UDP_CLIENT, &server->addr, "status");
}

/**
 * @brief Sends a LAN broadcast and resets ping times for all broadcast servers.
 */
static void Cl_SendBroadcast(void) {
  for (size_t i = 0; i < (cls.servers ? cls.servers->count : 0); i++) { // update old ping times
    cl_server_info_t *s = (cl_server_info_t *) $(cls.servers, get, i);

    if (s->source == SERVER_SOURCE_BCAST) {
      s->ping_time = quetoo.ticks;
      s->ping = 999;
    }
  }

  net_addr_t addr;
  memset(&addr, 0, sizeof(addr));

  addr.type = NA_BROADCAST;
  addr.port = htons(PORT_SERVER);

  Com_Debug(DEBUG_CLIENT, "Broadcasting status to %s\n", Net_NetaddrToString(&addr));

  Netchan_OutOfBandPrint(NS_UDP_CLIENT, &addr, "status");

  cls.broadcast_time = quetoo.ticks;
}

/**
 * @brief Handles the `servers` console command, querying the master server and sending a LAN broadcast.
 */
void Cl_Servers_f(void) {
  net_addr_t addr;

  if (!Net_StringToNetaddr(HOST_MASTER, &addr)) {
    Com_Print("Failed to resolve %s\n", HOST_MASTER);
    return;
  }

  Com_Print("Refreshing servers\n");

  addr.type = NA_DATAGRAM;
  addr.port = htons(PORT_MASTER);

  Com_Debug(DEBUG_CLIENT, "Requesting servers from %s (%s) for protocol %d\n",
            HOST_MASTER, Net_NetaddrToString(&addr), PROTOCOL_MAJOR);

  Netchan_OutOfBandPrint(NS_UDP_CLIENT, &addr, "getservers %d", PROTOCOL_MAJOR);

  Cl_SendBroadcast();
}

/**
 * @brief Parses the server list from a master server response and pings each entry.
 */
void Cl_ParseServers(void) {
  cl_server_info_t *server;

  Com_Debug(DEBUG_CLIENT, "Servers list from %s: %" PRIuPTR " bytes\n",
            Net_NetaddrToString(&net_from), (uintptr_t) net_message.size);

  if (net_message.size <= 12) {
    Com_Debug(DEBUG_CLIENT, "Servers list is empty (the master knows of no servers "
              "for protocol %d)\n", PROTOCOL_MAJOR);
    return;
  }

  byte *buffptr = net_message.data + 12;
  byte *buffend = buffptr + net_message.size - 12;

  uint32_t parsed = 0;

  // parse the list
  while (buffptr + 1 < buffend) {
    net_addr_t addr;
    byte ip[4];

    ip[0] = *buffptr++; // parse the address
    ip[1] = *buffptr++;
    ip[2] = *buffptr++;
    ip[3] = *buffptr++;

    uint16_t port = (*buffptr++) << 8; // and the port
    port += *buffptr++;

    char s[32];
    q_snprintf(s, sizeof(s), "%d.%d.%d.%d:%d", ip[0], ip[1], ip[2], ip[3], port);

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

  net_message.read = net_message.size;

  // then ping them

  uint32_t queried = 0;

  for (size_t i = 0; i < (cls.servers ? cls.servers->count : 0); i++) {
    server = (cl_server_info_t *) $(cls.servers, get, i);

    if (server->source == SERVER_SOURCE_INTERNET) {
      server->ping_time = quetoo.ticks;
      server->ping = 0;

      Netchan_OutOfBandPrint(NS_UDP_CLIENT, &server->addr, "status");
      queried++;
    }
  }

  Com_Debug(DEBUG_CLIENT, "Parsed %u servers, queried %u for status\n", parsed, queried);

  // and inform the user interface

  SDL_PushEvent(&(SDL_Event) {
    .user.type = MVC_NOTIFICATION_EVENT,
    .user.code = NOTIFICATION_SERVER_PARSED,
  });
}

/**
 * @brief Handles the `servers_list` console command, printing all known servers to the console.
 */
void Cl_Servers_List_f(void) {
  char string[256];

  for (size_t i = 0; i < (cls.servers ? cls.servers->count : 0); i++) {
    const cl_server_info_t *s = (const cl_server_info_t *) $(cls.servers, get, i);

    q_snprintf(string, sizeof(string), "%-40.40s %-20.20s %-16.16s %-24.24s %02d/%02d %5dms",
               s->hostname, Net_NetaddrToString(&s->addr), s->name, s->gameplay, s->clients,
               s->max_clients, s->ping);
    Com_Print("%s\n", string);
  }
}
