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

#include "g_local.h"

/**
 * @brief The interval in milliseconds at which a bot runs its state machine.
 */
#define AI_STATE_MILLIS 100

/**
 * @brief The interval in milliseconds at which a state reviews the bot's choice of item, detour or
 * approach, while the bot has a move target.
 */
#define AI_REVIEW_MILLIS 500

/**
 * @brief The distance within which a bot finds the node that it travels from, or that an item or
 * a position lies at.
 */
#define AI_NODE_DISTANCE 256.f

/**
 * @brief The travel cost at which an item's utility is half of its value. A travel cost is a
 * distance at run speed, so this is about two seconds of travel.
 */
#define AI_ITEM_COST_SCALE (PM_SPEED_RUN * 2.f)

/**
 * @brief The random spread of an item's utility, for the least and the most aware bots.
 */
#define AI_ITEM_NOISE_MAX .3f
#define AI_ITEM_NOISE_MIN .05f

/**
 * @brief The value scales of an item that the bot covets or that would arm it, and of health and
 * armor for a bot that should retreat.
 */
#define AI_COVETED_SCALE 4.f
#define AI_HEAL_SCALE 3.f

/**
 * @brief The least share of an item that a bot would gain for it to want the item, so that a bot
 * that needs little roams rather than tops itself up at the nearest item, again and again.
 */
#define AI_ITEM_MIN_NEED .25f

/**
 * @brief A bot changes the item that it collects only for one of this many times more utility, so
 * that it finishes what it starts.
 */
#define AI_SWITCH_SCALE 1.5f

/**
 * @brief The most travel cost that a detour may add to the route to the goal: about one second.
 */
#define AI_DETOUR_COST PM_SPEED_RUN

/**
 * @brief The most items for which a bot plans the route from the item to its goal.
 */
#define AI_DETOUR_CANDIDATES 3

/**
 * @brief The longest time in milliseconds that a detour lasts.
 */
#define AI_DETOUR_MILLIS 3000

/**
 * @brief The time in milliseconds over which a bot's avoidance of an item fades.
 */
#define AI_AVOID_MILLIS 20000

/**
 * @brief The utility scale of an item that the bot just began to avoid.
 */
#define AI_AVOID_SCALE .2f

/**
 * @brief The longest time in milliseconds that a bot waits at an item for it to respawn. It gives
 * the item up then, even if it remembered the respawn wrongly.
 */
#define AI_WAIT_MILLIS 3000

/**
 * @brief The least travel cost of a roam point, so that a bot with nothing to collect crosses the
 * map.
 */
#define AI_ROAM_COST (PM_SPEED_RUN * 4.f)

/**
 * @brief The number of random nodes that a bot considers for a roam point.
 */
#define AI_ROAM_SAMPLES 32

/**
 * @brief The time in milliseconds between the positions of a bot's trail, which spans
 * `AI_TRAIL` times this.
 */
#define AI_TRAIL_MILLIS 5000

/**
 * @brief The distance within which a bot has reached its roam point, or waits for the item that it
 * collects to return.
 */
#define AI_ROAM_REACHED 64.f

/**
 * @brief The move target priorities of the states that do not collect an item.
 */
#define AI_ROAM_PRIORITY .1f
#define AI_PURSUE_PRIORITY .7f

/**
 * @brief A fighting bot strafes around an enemy within this multiple of its preferred range. Beyond
 * it, the bot collects items while it fires, or paths toward the enemy if it has none to collect.
 */
#define AI_ENGAGE_SCALE 1.5f

/**
 * @brief A bot that strafes around an enemy stops only beyond this multiple of its preferred range,
 * so that an enemy near `AI_ENGAGE_SCALE` does not switch the bot at each review.
 */
#define AI_DISENGAGE_SCALE 2.f

/**
 * @brief The most travel cost of an item that a bot strafing around an enemy goes for.
 */
#define AI_FIGHT_ITEM_COST 256.f

/**
 * @brief A retreating bot fights again once its health exceeds this multiple of its retreat health.
 */
#define AI_RECOVER_SCALE 1.5f

/**
 * @brief The utility scale of health and armor that lie nearer to the enemy than to a retreating
 * bot.
 */
#define AI_TOWARD_ENEMY_SCALE .25f

/**
 * @brief The chance that a bot chases a lost enemy, or investigates an alert, for the least and the
 * most aggressive bots.
 */
#define AI_PURSUE_CHANCE_MIN .25f
#define AI_PURSUE_CHANCE_MAX .9f

/**
 * @brief The longest time in milliseconds that a chase or an investigation lasts.
 */
#define AI_PURSUE_MILLIS 6000

/**
 * @brief The most items that a map marks with `ai_access_by`, and the most buttons and doors of
 * each.
 */
#define AI_MAX_ACCESS 32
#define AI_ACCESS_BUTTONS 4
#define AI_ACCESS_DOORS 8

/**
 * @brief The most nodes from which a bot walks to a button.
 */
#define AI_BUTTON_NODES 8

/**
 * @brief The factor by which the way from a button to the item that it opens is taken to be longer
 * than the straight line between them, because the way is not linked until the doors move.
 */
#define AI_ACCESS_DETOUR_SCALE 1.5f

/**
 * @brief The longest time in milliseconds that a bot spends in `AI_STATE_ACTIVATE`.
 */
#define AI_ACTIVATE_MILLIS 15000

/**
 * @brief The distance from a button's center within which a bot walks straight into it.
 */
#define AI_ACTIVATE_REACH 128.f

/**
 * @brief The distance from a button that a shot presses, within which a bot that sees it shoots it.
 */
#define AI_SHOOT_REACH 768.f

/**
 * @brief The height of the eye above a node, from which a bot sees a button that a shot presses:
 * the lowest eye of the movements.
 */
#define AI_SHOOT_EYE_HEIGHT 22.f

/**
 * @brief The time in milliseconds after the doors of an access come to rest activated, within which
 * a bot MUST find a path to the item.
 */
#define AI_ACCESS_PATH_MILLIS 1000

/**
 * @brief The time in milliseconds for which no bot tries an access that failed once, because its
 * doors did not open a way to the item, or the bot did not press the button. Each further failure
 * doubles it, up to `AI_ACCESS_RETRY_DOUBLINGS` times, until the item is taken through the access.
 */
#define AI_ACCESS_RETRY_MILLIS 60000
#define AI_ACCESS_RETRY_DOUBLINGS 4

/**
 * @brief The most doors that a map marks with `"ai_hint" "trap"`, grouped by `targetname`, and the
 * most doors and buttons of each trap.
 */
#define AI_MAX_TRAPS 16
#define AI_TRAP_DOORS 8
#define AI_TRAP_BUTTONS 4

/**
 * @brief The distance from a trap's button within which a bot in a fight walks into it, while its
 * enemy stands on the trap.
 */
#define AI_TRAP_REACH 256.f

/**
 * @brief The doors with one `targetname` that a map marks with `"ai_hint" "trap"`, and the buttons
 * that target them, which a bot touches to spring the trap under an enemy.
 */
typedef struct {
  const GameEntity *doors[AI_TRAP_DOORS];
  int32_t numDoors;
  const GameEntity *buttons[AI_TRAP_BUTTONS];
  int32_t numButtons;
} GameAiTrap;

/**
 * @brief An item that a map marks with `ai_access_by`: the doors with that `targetname`, and the
 * buttons that target them, which a bot touches to move the doors.
 */
