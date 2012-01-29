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

#include "sv_local.h"
#include "pmove.h"

/*
 * Sv_Print
 */
static void Sv_Print(const char *fmt, ...) __attribute__((format(printf, 1, 2)));
static void Sv_Print(const char *fmt, ...) {
	char msg[MAX_STRING_CHARS];
	va_list args;

	va_start(args, fmt);
	vsprintf(msg, fmt, args);
	va_end(args);

	Com_Print("%s", msg);
}

/*
 * Sv_Debug
 */
static void Sv_Debug(const char *fmt, ...) __attribute__((format(printf, 1, 2)));
static void Sv_Debug(const char *fmt, ...) {
	char msg[MAX_STRING_CHARS];
	va_list args;

	va_start(args, fmt);
	vsprintf(msg, fmt, args);
	va_end(args);

	Com_Debug("%s", msg);
}

/*
 * Sv_Error
 *
 * Abort the server with a game error
 */
static void Sv_Error(const char *fmt, ...) __attribute__((noreturn, format(printf, 1, 2)));
static void Sv_Error(const char *fmt, ...) {
	char msg[MAX_STRING_CHARS];
	va_list args;

	va_start(args, fmt);
	vsprintf(msg, fmt, args);
	va_end(args);

	Com_Error(ERR_DROP, "Game error: %s.\n", msg);
}

/*
 * Sv_SetModel
 *
 * Also sets mins and maxs for inline bsp models.
 */
static void Sv_SetModel(g_edict_t *ent, const char *name) {
	c_model_t *mod;

	if (!name) {
		Com_Warn("Sv_SetModel %d: NULL.\n", (int) NUM_FOR_EDICT(ent));
		return;
	}

	ent->s.model1 = Sv_ModelIndex(name);

	// if it is an inline model, get the size information for it
	if (name[0] == '*') {
		mod = Cm_Model(name);
		VectorCopy(mod->mins, ent->mins);
		VectorCopy(mod->maxs, ent->maxs);
		Sv_LinkEdict(ent);
	}
}

/*
 * Sv_ConfigString
 */
static void Sv_ConfigString(int index, const char *val) {

	if (index < 0 || index >= MAX_CONFIG_STRINGS) {
		Com_Warn("Sv_ConfigString: bad index %i.\n", index);
		return;
	}

	if (!val)
		val = "";

	// change the string in sv.config_strings
	strncpy(sv.config_strings[index], val, sizeof(sv.config_strings[0]));

	if (sv.state != SV_LOADING) { // send the update to everyone
		Sb_Clear(&sv.multicast);
		Msg_WriteChar(&sv.multicast, SV_CMD_CONFIG_STRING);
		Msg_WriteShort(&sv.multicast, index);
		Msg_WriteString(&sv.multicast, val);

		Sv_Multicast(vec3_origin, MULTICAST_ALL_R);
	}
}

/*
 * Message wrappers which target the multicast buffer.
 */

static void Sv_WriteData(void *data, size_t len) {
	Msg_WriteData(&sv.multicast, data, len);
}

static void Sv_WriteChar(int c) {
	Msg_WriteChar(&sv.multicast, c);
}

static void Sv_WriteByte(int c) {
	Msg_WriteByte(&sv.multicast, c);
}

static void Sv_WriteShort(int c) {
	Msg_WriteShort(&sv.multicast, c);
}

static void Sv_WriteLong(int c) {
	Msg_WriteLong(&sv.multicast, c);
}

static void Sv_WriteString(const char *s) {
	Msg_WriteString(&sv.multicast, s);
}

static void Sv_WritePos(vec3_t pos) {
	Msg_WritePos(&sv.multicast, pos);
}

static void Sv_WriteDir(vec3_t dir) {
	Msg_WriteDir(&sv.multicast, dir);
}

static void Sv_WriteAngle(float f) {
	Msg_WriteAngle(&sv.multicast, f);
}

/*
 * Sv_inPVS
 *
 * Also checks portal_areas so that doors block sight
 */
static boolean_t Sv_inPVS(const vec3_t p1, const vec3_t p2) {
	int leaf_num;
	int cluster;
	int area1, area2;
	byte *mask;

	leaf_num = Cm_PointLeafnum(p1);
	cluster = Cm_LeafCluster(leaf_num);
	area1 = Cm_LeafArea(leaf_num);
	mask = Cm_ClusterPVS(cluster);

	leaf_num = Cm_PointLeafnum(p2);
	cluster = Cm_LeafCluster(leaf_num);
	area2 = Cm_LeafArea(leaf_num);

	if (mask && (!(mask[cluster >> 3] & (1 << (cluster & 7)))))
		return false;

	if (!Cm_AreasConnected(area1, area2))
		return false; // a door blocks sight

	return true;
}

/*
 * Sv_inPHS
 *
 * Also checks portal_areas so that doors block sound
 */
