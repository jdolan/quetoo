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

#include "cl_local.h"

cvar_t *cl_add_emits;
cvar_t *cl_add_entities;
cvar_t *cl_add_particles;
cvar_t *cl_async;
cvar_t *cl_bob;
cvar_t *cl_chat_sound;
cvar_t *cl_counters;
cvar_t *cl_fov;
cvar_t *cl_fov_zoom;
cvar_t *cl_ignore;
cvar_t *cl_max_fps;
cvar_t *cl_max_pps;
cvar_t *cl_net_graph;
cvar_t *cl_predict;
cvar_t *cl_show_prediction_misses;
cvar_t *cl_show_net_messages;
cvar_t *cl_show_renderer_stats;
cvar_t *cl_team_chat_sound;
cvar_t *cl_third_person;
cvar_t *cl_timeout;
cvar_t *cl_view_size;
cvar_t *cl_weapon;
cvar_t *cl_weather;

cvar_t *rcon_password;
cvar_t *rcon_address;

// user info
cvar_t *color;
cvar_t *message_level;
cvar_t *name;
cvar_t *password;
cvar_t *rate;
cvar_t *skin;

cvar_t *recording;

cl_static_t cls;
cl_client_t cl;

extern void Sv_ShutdownServer(const char *msg);

/*
 * Cl_SendConnect
 *
 * We have gotten a challenge from the server, so try and connect.
 */
static void Cl_SendConnect(void) {
	net_addr_t addr;
	byte qport;

	memset(&addr, 0, sizeof(addr));

	if (!Net_StringToNetaddr(cls.server_name, &addr)) {
		Com_Print("Bad server address\n");
		cls.connect_time = 0;
		return;
	}

	if (addr.port == 0) // use default port
		addr.port = BigShort(PORT_SERVER);

	qport = (byte) Cvar_GetValue("net_qport"); // has been set by netchan

	Netchan_OutOfBandPrint(NS_CLIENT, addr, "connect %i %i %i \"%s\"\n",
			PROTOCOL, qport, cls.challenge, Cvar_UserInfo());

	user_info_modified = false;
}

/*
 * Cl_CheckForResend
 *
 * Re-send a connect message if the last one has timed out.
 */
static void Cl_CheckForResend(void) {
	net_addr_t addr;

	// if the local server is running and we aren't then connect
	if (Com_WasInit(Q2W_SERVER) && strcmp(cls.server_name, "localhost")) {

		if (cls.state > CL_DISCONNECTED) {
			Cl_Disconnect();
		}

		strncpy(cls.server_name, "localhost", sizeof(cls.server_name) - 1);

		cls.state = CL_CONNECTING;
		cls.connect_time = -99999;
	}

	// re-send if we haven't received a reply yet
	if (cls.state != CL_CONNECTING)
		return;

	if (cls.real_time - cls.connect_time < 3000)
		return;

	if (!Net_StringToNetaddr(cls.server_name, &addr)) {
		Com_Print("Bad server address\n");
		cls.state = CL_DISCONNECTED;
		return;
	}

	if (addr.port == 0)
		addr.port = BigShort(PORT_SERVER);

	cls.connect_time = cls.real_time; // for retransmit requests

	Com_Print("Connecting to %s...\n", cls.server_name);

	Netchan_OutOfBandPrint(NS_CLIENT, addr, "get_challenge\n");
}

/*
 * Cl_Connect_f
 */
static void Cl_Connect_f(void) {

	if (Cmd_Argc() != 2) {
		Com_Print("Usage: %s <address>\n", Cmd_Argv(0));
		return;
	}

	if (Com_WasInit(Q2W_SERVER)) { // if running a local server, kill it
		Sv_ShutdownServer("Server quit.\n");
	}

	Cl_Disconnect();

	strncpy(cls.server_name, Cmd_Argv(1), sizeof(cls.server_name) - 1);

	cls.connect_time = -99999; // fire immediately
	cls.state = CL_CONNECTING;
}

/*
 * Cl_Rcon_f
 *
 * Send the rest of the command line over as an unconnected command.
 */
