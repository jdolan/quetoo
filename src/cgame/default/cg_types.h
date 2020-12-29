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

#include "cgame/cgame.h"
#include "game/default/g_types.h"

#ifdef __CG_LOCAL_H__

#define WEATHER_NONE        0x0
#define WEATHER_RAIN        0x1
#define WEATHER_SNOW        0x2

/**
 * @brief Client game state.
 */
typedef struct {
	/**
	 * @brief Grapple hook speed, for client side prediction.
	 */
	float hook_pull_speed;

	/**
	 * @brief The current weather bitmask.
	 */
	int32_t weather;
} cg_state_t;

extern cg_state_t cg_state;

/**
 * @brief Stores info related to teams on the server.
 */
typedef struct {
	char team_name[MAX_USER_INFO_KEY];
	float hue;
	color_t color;
} cg_team_info_t;

extern cg_team_info_t cg_team_info[MAX_TEAMS];

#endif /* __CG_LOCAL_H__ */
