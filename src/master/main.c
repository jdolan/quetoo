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

#if defined(_WIN32)
  #define _CRT_RAND_S
#endif

#include <ctype.h>
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
  #include <fcntl.h>
  #include <netinet/in.h>
  #include <sys/select.h>
  #include <sys/socket.h>
  #include <unistd.h>

#endif

#include "common/common.h"
#include <Objectively/RESTClient.h>

quetoo_t quetoo;

/**
 * @brief A server must heartbeat within this window or be probed with pings.
 */
#define SERVER_TIMEOUT_SECONDS 30

/**
 * @brief Grace period for a newly registered server to answer its challenge.
 */
#define VALIDATION_TIMEOUT_SECONDS 30

/**
 * @brief Minimum spacing between challenges issued to the same server.
 */
#define CHALLENGE_INTERVAL_SECONDS 1

/**
 * @brief Upper bound on concurrently registered servers.
 */
#define MAX_SERVERS 1024

/**
 * @brief Upper bound on servers awaiting validation, so that forged heartbeats
 * cannot crowd out registrations from servers that can actually answer.
 */
#define MAX_PENDING_SERVERS 256

typedef struct ms_server_s {
  struct sockaddr_in addr;
  time_t registered;
  time_t last_heartbeat;
  uint32_t challenge;
  time_t last_challenge;
  bool validated;
  char hostname[256];
  char map[64];
  int32_t protocol;
  int32_t num_clients;
  int32_t max_clients;
  char players[MAX_CLIENTS][64];
} ms_server_t;

static List *ms_servers;
static int32_t ms_sock;

#if !defined(_WIN32)
static int32_t ms_urandom = -1;
#endif

static bool verbose;
static bool debug;

static const char *ms_discord_webhook;

/**
 * @brief Extracts the value for the given key from a Quake infostring.
 * @return True if the key was found and the value copied, false otherwise.
 */
