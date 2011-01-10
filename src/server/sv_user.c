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

edict_t *sv_player;

/*
 *
 * USER STRINGCMD EXECUTION
 *
 * sv_client and sv_player will be valid.
 */


/*
 * Sv_BeginDemoServer
 *
 * Begin a demo server.  It is expected that the demo protocol has already
 * been resolved by Sv_CheckDemo.  Therefore we simply open it.
 */
static void Sv_BeginDemoServer(void){
	char demo[MAX_OSPATH];

	Com_Debug("Sv_BeginDemoServer\n");

	snprintf(demo, sizeof(demo), "demos/%s", sv.name);
	Fs_OpenFile(demo, &sv.demo_file, FILE_READ);

	if(!sv.demo_file)  // file was deleted during spawnserver
		Com_Error(ERR_DROP, "Sv_BeginDemoServer: %s no longer exists.\n", demo);
}


/*
 * Sv_New_f
 *
 * Sends the first message from the server to a connected client.
 * This will be sent on the initial connection and upon each server load.
 */
static void Sv_New_f(void){
	char *gamedir;
	int player_num;
	edict_t *ent;

	Com_Debug("New() from %s\n", sv_client->name);

	if(sv_client->state != cs_connected){
		Com_Print("New not valid -- already spawned\n");
		return;
	}

	// demo servers just dump the file message
	if(sv.state == ss_demo){
		Sv_BeginDemoServer();
		return;
	}

	// serverdata required to make sure the protocol is right, and to set the gamedir
	gamedir = Cvar_GetString("gamedir");

	// send the serverdata
	Msg_WriteByte(&sv_client->netchan.message, svc_server_data);
	Msg_WriteLong(&sv_client->netchan.message, PROTOCOL);
	Msg_WriteLong(&sv_client->netchan.message, svs.spawn_count);
	Msg_WriteLong(&sv_client->netchan.message, svs.frame_rate);
	Msg_WriteByte(&sv_client->netchan.message, 0);
	Msg_WriteString(&sv_client->netchan.message, gamedir);

	player_num = sv_client - svs.clients;
	Msg_WriteShort(&sv_client->netchan.message, player_num);

	// send full levelname
	Msg_WriteString(&sv_client->netchan.message, sv.config_strings[CS_NAME]);

	// set up the entity for the client
	ent = EDICT_FOR_NUM(player_num + 1);
	ent->s.number = player_num + 1;
	sv_client->edict = ent;
	memset(&sv_client->last_cmd, 0, sizeof(sv_client->last_cmd));

	// begin fetching config_strings
	Msg_WriteByte(&sv_client->netchan.message, svc_stuff_text);
	Msg_WriteString(&sv_client->netchan.message, va("config_strings %i 0\n", svs.spawn_count));
}


/*
 * Sv_ConfigStrings_f
 */
static void Sv_ConfigStrings_f(void){
	int start;

	Com_Debug("ConfigStrings() from %s\n", sv_client->name);

	if(sv_client->state != cs_connected){
		Com_Print("ConfigStrings not valid -- already spawned\n");
		return;
	}

	// handle the case of a level changing while a client was connecting
	if(atoi(Cmd_Argv(1)) != svs.spawn_count){
		Com_Print("Sv_ConfigStrings_f from different level\n");
		Sv_New_f();
		return;
	}

	start = atoi(Cmd_Argv(2));

	if(start < 0){  // catch negative offset
		Com_Print("Illegal config_string offset from %s\n", Sv_NetaddrToString(sv_client));
		Sv_KickClient(sv_client, NULL);
		return;
	}

	// write a packet full of data
	while(sv_client->netchan.message.size < MAX_MSGLEN / 2
			&& start < MAX_CONFIG_STRINGS){
		if(sv.config_strings[start][0]){
			Msg_WriteByte(&sv_client->netchan.message, svc_config_string);
			Msg_WriteShort(&sv_client->netchan.message, start);
			Msg_WriteString(&sv_client->netchan.message, sv.config_strings[start]);
		}
		start++;
	}

	// send next command
	if(start == MAX_CONFIG_STRINGS){
		Msg_WriteByte(&sv_client->netchan.message, svc_stuff_text);
		Msg_WriteString(&sv_client->netchan.message, va("baselines %i 0\n", svs.spawn_count));
	} else {
		Msg_WriteByte(&sv_client->netchan.message, svc_stuff_text);
		Msg_WriteString(&sv_client->netchan.message, va("config_strings %i %i\n", svs.spawn_count, start));
	}
}

