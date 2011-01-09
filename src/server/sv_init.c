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

sv_static_t svs;  // persistent server info
sv_server_t sv;  // local server

/*
 * Sv_FindIndex
 *
 * Searches sv.configstrings from the specified start, searching for the
 * desired name.  If not found, the name can be optionally created and sent to
 * all connected clients.  This allows the game to lazily load assets.
 */
static int Sv_FindIndex(const char *name, int start, int max, qboolean create){
	int i;

	if(!name || !name[0])
		return 0;

	for(i = 1; i < max && sv.configstrings[start + i][0]; i++)
		if(!strcmp(sv.configstrings[start + i], name))
			return i;

	if(!create)
		return 0;

	if(i == max){
		Com_Warn("Sv_FindIndex: max index reached.");
		return 0;
	}

	strncpy(sv.configstrings[start + i], name, sizeof(sv.configstrings[i]));

	if(sv.state != ss_loading){  // send the update to everyone
		Sb_Clear(&sv.multicast);
		Msg_WriteChar(&sv.multicast, svc_configstring);
		Msg_WriteShort(&sv.multicast, start + i);
		Msg_WriteString(&sv.multicast, name);
		Sv_Multicast(vec3_origin, MULTICAST_ALL_R);
	}

	return i;
}

int Sv_ModelIndex(const char *name){
	return Sv_FindIndex(name, CS_MODELS, MAX_MODELS, true);
}

int Sv_SoundIndex(const char *name){
	return Sv_FindIndex(name, CS_SOUNDS, MAX_SOUNDS, true);
}

int Sv_ImageIndex(const char *name){
	return Sv_FindIndex(name, CS_IMAGES, MAX_IMAGES, true);
}


/*
 * Sv_CreateBaseline
 *
 * Entity baselines are used to compress the update messages
 * to the clients -- only the fields that differ from the
 * baseline will be transmitted
 */
static void Sv_CreateBaseline(void){
	edict_t *svent;
	int ent_num;

	for(ent_num = 1; ent_num < ge->num_edicts; ent_num++){
		svent = EDICT_FOR_NUM(ent_num);
		if(!svent->inuse)
			continue;
		if(!svent->s.model_index && !svent->s.sound && !svent->s.effects)
			continue;
		svent->s.number = ent_num;

		// take current state as baseline
		VectorCopy(svent->s.origin, svent->s.old_origin);
		sv.baselines[ent_num] = svent->s;
	}
}


/*
 * Sv_CheckMap
 *
 * Ensures that map exists before attempting to spawn a server to it.
 * Returns true if the map exists, false otherwise.
 */
static qboolean Sv_CheckMap(const char *name){
	char map[MAX_OSPATH];
	FILE *f;

	snprintf(map, sizeof(map), "maps/%s.bsp", name);
	Fs_OpenFile(map, &f, FILE_READ);

	if(!f){
		Com_Print("Couldn't open %s\n", map);
		return false;
	}

	Fs_CloseFile(f);
	return true;
}


/*
 * Sv_CheckDemo
 *
 * Attempts to open and peek into the specified demo file.  Returns
 * true if the file exists and appears to be a valid demo, false otherwise.
 * File is closed.
 */
static qboolean Sv_CheckDemo(const char *name){
	char demo[MAX_OSPATH];
	byte buff[MAX_MSGLEN];
	int msglen, protocol;
	FILE *f;

	snprintf(demo, sizeof(demo), "demos/%s", name);
	Fs_OpenFile(demo, &f, FILE_READ);
	if(!f){
		Com_Print("Couldn't open %s\n", demo);
		return false;
	}

	if(fread(&msglen, 4, 1, f) != 1)
		Com_Warn("Sv_CheckDemo: Failed to read demo.\n");

	msglen = LittleLong(msglen);

	if(msglen == -1 || msglen > MAX_MSGLEN){
		Com_Print("Sv_CheckDemo: %s is not a demo.\n", demo);
		return false;
	}

	if(fread(buff, msglen, 1, f) != 1)
		Com_Warn("Sv_CheckDemo: Failed to read demo.\n");

	memcpy(&protocol, buff + 1, 4);

	Fs_CloseFile(f);

	if(LittleLong(protocol) != PROTOCOL){
		Com_Print("%s is protocol %d\n", demo, LittleLong(protocol));
		return false;
	}

	return true;
}

/*
 * Sv_UpdateLatchedVars
 *
 * Applies any pending variable changes and clamps ones we really care about.
 */
static void Sv_UpdateLatchedVars(void){
	const int flags = CVAR_SERVER_INFO | CVAR_LATCH;

	Cvar_UpdateLatchedVars();

	if(sv_packetrate->value < SERVER_PACKETRATE_MIN)
		Cvar_FullSet("sv_packetrate", va("%i", SERVER_PACKETRATE_MIN), flags);
	else if(sv_packetrate->value > SERVER_PACKETRATE_MAX)
		Cvar_FullSet("sv_packetrate", va("%i", SERVER_PACKETRATE_MAX), flags);

	if(sv_maxclients->value <= 1)
		Cvar_FullSet("sv_maxclients", "1", flags);
	else if(sv_maxclients->value > MAX_CLIENTS)
		Cvar_FullSet("sv_maxclients", va("%i", MAX_CLIENTS), flags);
}