static bool Ms_InfoValue(const char *info, const char *key, char *buf, size_t buf_size) {
  char search[256];
  q_snprintf(search, sizeof(search), "\\%s\\", key);

  const char *p = q_strstr(info, search);
  if (!p) {
    return false;
  }

  p += q_strlen(search);

  size_t len;
  const char *end = strpbrk(p, "\\\n");
  if (end) {
    len = end - p;
  } else {
    len = q_strlen(p);
  }
  len = Minui64(len, buf_size - 1);

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
static void Ms_DiscordNotify(const ms_server_t *server, const char *player_name, int32_t num_clients) {
  if (!ms_discord_webhook) {
    return;
  }

  char escaped_player[128];
  char escaped_host[256];
  char escaped_map[128];
  Ms_JsonEscape(player_name, escaped_player, sizeof(escaped_player));
  Ms_JsonEscape(server->hostname, escaped_host, sizeof(escaped_host));
  Ms_JsonEscape(server->map, escaped_map, sizeof(escaped_map));

  const char *ip = inet_ntoa(server->addr.sin_addr);
  const int32_t port = ntohs(server->addr.sin_port);

  char json[1024];
  q_snprintf(json, sizeof(json),
    "{\"embeds\":[{\"description\":\"\xF0\x9F\x8E\xAE **%s** joined **%s** on **%s** \xC2\xB7 %d/%d players \xC2\xB7 [Join](https://quetoo.org/join/?%s:%d)\",\"color\":3066993}]}",
    escaped_player, escaped_host, escaped_map,
    num_clients, server->max_clients,
    ip, port);

  Data *body = $$(Data, dataWithBytes, (const uint8_t *) json, q_strlen(json));
  $($$(RESTClient, sharedInstance), postAsync, ms_discord_webhook, body, NULL, NULL, NULL);
  release(body);
}

/**
 * @brief Parses a status string from a server heartbeat, updates the server's
 * player list, and fires Discord notifications for any new players detected.
 * On first call (num_clients == -1), records current state without notifying.
 */
static void Ms_ParseStatusString(ms_server_t *server, const char *status) {

  char val[256];

  if (Ms_InfoValue(status, "sv_hostname", val, sizeof(val))) {
    q_strcolorstrip(val, server->hostname);
  }

  if (Ms_InfoValue(status, "sv_protocol", val, sizeof(val))) {
    server->protocol = atoi(val);
  }

  server->max_clients = 0;
  if (Ms_InfoValue(status, "sv_max_clients", val, sizeof(val))) {
    server->max_clients = atoi(val);
  }

  bool map_changed = false;
  if (Ms_InfoValue(status, "sv_map", val, sizeof(val))) {
    map_changed = q_strcmp(server->map, val) != 0;
    q_strlcpy(server->map, val, sizeof(server->map));
  }

  char new_players[MAX_CLIENTS][64];
  int32_t new_count = 0;

  // player lines begin after the infostring's trailing newline
  const char *line = q_strchr(status, '\n');
  while (line && new_count < MAX_CLIENTS) {
    line++; // skip the newline
    if (*line == '\0') {
      break;
    }

    // isolate the current player line to prevent cross-line key lookups
    const char *line_end = q_strchr(line, '\n');
    char cur_line[256];
    if (line_end) {
      q_strlcpy(cur_line, line, (size_t) (line_end - line) + 1 < sizeof(cur_line) ? (size_t)(line_end - line) + 1 : sizeof(cur_line));
    } else {
      q_strlcpy(cur_line, line, sizeof(cur_line));
    }

    char name[64] = { 0 };
    char ai_val[4] = { 0 };
    if (Ms_InfoValue(cur_line, "name", name, sizeof(name)) && name[0]) {
      char stripped[64];
      q_strcolorstrip(name, stripped);
      Ms_InfoValue(cur_line, "ai", ai_val, sizeof(ai_val));
      Com_Verbose("Player: %s ai=%s\n", stripped, ai_val[0] ? ai_val : "(none)");
      if (!atoi(ai_val)) {
        q_strlcpy(new_players[new_count], stripped, sizeof(new_players[new_count]));
        new_count++;
      }
    }

    line = line_end;
  }

  const int32_t old_count = server->num_clients;
  const bool initialized = (old_count >= 0);

  if (initialized && !map_changed) {
    for (int32_t i = 0; i < new_count; i++) {
      bool found = false;
      for (int32_t j = 0; j < old_count; j++) {
        if (!q_strcmp(new_players[i], server->players[j])) {
          found = true;
          break;
        }
      }
      if (!found) {
        Ms_DiscordNotify(server, new_players[i], new_count);
      }
    }
  }

  server->num_clients = new_count;
  for (int32_t i = 0; i < new_count; i++) {
    q_strlcpy(server->players[i], new_players[i], sizeof(server->players[i]));
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

  for (const ListNode *s = ms_servers ? ms_servers->head : NULL; s; s = s->next) {
    ms_server_t *server = (ms_server_t *) s->element;

    const struct sockaddr_in *addr = &server->addr;
    if (addr->sin_addr.s_addr == from->sin_addr.s_addr && addr->sin_port == from->sin_port) {
      return server;
    }
  }

  return NULL;
}

/**
 * @brief Removes the specified server.
 */
static void Ms_DropServer(ms_server_t *server) {

  if (ms_servers) {
    for (const ListNode *s = ms_servers->head; s; s = s->next) {
      if (s->element == server) {
        $(ms_servers, removeNode, (ListNode *) s);
        break;
      }
    }
  }

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
    c += q_strlen(line) + 1;

    const char *l = line;
    while (isspace((unsigned char) *l)) { l++; }
    char *_lend = (char *) l + q_strlen(l) - 1;
    while (_lend >= l && isspace((unsigned char) *_lend)) { *_lend-- = '\0'; }

    if (!q_strlen(l) || !q_strncmp(l, "//", 2) || l[0] == '#') {
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
 * @brief Returns an unpredictable non-zero challenge value. A guessable
 * challenge would let a spoofer validate an address it cannot receive at,
 * which is the entire attack this handshake exists to stop.
 */
static uint32_t Ms_Challenge(void) {
  uint32_t challenge = 0;

  while (challenge == 0) {
#if defined(_WIN32)
    if (rand_s(&challenge)) {
      Com_Error(ERROR_FATAL, "Failed to generate a challenge\n");
    }
#else
    if (read(ms_urandom, &challenge, sizeof(challenge)) != (ssize_t) sizeof(challenge)) {
      Com_Error(ERROR_FATAL, "Failed to read /dev/urandom: %s\n", strerror(errno));
    }
#endif
  }

  return challenge;
}

/**
 * @brief Issues the specified server's challenge, which it must echo in a
 * subsequent heartbeat to be listed.
 */
static void Ms_SendChallenge(ms_server_t *server, time_t now) {

  if (server->last_challenge && now - server->last_challenge < CHALLENGE_INTERVAL_SECONDS) {
    return; // do not let a heartbeat flood become a challenge flood
  }

  if (!server->challenge) {
    server->challenge = Ms_Challenge();
  }

  server->last_challenge = now;

  char buffer[32];
  memcpy(buffer, "\xFF\xFF\xFF\xFF", 4);

  const int32_t len = q_snprintf(buffer + 4, sizeof(buffer) - 4, "challenge %u", server->challenge);

  Com_Verbose("Challenging %s\n", stos(server));

  sendto(ms_sock, buffer, 4 + len, 0, (struct sockaddr *) &server->addr, sizeof(server->addr));
}

/**
 * @brief Adds the specified server to the master.
 * @return The newly registered server, or `NULL` if it was rejected.
 */
static ms_server_t *Ms_AddServer(struct sockaddr_in *from) {

  if (Ms_GetServer(from)) {
    Com_Warn("Duplicate registration from %s\n", atos(from));
    return NULL;
  }

  // bound the list before touching the filesystem for the blacklist
  if (ms_servers && ms_servers->count >= MAX_SERVERS) {
    Com_Warn("Server list is full, rejecting %s\n", atos(from));
    return NULL;
  }

  size_t pending = 0;
  for (const ListNode *s = ms_servers ? ms_servers->head : NULL; s; s = s->next) {
    if (!((const ms_server_t *) s->element)->validated) {
      pending++;
    }
  }

  if (pending >= MAX_PENDING_SERVERS) {
    Com_Warn("Too many servers awaiting validation, rejecting %s\n", atos(from));
    return NULL;
  }

  if (Ms_BlacklistServer(from)) {
    Com_Warn("Server %s has been blacklisted\n", atos(from));
    return NULL;
  }

  ms_server_t *server = Mem_Malloc(sizeof(ms_server_t));

  server->addr = *from;
  server->registered = time(NULL);
  server->last_heartbeat = server->registered;
  server->num_clients = -1;

  if (!ms_servers) {
    ms_servers = $(alloc(List), init);
  }
  $(ms_servers, append, server);
  Com_Print("Server %s registered, awaiting validation\n", stos(server));

  return server;
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
 * @brief Processes one master-server tick, evicting servers that have gone quiet
 * and those that never answered their challenge.
 */
static void Ms_Frame(void) {
  const time_t now = time(NULL);

  for (ListNode *s = ms_servers ? ms_servers->head : NULL; s; ) {
    ListNode *next = s->next;
    ms_server_t *server = (ms_server_t *) s->element;

    if (now - server->last_heartbeat > SERVER_TIMEOUT_SECONDS) {
      Com_Print("Server %s timed out\n", stos(server));
      Ms_DropServer(server);
    } else if (!server->validated && now - server->registered > VALIDATION_TIMEOUT_SECONDS) {
      Com_Print("Server %s failed to validate\n", stos(server));
      Ms_DropServer(server);
    }

    s = next;
  }
}

/**
 * @brief Send the servers list to the specified client address.
 */
static void Ms_GetServers(struct sockaddr_in *from, const char *cmd) {
  mem_buf_t buf;
  byte buffer[0xffff];

  // parse optional protocol version from command (e.g. "getservers 2026"), zero for all
  int32_t protocol = 0;
  const char *p = cmd + q_strlen("getservers");
  while (*p == ' ') p++;
  if (*p) {
    const int32_t requested = atoi(p);
    if (requested > 0) {
      protocol = requested;
    }
  }

  Mem_InitBuffer(&buf, buffer, sizeof(buffer));

  const char *servers = "\xFF\xFF\xFF\xFF" "servers ";
  Mem_WriteBuffer(&buf, servers, q_strlen(servers));

  uint32_t i = 0;
  for (const ListNode *s = ms_servers ? ms_servers->head : NULL; s; s = s->next) {
    const ms_server_t *server = (ms_server_t *) s->element;
    if (server->validated && (protocol == 0 || server->protocol == protocol)) {
      Mem_WriteBuffer(&buf, &server->addr.sin_addr, sizeof(server->addr.sin_addr));
      Mem_WriteBuffer(&buf, &server->addr.sin_port, sizeof(server->addr.sin_port));
      i++;
    }
  }

  if ((sendto(ms_sock, (const char *) buf.data, (int32_t) buf.size, 0, (struct sockaddr *) from, sizeof(*from))) == -1) {
    Com_Warn("%s: %s\n", atos(from), strerror(errno));
  } else {
    Com_Verbose("Sent %d servers (protocol %d) to %s\n", i, protocol, atos(from));
  }
}

/**
 * @brief Accept a "heartbeat" from the specified server address. The command is
 * `heartbeat <challenge>`; a server is listed only once it echoes the challenge
 * we issued to the address it heartbeats from.
 */
static void Ms_Heartbeat(struct sockaddr_in *from, const char *cmd, const char *status) {
  const time_t now = time(NULL);

  // the echoed challenge, if any, follows the command name
  const char *c = cmd + q_strlen("heartbeat");
  while (*c == ' ') {
    c++;
  }

  const uint32_t challenge = (uint32_t) strtoul(c, NULL, 10);

  ms_server_t *server = Ms_GetServer(from);

  if (!server) {
    if (!(server = Ms_AddServer(from))) {
      return;
    }
  }

  server->last_heartbeat = now;

  if (!server->validated) {

    if (!challenge || challenge != server->challenge) {
      Ms_SendChallenge(server, now);
      return;
    }

    server->validated = true;
    Com_Print("Server %s validated\n", stos(server));
  }

  Com_Verbose("Heartbeat from %s\n", stos(server));

  // only a validated server may shape what we publish or announce
  if (status && *status) {
    Ms_ParseStatusString(server, status);
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

  if (*line == '\n') {
    *(line++) = '\0';
  }

  cmd += 4;

  if (!q_strncasecmp(cmd, "heartbeat", 9)) {
    Ms_Heartbeat(from, cmd, line);
  } else if (!q_strncasecmp(cmd, "shutdown", 8)) {
    Ms_RemoveServer(from);
  } else if (!q_strncasecmp(cmd, "getservers", 10) || !q_strncasecmp(cmd, "y", 1)) {
    Ms_GetServers(from, cmd);
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

  if (ms_servers) {
    for (const ListNode *s = ms_servers->head; s; s = s->next) {
      Mem_Free(s->element);
    }
    release(ms_servers);
  }

  Fs_Shutdown();

  Mem_Shutdown();
}

/**
 * @brief Master server entry point: opens the UDP socket and runs the main receive/dispatch loop.
 */
int32_t quetoo_main(int32_t argc, char **argv) {

  setvbuf(stdout, NULL, _IOLBF, 0);

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

    if (!q_strcmp(Com_Argv(i), "-v") || !q_strcmp(Com_Argv(i), "--verbose")) {
      verbose = true;
      continue;
    }

    if (!q_strcmp(Com_Argv(i), "-d") || !q_strcmp(Com_Argv(i), "--debug")) {
      debug = true;
      continue;
    }
  }

  ms_discord_webhook = getenv("QUETOO_DISCORD_WEBHOOK");
  if (ms_discord_webhook) {
    Com_Print("Discord webhook configured\n");
  }

#if !defined(_WIN32)
  if ((ms_urandom = open("/dev/urandom", O_RDONLY)) == -1) {
    Com_Error(ERROR_FATAL, "Failed to open /dev/urandom: %s\n", strerror(errno));
  }
#endif

  ms_sock = (int32_t) socket(AF_INET, SOCK_DGRAM, IPPROTO_UDP);

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
#if defined(_WIN32)
    FD_SET((SOCKET) ms_sock, &set);
#else
    FD_SET(ms_sock, &set);
#endif

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

        const ssize_t len = recvfrom(ms_sock, buffer, sizeof(buffer) - 1, 0,
                                     (struct sockaddr *) &from, &from_len);

        if (len > 0) {
          buffer[len] = '\0';

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
