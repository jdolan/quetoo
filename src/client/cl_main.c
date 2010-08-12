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

#include <unistd.h>

#include "client.h"
#include "ui/ui_main.h"
#include "ui/ui_font.h"
#include "ui/ui_parse.h"

cvar_t *cl_addentities;
cvar_t *cl_addparticles;
cvar_t *cl_async;
cvar_t *cl_blend;
cvar_t *cl_bob;
cvar_t *cl_chatsound;
cvar_t *cl_teamchatsound;
cvar_t *cl_counters;
cvar_t *cl_crosshair;
cvar_t *cl_crosshaircolor;
cvar_t *cl_crosshairscale;
cvar_t *cl_emits;
cvar_t *cl_fov;
cvar_t *cl_fovzoom;
cvar_t *cl_hud;
cvar_t *cl_ignore;
cvar_t *cl_maxfps;
cvar_t *cl_maxpps;
cvar_t *cl_netgraph;
cvar_t *cl_predict;
cvar_t *cl_showmiss;
cvar_t *cl_shownet;
cvar_t *cl_thirdperson;
cvar_t *cl_timeout;
cvar_t *cl_viewsize;
cvar_t *cl_weapon;
cvar_t *cl_weather;

cvar_t *rcon_password;
cvar_t *rcon_address;

// userinfo
cvar_t *color;
cvar_t *msg;
cvar_t *name;
cvar_t *password;
cvar_t *rate;
cvar_t *skin;

cvar_t *recording;

client_static_t cls;
client_state_t cl;

centity_t cl_entities[MAX_EDICTS];

entity_state_t cl_parse_entities[MAX_PARSE_ENTITIES];

char cl_weaponmodels[MAX_CLIENTWEAPONMODELS][MAX_QPATH];
int num_cl_weaponmodels;

char demoname[MAX_OSPATH];


/*
 * Cl_WriteDemoHeader
 *
 * Writes serverdata, configstrings, and baselines once a non-delta
 * compressed frame arrives from the server.
 */
static void Cl_WriteDemoHeader(void){
	byte buf_data[MAX_MSGLEN];
	sizebuf_t buf;
	int i;
	int len;
	entity_state_t nullstate;

	// write out messages to hold the startup information
	Sb_Init(&buf, buf_data, sizeof(buf_data));

	// write the serverdata
	Msg_WriteByte(&buf, svc_serverdata);
	Msg_WriteLong(&buf, PROTOCOL);
	Msg_WriteLong(&buf, cl.servercount);
	Msg_WriteLong(&buf, cl.serverrate);
	Msg_WriteByte(&buf, 1);  // demoserver byte
	Msg_WriteString(&buf, cl.gamedir);
	Msg_WriteShort(&buf, cl.playernum);
	Msg_WriteString(&buf, cl.configstrings[CS_NAME]);

	// and configstrings
	for(i = 0; i < MAX_CONFIGSTRINGS; i++){
		if(cl.configstrings[i][0]){
			if(buf.cursize + strlen(cl.configstrings[i]) + 32 > buf.maxsize){  // write it out
				len = LittleLong(buf.cursize);
				Fs_Write(&len, 4, 1, cls.demofile);
				Fs_Write(buf.data, buf.cursize, 1, cls.demofile);
				buf.cursize = 0;
			}

			Msg_WriteByte(&buf, svc_configstring);
			Msg_WriteShort(&buf, i);
			Msg_WriteString(&buf, cl.configstrings[i]);
		}
	}

	// and baselines
	for(i = 0; i < MAX_EDICTS; i++){
		entity_state_t *ent = &cl_entities[i].baseline;
		if(!ent->number)
			continue;

		if(buf.cursize + 64 > buf.maxsize){  // write it out
			len = LittleLong(buf.cursize);
			Fs_Write(&len, 4, 1, cls.demofile);
			Fs_Write(buf.data, buf.cursize, 1, cls.demofile);
			buf.cursize = 0;
		}

		memset(&nullstate, 0, sizeof(nullstate));

		Msg_WriteByte(&buf, svc_spawnbaseline);
		Msg_WriteDeltaEntity(&nullstate, &cl_entities[i].baseline, &buf, true, true);
	}

	Msg_WriteByte(&buf, svc_stufftext);
	Msg_WriteString(&buf, "precache\n");

	// write it to the demo file

	len = LittleLong(buf.cursize);
	Fs_Write(&len, 4, 1, cls.demofile);
	Fs_Write(buf.data, buf.cursize, 1, cls.demofile);

	Com_Printf("Recording to %s.\n", demoname);
	// the rest of the demo file will be individual frames
}


