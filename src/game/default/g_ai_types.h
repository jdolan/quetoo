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
 * @brief Sentinel value indicating an invalid or unset navigation node ID.
 */
#define AI_NODE_INVALID ((ai_node_id_t)-1)

#if defined(__GAME_LOCAL_H__)

/**
 * @brief The default user info string (name and skin).
 */
#define DEFAULT_BOT_INFO "\\name\\newbiebot\\skin\\qforcer/default"

/**
 * @brief The type of goal we're after. This controls which variant in ai_goal_t
 * we can access.
 */
typedef enum {
  AI_GOAL_NONE,     ///< No goal; treated as valid but lowest priority.
  AI_GOAL_POSITION, ///< Positional goal; only a world-space destination is known.
  AI_GOAL_ENTITY,   ///< Entity goal; tied to the lifetime of a specific entity.
  AI_GOAL_PATH      ///< Path goal; stores a sequence of connected nodes to a destination.
} ai_goal_type_t;

/**
 * @brief Bot combat styles.
 */
typedef enum {
  AI_COMBAT_NONE,   ///< No active combat style.
  AI_COMBAT_CLOSE,  ///< Rush the enemy at close range.
  AI_COMBAT_FLANK,  ///< Circle-strafe the enemy.
  AI_COMBAT_WANDER, ///< Wander rather than engage directly.
  AI_COMBAT_TOTAL   ///< Total number of combat styles.
} ai_combat_type_t;

/**
 * @brief Bot trick jump timing states.
 */
typedef enum {
  TRICK_JUMP_NONE,    ///< No trick jump in progress.
  TRICK_JUMP_START,   ///< Trick jump initiated; waiting for ground contact.
  TRICK_JUMP_WAITING, ///< Waiting for the correct moment to jump.
  TRICK_JUMP_TURNING  ///< Rotating to align for the trick jump.
} ai_trick_jump_t;

/**
 * @brief The variant structure of a goal.
 */
typedef struct {
  ai_goal_type_t type;     ///< Type of this goal; controls which union variant is active.
  float priority;          ///< Priority used to replace this goal with a more important one.
  uint32_t time;           ///< Level time when this goal was set.
  float distress;          ///< Accumulated distress; when threshold is reached the goal is abandoned.
  float last_distress;     ///< Previous distress value, used for debug logging.
  float last_distance;     ///< Last recorded distance to the goal destination.
  bool distress_extension; ///< When true, extends the distress timeout from 1s to 15s.
  
  union {
    struct {
      float angle; ///< Wander direction angle in degrees.
    } wander;

    struct {
      vec3_t pos; ///< Target world-space position.
    } position;

    struct {
      const g_entity_t *ent;        ///< Target entity.
      uint8_t spawn_id;             ///< Spawn ID at goal-set time; used to detect entity reuse.

      // specific to combat goal

      ai_combat_type_t combat_type; ///< Active combat style against this entity.
      uint32_t lock_on_time;        ///< Level time when the bot first locked on to this enemy.
      float flank_angle;            ///< Current flank offset angle for circle-strafing.
    } entity;

    struct {
      GArray *path;                             ///< Array of ai_node_id_t forming the route.
      guint path_index;                         ///< Index of the current node being navigated toward.
      vec3_t path_position, next_path_position; ///< World positions of current and next node.
      ai_trick_jump_t trick_jump;               ///< Current trick jump state for this path segment.
      vec3_t trick_position;                    ///< World position used as the trick jump target.
      const g_entity_t *path_target;            ///< Optional entity the path is leading to.
      uint32_t path_target_spawn_id;            ///< Spawn ID of path_target at goal-set time.
    } path;
  };
} ai_goal_t;

/**
 * @brief A functional AI goal. It returns the amount of time to wait
 * until the goal should be run again.
 */
typedef uint32_t (*G_Ai_GoalFunc)(g_client_t *cl, pm_cmd_t *cmd);

/**
 * @brief Functional AI goal slot IDs, one per periodic decision function.
 */
typedef enum {
  AI_FUNC_GOAL_LONGRANGE,  ///< Long-range target acquisition (sniper, railgun).
  AI_FUNC_GOAL_HUNT,       ///< Active pursuit of the current combat target.
  AI_FUNC_GOAL_WEAPONRY,   ///< Weapon selection and management.
  AI_FUNC_GOAL_ACROBATICS, ///< Jump, dodge, and trick movement.
  AI_FUNC_GOAL_FINDITEMS,  ///< Item pickup pathfinding.
  AI_FUNC_GOAL_TURN,       ///< Turning and aiming toward the current goal.
  AI_FUNC_GOAL_MOVE,       ///< Movement execution toward the current goal.
  AI_FUNC_GOAL_TOTAL       ///< Total number of functional goal slots.
} ai_func_goal_t;

/**
 * @brief Static bot definition from the roster.
 */
typedef struct {
  const char *name; ///< Bot display name.
  const char *skin; ///< Player model/skin path.
  float skill;      ///< 0.0 (easy) to 1.0 (hard): aim, turn speed, reaction time.
  float aggression; ///< 0.0 (cautious) to 1.0 (reckless): retreat/chase/combat style.
  float awareness;  ///< 0.0 (oblivious) to 1.0 (perceptive): item range, weapon choice.
} g_ai_roster_t;

/**
 * @brief Per-bot runtime personality, initialized from the roster entry on spawn.
 */
typedef struct {
  float skill;      ///< Aim accuracy and reaction speed (0.0–1.0).
  float aggression; ///< Tendency to chase vs. retreat (0.0–1.0).
  float awareness;  ///< Item-detection range and weapon-selection quality (0.0–1.0).
  float aim_phase;  ///< Per-bot phase offset for sinusoidal aim wobble.
} ai_personality_t;

/**
 * @brief AI-specific per-client state.
 */
typedef struct ai_s {
  const g_ai_roster_t *roster;                        ///< Pointer to this bot's static roster definition.
  ai_personality_t personality;                       ///< Runtime personality derived from the roster.

  uint32_t func_goal_next_thinks[AI_FUNC_GOAL_TOTAL]; ///< Next think times indexed by ai_func_goal_t.

  ai_goal_t move_target;                              ///< Current movement/navigation goal.
  ai_goal_t backup_move_target;                       ///< Saved movement goal, restored after a detour.
  ai_goal_t combat_target;                            ///< Current combat/enemy goal.

  uint32_t weapon_check_time;                         ///< Next level time to re-evaluate weapon selection.
  uint32_t reacquire_time;                            ///< Level time before the bot attempts to reacquire a goal.
  uint32_t distress_jump_offset;                      ///< Random per-bot offset applied to distress jump timing.
} ai_t;

#endif /* __GAME_LOCAL_H__ */
