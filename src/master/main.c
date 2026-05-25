/*
 * Copyright(c) 2002 r1ch.net.
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

#include <errno.h>
#include <signal.h>
#include <sys/types.h>

#if defined(_WIN32)

  #include <winsock2.h>
  #include <ws2tcpip.h>

  #include <inttypes.h>
  typedef uint32_t in_addr_t;
  typedef uint16_t in_port_t;

  #undef  EWOULDBLOCK
  #define EWOULDBLOCK  WSAEWOULDBLOCK
  #undef  ECONNREFUSED
  #define ECONNREFUSED WSAECONNREFUSED
  #undef  EINPROGRESS
  #define EINPROGRESS  WSAEINPROGRESS

#else

  #include <arpa/inet.h>
  #include <netinet/in.h>
  #include <sys/select.h>
  #include <sys/socket.h>

#endif

#include "common/common.h"
#include "net/net_http.h"

quetoo_t quetoo;

typedef struct ms_server_s {
  struct sockaddr_in addr;
  uint16_t queued_pings;
  time_t last_heartbeat;
  time_t last_ping;
  bool validated;
  char hostname[256];
  int32_t num_clients;
  int32_t max_clients;
  char players[MAX_CLIENTS][64];
} ms_server_t;

static GList *ms_servers;
static int32_t ms_sock;

static bool verbose;
static bool debug;

static const char *ms_discord_webhook;

/**
 * @brief Extracts the value for the given key from a Quake infostring.
 * @return True if the key was found and the value copied, false otherwise.
 */
static bool Ms_InfoValue(const char *info, const char *key, char *buf, size_t buf_size) {
  char search[256];
  g_snprintf(search, sizeof(search), "\\%s\\", key);

  const char *p = strstr(info, search);
  if (!p) {
    return false;
  }

  p += strlen(search);

  const char *end = strpbrk(p, "\\\n");
  const size_t len = end ? Mini((size_t) (end - p), buf_size - 1) : Mini(strlen(p), buf_size - 1);

  memcpy(buf, p, len);
  buf[len] = '\0';
  return true;
}

/**
 * @brief JSON-escapes `src` into `buf`.
 */
static void Ms_JsonEscape(const char *src, char *buf, size_t buf_size) {
  size_t out = 0;
  for (const char *s = src; *s && out + 2 < buf_size; s++) {
    if (*s == '"' || *s == '\\') {
      if (out + 3 < buf_size) {
        buf[out++] = '\\';
      }
    }
    buf[out++] = *s;
  }
  buf[out] = '\0';
}

/**
 * @brief Posts a Discord webhook notification for a player joining a server.
 */
static void Ms_DiscordNotify(const ms_server_t *server, const char *player_name) {
  if (!ms_discord_webhook) {
    return;
  }

  char escaped_player[128];
  char escaped_host[256];
  Ms_JsonEscape(player_name, escaped_player, sizeof(escaped_player));
  Ms_JsonEscape(server->hostname, escaped_host, sizeof(escaped_host));

  char json[1024];
  g_snprintf(json, sizeof(json),
    "{\"content\": \"\xF0\x9F\x8E\xAE **%s** joined **%s** (%d/%d players)\"}",
    escaped_player, escaped_host, server->num_clients, server->max_clients);

  Net_HttpPostAsync(ms_discord_webhook, json, strlen(json), "application/json", NULL, NULL);
}

/**
 * @brief Parses a status string from a server heartbeat, updates the server's
 * player list, and fires Discord notifications for any new players detected.
 * On first call (num_clients == -1), records current state without notifying.
 */
