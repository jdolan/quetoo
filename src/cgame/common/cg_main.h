/*
 * Copyright(c) 1997-2001 id Software, Inc.
 * Copyright(c) 2002 The Quakeforge Project.
 * Copyright(c) 2006 Quetoo.
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

#pragma once

#include "cg_types.h"

#if defined(__CG_LOCAL_H__)
#if defined(G_HOOK)
float Cg_GetHookPullSpeed(void);
#endif

extern Cvar *cg_addAtmospheric;
extern Cvar *cg_addDecals;
extern Cvar *cg_addEntities;
extern Cvar *cg_addFlares;
extern Cvar *cg_addLights;
extern Cvar *cg_addSprites;
extern Cvar *cg_addWeather;
extern Cvar *cg_bob;
extern Cvar *cg_drawBlend;
extern Cvar *cg_drawBlendDamage;
extern Cvar *cg_drawBlendLiquid;
extern Cvar *cg_drawBlendPickup;
extern Cvar *cg_drawBlendPowerup;
extern Cvar *cg_drawCrosshair;
extern Cvar *cg_drawCrosshairAlpha;
extern Cvar *cg_drawCrosshairColor;
extern Cvar *cg_drawCrosshairHealth;
extern Cvar *cg_drawCrosshairPulse;
extern Cvar *cg_drawCrosshairScale;
extern Cvar *cg_drawDiagnostics;
extern Cvar *cg_drawFps;
extern Cvar *cg_drawHud;
extern Cvar *cg_drawPing;
extern Cvar *cg_drawPingWarn;
extern Cvar *cg_hud;
extern Cvar *cg_drawTargetName;
extern Cvar *cg_drawWeapon;
extern Cvar *cg_drawWeaponAlpha;
extern Cvar *cg_drawWeaponBob;
extern Cvar *cg_drawWeaponX;
extern Cvar *cg_drawWeaponY;
extern Cvar *cg_drawWeaponZ;
extern Cvar *cg_drawVitalsPulse;
extern Cvar *cg_entityBob;
extern Cvar *cg_entityRotate;
extern Cvar *cg_forceSkin;
extern Cvar *cg_fov;
extern Cvar *cg_fovZoom;
extern Cvar *cg_fovInterpolate;
extern Cvar *cg_hitSound;
extern Cvar *cg_predict;
extern Cvar *cg_quickJoinMaxPing;
extern Cvar *cg_quickJoinMinClients;
extern Cvar *cg_spritePhysics;
extern Cvar *cg_cameraMode;
extern Cvar *cg_thirdPerson;
extern Cvar *cg_thirdPersonX;
extern Cvar *cg_thirdPersonY;
extern Cvar *cg_thirdPersonZ;
extern Cvar *cg_thirdPersonPitch;
extern Cvar *cg_thirdPersonYaw;

extern Cvar *cg_autoSwitch;
extern Cvar *cg_color;
extern Cvar *cg_hand;
extern Cvar *cg_helmet;
#if defined(G_HOOK)
extern Cvar *cg_hookStyle;
#endif
extern Cvar *cg_pants;
extern Cvar *cg_shirt;
extern Cvar *cg_skin;

extern ClientGameImport cgi;

CGAME_EXPORT ClientGameExport *Cg_LoadCgame(ClientGameImport *import);

#endif
