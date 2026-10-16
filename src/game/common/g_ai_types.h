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
#define AI_NODE_INVALID ((GameAiNodeId)-1)

/**
 * @brief Temporary hardening for #960 (dangling `GameAiGoal` entity pointer
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
 * @brief The type of goal we're after. This controls which variant in `GameAiGoal`
 * we can access.
 */
typedef enum {
  AI_GOAL_NONE,
  AI_GOAL_POSITION,
  AI_GOAL_ENTITY,
  AI_GOAL_PATH
} GameAiGoalType;

/**
 * @brief Bot combat styles.
 */
typedef enum {
  AI_COMBAT_NONE,
  AI_COMBAT_CLOSE,
  AI_COMBAT_FLANK,
  AI_COMBAT_WANDER,
  AI_COMBAT_TOTAL
} GameAiCombatType;

/**
 * @brief Bot trick jump timing states.
 */
typedef enum {
  TRICK_JUMP_NONE,
  TRICK_JUMP_START,
  TRICK_JUMP_WAITING,
  TRICK_JUMP_TURNING
} GameAiTrickJump;

/**
 * @brief The variant structure of a goal.
 */
typedef struct {
  /**
   * @brief Type of this goal; controls which union variant is active.
   */
  GameAiGoalType type;

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
      GameAiCombatType combatType;

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
       * @brief Array of `GameAiNodeId` forming the route.
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
      GameAiTrickJump trickJump;

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
} GameAiGoal;

/**
 * @brief A functional AI goal. It returns the amount of time to wait
 * until the goal should be run again.
 */
typedef uint32_t (*GameAiGoalFunc)(GameClient *cl, PMoveCmd *cmd);

/**
 * @brief Functional AI goal slot IDs, one per periodic decision function.
 */
typedef enum {
  AI_FUNC_GOAL_TARGET,
  AI_FUNC_GOAL_STATE,
  AI_FUNC_GOAL_WEAPONRY,
  AI_FUNC_GOAL_ACROBATICS,
  AI_FUNC_GOAL_TURN,
  AI_FUNC_GOAL_MOVE,
  AI_FUNC_GOAL_TOTAL
} GameAiFuncGoal;

/**
 * @brief The states of a bot's decision state machine. Only the active state sets the bot's move
 * target.
 */
typedef enum {
  /**
   * @brief Go to the item of most value for its travel time.
   */
  AI_STATE_COLLECT,

  /**
   * @brief Go to a distant node, because no item has value to the bot.
   */
  AI_STATE_ROAM,

  /**
   * @brief Take an item that adds little travel time to the route to the goal, then resume it.
   */
  AI_STATE_DETOUR,

  /**
   * @brief Fight the combat target.
   */
  AI_STATE_FIGHT,

  /**
   * @brief Go to where the bot last saw the combat target that it lost.
   */
  AI_STATE_CHASE,

  /**
   * @brief Go to health, armor or a weapon, away from the combat target, and fire back.
   */
  AI_STATE_RETREAT,

  /**
   * @brief Go to where the bot heard an enemy, or was shot from.
   */
  AI_STATE_INVESTIGATE,

  /**
   * @brief Press the button that opens the way to the item to collect, then collect it.
   */
  AI_STATE_ACTIVATE,

  /**
   * @brief Leave the lava that the bot is in, by the nearest way out.
   */
  AI_STATE_ESCAPE,

  /**
   * @brief Climb the ladder that the bot holds, and nothing else: no fight, no fire, no new goal.
   */
  AI_STATE_CLIMB,

  AI_STATE_TOTAL
} GameAiStateId;

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
} GameAiRoster;

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
} GameAiPersonality;

/**
 * @brief The most item respawns that a bot remembers.
 */
#define AI_ITEM_TIMINGS 16

/**
 * @brief An item that a bot saw or heard taken, and when it returns.
 */
typedef struct {
  /**
   * @brief The item's entity number.
   */
  int32_t number;

  /**
   * @brief Level time at which the item respawns.
   */
  uint32_t respawnTime;
} GameAiItemTiming;

/**
 * @brief Why a bot on a path does not strafe jump, as `G_Ai_StrafeJump` last found, for the probe.
 */
