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
	#include <winsock2.h> // for htons
#endif

#include "sv_local.h"

/**
 * @brief
 */
static void Sv_SetMaster_f(void) {
	int32_t i, slot;
	net_addr_t *addr;

	// only dedicated servers send heartbeats
	if (!dedicated->value) {
		Com_Print("Only dedicated servers use masters\n");
		return;
	}

	// make sure the server is listed public
	Cvar_ForceSetInteger("public", 1);

	for (i = 1; i < MAX_MASTERS; i++) {
		addr = &svs.masters[i];
		memset(addr, 0, sizeof(*addr));
	}

	// the first slot will always contain the default master
	for (slot = i = 1; i < Cmd_Argc(); i++) {

		if (slot == MAX_MASTERS) {
			break;
		}

		addr = &svs.masters[slot];

		if (!Net_StringToNetaddr(Cmd_Argv(i), addr)) {
			Com_Print("Bad address: %s\n", Cmd_Argv(i));
			continue;
		}

		if (addr->port == 0) {
			addr->port = htons(PORT_MASTER);
		}

		Com_Print("Master server at %s\n", Net_NetaddrToString(addr));
		Netchan_OutOfBandPrint(NS_UDP_SERVER, addr, "ping");

		slot++;
	}

	svs.next_heartbeat = 0;
}

/**
 * @brief
 */
static void Sv_Heartbeat_f(void) {
	svs.next_heartbeat = 0;
}

/**
 * @brief Sets sv_client and sv_player to the player identified by Cmd_Argv(1).
 */
static _Bool Sv_SetPlayer(void) {
	sv_client_t *cl;
	int32_t i;

	if (Cmd_Argc() < 2) {
		return false;
	}

	const char *s = Cmd_Argv(1);

	// numeric values are just slot numbers
	if (s[0] >= '0' && s[0] <= '9') {
		const int32_t idnum = atoi(Cmd_Argv(1));
		if (idnum < 0 || idnum >= sv_max_clients->integer) {
			Com_Print("Bad client slot: %i\n", idnum);
			return false;
		}

		sv_client = &svs.clients[idnum];
		if (!sv_client->state) {
			Com_Print("Client %i is not active\n", idnum);
			return false;
		}
		return true;
	}

	// check for a name match
	for (i = 0, cl = svs.clients; i < sv_max_clients->integer; i++, cl++) {

		if (!cl->state) {
			continue;
		}

		if (!g_strcmp0(cl->name, s)) {
			sv_client = cl;
			return true;
		}
	}

	Com_Print("Player %s is not on the server\n", s);
	return false;
}

/**
 * @brief Demo command autocompletion.
 */
static void Sv_Demo_Autocomplete_f(const uint32_t argi, GList **matches) {
	const char *pattern = va("demos/%s*.demo", Cmd_Argv(argi));
	Fs_CompleteFile(pattern, matches);
}

/**
 * @brief Starts playback of the specified demo file.
 */
static void Sv_Demo_f(void) {

	if (Cmd_Argc() != 2) {
		Com_Print("Usage: %s <demo>\n", Cmd_Argv(0));
		return;
	}

	// start up the demo
	Sv_InitServer(Cmd_Argv(1), SV_ACTIVE_DEMO);
}

/**
 * @brief Map command autocompletion.
 */
static void Sv_Map_Autocomplete_f(const uint32_t argi, GList **matches) {
	const char *pattern = va("maps/%s*.bsp", Cmd_Argv(argi));
	Fs_CompleteFile(pattern, matches);
}

/**
 * @brief Creates a server for the specified map.
 */
static void Sv_Map_f(void) {

	if (Cmd_Argc() != 2) {
		Com_Print("Usage: %s <map>\n", Cmd_Argv(0));
		return;
	}

	// start up the map
	Sv_InitServer(Cmd_Argv(1), SV_ACTIVE_GAME);
}

/**
 * @brief Kick a user off of the server
 */
static void Sv_Kick_f(void) {

	if (!svs.initialized) {
		Com_Print("No server running\n");
		return;
	}

	if (Cmd_Argc() != 2) {
		Com_Print("Usage: %s <userid>\n", Cmd_Argv(0));
		return;
	}

	if (!Sv_SetPlayer()) {
		return;
	}

	Sv_KickClient(sv_client, NULL);
}

/**
 * @brief
 */
static void Sv_Status_f(void) {

	if (!svs.initialized) {
		Com_Print("No server running\n");
		return;
	}

	Com_Print("map: %s\n", sv.name);
	Com_Print("num ping name             lastmsg address               qport\n");
	Com_Print("--- ---- ---------------- ------- --------------------- -----\n");

	sv_client_t *cl = svs.clients;
	for (int32_t i = 0; i < sv_max_clients->integer; i++, cl++) {

		if (cl->state == SV_CLIENT_FREE) {
			continue;
		}

		const uint32_t ping = cl->entity->client->ping < 9999 ? cl->entity->client->ping : 9999;

		char status[MAX_STRING_CHARS];
		g_snprintf(status, sizeof(status), "%3d %4d %16s %7d %22s %3d",
		           i,
		           ping,
		           cl->name,
		           quetoo.ticks - cl->last_message,
		           Net_NetaddrToString(&(cl->net_chan.remote_address)),
		           cl->net_chan.qport);

		Com_Print("%s\n", status);
	}
}

/**
 * @brief Lists all entities currently in use.
 */
