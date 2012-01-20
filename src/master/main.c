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

#include "common.h"

typedef struct server_s {
	struct server_s *prev;
	struct server_s *next;
	struct sockaddr_in ip;
	unsigned short port;
	unsigned int queued_pings;
	unsigned int heartbeats;
	unsigned long last_heartbeat;
	unsigned long last_ping;
	unsigned char validated;
} server_t;

static server_t servers;
int sock;

/*
 * Ms_Print
 */
static void Ms_Print(const char *msg, ...) {
	va_list args;
	char text[1024];

	va_start(args, msg);
	vsnprintf(text, sizeof(text), msg, args);
	va_end(args);

	text[sizeof(text) - 1] = 0;
	printf("%s", text);
}

/*
 * Ms_Shutdown
 */
static void Ms_Shutdown(void) {
	server_t *server = &servers;
	server_t *old = NULL;

	Ms_Print("Master server shutting down.\n");

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
 * Ms_GetServer
 */
static server_t *Ms_GetServer(struct sockaddr_in *from) {
	server_t *server = &servers;

	while (server->next) {

		server = server->next;

		if (*(int *) &from->sin_addr == *(int *) &server->ip.sin_addr
				&& from->sin_port == server->port) {
			return server;
		}
	}

	return NULL;
}

/*
 * Ms_DropServer
 */
static void Ms_DropServer(server_t *server) {

	if (server->next)
		server->next->prev = server->prev;

	if (server->prev)
		server->prev->next = server->next;

	free(server);
}

/*
 * Ms_AddServer
 */
static void Ms_AddServer(struct sockaddr_in *from) {
	struct sockaddr_in addr;
	server_t *server = &servers;
	int preserved_heartbeats = 0;

	if (Ms_GetServer(from)) {
		Ms_Print("Duplicate ping from %s\n", inet_ntoa(from->sin_addr));
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
	server->queued_pings = server->last_ping = server->validated = 0;

	Ms_Print("Server %s registered\n", inet_ntoa(from->sin_addr));

	// send an ack
	addr.sin_addr = server->ip.sin_addr;
	addr.sin_family = AF_INET;
	addr.sin_port = server->port;
	memset(&addr.sin_zero, 0, sizeof(addr.sin_zero));
	sendto(sock, "\xFF\xFF\xFF\xFF" "ack", 7, 0, (struct sockaddr*) &addr,
			sizeof(addr));
}

/*
 * Ms_RemoveServer
 */
static void Ms_RemoveServer(struct sockaddr_in *from, server_t *server) {

	if (!server) // resolve from address
		server = Ms_GetServer(from);

	if (!server) {
		Ms_Print("Shutdown from unregistered server %s\n",
				inet_ntoa(from->sin_addr));
		return;
	}

	Ms_Print("Shutdown from %s\n", inet_ntoa(from->sin_addr));
	Ms_DropServer(server);
}

/*
 * Ms_RunFrame
 */
static void Ms_RunFrame(void) {
	server_t *server = &servers;
	unsigned int curtime = time(0);

	while (server->next) {
		server = server->next;
		if (curtime - server->last_heartbeat > 30) {
			server_t *old = server;

			server = old->prev;

			if (old->queued_pings > 6) {
				Ms_Print("Server %s timed out.\n", inet_ntoa(old->ip.sin_addr));
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
				Ms_Print("Pinging %s\n", inet_ntoa(server->ip.sin_addr));
				sendto(sock, "\xFF\xFF\xFF\xFF" "ping", 8, 0,
						(struct sockaddr *) &addr, sizeof(addr));
			}
		}
	}
}

/*
 * Ms_SendServersList
 */
static void Ms_SendServersList(struct sockaddr_in *from) {
	int buflen;
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

	if ((sendto(sock, buff, buflen, 0, (struct sockaddr *) from, sizeof(*from)))
			== -1)
		Ms_Print("Socket error on send: %d.\n", strerror(errno));
	else
		Ms_Print("Sent server list to %s\n", inet_ntoa(from->sin_addr));
}

/*
 * Ms_Ack
 */
static void Ms_Ack(struct sockaddr_in *from) {
	server_t *server;

	if (!(server = Ms_GetServer(from)))
		return;

	Ms_Print("Ack from %s (%d).\n", inet_ntoa(server->ip.sin_addr),
			server->queued_pings);

	server->last_heartbeat = time(0);
	server->queued_pings = 0;
	server->heartbeats++;
	server->validated = 1;
}

/*
 * Ms_Heartbeat
 */
static void Ms_Heartbeat(struct sockaddr_in *from, char *data) {
	server_t *server;
	struct sockaddr_in addr;

	if ((server = Ms_GetServer(from))) { // update their timestamp
		addr.sin_addr = server->ip.sin_addr;
		addr.sin_family = AF_INET;
		addr.sin_port = server->port;
		memset(&addr.sin_zero, 0, sizeof(addr.sin_zero));

		server->validated = 1;
		server->last_heartbeat = time(0);

		Ms_Print("Heartbeat from %s.\n", inet_ntoa(server->ip.sin_addr));

		sendto(sock, "\xFF\xFF\xFF\xFF" "ack", 7, 0, (struct sockaddr*) &addr,
				sizeof(addr));
		return;
	}

	Ms_AddServer(from);
}

/*
 * Ms_ParseMessage
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
	} else if (!strncasecmp(cmd, "heartbeat", 9) || !strncasecmp(cmd, "print",
			5)) {
		Ms_Heartbeat(from, line);
	} else if (!strncasecmp(cmd, "ack", 3)) {
		Ms_Ack(from);
	} else if (!strncasecmp(cmd, "shutdown", 8)) {
		Ms_RemoveServer(from, NULL);
	} else if (!strncasecmp(cmd, "getservers", 10) || !strncasecmp(cmd, "y", 1)) {
		Ms_SendServersList(from);
	} else {
		Ms_Print("Unknown command from %s: '%s'", inet_ntoa(from->sin_addr),
				cmd);
	}
}

/*
 * Ms_HandleSignal
 */
static void Ms_HandleSignal(int sig) {

	Ms_Print("Received signal %d, exiting...\n", sig);

	Ms_Shutdown();

	exit(0);
}

/*
 * main
 */
int main(int argc, char **argv) {
	struct sockaddr_in address, from;
	struct timeval delay;
	socklen_t fromlen;
	fd_set set;
	char buffer[16384];
	int len;

	Ms_Print("Quake2World Master Server %s\n", VERSION);

	sock = socket(AF_INET, SOCK_DGRAM, IPPROTO_UDP);

	memset(&address, 0, sizeof(address));

	address.sin_family = AF_INET;
	address.sin_port = htons(PORT_MASTER);
	address.sin_addr.s_addr = INADDR_ANY;

	if ((bind(sock, (struct sockaddr *) &address, sizeof(address))) == -1) {
		Ms_Print("Failed to bind port %i\n", PORT_MASTER);
		return 1;
	}

	memset(&servers, 0, sizeof(servers));

	Ms_Print("Listening on %i\n", PORT_MASTER);

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

				len = recvfrom(sock, buffer, sizeof(buffer), 0,
						(struct sockaddr *) &from, &fromlen);

				if (len > 0) {
					if (len > 4)
						Ms_ParseMessage(&from, buffer);
					else
						Ms_Print("Invalid packet from %s:%d\n",
								inet_ntoa(from.sin_addr), ntohs(from.sin_port));
				} else {
					Ms_Print("Socket error from %s:%d (%d)\n",
							inet_ntoa(from.sin_addr), ntohs(from.sin_port),
							strerror(errno));
				}
			}
		}

		Ms_RunFrame();
	}
}
