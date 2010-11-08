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

#include "server.h"

/*
 *
 * OPERATOR CONSOLE ONLY COMMANDS
 *
 * These commands can only be entered from stdin or by a remote operator datagram
 */

/*
 * Sv_SetMaster_f
 */
static void Sv_SetMaster_f(void){
	int i, slot;

	// only dedicated servers send heartbeats
	if(!dedicated->value){
		Com_Print("Only dedicated servers use masters.\n");
		return;
	}

	// make sure the server is listed public
	Cvar_Set("public", "1");

	for(i = 1; i < MAX_MASTERS; i++)
		memset(&master_addr[i], 0, sizeof(master_addr[i]));

	slot = 1;  // slot 0 will always contain the q2w master
	for(i = 1; i < Cmd_Argc(); i++){
		if(slot == MAX_MASTERS)
			break;

		if(!Net_StringToNetaddr(Cmd_Argv(i), &master_addr[i])){
			Com_Print("Bad address: %s\n", Cmd_Argv(i));
			continue;
		}
		if(master_addr[slot].port == 0)
			master_addr[slot].port = BigShort(PORT_MASTER);

		Com_Print("Master server at %s\n", Net_NetaddrToString(master_addr[slot]));
		Netchan_OutOfBandPrint(NS_SERVER, master_addr[slot], "ping");

		slot++;
	}

	svs.last_heartbeat = -9999999;
}


/*
 * Sv_Heartbeat_f
 */
static void Sv_Heartbeat_f(void){
	svs.last_heartbeat = -9999999;
}


/*
 * Sv_SetPlayer
 *
 * Sets sv_client and sv_player to the player with idnum Cmd_Argv(1)
 */
static qboolean Sv_SetPlayer(void){
	sv_client_t *cl;
	int i;
	int idnum;
	char *s;

	if(Cmd_Argc() < 2)
		return false;

	s = Cmd_Argv(1);

	// numeric values are just slot numbers
	if(s[0] >= '0' && s[0] <= '9'){
		idnum = atoi(Cmd_Argv(1));
		if(idnum < 0 || idnum >= sv_maxclients->value){
			Com_Print("Bad client slot: %i\n", idnum);
			return false;
		}

		sv_client = &svs.clients[idnum];
		sv_player = sv_client->edict;
		if(!sv_client->state){
			Com_Print("Client %i is not active\n", idnum);
			return false;
		}
		return true;
	}

	// check for a name match
	for(i = 0, cl = svs.clients; i < sv_maxclients->value; i++, cl++){
		if(!cl->state)
			continue;
		if(!strcmp(cl->name, s)){
			sv_client = cl;
			sv_player = sv_client->edict;
			return true;
		}
	}

	Com_Print("Userid %s is not on the server\n", s);
	return false;
}


/*
 * Sv_Demo_f
 *
 * Starts playback of the specified demo file.
 */
static void Sv_Demo_f(void){
	char demo[MAX_QPATH];

	if(Cmd_Argc() != 2){
		Com_Print("Usage: %s <demo>\n", Cmd_Argv(0));
		return;
	}

	// start up the demo
	snprintf(demo, sizeof(demo), "%s.dem", Cmd_Argv(1));
	Sv_Map(demo);
}


/*
 * Sv_Map_f
 */
static void Sv_Map_f(void){

	if(Cmd_Argc() != 2){
		Com_Print("Usage: %s <map>\n", Cmd_Argv(0));
		return;
	}

	// start up the map
	Sv_Map(Cmd_Argv(1));
}


/*
 * Sv_Kick_f
 *
 * Kick a user off of the server
 */
static void Sv_Kick_f(void){

	if(!svs.initialized){
		Com_Print("No server running.\n");
		return;
	}

	if(Cmd_Argc() != 2){
		Com_Print("Usage: %s <userid>\n", Cmd_Argv(0));
		return;
	}

	if(!Sv_SetPlayer())
		return;

	Sv_KickClient(sv_client, NULL);
}


/*
 * Sv_Status_f
 */
