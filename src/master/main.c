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
 * Foundation, Inc., 59 Temple Place - Suite 330, Boston, MA 02111-1307, USA.
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

Quetoo quetoo;

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

typedef struct {
  struct sockaddr_in addr;
  time_t registered;
  time_t lastHeartbeat;
  uint32_t challenge;
  time_t lastChallenge;
  bool validated;
  char hostname[256];
  char map[64];
  int32_t protocol;
  int32_t numClients;
  int32_t maxClients;
  char players[MAX_CLIENTS][64];
} MasterServer;

static List *msServers;
static int32_t msSock;

#if !defined(_WIN32)
static int32_t msUrandom = -1;
#endif

static bool verbose;
static bool debug;

static const char *msDiscordWebhook;

/**
 * @brief Extracts the value for the given key from a Quake infostring.
 * @return True if the key was found and the value copied, false otherwise.
 */
static bool Ms_InfoValue(const char *info, const char *key, char *buf, size_t bufSize) {
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
  len = Minui64(len, bufSize - 1);

  memcpy(buf, p, len);
  buf[len] = '\0';
  return true;
}

/**
 * @brief JSON-escapes `src` into `buf`.
 */
static void Ms_JsonEscape(const char *src, char *buf, size_t bufSize) {
  size_t out = 0;
  for (const char *s = src; *s && out + 2 < bufSize; s++) {
    if (*s == '"' || *s == '\\') {
      if (out + 3 < bufSize) {
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
static void Ms_DiscordNotify(const MasterServer *server, const char *playerName, int32_t numClients) {
  if (!msDiscordWebhook) {
    return;
  }

  char escapedPlayer[128];
  char escapedHost[256];
  char escapedMap[128];
  Ms_JsonEscape(playerName, escapedPlayer, sizeof(escapedPlayer));
  Ms_JsonEscape(server->hostname, escapedHost, sizeof(escapedHost));
  Ms_JsonEscape(server->map, escapedMap, sizeof(escapedMap));

  const char *ip = inet_ntoa(server->addr.sin_addr);
  const int32_t port = ntohs(server->addr.sin_port);

  char json[1024];
  q_snprintf(json, sizeof(json),
    "{\"embeds\":[{\"description\":\"\xF0\x9F\x8E\xAE **%s** joined **%s** on **%s** \xC2\xB7 %d/%d players \xC2\xB7 [Join](https://quetoo.org/join/?%s:%d)\",\"color\":3066993}]}",
    escapedPlayer, escapedHost, escapedMap,
    numClients, server->maxClients,
    ip, port);

  Data *body = $$(Data, dataWithBytes, (const uint8_t *) json, q_strlen(json));
  $($$(RESTClient, sharedInstance), postAsync, msDiscordWebhook, body, NULL, NULL, NULL);
  release(body);
}

/**
 * @brief Parses a status string from a server heartbeat, updates the server's
 * player list, and fires Discord notifications for any new players detected.
 * On first call (numClients == -1), records current state without notifying.
 */
static void Ms_ParseStatusString(MasterServer *server, const char *status) {

  char val[256];

  if (Ms_InfoValue(status, "sv_hostname", val, sizeof(val))) {
    q_strcolorstrip(val, server->hostname);
  }

  if (Ms_InfoValue(status, "sv_protocol", val, sizeof(val))) {
    server->protocol = atoi(val);
  }

  server->maxClients = 0;
  if (Ms_InfoValue(status, "sv_maxClients", val, sizeof(val))) {
    server->maxClients = atoi(val);
  }

  bool mapChanged = false;
  if (Ms_InfoValue(status, "sv_map", val, sizeof(val))) {
    mapChanged = q_strcmp(server->map, val) != 0;
    q_strlcpy(server->map, val, sizeof(server->map));
  }

  char newPlayers[MAX_CLIENTS][64];
  int32_t newCount = 0;

  // player lines begin after the infostring's trailing newline
  const char *line = q_strchr(status, '\n');
  while (line && newCount < MAX_CLIENTS) {
    line++; // skip the newline
    if (*line == '\0') {
      break;
    }

    // isolate the current player line to prevent cross-line key lookups
    const char *lineEnd = q_strchr(line, '\n');
    char curLine[256];
    if (lineEnd) {
      q_strlcpy(curLine, line, (size_t) (lineEnd - line) + 1 < sizeof(curLine) ? (size_t)(lineEnd - line) + 1 : sizeof(curLine));
    } else {
      q_strlcpy(curLine, line, sizeof(curLine));
    }

    char name[64] = { 0 };
    char aiVal[4] = { 0 };
    if (Ms_InfoValue(curLine, "name", name, sizeof(name)) && name[0]) {
      char stripped[64];
      q_strcolorstrip(name, stripped);
      Ms_InfoValue(curLine, "ai", aiVal, sizeof(aiVal));
      Com_Verbose("Player: %s ai=%s\n", stripped, aiVal[0] ? aiVal : "(none)");
      if (!atoi(aiVal)) {
        q_strlcpy(newPlayers[newCount], stripped, sizeof(newPlayers[newCount]));
        newCount++;
      }
    }

    line = lineEnd;
  }

  const int32_t oldCount = server->numClients;
  const bool initialized = (oldCount >= 0);

  if (initialized && !mapChanged) {
    for (int32_t i = 0; i < newCount; i++) {
      bool found = false;
      for (int32_t j = 0; j < oldCount; j++) {
        if (!q_strcmp(newPlayers[i], server->players[j])) {
          found = true;
          break;
        }
      }
      if (!found) {
        Ms_DiscordNotify(server, newPlayers[i], newCount);
      }
    }
  }

  server->numClients = newCount;
  for (int32_t i = 0; i < newCount; i++) {
    q_strlcpy(server->players[i], newPlayers[i], sizeof(server->players[i]));
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
static MasterServer *Ms_GetServer(struct sockaddr_in *from) {

  for (const ListNode *s = msServers ? msServers->head : NULL; s; s = s->next) {
    MasterServer *server = (MasterServer *) s->element;

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
static void Ms_DropServer(MasterServer *server) {

  if (msServers) {
    for (const ListNode *s = msServers->head; s; s = s->next) {
      if (s->element == server) {
        $(msServers, removeNode, (ListNode *) s);
        break;
      }
    }
  }

  Mem_Free(server);
}

/**
 * @brief The blacklist file, relative to any filesystem search path root.
 */
#define BLACKLIST_FILE "servers-blacklist"

/**
 * @brief Upper bound on the number of blacklist rules.
 */
#define MAX_BLACKLIST_RULES 256

/**
 * @brief The parsed blacklist, reloaded when the file changes on disk.
 */
static struct {
  char rules[MAX_BLACKLIST_RULES][64];
  size_t count;
  int64_t modified;
  int64_t size;
  bool loaded;
} msBlacklist;

/**
 * @brief Parses the contents of the blacklist file into the rule cache.
 */
static void Ms_ParseBlacklist(const char *buffer, int64_t length) {

  msBlacklist.count = 0;

  const char *c = buffer, *end = buffer + length;

  while (c < end) {
    const char *newline = memchr(c, '\n', (size_t) (end - c));
    const char *lineStart = c, *lineEnd = newline ? newline : end;

    c = newline ? newline + 1 : end;

    while (lineStart < lineEnd && isspace((unsigned char) *lineStart)) {
      lineStart++;
    }

    // a comment may follow a rule on the same line, so that an operator can
    // say what each address is without giving it a line of its own
    for (const char *comment = lineStart; comment < lineEnd; comment++) {
      if (*comment == '#' || (comment + 1 < lineEnd && *comment == '/' && comment[1] == '/')) {
        lineEnd = comment;
        break;
      }
    }

    while (lineEnd > lineStart && isspace((unsigned char) *(lineEnd - 1))) {
      lineEnd--;
    }

    const size_t size = (size_t) (lineEnd - lineStart);

    if (!size) {
      continue;
    }

    if (lineEnd[-1] == ':') {
      Com_Warn("Blacklist rule names no port after its ':', ignoring: %.*s\n", (int32_t) size, lineStart);
      continue;
    }

    if (size >= sizeof(msBlacklist.rules[0])) {
      Com_Warn("Blacklist rule is too long, ignoring: %.*s\n", (int32_t) size, lineStart);
      continue;
    }

    if (msBlacklist.count == MAX_BLACKLIST_RULES) {
      Com_Warn("Blacklist is full, ignoring %.*s and all that follow\n", (int32_t) size, lineStart);
      break;
    }

    memcpy(msBlacklist.rules[msBlacklist.count], lineStart, size);
    msBlacklist.rules[msBlacklist.count][size] = '\0';
    msBlacklist.count++;
  }
}

/**
 * @brief Reloads the blacklist if the file has changed since the last read.
 * @remarks This runs on every heartbeat, so the file is stat'ed, not re-read,
 * unless its modification time has moved.
 */
static void Ms_LoadBlacklist(void) {

  FsStat stat;
  if (!Fs_Stat(BLACKLIST_FILE, &stat)) {
    msBlacklist.count = 0;
    msBlacklist.loaded = false;
    return;
  }

  // the size joins the modification time, which PhysFS reports to the second,
  // so that an edit landing in the same second as the last read is still seen
  if (msBlacklist.loaded && stat.modified == msBlacklist.modified && stat.size == msBlacklist.size) {
    return;
  }

  msBlacklist.loaded = true;
  msBlacklist.modified = stat.modified;
  msBlacklist.size = stat.size;

  char *buffer;
  const int64_t length = Fs_Load(BLACKLIST_FILE, (void *) &buffer);

  if (length == -1) {
    Com_Warn("Failed to load %s, keeping %u rules: %s\n", BLACKLIST_FILE,
             (uint32_t) msBlacklist.count, Fs_LastError());
    return;
  }

  if (length) {
    Ms_ParseBlacklist(buffer, length);
  } else {
    msBlacklist.count = 0;
  }

  Fs_Free((void *) buffer);

  Com_Print("Loaded %u blacklist rules\n", (uint32_t) msBlacklist.count);
}

/**
 * @brief Returns true if the specified server has been blacklisted, false otherwise.
 * The format of the blacklist file is one rule per line, with wildcards. A rule
 * may qualify the address with a port, and matches any port if it does not.
 * `#` or `//` begins a comment, which may follow a rule on the same line. Ex:
 *
 * // This guy is a joker
 * 66.182.58.*
 * 203.0.113.7:27910 # and only on that port
 */
static bool Ms_BlacklistServer(const struct sockaddr_in *from) {

  Ms_LoadBlacklist();

  if (!msBlacklist.count) {
    return false;
  }

  const char *ip = inet_ntoa(from->sin_addr);

  char ipPort[64];
  q_snprintf(ipPort, sizeof(ipPort), "%s:%d", ip, ntohs(from->sin_port));

  for (size_t i = 0; i < msBlacklist.count; i++) {
    const char *rule = msBlacklist.rules[i];

    if (GlobMatch(rule, q_strchr(rule, ':') ? ipPort : ip, GLOB_FLAGS_NONE)) {
      return true;
    }
  }

  return false;
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
    if (read(msUrandom, &challenge, sizeof(challenge)) != (ssize_t) sizeof(challenge)) {
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
static void Ms_SendChallenge(MasterServer *server, time_t now) {

  if (server->lastChallenge && now - server->lastChallenge < CHALLENGE_INTERVAL_SECONDS) {
    return; // do not let a heartbeat flood become a challenge flood
  }

  if (!server->challenge) {
    server->challenge = Ms_Challenge();
  }

  server->lastChallenge = now;

  char buffer[32];
  memcpy(buffer, "\xFF\xFF\xFF\xFF", 4);

  const int32_t len = q_snprintf(buffer + 4, sizeof(buffer) - 4, "challenge %u", server->challenge);

  Com_Verbose("Challenging %s\n", stos(server));

  sendto(msSock, buffer, 4 + len, 0, (struct sockaddr *) &server->addr, sizeof(server->addr));
}

/**
 * @brief Returns the challenge echoed after the given command name, or zero if
 * none was supplied. The caller has already matched `name` as a prefix of `cmd`.
 */
static uint32_t Ms_ParseChallenge(const char *cmd, const char *name) {

  const char *c = cmd + q_strlen(name);
  while (*c == ' ') {
    c++;
  }

  return (uint32_t) strtoul(c, NULL, 10);
}

/**
 * @brief Adds the specified server to the master.
 * @return The newly registered server, or `NULL` if it was rejected.
 */
static MasterServer *Ms_AddServer(struct sockaddr_in *from) {

  if (Ms_GetServer(from)) {
    Com_Warn("Duplicate registration from %s\n", atos(from));
    return NULL;
  }

  // bound the list before touching the filesystem for the blacklist
  if (msServers && msServers->count >= MAX_SERVERS) {
    Com_Warn("Server list is full, rejecting %s\n", atos(from));
    return NULL;
  }

  size_t pending = 0;
  for (const ListNode *s = msServers ? msServers->head : NULL; s; s = s->next) {
    if (!((const MasterServer *) s->element)->validated) {
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

  MasterServer *server = Mem_Malloc(sizeof(MasterServer));

  server->addr = *from;
  server->registered = time(NULL);
  server->lastHeartbeat = server->registered;
  server->numClients = -1;

  if (!msServers) {
    msServers = $(alloc(List), init);
  }
  $(msServers, append, server);
  Com_Print("Server %s registered, awaiting validation\n", stos(server));

  return server;
}

/**
 * @brief Removes the specified server.
 */
static void Ms_RemoveServer(struct sockaddr_in *from, const char *cmd) {
  MasterServer *server = Ms_GetServer(from);

  if (!server) {
    Com_Warn("Shutdown from unregistered server %s\n", atos(from));
    return;
  }

  // a delisting must be authenticated too, or one forged packet unlists anyone
  const uint32_t challenge = Ms_ParseChallenge(cmd, "shutdown");

  if (!challenge || challenge != server->challenge) {
    Com_Warn("Shutdown from %s without its challenge\n", stos(server));
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

  for (ListNode *s = msServers ? msServers->head : NULL; s; ) {
    ListNode *next = s->next;
    MasterServer *server = (MasterServer *) s->element;

    if (now - server->lastHeartbeat > SERVER_TIMEOUT_SECONDS) {
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
  MemBuf buf;
  byte buffer[0xffff];

  // parse optional protocol version from command (e.g. "getservers 2026"). A query
  // that names no protocol gets the current one, so that a stale server is never
  // offered to a client that could not join it. The legacy "y" alias carries no
  // arguments, and must not be read past.
  int32_t protocol = PROTOCOL_MAJOR;
  if (!q_strncasecmp(cmd, "getservers", 10)) {
    const char *p = cmd + q_strlen("getservers");
    while (*p == ' ') p++;
    if (*p) {
      const int32_t requested = atoi(p);
      if (requested > 0) {
        protocol = requested;
      }
    }
  }

  Mem_InitBuffer(&buf, buffer, sizeof(buffer));

  const char *servers = "\xFF\xFF\xFF\xFF" "servers ";
  Mem_WriteBuffer(&buf, servers, q_strlen(servers));

  uint32_t i = 0;
  for (const ListNode *s = msServers ? msServers->head : NULL; s; s = s->next) {
    const MasterServer *server = (MasterServer *) s->element;
    if (server->validated && server->protocol == protocol) {
      Mem_WriteBuffer(&buf, &server->addr.sin_addr, sizeof(server->addr.sin_addr));
      Mem_WriteBuffer(&buf, &server->addr.sin_port, sizeof(server->addr.sin_port));
      i++;
    }
  }

  if ((sendto(msSock, (const char *) buf.data, (int32_t) buf.size, 0, (struct sockaddr *) from, sizeof(*from))) == -1) {
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

  const uint32_t challenge = Ms_ParseChallenge(cmd, "heartbeat");

  MasterServer *server = Ms_GetServer(from);

  if (!server) {
    if (!(server = Ms_AddServer(from))) {
      return;
    }
  } else if (Ms_BlacklistServer(from)) {
    Com_Print("Server %s has been blacklisted\n", stos(server));
    Ms_DropServer(server);
    return;
  }

  // every heartbeat carries the challenge, not just the one that validates, so
  // that a forged packet can neither keep a listing alive nor restate it. A
  // mismatch only re-issues the challenge; it never unlists a server, or a
  // spoofer could delist one at will
  if (!challenge || challenge != server->challenge) {
    Ms_SendChallenge(server, now);
    return;
  }

  server->lastHeartbeat = now;

  if (!server->validated) {
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
 * @brief Parses and dispatches an incoming UDP message (heartbeat, shutdown or
 * getservers) from a game server.
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
    Ms_RemoveServer(from, cmd);
  } else if (!q_strncasecmp(cmd, "getservers", 10)) {
    Ms_GetServers(from, cmd);
  } else {
    Com_Warn("Unknown command from %s: '%s'\n", atos(from), cmd);
  }
}

/**
 * @brief `Com_Debug` implementation.
 */
static void Debug(const DebugFlags debug, const char *msg) {

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

  if (msServers) {
    for (const ListNode *s = msServers->head; s; s = s->next) {
      Mem_Free(s->element);
    }
    release(msServers);
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
  quetoo.logFileName = "quetoo-master.log";

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

  msDiscordWebhook = getenv("QUETOO_DISCORD_WEBHOOK");
  if (msDiscordWebhook) {
    Com_Print("Discord webhook configured\n");
  }

#if !defined(_WIN32)
  if ((msUrandom = open("/dev/urandom", O_RDONLY)) == -1) {
    Com_Error(ERROR_FATAL, "Failed to open /dev/urandom: %s\n", strerror(errno));
  }
#endif

  msSock = (int32_t) socket(AF_INET, SOCK_DGRAM, IPPROTO_UDP);

  struct sockaddr_in address;
  memset(&address, 0, sizeof(address));

  address.sin_family = AF_INET;
  address.sin_port = htons(PORT_MASTER);
  address.sin_addr.s_addr = INADDR_ANY;

  if ((bind(msSock, (struct sockaddr *) &address, sizeof(address))) == -1) {
    Com_Error(ERROR_FATAL, "Failed to bind port %i\n", PORT_MASTER);
  }

  Com_Print("Listening on %s\n", atos(&address));

  while (true) {
    fd_set set;

    FD_ZERO(&set);
#if defined(_WIN32)
    FD_SET((SOCKET) msSock, &set);
#else
    FD_SET(msSock, &set);
#endif

    struct timeval delay;
    delay.tv_sec = 1;
    delay.tv_usec = 0;

    if (select(msSock + 1, &set, NULL, NULL, &delay) > 0) {

      if (FD_ISSET(msSock, &set)) {

        char buffer[0xffff];
        memset(buffer, 0, sizeof(buffer));

        struct sockaddr_in from;
        memset(&from, 0, sizeof(from));

        socklen_t fromLen = sizeof(from);

        const ssize_t len = recvfrom(msSock, buffer, sizeof(buffer) - 1, 0,
                                     (struct sockaddr *) &from, &fromLen);

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
