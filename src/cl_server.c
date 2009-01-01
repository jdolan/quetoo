/*
 * Copyright(c) 1997-2001 Id Software, Inc.
 * Copyright(c) 2002 The Quakeforge Project.
 * Copyright(c) 2006 Quake2World.
 * *
 * This program is free software; you can redistribute it and/or
 * modify it under the terms of the GNU General Public License
 * as published by the Free Software Foundation; either version 2
 * of the License, or(at your option) any later version.
 * *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.
 * *
 * See the GNU General Public License for more details.
 * *
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, write to the Free Software
 * Foundation, Inc., 59 Temple Place - Suite 330, Boston, MA  02111-1307, USA.
 */

#include "client.h"


/*
 * Cl_AddServer
 */
static server_info_t *Cl_AddServer(const netadr_t *adr){
	server_info_t *s;

	s = (server_info_t *)Z_Malloc(sizeof(*s));

	s->next = cls.servers;
	cls.servers = s;

	s->adr = *adr;
	s->num = cls.num_servers++;

	return s;
}


/*
 * Cl_ServerForAdr
 */
static server_info_t *Cl_ServerForAdr(const netadr_t *adr){
	server_info_t *s;

	s = cls.servers;

	while(s){

		if(Net_CompareAdr(*adr, s->adr))
			return s;

		s = s->next;
	}

	return NULL;
}


/*
 * Cl_ServerForNum
 */
server_info_t *Cl_ServerForNum(int num){
	server_info_t *s;

	s = cls.servers;

	while(s){

		if(s->num == num)
			return s;

		s = s->next;
	}

	return NULL;
}


/*
 * Cl_FreeServers
 */
void Cl_FreeServers(void){
	server_info_t *s, *next;

	s = cls.servers;

	while(s){

		next = s->next;
		Z_Free(s);
		s = next;
	}
}


/*
 * Cl_ServerSourceName
 */
static inline char *Cl_ServerSourceName(server_source_t source){

	switch(source){
		case SERVER_SOURCE_INTERNET:
			return "Internet";
		case SERVER_SOURCE_USER:
			return "User";
		case SERVER_SOURCE_BCAST:
			return "LAN";
		default:
			return "";
	}
}


/*
 * Cl_ParseStatusMessage
 */
void Cl_ParseStatusMessage(void){
	server_info_t *server;
	char *c;
	const char *source;

	server = Cl_ServerForAdr(&net_from);

	if(!server){  // unknown server, assumed response to broadcast
		server = Cl_AddServer(&net_from);

		server->source = SERVER_SOURCE_BCAST;
		server->pingtime = cls.bcasttime;
	}

	source = Cl_ServerSourceName(server->source);

	strncpy(server->info, Msg_ReadString(&net_message), sizeof(server->info) - 1);

	if((c = strstr(server->info, "\n")))
		*c = '\0';

	server->ping = cls.realtime - server->pingtime;

	if(server->ping > 1000)  // clamp the ping
		server->ping = 999;

	Com_Printf("%02d. %8s %s  %3dms\n", server->num + 1, source, server->info, server->ping);
}


/*
 * Cl_Ping_f
 */
void Cl_Ping_f(void){
	netadr_t adr;
	server_info_t *server;
	int i;

	if(Cmd_Argc() != 2){
		Com_Printf("Usage: %s <address|num>\n", Cmd_Argv(0));
		return;
	}

	server = NULL;

	i = atoi(Cmd_Argv(1));
	if(i > 0 && !strchr(Cmd_Argv(1), '.')){  // try numeric reference

		server = Cl_ServerForNum(i - 1);

		if(!server){
			Com_Printf("Invalid server number\n");
			return;
		}
	}
	else {  // try hostname or ip
		if(!Net_StringToAdr(Cmd_Argv(1), &adr)){
			Com_Printf("Invalid address\n");
			return;
		}

		if(!adr.port)  // use default
			adr.port = (unsigned short)BigShort(PORT_SERVER);

		server = Cl_ServerForAdr(&adr);

		if(!server){  // add it
			server = Cl_AddServer(&adr);
			server->source = SERVER_SOURCE_USER;
		}
	}

	server->pingtime = cls.realtime;

	Com_Printf("Pinging %s\n", Net_AdrToString(server->adr));
	Com_Printf("#   Source   Hostname                 Map          Clients Ping\n");
	Com_Printf("----------------------------------------------------------------\n");

	Netchan_OutOfBandPrint(NS_CLIENT, server->adr, "info %i", PROTOCOL);
}


/*
 * Cl_SendBroadcast
 */
static void Cl_SendBroadcast(void){
	server_info_t *server;
	netadr_t adr;

	cls.bcasttime = cls.realtime;

	server = cls.servers;

	while(server){  // update old pingtimes

		if(server->source == SERVER_SOURCE_BCAST)
			server->pingtime = cls.bcasttime;

		server = server->next;
	}

	adr.type = NA_IP_BROADCAST;
	adr.port = (unsigned short)BigShort(PORT_SERVER);
	Netchan_OutOfBandPrint(NS_CLIENT, adr, "info %i", PROTOCOL);
}


/*
 * Cl_Servers_f
 */
void Cl_Servers_f(void){
	netadr_t adr;

	if(!Net_StringToAdr(IP_MASTER, &adr)){
		Com_Printf("Failed to resolve %s\n", IP_MASTER);
		return;
	}

	Com_Printf("Refreshing servers..\n");
	Com_Printf("#   Source   Hostname                 Map          Clients Ping\n");
	Com_Printf("----------------------------------------------------------------\n");

	adr.type = NA_IP;
	adr.port = (unsigned short)BigShort(PORT_MASTER);
	Netchan_OutOfBandPrint(NS_CLIENT, adr, "getservers");

	Cl_SendBroadcast();
}


/*
 * Cl_ParseServersList
 */
void Cl_ParseServersList(void){
	byte *buffptr;
	byte *buffend;
	byte ip[4];
	unsigned short port;
	netadr_t adr;
	server_info_t *server;
	char s[32];

	buffptr = net_message.data + 12;
	buffend = buffptr + net_message.cursize;

	// parse the list
	while(buffptr + 1 < buffend){

		ip[0] = *buffptr++;  // parse the address
		ip[1] = *buffptr++;
		ip[2] = *buffptr++;
		ip[3] = *buffptr++;

		port = (*buffptr++) << 8;  // and the port
		port += *buffptr++;

		snprintf(s, sizeof(s), "%d.%d.%d.%d:%d", ip[0], ip[1], ip[2], ip[3], port);

		if(!Net_StringToAdr(s, &adr)){  // make sure it's valid
			Com_Warn("Cl_ParseServersList: Invalid address: %s.\n", s);
			break;
		}

		if(!adr.port)  // 0's mean we're done
			break;

		server = Cl_ServerForAdr(&adr);

		if(!server)
			server = Cl_AddServer(&adr);

		server->source = SERVER_SOURCE_INTERNET;
	}

	net_message.readcount = net_message.cursize;

	// then ping them
	server = cls.servers;

	while(server){

		if(server->source == SERVER_SOURCE_INTERNET){
			server->pingtime = cls.realtime;
			Netchan_OutOfBandPrint(NS_CLIENT, server->adr, "info %i", PROTOCOL);
		}

		server = server->next;
	}
}