typedef enum {
  /**
   * @brief The bot strafe jumps.
   */
  AI_STRAFE_BLOCK_NONE,

  /**
   * @brief The bot's skill is too low, or the level's movement has no strafe jumping.
   */
  AI_STRAFE_BLOCK_SKILL,

  /**
   * @brief The bot is in water, on a mover, crouched, on a ladder, pushed, teleported, trick
   * jumping or water jumping, or its path leads onto a mover that is not there.
   */
  AI_STRAFE_BLOCK_STATE,

  /**
   * @brief No node ahead passes the turn, floor and trigger tests of `G_Ai_StrafeTarget`.
   */
  AI_STRAFE_BLOCK_TURN,

  /**
   * @brief The path ahead is too short for the bot's skill: see `AI_STRAFE_RUN_MAX` and
   * `AI_STRAFE_RUN_MIN`.
   */
  AI_STRAFE_BLOCK_SHORT,

  /**
   * @brief The simulated jump does not land safely.
   */
  AI_STRAFE_BLOCK_UNSAFE,

  /**
   * @brief The bot is in the air, but not strafe jumping: after a drop, a jump link, or a strafe
   * jump that it gave up in flight.
   */
  AI_STRAFE_BLOCK_FALL,

  AI_STRAFE_BLOCK_TOTAL
} GameAiStrafeBlock;

/**
 * @brief The most items that a bot avoids at once.
 */
#define AI_AVOIDS 8

/**
 * @brief The most positions that a bot remembers on its trail.
 */
#define AI_TRAIL 12

/**
 * @brief An item that a bot took, or failed to reach, and so avoids for a time.
 */
typedef struct {
  /**
   * @brief The item's entity number.
   */
  int32_t number;

  /**
   * @brief The item's spawn ID, because a dropped item's entity number is reused once it is taken.
   */
  uint8_t spawnId;

  /**
   * @brief Level time at which the bot began to avoid the item.
   */
  uint32_t time;
} GameAiAvoid;

/**
 * @brief AI-specific per-client state.
 */
