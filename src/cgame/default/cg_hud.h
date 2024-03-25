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

typedef enum {
	CROSSHAIR_HEALTH_NONE,
	CROSSHAIR_HEALTH_RED_WHITE,
	CROSSHAIR_HEALTH_RED_WHITE_GREEN,
	CROSSHAIR_HEALTH_RED_YELLOW_WHITE,
	CROSSHAIR_HEALTH_RED_YELLOW_WHITE_GREEN,
	CROSSHAIR_HEALTH_WHITE_GREEN
} cg_crosshair_health_t;

#define CROSSHAIR_HEALTH_FIRST CROSSHAIR_HEALTH_NONE
#define CROSSHAIR_HEALTH_LAST CROSSHAIR_HEALTH_WHITE_BLUE

#define CROSSHAIR_SCALE 0.125f
#define CROSSHAIR_PULSE_ALPHA 0.5f

#ifdef __CG_LOCAL_H__
void Cg_ParseCenterPrint(void);
void Cg_ParseWeaponInfo(const char *s);
bool Cg_AttemptSelectWeapon(const player_state_t *ps);
void Cg_DrawHud(const player_state_t *ps);
void Cg_ClearHud(void);
void Cg_LoadHudMedia(void);
void Cg_InitHud(void);
#endif /* __CG_LOCAL_H__ */
