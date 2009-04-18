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

#include "client.h"

#include <zlib.h>
static z_stream z;
static byte zbuf[MAX_MSGLEN];

extern cvar_t *cl_chatsound;
extern cvar_t *cl_teamchatsound;
extern cvar_t *cl_ignore;

char *svc_strings[256] = {
	"svc_bad",
	"svc_nop",

	"svc_muzzleflash",
	"svc_temp_entity",
	"svc_layout",

	"svc_disconnect",
	"svc_reconnect",
	"svc_sound",
	"svc_print",
	"svc_stufftext",
	"svc_serverdata",
	"svc_configstring",
	"svc_spawnbaseline",
	"svc_centerprint",
	"svc_download",
	"svc_frame",
	"svc_zlib"
};


/*
 * Cl_CheckOrDownloadFile
 *
 * Returns true if the file exists, otherwise it attempts
 * to start a download from the server.
 */
qboolean Cl_CheckOrDownloadFile(const char *filename){
	FILE *fp;
	char name[MAX_OSPATH];
	char cmd[MAX_STRING_CHARS];

	if(cls.state == ca_disconnected){
		Com_Printf("Not connected.\n");
		return true;
	}

	if(filename[0] == '/'){
		Com_Warn("Refusing to download a path starting with /.\n");
		return true;
	}
	if(strstr(filename, "..")){
		Com_Warn("Refusing to download a path with .. .\n");
		return true;
	}
	if(strchr(filename, ' ')){
		Com_Warn("Refusing to download a path with whitespace.\n");
		return true;
	}

	Com_Dprintf("Checking for %s\n", filename);

	if(Fs_LoadFile(filename, NULL) != -1){  // it exists, no need to download
		return true;
	}

	Com_Dprintf("Attempting to download %s\n", filename);

	strcpy(cls.download.name, filename);

	// udp downloads to a temp name, and only renames when done
	Com_StripExtension(cls.download.name, cls.download.tempname);
	strcat(cls.download.tempname, ".tmp");

	// attempt an http download if available
	if(cls.downloadurl[0] && Cl_HttpDownload())
		return false;

	// check to see if we already have a tmp for this file, if so, try to resume
	// open the file if not opened yet
	snprintf(name, sizeof(name), "%s/%s", Fs_Gamedir(), cls.download.tempname);

	fp = fopen(name, "r+b");
	if(fp){ // temp fie exists, resume download
		int len;
		fseek(fp, 0, SEEK_END);
		len = ftell(fp);

		cls.download.file = fp;

		// give the server the offset to start the download
		Com_Dprintf("Resuming %s..\n", cls.download.name);

		snprintf(cmd, sizeof(cmd), "download %s %i", cls.download.name, len);
		Msg_WriteByte(&cls.netchan.message, clc_stringcmd);
		Msg_WriteString(&cls.netchan.message, cmd);
	} else {
		// or start if from the beginning
		Com_Dprintf("Downloading %s..\n", cls.download.name);

		snprintf(cmd, sizeof(cmd), "download %s", cls.download.name);
		Msg_WriteByte(&cls.netchan.message, clc_stringcmd);
		Msg_WriteString(&cls.netchan.message, cmd);
	}

	return false;
}


/*
 * Cl_Download_f
 *
 * Manually request a download from the server.
 */
void Cl_Download_f(void){

	if(Cmd_Argc() != 2){
		Com_Printf("Usage: %s <filename>\n", Cmd_Argv(0));
		return;
	}

	Cl_CheckOrDownloadFile(Cmd_Argv(1));
}


/*
 * Cl_ParseDownload
 *
 * A download message has been received from the server
 */