static void Ms_ParseStatusString(ms_server_t *server, const char *status) {

  char val[256];

  if (Ms_InfoValue(status, "hostname", val, sizeof(val))) {
    g_strlcpy(server->hostname, val, sizeof(server->hostname));
  }

  server->max_clients = 0;
  if (Ms_InfoValue(status, "maxclients", val, sizeof(val))) {
    server->max_clients = atoi(val);
  }

  char new_players[MAX_CLIENTS][64];
  int32_t new_count = 0;

  // player lines begin after the infostring's trailing newline
  const char *line = strchr(status, '\n');
  while (line && new_count < MAX_CLIENTS) {
    line++; // skip the newline
    if (*line == '\0') {
      break;
    }

    int32_t score, ping;
    char name[64] = { 0 };
    if (sscanf(line, "%d %d \"%63[^\"]\"", &score, &ping, name) >= 2 && name[0]) {
      g_strlcpy(new_players[new_count], name, sizeof(new_players[new_count]));
      new_count++;
    }

    line = strchr(line, '\n');
  }

  const bool initialized = (server->num_clients >= 0);

  if (initialized) {
    for (int32_t i = 0; i < new_count; i++) {
      bool found = false;
      for (int32_t j = 0; j < server->num_clients; j++) {
        if (!g_strcmp0(new_players[i], server->players[j])) {
          found = true;
          break;
        }
      }
      if (!found) {
        server->num_clients = new_count; // update before notify so count is current
        Ms_DiscordNotify(server, new_players[i]);
      }
    }
  }

  server->num_clients = new_count;
  for (int32_t i = 0; i < new_count; i++) {
    g_strlcpy(server->players[i], new_players[i], sizeof(server->players[i]));
  }
}

/**
 * @brief Shorthand for printing Internet addresses.
 */
static const char *atos(const struct sockaddr_in *addr) {
  return va("%s:%d", inet_ntoa(addr->sin_addr), ntohs(addr->sin_port));
}

#define stos(s) (atos(&s->addr))

/**
 * @brief Returns the server for the specified address, or `NULL`.
 */
static ms_server_t *Ms_GetServer(struct sockaddr_in *from) {

  GList *s = ms_servers;
  while (s) {
    ms_server_t *server = (ms_server_t *) s->data;

    const struct sockaddr_in *addr = &server->addr;
    if (addr->sin_addr.s_addr == from->sin_addr.s_addr && addr->sin_port == from->sin_port) {
      return server;
    }

    s = s->next;
  }

  return NULL;
}

/**
 * @brief Removes the specified server.
 */
static void Ms_DropServer(ms_server_t *server) {

  ms_servers = g_list_remove(ms_servers, server);

  Mem_Free(server);
}

/**
 * @brief Returns true if the specified server has been blacklisted, false otherwise.
 * The format of the blacklist file is one-IP-per-line, with wildcards. Ex:
 *
 * // This guy is a joker
 * 66.182.58.*
 *
 * Ensure that the file is new-line terminated for all rules to be evaluated.
 */
static bool Ms_BlacklistServer(struct sockaddr_in *from) {
  char *buffer;
  int64_t len;

  if ((len = Fs_Load("servers-blacklist", (void *) &buffer)) == -1) {
    return false;
  }

  char *c = buffer;
  char *ip = inet_ntoa(from->sin_addr);

  bool blacklisted = false;

  while ((c - buffer) < len) {
    char line[256];

    sscanf(c, "%255s\n", line);
    c += strlen(line) + 1;

    const char *l = g_strstrip(line);

    if (!strlen(l) || g_str_has_prefix(l, "//") || g_str_has_prefix(l, "#")) {
      continue;
    }

    if (GlobMatch(l, ip, GLOB_FLAGS_NONE)) {
      blacklisted = true;
      break;
    }
  }

  Fs_Free((void *) buffer);

  return blacklisted;
}

/**
 * @brief Adds the specified server to the master.
 */
static void Ms_AddServer(struct sockaddr_in *from) {

  if (Ms_GetServer(from)) {
    Com_Warn("Duplicate ping from %s\n", atos(from));
    return;
  }

  if (Ms_BlacklistServer(from)) {
    Com_Warn("Server %s has been blacklisted\n", atos(from));
    return;
  }

  ms_server_t *server = Mem_Malloc(sizeof(ms_server_t));

  server->addr = *from;
  server->last_heartbeat = time(NULL);
  server->num_clients = -1;

  ms_servers = g_list_prepend(ms_servers, server);
  Com_Print("Server %s registered\n", stos(server));

  // send an acknowledgment
  sendto(ms_sock, "\xFF\xFF\xFF\xFF" "ack", 7, 0, (struct sockaddr *) from, sizeof(*from));
}

/**
 * @brief Removes the specified server.
 */
static void Ms_RemoveServer(struct sockaddr_in *from) {
  ms_server_t *server = Ms_GetServer(from);

  if (!server) {
    Com_Warn("Shutdown from unregistered server %s\n", atos(from));
    return;
  }

  Com_Print("Shutdown from %s\n", stos(server));
  Ms_DropServer(server);
}