/*
 * Sv_Baselines_f
 */
static void Sv_Baselines_f(void){
	int start;
	entity_state_t nullstate;
	entity_state_t *base;

	Com_Debug("Baselines() from %s\n", sv_client->name);

	if(sv_client->state != cs_connected){
		Com_Print("baselines not valid -- already spawned\n");
		return;
	}

	// handle the case of a level changing while a client was connecting
	if(atoi(Cmd_Argv(1)) != svs.spawn_count){
		Com_Print("Sv_Baselines_f from different level\n");
		Sv_New_f();
		return;
	}

	start = atoi(Cmd_Argv(2));

	if(start < 0){  // catch negative offset
		Com_Print("Illegal baseline offset from %s\n", Sv_NetaddrToString(sv_client));
		Sv_KickClient(sv_client, NULL);
		return;
	}

	memset(&nullstate, 0, sizeof(nullstate));

	// write a packet full of data
	while(sv_client->netchan.message.size < MAX_MSGLEN / 2
			&& start < MAX_EDICTS){
		base = &sv.baselines[start];
		if(base->model_index || base->sound || base->effects){
			Msg_WriteByte(&sv_client->netchan.message, svc_spawn_baseline);
			Msg_WriteDeltaEntity(&nullstate, base, &sv_client->netchan.message, true, true);
		}
		start++;
	}

	// send next command
	if(start == MAX_EDICTS){
		Msg_WriteByte(&sv_client->netchan.message, svc_stuff_text);
		Msg_WriteString(&sv_client->netchan.message, va("precache %i\n", svs.spawn_count));
	} else {
		Msg_WriteByte(&sv_client->netchan.message, svc_stuff_text);
		Msg_WriteString(&sv_client->netchan.message, va("baselines %i %i\n", svs.spawn_count, start));
	}
}


/*
 * Sv_Begin_f
 */
static void Sv_Begin_f(void){
	Com_Debug("Begin() from %s\n", sv_client->name);

	if(sv_client->state != cs_connected){  // catch duplicate spawns
		Com_Print("Illegal begin from %s\n", Sv_NetaddrToString(sv_client));
		Sv_KickClient(sv_client, NULL);
		return;
	}

	if(sv.state == ss_demo)
		return;

	// handle the case of a level changing while a client was connecting
	if(atoi(Cmd_Argv(1)) != svs.spawn_count){
		Com_Print("Sv_Begin_f from different level\n");
		Sv_New_f();
		return;
	}

	sv_client->state = cs_spawned;

	// call the game begin function
	svs.game->ClientBegin(sv_player);

	Cbuf_InsertFromDefer();
}

/*
 * Sv_NextDownload_f
 */
static void Sv_NextDownload_f(void){
	int r, size, percent;
	byte buf[MAX_MSGLEN];
	size_buf_t msg;

	if(!sv_client->download)
		return;

	r = sv_client->download_size - sv_client->download_count;
	if(r > 1280)  // stock quake2 sends 1024 byte chunks
		r = 1280;  // but i see no harm in sending another 256 bytes

	Sb_Init(&msg, buf, MAX_MSGLEN);

	Msg_WriteByte(&msg, svc_download);
	Msg_WriteShort(&msg, r);

	sv_client->download_count += r;
	size = sv_client->download_size;
	if(!size)
		size = 1;

	percent = sv_client->download_count * 100 / size;
	Msg_WriteByte(&msg, percent);

	Sb_Write(&msg, sv_client->download + sv_client->download_count - r, r);
	Sb_Write(&sv_client->netchan.message, msg.data, msg.size);

	if(sv_client->download_count != sv_client->download_size)
		return;

	Fs_FreeFile(sv_client->download);
	sv_client->download = NULL;
}


