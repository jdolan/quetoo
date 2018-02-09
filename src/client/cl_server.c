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
 * @brief
 */
static cl_server_info_t *Cl_AddServer(const net_addr_t *addr) {
	cl_server_info_t *s;

	s = (cl_server_info_t *) Mem_TagMalloc(sizeof(*s), MEM_TAG_CLIENT);

	s->addr = *addr;
	g_strlcpy(s->hostname, Net_NetaddrToString(&s->addr), sizeof(s->hostname));

	cls.servers = g_list_prepend(cls.servers, s);

	return s;
}

/**
 * @brief
 */
static cl_server_info_t *Cl_ServerForNetaddr(const net_addr_t *addr) {
	const GList *e = cls.servers;

	while (e) {
		cl_server_info_t *s = (cl_server_info_t *) e->data;

		if (Net_CompareNetaddr(addr, &s->addr)) {
			return s;
		}

		e = e->next;
	}

	return NULL;
}

/**
 * @brief
 */
void Cl_FreeServers(void) {

	g_list_free_full(cls.servers, Mem_Free);

	cls.servers = NULL;
}

/**
 * @brief
 */
void Cl_ParseServerInfo(void) {
	char info[MAX_MSG_SIZE];

	cl_server_info_t *server = Cl_ServerForNetaddr(&net_from);
	if (!server) { // unknown server, assumed response to broadcast

		server = Cl_AddServer(&net_from);

		server->source = SERVER_SOURCE_BCAST;
		server->ping_time = cls.broadcast_time;
	}

	// try to parse the info string
	g_strlcpy(info, Net_ReadString(&net_message), sizeof(info));
	if (sscanf(info, "%63c\\%31c\\%31c\\%hu\\%hu", server->hostname, server->name,
	           server->gameplay, &server->clients, &server->max_clients) != 5) {

		Com_Debug(DEBUG_CLIENT, "Failed to parse info \"%s\" for %s\n", info, Net_NetaddrToString(&server->addr));

		server->hostname[0] = '\0';
		server->name[0] = '\0';
		server->gameplay[0] = '\0';
		server->clients = 0;
		server->max_clients = 0;

		return;
	}

	g_strchomp(server->hostname);
	g_strchomp(server->name);
	g_strchomp(server->gameplay);

	server->hostname[sizeof(server->hostname) - 1] = '\0';
	server->name[sizeof(server->name) - 1] = '\0';
	server->gameplay[sizeof(server->name) - 1] = '\0';

	server->ping = Clamp(quetoo.ticks - server->ping_time, 1u, 999u);

	MVC_PostNotification(&(const Notification) {
		.name = NOTIFICATION_SERVER_PARSED,
		.data = server
	});
}

/**
 * @brief
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

	Netchan_OutOfBandPrint(NS_UDP_CLIENT, &server->addr, "info %i", PROTOCOL_MAJOR);
}

/**
 * @brief
 */
static void Cl_SendBroadcast(void) {
	const GList *e = cls.servers;


	while (e) { // update old ping times
		cl_server_info_t *s = (cl_server_info_t *) e->data;

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

	Netchan_OutOfBandPrint(NS_UDP_CLIENT, &addr, "info %i", PROTOCOL_MAJOR);

	cls.broadcast_time = quetoo.ticks;
}

/**
 * @brief
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

	Netchan_OutOfBandPrint(NS_UDP_CLIENT, &addr, "getservers");

	Cl_SendBroadcast();
}

/**
 * @brief
 */
void Cl_ParseServers(void) {
	cl_server_info_t *server;

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
		g_snprintf(s, sizeof(s), "%d.%d.%d.%d:%d", ip[0], ip[1], ip[2], ip[3], port);

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

	GList *e = cls.servers;

	while (e) {
		server = (cl_server_info_t *) e->data;

		if (server->source == SERVER_SOURCE_INTERNET) {
			server->ping_time = quetoo.ticks;
			server->ping = 0;

			Netchan_OutOfBandPrint(NS_UDP_CLIENT, &server->addr, "info %i", PROTOCOL_MAJOR);
		}

		e = e->next;
	}

	// and inform the user interface

	MVC_PostNotification(&(const Notification) {
		.name = NOTIFICATION_SERVER_PARSED
	});
}

/**
 * @brief
 */
void Cl_Servers_List_f(void) {
	char string[256];

	GList *e = cls.servers;

	while (e) {
		const cl_server_info_t *s = (cl_server_info_t *) e->data;

		g_snprintf(string, sizeof(string), "%-40.40s %-20.20s %-16.16s %-24.24s %02d/%02d %5dms",
		           s->hostname, Net_NetaddrToString(&s->addr), s->name, s->gameplay, s->clients,
		           s->max_clients, s->ping);
		Com_Print("%s\n", string);
		e = e->next;
	}
}
