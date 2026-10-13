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
 * @brief The least travel cost of a roam point, so that a bot with nothing to collect crosses the
 * map.
 */
#define AI_ROAM_COST (PM_SPEED_RUN * 4.f)

/**
 * @brief The number of random nodes that a bot considers for a roam point.
 */
#define AI_ROAM_SAMPLES 32

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
 * @brief An item that a bot may go to, with its node and its utility.
 */
typedef struct {
  const GameEntity *ent;
  GameAiNodeId node;
  float value;
  float utility;
} GameAiChoice;

static const char *const stateNames[AI_STATE_TOTAL] = {
  [AI_STATE_COLLECT] = "collect",
  [AI_STATE_ROAM] = "roam",
  [AI_STATE_DETOUR] = "detour",
  [AI_STATE_FIGHT] = "fight",
  [AI_STATE_CHASE] = "chase",
  [AI_STATE_RETREAT] = "retreat",
  [AI_STATE_INVESTIGATE] = "investigate",
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
 * @see g_ai_state.h
 */
void G_Ai_Avoid(GameClient *cl, const GameEntity *item) {

  GameAiAvoid *slot = &cl->ai->avoids[0];

  for (int32_t i = 0; i < AI_AVOIDS; i++) {
    GameAiAvoid *avoid = &cl->ai->avoids[i];

    if (avoid->time && avoid->number == item->s.number) {
      slot = avoid;
      break;
    }

    if (avoid->time < slot->time) {
      slot = avoid;
    }
  }

  slot->number = item->s.number;
  slot->time = gameLevel.time;
}

/**
 * @return The utility scale of the item, which rises back to 1 as the bot's avoidance of it fades.
 */
static float G_Ai_AvoidScale(const GameClient *cl, const GameEntity *item) {

  for (int32_t i = 0; i < AI_AVOIDS; i++) {
    const GameAiAvoid *avoid = &cl->ai->avoids[i];

    if (avoid->time && avoid->number == item->s.number) {
      const uint32_t elapsed = gameLevel.time - avoid->time;

      if (elapsed < AI_AVOID_MILLIS) {
        return Mixf(AI_AVOID_SCALE, 1.f, elapsed / (float) AI_AVOID_MILLIS);
      }
    }
  }

  return 1.f;
}

/**
 * @return The value of the item to the bot, or 0 if the bot does not want it, cannot take it, or
 * would arrive before it returns.
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

  float value = item->def.priority;

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
 * @return The item of most utility, its value per travel time, that the bot can reach within
 * `maxCost`, or a choice with no entity.
 * @param maxCost The most travel cost, or 0 for no limit.
 * @param retreat True to choose only health, armor and the weapons that would arm the bot, and to
 * shun those that lie toward the enemy.
 */
static GameAiChoice G_Ai_ChooseItem(const GameClient *cl, const float *costs, const float maxCost, const bool retreat) {

  GameAiChoice best = { .node = AI_NODE_INVALID };

  const GameEntity *enemy = cl->ai->combatTarget.type == AI_GOAL_ENTITY ? cl->ai->combatTarget.entity.ent : NULL;
  const float noise = Mixf(AI_ITEM_NOISE_MAX, AI_ITEM_NOISE_MIN, cl->ai->personality.awareness);

  G_ForEachEntity(ent, {
    if (!ent->item) {
      continue;
    }

    const GameAiNodeId node = G_Ai_ItemNode(ent);

    if (node == AI_NODE_INVALID || costs[node] == FLT_MAX || (maxCost > 0.f && costs[node] > maxCost)) {
      continue;
    }

    const float value = G_Ai_ItemValue(cl, ent, G_Ai_TravelMillis(costs[node]), retreat);

    if (value <= 0.f) {
      continue;
    }

    float utility = G_Ai_Utility(value, costs[node]) * RandomRangef(1.f - noise, 1.f + noise);

    if (retreat && enemy &&
        Vec3_Distance(ent->s.origin, enemy->s.origin) < Vec3_Distance(ent->s.origin, cl->entity->s.origin)) {
      utility *= AI_TOWARD_ENEMY_SCALE;
    }

    if (utility > best.utility) {
      best.ent = ent;
      best.node = node;
      best.value = value;
      best.utility = utility;
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

    if (value > 0.f && (current == choice->ent || choice->utility <= G_Ai_Utility(value, cost) * AI_SWITCH_SCALE)) {
      return true;
    }
  }

  return choice->ent && G_Ai_PathTo(cl, choice->node, choice->utility, choice->ent);
}

/**
 * @return The item that the bot collects, or NULL if it collects none, or the item is gone.
 */
static const GameEntity *G_Ai_GoalEntity(const GameClient *cl) {

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

  G_Ai_ClearGoal(&ai->moveTarget);
}

/**
 * @brief Returns the bot to its goal: the item that it collects, or its roam point.
 */
static void G_Ai_Resume(GameClient *cl) {
  G_Ai_Enter(cl, G_Ai_GoalEntity(cl) ? AI_STATE_COLLECT : AI_STATE_ROAM);
}

/**
 * @return True if the bot collects an item that is taken, and stands where it returns, so that the
 * bot reviews its choices only at intervals while it waits.
 */
static bool G_Ai_IsWaiting(const GameClient *cl) {

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
 * @return A random node that the bot can reach, at least `AI_ROAM_COST` away if one is found, or
 * else the farthest of those sampled.
 */
static GameAiNodeId G_Ai_RoamNode(const float *costs) {

  const uint32_t count = G_Ai_Node_Count();

  GameAiNodeId best = AI_NODE_INVALID;
  float bestCost = 0.f;

  for (int32_t i = 0; i < AI_ROAM_SAMPLES; i++) {
    const GameAiNodeId node = (GameAiNodeId) RandomRangeu(0, count);

    if (costs[node] == FLT_MAX) {
      continue;
    }

    if (costs[node] >= AI_ROAM_COST) {
      return node;
    }

    if (costs[node] > bestCost) {
      best = node;
      bestCost = costs[node];
    }
  }

  return best;
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

    choice->utility = G_Ai_Utility(choice->value, Maxf(added, 0.f));

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
 * @return True if the bot, which has a combat target, should retreat from it: for health, or from an
 * enemy that outguns it. A bot that retreats fights again only once its health exceeds its retreat
 * health by `AI_RECOVER_SCALE`, so that it does not switch at the threshold.
 * @remarks An unarmed bot does not retreat from an enemy that outguns it, since nearly every enemy
 * does. It fights, and collects a weapon while it fires.
 */
static bool G_Ai_WantsRetreat(const GameClient *cl) {

  const float scale = cl->ai->state == AI_STATE_RETREAT ? AI_RECOVER_SCALE : 1.f;

  if (cl->entity->health < G_Ai_RetreatHealth(cl) * scale) {
    return true;
  }

  return G_Ai_IsArmed(cl) && G_Ai_IsOutgunned(cl, cl->ai->combatTarget.entity.ent);
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
 * utility.
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
    return;
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
      goalUtility = G_Ai_Utility(value, cost);
    } else {
      G_Ai_SetGoal(cl, NULL, AI_NODE_INVALID);
      G_Ai_ClearGoal(&ai->moveTarget);
      goal = NULL;
    }
  }

  const GameAiChoice item = G_Ai_ChooseItem(cl, costs, 0.f, false);

  if (item.ent && item.ent != goal && (!goal || item.utility > goalUtility * AI_SWITCH_SCALE)) {
    G_Ai_Enter(cl, AI_STATE_COLLECT);
    G_Ai_ClearGoal(&ai->moveTarget);

    if (G_Ai_PathTo(cl, item.node, item.utility, item.ent)) {
      G_Ai_SetGoal(cl, item.ent, item.node);
      return;
    }

    G_Ai_Avoid(cl, item.ent);
  }

  if (ai->moveTarget.type == AI_GOAL_NONE) {

    if (goal) {
      if (goal->solid != SOLID_TRIGGER && Vec3_Distance(cl->entity->s.origin, goal->s.origin) < AI_ROAM_REACHED) {
        return;
      }

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
      node = G_Ai_RoamNode(costs);
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
 * @brief Fights the combat target. Within `AI_ENGAGE_SCALE` of the weapon's preferred range, the
 * bot strafes around the enemy, and goes only for items close by. Beyond it, the bot collects items
 * while it fires, or, if it is armed and has none to collect, paths toward the enemy.
 */
static void G_Ai_State_Fight(GameClient *cl) {

  GameAi *ai = cl->ai;
  const GameEntity *enemy = ai->combatTarget.entity.ent;

  if (!G_Ai_ShouldReview(cl)) {
    return;
  }

  const float *costs = G_Ai_TravelCosts(cl);
  const bool strafing = ai->moveTarget.type == AI_GOAL_ENTITY && G_Ai_GoalHasEntity(&ai->moveTarget, enemy);
  const float scale = strafing ? AI_DISENGAGE_SCALE : AI_ENGAGE_SCALE;
  const bool engaged = Vec3_Distance(cl->entity->s.origin, enemy->s.origin) <= G_Ai_PreferredRange(cl) * scale;

  if (costs) {
    const float maxCost = engaged ? AI_FIGHT_ITEM_COST : 0.f;
    const GameAiChoice item = G_Ai_ChooseItem(cl, costs, maxCost, false);

    if (G_Ai_GoToItem(cl, costs, &item, maxCost, false)) {
      return;
    }
  }

  if (!engaged && G_Ai_IsArmed(cl)) {
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
 * while the aim stays on the enemy so that the bot fires back. With none to go to, the bot strafes
 * around the enemy and fights.
 */
static void G_Ai_State_Retreat(GameClient *cl) {

  GameAi *ai = cl->ai;
  const GameEntity *enemy = ai->combatTarget.entity.ent;

  if (!G_Ai_ShouldReview(cl)) {
    return;
  }

  const float *costs = G_Ai_TravelCosts(cl);

  if (costs) {
    const GameAiChoice item = G_Ai_ChooseItem(cl, costs, 0.f, true);

    if (G_Ai_GoToItem(cl, costs, &item, 0.f, true)) {
      return;
    }
  }

  if (ai->moveTarget.type != AI_GOAL_ENTITY || !G_Ai_GoalHasEntity(&ai->moveTarget, enemy)) {
    G_Ai_CopyGoal(&ai->combatTarget, &ai->moveTarget);
    ai->moveTarget.entity.combatType = AI_COMBAT_FLANK;
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
 * @see g_ai_state.h
 */
uint32_t G_Ai_State(GameClient *cl, PMoveCmd *cmd) {

  if (G_IsMeat(cl->entity) && cl->entity->dead) {
    return 1;
  }

  GameAi *ai = cl->ai;

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

  G_Ai_SetGoal(cl, NULL, AI_NODE_INVALID);
}

/**
 * @see g_ai_state.h
 */
void G_Ai_State_Shutdown(void) {

  free(module.costs);
  module.costs = NULL;
  module.count = 0;
}