/*
 * Cl_WriteDemoMessage
 *
 * Dumps the current net message, prefixed by the length.
 */
void Cl_WriteDemoMessage(void){
	int len;

	if(!cls.demofile)
		return;

	if(cls.demowaiting)  // we have not yet received a non-delta frame
		return;

	if(!ftell(cls.demofile))  // write header
		Cl_WriteDemoHeader();

	// the first eight bytes are just packet sequencing stuff
	len = LittleLong(net_message.cursize - 8);
	Fs_Write(&len, 4, 1, cls.demofile);

	// write the message payload
	Fs_Write(net_message.data + 8, len, 1, cls.demofile);
}


/*
 * Cl_Stop_f
 *
 * Stop recording a demo
 */
static void Cl_Stop_f(void){
	int len;

	if(!cls.demofile){
		Com_Printf("Not recording a demo.\n");
		return;
	}

	// finish up
	len = -1;
	Fs_Write(&len, 4, 1, cls.demofile);
	Fs_CloseFile(cls.demofile);

	cls.demofile = NULL;
	Com_Printf("Stopped demo.\n");

	// inform server we're done recording
	Cvar_ForceSet("recording", "0");
}


/*
 * Cl_Record_f
 *
 * record <demoname>
 *
 * Begins recording a demo from the current position
 */
static void Cl_Record_f(void){
	if(Cmd_Argc() != 2){
		Com_Printf("Usage: %s <demoname>\n", Cmd_Argv(0));
		return;
	}

	if(cls.demofile){
		Com_Printf("Already recording.\n");
		return;
	}

	if(cls.state != ca_active){
		Com_Printf("You must be in a level to record.\n");
		return;
	}

	// open the demo file
	snprintf(demoname, sizeof(demoname), "%s/demos/%s.dem", Fs_Gamedir(), Cmd_Argv(1));

	Fs_CreatePath(demoname);
	cls.demofile = fopen(demoname, "wb");
	if(!cls.demofile){
		Com_Warn("Cl_Record_f: couldn't open %s.\n", demoname);
		return;
	}

	// don't start saving messages until a non-delta compressed message is received
	cls.demowaiting = true;

	// update userinfo var to inform server to send angles
	Cvar_ForceSet("recording", "1");

	Com_Printf("Requesting demo support from server..\n");
}


// a copy of the last item we dropped, for %d
static char last_dropped_item[MAX_TOKEN_CHARS];

/*
 * Cl_VariableString
 *
 * This is the client-specific sibling to Cvar_VariableString.
 */
static const char *Cl_ExpandVariable(char v){
	int i;

	switch(v){

		case 'l':  // client's location
			return Cl_LocationHere();
		case 'L':  // client's line of sight
			return Cl_LocationThere();

		case 'd':  // last dropped item
			return last_dropped_item;

		case 'h':  // health
			if(!cl.frame.valid)
				return "";
			i = cl.frame.playerstate.stats[STAT_HEALTH];
			return va("%d", i);

		case 'a':  // armor
			if(!cl.frame.valid)
				return "";
			i = cl.frame.playerstate.stats[STAT_ARMOR];
			return va("%d", i);

		default:
			return "";
	}
}


// expand %variables for say and say_team
static char expanded[MAX_STRING_CHARS];

/*
 * Cl_ExpandVariables
 */
static char *Cl_ExpandVariables(const char *text){
	int i, j, len;

	if(!text || !text[0])
		return "";

	memset(expanded, 0, sizeof(expanded));
	len = strlen(text);

	for(i = j = 0; i < len; i++){
		if(text[i] == '%' && i < len - 1){  // expand %variables
			const char *c = Cl_ExpandVariable(text[i + 1]);
			strcat(expanded, c);
			j += strlen(c);
			i++;
		}
		else  // or just append normal chars
			expanded[j++] = text[i];
	}

	return expanded;
}


/*
 * Cmd_ForwardToServer
 *
 * Adds the current command line as a clc_stringcmd to the client message.
 * things like godmode, noclip, etc, are commands directed to the server,
 * so when they are typed in at the console, they will need to be forwarded.
 */
