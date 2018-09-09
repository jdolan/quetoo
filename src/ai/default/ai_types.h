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
 * @brief Flags that bots use for its timing
 */
typedef enum {
	AI_ITEM_AMMO			= (1 << 0),
	AI_ITEM_ARMOR			= (1 << 1),
	AI_ITEM_FLAG			= (1 << 2),
	AI_ITEM_HEALTH			= (1 << 3),
	AI_ITEM_POWERUP			= (1 << 4),
	AI_ITEM_WEAPON			= (1 << 5),
	AI_ITEM_TECH			= (1 << 6),

	AI_WEAPON_PROJECTILE	= (1 << 16), // fires a projectile with "speed" speed
	AI_WEAPON_HITSCAN		= (1 << 17), // fires hitscan shot(s)
	AI_WEAPON_TIMED			= (1 << 18), // a holdable that must be thrown within "time" milliseconds
	AI_WEAPON_EXPLOSIVE		= (1 << 19), // fires explosive shots (might hurt self),
	AI_WEAPON_SHORT_RANGE	= (1 << 20), // weapon works at close range
	AI_WEAPON_MED_RANGE		= (1 << 21), // weapon works at medium range
	AI_WEAPON_LONG_RANGE	= (1 << 22) // weapon works at long range
} ai_item_flags_t;

/**
 * @brief Forward declaration of item registration struct.
 */
typedef struct ai_item_s {
	const char *class_name;
	const char *name;
	ai_item_flags_t flags;
	uint16_t ammo; // index to item
	g_weapon_tag_t tag;
	vec_t priority;
	uint16_t quantity;
	uint16_t max;

	int32_t speed; // used for projectile weapons
	uint32_t time; // used for timing items (handgrenades)
} ai_item_t;

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
	const vec_t *velocity;

	/**
	 * @brief Offset to health
	 */
	const int16_t *health;

	/**
	 * @brief Offset to max health
	 */
	const int16_t *max_health;

	/**
	 * @brief Offset to max armor
	 */
	const int16_t *max_armor;

	/**
	 * @brief Offset to water level
	 */
	const pm_water_level_t *water_level;
} ai_entity_data_t;

/**
 * @brief Struct of parameters from g_client_t that the bot
 * will make use of. These will be offsets, not actual pointers.
 */
typedef struct ai_client_data_s {
	/**
	 * @brief Offset to view angles
	 */
	const vec_t *angles;

	/**
	 * @brief Offset to inventory
	 */
	const int16_t *inventory;

	/**
	 * @brief Offset to current weapon
	 */
	const g_item_t *const *weapon;
} ai_client_data_t;

#ifdef __AI_LOCAL_H__

typedef struct {
	uint32_t frame_num;
	uint32_t time;
	g_gameplay_t gameplay;
	_Bool teams;
	_Bool ctf;
	_Bool match;
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
	vec_t priority;
	uint32_t time; // time this goal was set
	const g_entity_t *ent; // for AI_GOAL_ITEM/ENEMY/TEAMMATE
	uint16_t ent_id; // FOR AI_GOAL_ITEM/ENEMY/TEAMMATE
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

	vec_t wander_angle;
	vec3_t ghost_position;

	uint32_t weapon_check_time;

	uint32_t no_movement_frames;
} ai_locals_t;
#endif /* __AI_LOCAL_H__ */