/*
 * Sv_RestartGame
 *
 * Reloads svs.clients, svs.client_entities, the game programs, etc.
 */
static void Sv_RestartGame(void){
	int i;

	svs.initialized = false;

	Sv_ShutdownGameProgs();

	Sv_UpdateLatchedVars();

	// free server static data
	if(svs.clients)
		Z_Free(svs.clients);

	// initialize the clients array
	svs.clients = Z_Malloc(sizeof(sv_client_t) * sv_maxclients->value);

	if(svs.entity_states)
		Z_Free(svs.entity_states);

	// and the entity states array
	svs.num_entity_states = sv_maxclients->value * UPDATE_BACKUP * MAX_PACKET_ENTITIES;
	svs.entity_states = Z_Malloc(sizeof(entity_state_t) * svs.num_entity_states);

	svs.packet_rate = sv_packetrate->value;

	svs.spawn_count = rand();

	Sv_InitGameProgs();

	// align the game entities with the server's clients
	for(i = 0; i < sv_maxclients->value; i++){

		edict_t *edict = EDICT_FOR_NUM(i + 1);
		edict->s.number = i + 1;

		svs.clients[i].edict = edict;
	}

	svs.last_heartbeat = -99999;

	svs.initialized = true;
}


/*
 * Sv_SpawnServer
 *
 * Change the server to a new map, taking all connected clients along with it.
 * The state parameter must either be ss_game or ss_demo.  See Sv_Map.
 */
static void Sv_SpawnServer(const char *server, sv_state_t state){
	int i;
	int mapsize;
	FILE *f;
	extern char *last_pak;

	Sv_FinalMessage("Server restarting..", true);

	Com_Print("Server initialization..\n");

	svs.real_time = 0;

	if(sv.demo_file)  // TODO: move this to svs, this isn't safe
		Fs_CloseFile(sv.demo_file);

	memset(&sv, 0, sizeof(sv));
	Com_SetServerState(sv.state);

	if(!svs.initialized || Cvar_PendingLatchedVars())
		Sv_RestartGame();
	else
		svs.spawn_count++;

	Sb_Init(&sv.multicast, sv.multicast_buffer, sizeof(sv.multicast_buffer));

	// ensure all clients are no greater than connected
	for(i = 0; i < sv_maxclients->value; i++){

		if(svs.clients[i].state > cs_connected)
			svs.clients[i].state = cs_connected;

		svs.clients[i].last_frame = -1;
	}

	// load the new server
	strcpy(sv.name, server);
	strcpy(sv.configstrings[CS_NAME], server);

	if(state == ss_demo){  // playing a demo, no map on the server
		sv.models[1] = Cm_LoadMap(NULL, &mapsize);
	} else {  // playing a game, load the map
		snprintf(sv.configstrings[CS_MODELS + 1], MAX_QPATH, "maps/%s.bsp", server);

		Fs_OpenFile(sv.configstrings[CS_MODELS + 1], &f, FILE_READ);  // resolve CS_PAK
		strcpy(sv.configstrings[CS_PAK], (last_pak ? last_pak : ""));
		Fs_CloseFile(f);

		sv.models[1] = Cm_LoadMap(sv.configstrings[CS_MODELS + 1], &mapsize);
	}
	snprintf(sv.configstrings[CS_MAPSIZE], MAX_QPATH, "%i", mapsize);

	// clear physics interaction links
	Sv_ClearWorld();

	for(i = 1; i < Cm_NumInlineModels(); i++){
		snprintf(sv.configstrings[CS_MODELS + 1 + i], MAX_QPATH, "*%i", i);
		sv.models[i + 1] = Cm_InlineModel(sv.configstrings[CS_MODELS + 1 + i]);
	}

	if(state == ss_game){  // load and spawn all other entities
		sv.state = ss_loading;
		Com_SetServerState(sv.state);

		ge->SpawnEntities(sv.name, Cm_EntityString());

		Sv_CreateBaseline();

		Com_Print("  Loaded %s, %d entities.\n", sv.configstrings[CS_MODELS + 1], ge->num_edicts);
	}
	else {
		Com_Print("  Loaded demo %s.\n", sv.name);
	}
	// set serverinfo variable
	Cvar_FullSet("mapname", sv.name, CVAR_SERVER_INFO | CVAR_NOSET);

	// all precaches are complete
	sv.state = state;
	Com_SetServerState(sv.state);

	// prepare the socket
	Net_Config(NS_SERVER, true);

	Com_Print("Server initialized.\n");
}


/*
 * Sv_Map
 *
 * Entry point for spawning a server on a .bsp or .dem.
 */
void Sv_Map(const char *level){
	sv_state_t state;
	qboolean exists;
	int i;

	i = strlen(level);
	state = i > 4 && !strcasecmp(level + i - 4, ".dem") ? ss_demo : ss_game;

	if(state == ss_demo)  // ensure demo or map exists
		exists = Sv_CheckDemo(level);
	else
		exists = Sv_CheckMap(level);

	if(!exists)  // demo or map file didn't exist
		return;

	Cbuf_CopyToDefer();
	Sv_SpawnServer(level, state);
}