static boolean_t Sv_inPHS(const vec3_t p1, const vec3_t p2) {
	int leaf_num;
	int cluster;
	int area1, area2;
	byte *mask;

	leaf_num = Cm_PointLeafnum(p1);
	cluster = Cm_LeafCluster(leaf_num);
	area1 = Cm_LeafArea(leaf_num);
	mask = Cm_ClusterPHS(cluster);

	leaf_num = Cm_PointLeafnum(p2);
	cluster = Cm_LeafCluster(leaf_num);
	area2 = Cm_LeafArea(leaf_num);
	if (mask && (!(mask[cluster >> 3] & (1 << (cluster & 7)))))
		return false; // more than one bounce away
	if (!Cm_AreasConnected(area1, area2))
		return false; // a door blocks hearing

	return true;
}

/*
 * Sv_Sound
 */
static void Sv_Sound(g_edict_t *ent, unsigned short index, unsigned short atten) {

	if (!ent)
		return;

	Sv_PositionedSound(NULL, ent, index, atten);
}

static void *game_handle;

/*
 * Sv_ShutdownGame
 *
 * Called when either the entire server is being killed, or it is changing to a
 * different game directory.
 */
void Sv_ShutdownGame(void) {

	if (!svs.game)
		return;

	Com_Print("Game shutdown...\n");

	svs.game->Shutdown();
	svs.game = NULL;

	Com_Print("Game down.\n");
	Com_QuitSubsystem(Q2W_GAME);

	Sys_CloseLibrary(&game_handle);
}

/*
 * Sv_InitGame
 *
 * Initializes the game module by exposing a subset of server functionality
 * through function pointers.  In return, the game module allocates memory for
 * entities and returns a few pointers of its own.
 *
 * Note that the terminology here is worded from the game module's perspective;
 * that is, "import" is what we give to the game, and "export" is what the game
 * returns to us.  This distinction seems a bit backwards, but it was likely
 * deemed less confusing to "mod" authors back in the day.
 */
void Sv_InitGame(void) {
	g_import_t import;

	if (svs.game) {
		Sv_ShutdownGame();
	}

	Com_Print("Game initialization...\n");

	memset(&import, 0, sizeof(import));

	import.frame_rate = svs.frame_rate;
	import.server_frame = 1.0 / svs.frame_rate;

	import.Print = Sv_Print;
	import.Debug = Sv_Debug;
	import.BroadcastPrint = Sv_BroadcastPrint;
	import.ClientPrint = Sv_ClientPrint;
	import.ClientCenterPrint = Sv_ClientCenterPrint;

	import.Error = Sv_Error;

	import.ConfigString = Sv_ConfigString;

	import.ModelIndex = Sv_ModelIndex;
	import.SoundIndex = Sv_SoundIndex;
	import.ImageIndex = Sv_ImageIndex;

	import.SetModel = Sv_SetModel;
	import.Sound = Sv_Sound;
	import.PositionedSound = Sv_PositionedSound;

	import.Trace = Sv_Trace;
	import.PointContents = Sv_PointContents;
	import.inPVS = Sv_inPVS;
	import.inPHS = Sv_inPHS;
	import.SetAreaPortalState = Cm_SetAreaPortalState;
	import.AreasConnected = Cm_AreasConnected;
	import.Pmove = Pmove;

	import.LinkEntity = Sv_LinkEdict;
	import.UnlinkEntity = Sv_UnlinkEdict;
	import.BoxEdicts = Sv_AreaEdicts;

	import.Multicast = Sv_Multicast;
	import.Unicast = Sv_Unicast;
	import.WriteData = Sv_WriteData;
	import.WriteChar = Sv_WriteChar;
	import.WriteByte = Sv_WriteByte;
	import.WriteShort = Sv_WriteShort;
	import.WriteLong = Sv_WriteLong;
	import.WriteString = Sv_WriteString;
	import.WritePosition = Sv_WritePos;
	import.WriteDir = Sv_WriteDir;
	import.WriteAngle = Sv_WriteAngle;

	import.TagMalloc = Z_TagMalloc;
	import.TagFree = Z_Free;
	import.FreeTags = Z_FreeTags;

	import.Gamedir = Fs_Gamedir;
	import.OpenFile = Fs_OpenFile;
	import.CloseFile = Fs_CloseFile;
	import.LoadFile = Fs_LoadFile;

	import.Cvar = Cvar_Get;

	import.Argc = Cmd_Argc;
	import.Argv = Cmd_Argv;
	import.Args = Cmd_Args;

	import.AddCommandString = Cbuf_AddText;

	svs.game = (g_export_t *) Sys_LoadLibrary("game", &game_handle,
			"G_LoadGame", &import);

	if (!svs.game) {
		Com_Error(ERR_DROP, "Sv_InitGame: Failed to load game module.\n");
	}

	if (svs.game->api_version != GAME_API_VERSION) {
		Com_Error(ERR_DROP, "Sv_InitGame: Game is version %i, not %i.\n",
				svs.game->api_version, GAME_API_VERSION);
	}

	svs.game->Init();

	Com_Print("Game initialized, starting...\n");
	Com_InitSubsystem(Q2W_GAME);
}