static void Sv_Status_f(void){
	int i, j, l;
	extern int zlib_accum;
	sv_client_t *cl;
	char *s;
	int ping;

	if(!svs.initialized){
		Com_Print("No server running.\n");
		return;
	}

	Com_Print("map: %s\n", sv.name);
	Com_Print("num score ping name            lastmsg exts address               qport \n");
	Com_Print("--- ----- ---- --------------- ------- ---- --------------------- ------\n");
	for(i = 0, cl = svs.clients; i < sv_maxclients->value; i++, cl++){
		if(!cl->state)
			continue;
		Com_Print("%3i ", i);
		Com_Print("%5i ", cl->edict->client->ps.stats[STAT_FRAGS]);

		if(cl->state == cs_connected)
			Com_Print("CNCT ");
		else {
			ping = cl->ping < 9999 ? cl->ping : 9999;
			Com_Print("%4i ", ping);
		}

		Com_Print("%s", cl->name);
		l = 16 - strlen(cl->name);
		for(j = 0; j < l; j++)
			Com_Print(" ");

		Com_Print("%7i ", svs.realtime - cl->lastmessage);
		Com_Print("%d   ", (int)cl->extensions);

		s = Net_NetaddrToString(cl->netchan.remote_address);
		Com_Print("%s", s);
		l = 22 - strlen(s);
		for(j = 0; j < l; j++)
			Com_Print(" ");

		Com_Print("%5i", cl->netchan.qport);

		Com_Print("\n");
	}
	Com_Print("Zlib: %d bytes saved\n", zlib_accum);
}


/*
 * Sv_Say_f
 */
static void Sv_Say_f(void){
	sv_client_t *client;
	int j;
	char *p;
	char text[1024];

	if(Cmd_Argc() < 2)
		return;

	strcpy(text, "console^1:^7 ");
	p = Cmd_Args();

	if(*p == '"'){
		p++;
		p[strlen(p) - 1] = 0;
	}

	strcat(text, p);

	for(j = 0, client = svs.clients; j < sv_maxclients->value; j++, client++){

		if(client->state != cs_spawned)
			continue;

		Sv_ClientPrint(EDICT_FOR_CLIENT(client), PRINT_CHAT, "%s\n", text);
	}

	Com_Print("%s\n", text);
}


/*
 * Sv_Serverinfo_f
 */
static void Sv_Serverinfo_f(void){

	if(!svs.initialized){
		Com_Print("No server running.\n");
		return;
	}

	Com_Print("Server info settings:\n");
	Com_PrintInfo(Cvar_Serverinfo());
}


/*
 * Sv_Userinfo_f
 */
static void Sv_Userinfo_f(void){

	if(!svs.initialized){
		Com_Print("No server running.\n");
		return;
	}

	if(Cmd_Argc() != 2){
		Com_Print("Usage: %s <userid>\n", Cmd_Argv(0));
		return;
	}

	if(!Sv_SetPlayer())
		return;

	Com_PrintInfo(sv_client->userinfo);
}


/*
 * Sv_InitOperatorCommands
 */
void Sv_InitOperatorCommands(void){

	Cmd_AddCommand("kick", Sv_Kick_f, "Kick a specific user");
	Cmd_AddCommand("status", Sv_Status_f, "Print some server status information");
	Cmd_AddCommand("serverinfo", Sv_Serverinfo_f, "Print server info settings");
	Cmd_AddCommand("userinfo", Sv_Userinfo_f, "Print information for a given user");

	Cmd_AddCommand("demo", Sv_Demo_f, "Start playback of the specified demo file");
	Cmd_AddCommand("map", Sv_Map_f, "Start a new map");

	Cmd_AddCommand("setmaster", Sv_SetMaster_f, "Set the masterserver for the dedicated server");
	Cmd_AddCommand("heartbeat", Sv_Heartbeat_f, "Send a heartbeat to the masterserver");

	if(dedicated->value)
		Cmd_AddCommand("say", Sv_Say_f, NULL);
}