static void Sv_ListEntities_f(void) {
	uint16_t i;

	if (!svs.initialized) {
		Com_Print("No server running\n");
		return;
	}

	for (i = 0; i < svs.game->num_entities; i++) {
		const g_entity_t *e = ENTITY_FOR_NUM(i);

		if (Cmd_Argc() > 1) {
			if (!GlobMatch(Cmd_Argv(1), e->class_name, GLOB_FLAGS_NONE)) {
				continue;
			}
		}

		Com_Print("%s\n", etos(e));
	}
}

/**
 * @brief
 */
static void Sv_Say_f(void) {
	char text[MAX_STRING_CHARS];
	int32_t i;

	if (Cmd_Argc() < 2) {
		return;
	}

	StripColors(Cmd_Args(), text);
	if (!strlen(text)) {
		return;
	}

	g_strlcpy(text, Cmd_Args(), sizeof(text));
	char *s = text;

	if (s[0] == '"' && s[strlen(s) - 1] == '"') {
		s[strlen(s) - 1] = '\0';
		s++;
	}

	const sv_client_t *client = svs.clients;
	for (i = 0; i < sv_max_clients->integer; i++, client++) {

		if (client->state != SV_CLIENT_ACTIVE) {
			continue;
		}

		Sv_ClientPrint(client->entity, PRINT_CHAT, "^1console^%d: %s\n", CON_COLOR_CHAT, s);
	}

	Com_Print("^1console^%d: %s\n", CON_COLOR_CHAT, s);
}

/**
 * @brief
 */
static void Sv_Tell_f(void) {
	char text[MAX_STRING_CHARS];

	if (Cmd_Argc() < 3) {
		return;
	}

	if (!Sv_SetPlayer()) {
		return;
	}

	const char *msg = Cmd_Args() + strlen(Cmd_Argv(1)) + 1;
	StripColors(msg, text);
	if (!strlen(text)) {
		return;
	}

	g_strlcpy(text, msg, sizeof(text));
	char *s = text;

	if (s[0] == '"' && s[strlen(s) - 1] == '"') {
		s[strlen(s) - 1] = '\0';
		s++;
	}

	if (sv_client->state != SV_CLIENT_ACTIVE) {
		return;
	}

	Sv_ClientPrint(sv_client->entity, PRINT_CHAT, "^1console^%d: %s\n", CON_COLOR_TEAMCHAT, s);
	Com_Print("^1console^%d: %s\n", CON_COLOR_TEAMCHAT, s);
}

/**
 * @brief
 */
static void Sv_ServerInfo_f(void) {

	if (!svs.initialized) {
		Com_Print("No server running\n");
		return;
	}

	Com_Print("Server info settings:\n");
	Com_PrintInfo(Cvar_ServerInfo());
}

/**
 * @brief
 */
static void Sv_UserInfo_f(void) {

	if (!svs.initialized) {
		Com_Print("No server running\n");
		return;
	}

	if (Cmd_Argc() != 2) {
		Com_Print("Usage: %s <userid>\n", Cmd_Argv(0));
		return;
	}

	if (!Sv_SetPlayer()) {
		return;
	}

	Com_PrintInfo(sv_client->user_info);
}

/**
 * @brief Force a client-side command. Only available for dedicated servers.
 *  Ex: /stuff <clientid/name> command args
 */
static void Sv_Stuff_f(void) {
	char text[MAX_STRING_CHARS];
	int32_t i;

	if (Cmd_Argc() < 3) {
		return;
	}

	if (!Sv_SetPlayer()) {
		return;
	}

	if (sv_client->state != SV_CLIENT_ACTIVE) {
		return;
	}

	strcpy(text, Cmd_Argv(2));
	for (i = 3; i <= Cmd_Argc(); i++) {
		g_strlcat(text, " ", sizeof(text));
		g_strlcat(text, Cmd_Argv(i), sizeof(text));
	}

	Net_WriteByte(&sv_client->net_chan.message, SV_CMD_CBUF_TEXT);
	Net_WriteString(&sv_client->net_chan.message, va("%s\n", text));
}

/**
 * @brief
 */
void Sv_InitAdmin(void) {

	Cmd_Add("kick", Sv_Kick_f, CMD_SERVER, "Kick a specific user");
	Cmd_Add("status", Sv_Status_f, CMD_SERVER, "Print server status information");
	Cmd_Add("list_entities", Sv_ListEntities_f, CMD_SERVER, "List all entities in use");
	Cmd_Add("server_info", Sv_ServerInfo_f, CMD_SERVER, "Print server info settings");
	Cmd_Add("user_info", Sv_UserInfo_f, CMD_SERVER, "Print information for a given user");

	cmd_t *demo_cmd = Cmd_Add("demo", Sv_Demo_f, CMD_SERVER, "Start playback of the specified demo file");
	Cmd_SetAutocomplete(demo_cmd, Sv_Demo_Autocomplete_f);

	cmd_t *map_cmd = Cmd_Add("map", Sv_Map_f, CMD_SERVER, "Start a server for the specified map");
	Cmd_SetAutocomplete(map_cmd, Sv_Map_Autocomplete_f);

	Cmd_Add("set_master", Sv_SetMaster_f, CMD_SERVER,
	        "Set the master server(s) for the dedicated server");
	Cmd_Add("heartbeat", Sv_Heartbeat_f, CMD_SERVER, "Send a heartbeat to the master server");

	if (dedicated->value) {
		Cmd_Add("say", Sv_Say_f, CMD_SERVER, "Send a global chat message");
		Cmd_Add("tell", Sv_Tell_f, CMD_SERVER, "Send a private chat message");
		Cmd_Add("stuff", Sv_Stuff_f, CMD_SERVER, "Force a client to execute a command");
	}
}

