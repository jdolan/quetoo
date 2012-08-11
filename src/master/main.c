/*
 * Copyright(c) 2002 r1ch.net.
 * Copyright(c) 2006 Quake2World.
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

#include <stdio.h>
#include <string.h>
#include <time.h>
#include <signal.h>
#include <errno.h>
#include <arpa/inet.h>
#include <netinet/in.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <unistd.h>

#include "filesystem.h"
#include "net.h"

quake2world_t quake2world;

typedef struct server_s {
	struct server_s *prev;
	struct server_s *next;
	struct sockaddr_in ip;
	uint16_t port;
	uint32_t queued_pings;
	uint32_t heartbeats;
	uint32_t last_heartbeat;
	uint32_t last_ping;
	bool_ validated;
} server_t;

static server_t servers;
int32_t sock;

/*
 * @brief
 */
static void Ms_Shutdown(void) {
	server_t *server = &servers;
	server_t *old = NULL;

	Com_Print("Master server shutting down.\n");

	while (server->next) {
		if (old)
			free(old);
		server = server->next;
		old = server;
	}

	if (old)
		free(old);
}

/*
 * @brief
 */
static server_t *Ms_GetServer(struct sockaddr_in *from) {
	server_t *server = &servers;

	while (server->next) {

		server = server->next;

		if (*(int32_t *) &from->sin_addr == *(int32_t *) &server->ip.sin_addr && from->sin_port
				== server->port) {
			return server;
		}
	}

	return NULL;
}

/*
 * @brief
 */
static void Ms_DropServer(server_t *server) {

	if (server->next)
		server->next->prev = server->prev;

	if (server->prev)
		server->prev->next = server->next;

	free(server);
}

/*
 * @brief Returns true if the specified server has been blacklisted, false otherwise.
 * The format of the blacklist file is one-IP-per-line, with wildcards. Ex:
 *
 * // This guy is a joker
 * 66.182.58.*
 *
 * Ensure that the file is new-line terminated for all rules to be evaluated.
 */
static bool Ms_BlacklistServer(struct sockaddr_in *from) {
	void *buf;
	int32_t len;

	if ((len = Fs_LoadFile("servers-blacklist", &buf)) == -1) {
		return false;
	}

	char *c = (char *) buf;
	char *ip = inet_ntoa(from->sin_addr);

	while ((c - (char *) buf) < len) {
		char line[256];

		sscanf(c, "%s\n", line);

		if (strncmp(line, "//", 2)) {
			if (GlobMatch(line, ip)) {
				return true;
			}
		}

		c += strlen(line) + 1;
	}

	return false;
}

/*
 * @brief
 */
static void Ms_AddServer(struct sockaddr_in *from) {
	struct sockaddr_in addr;
	server_t *server = &servers;
	int32_t preserved_heartbeats = 0;

	if (Ms_GetServer(from)) {
		Com_Print("Duplicate ping from %s\n", inet_ntoa(from->sin_addr));
		return;
	}

	if (Ms_BlacklistServer(from)) {
		Com_Print("Server %s has been blacklisted\n", inet_ntoa(from->sin_addr));
		return;
	}

	while (server->next) // append to tail
		server = server->next;

	server->next = (server_t *) malloc(sizeof(server_t));

	server->next->prev = server;
	server = server->next;

	server->heartbeats = preserved_heartbeats;
	server->ip = *from;
	server->last_heartbeat = time(0);
	server->next = NULL;
	server->port = from->sin_port;
	server->queued_pings = server->last_ping = 0;
	server->validated = false;

	Com_Print("Server %s registered\n", inet_ntoa(from->sin_addr));

	// send an ack
	addr.sin_addr = server->ip.sin_addr;
	addr.sin_family = AF_INET;
	addr.sin_port = server->port;
	memset(&addr.sin_zero, 0, sizeof(addr.sin_zero));
	sendto(sock, "\xFF\xFF\xFF\xFF" "ack", 7, 0, (struct sockaddr*) &addr, sizeof(addr));
}

/*
 * @brief
 */
static void Ms_RemoveServer(struct sockaddr_in *from, server_t *server) {

	if (!server) // resolve from address
		server = Ms_GetServer(from);

	if (!server) {
		Com_Print("Shutdown from unregistered server %s\n", inet_ntoa(from->sin_addr));
		return;
	}

	Com_Print("Shutdown from %s\n", inet_ntoa(from->sin_addr));
	Ms_DropServer(server);
}

/*
 * @brief
 */
static void Ms_RunFrame(void) {
	server_t *server = &servers;
	uint32_t curtime = time(0);

	while (server->next) {
		server = server->next;
		if (curtime - server->last_heartbeat > 30) {
			server_t *old = server;

			server = old->prev;

			if (old->queued_pings > 6) {
				Com_Print("Server %s timed out.\n", inet_ntoa(old->ip.sin_addr));
				Ms_DropServer(old);
				continue;
			}

			server = old;
			if (curtime - server->last_ping >= 10) {
				struct sockaddr_in addr;
				addr.sin_addr = server->ip.sin_addr;
				addr.sin_family = AF_INET;
				addr.sin_port = server->port;
				memset(&addr.sin_zero, 0, sizeof(addr.sin_zero));
				server->queued_pings++;
				server->last_ping = curtime;
				Com_Print("Pinging %s\n", inet_ntoa(server->ip.sin_addr));
				sendto(sock, "\xFF\xFF\xFF\xFF" "ping", 8, 0, (struct sockaddr *) &addr,
						sizeof(addr));
			}
		}
	}
}

/*
 * @brief
 */
