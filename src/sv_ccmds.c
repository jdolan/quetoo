/*
 * Copyright(c) 1997-2001 Id Software, Inc.
 * Copyright(c) 2002 The Quakeforge Project.
 * Copyright(c) 2006 Quake2World.
 *
 * This program is free software; you can redistribute it and/or
 * modify it under the terms of the GNU General Public License
 * as published by the Free Software Foundation; either version 2
 * of the License, or(at your option) any later version.
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
		Com_Printf("Only dedicated servers use masters.\n");
		return;
	}

	// make sure the server is listed public
	Cvar_Set("public", "1");

	for(i = 1; i < MAX_MASTERS; i++)
		memset(&master_adr[i], 0, sizeof(master_adr[i]));

	slot = 1;  // slot 0 will always contain the q2w master
	for(i = 1; i < Cmd_Argc(); i++){
		if(slot == MAX_MASTERS)
			break;

		if(!Net_StringToAdr(Cmd_Argv(i), &master_adr[i])){
			Com_Printf("Bad address: %s\n", Cmd_Argv(i));
			continue;
		}
		if(master_adr[slot].port == 0)
			master_adr[slot].port = BigShort(PORT_MASTER);

		Com_Printf("Master server at %s\n", Net_AdrToString(master_adr[slot]));
		Netchan_OutOfBandPrint(NS_SERVER, master_adr[slot], "ping");

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
	client_t *cl;
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
			Com_Printf("Bad client slot: %i\n", idnum);
			return false;
		}

		sv_client = &svs.clients[idnum];
		sv_player = sv_client->edict;
		if(!sv_client->state){
			Com_Printf("Client %i is not active\n", idnum);
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

	Com_Printf("Userid %s is not on the server\n", s);
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
		Com_Printf("Usage: %s <demo>\n", Cmd_Argv(0));
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
		Com_Printf("Usage: %s <map>\n", Cmd_Argv(0));
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
		Com_Printf("No server running.\n");
		return;
	}

	if(Cmd_Argc() != 2){
		Com_Printf("Usage: %s <userid>\n", Cmd_Argv(0));
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
	client_t *cl;
	char *s;
	int ping;

	if(!svs.initialized){
		Com_Printf("No server running.\n");
		return;
	}

	Com_Printf("map: %s\n", sv.name);
	Com_Printf("num score ping name            lastmsg exts address               qport \n");
	Com_Printf("--- ----- ---- --------------- ------- ---- --------------------- ------\n");
	for(i = 0, cl = svs.clients; i < sv_maxclients->value; i++, cl++){
		if(!cl->state)
			continue;
		Com_Printf("%3i ", i);
		Com_Printf("%5i ", cl->edict->client->ps.stats[STAT_FRAGS]);

		if(cl->state == cs_connected)
			Com_Printf("CNCT ");
		else {
			ping = cl->ping < 9999 ? cl->ping : 9999;
			Com_Printf("%4i ", ping);
		}

		Com_Printf("%s", cl->name);
		l = 16 - strlen(cl->name);
		for(j = 0; j < l; j++)
			Com_Printf(" ");

		Com_Printf("%7i ", svs.realtime - cl->lastmessage);
		Com_Printf("%d   ", (int)cl->extensions);

		s = Net_AdrToString(cl->netchan.remote_address);
		Com_Printf("%s", s);
		l = 22 - strlen(s);
		for(j = 0; j < l; j++)
			Com_Printf(" ");

		Com_Printf("%5i", cl->netchan.qport);

		Com_Printf("\n");
	}
	Com_Printf("Zlib: %d bytes saved\n", zlib_accum);
}


/*
 * Sv_Say_f
 */
static void Sv_Say_f(void){
	client_t *client;
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
		Sv_ClientPrintf(client, PRINT_CHAT, "%s\n", text);
	}

	Com_Printf("%s\n", text);
}


/*
 * Sv_Serverinfo_f
 */
static void Sv_Serverinfo_f(void){

	if(!svs.initialized){
		Com_Printf("No server running.\n");
		return;
	}

	Com_Printf("Server info settings:\n");
	Info_Print(Cvar_Serverinfo());
}


/*
 * Sv_Userinfo_f
 */
static void Sv_Userinfo_f(void){

	if(!svs.initialized){
		Com_Printf("No server running.\n");
		return;
	}

	if(Cmd_Argc() != 2){
		Com_Printf("Usage: %s <userid>\n", Cmd_Argv(0));
		return;
	}

	if(!Sv_SetPlayer())
		return;

	Info_Print(sv_client->userinfo);
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