// only these prefixes are valid downloads, all else are denied
static const char *downloadable[] = {
	"*.pak", "maps/*.bsp", "sounds/*", "env/*.tga",
	"textures/*.wal", "textures/*.tga", NULL
};

/*
 * Sv_Download_f
 */
static void Sv_Download_f(void){
	const char *name;
	void *buf;
	int i = 0, offset = 0;

	name = Cmd_Argv(1);

	if(Cmd_Argc() > 2)
		offset = atoi(Cmd_Argv(2)); // downloaded offset

	// catch illegal offset or file_names
	if(offset < 0 || *name == '.' || *name == '/' || *name == '\\' || strstr(name, "..")){
		Com_Print("Malicious download from %s\n", Sv_NetaddrToString(sv_client));
		Sv_KickClient(sv_client, NULL);
		return;
	}

	while(downloadable[i]){  // ensure download name is allowed
		if(Com_GlobMatch(downloadable[i], name))
			break;
		i++;
	}

	if(!downloadable[i]){  // it wasnt
		Com_Print("Illegal download (%s) from %s\n", name, Sv_NetaddrToString(sv_client));
		Sv_KickClient(sv_client, NULL);
		return;
	}

	if(!sv_udpdownload->value){  // lastly, ensure server wishes to allow
		Msg_WriteByte(&sv_client->netchan.message, svc_download);
		Msg_WriteShort(&sv_client->netchan.message, -1);
		Msg_WriteByte(&sv_client->netchan.message, 0);
		return;
	}

	if(sv_client->download)  // free last download
		Fs_FreeFile(sv_client->download);

	sv_client->download_size = Fs_LoadFile(name, &buf);
	sv_client->download = (byte *)buf;
	sv_client->download_count = offset;

	if(offset > sv_client->download_size)
		sv_client->download_count = sv_client->download_size;

	if(!sv_client->download){  // legal file_name, but missing file
		Com_Debug("Couldn't download %s to %s\n", name, sv_client->name);
		Msg_WriteByte(&sv_client->netchan.message, svc_download);
		Msg_WriteShort(&sv_client->netchan.message, -1);
		Msg_WriteByte(&sv_client->netchan.message, 0);
		return;
	}

	Sv_NextDownload_f();
	Com_Debug("Downloading %s to %s\n", name, sv_client->name);
}


/*
 * Sv_Disconnect_f
 *
 * The client is going to disconnect, so remove the connection immediately
 */
static void Sv_Disconnect_f(void){
	Sv_DropClient(sv_client);
}


/*
 * Sv_Serverinfo_f
 *
 * Dumps the serverinfo info string
 */
static void Sv_Info_f(void){
	const cvar_t *cvar;
	char line[MAX_STRING_CHARS];

	if(!sv_client){  //print to server console
		Com_PrintInfo(Cvar_ServerInfo());
		return;
	}

	for(cvar = cvar_vars; cvar; cvar = cvar->next){

		if(!(cvar->flags & CVAR_SERVER_INFO))
			continue;  //only print serverinfo cvars

		snprintf(line, sizeof(line), "%s %s\n", cvar->name, cvar->string);
		Sv_ClientPrint(EDICT_FOR_CLIENT(sv_client), PRINT_MEDIUM, "%s", line);
	}
}


typedef struct {
	char *name;
	void (*func)(void);
} ucmd_t;

ucmd_t ucmds[] = {
	{"new", Sv_New_f},
	{"config_strings", Sv_ConfigStrings_f},
	{"baselines", Sv_Baselines_f},
	{"begin", Sv_Begin_f},
	{"disconnect", Sv_Disconnect_f},
	{"info", Sv_Info_f},
	{"download", Sv_Download_f},
	{"nextdl", Sv_NextDownload_f},
	{NULL, NULL}
};

/*
 * Sv_ExecuteUserCommand
 */