static void Ms_SendServersList(struct sockaddr_in *from) {
	int32_t buflen;
	char buff[0xffff];
	server_t *server = &servers;

	buflen = 0;
	memset(buff, 0, sizeof(buff));

	memcpy(buff, "\xFF\xFF\xFF\xFF""servers ", 12);
	buflen += 12;

	while (server->next) {
		server = server->next;
		if (server->validated) {
			memcpy(buff + buflen, &server->ip.sin_addr, 4);
			buflen += 4;
			memcpy(buff + buflen, &server->port, 2);
			buflen += 2;
		}
	}

	if ((sendto(sock, buff, buflen, 0, (struct sockaddr *) from, sizeof(*from))) == -1)
		Com_Print("Socket error on send: %s.\n", strerror(errno));
	else
		Com_Print("Sent server list to %s\n", inet_ntoa(from->sin_addr));
}

/*
 * @brief
 */
static void Ms_Ack(struct sockaddr_in *from) {
	server_t *server;

	if (!(server = Ms_GetServer(from)))
		return;

	Com_Print("Ack from %s (%d).\n", inet_ntoa(server->ip.sin_addr), server->queued_pings);

	server->last_heartbeat = time(0);
	server->queued_pings = 0;
	server->heartbeats++;
	server->validated = true;
}

/*
 * @brief
 */
static void Ms_Heartbeat(struct sockaddr_in *from) {
	server_t *server;
	struct sockaddr_in addr;

	if ((server = Ms_GetServer(from))) { // update their timestamp
		addr.sin_addr = server->ip.sin_addr;
		addr.sin_family = AF_INET;
		addr.sin_port = server->port;
		memset(&addr.sin_zero, 0, sizeof(addr.sin_zero));

		server->validated = true;
		server->last_heartbeat = time(0);

		Com_Print("Heartbeat from %s.\n", inet_ntoa(server->ip.sin_addr));

		sendto(sock, "\xFF\xFF\xFF\xFF" "ack", 7, 0, (struct sockaddr*) &addr, sizeof(addr));
		return;
	}

	Ms_AddServer(from);
}

/*
 * @brief
 */
static void Ms_ParseMessage(struct sockaddr_in *from, char *data) {
	char *cmd = data;
	char *line = data;

	while (*line && *line != '\n')
		line++;

	*(line++) = '\0';
	cmd += 4;

	if (!strncasecmp(cmd, "ping", 4)) {
		Ms_AddServer(from);
	} else if (!strncasecmp(cmd, "heartbeat", 9) || !strncasecmp(cmd, "print", 5)) {
		Ms_Heartbeat(from);
	} else if (!strncasecmp(cmd, "ack", 3)) {
		Ms_Ack(from);
	} else if (!strncasecmp(cmd, "shutdown", 8)) {
		Ms_RemoveServer(from, NULL);
	} else if (!strncasecmp(cmd, "getservers", 10) || !strncasecmp(cmd, "y", 1)) {
		Ms_SendServersList(from);
	} else {
		Com_Print("Unknown command from %s: '%s'", inet_ntoa(from->sin_addr), cmd);
	}
}

/*
 * @brief
 */
static void Ms_HandleSignal(int32_t sig) {

	Com_Print("Received signal %d, exiting...\n", sig);

	Ms_Shutdown();

	exit(0);
}

/*
 * @brief
 */
int32_t main(int32_t argc __attribute__((unused)), char **argv __attribute__((unused))) {
	struct sockaddr_in address, from;
	struct timeval delay;
	socklen_t fromlen;
	fd_set set;
	char buffer[16384];
	int32_t len;

	memset(&quake2world, 0, sizeof(quake2world));

	Com_Print("Quake2World Master Server %s\n", VERSION);

	// init core facilities
	Z_Init();

	Cvar_Init();

	Cmd_Init();

	Fs_Init();

	sock = socket(AF_INET, SOCK_DGRAM, IPPROTO_UDP);

	memset(&address, 0, sizeof(address));

	address.sin_family = AF_INET;
	address.sin_port = htons(PORT_MASTER);
	address.sin_addr.s_addr = INADDR_ANY;

	if ((bind(sock, (struct sockaddr *) &address, sizeof(address))) == -1) {
		Com_Print("Failed to bind port %i\n", PORT_MASTER);
		return 1;
	}

	memset(&servers, 0, sizeof(servers));

	Com_Print("Listening on %i\n", PORT_MASTER);

	signal(SIGHUP, Ms_HandleSignal);
	signal(SIGINT, Ms_HandleSignal);
	signal(SIGQUIT, Ms_HandleSignal);
	signal(SIGILL, Ms_HandleSignal);
	signal(SIGTRAP, Ms_HandleSignal);
	signal(SIGFPE, Ms_HandleSignal);
	signal(SIGSEGV, Ms_HandleSignal);
	signal(SIGTERM, Ms_HandleSignal);

	while (true) {
		FD_ZERO(&set);
		FD_SET(sock, &set);

		memset(&delay, 0, sizeof(struct timeval));
		delay.tv_sec = 1;
		delay.tv_usec = 0;

		if (select(sock + 1, &set, NULL, NULL, &delay) > 0) {
			if (FD_ISSET(sock, &set)) {

				fromlen = sizeof(from);
				memset(buffer, 0, sizeof(buffer));

				len
						= recvfrom(sock, buffer, sizeof(buffer), 0, (struct sockaddr *) &from,
								&fromlen);

				if (len > 0) {
					if (len > 4)
						Ms_ParseMessage(&from, buffer);
					else
						Com_Print("Invalid packet from %s:%d\n", inet_ntoa(from.sin_addr),
								ntohs(from.sin_port));
				} else {
					Com_Print("Socket error from %s:%d (%s)\n", inet_ntoa(from.sin_addr),
							ntohs(from.sin_port), strerror(errno));
				}
			}
		}

		Ms_RunFrame();
	}
}
