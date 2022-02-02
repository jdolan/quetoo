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

/**
 * @brief The client game reprensetation of teams.
 */
typedef struct {
	/**
	 * @brief Team ID.
	 */
	g_team_id_t id;

	/**
	 * @brief Team name.
	 */
	char name[MAX_USER_INFO_KEY];

	/**
	 * @brief Shirt color.
	 */
	color_t color;

	/**
	 * @brief Effects color.
	 */
	float hue;
} cg_team_info_t;

/**
 * @brief The client game representation of clients (players).
 */
typedef struct {
	/**
	 * @brief The client info string, e.g. "newbie\qforcer/default."
	 */
	char info[MAX_STRING_CHARS];

	/**
	 * @brief The player name, e.g. "newbie."
	 */
	char name[MAX_USER_INFO_VALUE];

	/**
	 * @brief The model name, e.g. "qforcer."
	 */
	char model[MAX_USER_INFO_VALUE];

	/**
	 * @brief The skin name, e.g. "default."
	 */
	char skin[MAX_USER_INFO_VALUE];

	/**
	 * @brief Shirt, pants and helmet color.
	 */
	color_t shirt, pants, helmet;

	/**
	 * @brief Effects color.
	 */
	float hue;

	/**
	 * @brief The head model and materials.
	 */
	r_model_t *head;
	r_material_t *head_skins[MAX_ENTITY_SKINS];

	/**
	 * @brief The torso model and materials.
	 */
	r_model_t *torso;
	r_material_t *torso_skins[MAX_ENTITY_SKINS];

	/**
	 * @brief The legs model and materials.
	 */
	r_model_t *legs;
	r_material_t *legs_skins[MAX_ENTITY_SKINS];

	/**
	 * @brief The skin icon for the scoreboard.
	 */
	r_image_t *icon;

	/**
	 * @brief The team identifier.
	 */
	cg_team_info_t *team;
} cg_client_info_t;

#define WEATHER_NONE        0x0
#define WEATHER_RAIN        0x1
#define WEATHER_SNOW        0x2

/**
 * @brief Client game state. Most of this is parsed from ConfigStrings when they change.
 */
typedef struct {
	/**
	 * @brief The clients (players).
	 */
	cg_client_info_t clients[MAX_CLIENTS];

	/**
	 * @brief The teams.
	 */
	cg_team_info_t teams[MAX_TEAMS];

	/**
	 * @brief The gameplay mode.
	 */
	g_gameplay_t gameplay;

	/**
	 * @brief Non-zero if CTF is enabled.
	 */
	int32_t ctf;

	/**
	 * @brief Non-zero if teams play is enabled.
	 */
	int32_t num_teams;

	/**
	 * @brief Grapple hook speed, for client side prediction.
	 */
	float hook_pull_speed;

	/**
	 * @brief The current weather bitmask.
	 */
	int32_t weather;
	
	/**
	 * @brief The current max_clients value of the server.
	 */
	int32_t max_clients;
	
	/**
	 * @brief The current number of clients connected to the server.
	 */
	int32_t num_clients;

	/**
	 * @brief Whether match mode is enabled
	 */
	int32_t match;

	/**
	 * @brief The current round #
	 */
	int32_t round;

	/**
	 * @brief The current number of rounds
	 */
	int32_t num_rounds;
} cg_state_t;

extern cg_state_t cg_state;



#endif /* __CG_LOCAL_H__ */