void Cmd_ForwardToServer(void){
	const char *cmd, *args;

	if(cls.state <= ca_disconnected){
		Com_Printf("Not connected.\n");
		return;
	}

	cmd = Cmd_Argv(0);

	if(*cmd == '-' || *cmd == '+'){
		Com_Printf("Unknown command \"%s\"\n", cmd);
		return;
	}

	args = Cmd_Args();

	if(!strcmp(cmd, "drop"))  // maintain last item dropped for 'say %d'
		strncpy(last_dropped_item, args, sizeof(last_dropped_item) - 1);

	if(!strcmp(cmd, "say") || !strcmp(cmd, "say_team"))
		args = Cl_ExpandVariables(args);

	Msg_WriteByte(&cls.netchan.message, clc_stringcmd);
	Sb_Print(&cls.netchan.message, cmd);
	if(Cmd_Argc() > 1){
		Sb_Print(&cls.netchan.message, " ");
		Sb_Print(&cls.netchan.message, args);
	}
}


/*
 * Cl_Quit_f
 */
static void Cl_Quit_f(void){
	Cl_Disconnect();
	Com_Quit();
}


/*
 * Cl_Drop
 *
 * Called after an ERR_DROP or ERR_NONE was thrown
 */
void Cl_Drop(void){

	cls.loading = 0;

	if(cls.state == ca_uninitialized)
		return;

	if(cls.state == ca_disconnected)
		return;

	Cl_Disconnect();
}


/*
 * Cl_SendConnect
 *
 * We have gotten a challenge from the server, so try and connect.
 */
static void Cl_SendConnect(void){
	netadr_t adr;
	int qport;

	memset(&adr, 0, sizeof(adr));

	if(!Net_StringToAdr(cls.servername, &adr)){
		Com_Printf("Bad server address\n");
		cls.connect_time = 0;
		return;
	}

	if(adr.port == 0)  // use default port
		adr.port = BigShort(PORT_SERVER);

	qport = (int)Cvar_GetValue("net_qport");  // has been set by netchan

	Netchan_OutOfBandPrint(NS_CLIENT, adr, "connect %i %i %i \"%s\"\n",
			PROTOCOL, qport, cls.challenge, Cvar_Userinfo());

	userinfo_modified = false;
}

extern cvar_t *sv_maxclients;

/*
 * Cl_CheckForResend
 *
 * Resend a connect message if the last one has timed out
 */
static void Cl_CheckForResend(void){
	netadr_t adr;

	// if the local server is running and we aren't then connect
	if(cls.state == ca_disconnected && Com_ServerState()){
		cls.state = ca_connecting;

		Cvar_LockCheatVars(sv_maxclients->value > 1);  // lock cheat vars

		strncpy(cls.servername, "localhost", sizeof(cls.servername) - 1);
		// we don't need a challenge on the localhost
		Cl_SendConnect();
		return;
	}

	// resend if we haven't gotten a reply yet
	if(cls.state != ca_connecting)
		return;

	if(cls.realtime - cls.connect_time < 3000)
		return;

	if(!Net_StringToAdr(cls.servername, &adr)){
		Com_Printf("Bad server address\n");
		cls.state = ca_disconnected;
		return;
	}

	// if we are running a local server, and no one else is connected, cheat
	// cvars may be modified, otherwise they may not
	if(Com_ServerState())
		Cvar_LockCheatVars(sv_maxclients->value > 1);
	else
		Cvar_LockCheatVars(true);

	if(adr.port == 0)
		adr.port = BigShort(PORT_SERVER);

	cls.connect_time = cls.realtime;  // for retransmit requests

	Com_Printf("Connecting to %s...\n", cls.servername);

	Netchan_OutOfBandPrint(NS_CLIENT, adr, "getchallenge\n");
}


/*
 * Cl_Connect_f
 */
static void Cl_Connect_f(void){
	const char *s;

	if(Cmd_Argc() != 2){
		Com_Printf("Usage: %s <address>\n", Cmd_Argv(0));
		return;
	}

	if(Com_ServerState())  // if running a local server, kill it and reissue
		Sv_Shutdown("Server quit.\n", false);

	s = Cmd_Argv(1);

	if(s[0] == '*'){  // resolve by server number
		const server_info_t *server = Cl_ServerForNum(atoi(s + 1));

		if(!server){
			Com_Warn("Invalid server: %s\n", Cmd_Argv(1));
			return;
		}

		strncpy(cls.servername, Net_AdrToString(server->adr),
				sizeof(cls.servername) - 1);
	}
	else
		strncpy(cls.servername, s, sizeof(cls.servername) - 1);

	Cl_Disconnect();

	cls.connect_time = -99999;  // fire immediately
	cls.state = ca_connecting;
}


/*
 * Cl_Rcon_f
 *
 * Send the rest of the command line over as an unconnected command.
 */
