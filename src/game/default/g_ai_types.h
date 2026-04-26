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
  AI_GOAL_NONE,
  AI_GOAL_POSITION,
  AI_GOAL_ENTITY,
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
 * @brief Bot trick jump timing states.
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
   * @brief Type of this goal; controls which union variant is active.
   */
  ai_goal_type_t type;

  /**
   * @brief Priority used to replace this goal with a more important one.
   */
  float priority;

  /**
   * @brief Level time when this goal was set.
   */
  uint32_t time;

  /**
   * @brief Accumulated distress; when threshold is reached the goal is abandoned.
   */
  float distress;

  /**
   * @brief Previous distress value, used for debug logging.
   */
  float last_distress;

  /**
   * @brief Last recorded distance to the goal destination.
   */
  float last_distance;

  /**
   * @brief When true, extends the distress timeout from 1s to 15s.
   */
  bool distress_extension;
  
  union {
    struct {

      /**
       * @brief Wander direction angle in degrees.
       */
      float angle;
    } wander;

    struct {

      /**
       * @brief Target world-space position.
       */
      vec3_t pos;
    } position;

    struct {

      /**
       * @brief Target entity.
       */
      const g_entity_t *ent;

      /**
       * @brief Spawn ID at goal-set time; used to detect entity reuse.
       */
      uint8_t spawn_id;

      // specific to combat goal

      /**
       * @brief Active combat style against this entity.
       */
      ai_combat_type_t combat_type;

      /**
       * @brief Level time when the bot first locked on to this enemy.
       */
      uint32_t lock_on_time;

      /**
       * @brief Current flank offset angle for circle-strafing.
       */
      float flank_angle;
    } entity;

    struct {

      /**
       * @brief Array of ai_node_id_t forming the route.
       */
      GArray *path;

      /**
       * @brief Index of the current node being navigated toward.
       */
      guint path_index;

      /**
       * @brief World positions of current and next node.
       */
      vec3_t path_position, next_path_position;

      /**
       * @brief Current trick jump state for this path segment.
       */
      ai_trick_jump_t trick_jump;

      /**
       * @brief World position used as the trick jump target.
       */
      vec3_t trick_position;

      /**
       * @brief Optional entity the path is leading to.
       */
      const g_entity_t *path_target;

      /**
       * @brief Spawn ID of path_target at goal-set time.
       */
      uint32_t path_target_spawn_id;
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
  AI_FUNC_GOAL_LONGRANGE,
  AI_FUNC_GOAL_HUNT,
  AI_FUNC_GOAL_WEAPONRY,
  AI_FUNC_GOAL_ACROBATICS,
  AI_FUNC_GOAL_FINDITEMS,
  AI_FUNC_GOAL_TURN,
  AI_FUNC_GOAL_MOVE,
  AI_FUNC_GOAL_TOTAL
} ai_func_goal_t;

/**
 * @brief Static bot definition from the roster.
 */
typedef struct {

  /**
   * @brief Bot display name.
   */
  const char *name;

  /**
   * @brief Player model/skin path.
   */
  const char *skin;

  /**
   * @brief 0.0 (easy) to 1.0 (hard): aim, turn speed, reaction time.
   */
  float skill;

  /**
   * @brief 0.0 (cautious) to 1.0 (reckless): retreat/chase/combat style.
   */
  float aggression;

  /**
   * @brief 0.0 (oblivious) to 1.0 (perceptive): item range, weapon choice.
   */
  float awareness;
} g_ai_roster_t;

/**
 * @brief Per-bot runtime personality, initialized from the roster entry on spawn.
 */
typedef struct {

  /**
   * @brief Aim accuracy and reaction speed (0.0–1.0).
   */
  float skill;

  /**
   * @brief Tendency to chase vs. retreat (0.0–1.0).
   */
  float aggression;

  /**
   * @brief Item-detection range and weapon-selection quality (0.0–1.0).
   */
  float awareness;

  /**
   * @brief Per-bot phase offset for sinusoidal aim wobble.
   */
  float aim_phase;
} ai_personality_t;

/**
 * @brief AI-specific per-client state.
 */
typedef struct ai_s {

  /**
   * @brief Pointer to this bot's static roster definition.
   */
  const g_ai_roster_t *roster;

  /**
   * @brief Runtime personality derived from the roster.
   */
  ai_personality_t personality;

  /**
   * @brief Next think times indexed by ai_func_goal_t.
   */
  uint32_t func_goal_next_thinks[AI_FUNC_GOAL_TOTAL];

  /**
   * @brief Current movement/navigation goal.
   */
  ai_goal_t move_target;

  /**
   * @brief Saved movement goal, restored after a detour.
   */
  ai_goal_t backup_move_target;

  /**
   * @brief Current combat/enemy goal.
   */
  ai_goal_t combat_target;

  /**
   * @brief Next level time to re-evaluate weapon selection.
   */
  uint32_t weapon_check_time;

  /**
   * @brief Level time before the bot attempts to reacquire a goal.
   */
  uint32_t reacquire_time;

  /**
   * @brief Random per-bot offset applied to distress jump timing.
   */
  uint32_t distress_jump_offset;
} ai_t;

#endif /* __GAME_LOCAL_H__ */