typedef struct {
  const GameEntity *ent;
  const GameEntity *doors[AI_ACCESS_DOORS];
  int32_t numDoors;
  const GameEntity *buttons[AI_ACCESS_BUTTONS];
  int32_t numButtons;

  /**
   * @brief The nodes of each button within `AI_ACTIVATE_REACH` that see it.
   */
  GameAiNodeId buttonNodes[AI_ACCESS_BUTTONS][AI_BUTTON_NODES];
  int32_t numButtonNodes[AI_ACCESS_BUTTONS];

  /**
   * @brief The shortest time in milliseconds for which a door stays activated after a press: its
   * travel, and its wait.
   */
  uint32_t openMillis;

  /**
   * @brief The level time at which a bot last failed to use the access, or 0.
   */
  uint32_t failTime;

  /**
   * @brief The count of failures since the item was last taken through the access.
   */
  int32_t numFails;
} GameAiAccess;

/**
 * @brief An item that a bot may go to, with its node and its utility.
 */
typedef struct {
  const GameEntity *ent;
  GameAiNodeId node;
  float value;
  float utility;

  /**
   * @brief The index of the item's access, and of its button, if the bot reaches the item only
   * through a button, or -1.
   */
  int32_t access;
  int32_t button;

  /**
   * @brief The node from which the bot walks to the button.
   */
  GameAiNodeId buttonNode;
} GameAiChoice;

static const char *const stateNames[AI_STATE_TOTAL] = {
  [AI_STATE_COLLECT] = "collect",
  [AI_STATE_ROAM] = "roam",
  [AI_STATE_DETOUR] = "detour",
  [AI_STATE_FIGHT] = "fight",
  [AI_STATE_CHASE] = "chase",
  [AI_STATE_RETREAT] = "retreat",
  [AI_STATE_INVESTIGATE] = "investigate",
  [AI_STATE_ACTIVATE] = "activate",
};

static struct {
  /**
   * @brief The travel costs from the bot that decides, to every node.
   */
  float *costs;

  /**
   * @brief The number of costs that `costs` holds.
   */
  uint32_t count;

  /**
   * @brief The items that a map marks with `ai_access_by`.
   */
  GameAiAccess access[AI_MAX_ACCESS];
  int32_t numAccess;

  /**
   * @brief The traps that a map marks with `"ai_hint" "trap"`.
   */
  GameAiTrap traps[AI_MAX_TRAPS];
  int32_t numTraps;
} module;

/**
 * @return The travel costs from the bot to every node, or NULL if the bot is not near the graph.
 * The costs are valid until the next call.
 */
static const float *G_Ai_TravelCosts(const GameClient *cl) {

  const uint32_t count = G_Ai_Node_Count();

  if (count == 0) {
    return NULL;
  }

  if (count > module.count) {
    module.costs = realloc(module.costs, count * sizeof(float));

    if (!module.costs) {
      G_Error("Failed to allocate travel costs for %u nodes\n", count);
    }

    module.count = count;
  }

  const GameAiNodeId start = G_Ai_Node_FindClosest(cl->entity->s.origin, AI_NODE_DISTANCE, true, true);

  if (!G_Ai_Node_TravelCosts(cl, start, module.costs)) {
    return NULL;
  }

  return module.costs;
}

/**
 * @return The travel time in milliseconds for the travel cost.
 */
static inline uint32_t G_Ai_TravelMillis(const float cost) {
  return (uint32_t) (cost / PM_SPEED_RUN * 1000.f);
}

/**
 * @return The utility of an item of the given value at the given travel cost.
 */
static inline float G_Ai_Utility(const float value, const float cost) {
  return value / (1.f + cost / AI_ITEM_COST_SCALE);
}

/**
 * @return The cost of the item to the bot: the travel cost to it, and the time that the bot would
 * wait there for it to return, in the same units.
 */
static float G_Ai_ItemCost(const GameClient *cl, const GameEntity *ent, const float cost) {

  const uint32_t wait = G_Ai_ItemWaitMillis(cl, ent, G_Ai_TravelMillis(cost));

  return cost + wait * PM_SPEED_RUN / 1000.f;
}

/**
 * @see g_ai_state.h
 */
void G_Ai_Avoid(GameClient *cl, const GameEntity *item) {

  GameAiAvoid *slot = &cl->ai->avoids[0];

  for (int32_t i = 0; i < AI_AVOIDS; i++) {
    GameAiAvoid *avoid = &cl->ai->avoids[i];

    if (avoid->time && avoid->number == item->s.number && avoid->spawnId == item->s.spawnId) {
      slot = avoid;
      break;
    }

    if (avoid->time < slot->time) {
      slot = avoid;
    }
  }

  slot->number = item->s.number;
  slot->spawnId = item->s.spawnId;
  slot->time = gameLevel.time;
}

/**
 * @return The utility scale of the item, which rises back to 1 as the bot's avoidance of it fades.
 */
static float G_Ai_AvoidScale(const GameClient *cl, const GameEntity *item) {

  for (int32_t i = 0; i < AI_AVOIDS; i++) {
    const GameAiAvoid *avoid = &cl->ai->avoids[i];

    if (avoid->time && avoid->number == item->s.number && avoid->spawnId == item->s.spawnId) {
      const uint32_t elapsed = gameLevel.time - avoid->time;

      if (elapsed < AI_AVOID_MILLIS) {
        return Mixf(AI_AVOID_SCALE, 1.f, elapsed / (float) AI_AVOID_MILLIS);
      }
    }
  }

  return 1.f;
}

/**
 * @return The value of the item to the bot: its priority, or its ammo's for a weapon that the bot
 * holds, by the share of it that the bot would gain. 0 if the bot would gain less than
 * `AI_ITEM_MIN_NEED` of it, cannot take it, or would arrive before it returns.
 * @param eta The time in milliseconds that the bot needs to get to the item.
 * @param retreat True to value only health, armor and the weapons that would arm the bot.
 */
static float G_Ai_ItemValue(const GameClient *cl, const GameEntity *ent, const uint32_t eta, const bool retreat) {

  const GameItem *item = ent->item;

  if (!item) {
    return 0.f;
  }

  if (ent->solid != SOLID_TRIGGER && !G_Ai_IsTimedItem(cl, ent, eta)) {
    return 0.f;
  }

  const bool coveted = G_Ai_IsCovetedItem(cl, ent);

  if (!G_Ai_IsItemKnown(cl, ent, coveted) || !G_Ai_CanPickup(cl, ent)) {
    return 0.f;
  }

  const bool arms = G_Ai_WouldArm(cl, item);
  const bool heals = item->def.type == ITEM_TYPE_HEALTH || item->def.type == ITEM_TYPE_ARMOR;

  if (retreat && !heals && !arms) {
    return 0.f;
  }

  const float need = G_Ai_ItemNeed(cl, ent);

  if (need < AI_ITEM_MIN_NEED) {
    return 0.f;
  }

  const bool owned = item->def.type == ITEM_TYPE_WEAPON && cl->inventory[item->def.tag];
  const GameItem *valued = owned && item->def.ammo ? &gameItems[item->def.ammo] : item;

  float value = valued->def.priority * need;

  if (coveted || arms) {
    value *= AI_COVETED_SCALE;
  } else if (heals && (retreat || G_Ai_ShouldRetreat(cl))) {
    value *= AI_HEAL_SCALE;
  }

  return value * G_Ai_AvoidScale(cl, ent);
}

/**
 * @return The node of the item, or `AI_NODE_INVALID`.
 */