static void Cl_Rcon_f(void){
	char message[1024];
	int i;
	netadr_t to;

	if(!rcon_password->string){
		Com_Printf("No rcon_password set.\n");
		return;
	}

	memset(&to, 0, sizeof(to));

	message[0] = (char)255;
	message[1] = (char)255;
	message[2] = (char)255;
	message[3] = (char)255;
	message[4] = 0;

	strcat(message, "rcon ");
	strcat(message, rcon_password->string);
	strcat(message, " ");

	for(i = 1; i < Cmd_Argc(); i++){
		strcat(message, Cmd_Argv(i));
		strcat(message, " ");
	}

	if(cls.state >= ca_connected)
		to = cls.netchan.remote_address;
	else {
		if(!strlen(rcon_address->string)){
			Com_Printf("Not connected and no rcon_address set.\n");
			return;
		}
		Net_StringToAdr(rcon_address->string, &to);
		if(to.port == 0)
			to.port = BigShort(PORT_SERVER);
	}

	Net_SendPacket(NS_CLIENT, strlen(message) + 1, message, to);
}


/*
 * Cl_ClearState
 */
void Cl_ClearState(void){

	Cl_ClearEffects();

	// wipe the entire view
	memset(&r_view, 0, sizeof(r_view));

	// wipe the entire cl structure
	memset(&cl, 0, sizeof(cl));

	// as well as the parse entities
	memset(&cl_entities, 0, sizeof(cl_entities));

	Sb_Clear(&cls.netchan.message);
}


/*
 * Cl_SendDisconnect
 *
 * Sends the disconnect command to the server (several times).  This is used
 * when the client actually wishes to disconnect or quit, or when an Http
 * download has begun.  This way, the client does not waste a server slot
 * (or just timeout) while downloading a level.
 */
void Cl_SendDisconnect(void){
	byte final[16];

	if(cls.state == ca_disconnected)
		return;

	// send a disconnect message to the server
	final[0] = clc_stringcmd;
	strcpy((char *)final + 1, "disconnect");
	Netchan_Transmit(&cls.netchan, strlen((char *)final), final);
	Netchan_Transmit(&cls.netchan, strlen((char *)final), final);
	Netchan_Transmit(&cls.netchan, strlen((char *)final), final);

	Cl_ClearState();

	cls.connect_time = 0;
	cls.state = ca_disconnected;
}


/*
 * Cl_Disconnect
 *
 * Sends a disconnect message to the current server, stops any pending
 * demo recording, and updates cls.state so that we drop to console
 */
void Cl_Disconnect(void){

	if(cls.state == ca_disconnected)
		return;

	if(timedemo->value){  // summarize timedemo results

		const float s = (cls.realtime - cl.timedemo_start) / 1000.0;

		Com_Printf("%i frames, %3.2f seconds: %4.2ffps\n",
				cl.timedemo_frames, s, cl.timedemo_frames / s);

		cl.timedemo_frames = cl.timedemo_start = 0;
	}

	Cl_SendDisconnect();  // tell the server to disconnect

	if(cls.demofile)  // stop demo recording
		Cl_Stop_f();

	// stop download
	if(cls.download.file){

		if(cls.download.http)  // clean up http downloads
			Cl_HttpDownloadCleanup();
		else  // or just stop legacy ones
			Fs_CloseFile(cls.download.file);

		cls.download.file = NULL;
	}
}


/*
 * Cl_Disconnect_f
 */
static void Cl_Disconnect_f(void){
	Com_Error(ERR_NONE, "Disconnected from server.\n");
}


/*
 * Cl_Reconnect_f
 */
void Cl_Reconnect_f(void){
	qboolean reconnect;

	if(cls.download.file)  // don't disrupt downloads
		return;

	if(cls.servername[0] != '\0'){

		reconnect = !cl.demoserver;  // don't reconnect to demos

		if(cls.state >= ca_connecting){
			Com_Printf("Disconnecting...\n");
			Cl_Disconnect();
		}

		if(!reconnect)
			return;

		cls.connect_time = -99999;  // fire immediately
		cls.state = ca_connecting;
	}
	else Com_Printf("No server to reconnect to\n");
}


/*
 * Cl_ConnectionlessPacket
 *
 * Responses to broadcasts, etc
 */
