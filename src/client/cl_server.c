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

  if (!cls.servers) { cls.servers = $(alloc(List), init); cls.servers->destroy = (ListDestroyFunc) Mem_Free; } $(cls.servers, prependElement, s);

  return s;
}

/**
 * @brief Finds the server info entry matching the given network address.
 */
static cl_server_info_t *Cl_ServerForNetaddr(const net_addr_t *addr) {
  const ListNode *e = cls.servers ? cls.servers->head : NULL;

  while (e) {
    cl_server_info_t *s = (cl_server_info_t *) e->element;

    if (Net_CompareNetaddr(addr, &s->addr)) {
      return s;
    }

    e = e->next;
  }

  return NULL;
}

/**
 * @brief Frees the list of known servers and clears the pointer.
 */
void Cl_FreeServers(void) {

  if (cls.servers) { $(cls.servers, removeAll); release(cls.servers); }

  cls.servers = NULL;
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

  q_strlcpy(string, Net_ReadString(&net_message), sizeof(string));

  // First line is the server infostring; subsequent lines are player entries.
  char *player_start = q_strchr(string, '\n');
  if (player_start) {
    *player_start++ = '\0';
  }

  char hostname[sizeof(server->hostname)];
  char name[sizeof(server->name)];
  char gameplay[sizeof(server->gameplay)];

  q_strlcpy(hostname, InfoString_Get(string, "sv_hostname"), sizeof(hostname));
  q_strlcpy(name, InfoString_Get(string, "sv_map"), sizeof(name));
  q_strlcpy(gameplay, InfoString_Get(string, "g_gameplay"), sizeof(gameplay));
  const int32_t max_clients = atoi(InfoString_Get(string, "sv_max_clients"));

  if (hostname[0] && name[0]) {
    q_strlcpy(server->hostname, hostname, sizeof(server->hostname));
    q_strlcpy(server->name, name, sizeof(server->name));
    q_strlcpy(server->gameplay, gameplay, sizeof(server->gameplay));
    server->max_clients = max_clients;

    server->clients = 0;
    server->bots = 0;

    const char *line = player_start;
    while (line && *line) {
      const char *end = q_strchr(line, '\n');

      char player[MAX_TOKEN_CHARS];
      const size_t len = end ? (size_t) (end - line) : q_strlen(line);
      q_strlcpy(player, line, Mini((int32_t) len + 1, sizeof(player)));

      if (player[0]) {
        server->clients++;
        if (atoi(InfoString_Get(player, "ai"))) {
          server->bots++;
        }
      }

      line = end ? end + 1 : NULL;
    }

    server->ping = Clampf(quetoo.ticks - server->ping_time, 1u, 999u);
    server->error[0] = '\0';

  } else {
    server->hostname[0] = '\0';
    server->name[0] = '\0';
    server->gameplay[0] = '\0';

    server->clients = 0;
    server->max_clients = 0;
    server->bots = 0;

    q_snprintf(server->error, sizeof(server->error), "Invalid response from %s\n", Net_NetaddrToString(&server->addr));
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
  const ListNode *e = cls.servers ? cls.servers->head : NULL;


  while (e) { // update old ping times
    cl_server_info_t *s = (cl_server_info_t *) e->element;

    if (s->source == SERVER_SOURCE_BCAST) {
      s->ping_time = quetoo.ticks;
      s->ping = 999;
    }

    e = e->next;
  }

  net_addr_t addr;
  memset(&addr, 0, sizeof(addr));

  addr.type = NA_BROADCAST;
  addr.port = htons(PORT_SERVER);

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

  Netchan_OutOfBandPrint(NS_UDP_CLIENT, &addr, "getservers %d", PROTOCOL_MAJOR);

  Cl_SendBroadcast();
}

/**
 * @brief Parses the server list from a master server response and pings each entry.
 */
void Cl_ParseServers(void) {
  cl_server_info_t *server;

  if (net_message.size <= 12) {
    return;
  }

  byte *buffptr = net_message.data + 12;
  byte *buffend = buffptr + net_message.size - 12;

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
      break;
    }

    server = Cl_ServerForNetaddr(&addr);

    if (!server) {
      server = Cl_AddServer(&addr);
    }

    server->source = SERVER_SOURCE_INTERNET;
  }

  net_message.read = net_message.size;

  // then ping them

  const ListNode *e = cls.servers ? cls.servers->head : NULL;

  while (e) {
    server = (cl_server_info_t *) e->element;

    if (server->source == SERVER_SOURCE_INTERNET) {
      server->ping_time = quetoo.ticks;
      server->ping = 0;

      Netchan_OutOfBandPrint(NS_UDP_CLIENT, &server->addr, "status");
    }

    e = e->next;
  }

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

  const ListNode *e = cls.servers ? cls.servers->head : NULL;

  while (e) {
    const cl_server_info_t *s = (const cl_server_info_t *) e->element;

    q_snprintf(string, sizeof(string), "%-40.40s %-20.20s %-16.16s %-24.24s %02d/%02d %5dms",
               s->hostname, Net_NetaddrToString(&s->addr), s->name, s->gameplay, s->clients,
               s->max_clients, s->ping);
    Com_Print("%s\n", string);
    e = e->next;
  }
}
