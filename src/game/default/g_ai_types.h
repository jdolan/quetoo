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
#include "g_types.h"

/**
 * @brief
 */
#define AI_NODE_INVALID	((ai_node_id_t)-1)

#ifdef __GAME_LOCAL_H__

typedef struct {
	_Bool load_finished;
} ai_level_t;

/**
 * @brief The default user info string (name and skin).
 */
#define DEFAULT_BOT_INFO "\\name\\newbiebot\\skin\\qforcer/default"

/**
 * @brief The type of goal we're after. This controls which variant in ai_goal_t
 * we can access.
 */
typedef enum {
	/**
	 * @brief This goal is empty. This is still technically a valid goal type, just
	 * very low priority generally.
	 */
	AI_GOAL_NONE,

	/**
	 * @brief This goal is a positional goal. It only has a known position
	 * in the world.
	 */
	AI_GOAL_POSITION,

	/**
	 * @brief This goal is an entity goal. It is attached to an entity's existence.
	 */
	AI_GOAL_ENTITY,
	
	/**
	 * @brief This goal is a path goal. It stores a whole path of connected nodes
	 * to reach a destination.
	 */
	AI_GOAL_PATH
} ai_goal_type_t;

/**
 * @brief Bot combat styles.
 */
typedef enum {
	AI_COMBAT_NONE,

	AI_COMBAT_CLOSE,
	AI_COMBAT_FLANK,
	AI_COMBAT_WANDER,

	AI_COMBAT_TOTAL
} ai_combat_type_t;

/**
 * @brief Bot trick jump timing.
 */
typedef enum {
	TRICK_JUMP_NONE,
	TRICK_JUMP_START,
	TRICK_JUMP_WAITING,
	TRICK_JUMP_TURNING
} ai_trick_jump_t;

/**
 * @brief The variant structure of a goal.
 */
typedef struct {
	/**
	 * @brief The type of goal we hold.
	 */
	ai_goal_type_t type;

	/**
	 * @brief The priority of this goal. This can be used, for instance, to find a more suitable
	 * target while we're actively heading for a particular goal, and replacing it.
	 */
	float priority;

	/**
	 * @brief The time this goal was set at.
	 */
	uint32_t time;

	/**
	 * @brief Distress counter; if we reach a threshold, this goal will be abandoned.
	 */
	float distress;

	/**
	 * @brief Just used for debugging.
	 */
	float last_distress;

	/**
	 * @brief Last distance we recorded to our goal.
	 */
	float last_distance;

	/**
	 * @brief extends the time from 1s to 10s+ FIXME: change to a fixed # that can be added to distress timeout
	 */
	_Bool distress_extension;
	
	union {
		struct {
			float angle;
		} wander;

		struct {
			vec3_t pos;
		} position;

		struct {
			const g_entity_t *ent;
			uint8_t spawn_id;

			// specific to combat goal

			ai_combat_type_t combat_type;
			uint32_t lock_on_time;
			float flank_angle;
		} entity;

		struct {
			GArray *path;
			guint path_index;
			vec3_t path_position, next_path_position;
			ai_trick_jump_t trick_jump;
			vec3_t trick_position;
			const g_entity_t *path_target;
			uint32_t path_target_spawn_id;
		} path;
	};
} ai_goal_t;

/**
 * @brief A functional AI goal. It returns the amount of time to wait
 * until the goal should be run again.
 */
typedef uint32_t (*Ai_GoalFunc)(g_entity_t *ent, pm_cmd_t *cmd);

/**
 * @brief Functional AI goal IDs.
 */
typedef enum {
	AI_FUNCGOAL_LONGRANGE,
	AI_FUNCGOAL_HUNT,
	AI_FUNCGOAL_WEAPONRY,
	AI_FUNCGOAL_ACROBATICS,
	AI_FUNCGOAL_FINDITEMS,
	AI_FUNCGOAL_TURN,
	AI_FUNCGOAL_MOVE,

	AI_FUNCGOAL_TOTAL
} ai_funcgoal_t;

/**
 * @brief AI-specific locals
 */
typedef struct ai_locals_s {
	uint32_t funcgoal_nextthinks[AI_FUNCGOAL_TOTAL];

	vec3_t last_origin;
	vec3_t aim_forward; // calculated at start of thinking
	vec3_t eye_origin; //  ^^^

	ai_goal_t move_target, backup_move_target;
	ai_goal_t combat_target;

	uint32_t weapon_check_time;
	uint32_t reacquire_time;
	uint32_t distress_jump_offset;
	vec3_t ideal_angles;
} ai_locals_t;
#endif /* __GAME_LOCAL_H__ */