static void Cl_ConnectionlessPacket(void){
	char *s;
	char *c;
	int qport;

	Msg_BeginReading(&net_message);
	Msg_ReadLong(&net_message);  // skip the -1

	s = Msg_ReadStringLine(&net_message);

	Cmd_TokenizeString(s);

	c = Cmd_Argv(0);

	Com_Dprintf("%s: %s\n", Net_AdrToString(net_from), c);

	// server connection
	if(!strcmp(c, "client_connect")){

		if(cls.state == ca_connected){
			Com_Printf("Dup connect received.  Ignored.\n");
			return;
		}

		qport = (int)Cvar_GetValue("net_qport");
		Netchan_Setup(NS_CLIENT, &cls.netchan, net_from, qport);
		Msg_WriteChar(&cls.netchan.message, clc_stringcmd);
		Msg_WriteString(&cls.netchan.message, "new");
		cls.state = ca_connected;

		memset(cls.downloadurl, 0, sizeof(cls.downloadurl));
		if(Cmd_Argc() == 2)  // http download url
			strncpy(cls.downloadurl, Cmd_Argv(1), sizeof(cls.downloadurl) - 1);
		return;
	}

	// server responding to a status broadcast
	if(!strcmp(c, "info")){
		Cl_ParseStatusMessage();
		return;
	}

	// print command from somewhere
	if(!strcmp(c, "print")){
		s = Msg_ReadString(&net_message);
		Com_Printf("%s", s);
		return;
	}

	// ping from somewhere
	if(!strcmp(c, "ping")){
		Netchan_OutOfBandPrint(NS_CLIENT, net_from, "ack");
		return;
	}

	// servers list from master
	if(!strcmp(c, "servers")){
		Cl_ParseServersList();
		return;
	}

	// challenge from the server we are connecting to
	if(!strcmp(c, "challenge")){
		if(cls.state != ca_connecting){
			Com_Printf("Dup challenge received.  Ignored.\n");
			return;
		}
		cls.challenge = atoi(Cmd_Argv(1));
		Cl_SendConnect();
		return;
	}

	Com_Warn("Cl_ConnectionlessPacket: Unknown command: %s.\n", c);
}


/*
 * Cl_ReadPackets
 */
static void Cl_ReadPackets(void){

	memset(&net_from, 0, sizeof(net_from));

	while(Net_GetPacket(NS_CLIENT, &net_from, &net_message)){

		// remote command packet
		if(*(int *)net_message.data == -1){
			Cl_ConnectionlessPacket();
			continue;
		}

		if(cls.state <= ca_connecting)
			continue;  // dump it if not connected

		if(net_message.cursize < 8){
			Com_Dprintf("%s: Runt packet.\n", Net_AdrToString(net_from));
			continue;
		}

		// packet from server
		if(!Net_CompareAdr(net_from, cls.netchan.remote_address)){
			Com_Dprintf("%s: Sequenced packet without connection.\n",
					Net_AdrToString(net_from));
			continue;
		}

		if(!Netchan_Process(&cls.netchan, &net_message))
			continue;  // wasn't accepted for some reason

		Cl_ParseServerMessage();
	}

	// check timeout
	if(cls.state >= ca_connected && cls.realtime - cls.netchan.last_received > cl_timeout->value * 1000){
		Com_Printf("%s: Timed out.\n", Net_AdrToString(net_from));
		Cl_Disconnect();
	}
}


/*
 * Cl_LoadProgress
 */
void Cl_LoadProgress(int percent){

	cls.loading = percent;

	Cl_UpdateScreen();
}


/*
 * Cl_LoadMedia
 */
static void Cl_LoadMedia(void){

	cls.loading = 1;

	R_LoadMedia();

	S_LoadMedia();

	Cl_LoadEmits();

	Cl_LoadLocations();

	cls.key_dest = key_game;

	cls.loading = 0;
}


int precache_check;  // for autodownload of precache items
int precache_spawncount;

/*
 * Cl_RequestNextDownload
 *
 * Entry point for file downloads, or "precache" from server.  Attempt to
 * download .pak and .bsp from server.  Pak is preferred.
 */
void Cl_RequestNextDownload(void){

	if(cls.state < ca_connected)
		return;

	// check pak
	if(precache_check == CS_PAK){

		precache_check = CS_MODELS;

		if(strlen(cl.configstrings[CS_PAK])){

			if(!Cl_CheckOrDownloadFile(cl.configstrings[CS_PAK]))
				return;  // started a download
		}
	}

	// check .bsp via models
	if(precache_check == CS_MODELS){  // the map is the only model we care about

		precache_check++;

		if(!Cl_CheckOrDownloadFile(cl.configstrings[CS_MODELS + 1]))
			return;  // started a download
	}

	Cl_LoadMedia();

	Msg_WriteByte(&cls.netchan.message, clc_stringcmd);
	Msg_WriteString(&cls.netchan.message, va("begin %i\n", precache_spawncount));
}


