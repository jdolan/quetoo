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

	Com_Dprintf("Sv_BeginDemoServer\n");

	snprintf(demo, sizeof(demo), "demos/%s", sv.name);
	Fs_OpenFile(demo, &sv.demofile, FILE_READ);

	if(!sv.demofile)  // file was deleted during spawnserver
		Com_Error(ERR_DROP, "Sv_BeginDemoServer: %s no longer exists.", demo);
}


/*
 * Sv_New_f
 *
 * Sends the first message from the server to a connected client.
 * This will be sent on the initial connection and upon each server load.
 */
static void Sv_New_f(void){
	char *gamedir;
	int playernum;
	edict_t *ent;

	Com_Dprintf("New() from %s\n", sv_client->name);

	if(sv_client->state != cs_connected){
		Com_Printf("New not valid -- already spawned\n");
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
	Msg_WriteByte(&sv_client->netchan.message, svc_serverdata);
	Msg_WriteLong(&sv_client->netchan.message, PROTOCOL);
	Msg_WriteLong(&sv_client->netchan.message, svs.spawncount);
	Msg_WriteLong(&sv_client->netchan.message, svs.packetrate);
	Msg_WriteByte(&sv_client->netchan.message, 0);
	Msg_WriteString(&sv_client->netchan.message, gamedir);

	playernum = sv_client - svs.clients;
	Msg_WriteShort(&sv_client->netchan.message, playernum);

	// send full levelname
	Msg_WriteString(&sv_client->netchan.message, sv.configstrings[CS_NAME]);

	// set up the entity for the client
	ent = EDICT_NUM(playernum + 1);
	ent->s.number = playernum + 1;
	sv_client->edict = ent;
	memset(&sv_client->lastcmd, 0, sizeof(sv_client->lastcmd));

	// begin fetching configstrings
	Msg_WriteByte(&sv_client->netchan.message, svc_stufftext);
	Msg_WriteString(&sv_client->netchan.message, va("configstrings %i 0\n", svs.spawncount));
}


/*
 * Sv_Configstrings_f
 */
static void Sv_Configstrings_f(void){
	int start;

	Com_Dprintf("Configstrings() from %s\n", sv_client->name);

	if(sv_client->state != cs_connected){
		Com_Printf("Configstrings not valid -- already spawned\n");
		return;
	}

	// handle the case of a level changing while a client was connecting
	if(atoi(Cmd_Argv(1)) != svs.spawncount){
		Com_Printf("Sv_Configstrings_f from different level\n");
		Sv_New_f();
		return;
	}

	start = atoi(Cmd_Argv(2));

	if(start < 0){  // catch negative offset
		Com_Printf("Illegal configstring offset from %s\n", Sv_ClientAdrToString(sv_client));
		Sv_KickClient(sv_client, NULL);
		return;
	}

	// write a packet full of data
	while(sv_client->netchan.message.cursize < MAX_MSGLEN / 2
			&& start < MAX_CONFIGSTRINGS){
		if(sv.configstrings[start][0]){
			Msg_WriteByte(&sv_client->netchan.message, svc_configstring);
			Msg_WriteShort(&sv_client->netchan.message, start);
			Msg_WriteString(&sv_client->netchan.message, sv.configstrings[start]);
		}
		start++;
	}

	// send next command
	if(start == MAX_CONFIGSTRINGS){
		Msg_WriteByte(&sv_client->netchan.message, svc_stufftext);
		Msg_WriteString(&sv_client->netchan.message, va("baselines %i 0\n", svs.spawncount));
	} else {
		Msg_WriteByte(&sv_client->netchan.message, svc_stufftext);
		Msg_WriteString(&sv_client->netchan.message, va("configstrings %i %i\n", svs.spawncount, start));
	}
}

/*
 * Sv_Baselines_f
 */
static void Sv_Baselines_f(void){
	int start;
	entity_state_t nullstate;
	entity_state_t *base;

	Com_Dprintf("Baselines() from %s\n", sv_client->name);

	if(sv_client->state != cs_connected){
		Com_Printf("baselines not valid -- already spawned\n");
		return;
	}

	// handle the case of a level changing while a client was connecting
	if(atoi(Cmd_Argv(1)) != svs.spawncount){
		Com_Printf("Sv_Baselines_f from different level\n");
		Sv_New_f();
		return;
	}

	start = atoi(Cmd_Argv(2));

	if(start < 0){  // catch negative offset
		Com_Printf("Illegal baseline offset from %s\n", Sv_ClientAdrToString(sv_client));
		Sv_KickClient(sv_client, NULL);
		return;
	}

	memset(&nullstate, 0, sizeof(nullstate));

	// write a packet full of data
	while(sv_client->netchan.message.cursize < MAX_MSGLEN / 2
			&& start < MAX_EDICTS){
		base = &sv.baselines[start];
		if(base->modelindex || base->sound || base->effects){
			Msg_WriteByte(&sv_client->netchan.message, svc_spawnbaseline);
			Msg_WriteDeltaEntity(&nullstate, base, &sv_client->netchan.message, true, true);
		}
		start++;
	}

	// send next command
	if(start == MAX_EDICTS){
		Msg_WriteByte(&sv_client->netchan.message, svc_stufftext);
		Msg_WriteString(&sv_client->netchan.message, va("precache %i\n", svs.spawncount));
	} else {
		Msg_WriteByte(&sv_client->netchan.message, svc_stufftext);
		Msg_WriteString(&sv_client->netchan.message, va("baselines %i %i\n", svs.spawncount, start));
	}
}


/*
 * Sv_Begin_f
 */
static void Sv_Begin_f(void){
	Com_Dprintf("Begin() from %s\n", sv_client->name);

	if(sv_client->state != cs_connected){  // catch duplicate spawns
		Com_Printf("Illegal begin from %s\n", Sv_ClientAdrToString(sv_client));
		Sv_KickClient(sv_client, NULL);
		return;
	}

	if(sv.state == ss_demo)
		return;

	// handle the case of a level changing while a client was connecting
	if(atoi(Cmd_Argv(1)) != svs.spawncount){
		Com_Printf("Sv_Begin_f from different level\n");
		Sv_New_f();
		return;
	}

	sv_client->state = cs_spawned;

	// call the game begin function
	ge->ClientBegin(sv_player);

	Cbuf_InsertFromDefer();
}

/*
 * Sv_NextDownload_f
 */
static void Sv_NextDownload_f(void){
	int r, size, percent;
	byte buf[MAX_MSGLEN];
	sizebuf_t msg;

	if(!sv_client->download)
		return;

	r = sv_client->downloadsize - sv_client->downloadcount;
	if(r > 1280)  // stock quake2 sends 1024 byte chunks
		r = 1280;  // but i see no harm in sending another 256 bytes

	Sb_Init(&msg, buf, MAX_MSGLEN);

	Msg_WriteByte(&msg, svc_download);
	Msg_WriteShort(&msg, r);

	sv_client->downloadcount += r;
	size = sv_client->downloadsize;
	if(!size)
		size = 1;

	percent = sv_client->downloadcount * 100 / size;
	Msg_WriteByte(&msg, percent);

	Sb_Write(&msg, sv_client->download + sv_client->downloadcount - r, r);
	Sb_Write(&sv_client->netchan.message, msg.data, msg.cursize);

	if(sv_client->downloadcount != sv_client->downloadsize)
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
	extern cvar_t *sv_udpdownload;
	const char *name;
	void *buf;
	int i = 0, offset = 0;

	name = Cmd_Argv(1);

	if(Cmd_Argc() > 2)
		offset = atoi(Cmd_Argv(2)); // downloaded offset

	// catch illegal offset or filenames
	if(offset < 0 || *name == '.' || *name == '/' || *name == '\\' || strstr(name, "..")){
		Com_Printf("Malicious download from %s\n", Sv_ClientAdrToString(sv_client));
		Sv_KickClient(sv_client, NULL);
		return;
	}

	while(downloadable[i]){  // ensure download name is allowed
		if(Com_GlobMatch(downloadable[i], name))
			break;
		i++;
	}

	if(!downloadable[i]){  // it wasnt
		Com_Printf("Illegal download (%s) from %s\n", name, Sv_ClientAdrToString(sv_client));
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

	sv_client->downloadsize = Fs_LoadFile(name, &buf);
	sv_client->download = (byte *)buf;
	sv_client->downloadcount = offset;

	if(offset > sv_client->downloadsize)
		sv_client->downloadcount = sv_client->downloadsize;

	if(!sv_client->download){  // legal filename, but missing file
		Com_Dprintf("Couldn't download %s to %s\n", name, sv_client->name);
		Msg_WriteByte(&sv_client->netchan.message, svc_download);
		Msg_WriteShort(&sv_client->netchan.message, -1);
		Msg_WriteByte(&sv_client->netchan.message, 0);
		return;
	}

	Sv_NextDownload_f();
	Com_Dprintf("Downloading %s to %s\n", name, sv_client->name);
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
		Info_Print(Cvar_Serverinfo());
		return;
	}

	for(cvar = cvar_vars; cvar; cvar = cvar->next){

		if(!(cvar->flags & CVAR_SERVERINFO))
			continue;  //only print serverinfo cvars

		snprintf(line, sizeof(line), "%s %s\n", cvar->name, cvar->string);
		Sv_ClientPrintf(sv_client, PRINT_MEDIUM, "%s", line);
	}
}


typedef struct {
	char *name;
	void (*func)(void);
} ucmd_t;

ucmd_t ucmds[] = {
	{"new", Sv_New_f},
	{"configstrings", Sv_Configstrings_f},
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
		Com_Printf("Illegal command contained xFF from %s\n", Sv_ClientAdrToString(sv_client));
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
		ge->ClientCommand(sv_player);
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
static void Sv_ClientThink(client_t *cl, usercmd_t *cmd){
	cl->cmd_msec -= cmd->msec;
	ge->ClientThink(cl->edict, cmd);
}


#define MAX_STRINGCMDS 8
/*
 * Sv_ExecuteClientMessage
 *
 * The current net_message is parsed for the given client
 */
void Sv_ExecuteClientMessage(client_t *cl){
	usercmd_t nullcmd, oldest, oldcmd, newcmd;
	int net_drop;
	int stringCmdCount;
	qboolean move_issued;
	int lastframe;
	int c;
	char *s;

	sv_client = cl;
	sv_player = sv_client->edict;

	// only allow one move command
	move_issued = false;
	stringCmdCount = 0;

	while(true){

		if(net_message.readcount > net_message.cursize){
			Com_Printf("Sv_ReadClientMessage: badread\n");
			Sv_DropClient(cl);
			return;
		}

		c = Msg_ReadByte(&net_message);
		if(c == -1)
			break;

		switch(c){
			case clc_nop:
				break;

			case clc_userinfo:
				strncpy(cl->userinfo, Msg_ReadString(&net_message), sizeof(cl->userinfo) - 1);
				Sv_UserinfoChanged(cl);
				break;

			case clc_move:
				if(move_issued)
					return;  // someone is trying to cheat

				move_issued = true;

				lastframe = Msg_ReadLong(&net_message);
				if(lastframe != cl->lastframe){
					cl->lastframe = lastframe;
					if(cl->lastframe > -1){
						cl->frame_latency[cl->lastframe & (LATENCY_COUNTS - 1)] =
							svs.realtime - cl->frames[cl->lastframe & UPDATE_MASK].senttime;
					}
				}

				memset(&nullcmd, 0, sizeof(nullcmd));
				Msg_ReadDeltaUsercmd(&net_message, &nullcmd, &oldest);
				Msg_ReadDeltaUsercmd(&net_message, &oldest, &oldcmd);
				Msg_ReadDeltaUsercmd(&net_message, &oldcmd, &newcmd);

				// don't start delta compression until the client is spawned
				if(cl->state != cs_spawned){
					cl->lastframe = -1;
					break;
				}

				if(nullcmd.msec > 250 || oldest.msec > 250 ||  // catch illegal msec
						oldcmd.msec > 250 || newcmd.msec > 250){
					Com_Printf("Illegal msec in usercmd from %s\n", Sv_ClientAdrToString(cl));
					Sv_KickClient(cl, NULL);
					return;
				}

				net_drop = cl->netchan.dropped;
				if(net_drop < 20){
					while(net_drop > 2){
						Sv_ClientThink(cl, &cl->lastcmd);
						net_drop--;
					}
					if(net_drop > 1)
						Sv_ClientThink(cl, &oldest);
					if(net_drop > 0)
						Sv_ClientThink(cl, &oldcmd);
				}
				Sv_ClientThink(cl, &newcmd);
				cl->lastcmd = newcmd;
				break;

			case clc_stringcmd:
				s = Msg_ReadString(&net_message);

				// malicious users may try using too many string commands
				if(++stringCmdCount < MAX_STRINGCMDS)
					Sv_ExecuteUserCommand(s);

				if(cl->state == cs_free)
					return;  // disconnect command
				break;

			default:
				Com_Printf("Sv_ReadClientMessage: unknown command %d\n", c);
				Sv_DropClient(cl);
				return;
		}
	}
}
