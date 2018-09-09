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

#define AI_API_VERSION 5

/**
 * @brief Forward declaration of entity type, since
 * game.h requires ai.h
 */
typedef struct g_entity_s g_entity_t;
typedef struct g_import_s g_import_t;
typedef struct g_export_s g_export_t;

/**
 * @brief Items are opaque pointers to AI.
 */
typedef struct g_item_s g_item_t;

typedef _Bool (*EntityFilterFunc)(const g_entity_t *ent);

/**
 * @brief Functions and the like that the AI system imports from the game.
 */
typedef struct {
	g_import_t *gi;
	g_export_t *ge;
	
	/**
	 * @brief Query if two entities are on the same team
	 */
	_Bool (*OnSameTeam)(const g_entity_t *self, const g_entity_t *other);

	/**
	 * @brief TODO FIXME - TEMPORARY API SCRATCH SPACE.
	 */
	uint16_t (*ItemIndex)(const g_item_t *item);
	_Bool (*CanPickupItem)(const g_entity_t *self, const g_entity_t *other);
} ai_import_t;

/**
 * @brief Forward declaration of AI structs.
 */
typedef struct ai_item_s ai_item_t;
typedef struct ai_entity_data_s ai_entity_data_t;
typedef struct ai_client_data_s ai_client_data_t;

/**
 * @brief Functions and the like that the AI system exports to the game.
 */
typedef struct {
	int32_t	api_version;

	/**
	 * @brief Called when the game has restarted
	 */
	void (*GameRestarted)(void);

	/**
	 * @brief Initialize AI facilities. This is called by the server, do not call it.
	 */
	void (*Init)(void);

	/**
	 * @brief Shutdown AI facilities. This is called by the server, do not call it.
	 */
	void (*Shutdown)(void);

	/**
	 * @brief Called once by game to setup its data pointers
	 */
	void (*SetDataPointers)(ai_entity_data_t *entity, ai_client_data_t *client);

	/**
	 * @brief Run an AI frame.
	 */
	void (*Frame)(void);

	/**
	 * @brief Generate the user info for the specified bot.
	 */
	void (*GetUserInfo)(const g_entity_t *self, char *userinfo);

	/**
	 * @brief Called to setup an AI's initial state.
	 */
	void (*Begin)(g_entity_t *self);

	/**
	 * @brief Called when an AI spawns.
	 */
	void (*Spawn)(g_entity_t *self);

	/**
	 * @brief Called to think for an AI. Returns the movement command for the bot.
	 */
	void (*Think)(g_entity_t *self, pm_cmd_t *cmd);

	/**
	 * @brief Called when a human client has a command that the AI can learn from.
	 */
	void (*Learn)(const g_entity_t *self, const pm_cmd_t *cmd);

	/**
	 * @brief Register an item on the AI system
	 */
	void (*RegisterItem)(const uint16_t index, const ai_item_t *item);
} ai_export_t;