static GameAiNodeId G_Ai_ItemNode(const GameEntity *ent) {

  const GameAiNodeId node = ent->node != AI_NODE_INVALID
    ? ent->node
    : G_Ai_Node_FindClosest(ent->s.origin, AI_NODE_DISTANCE, true, true);

  return node < G_Ai_Node_Count() ? node : AI_NODE_INVALID;
}

/**
 * @return The travel cost to the item through the nearest button of its access: to a node of the
 * button, and from there to the item, and the access, the button and the node, or `FLT_MAX` if the
 * item has no access, or the bot cannot reach a button and then the item before its doors move back.
 */
static float G_Ai_AccessCost(const float *costs, const GameEntity *ent, int32_t *access, int32_t *button,
                             GameAiNodeId *buttonNode) {

  float best = FLT_MAX;

  for (int32_t i = 0; i < module.numAccess; i++) {
    const GameAiAccess *a = &module.access[i];

    if (a->ent != ent) {
      continue;
    }

    if (a->numFails && gameLevel.time - a->failTime < (uint32_t) AI_ACCESS_RETRY_MILLIS << Mini(a->numFails - 1, AI_ACCESS_RETRY_DOUBLINGS)) {
      continue;
    }

    for (int32_t j = 0; j < a->numButtons; j++) {

      for (int32_t k = 0; k < a->numButtonNodes[j]; k++) {
        const GameAiNodeId node = a->buttonNodes[j][k];
        const float through = Vec3_Distance(G_Ai_Node_GetPosition(node), ent->s.origin) * AI_ACCESS_DETOUR_SCALE;

        if (costs[node] == FLT_MAX || costs[node] + through >= best || G_Ai_TravelMillis(through) > a->openMillis) {
          continue;
        }

        best = costs[node] + through;
        *access = i;
        *button = j;
        *buttonNode = node;
      }
    }
  }

  return best;
}

/**
 * @return The item of most utility, its value per travel time, that the bot can reach within
 * `maxCost`, or a choice with no entity.
 * @param maxCost The most travel cost, or 0 for no limit.
 * @param retreat True to choose only health, armor and the weapons that would arm the bot, and to
 * shun those that lie toward the enemy.
 * @param access True to choose also an item that the bot reaches only through a button.
 */
static GameAiChoice G_Ai_ChooseItem(const GameClient *cl, const float *costs, const float maxCost, const bool retreat,
                                    const bool access) {

  GameAiChoice best = { .node = AI_NODE_INVALID, .access = -1, .button = -1, .buttonNode = AI_NODE_INVALID };

  const GameEntity *enemy = cl->ai->combatTarget.type == AI_GOAL_ENTITY ? cl->ai->combatTarget.entity.ent : NULL;
  const float noise = Mixf(AI_ITEM_NOISE_MAX, AI_ITEM_NOISE_MIN, cl->ai->personality.awareness);

  G_ForEachEntity(ent, {
    if (!ent->item) {
      continue;
    }

    const GameAiNodeId node = G_Ai_ItemNode(ent);

    if (node == AI_NODE_INVALID) {
      continue;
    }

    float cost = costs[node];
    int32_t through = -1;
    int32_t button = -1;
    GameAiNodeId buttonNode = AI_NODE_INVALID;

    if (cost == FLT_MAX && access) {
      cost = G_Ai_AccessCost(costs, ent, &through, &button, &buttonNode);
    }

    if (cost == FLT_MAX || (maxCost > 0.f && cost > maxCost)) {
      continue;
    }

    const float value = G_Ai_ItemValue(cl, ent, G_Ai_TravelMillis(cost), retreat);

    if (value <= 0.f) {
      continue;
    }

    float utility = G_Ai_Utility(value, G_Ai_ItemCost(cl, ent, cost)) * RandomRangef(1.f - noise, 1.f + noise);

    if (retreat && enemy &&
        Vec3_Distance(ent->s.origin, enemy->s.origin) < Vec3_Distance(ent->s.origin, cl->entity->s.origin)) {
      utility *= AI_TOWARD_ENEMY_SCALE;
    }

    if (utility > best.utility) {
      best.ent = ent;
      best.node = node;
      best.value = value;
      best.utility = utility;
      best.access = through;
      best.button = button;
      best.buttonNode = buttonNode;
    }
  });

  return best;
}

/**
 * @brief Sets the bot's move target to a path to the node.
 * @return True if the bot has a path.
 */
static bool G_Ai_PathTo(GameClient *cl, const GameAiNodeId dest, const float priority, const GameEntity *target) {

  const GameAiNodeId src = G_Ai_Node_FindClosest(cl->entity->s.origin, AI_NODE_DISTANCE, true, true);

  if (src == AI_NODE_INVALID || dest == AI_NODE_INVALID) {
    return false;
  }

  Vector *path = G_Ai_Node_FindPath(cl, src, dest, G_Ai_Node_Heuristic, 0.f, NULL);

  if (!path) {
    return false;
  }

  G_Ai_SetPathGoal(cl, &cl->ai->moveTarget, priority, path, target);
  release(path);

  return true;
}

/**
 * @return The entity that the bot's move target leads to, or NULL.
 */
static const GameEntity *G_Ai_MoveEntity(const GameClient *cl) {

  const GameAiGoal *goal = &cl->ai->moveTarget;

  if (goal->type == AI_GOAL_ENTITY) {
    return goal->entity.ent;
  } else if (goal->type == AI_GOAL_PATH) {
    return goal->path.pathTarget;
  }

  return NULL;
}

/**
 * @brief Paths to the chosen item, unless the bot already goes to an item of at least
 * `1 / AI_SWITCH_SCALE` of its utility, so that a bot in a fight or a retreat does not change its
 * item at each review.
 * @param maxCost The most travel cost of the item that the bot goes to, or 0 for no limit.
 * @return True if the bot goes to an item.
 */
static bool G_Ai_GoToItem(GameClient *cl, const float *costs, const GameAiChoice *choice, const float maxCost, const bool retreat) {

  const GameEntity *current = G_Ai_MoveEntity(cl);

  if (current && current->item) {
    const GameAiNodeId node = G_Ai_ItemNode(current);
    float cost = node == AI_NODE_INVALID ? FLT_MAX : costs[node];

    if (maxCost > 0.f && cost > maxCost) {
      cost = FLT_MAX;
    }

    const float value = cost == FLT_MAX ? 0.f : G_Ai_ItemValue(cl, current, G_Ai_TravelMillis(cost), retreat);

    if (value > 0.f && (current == choice->ent || choice->utility <= G_Ai_Utility(value, G_Ai_ItemCost(cl, current, cost)) * AI_SWITCH_SCALE)) {
      return true;
    }
  }

  return choice->ent && G_Ai_PathTo(cl, choice->node, choice->utility, choice->ent);
}

/**
 * @see g_ai_state.h
 */
const char *G_Ai_StateName(const GameClient *cl) {
  return stateNames[cl->ai->state];
}

/**
 * @see g_ai_state.h
 */
const GameEntity *G_Ai_GoalEntity(const GameClient *cl) {

  const int32_t number = cl->ai->goalNumber;

  if (number < 0 || number >= sv_maxEntities->integer) {
    return NULL;
  }

  const GameEntity *ent = ge.entities[number];

  if (!ent->inUse || !ent->item || ent->s.spawnId != cl->ai->goalSpawnId) {
    return NULL;
  }

  return ent;
}

/**
 * @brief Sets the bot's goal: the item to collect and its node, or a roam point with no item.
 */
static void G_Ai_SetGoal(GameClient *cl, const GameEntity *ent, const GameAiNodeId node) {

  cl->ai->goalNumber = ent ? ent->s.number : -1;
  cl->ai->goalSpawnId = ent ? ent->s.spawnId : 0;
  cl->ai->goalNode = node;
}

