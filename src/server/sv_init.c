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
 * Sv_FindIndex
 *
 * Searches sv.config_strings from the specified start, searching for the
 * desired name.  If not found, the name can be optionally created and sent to
 * all connected clients.  This allows the game to lazily load assets.
 */
static int Sv_FindIndex(const char *name, int start, int max, qboolean create){
	int i;

	if(!name || !name[0])
		return 0;

	for(i = 1; i < max && sv.config_strings[start + i][0]; i++)
		if(!strcmp(sv.config_strings[start + i], name))
			return i;

	if(!create)
		return 0;

	if(i == max){
		Com_Warn("Sv_FindIndex: max index reached.");
		return 0;
	}

	strncpy(sv.config_strings[start + i], name, sizeof(sv.config_strings[i]));

	if(sv.state != ss_loading){  // send the update to everyone
		Sb_Clear(&sv.multicast);
		Msg_WriteChar(&sv.multicast, svc_config_string);
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
	edict_t *ent;
	int i;

	for(i = 1; i < svs.game->num_edicts; i++){

		ent = EDICT_FOR_NUM(i);

		if(!ent->in_use)
			continue;

		if(!ent->s.model_index && !ent->s.sound && !ent->s.effects)
			continue;

		ent->s.number = i;

		// take current state as baseline
		VectorCopy(ent->s.origin, ent->s.old_origin);
		sv.baselines[i] = ent->s;
	}
}


/*
 * Sv_ShutdownMessage
 *
 * Sends the shutdown message message to all connected clients.  The message
 * is sent immediately, because the server could completely terminate after
 * returning from this function.
 */
static void Sv_ShutdownMessage(const char *msg, qboolean reconnect){
	sv_client_t *cl;
	int i;

	if(!svs.initialized)
		return;

	Sb_Clear(&net_message);

	if(msg){  // send message
		Msg_WriteByte(&net_message, svc_print);
		Msg_WriteByte(&net_message, PRINT_HIGH);
		Msg_WriteString(&net_message, msg);
	}

	if(reconnect)  // send reconnect
		Msg_WriteByte(&net_message, svc_reconnect);
	else  // or just disconnect
		Msg_WriteByte(&net_message, svc_disconnect);

	for(i = 0, cl = svs.clients; i < sv_maxclients->value; i++, cl++)
		if(cl->state >= cs_connected)
			Netchan_Transmit(&cl->netchan, net_message.size, net_message.data);
}


/*
 * Sv_ClearState
 *
 * Wipes the sv_server_t structure after freeing any references it holds.
 */
static void Sv_ClearState() {

	if(svs.initialized){  // if we were intialized, cleanup

		if(sv.demo_file){
			Fs_CloseFile(sv.demo_file);
		}
	}

	memset(&sv, 0, sizeof(sv));
	Com_SetServerState(sv.state);

	svs.real_time = 0;
	svs.last_heartbeat = -9999999;
}


/*
 * Sv_UpdateLatchedVars
 *
 * Applies any pending variable changes and clamps ones we really care about.
 */
static void Sv_UpdateLatchedVars(void){

	Cvar_UpdateLatchedVars();

	if(sv_maxclients->value < MIN_CLIENTS)
		sv_maxclients->value = MIN_CLIENTS;
	else if(sv_maxclients->value > MAX_CLIENTS)
		sv_maxclients->value = MAX_CLIENTS;


	if(sv_framerate->value < SERVER_FRAME_RATE_MIN)
		sv_framerate->value = SERVER_FRAME_RATE_MIN;
	else if(sv_framerate->value > SERVER_FRAME_RATE_MAX)
		sv_framerate->value = SERVER_FRAME_RATE_MAX;
}


/*
 * Sv_InitClients
 *
 * Reloads svs.clients, svs.client_entities, the game programs, etc.  Because
 * we must allocate clients and edicts based on sizes the game module requests,
 * we refresh the game module
 */
static void Sv_InitClients(void){
	int i;

	if(!svs.initialized || Cvar_PendingLatchedVars()){

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

		svs.frame_rate = sv_framerate->value;

		svs.spawn_count = rand();

		Sv_InitGameProgs();
	}
	else {
		svs.spawn_count++;
	}

	// align the game entities with the server's clients
	for(i = 0; i < sv_maxclients->value; i++){

		edict_t *edict = EDICT_FOR_NUM(i + 1);
		edict->s.number = i + 1;

		// assign their edict
		svs.clients[i].edict = edict;

		// reset state of spawned clients back to connected
		if(svs.clients[i].state > cs_connected)
			svs.clients[i].state = cs_connected;

		// invalidate last frame to force a baseline
		svs.clients[i].last_frame = -1;
		svs.clients[i].last_message = 0;
	}
}


/*
 * Sv_LoadMedia
 *
 * Loads the map or demo file and populates the server-controlled "config
 * strings."  We hand off the entity string to the game module, which will
 * load the rest.
 */
static void Sv_LoadMedia(const char *server, sv_state_t state){
	extern char *fs_last_pak;
	char demo[MAX_QPATH];
	int i, mapsize;

	strcpy(sv.name, server);
	strcpy(sv.config_strings[CS_NAME], server);

	if(state == ss_demo){  // loading a demo
		snprintf(demo, sizeof(demo), "demos/%s.dem", sv.name);

		sv.models[1] = Cm_LoadMap(NULL, &mapsize);

		Fs_OpenFile(demo, &sv.demo_file, FILE_READ);

		Com_Print("  Loaded demo %s.\n", sv.name);
	}
	else {  // loading a map
		snprintf(sv.config_strings[CS_MODELS + 1], MAX_QPATH, "maps/%s.bsp", sv.name);

		sv.models[1] = Cm_LoadMap(sv.config_strings[CS_MODELS + 1], &mapsize);

		if(fs_last_pak){
			strncpy(sv.config_strings[CS_PAK], fs_last_pak, MAX_QPATH);
		}

		for(i = 1; i < Cm_NumInlineModels(); i++){

			char *s = sv.config_strings[CS_MODELS + 1 + i];
			snprintf(s, MAX_QPATH, "*%i", i);

			sv.models[i + 1] = Cm_InlineModel(s);
		}

		sv.state = ss_loading;
		Com_SetServerState(sv.state);

		Sv_InitWorld();

		svs.game->SpawnEntities(sv.name, Cm_EntityString());

		Sv_CreateBaseline();

		Com_Print("  Loaded map %s, %d entities.\n", sv.name, svs.game->num_edicts);
	}
	snprintf(sv.config_strings[CS_MAP_SIZE], MAX_QPATH, "%i", mapsize);

	Cvar_FullSet("mapname", sv.name, CVAR_SERVER_INFO | CVAR_NOSET);
}


/*
 * Sv_InitServer
 *
 * Entry point for spawning a new server or changing maps / demos.  Brings any
 * connected clients along for the ride by broadcasting a reconnect before
 * clearing state.  Special effort is made to ensure that a locally connected
 * client sees the reconnect message immediately.
 */
void Sv_InitServer(const char *server, sv_state_t state){
#ifdef BUILD_CLIENT
	extern void Cl_Frame(int msec);
#endif
	char path[MAX_QPATH];
	FILE *file;

	Com_Debug("Sv_InitServer: %s (%d)\n", server, state);

	// ensure that the requested map or demo exists
	if(state == ss_demo)
		snprintf(path, sizeof(path), "demos/%s.dem", server);
	else
		snprintf(path, sizeof(path), "maps/%s.bsp", server);

	Fs_OpenFile(path, &file, FILE_READ);

	if(!file){
		Com_Print("Couldn't open %s\n", path);
		return;
	}

	Fs_CloseFile(file);

	// inform any connected clients to reconnect to us
	Sv_ShutdownMessage("Server restarting...\n", true);

#ifdef BUILD_CLIENT
	// pump a frame through our client so that they reconnect
	Cl_Frame(999);
#endif

	// clear the sv_server_t structure
	Sv_ClearState();

	Com_Print("Server initialization...\n");

	// initialize the clients, loading the game progs if we need them
	Sv_InitClients();

	// load the map or demo and related media
	Sv_LoadMedia(server, state);

	// we unlock the "cheat" cvars if we're just running 1 client
	Cvar_LockCheatVars(sv_maxclients->value > 1);

	sv.state = state;
	Com_SetServerState(sv.state);

	Sb_Init(&sv.multicast, sv.multicast_buffer, sizeof(sv.multicast_buffer));

	Com_Print("Server initialized.\n");

	svs.initialized = true;
}


/*
 * Sv_ShutdownServer
 *
 * Called with the game is shutting down.
 */
void Sv_ShutdownServer(const char *msg){

	Com_Debug("Sv_ShutdownServer: %s\n", msg);

	Com_Print("Server shutdown...\n");

	Sv_ShutdownMessage(msg, false);

	Sv_ShutdownGameProgs();

	Sv_ClearState();

	Com_Print("Server down\n");

	svs.initialized = false;
}