static void Cl_ParseDownload(void){
	int size, percent;
	char name[MAX_OSPATH];

	// read the data
	size = Msg_ReadShort(&net_message);
	percent = Msg_ReadByte(&net_message);
	if(size < 0){
		Com_Dprintf("Server does not have this file.\n");
		if(cls.download.file){
			// if here, we tried to resume a file but the server said no
			Fs_CloseFile(cls.download.file);
			cls.download.file = NULL;
		}
		Cl_RequestNextDownload();
		return;
	}

	// open the file if not opened yet
	if(!cls.download.file){
		snprintf(name, sizeof(name), "%s/%s", Fs_Gamedir(), cls.download.tempname);

		Fs_CreatePath(name);

		if(!(cls.download.file = fopen(name, "wb"))){
			net_message.readcount += size;
			Com_Warn("Failed to open %s.\n", name);
			Cl_RequestNextDownload();
			return;
		}
	}

	Fs_Write(net_message.data + net_message.readcount, 1, size, cls.download.file);

	net_message.readcount += size;

	if(percent != 100){
		Msg_WriteByte(&cls.netchan.message, clc_stringcmd);
		Sb_Print(&cls.netchan.message, "nextdl");
	} else {
		char oldn[MAX_OSPATH];
		char newn[MAX_OSPATH];

		Fs_CloseFile(cls.download.file);

		// rename the temp file to it's final name
		snprintf(oldn, sizeof(oldn), "%s/%s", Fs_Gamedir(), cls.download.tempname);
		snprintf(newn, sizeof(newn), "%s/%s", Fs_Gamedir(), cls.download.name);

		if(rename(oldn, newn))
			Com_Warn("Failed to rename %s to %s.\n", oldn, newn);

		cls.download.file = NULL;

		if(strstr(newn, ".pak"))  // append paks to searchpaths
			Fs_AddPakfile(newn);

		// get another file if needed
		Cl_RequestNextDownload();
	}
}


/*
 *
 *   SERVER CONNECTING MESSAGES
 *
 */

/*
 * Cl_ParseServerData
 */
static qboolean Cl_ParseServerData(void){
	extern cvar_t *fs_gamedirvar;
	char *str;
	int i;

	// wipe the client_state_t struct
	Cl_ClearState();
	cls.state = ca_connected;

	// parse protocol version number
	i = Msg_ReadLong(&net_message);

	// ensure protocol matches
	if(i != PROTOCOL){
		Com_Error(ERR_DROP, "Cl_ParseServerData: Server is using unknown protocol %d.", i);
	}

	// retrieve spawn count and packet rate
	cl.servercount = Msg_ReadLong(&net_message);
	cl.serverrate = Msg_ReadLong(&net_message);

	// determine if we're viewing a demo
	cl.demoserver = Msg_ReadByte(&net_message);

	// game directory
	str = Msg_ReadString(&net_message);
	strncpy(cl.gamedir, str, sizeof(cl.gamedir) - 1);

	// set gamedir
	if((*str && (!*fs_gamedirvar->string || strcmp(fs_gamedirvar->string, str))) ||
			(!*str && (*fs_gamedirvar->string))){

		if(strcmp(fs_gamedirvar->string, str)){
			if(cl.demoserver){
				Cvar_ForceSet("game", str);
				Fs_SetGamedir(str);
			}
			else Cvar_Set("game", str);
		}
	}

	// parse player entity number
	cl.playernum = Msg_ReadShort(&net_message);

	// get the full level name
	str = Msg_ReadString(&net_message);
	Com_Printf("\n"); Com_Printf("%c%s\n", 2, str);

	// need to prep view at next oportunity
	r_view.ready = false;
	return true;
}

/*
 * Cl_ParseBaseline
 */
static void Cl_ParseBaseline(void){
	entity_state_t *es;
	unsigned int bits;
	int newnum;
	entity_state_t nullstate;

	memset(&nullstate, 0, sizeof(nullstate));

	newnum = Cl_ParseEntityBits(&bits);
	es = &cl_entities[newnum].baseline;
	Cl_ParseDelta(&nullstate, es, newnum, bits);
}


/*
 * Cl_LoadClientinfo
 *
 */