/**
 * @brief Enters the state, and clears the move target so that the state sets its own.
 */
static void G_Ai_Enter(GameClient *cl, const GameAiStateId state) {

  GameAi *ai = cl->ai;

  if (ai->state == state) {
    return;
  }

  G_Ai_Debug("%s: %s -> %s\n", cl->persistent.netName, stateNames[ai->state], stateNames[state]);

  ai->state = state;
  ai->stateTime = gameLevel.time;
  ai->reviewTime = 0;
  ai->statePathed = false;
  ai->shootTarget = NULL;

  G_Ai_ClearGoal(&ai->moveTarget);
}

/**
 * @brief Returns the bot to its goal: the item that it collects, or its roam point.
 */
static void G_Ai_Resume(GameClient *cl) {
  G_Ai_Enter(cl, G_Ai_GoalEntity(cl) ? AI_STATE_COLLECT : AI_STATE_ROAM);
}

/**
 * @see g_ai_state.h
 */
bool G_Ai_IsWaiting(const GameClient *cl) {

  const GameEntity *goal = G_Ai_GoalEntity(cl);

  return goal && goal->solid != SOLID_TRIGGER && Vec3_Distance(cl->entity->s.origin, goal->s.origin) < AI_ROAM_REACHED;
}

/**
 * @return True if the state should review its choices now: the bot has no move target, or the
 * review interval passed.
 */
static bool G_Ai_ShouldReview(GameClient *cl) {

  GameAi *ai = cl->ai;

  if (ai->reviewTime > gameLevel.time && (ai->moveTarget.type != AI_GOAL_NONE || G_Ai_IsWaiting(cl))) {
    return false;
  }

  ai->reviewTime = gameLevel.time + AI_REVIEW_MILLIS;
  return true;
}

/**
 * @brief Adds the bot's position to its trail, each `AI_TRAIL_MILLIS`. A trail from before the
 * level time began again is from the last level, and is cleared, because the game keeps a bot's
 * `GameAi` across a level change.
 */
static void G_Ai_UpdateTrail(GameClient *cl) {

  GameAi *ai = cl->ai;

  if (ai->trailTime > gameLevel.time) {
    ai->trailCount = 0;
  }

  if (ai->trailCount && gameLevel.time - ai->trailTime < AI_TRAIL_MILLIS) {
    return;
  }

  ai->trail[ai->trailCount % AI_TRAIL] = cl->entity->s.origin;
  ai->trailCount++;
  ai->trailTime = gameLevel.time;
}

/**
 * @return The distance from `position` to the nearest position of the bot's trail, or to the bot.
 */
static float G_Ai_TrailDistance(const GameClient *cl, const Vec3 position) {

  const GameAi *ai = cl->ai;
  float distance = Vec3_Distance(position, cl->entity->s.origin);

  for (size_t i = 0; i < Minz(ai->trailCount, AI_TRAIL); i++) {
    distance = Minf(distance, Vec3_Distance(position, ai->trail[i]));
  }

  return distance;
}

/**
 * @return Of `AI_ROAM_SAMPLES` random nodes that the bot can reach, the one at least `AI_ROAM_COST`
 * away that lies farthest from the bot's trail, so that it goes somewhere that it did not visit
 * recently. If none is that far, the farthest of those sampled.
 */
static GameAiNodeId G_Ai_RoamNode(const GameClient *cl, const float *costs) {

  const uint32_t count = G_Ai_Node_Count();

  GameAiNodeId best = AI_NODE_INVALID;
  float bestCost = 0.f;

  GameAiNodeId fresh = AI_NODE_INVALID;
  float freshDistance = -1.f;

  for (int32_t i = 0; i < AI_ROAM_SAMPLES; i++) {
    const GameAiNodeId node = (GameAiNodeId) RandomRangeu(0, count);

    if (costs[node] == FLT_MAX) {
      continue;
    }

    if (costs[node] >= AI_ROAM_COST) {
      const float distance = G_Ai_TrailDistance(cl, G_Ai_Node_GetPosition(node));
      if (distance > freshDistance) {
        fresh = node;
        freshDistance = distance;
      }
      continue;
    }

    if (costs[node] > bestCost) {
      best = node;
      bestCost = costs[node];
    }
  }

  return fresh != AI_NODE_INVALID ? fresh : best;
}

/**
 * @brief Chooses a detour: the item whose value per added travel time is greatest, among those that
 * add at most `AI_DETOUR_COST` to the route to the goal. The added time is the travel to the item
 * and on from it to the goal, less the travel straight to the goal, so an item on the route costs
 * little, and an item behind the bot costs much.
 * @return True if the bot takes a detour.
 */
static bool G_Ai_ChooseDetour(GameClient *cl, const float *costs) {

  const GameAiNodeId goalNode = cl->ai->goalNode;

  if (goalNode == AI_NODE_INVALID || goalNode >= G_Ai_Node_Count() || costs[goalNode] == FLT_MAX) {
    return false;
  }

  const float direct = costs[goalNode];
  const GameEntity *goal = G_Ai_GoalEntity(cl);

  GameAiChoice candidates[AI_DETOUR_CANDIDATES] = { 0 };

  G_ForEachEntity(ent, {
    if (!ent->item || ent == goal) {
      continue;
    }

    const GameAiNodeId node = G_Ai_ItemNode(ent);

    if (node == AI_NODE_INVALID || node == goalNode || costs[node] == FLT_MAX || costs[node] > direct + AI_DETOUR_COST) {
      continue;
    }

    const float value = G_Ai_ItemValue(cl, ent, G_Ai_TravelMillis(costs[node]), false);

    if (value <= 0.f) {
      continue;
    }

    for (int32_t i = 0; i < AI_DETOUR_CANDIDATES; i++) {
      if (value > candidates[i].value) {
        memmove(&candidates[i + 1], &candidates[i], sizeof(GameAiChoice) * (size_t) (AI_DETOUR_CANDIDATES - i - 1));
        candidates[i].ent = ent;
        candidates[i].node = node;
        candidates[i].value = value;
        candidates[i].utility = 0.f;
        break;
      }
    }
  });

  GameAiChoice best = { .node = AI_NODE_INVALID };

  for (int32_t i = 0; i < AI_DETOUR_CANDIDATES && candidates[i].ent; i++) {
    GameAiChoice *choice = &candidates[i];

    float onward;
    Vector *path = G_Ai_Node_FindPath(cl, choice->node, goalNode, G_Ai_Node_Heuristic, direct + AI_DETOUR_COST, &onward);

    if (!path) {
      continue;
    }

    release(path);

    const float added = costs[choice->node] + onward - direct;

    if (added > AI_DETOUR_COST) {
      continue;
    }

    choice->utility = G_Ai_Utility(choice->value, G_Ai_ItemCost(cl, choice->ent, Maxf(added, 0.f)));

    if (choice->utility > best.utility) {
      best = *choice;
    }
  }

  if (!best.ent) {
    return false;
  }

  G_Ai_Enter(cl, AI_STATE_DETOUR);

  if (!G_Ai_PathTo(cl, best.node, best.utility, best.ent)) {
    G_Ai_Resume(cl);
    return false;
  }

  G_Ai_Avoid(cl, best.ent);
  return true;
}

/**
 * @return True if the bot chooses to pursue an enemy that it lost, or an alert. Aggressive bots
 * pursue more often, and no bot pursues while it should retreat.
 */
