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

cvar_t *cl_async;
cvar_t *cl_chat_sound;
cvar_t *cl_draw_counters;
cvar_t *cl_draw_net_graph;
cvar_t *cl_ignore;
cvar_t *cl_max_fps;
cvar_t *cl_max_pps;
cvar_t *cl_predict;
cvar_t *cl_show_net_messages;
cvar_t *cl_show_renderer_stats;
cvar_t *cl_team_chat_sound;
cvar_t *cl_timeout;
cvar_t *cl_view_size;

cvar_t *rcon_password;
cvar_t *rcon_address;

// user info
cvar_t *active;
cvar_t *color;
cvar_t *message_level;
cvar_t *name;
cvar_t *password;
cvar_t *rate;
cvar_t *skin;

cl_static_t cls;
cl_client_t cl;

extern void Sv_ShutdownServer(const char *msg);

/*
 * @brief We have gotten a challenge from the server, so try and connect.
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

	Netchan_OutOfBandPrint(NS_CLIENT, addr, "connect %i %i %i \"%s\"\n", PROTOCOL, qport,
			cls.challenge, Cvar_UserInfo());

	user_info_modified = false;
}

/*
 * @brief Re-send a connect message if the last one has timed out.
 */
static void Cl_CheckForResend(void) {
	net_addr_t addr;

	// if the local server is running and we aren't then connect
	if (Com_WasInit(Q2W_SERVER) && strcmp(cls.server_name, "localhost")) {

		if (cls.state > CL_DISCONNECTED) {
			Cl_Disconnect();
		}

		g_strlcpy(cls.server_name, "localhost", sizeof(cls.server_name));

		cls.state = CL_CONNECTING;
		cls.connect_time = 0;
	}

	// re-send if we haven't received a reply yet
	if (cls.state != CL_CONNECTING)
		return;

	// don't flood connection packets
	if (cls.connect_time && (cls.real_time - cls.connect_time < 3000))
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
 * @brief
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

	strncpy(cls.server_name, Cmd_Argv(1), sizeof(cls.server_name));
	cls.server_name[sizeof(cls.server_name) - 1] = '\0';

	cls.state = CL_CONNECTING;
	cls.connect_time = 0; // fire immediately
}

/*
 * @brief Send the rest of the command line over as an unconnected command.
 */