/*
 * Cl_Precache_f
 *
 * The server sends this command once serverdata has been parsed.
 */
static void Cl_Precache_f(void){

	precache_spawncount = atoi(Cmd_Argv(1));
	precache_check = CS_PAK;

	Cl_RequestNextDownload();
}


/*
 * Cl_GetUserName
 */
static const char *Cl_GetUserName(void){
	const char *username = Sys_GetCurrentUser();

	if(username[0] == '\0')
		username = "newbie";

	return username;
}


/*
 * Cl_InitMenuFonts
 */
static void Cl_InitMenuFonts(const char *filename){
	char *buffer;
	const char *buf;
	const char *token;

	// load the file header
	if(Fs_LoadFile(filename, (void **)(char *)&buffer) == -1)
		Sys_Error("Failed to open %s\n", filename);

	buf = buffer;

	do {
		token = Com_Parse(&buf);
		if (!token || !buf)
			break;
		if (!strcmp(token, "font")) {
			token = Com_Parse(&buf);
			MN_ParseFont(token, &buf);
		}
	} while (true);

	Fs_FreeFile(buffer);
}


/*
 * Cl_InitMenu
 */
static void Cl_InitMenu(const char *filename){
	char *buffer;
	const char *buf;
	const char *token;

	// load the file header
	if(Fs_LoadFile(filename, (void **)(char *)&buffer) == -1)
		Sys_Error("Failed to open %s\n", filename);

	buf = buffer;

	token = Com_Parse(&buf);
	if (strcmp(token, "window"))
		Sys_Error("Failed to parse %s\n", filename);

	token = Com_Parse(&buf);
	MN_ParseMenu("window", token, &buf);

	Fs_FreeFile(buffer);
}


/*
 * Cl_InitMenus
 */
static void Cl_InitMenus(void){

	MN_Init();

	Cl_InitMenuFonts("ui/fonts.ui");

	Cl_InitMenu("ui/main.ui");
	Cl_InitMenu("ui/editor.ui");
	Cl_InitMenu("ui/game.ui");
	Cl_InitMenu("ui/create.ui");
	Cl_InitMenu("ui/options.ui");
	Cl_InitMenu("ui/credits.ui");

	Cbuf_AddText("mn_push main;");

	cls.key_dest = key_menu;
}

/*
 * Cl_InitLocal
 */
