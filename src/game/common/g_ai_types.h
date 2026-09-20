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
 * Foundation, Inc., 59 Temple Place - Suite 330, Boston, MA 02111-1307, USA.
 */

#pragma once

#include "game/game.h"
#include "g_types.h"

/**
 * @brief Sentinel value indicating an invalid or unset navigation node ID.
 */
#define AI_NODE_INVALID ((AiNodeId)-1)

/**
 * @brief Temporary hardening for #960 (dangling `AiGoal` entity pointer
 * crashes in `G_Ai_Think`, root cause still unknown): re-resolves goal
 * entities by slot number against the canonical `ge.entities` table instead
 * of trusting the cached pointer, self-healing or clearing the goal as
 * needed, and logs a warning + backtrace to aid diagnosis in the field.
 * @remarks This is a stopgap, not a fix. Once the root cause is understood
 * and addressed, delete everything guarded by this flag (search for
 * `AI_GOAL_HARDENING`) and this comment.
 */
#define AI_GOAL_HARDENING 1

#if defined(__G_LOCAL_H__)

/**
 * @brief The default user info string (name and skin).
 */
#define DEFAULT_BOT_INFO "\\name\\newbiebot\\skin\\enforcer/default"

/**
 * @brief The type of goal we're after. This controls which variant in `AiGoal`
 * we can access.
 */
typedef enum {
  AI_GOAL_NONE,
  AI_GOAL_POSITION,
  AI_GOAL_ENTITY,
  AI_GOAL_PATH
} AiGoalType;

/**
 * @brief Bot combat styles.
 */
typedef enum {
  AI_COMBAT_NONE,
  AI_COMBAT_CLOSE,
  AI_COMBAT_FLANK,
  AI_COMBAT_WANDER,
  AI_COMBAT_TOTAL
} AiCombatType;

/**
 * @brief Bot trick jump timing states.
 */
typedef enum {
  TRICK_JUMP_NONE,
  TRICK_JUMP_START,
  TRICK_JUMP_WAITING,
  TRICK_JUMP_TURNING
} AiTrickJump;

/**
 * @brief The variant structure of a goal.
 */
typedef struct {

  /**
   * @brief Type of this goal; controls which union variant is active.
   */
  AiGoalType type;

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
  float lastDistress;

  /**
   * @brief Last recorded distance to the goal destination.
   */
  float lastDistance;

  /**
   * @brief When true, extends the distress timeout from 1s to 15s.
   */
  bool distressExtension;
  
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
      Vec3 pos;
    } position;

    struct {

      /**
       * @brief Target entity.
       */
      const GameEntity *ent;

#if AI_GOAL_HARDENING
      /**
       * @brief Entity slot number at goal-set time; used to safely re-resolve
       * `ent` against the canonical `ge.entities` table instead of trusting a
       * cached pointer that may have been corrupted or gone stale (#960).
       */
      int32_t number;
#endif

      /**
       * @brief Spawn ID at goal-set time; used to detect entity reuse.
       */
      uint8_t spawnId;

      // specific to combat goal

      /**
       * @brief Active combat style against this entity.
       */
      AiCombatType combatType;

      /**
       * @brief Level time when the bot first locked on to this enemy.
       */
      uint32_t lockOnTime;

      /**
       * @brief Current flank offset angle for circle-strafing.
       */
      float flankAngle;
    } entity;

    struct {

      /**
       * @brief Array of `AiNodeId` forming the route.
       */
      Vector *path;

      /**
       * @brief Index of the current node being navigated toward.
       */
      uint32_t pathIndex;

      /**
       * @brief World positions of current and next node.
       */
      Vec3 pathPosition, nextPathPosition;

      /**
       * @brief Current trick jump state for this path segment.
       */
      AiTrickJump trickJump;

      /**
       * @brief World position used as the trick jump target.
       */
      Vec3 trickPosition;

      /**
       * @brief Optional entity the path is leading to.
       */
      const GameEntity *pathTarget;

#if AI_GOAL_HARDENING
      /**
       * @brief Entity slot number of `pathTarget` at goal-set time; see
       * `entity.number` above.
       */
      int32_t pathTargetNumber;
#endif

      /**
       * @brief Spawn ID of `pathTarget` at goal-set time.
       */
      uint32_t pathTargetSpawnId;
    } path;
  };
} AiGoal;

/**
 * @brief A functional AI goal. It returns the amount of time to wait
 * until the goal should be run again.
 */
typedef uint32_t (*G_Ai_GoalFunc)(GameClient *cl, PMoveCmd *cmd);

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
} AiFuncGoal;

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
   * @brief Stable per-bot UUID for stats tracking.
   */
  const char *guid;

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
} AiRoster;

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
  float aimPhase;
} AiPersonality;

/**
 * @brief AI-specific per-client state.
 */
typedef struct Ai {

  /**
   * @brief Pointer to this bot's static roster definition.
   */
  const AiRoster *roster;

  /**
   * @brief Runtime personality derived from the roster.
   */
  AiPersonality personality;

  /**
   * @brief Next think times indexed by `AiFuncGoal`.
   */
  uint32_t funcGoalNextThinks[AI_FUNC_GOAL_TOTAL];

  /**
   * @brief Current movement/navigation goal.
   */
  AiGoal moveTarget;

  /**
   * @brief Saved movement goal, restored after a detour.
   */
  AiGoal backupMoveTarget;

  /**
   * @brief Current combat/enemy goal.
   */
  AiGoal combatTarget;

  /**
   * @brief Next level time to re-evaluate weapon selection.
   */
  uint32_t weaponCheckTime;

  /**
   * @brief Level time before the bot attempts to reacquire a goal.
   */
  uint32_t reacquireTime;

  /**
   * @brief Random per-bot offset applied to distress jump timing.
   */
  uint32_t distressJumpOffset;

  /**
   * @brief Frame number for which the lookahead ground-loss result was computed.
   */
  uint32_t lookaheadFrame;

  /**
   * @brief Cached lookahead result: true if the bot will lose ground 100ms ahead.
   * Valid only when lookaheadFrame == gLevel.frameNum.
   */
  bool lookaheadNoGround;
} Ai;

#endif