static bool G_Ai_WantsPursuit(const GameClient *cl) {

  if (!G_Ai_IsArmed(cl) || G_Ai_ShouldRetreat(cl)) {
    return false;
  }

  return Randomf() < Mixf(AI_PURSUE_CHANCE_MIN, AI_PURSUE_CHANCE_MAX, cl->ai->personality.aggression);
}

/**
 * @return True if the bot, which has a combat target, should retreat from it: if it is unarmed, for
 * health, or from an enemy that outguns it. A bot that retreats fights again only once its health
 * exceeds its retreat health by `AI_RECOVER_SCALE`, so that it does not switch at the threshold.
 * @remarks An unarmed bot only defends itself with the Blaster: it retreats to the weapons, armor
 * and health of most utility, and fires back on the way.
 */
static bool G_Ai_WantsRetreat(const GameClient *cl) {

  if (!G_Ai_IsArmed(cl)) {
    return true;
  }

  const float scale = cl->ai->state == AI_STATE_RETREAT ? AI_RECOVER_SCALE : 1.f;

  if (cl->entity->health < G_Ai_RetreatHealth(cl) * scale) {
    return true;
  }

  return G_Ai_IsOutgunned(cl, cl->ai->combatTarget.entity.ent);
}

/**
 * @return True if the bot heard an alert that it has not yet considered, and chose to investigate.
 */
static bool G_Ai_ConsiderAlert(GameClient *cl) {

  if (!G_Ai_IsAlerted(cl) || cl->ai->alertInvestigated) {
    return false;
  }

  cl->ai->alertInvestigated = true;

  if (!G_Ai_WantsPursuit(cl)) {
    return false;
  }

  G_Ai_Enter(cl, AI_STATE_INVESTIGATE);
  return true;
}

/**
 * @brief Collects the item of most value per travel time, roams when no item has value, and
 * takes detours on the way. The bot changes its item only for one of `AI_SWITCH_SCALE` times more
 * utility. After a path that ended in distress, the bot avoids the goal that it failed to reach,
 * and chooses another at once. It waits at an item for its respawn for at most `AI_WAIT_MILLIS`.
 */
static void G_Ai_State_Navigate(GameClient *cl) {

  GameAi *ai = cl->ai;

  if (G_Ai_ConsiderAlert(cl)) {
    return;
  }

  if (ai->moveTarget.type == AI_GOAL_NONE && ai->reacquireTime > gameLevel.time) {
    const GameEntity *goal = G_Ai_GoalEntity(cl);

    if (goal) {
      G_Ai_Avoid(cl, goal);
    }

    G_Ai_SetGoal(cl, NULL, AI_NODE_INVALID);
    ai->reacquireTime = 0;
  }

  if (!G_Ai_IsWaiting(cl)) {
    ai->waitTime = 0;
  } else if (!ai->waitTime) {
    ai->waitTime = gameLevel.time;
  }

  if (!G_Ai_ShouldReview(cl)) {
    return;
  }

  const float *costs = G_Ai_TravelCosts(cl);

  if (!costs) {
    return;
  }

  const GameEntity *goal = G_Ai_GoalEntity(cl);
  float goalUtility = 0.f;

  if (goal) {
    const float cost = ai->goalNode < G_Ai_Node_Count() ? costs[ai->goalNode] : FLT_MAX;
    const float value = cost == FLT_MAX ? 0.f : G_Ai_ItemValue(cl, goal, G_Ai_TravelMillis(cost), false);

    if (value > 0.f) {
      goalUtility = G_Ai_Utility(value, G_Ai_ItemCost(cl, goal, cost));
    } else {
      G_Ai_SetGoal(cl, NULL, AI_NODE_INVALID);
      G_Ai_ClearGoal(&ai->moveTarget);
      goal = NULL;
    }
  }

  const GameAiChoice item = G_Ai_ChooseItem(cl, costs, 0.f, false, true);

  if (item.ent && item.ent != goal && (!goal || item.utility > goalUtility * AI_SWITCH_SCALE)) {

    if (item.access != -1) {
      G_Ai_Enter(cl, AI_STATE_ACTIVATE);
      ai->access = item.access;
      ai->accessButton = item.button;
      ai->accessNode = item.buttonNode;
      ai->accessRestTime = 0;
      G_Ai_SetGoal(cl, item.ent, item.node);

      if (G_Ai_PathTo(cl, item.buttonNode, item.utility, NULL)) {
        return;
      }

      G_Ai_Avoid(cl, item.ent);
      G_Ai_SetGoal(cl, NULL, AI_NODE_INVALID);
      G_Ai_Enter(cl, AI_STATE_ROAM);
      return;
    }

    G_Ai_Enter(cl, AI_STATE_COLLECT);
    G_Ai_ClearGoal(&ai->moveTarget);

    if (G_Ai_PathTo(cl, item.node, item.utility, item.ent)) {
      G_Ai_SetGoal(cl, item.ent, item.node);
      return;
    }

    G_Ai_Avoid(cl, item.ent);
  }

  if (ai->moveTarget.type == AI_GOAL_NONE) {

    if (goal && G_Ai_IsWaiting(cl)) {

      if (gameLevel.time - ai->waitTime < AI_WAIT_MILLIS) {
        return;
      }

      G_Ai_Avoid(cl, goal);
      G_Ai_SetGoal(cl, NULL, AI_NODE_INVALID);
      goal = NULL;
    }

    if (goal) {
      if (!G_Ai_PathTo(cl, ai->goalNode, goalUtility, goal)) {
        G_Ai_Avoid(cl, goal);
        G_Ai_SetGoal(cl, NULL, AI_NODE_INVALID);
      }
      return;
    }

    G_Ai_Enter(cl, AI_STATE_ROAM);

    GameAiNodeId node = ai->goalNode;

    if (node == AI_NODE_INVALID || node >= G_Ai_Node_Count() ||
        Vec3_Distance(cl->entity->s.origin, G_Ai_Node_GetPosition(node)) < AI_ROAM_REACHED) {
      node = G_Ai_RoamNode(cl, costs);
    }

    if (G_Ai_PathTo(cl, node, AI_ROAM_PRIORITY, NULL)) {
      G_Ai_SetGoal(cl, NULL, node);
    } else {
      G_Ai_SetGoal(cl, NULL, AI_NODE_INVALID);
    }
    return;
  }

  G_Ai_ChooseDetour(cl, costs);
}

/**
 * @brief Takes the detour item, then resumes the goal. A detour that fails does not count against
 * the goal: the bot avoids the detour item from the start of the detour.
 */
static void G_Ai_State_Detour(GameClient *cl) {

  GameAi *ai = cl->ai;

  if (G_Ai_ConsiderAlert(cl)) {
    return;
  }

  const GameEntity *item = G_Ai_MoveEntity(cl);

  if (!item || !item->item || item->solid != SOLID_TRIGGER || gameLevel.time - ai->stateTime > AI_DETOUR_MILLIS) {
    ai->reacquireTime = 0;
    G_Ai_Resume(cl);
  }
}

/**
 * @return The trap whose door the entity stands on, or NULL.
 */
static const GameAiTrap *G_Ai_TrapUnder(const GameEntity *ent) {

  if (!ent->ground.ent) {
    return NULL;
  }

  for (int32_t i = 0; i < module.numTraps; i++) {
    const GameAiTrap *trap = &module.traps[i];

    for (int32_t j = 0; j < trap->numDoors; j++) {
      if (trap->doors[j] == ent->ground.ent) {
        return trap;
      }
    }
  }

  return NULL;
}

