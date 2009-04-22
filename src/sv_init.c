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

server_static_t svs;  // persistent server info
server_t sv;  // local server

/*
 * Sv_FindIndex
 *
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
	int entnum;

	for(entnum = 1; entnum < ge->num_edicts; entnum++){
		svent = EDICT_NUM(entnum);
		if(!svent->inuse)
			continue;
		if(!svent->s.modelindex && !svent->s.sound && !svent->s.effects)
			continue;
		svent->s.number = entnum;

		// take current state as baseline
		VectorCopy(svent->s.origin, svent->s.old_origin);
		sv.baselines[entnum] = svent->s;
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
		Com_Printf("Couldn't open %s\n", map);
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
		Com_Printf("Couldn't open %s\n", demo);
		return false;
	}

	if(fread(&msglen, 4, 1, f) != 1)
		Com_Warn("Sv_CheckDemo: Failed to read demo.\n");

	msglen = LittleLong(msglen);

	if(msglen == -1 || msglen > MAX_MSGLEN){
		Com_Printf("Sv_CheckDemo: %s is not a demo.\n", demo);
		return false;
	}

	if(fread(buff, msglen, 1, f) != 1)
		Com_Warn("Sv_CheckDemo: Failed to read demo.\n");

	memcpy(&protocol, buff + 1, 4);

	Fs_CloseFile(f);

	if(LittleLong(protocol) != PROTOCOL){
		Com_Printf("%s is protocol %d\n", demo, LittleLong(protocol));
		return false;
	}

	return true;
}


/*
 * Sv_InitGame
 *
 * A brand new game has been started
 */
static void Sv_InitGame(void){
	int i;
	edict_t *ent;

	svs.initialized = true;

	// init clients
	if(sv_maxclients->value <= 1)
		Cvar_FullSet("sv_maxclients", "1", CVAR_SERVERINFO | CVAR_LATCH);
	else if(sv_maxclients->value > MAX_CLIENTS)
		Cvar_FullSet("sv_maxclients", va("%i", MAX_CLIENTS), CVAR_SERVERINFO | CVAR_LATCH);

	svs.spawncount = rand();
	svs.clients = Z_Malloc(sizeof(client_t) * sv_maxclients->value);
	svs.num_client_entities = sv_maxclients->value * UPDATE_BACKUP * 64;
	svs.client_entities = Z_Malloc(sizeof(entity_state_t) * svs.num_client_entities);
	svs.last_heartbeat = -999999;

	Sv_InitGameProgs();

	for(i = 0; i < sv_maxclients->value; i++){
		ent = EDICT_NUM(i + 1);
		ent->s.number = i + 1;
		svs.clients[i].edict = ent;
		memset(&svs.clients[i].lastcmd, 0, sizeof(svs.clients[i].lastcmd));
	}
}


/*
 * Sv_SpawnServer
 *
 * Change the server to a new map, taking all connected clients along with it.
 * The serverstate parameter must either be ss_game or ss_demo.  See Sv_Map.
 */
static void Sv_SpawnServer(const char *server, server_state_t serverstate){
	int i;
	int mapsize;
	qboolean reconnect;
	FILE *f;
	extern char *last_pak;

	Com_Printf("Server initialization..\n");

	reconnect = false;
	if(!svs.initialized || Cvar_PendingLatchedVars()){

		Sv_Shutdown("Server restarting..\n", true);

		Cvar_UpdateLatchedVars();  // apply latched changes

		// clamp the packet rate
		if(sv_packetrate->value < MIN_PACKETRATE)
			Cvar_SetValue("sv_packetrate", MIN_PACKETRATE);
		else if(sv_packetrate->value > MAX_PACKETRATE)
			Cvar_SetValue("sv_packetrate", MAX_PACKETRATE);

		svs.packetrate = sv_packetrate->value;

		Sv_InitGame();  // init clients, game progs, etc
	}
	else  // clients can immediately reconnect
		reconnect = true;

	if(sv.demofile)
		Fs_CloseFile(sv.demofile);

	svs.spawncount++;  // any partially connected client will be restarted

	svs.realtime = 0;

	sv.state = ss_dead;
	Com_SetServerState(sv.state);

	// wipe the entire per-level structure
	memset(&sv, 0, sizeof(sv));

	Sb_Init(&sv.multicast, sv.multicast_buf, sizeof(sv.multicast_buf));

	// ensure all clients are no greater than connected
	for(i = 0; i < sv_maxclients->value; i++){
		if(svs.clients[i].state > cs_connected)
			svs.clients[i].state = cs_connected;
		svs.clients[i].lastframe = -1;
	}

	sv.time = 1000;  // set time

	strcpy(sv.name, server);
	strcpy(sv.configstrings[CS_NAME], server);

	if(serverstate == ss_demo){  // playing a demo, no map on the server
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

	if(serverstate == ss_game){  // load and spawn all other entities

		sv.state = ss_loading;
		Com_SetServerState(sv.state);

		if(ge == NULL){
			Com_Error(ERR_FATAL, "Sv_SpawnServer: Game module not loaded.");
		}

		ge->SpawnEntities(sv.name, Cm_EntityString());

		ge->RunFrame();  // run two frames to allow everything to settle
		ge->RunFrame();

		// create a baseline for more efficient communications
		Sv_CreateBaseline();

		Com_Printf("  Loaded %s, %d entities.\n", sv.configstrings[CS_MODELS + 1],
			ge->num_edicts);
	}
	else {
		Com_Printf("  Loaded demo %s.\n", sv.name);
	}

	Net_Config(NS_SERVER, true);

	// all precaches are complete
	sv.state = serverstate;
	Com_SetServerState(sv.state);

	// set serverinfo variable
	Cvar_FullSet("mapname", sv.name, CVAR_SERVERINFO | CVAR_NOSET);

	if(reconnect){  // tell clients to reconnect
		Sv_BroadcastCommand("reconnect\n");
		Sv_SendClientMessages();
	}

	Com_Printf("Server initialized.\n");
}


/*
 * Sv_Map
 *
 * Entry point for spawning a server on a .bsp or .dem.
 */
void Sv_Map(const char *level){
	server_state_t state;
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

	Cl_Drop();  // make sure local client drops
	Cbuf_CopyToDefer();
	Sv_SpawnServer(level, state);
}