static void Cl_InitLocal(void){

	// register our variables
	cl_addentities = Cvar_Get("cl_addentities", "3", 0, NULL);
	cl_addparticles = Cvar_Get("cl_addparticles", "1", 0, NULL);
	cl_async = Cvar_Get("cl_async", "0", CVAR_ARCHIVE, NULL);
	cl_blend = Cvar_Get("cl_blend", "1", CVAR_ARCHIVE, NULL);
	cl_bob = Cvar_Get("cl_bob", "1", CVAR_ARCHIVE, NULL);
	cl_chatsound = Cvar_Get("cl_chatsound", "misc/chat", 0, NULL);
	cl_teamchatsound = Cvar_Get("cl_teamchatsound", "misc/teamchat", 0, NULL);
	cl_counters = Cvar_Get("cl_counters", "1", CVAR_ARCHIVE, NULL);
	cl_crosshair = Cvar_Get("cl_crosshair", "1", CVAR_ARCHIVE, NULL);
	cl_crosshaircolor = Cvar_Get("cl_crosshaircolor", "default", CVAR_ARCHIVE, NULL);
	cl_crosshairscale = Cvar_Get("cl_crosshairscale", "1.0", CVAR_ARCHIVE, NULL);
	cl_emits = Cvar_Get("cl_emits", "1", CVAR_ARCHIVE, NULL);
	cl_fov = Cvar_Get("cl_fov", "100.0", CVAR_ARCHIVE, NULL);
	cl_fovzoom = Cvar_Get("cl_fovzoom", "40.0", CVAR_ARCHIVE, NULL);
	cl_hud = Cvar_Get("cl_hud", "1", CVAR_ARCHIVE, NULL);
	cl_ignore = Cvar_Get("cl_ignore", "", 0, NULL);
	cl_maxfps = Cvar_Get("cl_maxfps", "0", CVAR_ARCHIVE, NULL);
	cl_maxpps = Cvar_Get("cl_maxpps", "0", CVAR_ARCHIVE, NULL);
	cl_netgraph = Cvar_Get("cl_netgraph", "1", CVAR_ARCHIVE, NULL);
	cl_predict = Cvar_Get("cl_predict", "1", 0, NULL);
	cl_showmiss = Cvar_Get("cl_showmiss", "0", 0, NULL);
	cl_shownet = Cvar_Get("cl_shownet", "0", 0, NULL);
	cl_thirdperson = Cvar_Get("cl_thirdperson", "0", CVAR_ARCHIVE,
			"Toggles the third person camera.");
	cl_timeout = Cvar_Get("cl_timeout", "15.0", 0, NULL);
	cl_viewsize = Cvar_Get("cl_viewsize", "100.0", CVAR_ARCHIVE, NULL);
	cl_weapon = Cvar_Get("cl_weapon", "1", CVAR_ARCHIVE, NULL);
	cl_weather = Cvar_Get("cl_weather", "1", CVAR_ARCHIVE, NULL);

	rcon_password = Cvar_Get("rcon_password", "", 0, NULL);
	rcon_address = Cvar_Get("rcon_address", "", 0, NULL);

	// userinfo
	color = Cvar_Get("color", "default", CVAR_USERINFO | CVAR_ARCHIVE, NULL);
	msg = Cvar_Get("msg", "1", CVAR_USERINFO | CVAR_ARCHIVE, NULL);
	name = Cvar_Get("name", Cl_GetUserName(), CVAR_USERINFO | CVAR_ARCHIVE, NULL);
	password = Cvar_Get("password", "", CVAR_USERINFO, NULL);
	rate = Cvar_Get("rate", "10000", CVAR_USERINFO | CVAR_ARCHIVE, NULL);
	skin = Cvar_Get("skin", "ichabod/ichabod", CVAR_USERINFO | CVAR_ARCHIVE, NULL);
	recording = Cvar_Get("recording", "0", CVAR_USERINFO | CVAR_NOSET, NULL);

	// register our commands
	Cmd_AddCommand("ping", Cl_Ping_f, NULL);
	Cmd_AddCommand("servers", Cl_Servers_f, NULL);
	Cmd_AddCommand("record", Cl_Record_f, NULL);
	Cmd_AddCommand("stop", Cl_Stop_f, NULL);
	Cmd_AddCommand("quit", Cl_Quit_f, NULL);
	Cmd_AddCommand("connect", Cl_Connect_f, NULL);
	Cmd_AddCommand("reconnect", Cl_Reconnect_f, NULL);
	Cmd_AddCommand("disconnect", Cl_Disconnect_f, NULL);
	Cmd_AddCommand("rcon", Cl_Rcon_f, NULL);
	Cmd_AddCommand("precache", Cl_Precache_f, NULL);
	Cmd_AddCommand("download", Cl_Download_f, NULL);

	// forward to server commands
	Cmd_AddCommand("wave", NULL, NULL);
	Cmd_AddCommand("kill", NULL, NULL);
	Cmd_AddCommand("use", NULL, NULL);
	Cmd_AddCommand("drop", NULL, NULL);
	Cmd_AddCommand("say", NULL, NULL);
	Cmd_AddCommand("say_team", NULL, NULL);
	Cmd_AddCommand("info", NULL, NULL);
	Cmd_AddCommand("give", NULL, NULL);
	Cmd_AddCommand("god", NULL, NULL);
	Cmd_AddCommand("noclip", NULL, NULL);
	Cmd_AddCommand("weapnext", NULL, NULL);
	Cmd_AddCommand("weapprev", NULL, NULL);
	Cmd_AddCommand("weaplast", NULL, NULL);
	Cmd_AddCommand("vote", NULL, NULL);
	Cmd_AddCommand("team", NULL, NULL);
	Cmd_AddCommand("teamname", NULL, NULL);
	Cmd_AddCommand("teamskin", NULL, NULL);
	Cmd_AddCommand("spectate", NULL, NULL);
	Cmd_AddCommand("join", NULL, NULL);
	Cmd_AddCommand("score", NULL, NULL);
	Cmd_AddCommand("ready", NULL, NULL);
	Cmd_AddCommand("unready", NULL, NULL);
	Cmd_AddCommand("playerlist", NULL, NULL);
	Cmd_AddCommand("configstrings", NULL, NULL);
	Cmd_AddCommand("baselines", NULL, NULL);

	Cl_InitMenus();
}


/*
 * Cl_WriteConfiguration
 *
 * Writes key bindings and archived cvars to quake2world.cfg.
 */