void Cl_LoadClientinfo(clientinfo_t *ci, const char *s){
	int i;
	const char *t;
	char *u, *v;
	char model_name[MAX_QPATH];
	char skin_name[MAX_QPATH];
	char model_filename[MAX_QPATH];
	char skin_filename[MAX_QPATH];
	char weapon_filename[MAX_QPATH];

	// copy the entire string
	strncpy(ci->cinfo, s, sizeof(ci->cinfo));
	ci->cinfo[sizeof(ci->cinfo) - 1] = 0;

	i = 0;
	t = s;
	while(*t){  // check for non-printable chars
		if(*t <= 32){
			i = -1;
			break;
		}
		t++;
	}

	if(!strlen(ci->cinfo) || i == -1)  // use default
		strcpy(ci->cinfo, "newbie\\ichabod/ichabod");

	// isolate the player's name
	strncpy(ci->name, s, sizeof(ci->name));
	ci->name[sizeof(ci->name) - 1] = 0;

	v = strchr(ci->name, '\\');
	u = strchr(ci->name, '/');

	if(v && u && (v < u)){  // valid
		*v = *u = 0;
		strcpy(model_name, v + 1);
		strcpy(skin_name, u + 1);
	}
	else {  // invalid
		strcpy(ci->name, "newbie");
		strcpy(model_name, "ichabod");
		strcpy(skin_name, "ichabod");
	}

	// load the model
	snprintf(model_filename, sizeof(model_filename), "players/%s/tris.md2", model_name);
	ci->model = R_LoadModel(model_filename);
	if(!ci->model){
		strcpy(model_name, "ichabod");
		strcpy(skin_name, "ichabod");
		snprintf(model_filename, sizeof(model_filename), "players/ichabod/tris.md2");
		ci->model = R_LoadModel(model_filename);
	}

	// and the skin
	snprintf(skin_filename, sizeof(skin_filename), "players/%s/%s.pcx", model_name, skin_name);
	ci->skin = R_LoadImage(skin_filename, it_skin);

	// if we don't have it, use the first one we do have for the model
	if(ci->skin == r_notexture){
		snprintf(skin_filename, sizeof(skin_filename), "players/%s/?[!_]*.pcx", model_name);
		ci->skin = R_LoadImage(Fs_FindFirst(skin_filename, false), it_skin);
	}

	// weapon models
	for(i = 0; i < num_cl_weaponmodels; i++){
		snprintf(weapon_filename, sizeof(weapon_filename), "players/%s/%s", model_name, cl_weaponmodels[i]);
		ci->weaponmodel[i] = R_LoadModel(weapon_filename);
		if(!ci->weaponmodel[i])
			break;
	}

	// must have loaded all components to be valid
	if(!ci->model || ci->skin == r_notexture || i < num_cl_weaponmodels){
		strcpy(model_name, ci->name);  // borrow this to store name
		memcpy(ci, &cl.baseclientinfo, sizeof(*ci));
		strcpy(ci->name, model_name);
	}
}


/*
 * Cl_ParseClientinfo
 *
 * Load the model and skin for a client
 */
void Cl_ParseClientinfo(int player){
	const char *s;
	clientinfo_t *ci;

	s = cl.configstrings[player + CS_PLAYERSKINS];

	ci = &cl.clientinfo[player];

	Cl_LoadClientinfo(ci, s);
}


/*
 * Cl_ParseGravity
 */
static void Cl_ParseGravity(const char *gravity){
	int g;

	if((g = atoi(gravity)) > -1)
		cl_gravity = g;
}


/*
 * Cl_ParseConfigstring
 */
