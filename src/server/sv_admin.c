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

#include "sv_local.h"

/*
 *
 * OPERATOR CONSOLE ONLY COMMANDS
 *
 * These commands can only be entered from stdin or by a remote operator datagram
 */

/*
 * Sv_SetMaster_f
 */
static void Sv_SetMaster_f(void) {
	int i, slot;
	net_addr_t *addr;

	// only dedicated servers send heartbeats
	if (!dedicated->value) {
		Com_Print("Only dedicated servers use masters.\n");
		return;
	}

	// make sure the server is listed public
	Cvar_Set("public", "1");

	for (i = 1; i < MAX_MASTERS; i++) {
		addr = &svs.masters[i];
		memset(addr, 0, sizeof(*addr));
	}

	// the first slot will always contain the default master
	for (slot = i = 1; i < Cmd_Argc(); i++) {

		if (slot == MAX_MASTERS)
			break;

		addr = &svs.masters[slot];

		if (!Net_StringToNetaddr(Cmd_Argv(i), addr)) {
			Com_Print("Bad address: %s\n", Cmd_Argv(i));
			continue;
		}

		if (addr->port == 0)
			addr->port = BigShort(PORT_MASTER);

		Com_Print("Master server at %s\n", Net_NetaddrToString(*addr));
		Netchan_OutOfBandPrint(NS_SERVER, *addr, "ping");

		slot++;
	}

	svs.last_heartbeat = -9999999;
}

/*
 * Sv_Heartbeat_f
 */
static void Sv_Heartbeat_f(void) {
	svs.last_heartbeat = -9999999;
}

/*
 * Sv_SetPlayer
 *
 * Sets sv_client and sv_player to the player with idnum Cmd_Argv(1)
 */
static boolean_t Sv_SetPlayer(void) {
	sv_client_t *cl;
	int i;
	int idnum;
	char *s;

	if (Cmd_Argc() < 2)
		return false;

	s = Cmd_Argv(1);

	// numeric values are just slot numbers
	if (s[0] >= '0' && s[0] <= '9') {
		idnum = atoi(Cmd_Argv(1));
		if (idnum < 0 || idnum >= sv_max_clients->integer) {
			Com_Print("Bad client slot: %i\n", idnum);
			return false;
		}

		sv_client = &svs.clients[idnum];
		sv_player = sv_client->edict;
		if (!sv_client->state) {
			Com_Print("Client %i is not active\n", idnum);
			return false;
		}
		return true;
	}

	// check for a name match
	for (i = 0, cl = svs.clients; i < sv_max_clients->integer; i++, cl++) {

		if (!cl->state)
			continue;

		if (!strcmp(cl->name, s)) {
			sv_client = cl;
			sv_player = sv_client->edict;
			return true;
		}
	}

	Com_Print("Player %s is not on the server\n", s);
	return false;
}

/*
 * Sv_Demo_f
 *
 * Starts playback of the specified demo file.
 */
static void Sv_Demo_f(void) {

	if (Cmd_Argc() != 2) {
		Com_Print("Usage: %s <demo>\n", Cmd_Argv(0));
		return;
	}

	// start up the demo
	Sv_InitServer(Cmd_Argv(1), SV_ACTIVE_DEMO);
}

/*
 * Sv_Map_f
 *
 * Creates a server for the specified map.
 */
static void Sv_Map_f(void) {

	if (Cmd_Argc() != 2) {
		Com_Print("Usage: %s <map>\n", Cmd_Argv(0));
		return;
	}

	// start up the map
	Sv_InitServer(Cmd_Argv(1), SV_ACTIVE_GAME);
}

/*
 * Sv_Kick_f
 *
 * Kick a user off of the server
 */
static void Sv_Kick_f(void) {

	if (!svs.initialized) {
		Com_Print("No server running.\n");
		return;
	}

	if (Cmd_Argc() != 2) {
		Com_Print("Usage: %s <userid>\n", Cmd_Argv(0));
		return;
	}

	if (!Sv_SetPlayer())
		return;

	Sv_KickClient(sv_client, NULL);
}

/*
 * Sv_Status_f
 */