/**
 * @brief Walks into the nearest button of the trap under the enemy, if the bot is within
 * `AI_TRAP_REACH` of it, with a clear line for its box, raised by a step, the bot does not stand on
 * the trap itself, and the trap's doors are at rest, unsprung.
 * @return True if the bot goes to spring the trap.
 */
static bool G_Ai_SpringTrap(GameClient *cl, const GameEntity *enemy) {

  const GameAiTrap *trap = G_Ai_TrapUnder(enemy);

  if (!trap || G_Ai_TrapUnder(cl->entity) == trap) {
    return false;
  }

  for (int32_t i = 0; i < trap->numDoors; i++) {
    if (trap->doors[i]->moveInfo.state != MOVE_STATE_BOTTOM) {
      return false;
    }
  }

  const GameEntity *nearest = NULL;
  float nearestDistance = AI_TRAP_REACH;

  Box3 bounds = G_PlayerBounds();
  bounds.mins.z += PM_STEP_HEIGHT;

  for (int32_t i = 0; i < trap->numButtons; i++) {
    const Vec3 center = Box3_Center(trap->buttons[i]->absBounds);
    const float distance = Vec3_Distance(cl->entity->s.origin, center);

    if (distance >= nearestDistance) {
      continue;
    }

    const CollisionTrace tr = gi.Trace(cl->entity->s.origin, center, bounds, cl->entity, CONTENTS_MASK_CLIP_PLAYER);
    if (tr.fraction < 1.f && tr.ent != trap->buttons[i]) {
      continue;
    }

    nearest = trap->buttons[i];
    nearestDistance = distance;
  }

  if (!nearest) {
    return false;
  }

  G_Ai_Debug("%s springs a trap under %s\n", cl->persistent.netName, enemy->client ? enemy->client->persistent.netName : "?");
  G_Ai_SetPositionGoal(cl, &cl->ai->moveTarget, AI_PURSUE_PRIORITY, Box3_Center(nearest->absBounds));
  return true;
}

/**
 * @brief Fights the combat target, as only an armed bot does. Within `AI_ENGAGE_SCALE` of the
 * weapon's preferred range, the bot strafes around the enemy, and goes only for items close by.
 * Beyond it, the bot collects items while it fires, or, if it has none to collect, paths toward the
 * enemy.
 */
static void G_Ai_State_Fight(GameClient *cl) {

  GameAi *ai = cl->ai;
  const GameEntity *enemy = ai->combatTarget.entity.ent;

  if (!G_Ai_ShouldReview(cl)) {
    return;
  }

  if (G_Ai_SpringTrap(cl, enemy)) {
    return;
  }

  const float *costs = G_Ai_TravelCosts(cl);
  const bool strafing = ai->moveTarget.type == AI_GOAL_ENTITY && G_Ai_GoalHasEntity(&ai->moveTarget, enemy);
  const float scale = strafing ? AI_DISENGAGE_SCALE : AI_ENGAGE_SCALE;
  const bool engaged = Vec3_Distance(cl->entity->s.origin, enemy->s.origin) <= G_Ai_PreferredRange(cl) * scale;

  if (costs) {
    const float maxCost = engaged ? AI_FIGHT_ITEM_COST : 0.f;
    const GameAiChoice item = G_Ai_ChooseItem(cl, costs, maxCost, false, false);

    if (G_Ai_GoToItem(cl, costs, &item, maxCost, false)) {
      return;
    }
  }

  if (!engaged) {
    const GameAiNodeId node = G_Ai_Node_FindClosest(enemy->s.origin, AI_NODE_DISTANCE, true, true);

    if (G_Ai_PathTo(cl, node, AI_PURSUE_PRIORITY, enemy)) {
      return;
    }
  }

  if (ai->moveTarget.type != AI_GOAL_ENTITY || !G_Ai_GoalHasEntity(&ai->moveTarget, enemy)) {
    G_Ai_CopyGoal(&ai->combatTarget, &ai->moveTarget);
  }
}

/**
 * @brief Goes to the health, armor or weapon of most utility that does not lie toward the enemy,
 * while the aim stays on the enemy so that the bot fires back. An unarmed bot with none of those to
 * go to takes any item of value, because its strafe around an enemy far away moves toward it. With
 * none to go to, the bot strafes around the enemy and fights.
 */
static void G_Ai_State_Retreat(GameClient *cl) {

  GameAi *ai = cl->ai;
  const GameEntity *enemy = ai->combatTarget.entity.ent;

  if (!G_Ai_ShouldReview(cl)) {
    return;
  }

  const float *costs = G_Ai_TravelCosts(cl);

  if (costs) {
    const GameAiChoice item = G_Ai_ChooseItem(cl, costs, 0.f, true, false);

    if (G_Ai_GoToItem(cl, costs, &item, 0.f, true)) {
      return;
    }

    if (!G_Ai_IsArmed(cl)) {
      const GameAiChoice any = G_Ai_ChooseItem(cl, costs, 0.f, false, false);

      if (G_Ai_GoToItem(cl, costs, &any, 0.f, false)) {
        return;
      }
    }
  }

  if (ai->moveTarget.type != AI_GOAL_ENTITY || !G_Ai_GoalHasEntity(&ai->moveTarget, enemy)) {
    G_Ai_CopyGoal(&ai->combatTarget, &ai->moveTarget);
    ai->moveTarget.entity.combatType = AI_COMBAT_FLANK;
  }
}

/**
 * @return True if a door of the access is activated, so that the way to its item opens.
 */
static bool G_Ai_IsAccessOpen(const GameAiAccess *access) {

  for (int32_t i = 0; i < access->numDoors; i++) {
    if (G_func_door_IsActivated(access->doors[i])) {
      return true;
    }
  }

  return false;
}

/**
 * @return True if every activated door of the access has come to rest.
 */
static bool G_Ai_IsAccessAtRest(const GameAiAccess *access) {

  for (int32_t i = 0; i < access->numDoors; i++) {
    if (access->doors[i]->moveInfo.state == MOVE_STATE_GOING_UP) {
      return false;
    }
  }

  return true;
}

/**
 * @brief Marks the access as failed, so that no bot tries it for a time that doubles with each
 * failure, and returns the bot to roam, away from the item.
 */
static void G_Ai_FailAccess(GameClient *cl, GameAiAccess *access, const GameEntity *item) {

  access->failTime = gameLevel.time;
  access->numFails++;

  G_Ai_Avoid(cl, item);
  G_Ai_SetGoal(cl, NULL, AI_NODE_INVALID);
  G_Ai_Enter(cl, AI_STATE_ROAM);
}

/**
 * @return True if the button's center is within `reach` of the eye at `height` above `origin`, and
 * nothing but the button is in the way.
 */
static bool G_Ai_SeesButton(const Vec3 origin, const float height, const GameEntity *button, const float reach) {

  const Vec3 eye = Vec3_Add(origin, MakeVec3(0.f, 0.f, height));
  const Vec3 center = Box3_Center(button->absBounds);

  if (Vec3_Distance(eye, center) > reach) {
    return false;
  }

  const CollisionTrace tr = gi.Trace(eye, center, Box3_Zero(), NULL, CONTENTS_MASK_SOLID);

  return tr.fraction == 1.f || tr.ent == button;
}

/**
 * @return True if a shot presses the button, as a button with `health` is. Its health stays set
 * while it is pressed, and it takes no damage.
 */
static bool G_Ai_IsShotButton(const GameEntity *button) {
  return button->maxHealth > 0;
}

/**
 * @return True if the bot stands where it presses the button: within `AI_ACTIVATE_REACH` of a button
 * that it touches, or where it sees, within `AI_SHOOT_REACH`, a button that a shot presses.
 */