void Cl_ParseConfigstring(void){
	int i;
	char *s;
	char olds[MAX_QPATH];

	i = Msg_ReadShort(&net_message);
	if(i < 0 || i >= MAX_CONFIGSTRINGS){
		Com_Error(ERR_DROP, "Cl_ParseConfigstring: Invalid index %i.", i);
	}
	s = Msg_ReadString(&net_message);

	strncpy(olds, cl.configstrings[i], sizeof(olds));
	olds[sizeof(olds) - 1] = 0;

	strcpy(cl.configstrings[i], s);

	if(i == CS_GRAVITY)
		Cl_ParseGravity(cl.configstrings[i]);
	else if(i >= CS_MODELS && i < CS_MODELS + MAX_MODELS){
		if(r_view.ready){
			cl.model_draw[i - CS_MODELS] = R_LoadModel(cl.configstrings[i]);
			if(cl.configstrings[i][0] == '*')
				cl.model_clip[i - CS_MODELS] = Cm_InlineModel(cl.configstrings[i]);
			else
				cl.model_clip[i - CS_MODELS] = NULL;
		}
	} else if(i >= CS_SOUNDS && i < CS_SOUNDS + MAX_SOUNDS){
		if(r_view.ready)
			cl.sound_precache[i - CS_SOUNDS] = S_LoadSample(cl.configstrings[i]);
	} else if(i >= CS_IMAGES && i < CS_IMAGES + MAX_IMAGES){
		if(r_view.ready)
			cl.image_precache[i - CS_IMAGES] = R_LoadPic(cl.configstrings[i]);
	} else if(i >= CS_PLAYERSKINS && i < CS_PLAYERSKINS + MAX_CLIENTS){
		if(r_view.ready && strcmp(olds, s))
			Cl_ParseClientinfo(i - CS_PLAYERSKINS);
	}
}


/*
 * Cl_ParseSound
 */
static void Cl_ParseSound(void){
	vec3_t origin;
	float *org;
	int soundindex;
	int entnum;
	int atten;
	int flags;

	flags = Msg_ReadByte(&net_message);

	soundindex = Msg_ReadByte(&net_message);

	if(flags & S_ATTEN)
		atten = Msg_ReadByte(&net_message);
	else
		atten = DEFAULT_SOUND_ATTENUATION;

	if(flags & S_ENTNUM){  // entity relative
		entnum = Msg_ReadShort(&net_message);

		if(entnum > MAX_EDICTS)
			Com_Error(ERR_DROP, "Cl_ParseSound: entnum = %d.", entnum);
	} else {
		entnum = -1;
	}

	if(flags & S_ORIGIN){  // positioned in space
		Msg_ReadPos(&net_message, origin);

		org = origin;
	} else  // use entnum
		org = NULL;

	if(!cl.sound_precache[soundindex])
		return;

	S_PlaySample(org, entnum, cl.sound_precache[soundindex], atten);
}


/*
 * Cl_IgnoreChatMessage
 */
static qboolean Cl_IgnoreChatMessage(const char *msg){

	const char *s = strtok(cl_ignore->string, " ");

	if(!strlen(cl_ignore->string))
		return false;  // nothing currently filtered

	while(s){
		if(strstr(msg, s))
			return true;
		s = strtok(NULL, " ");
	}
	return false;  // msg is okay
}


/*
 * Cl_ShowNet
 */
static void Cl_ShowNet(const char *s){
	if(cl_shownet->value >= 2)
		Com_Printf("%3zd: %s\n", net_message.readcount - 1, s);
}


/*
 * Cl_ZlibServerMessage
 *
 * Called for svc_zlib, this function inflates the remainder of the
 * current net_message and adjusts its length accordingly.
 */
static void Cl_ZlibServerMessage(void){
	int len;

	memset(zbuf, 0, MAX_MSGLEN);

	z.zalloc = Z_NULL;
	z.zfree = Z_NULL;
	z.opaque = Z_NULL;

	inflateInit2(&z, -15);

	z.avail_in = net_message.cursize - net_message.readcount;
	z.next_in = net_message.data + net_message.readcount;

	z.avail_out = MAX_MSGLEN;
	z.next_out = zbuf;

	inflate(&z, Z_NO_FLUSH);
	len = MAX_MSGLEN - z.avail_out;

	inflateEnd(&z);

	net_message.readcount--;  // overwrite the zlib command

	// clear remainder of message, replace with deflated
	// content, and update message length accordingly
	memset(net_message.data + net_message.readcount, 0,
			net_message.cursize - net_message.readcount);
	memcpy(net_message.data + net_message.readcount, zbuf, len);
	net_message.cursize = net_message.readcount + len;
}