static void Cl_Rcon_f(void) {
	char message[1024];
	int32_t i;
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
 * @brief Client implementation of Cmd_ForwardToServer. Any commands not recognized
 * locally by the client will be sent to the server. Some will undergo parameter
 * expansion so that players can use macros for locations, weapons, etc.
 */
static void Cl_ForwardCmdToServer(void) {

	if (cls.state <= CL_DISCONNECTED) {
		Com_Print("Not connected.\n");
		return;
	}

	const char *cmd = Cmd_Argv(0);
	const char *args = Cmd_Args();

	Msg_WriteByte(&cls.netchan.message, CL_CMD_STRING);
	Sb_Print(&cls.netchan.message, va("%s %s", cmd, args));

	//Com_Debug("Forwarding '%s %s'\n", cmd, args);
}

/*
 * @brief
 */
void Cl_ClearState(void) {

	if (Com_WasInit(Q2W_CGAME))
		cls.cgame->ClearState();

	Cl_ClearInput();

	// wipe the entire cl_client_t structure
	memset(&cl, 0, sizeof(cl));

	Com_QuitSubsystem(Q2W_CLIENT);

	Sb_Clear(&cls.netchan.message);
}

/*
 * @brief Sends the disconnect command to the server (several times). This is used
 * when the client actually wishes to disconnect or quit, or when an HTTP
 * download has begun. This way, the client does not waste a server slot
 * (or just timeout) while downloading a level.
 */
void Cl_SendDisconnect(void) {
	byte final[16];

	if (cls.state <= CL_DISCONNECTED)
		return;

	Com_Print("Disconnecting from %s...\n", cls.server_name);

	// send a disconnect message to the server
	final[0] = CL_CMD_STRING;
	strcpy((char *) final + 1, "disconnect");
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
 * @brief Sends a disconnect message to the current server, stops any pending
 * demo recording, and updates cls.state so that we drop to console.
 */
void Cl_Disconnect(void) {

	if (cls.state <= CL_DISCONNECTED)
		return;

	if (time_demo->value) { // summarize time_demo results

		const float s = (cls.real_time - cl.time_demo_start) / 1000.0;

		Com_Print("%i frames, %3.2f seconds: %4.2ffps\n", cl.time_demo_frames, s,
				cl.time_demo_frames / s);

		cl.time_demo_frames = cl.time_demo_start = 0;
	}

	Cl_SendDisconnect(); // tell the server to deallocate us

	if (cls.demo_file) { // stop demo recording
		Cl_Stop_f();
	}

	// stop download
	if (cls.download.file) {

		if (cls.download.http) // clean up http downloads
			Cl_HttpDownload_Complete();
		else
			// or just stop legacy ones
			Fs_CloseFile(cls.download.file);

		cls.download.file = NULL;
	}

	memset(cls.server_name, 0, sizeof(cls.server_name));

	cls.key_state.dest = KEY_UI;
}

/*
 * @brief
 */
static void Cl_Disconnect_f(void) {

	if (Com_WasInit(Q2W_SERVER)) { // if running a local server, kill it
		Sv_ShutdownServer("Disconnected.\n");
	}

	Cl_Disconnect();
}

/*
 * @brief
 */
void Cl_Reconnect_f(void) {

	if (cls.download.file) // don't disrupt downloads
		return;

	if (cls.server_name[0] != '\0') {

		if (cls.state >= CL_CONNECTING) {
			char server_name[MAX_OSPATH];

			g_strlcpy(server_name, cls.server_name, sizeof(server_name));

			Cl_Disconnect();

			g_strlcpy(cls.server_name, server_name, sizeof(cls.server_name));
		}

		cls.connect_time = 0; // fire immediately
		cls.state = CL_CONNECTING;
	} else {
		Com_Print("No server to reconnect to\n");
	}
}

/*
 * @brief Responses to broadcasts, etc
 */
static void Cl_ConnectionlessPacket(void) {

	Msg_BeginReading(&net_message);
	Msg_ReadLong(&net_message); // skip the -1

	const char *s = Msg_ReadStringLine(&net_message);

	Cmd_TokenizeString(s);

	const char *c = Cmd_Argv(0);

	Com_Debug("%s: %s\n", Net_NetaddrToString(net_from), c);

	// server connection
	if (!strcmp(c, "client_connect")) {

		if (cls.state == CL_CONNECTED) {
			Com_Print("Duplicate connect received. Ignored.\n");
			return;
		}

		const byte qport = (byte) Cvar_GetValue("net_qport");
		Netchan_Setup(NS_CLIENT, &cls.netchan, net_from, qport);
		Msg_WriteChar(&cls.netchan.message, CL_CMD_STRING);
		Msg_WriteString(&cls.netchan.message, "new");
		cls.state = CL_CONNECTED;

		memset(cls.download_url, 0, sizeof(cls.download_url));
		if (Cmd_Argc() == 2) { // http download url
			g_strlcpy(cls.download_url, Cmd_Argv(1), sizeof(cls.download_url));
		}
		return;
	}

	// server responding to a status broadcast
	if (!strcmp(c, "info")) {
		Cl_ParseStatusMessage();
		return;
	}

	// print32_t command from somewhere
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
			Com_Print("Duplicate challenge received. Ignored.\n");
			return;
		}
		cls.challenge = atoi(Cmd_Argv(1));
		Cl_SendConnect();
		return;
	}

	Com_Warn("Cl_ConnectionlessPacket: Unknown command: %s.\n", c);
}

/*
 * @brief
 */
static void Cl_ReadPackets(void) {

	memset(&net_from, 0, sizeof(net_from));

	while (Net_GetPacket(NS_CLIENT, &net_from, &net_message)) {

		// remote command packet
		if (*(int32_t *) net_message.data == -1) {
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
			Com_Debug("%s: Sequenced packet without connection.\n", Net_NetaddrToString(net_from));
			continue;
		}

		if (!Netchan_Process(&cls.netchan, &net_message))
			continue; // wasn't accepted for some reason

		Cl_ParseServerMessage();
	}

	// check timeout
	if (cls.state >= CL_CONNECTED && cls.real_time - cls.netchan.last_received > cl_timeout->value
			* 1000) {
		Com_Print("%s: Timed out.\n", Net_NetaddrToString(net_from));
		Cl_Disconnect();
	}
}

/*
 * @brief
 */
static const char *Cl_GetUserName(void) {
	const char *username = Sys_GetCurrentUser();

	if (username[0] == '\0')
		username = "newbie";

	return username;
}

/*
 * @brief
 */