static void Sv_Status_f(void) {
	int i, j, l;
	sv_client_t *cl;
	char *s;
	unsigned int ping;

	if (!svs.initialized) {
		Com_Print("No server running.\n");
		return;
	}

	Com_Print("map: %s\n", sv.name);
	Com_Print(
			"num score ping name            lastmsg address               qport \n");
	Com_Print(
			"--- ----- ---- --------------- ------- --------------------- ------\n");
	for (i = 0, cl = svs.clients; i < sv_max_clients->integer; i++, cl++) {

		if (!cl->state)
			continue;

		Com_Print("%3i ", i);
		Com_Print("%5i ", cl->edict->client->ps.stats[STAT_FRAGS]);

		if (cl->state == SV_CLIENT_CONNECTED)
			Com_Print("CNCT ");
		else {
			ping = cl->ping < 9999 ? cl->ping : 9999;
			Com_Print("%4i ", ping);
		}

		Com_Print("%s", cl->name);
		l = 16 - strlen(cl->name);
		for (j = 0; j < l; j++)
			Com_Print(" ");

		Com_Print("%7i ", svs.real_time - cl->last_message);

		s = Net_NetaddrToString(cl->netchan.remote_address);
		Com_Print("%s", s);
		l = 22 - strlen(s);
		for (j = 0; j < l; j++)
			Com_Print(" ");

		Com_Print("%5i", (int)cl->netchan.qport);

		Com_Print("\n");
	}
}

/*
 * Sv_Say_f
 */
static void Sv_Say_f(void) {
	sv_client_t *client;
	int j;
	char *p;
	char text[1024];

	if (Cmd_Argc() < 2)
		return;

	strcpy(text, "console^1:^7 ");
	p = Cmd_Args();

	if (*p == '"') {
		p++;
		p[strlen(p) - 1] = 0;
	}

	strcat(text, p);

	for (j = 0, client = svs.clients; j < sv_max_clients->integer; j++, client++) {

		if (client->state != SV_CLIENT_ACTIVE)
			continue;

		Sv_ClientPrint(client->edict, PRINT_CHAT, "%s\n", text);
	}

	Com_Print("%s\n", text);
}

/*
 * Sv_Tell_f
 */
static void Sv_Tell_f(void) {
	char text[1024];
	char *p;

	if (Cmd_Argc() < 3)
		return;

	if (!Sv_SetPlayer())
		return;

	strcpy(text, "console^1:^7 ");
	p = Cmd_Args();
	p += strlen(Cmd_Argv(1)) + 1;
	if (*p == '"') {
		p++;
		p[strlen(p) - 1] = 0;
	}

	strcat(text, p);

	if (sv_client->state != SV_CLIENT_ACTIVE)
		return;

	Sv_ClientPrint(sv_client->edict, PRINT_CHAT, "%s\n", text);

	Com_Print("%s\n", text);
}

/*
 * Sv_Serverinfo_f
 */
static void Sv_Serverinfo_f(void) {

	if (!svs.initialized) {
		Com_Print("No server running.\n");
		return;
	}

	Com_Print("Server info settings:\n");
	Com_PrintInfo(Cvar_ServerInfo());
}

/*
 * Sv_UserInfo_f
 */
static void Sv_UserInfo_f(void) {

	if (!svs.initialized) {
		Com_Print("No server running.\n");
		return;
	}

	if (Cmd_Argc() != 2) {
		Com_Print("Usage: %s <userid>\n", Cmd_Argv(0));
		return;
	}

	if (!Sv_SetPlayer())
		return;

	Com_PrintInfo(sv_client->user_info);
}

/*
 * Sv_InitCommands
 */
void Sv_InitCommands(void) {

	Cmd_AddCommand("kick", Sv_Kick_f, "Kick a specific user");
	Cmd_AddCommand("status", Sv_Status_f,
			"Print some server status information");
	Cmd_AddCommand("serverinfo", Sv_Serverinfo_f, "Print server info settings");
	Cmd_AddCommand("userinfo", Sv_UserInfo_f,
			"Print information for a given user");

	Cmd_AddCommand("demo", Sv_Demo_f,
			"Start playback of the specified demo file");
	Cmd_AddCommand("map", Sv_Map_f, "Start a server for the specified map");

	Cmd_AddCommand("set_master", Sv_SetMaster_f,
			"Set the master server for the dedicated server");
	Cmd_AddCommand("heartbeat", Sv_Heartbeat_f,
			"Send a heartbeat to the master server");

	if (dedicated->value) {
		Cmd_AddCommand("say", Sv_Say_f, NULL);
		Cmd_AddCommand("tell", Sv_Tell_f, "Send a private message");
	}
}