static void Cl_Rcon_f(void) {
	char message[1024];
	int i;
	net_addr_t to;

	if (!rcon_password->string) {
		Com_Print("No rcon_password set.\n");
		return;
	}

	memset(&to, 0, sizeof(to));

	message[0] = (char) 255;
	message[1] = (char) 255;
	message[2] = (char) 255;
	message[3] = (char) 255;
	message[4] = 0;

	strcat(message, "rcon ");
	strcat(message, rcon_password->string);
	strcat(message, " ");

	for (i = 1; i < Cmd_Argc(); i++) {
		strcat(message, Cmd_Argv(i));
		strcat(message, " ");
	}

	if (cls.state >= CL_CONNECTED)
		to = cls.netchan.remote_address;
	else {
		if (*rcon_address->string == '\0') {
			Com_Print("Not connected and no rcon_address set.\n");
			return;
		}
		Net_StringToNetaddr(rcon_address->string, &to);
		if (to.port == 0)
			to.port = BigShort(PORT_SERVER);
	}

	Net_SendPacket(NS_CLIENT, strlen(message) + 1, message, to);
}

/*
 * Cl_ClearState
 */
void Cl_ClearState(void) {

	Cl_ClearEffects();

	// wipe the entire cl_client_t structure
	memset(&cl, 0, sizeof(cl));
	Com_QuitSubsystem(Q2W_CLIENT);

	Sb_Clear(&cls.netchan.message);
}

/*
 * Cl_SendDisconnect
 *
 * Sends the disconnect command to the server (several times).  This is used
 * when the client actually wishes to disconnect or quit, or when an HTTP
 * download has begun.  This way, the client does not waste a server slot
 * (or just timeout) while downloading a level.
 */
void Cl_SendDisconnect(void) {
	byte final[16];

	if (cls.state <= CL_DISCONNECTED)
		return;

	Com_Print("Disconnecting from %s...\n", cls.server_name);

	// send a disconnect message to the server
	final[0] = CL_CMD_STRING;
	strcpy((char *)final + 1, "disconnect");
	Netchan_Transmit(&cls.netchan, strlen((char *) final), final);
	Netchan_Transmit(&cls.netchan, strlen((char *) final), final);
	Netchan_Transmit(&cls.netchan, strlen((char *) final), final);

	Cl_ClearState();

	cls.broadcast_time = 0;
	cls.connect_time = 0;
	cls.loading = 0;
	cls.packet_delta = 9999;
	cls.state = CL_DISCONNECTED;
}

/*
 * Cl_Disconnect
 *
 * Sends a disconnect message to the current server, stops any pending
 * demo recording, and updates cls.state so that we drop to console.
 */
void Cl_Disconnect(void) {

	if (cls.state <= CL_DISCONNECTED)
		return;

	if (time_demo->value) { // summarize time_demo results

		const float s = (cls.real_time - cl.time_demo_start) / 1000.0;

		Com_Print("%i frames, %3.2f seconds: %4.2ffps\n", cl.time_demo_frames,
				s, cl.time_demo_frames / s);

		cl.time_demo_frames = cl.time_demo_start = 0;
	}

	Cl_SendDisconnect(); // tell the server to deallocate us

	if (cls.demo_file) { // stop demo recording
		Cl_Stop_f();
	}

	// stop download
	if (cls.download.file) {

		if (cls.download.http) // clean up http downloads
			Cl_HttpDownloadCleanup();
		else
			// or just stop legacy ones
			Fs_CloseFile(cls.download.file);

		cls.download.file = NULL;
	}

	memset(cls.server_name, 0, sizeof(cls.server_name));

	cls.key_state.dest = KEY_UI;
}

/*
 * Cl_Disconnect_f
 */
static void Cl_Disconnect_f(void) {

	if (Com_WasInit(Q2W_SERVER)) { // if running a local server, kill it
		Sv_ShutdownServer("Disconnected.\n");
	}

	Cl_Disconnect();
}

/*
 * Cl_Reconnect_f
 */
void Cl_Reconnect_f(void) {

	if (cls.download.file) // don't disrupt downloads
		return;

	if (cls.server_name[0] != '\0') {

		if (cls.state >= CL_CONNECTING) {
			char server_name[MAX_OSPATH];

			strncpy(server_name, cls.server_name, sizeof(server_name) - 1);

			Cl_Disconnect();

			strncpy(cls.server_name, server_name, sizeof(cls.server_name) - 1);
		}

		cls.connect_time = -99999; // fire immediately
		cls.state = CL_CONNECTING;
	} else {
		Com_Print("No server to reconnect to\n");
	}
}