/**
 * @brief Processes one master-server tick: evicts stale servers, handles pending pings, and cycles to the next server.
 */
static void Ms_Frame(void) {
  const time_t now = time(NULL);

  GList *s = ms_servers;
  while (s) {
    GList *next = s->next;
    ms_server_t *server = (ms_server_t *) s->data;
    if (now - server->last_heartbeat > 30) {

      if (server->queued_pings > 6) {
        Com_Print("Server %s timed out\n", stos(server));
        Ms_DropServer(server);
      } else {
        if (now - server->last_ping >= 10) {
          server->queued_pings++;
          server->last_ping = now;

          Com_Verbose("Pinging %s\n", stos(server));

          const char *ping = "\xFF\xFF\xFF\xFF" "ping";
          sendto(ms_sock, ping, strlen(ping), 0, (struct sockaddr *) &server->addr,
                 sizeof(server->addr));
        }
      }
    }

    s = next;
  }
}

/**
 * @brief Send the servers list to the specified client address.
 */
static void Ms_GetServers(struct sockaddr_in *from) {
  mem_buf_t buf;
  byte buffer[0xffff];

  Mem_InitBuffer(&buf, buffer, sizeof(buffer));

  const char *servers = "\xFF\xFF\xFF\xFF" "servers ";
  Mem_WriteBuffer(&buf, servers, strlen(servers));

  uint32_t i = 0;
  GList *s = ms_servers;
  while (s) {
    const ms_server_t *server = (ms_server_t *) s->data;
    if (server->validated) {
      Mem_WriteBuffer(&buf, &server->addr.sin_addr, sizeof(server->addr.sin_addr));
      Mem_WriteBuffer(&buf, &server->addr.sin_port, sizeof(server->addr.sin_port));
      i++;
    }
    s = s->next;
  }

  if ((sendto(ms_sock, buf.data, buf.size, 0, (struct sockaddr *) from, sizeof(*from))) == -1) {
    Com_Warn("%s: %s\n", atos(from), strerror(errno));
  } else {
    Com_Verbose("Sent %d servers to %s\n", i, atos(from));
  }
}

/**
 * @brief Acknowledge the server from the specified address.
 */
static void Ms_Ack(struct sockaddr_in *from) {
  ms_server_t *server = Ms_GetServer(from);

  if (server) {
    Com_Verbose("Ack from %s (%d)\n", stos(server), server->queued_pings);

    server->validated = true;
    server->queued_pings = 0;

  } else {
    Com_Warn("Ack from unregistered server %s\n", atos(from));
  }
}

/**
 * @brief Accept a "heartbeat" from the specified server address.
 */
static void Ms_Heartbeat(struct sockaddr_in *from, const char *status) {
  ms_server_t *server = Ms_GetServer(from);

  if (server) {
    server->last_heartbeat = time(NULL);

    Com_Verbose("Heartbeat from %s\n", stos(server));

    if (status && *status) {
      Ms_ParseStatusString(server, status);
    }

    const void *ack = "\xFF\xFF\xFF\xFF" "ack";
    sendto(ms_sock, ack, 7, 0, (struct sockaddr *) &server->addr, sizeof(server->addr));
  } else {
    Ms_AddServer(from);
  }
}

/**
 * @brief Parses and dispatches an incoming UDP message (heartbeat, ping, ack, or getservers) from a game server.
 */
static void Ms_ParseMessage(struct sockaddr_in *from, char *data) {
  char *cmd = data;
  char *line = data;

  while (*line && *line != '\n') {
    line++;
  }

  *(line++) = '\0';
  cmd += 4;

  if (!g_ascii_strncasecmp(cmd, "ping", 4)) {
    Ms_AddServer(from);
  } else if (!g_ascii_strncasecmp(cmd, "heartbeat", 9) || !g_ascii_strncasecmp(cmd, "print", 5)) {
    Ms_Heartbeat(from, line);
  } else if (!g_ascii_strncasecmp(cmd, "ack", 3)) {
    Ms_Ack(from);
  } else if (!g_ascii_strncasecmp(cmd, "shutdown", 8)) {
    Ms_RemoveServer(from);
  } else if (!g_ascii_strncasecmp(cmd, "getservers", 10) || !g_ascii_strncasecmp(cmd, "y", 1)) {
    Ms_GetServers(from);
  } else {
    Com_Warn("Unknown command from %s: '%s'\n", atos(from), cmd);
  }
}