typedef struct GameAi {

  /**
   * @brief Pointer to this bot's static roster definition.
   */
  const GameAiRoster *roster;

  /**
   * @brief Runtime personality derived from the roster.
   */
  GameAiPersonality personality;

  /**
   * @brief True if a command added this bot, so that the population control neither counts nor
   * kicks it.
   */
  bool added;

  /**
   * @brief Next think times indexed by `GameAiFuncGoal`.
   */
  uint32_t funcGoalNextThinks[AI_FUNC_GOAL_TOTAL];

  /**
   * @brief Current movement/navigation goal.
   */
  GameAiGoal moveTarget;

  /**
   * @brief Current combat/enemy goal.
   */
  GameAiGoal combatTarget;

  /**
   * @brief The active state of the decision state machine.
   */
  GameAiStateId state;

  /**
   * @brief The state that `AI_STATE_CLIMB` interrupted, to which the bot returns, with its path,
   * off the ladder.
   */
  GameAiStateId climbState;

  /**
   * @brief The `stateTime` of `climbState`. The bot returns to it later by the time of the climb,
   * so that the climb does not count against the timeouts of the state.
   */
  uint32_t climbStateTime;

  /**
   * @brief Level time until which the bot leaves the ladder on which its climb was blocked.
   */
  uint32_t ladderLeaveTime;

  /**
   * @brief The direction in which the bot leaves that ladder: the normal of its face.
   */
  Vec3 ladderLeaveDir;

  /**
   * @brief Level time at which the bot entered its state.
   */
  uint32_t stateTime;

  /**
   * @brief Level time at which the state next reviews the bot's choice of goal, item or approach.
   */
  uint32_t reviewTime;

  /**
   * @brief Level time at which the state last reviewed the choices of the bot with no move target,
   * or 0 if its last review was with one.
   */
  uint32_t reviewedTime;

  /**
   * @brief True once a chase or an investigation has planned its path.
   */
  bool statePathed;

  /**
   * @brief The entity number of the item to collect, or -1 for none.
   */
  int32_t goalNumber;

  /**
   * @brief The spawn ID of the item to collect, to detect entity reuse.
   */
  uint8_t goalSpawnId;

  /**
   * @brief The node of the item to collect, or of the roam point, or `AI_NODE_INVALID` for none.
   * A detour returns to it.
   */
  GameAiNodeId goalNode;

  /**
   * @brief The index of the gate through which the bot goes to its goal item, or -1. It stays set
   * after `AI_STATE_ACTIVATE`, until the bot takes an item or chooses another.
   */
  int32_t gate;

  /**
   * @brief In `AI_STATE_ACTIVATE`, the index of the gate's button that the bot presses.
   */
  int32_t gateButton;

  /**
   * @brief In `AI_STATE_ACTIVATE`, the node from which the bot presses the button.
   */
  GameAiNodeId gateNode;

  /**
   * @brief In `AI_STATE_ACTIVATE`, the level time at which the bot found the activated doors at
   * rest, with no path to the item, or 0.
   */
  uint32_t gateRestTime;

  /**
   * @brief True if the bot lost sight of its last combat target, rather than killed it or lost it
   * to death or disconnect.
   */
  bool combatTargetLost;

  /**
   * @brief The items that the bot avoids, so that it roams the map rather than circles a few items.
   */
  GameAiAvoid avoids[AI_AVOIDS];

  /**
   * @brief Where the bot was recently, one position each `AI_TRAIL_MILLIS`, so that it roams to
   * somewhere new rather than back to where it came from.
   */
  Vec3 trail[AI_TRAIL];

  /**
   * @brief The count of positions that the bot added to its trail. The newest is at
   * `trailCount - 1`, modulo `AI_TRAIL`.
   */
  uint32_t trailCount;

  /**
   * @brief Level time at which the bot last added a position to its trail.
   */
  uint32_t trailTime;

  /**
   * @brief Next level time to re-evaluate weapon selection.
   */
  uint32_t weaponCheckTime;

  /**
   * @brief Level time until which the bot, once it has no move target, avoids the goal of a path
   * that ended in distress, and chooses another.
   */
  uint32_t reacquireTime;

  /**
   * @brief Level time at which the bot began to wait at its goal item for it to respawn, or 0 while
   * it does not wait.
   */
  uint32_t waitTime;

  /**
   * @brief Random per-bot offset applied to distress jump timing.
   */
  uint32_t distressJumpOffset;

  /**
   * @brief Frame number for which the lookahead ground-loss result was computed.
   */
  uint32_t lookaheadFrame;

  /**
   * @brief Cached lookahead result: true if the bot will lose ground 100ms ahead, other than to
   * the ladder of its climb, which it grabs, and which is not an edge. Valid only when
   * `lookaheadFrame == gameLevel.frameNum`.
   */
  bool lookaheadNoGround;

  /**
   * @brief Level time before the bot attempts again to path around a raised plat.
   */
  uint32_t replanTime;

  /**
   * @brief Level time at which a `trigger_push` launched the bot, or 0 once it lands. The bot gives
   * no move input in between, so that it follows the arc of the jump pad. A push straight up does
   * not set it, so that the bot steers to the ledge above.
   */
  uint32_t pushTime;

  /**
   * @brief The position at which the bot last stood on the ground, to which it steers back if it
   * falls toward the void.
   */
  Vec3 groundPosition;

  /**
   * @brief Frame number for which `fallsToHazard` was computed.
   */
  uint32_t fallFrame;

  /**
   * @brief True if the bot is in the air, and `G_Ai_FallsToHazard` ends its fall in a hazard.
   * Valid only when `fallFrame == gameLevel.frameNum`.
   */
  bool fallsToHazard;

  /**
   * @brief True if the bot falls toward a hazard, and `G_Ai_SimulateDrop` lands it safely if it
   * steers toward its next path node. Valid only when `fallFrame == gameLevel.frameNum`.
   */
  bool fallToNode;

  /**
   * @brief The frame of the last `G_Ai_SimulateDrop`, whose result `dropSafe` holds for
   * `dropTarget` from `dropOrigin`.
   */
  uint32_t dropFrame;

  /**
   * @brief The origin of the bot at the last `G_Ai_SimulateDrop`. The bot moves between the passes
   * of `G_Ai_ClientThink` in one frame.
   */
  Vec3 dropOrigin;

  /**
   * @brief The target of the last `G_Ai_SimulateDrop`.
   */
  Vec3 dropTarget;

  /**
   * @brief The result of the last `G_Ai_SimulateDrop`.
   */
  bool dropSafe;

  /**
   * @brief Level time before the bot may turn again while it wanders.
   */
  uint32_t wanderTurnTime;

  /**
   * @brief The yaw in degrees that the bot wanders toward with no goal. It is not in the union of
   * `GameAiGoal`, whose entity pointer it shares memory with, because `G_Ai_Wander` turns it while
   * the move goal is the enemy.
   */
  float wanderAngle;

  /**
   * @brief Level time at which the bot last saw its combat target.
   */
  uint32_t combatTargetSeenTime;

  /**
   * @brief Where the bot last saw its combat target. A bot that loses sight of its target chases
   * this position, not the target's true one.
   */
  Vec3 combatTargetSeenPosition;

  /**
   * @brief The angular velocity of the view in degrees per second, so that turns ease in and out.
   */
  Vec3 viewVelocity;

  /**
   * @brief The combat target's velocity as the bot perceives it. It lags the true velocity by the
   * bot's reaction time, so a target that changes direction is briefly misjudged.
   */
  Vec3 perceivedVelocity;

  /**
   * @brief The pitch and yaw aim error in degrees: a random walk that the bot corrects over time.
   */
  Vec2 aimError;

  /**
   * @brief The view angles, before the aim error, at which the bot aims at its combat target: along
   * the arc for a lobbed weapon.
   */
  Vec3 aimAngles;

  /**
   * @brief The time in milliseconds to hold a hand grenade, so that it flies to the combat target.
   */
  uint32_t aimHoldMillis;

  /**
   * @brief The fractional error in the bot's estimate of its projectile speed. It is fixed for
   * each combat target, so that the lead is consistently wrong rather than jittery.
   */
  float leadError;

  /**
   * @brief The spot at which the bot lobs grenades while it has no combat target.
   */
  Vec3 spamPoint;

  /**
   * @brief A button that the bot shoots to press it, while it has no combat target, or NULL.
   */
  const GameEntity *shootTarget;

  /**
   * @brief Level time at which the current burst of grenade spam ends, or, between bursts, at
   * which the next may start.
   */
  uint32_t spamTime;

  /**
   * @brief The ammo count at which the current burst of grenade spam ends, or 0 between bursts.
   */
  int32_t spamAmmo;

  /**
   * @brief The map control items that the bot saw or heard taken, so that it can be there when
   * they return.
   */
  GameAiItemTiming itemTimings[AI_ITEM_TIMINGS];

  /**
   * @brief Level time at which the bot may next reverse its strafe around its target.
   */
  uint32_t strafeTime;

  /**
   * @brief Where the bot last heard an enemy, or was shot from.
   */
  Vec3 alertPosition;

  /**
   * @brief Level time of the alert, or 0 for none.
   */
  uint32_t alertTime;

  /**
   * @brief True once the bot has decided whether to investigate the alert.
   */
  bool alertInvestigated;

  /**
   * @brief The entity number of the last enemy that hurt the bot.
   */
  int32_t attacker;

  /**
   * @brief Level time at which `attacker` last hurt the bot.
   */
  uint32_t attackedTime;

  /**
   * @brief True from the takeoff of a strafe jump until the bot lands and does not jump again.
   */
  bool strafeJumping;

  /**
   * @brief True from the takeoff of a jump onto a ladder until the bot lands, grabs the ladder or
   * leaves the link.
   */
  bool ladderJumping;

  /**
   * @brief Why the bot did not strafe jump on its last move along a path, for the probe.
   */
  GameAiStrafeBlock strafeBlock;

  /**
   * @brief Level time until which `strafeLookIndex` and `strafeLookRun` are valid, for
   * `strafeLookPath` at `strafeLookPathIndex`.
   */
  uint32_t strafeLookTime;

  /**
   * @brief The path for which `strafeLookIndex` was found.
   */
  const Vector *strafeLookPath;

  /**
   * @brief The path index for which `strafeLookIndex` was found.
   */
  uint32_t strafeLookPathIndex;

  /**
   * @brief The index of the path node that a strafe jumping bot steers to, or -1 for none.
   */
  int32_t strafeLookIndex;

  /**
   * @brief The distance along the path to the node at `strafeLookIndex`.
   */
  float strafeLookRun;

  /**
   * @brief Level time until which `strafeSafe` is valid.
   */
  uint32_t strafeSafeTime;

  /**
   * @brief True if the strafe jump that `G_Ai_SimulateStrafeJump` last followed lands safely.
   */
  bool strafeSafe;

  /**
   * @brief The target of the strafe jump for which `strafeSafe` was found.
   */
  Vec3 strafeSafeTarget;
} GameAi;

#endif
