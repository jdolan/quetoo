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

#ifndef __CLIENT_H__
#define __CLIENT_H__

#include "cgame/cgame.h"
#include "cmodel.h"
#include "console.h"
#include "filesystem.h"
#include "net.h"
#include "renderer/renderer.h"
#include "sound/sound.h"
#include "ui/ui.h"

#include "cl_cgame.h"
#include "cl_client.h"
#include "cl_cmd.h"
#include "cl_console.h"
#include "cl_demo.h"
#include "cl_effect.h"
#include "cl_emit.h"
#include "cl_entity.h"
#include "cl_forward.h"
#include "cl_http.h"
#include "cl_input.h"
#include "cl_keys.h"
#include "cl_loc.h"
#include "cl_main.h"
#include "cl_parse.h"
#include "cl_pred.h"
#include "cl_screen.h"
#include "cl_server.h"
#include "cl_tentity.h"
#include "cl_types.h"
#include "cl_view.h"



// cl_client.c
void Cl_LoadClientInfo(cl_client_info_t *ci, const char *s);
void Cl_AnimateClientEntity(cl_entity_t *e, r_entity_t *upper, r_entity_t *lower);

// cl_cmd.c
void Cl_UpdateCmd(void);
void Cl_SendCmd(void);

// cl_console.c
extern console_t cl_con;

void Cl_InitConsole(void);
void Cl_DrawConsole(void);
void Cl_DrawNotify(void);
void Cl_UpdateNotify(int last_line);
void Cl_ClearNotify(void);
void Cl_ToggleConsole_f(void);

// cl_cgame.c
void Cl_InitCgame(void);
void Cl_ShutdownCgame(void);

// cl_demo.c
void Cl_WriteDemoMessage(void);
void Cl_Record_f(void);
void Cl_Stop_f(void);
void Cl_FastForward_f(void);
void Cl_SlowMotion_f(void);

// cl_effect.c
void Cl_BubbleTrail(const vec3_t start, const vec3_t end, float density);
void Cl_EntityEvent(entity_state_t *ent);
void Cl_TracerEffect(const vec3_t start, const vec3_t end);
void Cl_BulletEffect(const vec3_t org, const vec3_t dir);
void Cl_BurnEffect(const vec3_t org, const vec3_t dir, int scale);
void Cl_BloodEffect(const vec3_t org, const vec3_t dir, int count);
void Cl_GibEffect(const vec3_t org, int count);
void Cl_SmokeFlash(entity_state_t *ent);
void Cl_SmokeTrail(const vec3_t start, const vec3_t end, cl_entity_t *ent);
void Cl_FlameTrail(const vec3_t start, const vec3_t end, cl_entity_t *ent);
void Cl_SteamTrail(const vec3_t org, const vec3_t vel, cl_entity_t *ent);
void Cl_ExplosionEffect(const vec3_t org);
void Cl_ItemRespawnEffect(const vec3_t org);
void Cl_ItemPickupEffect(const vec3_t org);
void Cl_TeleporterTrail(const vec3_t org, cl_entity_t *ent);
void Cl_LogoutEffect(const vec3_t org);
void Cl_SparksEffect(const vec3_t org, const vec3_t dir, int count);
void Cl_EnergyTrail(cl_entity_t *ent, float radius, int color);
void Cl_EnergyFlash(entity_state_t *ent, int color, int count);
void Cl_RocketTrail(const vec3_t start, const vec3_t end, cl_entity_t *ent);
void Cl_GrenadeTrail(const vec3_t start, const vec3_t end, cl_entity_t *ent);
void Cl_HyperblasterTrail(cl_entity_t *ent);
void Cl_HyperblasterEffect(const vec3_t org);
void Cl_LightningEffect(const vec3_t org);
void Cl_LightningTrail(const vec3_t start, const vec3_t end);
void Cl_RailEffect(const vec3_t start, const vec3_t end, int flags, int color);
void Cl_BfgTrail(cl_entity_t *ent);
void Cl_BfgEffect(const vec3_t org);
void Cl_LoadEffectSamples(void);
void Cl_AddParticles(void);
r_particle_t *Cl_AllocParticle(void);
void Cl_ClearEffects(void);

// cl_emit.c
void Cl_LoadEmits(void);
void Cl_AddEmits(void);

// cl_entity.c
void Cl_ParseFrame(void);
void Cl_AddEntities(cl_frame_t *frame);

// cl_forward.c
void Cl_ForwardCmdToServer(void);

// cl_http.c
void Cl_InitHttpDownload(void);
void Cl_HttpDownloadCleanup(void);
boolean_t Cl_HttpDownload(void);
void Cl_HttpDownloadThink(void);
void Cl_ShutdownHttpDownload(void);

// cl_input.c
void Cl_InitInput(void);
void Cl_HandleEvents(void);
void Cl_Move(user_cmd_t *cmd);

// cl_keys.c
void Cl_KeyEvent(unsigned int ascii, unsigned short unicode, boolean_t down, unsigned time);
char *Cl_EditLine(void);
const char *Cl_KeyNumToString(int key_num);
void Cl_WriteBindings(FILE *f);
void Cl_InitKeys(void);
void Cl_ShutdownKeys(void);
void Cl_ClearTyping(void);

// cl_loc.c
void Cl_InitLocations(void);
void Cl_ShutdownLocations(void);
void Cl_LoadLocations(void);
const char *Cl_LocationHere(void);
const char *Cl_LocationThere(void);

// cl_parse.c
extern char *svc_strings[256];

boolean_t Cl_CheckOrDownloadFile(const char *file_name);
void Cl_ParseConfigString(void);
void Cl_ParseClientInfo(int player);
void Cl_ParseMuzzleFlash(void);
void Cl_ParseServerMessage(void);
void Cl_Download_f(void);

// cl_pred.c
extern int cl_gravity;
void Cl_PredictMovement(void);
void Cl_CheckPredictionError(void);

// cl_screen.c
void Cl_CenterPrint(char *s);
void Cl_AddNetgraph(void);
void Cl_UpdateScreen(void);

// cl_server.c
void Cl_Ping_f(void);
void Cl_Servers_f(void);
void Cl_ParseStatusMessage(void);
void Cl_ParseServersList(void);
void Cl_FreeServers(void);

// cl_tentity.c
void Cl_ParseTempEntity(void);

// cl_view.c
void Cl_InitView(void);
void Cl_ClearState(void);
void Cl_AddEntity(r_entity_t *ent);
void Cl_AddParticle(r_particle_t *p);
void Cl_UpdateView(void);

#endif /* __CLIENT_H__ */