static void Cl_InitLocal(void) {

	// register our variables
	cl_async = Cvar_Get("cl_async", "0", CVAR_ARCHIVE, NULL);
	cl_chat_sound = Cvar_Get("cl_chat_sound", "misc/chat", 0, NULL);
	cl_draw_counters = Cvar_Get("cl_draw_counters", "1", CVAR_ARCHIVE, NULL);
	cl_draw_net_graph = Cvar_Get("cl_draw_net_graph", "1", CVAR_ARCHIVE, NULL);
	cl_ignore = Cvar_Get("cl_ignore", "", 0, NULL);
	cl_max_fps = Cvar_Get("cl_max_fps", "0", CVAR_ARCHIVE, NULL);
	cl_max_pps = Cvar_Get("cl_max_pps", "0", CVAR_ARCHIVE, NULL);
	cl_predict = Cvar_Get("cl_predict", "1", 0, NULL);
	cl_show_net_messages = Cvar_Get("cl_show_net_messages", "0", CVAR_LO_ONLY, NULL);
	cl_show_renderer_stats = Cvar_Get("cl_show_renderer_stats", "0", CVAR_LO_ONLY, NULL);
	cl_team_chat_sound = Cvar_Get("cl_team_chat_sound", "misc/teamchat", 0, NULL);
	cl_timeout = Cvar_Get("cl_timeout", "15.0", 0, NULL);
	cl_view_size = Cvar_Get("cl_view_size", "100.0", CVAR_ARCHIVE, NULL);

	rcon_password = Cvar_Get("rcon_password", "", 0, NULL);
	rcon_address = Cvar_Get("rcon_address", "", 0, NULL);

	// user info
	active = Cvar_Get("active", "1", CVAR_USER_INFO | CVAR_NO_SET, NULL);
	color = Cvar_Get("color", "default", CVAR_USER_INFO | CVAR_ARCHIVE, NULL);
	message_level = Cvar_Get("message_level", "0", CVAR_USER_INFO | CVAR_ARCHIVE, NULL);
	name = Cvar_Get("name", Cl_GetUserName(), CVAR_USER_INFO | CVAR_ARCHIVE, NULL);
	password = Cvar_Get("password", "", CVAR_USER_INFO, NULL);
	rate = Cvar_Get("rate", va("%d", CLIENT_RATE), CVAR_USER_INFO | CVAR_ARCHIVE, NULL);
	skin = Cvar_Get("skin", "qforcer/enforcer", CVAR_USER_INFO | CVAR_ARCHIVE, NULL);

	// register our commands
	Cmd_AddCommand("ping", Cl_Ping_f, 0, NULL);
	Cmd_AddCommand("servers", Cl_Servers_f, 0, NULL);
	Cmd_AddCommand("record", Cl_Record_f, 0, NULL);
	Cmd_AddCommand("fast_forward", Cl_FastForward_f, 0, NULL);
	Cmd_AddCommand("servers_list", Cl_Servers_List_f, 0, NULL);
	Cmd_AddCommand("slow_motion", Cl_SlowMotion_f, 0, NULL);
	Cmd_AddCommand("stop", Cl_Stop_f, 0, NULL);
	Cmd_AddCommand("connect", Cl_Connect_f, 0, NULL);
	Cmd_AddCommand("reconnect", Cl_Reconnect_f, 0, NULL);
	Cmd_AddCommand("disconnect", Cl_Disconnect_f, 0, NULL);
	Cmd_AddCommand("rcon", Cl_Rcon_f, 0, NULL);
	Cmd_AddCommand("precache", Cl_Precache_f, 0, NULL);
	Cmd_AddCommand("download", Cl_Download_f, 0, NULL);

	// forward anything we don't handle locally to the server
	Cmd_ForwardToServer = Cl_ForwardCmdToServer;
}

/*
 * @brief Writes key bindings and archived cvars to quake2world.cfg.
 */
static void Cl_WriteConfiguration(void) {
	FILE *f;
	char path[MAX_OSPATH];

	if (cls.state == CL_UNINITIALIZED)
		return;

	g_snprintf(path, sizeof(path), "%s/quake2world.cfg", Fs_Gamedir());
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
 * @brief
 */
void Cl_Frame(uint32_t msec) {
	bool packet_frame = true, render_frame = true;
	uint32_t ms;

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
		usleep(16000); // idle at console
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
		Cl_HttpThink();

		cls.packet_delta = 0;
	}
}

#include "cl_binds.h"

/*
 * @brief
 */
void Cl_Init(void) {

	memset(&cls, 0, sizeof(cls));

	if (dedicated->value)
		return; // nothing running on the client

	cls.state = CL_DISCONNECTED;
	cls.real_time = quake2world.time;

	// initialize SDL
	if (SDL_WasInit(SDL_INIT_AUDIO | SDL_INIT_VIDEO) == 0) {
		if (SDL_Init(SDL_INIT_AUDIO | SDL_INIT_VIDEO) < 0) {
			Com_Error(ERR_FATAL, "Cl_Init: %s.\n", SDL_GetError());
		}
	}

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

	Cl_InitHttp();

	Fs_ExecAutoexec();

	Cl_ClearState();

	Ui_Init();

	Cl_InitCgame();

	cls.key_state.dest = KEY_UI;
}

/*
 * @brief
 */
void Cl_Shutdown(void) {

	if (dedicated->value)
		return;

	Cl_Disconnect();

	Cl_ShutdownCgame();

	Cl_ShutdownHttp();

	Cl_WriteConfiguration();

	Cl_FreeServers();

	Cl_ShutdownKeys();

	Ui_Shutdown();

	S_Shutdown();

	R_Shutdown();
}