static void Sv_ExecuteUserCommand(const char *s){
	ucmd_t *u;

	Cmd_TokenizeString(s);

	if(strchr(s, '\xFF')){  // catch end of message exploit
		Com_Print("Illegal command contained xFF from %s\n", Sv_NetaddrToString(sv_client));
		Sv_KickClient(sv_client, NULL);
		return;
	}

	sv_player = sv_client->edict;

	for(u = ucmds; u->name; u++)
		if(!strcmp(Cmd_Argv(0), u->name)){
			u->func();
			break;
		}

	if(!u->name && sv.state == ss_game)
		svs.game->ClientCommand(sv_player);
}

/*
 *
 * USER CMD EXECUTION
 *
 */


/*
 * Sv_ClientThink
 *
 * Account for command timeslice and pass command to game dll.
 */
static void Sv_ClientThink(sv_client_t *cl, user_cmd_t *cmd){
	cl->cmd_msec -= cmd->msec;
	svs.game->ClientThink(cl->edict, cmd);
}


#define MAX_STRINGCMDS 8
/*
 * Sv_ExecuteClientMessage
 *
 * The current net_message is parsed for the given client
 */
void Sv_ExecuteClientMessage(sv_client_t *cl){
	user_cmd_t null_cmd, oldest, old_cmd, newcmd;
	int net_drop;
	int stringCmdCount;
	qboolean move_issued;
	int last_frame;
	int c;
	char *s;

	sv_client = cl;
	sv_player = sv_client->edict;

	// only allow one move command
	move_issued = false;
	stringCmdCount = 0;

	while(true){

		if(net_message.read > net_message.size){
			Com_Print("Sv_ReadClientMessage: badread\n");
			Sv_DropClient(cl);
			return;
		}

		c = Msg_ReadByte(&net_message);
		if(c == -1)
			break;

		switch(c){
			case clc_nop:
				break;

			case clc_user_info:
				strncpy(cl->user_info, Msg_ReadString(&net_message), sizeof(cl->user_info) - 1);
				Sv_UserInfoChanged(cl);
				break;

			case clc_move:
				if(move_issued)
					return;  // someone is trying to cheat

				move_issued = true;

				last_frame = Msg_ReadLong(&net_message);
				if(last_frame != cl->last_frame){
					cl->last_frame = last_frame;
					if(cl->last_frame > -1){
						cl->frame_latency[cl->last_frame & (LATENCY_COUNTS - 1)] =
							svs.real_time - cl->frames[cl->last_frame & UPDATE_MASK].sent_time;
					}
				}

				memset(&null_cmd, 0, sizeof(null_cmd));
				Msg_ReadDeltaUsercmd(&net_message, &null_cmd, &oldest);
				Msg_ReadDeltaUsercmd(&net_message, &oldest, &old_cmd);
				Msg_ReadDeltaUsercmd(&net_message, &old_cmd, &newcmd);

				// don't start delta compression until the client is spawned
				if(cl->state != cs_spawned){
					cl->last_frame = -1;
					break;
				}

				if(null_cmd.msec > 250 || oldest.msec > 250 ||  // catch illegal msec
						old_cmd.msec > 250 || newcmd.msec > 250){
					Com_Warn("Illegal msec in usercmd from %s\n", Sv_NetaddrToString(cl));
					Sv_KickClient(cl, NULL);
					return;
				}

				net_drop = cl->netchan.dropped;
				if(net_drop < 20){
					while(net_drop > 2){
						Sv_ClientThink(cl, &cl->last_cmd);
						net_drop--;
					}
					if(net_drop > 1)
						Sv_ClientThink(cl, &oldest);
					if(net_drop > 0)
						Sv_ClientThink(cl, &old_cmd);
				}
				Sv_ClientThink(cl, &newcmd);
				cl->last_cmd = newcmd;
				break;

			case clc_string_cmd:
				s = Msg_ReadString(&net_message);

				// malicious users may try using too many string commands
				if(++stringCmdCount < MAX_STRINGCMDS)
					Sv_ExecuteUserCommand(s);

				if(cl->state == cs_free)
					return;  // disconnect command
				break;

			default:
				Com_Print("Sv_ReadClientMessage: unknown command %d\n", c);
				Sv_DropClient(cl);
				return;
		}
	}
}