/*
 * Cl_ConnectionlessPacket
 *
 * Responses to broadcasts, etc
 */
static void Cl_ConnectionlessPacket(void) {
	char *s;
	char *c;
	byte qport;

	Msg_BeginReading(&net_message);
	Msg_ReadLong(&net_message); // skip the -1

	s = Msg_ReadStringLine(&net_message);

	Cmd_TokenizeString(s);

	c = Cmd_Argv(0);

	Com_Debug("%s: %s\n", Net_NetaddrToString(net_from), c);

	// server connection
	if (!strcmp(c, "client_connect")) {

		if (cls.state == CL_CONNECTED) {
			Com_Print("Dup connect received.  Ignored.\n");
			return;
		}

		qport = (byte) Cvar_GetValue("net_qport");
		Netchan_Setup(NS_CLIENT, &cls.netchan, net_from, qport);
		Msg_WriteChar(&cls.netchan.message, CL_CMD_STRING);
		Msg_WriteString(&cls.netchan.message, "new");
		cls.state = CL_CONNECTED;

		memset(cls.download_url, 0, sizeof(cls.download_url));
		if (Cmd_Argc() == 2) // http download url
			strncpy(cls.download_url, Cmd_Argv(1), sizeof(cls.download_url) - 1);
		return;
	}

	// server responding to a status broadcast
	if (!strcmp(c, "info")) {
		Cl_ParseStatusMessage();
		return;
	}

	// print command from somewhere
	if (!strcmp(c, "print")) {
		s = Msg_ReadString(&net_message);
		Com_Print("%s", s);
		return;
	}

	// ping from somewhere
	if (!strcmp(c, "ping")) {
		Netchan_OutOfBandPrint(NS_CLIENT, net_from, "ack");
		return;
	}

	// servers list from master
	if (!strcmp(c, "servers")) {
		Cl_ParseServersList();
		return;
	}

	// challenge from the server we are connecting to
	if (!strcmp(c, "challenge")) {
		if (cls.state != CL_CONNECTING) {
			Com_Print("Duplicate challenge received.  Ignored.\n");
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
static void Cl_ReadPackets(void) {

	memset(&net_from, 0, sizeof(net_from));

	while (Net_GetPacket(NS_CLIENT, &net_from, &net_message)) {

		// remote command packet
		if (*(int *) net_message.data == -1) {
			Cl_ConnectionlessPacket();
			continue;
		}

		if (cls.state <= CL_CONNECTING)
			continue; // dump it if not connected

		if (net_message.size < 8) {
			Com_Debug("%s: Runt packet.\n", Net_NetaddrToString(net_from));
			continue;
		}

		// packet from server
		if (!Net_CompareNetaddr(net_from, cls.netchan.remote_address)) {
			Com_Debug("%s: Sequenced packet without connection.\n",
					Net_NetaddrToString(net_from));
			continue;
		}

		if (!Netchan_Process(&cls.netchan, &net_message))
			continue; // wasn't accepted for some reason

		Cl_ParseServerMessage();
	}

	// check timeout
	if (cls.state >= CL_CONNECTED && cls.real_time - cls.netchan.last_received
			> cl_timeout->value * 1000) {
		Com_Print("%s: Timed out.\n", Net_NetaddrToString(net_from));
		Cl_Disconnect();
	}
}

/*
 * Cl_LoadProgress
 */
void Cl_LoadProgress(unsigned short percent) {

	cls.loading = percent;

	Cl_UpdateScreen();
}

/*
 * Cl_UpdateMedia
 *
 * Reload stale media references on subsystem restarts.
 */
static void Cl_UpdateMedia(void) {

	if ((r_view.update || s_env.update) && (cls.state == CL_ACTIVE
			&& !cls.loading)) {

		Com_Debug("Cl_UpdateMedia: %s %s\n", r_view.update ? "view" : "",
				s_env.update ? "sound" : "");

		cls.loading = 1;

		Cl_LoadClients();

		Cl_LoadEffects();

		Cl_LoadEmits();

		Cl_UpdateEntities();

		cls.cgame->UpdateMedia();

		cls.loading = 0;
	}
}

/*
 * Cl_LoadMedia
 *
 * Load all game media through the relevant subsystems. This is called when
 * spawning into a server. For incremental reloads on subsystem restarts,
 * see Cl_UpdateMedia.
 */
static void Cl_LoadMedia(void) {

	cls.loading = 1;

	R_LoadMedia();

	S_LoadMedia();

	Cl_LoadClients();

	Cl_LoadEffects();

	Cl_LoadEmits();

	Cl_UpdateEntities();

	Cl_LoadLocations();

	cls.cgame->UpdateMedia();

	Cl_ClearNotify();

	cls.key_state.dest = KEY_GAME;

	cls.loading = 0;
}

static int precache_check; // for auto-download of precache items

/*
 * Cl_RequestNextDownload
 *
 * Entry point for file downloads, or "precache" from server.  Attempt to
 * download .pak and .bsp from server.  Pak is preferred. Once all precache
 * checks are completed, we load media and ask the server to begin sending
 * us frames.
 */
void Cl_RequestNextDownload(void) {

	if (cls.state < CL_CONNECTED)
		return;

	// check pak
	if (precache_check == CS_PAK) {

		precache_check = CS_MODELS;

		if (*cl.config_strings[CS_PAK] != '\0') {

			if (!Cl_CheckOrDownloadFile(cl.config_strings[CS_PAK]))
				return; // started a download
		}
	}

	// check .bsp via models
	if (precache_check == CS_MODELS) { // the map is the only model we care about

		precache_check++;

		if (*cl.config_strings[CS_MODELS + 1] != '\0') {

			if (!Cl_CheckOrDownloadFile(cl.config_strings[CS_MODELS + 1]))
				return; // started a download
		}
	}

	// we're good to go, lock and load (literally)

	Com_InitSubsystem(Q2W_CLIENT);
	Cvar_ResetLocalVars();

	Cl_LoadMedia();

	Msg_WriteByte(&cls.netchan.message, CL_CMD_STRING);
	Msg_WriteString(&cls.netchan.message, va("begin %i\n", cls.spawn_count));
}

/*
 * Cl_Precache_f
 *
 * The server sends this command just after server_data. Hang onto the spawn
 * count and check for the media we'll need to enter the game.
 */
static void Cl_Precache_f(void) {

	cls.spawn_count = strtoul(Cmd_Argv(1), NULL, 0);

	precache_check = CS_PAK;

	Cl_RequestNextDownload();
}

/*
 * Cl_GetUserName
 */
static const char *Cl_GetUserName(void) {
	const char *username = Sys_GetCurrentUser();

	if (username[0] == '\0')
		username = "newbie";

	return username;
}

/*
 * Cl_InitLocal
 */
static void Cl_InitLocal(void) {

	// register our variables
	cl_add_emits = Cvar_Get("cl_add_emits", "1", CVAR_LO_ONLY, NULL);
	cl_add_entities = Cvar_Get("cl_add_entities", "3", CVAR_LO_ONLY, NULL);
	cl_add_particles = Cvar_Get("cl_add_particles", "1", CVAR_LO_ONLY, NULL);
	cl_async = Cvar_Get("cl_async", "0", CVAR_ARCHIVE, NULL);
	cl_bob = Cvar_Get("cl_bob", "1", CVAR_ARCHIVE, NULL);
	cl_chat_sound = Cvar_Get("cl_chat_sound", "misc/chat", 0, NULL);
	cl_counters = Cvar_Get("cl_counters", "1", CVAR_ARCHIVE, NULL);
	cl_fov = Cvar_Get("cl_fov", "100.0", CVAR_ARCHIVE, NULL);
	cl_fov_zoom = Cvar_Get("cl_fov_zoom", "40.0", CVAR_ARCHIVE, NULL);
	cl_ignore = Cvar_Get("cl_ignore", "", 0, NULL);
	cl_max_fps = Cvar_Get("cl_max_fps", "0", CVAR_ARCHIVE, NULL);
	cl_max_pps = Cvar_Get("cl_max_pps", "0", CVAR_ARCHIVE, NULL);
	cl_net_graph = Cvar_Get("cl_net_graph", "1", CVAR_ARCHIVE, NULL);
	cl_predict = Cvar_Get("cl_predict", "1", 0, NULL);
	cl_show_prediction_misses = Cvar_Get("cl_show_prediction_misses", "0",
			CVAR_LO_ONLY, NULL);
	cl_show_net_messages = Cvar_Get("cl_show_net_messages", "0", CVAR_LO_ONLY,
			NULL);
	cl_show_renderer_stats = Cvar_Get("cl_show_renderer_stats", "0",
			CVAR_LO_ONLY, NULL);
	cl_team_chat_sound = Cvar_Get("cl_team_chat_sound", "misc/teamchat", 0,
			NULL);
	cl_third_person = Cvar_Get("cl_third_person", "0", CVAR_ARCHIVE,
			"Toggles the third person camera.");
	cl_timeout = Cvar_Get("cl_timeout", "15.0", 0, NULL);
	cl_view_size = Cvar_Get("cl_view_size", "100.0", CVAR_ARCHIVE, NULL);
	cl_weapon = Cvar_Get("cl_weapon", "1", CVAR_ARCHIVE, NULL);
	cl_weather = Cvar_Get("cl_weather", "1", CVAR_ARCHIVE, NULL);

	rcon_password = Cvar_Get("rcon_password", "", 0, NULL);
	rcon_address = Cvar_Get("rcon_address", "", 0, NULL);

	// user info
	color = Cvar_Get("color", "default", CVAR_USER_INFO | CVAR_ARCHIVE, NULL);
	message_level = Cvar_Get("message_level", "0",
			CVAR_USER_INFO | CVAR_ARCHIVE, NULL);
	name = Cvar_Get("name", Cl_GetUserName(), CVAR_USER_INFO | CVAR_ARCHIVE,
			NULL);
	password = Cvar_Get("password", "", CVAR_USER_INFO, NULL);
	rate = Cvar_Get("rate", va("%d", CLIENT_RATE),
			CVAR_USER_INFO | CVAR_ARCHIVE, NULL);
	skin = Cvar_Get("skin", "qforcer/enforcer", CVAR_USER_INFO | CVAR_ARCHIVE,
			NULL);
	recording = Cvar_Get("recording", "0", CVAR_USER_INFO | CVAR_NO_SET, NULL);

	// register our commands
	Cmd_AddCommand("ping", Cl_Ping_f, NULL);
	Cmd_AddCommand("servers", Cl_Servers_f, NULL);
	Cmd_AddCommand("record", Cl_Record_f, NULL);
	Cmd_AddCommand("fast_forward", Cl_FastForward_f, NULL);
	Cmd_AddCommand("slow_motion", Cl_SlowMotion_f, NULL);
	Cmd_AddCommand("stop", Cl_Stop_f, NULL);
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
	Cmd_AddCommand("no_clip", NULL, NULL);
	Cmd_AddCommand("weapon_next", NULL, NULL);
	Cmd_AddCommand("weapon_previous", NULL, NULL);
	Cmd_AddCommand("weapon_last", NULL, NULL);
	Cmd_AddCommand("vote", NULL, NULL);
	Cmd_AddCommand("team", NULL, NULL);
	Cmd_AddCommand("team_name", NULL, NULL);
	Cmd_AddCommand("team_skin", NULL, NULL);
	Cmd_AddCommand("spectate", NULL, NULL);
	Cmd_AddCommand("join", NULL, NULL);
	Cmd_AddCommand("score", NULL, NULL);
	Cmd_AddCommand("ready", NULL, NULL);
	Cmd_AddCommand("unready", NULL, NULL);
	Cmd_AddCommand("player_list", NULL, NULL);
	Cmd_AddCommand("config_strings", NULL, NULL);
	Cmd_AddCommand("baselines", NULL, NULL);

	// forward anything we don't handle locally to the server
	Cmd_ForwardToServer = Cl_ForwardCmdToServer;
}

/*
 * Cl_WriteConfiguration
 *
 * Writes key bindings and archived cvars to quake2world.cfg.
 */
static void Cl_WriteConfiguration(void) {
	FILE *f;
	char path[MAX_OSPATH];

	if (cls.state == CL_UNINITIALIZED)
		return;

	snprintf(path, sizeof(path), "%s/quake2world.cfg", Fs_Gamedir());
	f = fopen(path, "w");
	if (!f) {
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
void Cl_Frame(unsigned int msec) {
	boolean_t packet_frame = true, render_frame = true;
	unsigned int ms;

	if (dedicated->value)
		return;

	// update time reference
	cls.real_time = quake2world.time;

	// increment the server time as well
	cl.time += msec;

	cls.packet_delta += msec;
	cls.render_delta += msec;

	if (cl_max_fps->modified) { // ensure frame caps are sane

		if (cl_max_fps->value > 0.0 && cl_max_fps->value < 30.0)
			cl_max_fps->value = 30.0;

		cl_max_fps->modified = false;
	}

	if (cl_max_pps->modified) {

		if (cl_max_pps->value > 0.0 && cl_max_pps->value < 20.0)
			cl_max_pps->value = 20.0;

		cl_max_pps->modified = false;
	}

	if (time_demo->value) { // accumulate timed demo statistics

		if (!cl.time_demo_start)
			cl.time_demo_start = cls.real_time;

		cl.time_demo_frames++;
	} else { // check frame rate cap conditions

		if (cl_max_fps->value > 0.0) { // cap render frame rate
			ms = 1000.0 * time_scale->value / cl_max_fps->value;

			if (cls.render_delta < ms)
				render_frame = false;
		}

		if (cl_max_pps->value > 0.0) { // cap net frame rate
			ms = 1000.0 * time_scale->value / cl_max_pps->value;

			if (cls.packet_delta < ms)
				packet_frame = false;
		}
	}

	if (!cl_async->value) // run synchronous
		packet_frame = render_frame;

	if (!render_frame || cls.packet_delta < 10)
		packet_frame = false; // enforce a soft cap of 100pps

	if (cls.state == CL_CONNECTED && cls.packet_delta < 50)
		packet_frame = false; // don't flood the server while downloading

	if (cls.state <= CL_DISCONNECTED && !Com_WasInit(Q2W_SERVER)) {
		usleep(1000); // idle at console
	}

	if (render_frame) {
		// update any stale media references
		Cl_UpdateMedia();

		// fetch updates from server
		Cl_ReadPackets();

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

	if (packet_frame || user_info_modified) {
		// send command to the server
		Cl_SendCmd();

		// resend a connection request if necessary
		Cl_CheckForResend();

		// run http downloads
		Cl_HttpDownloadThink();

		cls.packet_delta = 0;
	}
}

#include "cl_binds.h"

/*
 * Cl_Init
 */
void Cl_Init(void) {

	memset(&cls, 0, sizeof(cls));

	if (dedicated->value)
		return; // nothing running on the client

	cls.state = CL_DISCONNECTED;
	cls.real_time = quake2world.time;

	Cl_InitKeys();

	Cbuf_AddText(DEFAULT_BINDS);
	Cbuf_AddText("exec quake2world.cfg\n");
	Cbuf_Execute();

	Net_Config(NS_CLIENT, true);

	S_Init();

	R_Init();

	net_message.data = net_message_buffer;
	net_message.max_size = sizeof(net_message_buffer);

	Cl_InitLocal();

	Cl_InitConsole();

	Cl_InitView();

	Cl_InitInput();

	Cl_InitHttpDownload();

	Cl_InitLocations();

	Fs_ExecAutoexec();

	Cl_ClearState();

	Ui_Init();

	Cl_InitCgame();

	cls.key_state.dest = KEY_UI;
}

/*
 * Cl_Shutdown
 */
void Cl_Shutdown(void) {

	if (dedicated->value)
		return;

	Cl_Disconnect();

	Cl_ShutdownCgame();

	Cl_ShutdownHttpDownload();

	Cl_WriteConfiguration();

	Cl_ShutdownLocations();

	Cl_FreeServers();

	Cl_ShutdownKeys();

	Ui_Shutdown();

	S_Shutdown();

	R_Shutdown();
}