static bool G_Ai_IsAtButton(const GameClient *cl, const GameEntity *button) {

  if (G_Ai_IsShotButton(button)) {
    return G_Ai_SeesButton(cl->entity->s.origin, cl->ps.pmState.viewOffset.z, button, AI_SHOOT_REACH);
  }

  return Vec3_Distance(cl->entity->s.origin, Box3_Center(button->absBounds)) < AI_ACTIVATE_REACH;
}

/**
 * @brief Goes to the button of the goal item's access, and within `AI_ACTIVATE_REACH` walks into it,
 * or, for a button that a shot presses, shoots it once it sees it within `AI_SHOOT_REACH`.
 * Once a door of the access moves, and a path to the item exists, the bot collects the item. If no
 * path exists `AI_ACCESS_PATH_MILLIS` after the doors come to rest while the bot is at its button,
 * or the bot does not press the button within `AI_ACTIVATE_MILLIS`, the access fails. The bot gives up, and avoids the item, if it
 * finds no path back to the button's node.
 */
static void G_Ai_State_Activate(GameClient *cl) {

  GameAi *ai = cl->ai;

  if (G_Ai_ConsiderAlert(cl)) {
    return;
  }

  const GameEntity *item = G_Ai_GoalEntity(cl);

  if (!item || ai->access < 0 || ai->access >= module.numAccess || module.access[ai->access].ent != item ||
      ai->accessButton < 0 || ai->accessButton >= module.access[ai->access].numButtons) {
    G_Ai_SetGoal(cl, NULL, AI_NODE_INVALID);
    G_Ai_Enter(cl, AI_STATE_ROAM);
    return;
  }

  GameAiAccess *access = &module.access[ai->access];

  if (gameLevel.time - ai->stateTime > AI_ACTIVATE_MILLIS) {
    G_Ai_FailAccess(cl, access, item);
    return;
  }

  const GameEntity *target = access->buttons[ai->accessButton];
  const Vec3 button = Box3_Center(target->absBounds);

  if (G_Ai_IsAccessOpen(access)) {
    ai->shootTarget = NULL;

    if (!G_Ai_ShouldReview(cl)) {
      return;
    }

    const float *costs = G_Ai_TravelCosts(cl);

    if (costs && ai->goalNode < G_Ai_Node_Count() && costs[ai->goalNode] != FLT_MAX) {
      G_Ai_Enter(cl, AI_STATE_COLLECT);
      G_Ai_PathTo(cl, ai->goalNode, AI_PURSUE_PRIORITY, item);
      return;
    }

    if (!G_Ai_IsAccessAtRest(access) || !G_Ai_IsAtButton(cl, target)) {
      ai->accessRestTime = 0;
    } else if (!ai->accessRestTime) {
      ai->accessRestTime = gameLevel.time;
    } else if (gameLevel.time - ai->accessRestTime > AI_ACCESS_PATH_MILLIS) {
      G_Ai_FailAccess(cl, access, item);
      return;
    }
  } else if (G_Ai_IsShotButton(target)) {
    ai->shootTarget = G_Ai_SeesButton(cl->entity->s.origin, cl->ps.pmState.viewOffset.z, target, AI_SHOOT_REACH) ? target : NULL;
  } else if (Vec3_Distance(cl->entity->s.origin, button) < AI_ACTIVATE_REACH) {
    G_Ai_SetPositionGoal(cl, &ai->moveTarget, AI_PURSUE_PRIORITY, button);
    return;
  }

  if (ai->moveTarget.type == AI_GOAL_NONE && !G_Ai_IsAtButton(cl, target) &&
      !G_Ai_PathTo(cl, ai->accessNode, AI_PURSUE_PRIORITY, NULL)) {
    G_Ai_Avoid(cl, item);
    G_Ai_SetGoal(cl, NULL, AI_NODE_INVALID);
    G_Ai_Enter(cl, AI_STATE_ROAM);
  }
}

/**
 * @brief Goes to the position, once, then resumes the goal. Gives up after `AI_PURSUE_MILLIS`.
 */
static void G_Ai_State_Pursue(GameClient *cl, const Vec3 position) {

  GameAi *ai = cl->ai;

  if (gameLevel.time - ai->stateTime > AI_PURSUE_MILLIS) {
    G_Ai_Resume(cl);
    return;
  }

  if (ai->moveTarget.type != AI_GOAL_NONE) {
    return;
  }

  if (ai->statePathed) {
    G_Ai_Resume(cl);
    return;
  }

  ai->statePathed = true;

  const GameAiNodeId node = G_Ai_Node_FindClosest(position, AI_NODE_DISTANCE, false, true);

  if (!G_Ai_PathTo(cl, node, AI_PURSUE_PRIORITY, NULL)) {
    G_Ai_Resume(cl);
  }
}

/**
 * @return True if the bot follows a path and is in the air, out of water and off a ladder. A new
 * path chosen in the air starts at the node nearest the bot in flight, which is often beside or
 * behind where it lands, and the bot turns toward it in the air, off the edge of a walkway.
 */
static bool G_Ai_IsAirborne(const GameClient *cl) {

  const GameEntity *ent = cl->entity;

  return cl->ai->moveTarget.type == AI_GOAL_PATH && !ent->ground.ent && ent->waterLevel < WATER_WAIST &&
         !(cl->ps.pmState.flags & PMF_ON_LADDER);
}

/**
 * @see g_ai_state.h
 */
uint32_t G_Ai_State(GameClient *cl, PMoveCmd *cmd) {

  if (G_IsMeat(cl->entity) && cl->entity->dead) {
    return 1;
  }

  GameAi *ai = cl->ai;

  G_Ai_UpdateTrail(cl);

  if (G_Ai_IsAirborne(cl)) {
    return 1;
  }

  if (ai->combatTarget.type == AI_GOAL_ENTITY) {
    G_Ai_Enter(cl, G_Ai_WantsRetreat(cl) ? AI_STATE_RETREAT : AI_STATE_FIGHT);
  } else if (ai->state == AI_STATE_FIGHT || ai->state == AI_STATE_RETREAT) {
    if (ai->combatTargetLost && G_Ai_WantsPursuit(cl)) {
      G_Ai_Enter(cl, AI_STATE_CHASE);
    } else {
      G_Ai_Resume(cl);
    }
  }

  switch (ai->state) {
    case AI_STATE_COLLECT:
    case AI_STATE_ROAM:
      G_Ai_State_Navigate(cl);
      break;
    case AI_STATE_DETOUR:
      G_Ai_State_Detour(cl);
      break;
    case AI_STATE_FIGHT:
      G_Ai_State_Fight(cl);
      break;
    case AI_STATE_CHASE:
      G_Ai_State_Pursue(cl, ai->combatTargetSeenPosition);
      break;
    case AI_STATE_RETREAT:
      G_Ai_State_Retreat(cl);
      break;
    case AI_STATE_INVESTIGATE:
      G_Ai_State_Pursue(cl, ai->alertPosition);
      break;
    case AI_STATE_ACTIVATE:
      G_Ai_State_Activate(cl);
      break;
    default:
      G_Error("Invalid state %d\n", ai->state);
  }

  return AI_STATE_MILLIS;
}

/**
 * @see g_ai_state.h
 */