static void Cl_WriteConfiguration(void){
	FILE *f;
	char path[MAX_OSPATH];

	if(cls.state == ca_uninitialized)
		return;

	snprintf(path, sizeof(path), "%s/quake2world.cfg", Fs_Gamedir());
	f = fopen(path, "w");
	if(!f){
		Com_Warn("Couldn't write %s.\n", path);
		return;
	}

	fprintf(f, "// generated by Quake2World, do not modify\n");
	Cl_WriteBindings(f);
	Fs_CloseFile(f);

	Cvar_WriteVars(path);
}


/*
 * Cl_Frame
 */
void Cl_Frame(int msec){
	qboolean packet_frame = true, render_frame = true;
	int ms;

	if(dedicated->value)
		return;

	cls.packet_delta += msec;
	cls.render_delta += msec;

	if(cl_maxfps->modified){  // ensure frame caps are sane

		if(cl_maxfps->value > 0.0 && cl_maxfps->value < 30.0)
			cl_maxfps->value = 30.0;

		cl_maxfps->modified = false;
	}

	if(cl_maxpps->modified){

		if(cl_maxpps->value > 0.0 && cl_maxpps->value < 20.0)
			cl_maxpps->value = 20.0;

		cl_maxpps->modified = false;
	}

	if(timedemo->value){  // accumulate timedemo statistics

		if(!cl.timedemo_start)
			cl.timedemo_start = cls.realtime;

		cl.timedemo_frames++;
	}
	else {  // check framerate cap conditions

		if(cl_maxfps->value > 0.0){  // cap render framerate
			ms = 1000.0 * timescale->value / cl_maxfps->value;

			if(cls.render_delta < ms)
				render_frame = false;
		}

		if(cl_maxpps->value > 0.0){  // cap net framerate
			ms = 1000.0 * timescale->value / cl_maxpps->value;

			if(cls.packet_delta < ms)
				packet_frame = false;
		}
	}

	if(!cl_async->value)  // run synchronous
		packet_frame = render_frame;

	if(!render_frame || cls.packet_delta < 10)
		packet_frame = false;  // enforce a soft cap of 100pps

	if(cls.state == ca_connected && cls.packet_delta < 50)
		packet_frame = false;  // dont spam the server while downloading

	if(cls.state <= ca_disconnected && !Com_ServerState()){
		usleep(1000);  // idle at console
	}

	cl.time += msec;  // update time references
	cls.realtime = curtime;

	if(render_frame){
		// fetch updates from server
		Cl_ReadPackets();

		// execute any pending console commands
		Cbuf_Execute();

		// fetch input from user
		Cl_HandleEvents();

		// and add it to the current command
		Cl_UpdateCmd();

		// predict all unacknowledged movements
		Cl_PredictMovement();

		// update the screen
		Cl_UpdateScreen();

		// update audio
		S_Frame();

		cls.render_delta = 0;
	}

	if(packet_frame || userinfo_modified){
		// send command to the server
		Cl_SendCmd();

		// resend a connection request if necessary
		Cl_CheckForResend();

		// run http downloads
		Cl_HttpDownloadThink();

		cls.packet_delta = 0;
	}
}

#include "binds.h"

/*
 * Cl_Init
 */
void Cl_Init(void){

	if(dedicated->value)
		return;  // nothing running on the client

	memset(&cls, 0, sizeof(cls));

	cls.state = ca_disconnected;
	cls.realtime = curtime;

	Cl_InitKeys();

	Cbuf_AddText(DEFAULT_BINDS);
	Cbuf_Execute();
	Cbuf_AddText("exec quake2world.cfg\n");
	Cbuf_Execute();

	Net_Config(NS_CLIENT, true);

	S_Init();

	R_Init();

	net_message.data = net_message_buffer;
	net_message.maxsize = sizeof(net_message_buffer);

	Cl_InitLocal();

	Cl_InitConsole();

	Cl_InitView();

	Cl_InitInput();

	Cl_InitHttpDownload();

	Cl_InitLocations();

	Fs_ExecAutoexec();

	Cl_ClearState();
}


/*
 * Cl_Shutdown
 */
void Cl_Shutdown(void){
	static qboolean isdown = false;

	if(isdown){
		printf("Recursive shutdown..\n");
		return;
	}

	isdown = true;

	MN_Shutdown();

	Cl_ShutdownHttpDownload();

	Cl_WriteConfiguration();

	Cl_ShutdownLocations();

	Cl_FreeServers();

	S_Shutdown();

	Cl_ShutdownKeys();

	R_Shutdown();
}