/**
 * @brief `Com_Debug` implementation.
 */
static void Debug(const debug_t debug, const char *msg) {

  if (debug) {
    fputs(msg, stdout);
  }
}

/**
 * @brief `Com_Verbose` implementation.
 */
static void Verbose(const char *msg) {

  if (verbose) {
    fputs(msg, stdout);
  }
}

/**
 * @brief `Com_Init` implementation.
 */
static void Init(void) {

  Mem_Init();

  Fs_Init(FS_NONE);
}

/**
 * @brief `Com_Shutdown` implementation.
 */
static void Shutdown(const char *msg) {

  if (msg) {
    fputs(msg, stdout);
  }

  g_list_free_full(ms_servers, Mem_Free);

  Fs_Shutdown();

  Mem_Shutdown();
}

/**
 * @brief Master server entry point: opens the UDP socket and runs the main receive/dispatch loop.
 */
int32_t quetoo_main(int32_t argc, char **argv) {

  printf("Quetoo Master Server %s %s\n", VERSION, BUILD);

  memset(&quetoo, 0, sizeof(quetoo));

  quetoo.Debug = Debug;
  quetoo.Verbose = Verbose;

  quetoo.Init = Init;
  quetoo.Shutdown = Shutdown;
  quetoo.log_file_name = "quetoo-master.log";

  signal(SIGINT, Sys_Signal);
  signal(SIGTERM, Sys_Signal);

#if !defined(_WIN32)
  signal(SIGQUIT, Sys_Signal);
  Sys_InitCrashSignals();
#endif

  Com_Init(argc, argv);

  int32_t i;
  for (i = 0; i < Com_Argc(); i++) {

    if (!g_strcmp0(Com_Argv(i), "-v") || !g_strcmp0(Com_Argv(i), "-verbose")) {
      verbose = true;
      continue;
    }

    if (!g_strcmp0(Com_Argv(i), "-d") || !g_strcmp0(Com_Argv(i), "-debug")) {
      debug = true;
      continue;
    }
  }

  ms_discord_webhook = getenv("QUETOO_DISCORD_WEBHOOK");
  if (ms_discord_webhook) {
    Com_Print("Discord webhook configured\n");
  }

  ms_sock = socket(AF_INET, SOCK_DGRAM, IPPROTO_UDP);

  struct sockaddr_in address;
  memset(&address, 0, sizeof(address));

  address.sin_family = AF_INET;
  address.sin_port = htons(PORT_MASTER);
  address.sin_addr.s_addr = INADDR_ANY;

  if ((bind(ms_sock, (struct sockaddr *) &address, sizeof(address))) == -1) {
    Com_Error(ERROR_FATAL, "Failed to bind port %i\n", PORT_MASTER);
  }

  Com_Print("Listening on %s\n", atos(&address));

  while (true) {
    fd_set set;

    FD_ZERO(&set);
    FD_SET(ms_sock, &set);

    struct timeval delay;
    delay.tv_sec = 1;
    delay.tv_usec = 0;

    if (select(ms_sock + 1, &set, NULL, NULL, &delay) > 0) {

      if (FD_ISSET(ms_sock, &set)) {

        char buffer[0xffff];
        memset(buffer, 0, sizeof(buffer));

        struct sockaddr_in from;
        memset(&from, 0, sizeof(from));

        socklen_t from_len = sizeof(from);

        const ssize_t len = recvfrom(ms_sock, buffer, sizeof(buffer), 0,
                                     (struct sockaddr *) &from, &from_len);

        if (len > 0) {
          if (len > 4) {
            Ms_ParseMessage(&from, buffer);
          } else {
            Com_Warn("Invalid packet from %s\n", atos(&from));
          }
        } else {
          Com_Warn("Socket error: %s\n", strerror(errno));
        }
      }
    }

    if (sys_signal_received) {
      Com_Shutdown("Received signal %d, quitting...\n", sys_signal_received);
    }

    Ms_Frame();
  }
}

#if !defined(_WIN32)
int32_t main(int32_t argc, char **argv) {
  return quetoo_main(argc, argv);
}
#endif
