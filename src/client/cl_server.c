/*
 * Copyright(c) 1997-2001 Id Software, Inc.
 * Copyright(c) 2002 The Quakeforge Project.
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

#include "cl_local.h"

/*
 * Cl_AddServer
 */
static cl_server_info_t *Cl_AddServer(const net_addr_t *addr) {
	cl_server_info_t *s;

	s = (cl_server_info_t *) Z_Malloc(sizeof(*s));

	s->next = cls.servers;
	cls.servers = s;

	s->addr = *addr;
	strcpy(s->hostname, Net_NetaddrToString(s->addr));

	return s;
}

/*
 * Cl_ServerForNetaddr
 */
static cl_server_info_t *Cl_ServerForNetaddr(const net_addr_t *addr) {
	cl_server_info_t *s;

	s = cls.servers;

	while (s) {

		if (Net_CompareNetaddr(*addr, s->addr))
			return s;

		s = s->next;
	}

	return NULL;
}

/*
 * Cl_FreeServers
 */
void Cl_FreeServers(void) {
	cl_server_info_t *s, *next;

	s = cls.servers;

	while (s) {

		next = s->next;
		Z_Free(s);
		s = next;
	}
}

/*
 * Cl_ParseStatusMessage
 */
void Cl_ParseStatusMessage(void) {
	extern void Ui_NewServer(void);
	cl_server_info_t *server;
	char info[MAX_MSG_SIZE];

	server = Cl_ServerForNetaddr(&net_from);

	if (!server) { // unknown server, assumed response to broadcast

		server = Cl_AddServer(&net_from);

		server->source = SERVER_SOURCE_BCAST;
		server->ping_time = cls.broadcast_time;
	}

	// try to parse the info string
	strncpy(info, Msg_ReadString(&net_message), sizeof(info) - 1);
	info[sizeof(info) - 1] = '\0';
	if (sscanf(info, "%63c\\%31c\\%31c\\%hu\\%hu", server->hostname,
			server->name, server->gameplay, &server->clients,
			&server->max_clients) != 5) {

		strcpy(server->hostname, Net_NetaddrToString(server->addr));
		server->name[0] = '\0';
		server->gameplay[0] = '\0';
		server->clients = 0;
		server->max_clients = 0;
	}
	server->hostname[63] = '\0';
	server->name[31] = '\0';
	server->gameplay[31] = '\0';

	server->ping = cls.real_time - server->ping_time;

	if (server->ping > 1000) // clamp the ping
		server->ping = 999;

	Ui_NewServer();
}

/*
 * Cl_Ping_f
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

	if (!addr.port) // use default
		addr.port = (unsigned short) BigShort(PORT_SERVER);

	server = Cl_ServerForNetaddr(&addr);

	if (!server) { // add it
		server = Cl_AddServer(&addr);
		server->source = SERVER_SOURCE_USER;
	}

	server->ping_time = cls.real_time;
	server->ping = 0;

	Com_Print("Pinging %s\n", Net_NetaddrToString(server->addr));

	Netchan_OutOfBandPrint(NS_CLIENT, server->addr, "info %i", PROTOCOL);
}

/*
 * Cl_SendBroadcast
 */
static void Cl_SendBroadcast(void) {
	cl_server_info_t *server;
	net_addr_t addr;

	cls.broadcast_time = cls.real_time;

	server = cls.servers;

	while (server) { // update old pingtimes

		if (server->source == SERVER_SOURCE_BCAST) {
			server->ping_time = cls.broadcast_time;
			server->ping = 0;
		}

		server = server->next;
	}

	addr.type = NA_IP_BROADCAST;
	addr.port = (unsigned short) BigShort(PORT_SERVER);
	Netchan_OutOfBandPrint(NS_CLIENT, addr, "info %i", PROTOCOL);
}

/*
 * Cl_Servers_f
 */
void Cl_Servers_f(void) {
	net_addr_t addr;

	if (!Net_StringToNetaddr(IP_MASTER, &addr)) {
		Com_Print("Failed to resolve %s\n", IP_MASTER);
		return;
	}

	Com_Print("Refreshing servers.\n");

	addr.type = NA_IP;
	addr.port = (unsigned short) BigShort(PORT_MASTER);
	Netchan_OutOfBandPrint(NS_CLIENT, addr, "getservers");

	Cl_SendBroadcast();
}

/*
 * Cl_ParseServersList
 */
void Cl_ParseServersList(void) {
	byte *buffptr;
	byte *buffend;
	byte ip[4];
	unsigned short port;
	net_addr_t addr;
	cl_server_info_t *server;
	char s[32];

	buffptr = net_message.data + 12;
	buffend = buffptr + net_message.size - 12;

	// parse the list
	while (buffptr + 1 < buffend) {

		ip[0] = *buffptr++; // parse the address
		ip[1] = *buffptr++;
		ip[2] = *buffptr++;
		ip[3] = *buffptr++;

		port = (*buffptr++) << 8; // and the port
		port += *buffptr++;

		snprintf(s, sizeof(s), "%d.%d.%d.%d:%d", ip[0], ip[1], ip[2], ip[3], port);

		if (!Net_StringToNetaddr(s, &addr)) { // make sure it's valid
			Com_Warn("Cl_ParseServersList: Invalid address: %s.\n", s);
			break;
		}

		if (!addr.port) // 0's mean we're done
			break;

		server = Cl_ServerForNetaddr(&addr);

		if (!server)
			server = Cl_AddServer(&addr);

		server->source = SERVER_SOURCE_INTERNET;
	}

	net_message.read = net_message.size;

	// then ping them
	server = cls.servers;

	while (server) {

		if (server->source == SERVER_SOURCE_INTERNET) {
			server->ping_time = cls.real_time;
			server->ping = 0;

			Netchan_OutOfBandPrint(NS_CLIENT, server->addr, "info %i", PROTOCOL);
		}

		server = server->next;
	}
}
