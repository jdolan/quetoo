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

#include "game/game.h"
#include "game/default/g_types.h"

/**
 * @brief Resolve typed data from a structure using offsets.
 */
#define MEMBER_DATA(from, member) \
	((typeof(member)) ((byte *) (from) + ((ptrdiff_t) member)))

/**
 * @brief Typed offsets into g_item_t, populated by the game module.
 */
typedef struct ai_item_data_s {

	/**
	 * @brief The item class name.
	 */
	const char *const *class_name;

	/**
	 * @brief The item index.
	 */
	uint16_t *index;

	/**
	 * @brief The item type.
	 */
	g_item_type_t *type;

	/**
	 * @brief The weapon tag, if any.
	 */
	g_weapon_tag_t *tag;

	/**
	 * @brief The weapon flags, if any.
	 */
	g_weapon_flags_t *flags;

	/**
	 * @brief The pickup name.
	 */
	const char *const *name;

	/**
	 * @brief The ammo item, if any.
	 */
	const char *const *ammo;

	/**
	 * @brief The quantity, provided or used, depending on the item type.
	 */
	uint16_t *quantity;

	/**
	 * @brief The maximum quantity the player can carry.
	 */
	uint16_t *max;

	/**
	 * @brief The priority, for bots to weigh.
	 */
	float *priority;

} ai_item_data_t;

#define ITEM_DATA(item, m) \
	(*MEMBER_DATA(item, ai_item_data.m))

/**
 * @brief Struct of parameters from g_entity_t that the bot
 * will make use of. These will be offsets, not actual pointers.
 */
typedef struct ai_entity_data_s {

	/**
	 * @brief Offset to ground entity
	 */
	g_entity_t *const *ground_entity;

	/**
	 * @brief Offset to item stored in this entity
	 */
	const g_item_t *const *item;

	/**
	 * @brief Offset to velocity
	 */
	const vec3_t *velocity;

	/**
	 * @brief Offset to health
	 */
	const int16_t *health;

	/**
	 * @brief Offset to max health
	 */
	const int16_t *max_health;

	/**
	 * @brief Offset to water level
	 */
	const pm_water_level_t *water_level;
} ai_entity_data_t;

/**
 * @brief Resolve the entity data ptr for the specified member
 */
#define ENTITY_DATA(ent, m) \
	(*MEMBER_DATA(&ent->locals, ai_entity_data.m))

/**
 * @brief Struct of parameters from g_client_t that the bot
 * will make use of. These will be offsets, not actual pointers.
 */
typedef struct ai_client_data_s {
	/**
	 * @brief Offset to view angles
	 */
	const vec3_t *angles;

	/**
	 * @brief Offset to inventory
	 */
	const int16_t *inventory;

	/**
	 * @brief Offset to max armor
	 */
	const int16_t *max_armor;

	/**
	 * @brief Offset to current weapon
	 */
	const g_item_t *const *weapon;

	/**
	 * @brief Offset to team id
	 */
	const g_team_id_t *team;
	
	/**
	 * @brief Offset to grenade hold time
	 */
	const uint32_t *grenade_hold_time;

} ai_client_data_t;

/**
 * @brief Resolve the client data ptr for the specified member
 */
#define CLIENT_DATA(client, m) \
	(*MEMBER_DATA(&client->locals, ai_client_data.m))

#ifdef __AI_LOCAL_H__

typedef struct {
	uint32_t frame_num;
	uint32_t time;
	g_gameplay_t gameplay;
	_Bool teams;
	_Bool ctf;
	_Bool match;
	_Bool load_finished;
	char mapname[MAX_QPATH];
} ai_level_t;

/**
 * @brief The default user info string (name and skin).
 */
#define DEFAULT_BOT_INFO "\\name\\newbiebot\\skin\\qforcer/default"

typedef enum {
	AI_GOAL_NONE,
	AI_GOAL_NAV,
	AI_GOAL_GHOST,
	AI_GOAL_ITEM,
	AI_GOAL_ENEMY,
	AI_GOAL_TEAMMATE
} ai_goal_type_t;

typedef struct {
	ai_goal_type_t type;
	float priority;
	uint32_t time; // time this goal was set
	
	// for AI_GOAL_ITEM/ENEMY/TEAMMATE

	const g_entity_t *ent;
	uint16_t ent_id;

	// for AI_GOAL_NAV

	GArray *path;
	guint path_index;
	vec3_t path_position;
} ai_goal_t;

/**
 * @brief A G_AIGoalFunc can return this if the goal is finished.
 */
#define AI_GOAL_COMPLETE	0

/**
 * @brief A functional AI goal. It returns the amount of time to wait
 * until the goal should be run again.
 */
typedef uint32_t (*Ai_GoalFunc)(g_entity_t *ent, pm_cmd_t *cmd);

/**
 * @brief A functional AI goal.
 */
typedef struct {
	Ai_GoalFunc think;
	uint32_t nextthink;
	uint32_t time; // time this funcgoal was added
} ai_funcgoal_t;

/**
 * @brief The total number of functional goals the AI may have at once.
 */
#define MAX_AI_FUNCGOALS	12

/**
 * @brief Bot combat styles
 */
typedef enum {
	AI_COMBAT_CLOSE,
	AI_COMBAT_FLANK,
	AI_COMBAT_WANDER,

	AI_COMBAT_TOTAL
} ai_combat_type_t;

/**
 * @brief AI-specific locals
 */
typedef struct ai_locals_s {
	ai_funcgoal_t funcgoals[MAX_AI_FUNCGOALS];

	vec3_t last_origin;
	vec3_t aim_forward; // calculated at start of thinking
	vec3_t eye_origin; //  ^^^

	// the AI can have two distinct targets: one it's aiming at,
	// and one it's moving towards. These aren't pointers because
	// the priority of an item/enemy might be different depending on
	// the state of the bot.
	ai_goal_t aim_target;
	ai_goal_t move_target;
	ai_combat_type_t combat_type;
	uint32_t lock_on_time;

	float wander_angle;
	vec3_t ghost_position;

	uint32_t weapon_check_time;

	uint32_t no_movement_frames;
	uint32_t reacquire_time;

	uint32_t goal_distress;
	float goal_distance;
} ai_locals_t;
#endif /* __AI_LOCAL_H__ */