/*
 * Cl_ParseServerMessage
 */
void Cl_ParseServerMessage(void){
	extern int bytes_this_second;
	int cmd, oldcmd;
	char *s;
	int i;

	if(cl_shownet->value == 1)
		Com_Printf(Q2W_SIZE_T" ", net_message.cursize);
	else if(cl_shownet->value >= 2)
		Com_Printf("------------------\n");

	bytes_this_second += net_message.cursize;
	cmd = 0;

	// parse the message
	while(true){
		if(net_message.readcount > net_message.cursize){
			Com_Error(ERR_DROP, "Cl_ParseServerMessage: Bad server message.");
		}

		oldcmd = cmd;
		cmd = Msg_ReadByte(&net_message);

		if(cmd == -1){
			Cl_ShowNet("END OF MESSAGE");
			break;
		}

		if(cl_shownet->value >= 2 && svc_strings[cmd])
			Cl_ShowNet(svc_strings[cmd]);

		switch(cmd){
			case svc_nop:
				break;

			case svc_disconnect:
				Com_Error(ERR_NONE, "Server disconnected.");
				break;

			case svc_reconnect:
				Com_Printf("Server disconnected, reconnecting..\n");
				// stop download
				if(cls.download.file){
					if(cls.download.http)  // clean up http downloads
						Cl_HttpDownloadCleanup();
					else  // or just stop legacy ones
						Fs_CloseFile(cls.download.file);
					cls.download.file = NULL;
				}
				cls.state = ca_connecting;
				cls.connect_time = -99999;  // fire immediately
				break;

			case svc_print:
				i = Msg_ReadByte(&net_message);
				s = Msg_ReadString(&net_message);
				if(i == PRINT_CHAT){
					if(Cl_IgnoreChatMessage(s))  // filter /ignore'd chatters
						break;
					if(*cl_chatsound->string)  // trigger chat sound
						S_StartLocalSample(cl_chatsound->string);
				} else if(i == PRINT_TEAMCHAT){
					if(Cl_IgnoreChatMessage(s))  // filter /ignore'd chatters
						break;
					if(*cl_teamchatsound->string)  // trigger chat sound
						S_StartLocalSample(cl_teamchatsound->string);
				}
				Com_Printf("%s", s);
				break;

			case svc_centerprint:
				Cl_CenterPrint(Msg_ReadString(&net_message));
				break;

			case svc_stufftext:
				s = Msg_ReadString(&net_message);
				Cbuf_AddText(s);
				break;

			case svc_serverdata:
				Cbuf_Execute();  // make sure any stuffed commands are done
				if(!Cl_ParseServerData())
					return;
				break;

			case svc_configstring:
				Cl_ParseConfigstring();
				break;

			case svc_sound:
				Cl_ParseSound();
				break;

			case svc_spawnbaseline:
				Cl_ParseBaseline();
				break;

			case svc_temp_entity:
				Cl_ParseTempEntity();
				break;

			case svc_muzzleflash:
				Cl_ParseMuzzleFlash();
				break;

			case svc_download:
				Cl_ParseDownload();
				break;

			case svc_frame:
				Cl_ParseFrame();
				break;

			case svc_zlib:
				Cl_ZlibServerMessage();
				break;

			case svc_layout:
				s = Msg_ReadString(&net_message);
				strncpy(cl.layout, s, sizeof(cl.layout) - 1);
				break;

			default:
				Com_Printf("Cl_ParseServerMessage: Illegible server message\n"
						"  %d: last command was %s\n", cmd, svc_strings[oldcmd]);
				break;
		}
	}

	Cl_AddNetgraph();

	Cl_WriteDemoMessage();
}