void G_Ai_State_Reset(GameClient *cl) {

  GameAi *ai = cl->ai;

  ai->state = AI_STATE_ROAM;
  ai->stateTime = gameLevel.time;
  ai->reviewTime = 0;
  ai->statePathed = false;
  ai->combatTargetLost = false;
  ai->access = ai->accessButton = -1;
  ai->accessNode = AI_NODE_INVALID;
  ai->shootTarget = NULL;

  G_Ai_SetGoal(cl, NULL, AI_NODE_INVALID);
}

/**
 * @return True if the entity is a door that `ai_access_by` MAY name.
 */
static bool G_Ai_IsAccessDoor(const GameEntity *ent) {
  return ent->classname && (!Str_Compare(ent->classname, "func_door") || !Str_Compare(ent->classname, "func_door_rotating"));
}

/**
 * @return The time in milliseconds for which the door stays activated after a press, or
 * `UINT32_MAX` if it does not move back.
 */
static uint32_t G_Ai_DoorOpenMillis(const GameEntity *door) {

  if (door->moveInfo.wait < 0.f) {
    return UINT32_MAX;
  }

  const float travel = door->moveInfo.speed > 0.f ? door->moveInfo.distance / door->moveInfo.speed : 0.f;

  return (uint32_t) ((door->moveInfo.wait + travel) * 1000.f);
}

/**
 * @brief Finds the nodes, up to `AI_BUTTON_NODES`, nearest the button first, from which a bot
 * presses it: within `AI_ACTIVATE_REACH` of the button's center, which sees it and which is within
 * the height of the player's box at the node, to walk into it, or, for a button that a shot presses,
 * with an eye that sees it within `AI_SHOOT_REACH`.
 */
static void G_Ai_ButtonNodes(const GameEntity *button, GameAiNodeId *nodes, int32_t *numNodes) {

  const Vec3 center = Box3_Center(button->absBounds);
  float distances[AI_BUTTON_NODES];

  *numNodes = 0;

  for (uint32_t i = 0; i < G_Ai_Node_Count(); i++) {
    const Vec3 position = G_Ai_Node_GetPosition((GameAiNodeId) i);
    const float distance = Vec3_Distance(position, center);

    if (G_Ai_IsShotButton(button)) {
      if (!G_Ai_SeesButton(position, AI_SHOOT_EYE_HEIGHT, button, AI_SHOOT_REACH)) {
        continue;
      }
    } else {
      if (distance > AI_ACTIVATE_REACH) {
        continue;
      }

      if (center.z < position.z + PM_BOUNDS.mins.z || center.z > position.z + PM_BOUNDS.maxs.z) {
        continue;
      }

      if (!G_Ai_SeesButton(position, 0.f, button, AI_ACTIVATE_REACH)) {
        continue;
      }
    }

    int32_t j = *numNodes;
    if (j == AI_BUTTON_NODES) {
      if (distance >= distances[j - 1]) {
        continue;
      }
      j--;
    } else {
      (*numNodes)++;
    }

    while (j > 0 && distances[j - 1] > distance) {
      nodes[j] = nodes[j - 1];
      distances[j] = distances[j - 1];
      j--;
    }

    nodes[j] = (GameAiNodeId) i;
    distances[j] = distance;
  }
}

/**
 * @brief Finds the doors that a map marks with `"ai_hint" "trap"`, grouped by `targetname`, and the
 * buttons that a bot touches to move them.
 */
static void G_Ai_LoadTraps(void) {

  module.numTraps = 0;

  G_ForEachEntity(ent, {
    if (!G_Ai_IsAccessDoor(ent) || !ent->targetName || Str_Compare(gi.EntityValue(ent->def, "ai_hint")->string, "trap")) {
      continue;
    }

    GameAiTrap *trap = NULL;

    for (int32_t j = 0; j < module.numTraps && !trap; j++) {
      if (!Str_Compare(module.traps[j].doors[0]->targetName, ent->targetName)) {
        trap = &module.traps[j];
      }
    }

    if (!trap) {
      if (module.numTraps == AI_MAX_TRAPS) {
        G_Warn("More than %d traps have \"ai_hint\" \"trap\"\n", AI_MAX_TRAPS);
        break;
      }

      trap = &module.traps[module.numTraps++];
      memset(trap, 0, sizeof(*trap));

      for (int32_t j = 0; j < sv_maxEntities->integer; j++) {
        const GameEntity *other = ge.entities[j];

        if (other->inUse && other->target && !Str_Compare(other->target, ent->targetName) && other->Touch &&
            !Str_Compare(other->classname, "func_button") && trap->numButtons < AI_TRAP_BUTTONS) {
          trap->buttons[trap->numButtons++] = other;
        }
      }
    }

    if (trap->numDoors < AI_TRAP_DOORS) {
      trap->doors[trap->numDoors++] = ent;
    }
  });
}

/**
 * @see g_ai_state.h
 */
void G_Ai_State_Load(void) {

  G_Ai_LoadTraps();

  module.numAccess = 0;

  G_ForEachEntity(ent, {
    const char *name = gi.EntityValue(ent->def, "ai_access_by")->string;

    if (!*name) {
      continue;
    }

    if (!ent->item) {
      G_Warn("%s at %s has ai_access_by, which only items use\n", ent->classname, vtos(ent->s.origin));
      continue;
    }

    if (module.numAccess == AI_MAX_ACCESS) {
      G_Warn("More than %d items have ai_access_by\n", AI_MAX_ACCESS);
      break;
    }

    GameAiAccess *access = &module.access[module.numAccess];
    memset(access, 0, sizeof(*access));

    access->ent = ent;
    access->openMillis = UINT32_MAX;

    for (int32_t j = 0; j < sv_maxEntities->integer; j++) {
      const GameEntity *other = ge.entities[j];

      if (!other->inUse) {
        continue;
      }

      if (other->targetName && !Str_Compare(other->targetName, name) && G_Ai_IsAccessDoor(other) &&
          access->numDoors < AI_ACCESS_DOORS) {
        access->doors[access->numDoors++] = other;
        const uint32_t open = G_Ai_DoorOpenMillis(other);
        if (open < access->openMillis) {
          access->openMillis = open;
        }
      }

      if (other->target && !Str_Compare(other->target, name) && (other->Touch || G_Ai_IsShotButton(other)) &&
          !Str_Compare(other->classname, "func_button") && access->numButtons < AI_ACCESS_BUTTONS) {
        G_Ai_ButtonNodes(other, access->buttonNodes[access->numButtons], &access->numButtonNodes[access->numButtons]);
        access->buttons[access->numButtons++] = other;
      }
    }

    if (!access->numDoors || !access->numButtons) {
      G_Warn("%s at %s: ai_access_by %s names %d doors, with %d buttons that a bot can press\n",
             ent->classname, vtos(ent->s.origin), name, access->numDoors, access->numButtons);
      continue;
    }

    G_Debug("%s at %s opens with %d buttons and %d doors, for %ums\n", ent->classname, vtos(ent->s.origin),
            access->numButtons, access->numDoors, access->openMillis);

    module.numAccess++;
  });
}

/**
 * @see g_ai_state.h
 */
void G_Ai_State_Unload(void) {

  module.numAccess = 0;
  module.numTraps = 0;
}

/**
 * @see g_ai_state.h
 */
void G_Ai_State_ItemTaken(const GameEntity *item) {

  for (int32_t i = 0; i < module.numAccess; i++) {
    GameAiAccess *access = &module.access[i];

    if (access->ent == item && G_Ai_IsAccessOpen(access)) {
      access->numFails = 0;
    }
  }
}

/**
 * @see g_ai_state.h
 */
void G_Ai_State_Shutdown(void) {

  free(module.costs);
  module.costs = NULL;
  module.count = 0;
}
