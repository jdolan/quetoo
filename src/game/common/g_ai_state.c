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
 * @brief The least interval in milliseconds at which a state reviews the choices of a bot that has
 * no move target and does not wait, so that a review that finds nothing does not repeat each tick.
 */
#define AI_IDLE_REVIEW_MILLIS 300

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
 * @brief The most travel cost that a detour may add to the route to the objective: about one second.
 */
#define AI_DETOUR_COST PM_SPEED_RUN

/**
 * @brief The most items for which a bot plans the route from the item to its objective.
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
 * @brief A fighting bot circle strafes around an enemy within this multiple of its preferred range. Beyond
 * it, the bot collects items while it fires, or paths toward the enemy if it has none to collect.
 */
#define AI_ENGAGE_SCALE 1.5f

/**
 * @brief A bot that circle strafes around an enemy stops only beyond this multiple of its preferred range,
 * so that an enemy near `AI_ENGAGE_SCALE` does not switch the bot at each review.
 */
#define AI_DISENGAGE_SCALE 2.f

/**
 * @brief The most travel cost of an item that a bot that circle strafes around an enemy goes for:
 * a little less than one second of running.
 */
#define AI_FIGHT_ITEM_COST (PM_SPEED_RUN * .85f)

/**
 * @brief A retreating bot fights again once its health exceeds this multiple of its retreat health.
 */
#define AI_RECOVER_SCALE 1.5f

/**
 * @brief The least time in milliseconds for which a bot retreats once it starts.
 */
#define AI_RETREAT_MILLIS 1500

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
 * @brief The most gates of a map, and the most buttons and doors of each.
 */
#define AI_MAX_GATES 32
#define AI_GATE_BUTTONS 4
#define AI_GATE_DOORS 8

/**
 * @brief The most nodes from which a bot walks to a button.
 */
#define AI_BUTTON_NODES 8

/**
 * @brief The shortest time in milliseconds between two plans of a bot in `AI_STATE_ESCAPE`.
 */
#define AI_ESCAPE_MILLIS 500

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
 * @brief The time in milliseconds after the doors of a gate come to rest activated, within which
 * a bot MUST find a path to the item.
 */
#define AI_GATE_PATH_MILLIS 1000

/**
 * @brief The time in milliseconds for which no bot tries a gate that failed once, because its
 * doors did not open a way to the item, or the bot did not press the button. Each further failure
 * doubles it, up to `AI_GATE_RETRY_DOUBLINGS` times, until a bot takes an item through the gate.
 */
#define AI_GATE_RETRY_MILLIS 60000
#define AI_GATE_RETRY_DOUBLINGS 4

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
  const char *name;
  const GameEntity *doors[AI_TRAP_DOORS];
  int32_t numDoors;
  const GameEntity *buttons[AI_TRAP_BUTTONS];
  int32_t numButtons;
} GameAiTrap;

/**
 * @brief A gate: the doors with one `targetname`, whose doorways the navigation links cross, and
 * the buttons that target them, which a bot touches or shoots to move the doors.
 */
typedef struct {
  const char *name;
  const GameEntity *doors[AI_GATE_DOORS];
  int32_t numDoors;
  const GameEntity *buttons[AI_GATE_BUTTONS];
  int32_t numButtons;

  /**
   * @brief The nodes from which a bot presses a button, and the button of each.
   */
  GameAiNodeId sources[AI_GATE_BUTTONS * AI_BUTTON_NODES];
  int32_t sourceButtons[AI_GATE_BUTTONS * AI_BUTTON_NODES];
  int32_t numSources;

  /**
   * @brief The shortest time in milliseconds for which a door stays activated after a press: its
   * travel, and its wait.
   */
  uint32_t openMillis;

  /**
   * @brief The level time at which a bot last failed to use the gate, or 0.
   */
  uint32_t failTime;

  /**
   * @brief The count of failures since a bot last took an item through the gate.
   */
  int32_t numFails;
} GameAiGate;

/**
 * @brief An item that a bot may go to, with its node and its utility.
 */
typedef struct {
  const GameEntity *ent;
  GameAiNodeId node;
  float value;
  float utility;

  /**
   * @brief The index of the gate, and of its button, if the bot reaches the item sooner through a
   * gate, or -1.
   */
  int32_t gate;
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
  [AI_STATE_ESCAPE] = "escape",
  [AI_STATE_CLIMB] = "climb",
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
   * @brief The gates of the map.
   */
  GameAiGate gates[AI_MAX_GATES];
  int32_t numGates;

  /**
   * @brief The travel costs from the bot that decides, to every node, through a gate.
   */
  GameAiGateCost *gateCosts;

  /**
   * @brief The number of costs that `gateCosts` holds.
   */
  uint32_t gateCount;

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
void G_Ai_State_Avoid(GameClient *cl, const GameEntity *item) {

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
 * @return The utility of the item to the bot, its value per travel time, at the given travel cost,
 * spread at random by `noise`, or 0 if the bot cannot reach the item or would not value it. In a
 * retreat, the utility of an item that lies nearer to the enemy than to the bot is scaled by
 * `AI_TOWARD_ENEMY_SCALE`.
 * @param cost The travel cost to the item, or `FLT_MAX` if the bot cannot reach it.
 * @param retreat True to value only health, armor and the weapons that would arm the bot, and to
 * shun those that lie toward the enemy.
 * @param noise The random spread of the utility, or 0 for none.
 * @param value If not NULL, receives the value of the item.
 */
static float G_Ai_ItemUtility(const GameClient *cl, const GameEntity *ent, const float cost, const bool retreat,
                              const float noise, float *value) {

  const float v = cost == FLT_MAX ? 0.f : G_Ai_ItemValue(cl, ent, G_Ai_TravelMillis(cost), retreat);

  if (value) {
    *value = v;
  }

  if (v <= 0.f) {
    return 0.f;
  }

  float utility = G_Ai_Utility(v, G_Ai_ItemCost(cl, ent, cost));

  if (noise > 0.f) {
    utility *= RandomRangef(1.f - noise, 1.f + noise);
  }

  const GameEntity *enemy = cl->ai->combatTarget.type == AI_GOAL_ENTITY ? cl->ai->combatTarget.entity.ent : NULL;

  if (retreat && enemy &&
      Vec3_Distance(ent->s.origin, enemy->s.origin) < Vec3_Distance(ent->s.origin, cl->entity->s.origin)) {
    utility *= AI_TOWARD_ENEMY_SCALE;
  }

  return utility;
}

/**
 * @return True if a door of the gate is activated, so that its doorways open.
 */
static bool G_Ai_IsGateOpen(const GameAiGate *gate) {

  for (int32_t i = 0; i < gate->numDoors; i++) {
    if (G_func_door_IsActivated(gate->doors[i])) {
      return true;
    }
  }

  return false;
}

/**
 * @return The travel costs from the bot to every node through the closed gates that it MAY try, or
 * NULL if the map has none. The costs are valid until the next call.
 * @param costs The travel costs from `G_Ai_TravelCosts`.
 */
static const GameAiGateCost *G_Ai_GateCosts(const GameClient *cl, const float *costs) {

  if (!module.numGates) {
    return NULL;
  }

  const uint32_t count = G_Ai_Node_Count();

  if (count > module.gateCount) {
    module.gateCosts = realloc(module.gateCosts, count * sizeof(GameAiGateCost));

    if (!module.gateCosts) {
      G_Error("Failed to allocate gate costs for %u nodes\n", count);
    }

    module.gateCount = count;
  }

  for (uint32_t i = 0; i < count; i++) {
    module.gateCosts[i] = (GameAiGateCost) { .cost = FLT_MAX, .key = -1, .source = AI_NODE_INVALID };
  }

  for (int32_t i = 0; i < module.numGates; i++) {
    const GameAiGate *gate = &module.gates[i];

    if (gate->numFails && gameLevel.time - gate->failTime < (uint32_t) AI_GATE_RETRY_MILLIS << Mini(gate->numFails - 1, AI_GATE_RETRY_DOUBLINGS)) {
      continue;
    }

    if (G_Ai_IsGateOpen(gate)) {
      continue;
    }

    const float window = gate->openMillis == UINT32_MAX ? FLT_MAX : gate->openMillis * PM_SPEED_RUN / 1000.f;

    if (!G_Ai_Node_GateCosts(cl, costs, gate->sources, gate->numSources, gate->name, window, i, module.gateCosts)) {
      return NULL;
    }
  }

  return module.gateCosts;
}

/**
 * @return The item of most utility, its value per travel time, that the bot can reach within
 * `maxCost`, or a choice with no entity.
 * @param maxCost The most travel cost, or 0 for no limit.
 * @param retreat True to choose only health, armor and the weapons that would arm the bot, and to
 * shun those that lie toward the enemy.
 * @param gateCosts The travel costs through gates, to choose also an item that the bot reaches
 * sooner through a gate, or NULL.
 */
static GameAiChoice G_Ai_ChooseItem(const GameClient *cl, const float *costs, const float maxCost, const bool retreat,
                                    const GameAiGateCost *gateCosts) {

  GameAiChoice best = { .node = AI_NODE_INVALID, .gate = -1, .button = -1, .buttonNode = AI_NODE_INVALID };

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
    int32_t gate = -1;
    int32_t button = -1;
    GameAiNodeId buttonNode = AI_NODE_INVALID;

    if (gateCosts && gateCosts[node].cost < cost) {
      const GameAiGate *g = &module.gates[gateCosts[node].key];

      for (int32_t i = 0; i < g->numSources; i++) {
        if (g->sources[i] == gateCosts[node].source) {
          cost = gateCosts[node].cost;
          gate = gateCosts[node].key;
          button = g->sourceButtons[i];
          buttonNode = g->sources[i];
          break;
        }
      }
    }

    if (cost == FLT_MAX || (maxCost > 0.f && cost > maxCost)) {
      continue;
    }

    float value;
    const float utility = G_Ai_ItemUtility(cl, ent, cost, retreat, noise, &value);

    if (utility <= 0.f) {
      continue;
    }

    if (utility > best.utility) {
      best.ent = ent;
      best.node = node;
      best.value = value;
      best.utility = utility;
      best.gate = gate;
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

  const GameAiNodeId src = G_Ai_PathStart(cl, AI_NODE_DISTANCE);

  if (src == AI_NODE_INVALID || dest == AI_NODE_INVALID) {
    return false;
  }

  Vector *path = G_Ai_Node_FindPath(cl, src, dest, 0.f, NULL);

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
 * item at each review. The item that the bot goes to is scored as a new choice is, but with no
 * noise, so that a retreating bot gives up an item that now lies toward the enemy.
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

    const float utility = G_Ai_ItemUtility(cl, current, cost, retreat, 0.f, NULL);

    if (utility > 0.f && (current == choice->ent || choice->utility <= utility * AI_SWITCH_SCALE)) {
      return true;
    }
  }

  return choice->ent && G_Ai_PathTo(cl, choice->node, choice->utility, choice->ent);
}

/**
 * @see g_ai_state.h
 */
const char *G_Ai_State_Name(const GameClient *cl) {
  return stateNames[cl->ai->state];
}

/**
 * @see g_ai_state.h
 */
const GameEntity *G_Ai_State_Objective(const GameClient *cl) {

  const int32_t number = cl->ai->objectiveNumber;

  if (number < 0 || number >= sv_maxEntities->integer) {
    return NULL;
  }

  const GameEntity *ent = ge.entities[number];

  if (!ent->inUse || !ent->item || ent->s.spawnId != cl->ai->objectiveSpawnId) {
    return NULL;
  }

  return ent;
}

/**
 * @brief Sets the bot's objective: the item to collect and its node, or a roam point with no item.
 */
static void G_Ai_SetObjective(GameClient *cl, const GameEntity *ent, const GameAiNodeId node) {

  cl->ai->objectiveNumber = ent ? ent->s.number : -1;
  cl->ai->objectiveSpawnId = ent ? ent->s.spawnId : 0;
  cl->ai->objectiveNode = node;
}

/**
 * @brief Switches the bot to the state from now, and forgets the button that it shoots.
 */
static void G_Ai_SwitchState(GameClient *cl, const GameAiStateId state) {

  GameAi *ai = cl->ai;

  G_Ai_Debug("%s: %s -> %s\n", cl->persistent.netName, stateNames[ai->state], stateNames[state]);

  ai->state = state;
  ai->stateTime = gameLevel.time;
  ai->shootTarget = NULL;
}

/**
 * @brief Enters the state, and clears the move target so that the state sets its own.
 */
static void G_Ai_Enter(GameClient *cl, const GameAiStateId state) {

  GameAi *ai = cl->ai;

  if (ai->state == state) {
    return;
  }

  G_Ai_SwitchState(cl, state);

  ai->reviewTime = ai->reviewedTime = 0;
  ai->statePathed = false;

  G_Ai_ClearGoal(&ai->moveTarget);
}

/**
 * @brief Returns the bot to its objective: the item that it collects, or its roam point.
 */
static void G_Ai_Resume(GameClient *cl) {
  G_Ai_Enter(cl, G_Ai_State_Objective(cl) ? AI_STATE_COLLECT : AI_STATE_ROAM);
}

/**
 * @brief Gives up the bot's objective, and avoids the item, if any, for a time.
 */
static void G_Ai_GiveUp(GameClient *cl, const GameEntity *item) {

  if (item) {
    G_Ai_State_Avoid(cl, item);
  }

  G_Ai_SetObjective(cl, NULL, AI_NODE_INVALID);
}

/**
 * @brief Gives up the bot's objective, avoids the item, if any, for a time, and roams.
 */
static void G_Ai_Abandon(GameClient *cl, const GameEntity *item) {

  G_Ai_GiveUp(cl, item);
  G_Ai_Enter(cl, AI_STATE_ROAM);
}

/**
 * @see g_ai_state.h
 */
bool G_Ai_State_IsWaiting(const GameClient *cl) {

  const GameEntity *objective = G_Ai_State_Objective(cl);

  return objective && objective->solid != SOLID_TRIGGER && Vec3_Distance(cl->entity->s.origin, objective->s.origin) < AI_ROAM_REACHED;
}

/**
 * @return True if the state should review its choices now: the review interval passed, or the bot
 * has no move target, does not wait, and no review with no move target came within
 * `AI_IDLE_REVIEW_MILLIS`. A review while the bot has a move target does not delay the next one, so
 * that a bot whose path ends chooses again at once.
 */
static bool G_Ai_ShouldReview(GameClient *cl) {

  GameAi *ai = cl->ai;

  const bool idle = ai->moveTarget.type == AI_GOAL_NONE && !G_Ai_State_IsWaiting(cl);

  if (idle ? gameLevel.time - ai->reviewedTime < AI_IDLE_REVIEW_MILLIS && ai->reviewedTime
           : ai->reviewTime > gameLevel.time) {
    return false;
  }

  ai->reviewedTime = idle ? gameLevel.time : 0;
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
 * are there and add at most `AI_DETOUR_COST` to the route to the objective. The added time is the travel
 * to the item and on from it to the objective, less the travel straight to it, so an item on the
 * route costs little, and an item behind the bot costs much. A detour never waits for an item to
 * return, because `G_Ai_State_detour` ends once the item is gone.
 * @return True if the bot takes a detour.
 */
static bool G_Ai_ChooseDetour(GameClient *cl, const float *costs) {

  const GameAiNodeId objectiveNode = cl->ai->objectiveNode;

  if (objectiveNode == AI_NODE_INVALID || objectiveNode >= G_Ai_Node_Count() || costs[objectiveNode] == FLT_MAX) {
    return false;
  }

  const float direct = costs[objectiveNode];
  const GameEntity *objective = G_Ai_State_Objective(cl);

  GameAiChoice candidates[AI_DETOUR_CANDIDATES] = { 0 };

  G_ForEachEntity(ent, {
    if (!ent->item || ent == objective || ent->solid != SOLID_TRIGGER) {
      continue;
    }

    const GameAiNodeId node = G_Ai_ItemNode(ent);

    if (node == AI_NODE_INVALID || node == objectiveNode || costs[node] == FLT_MAX || costs[node] > direct + AI_DETOUR_COST) {
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
    Vector *path = G_Ai_Node_FindPath(cl, choice->node, objectiveNode, direct + AI_DETOUR_COST, &onward);

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

  G_Ai_State_Avoid(cl, best.ent);
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
 * @return True if the entity is in water to its waist or deeper.
 */
static bool G_Ai_IsInWater(const GameEntity *ent) {
  return ent->waterLevel >= WATER_WAIST && (ent->waterType & CONTENTS_WATER);
}

/**
 * @brief The distance within which a bot in water goes on to the item in the water that it
 * collects, rather than retreat from an enemy out of the water.
 */
#define AI_WATER_ITEM_DISTANCE 256.f

/**
 * @return True if the item that the bot collects is there, in water, within
 * `AI_WATER_ITEM_DISTANCE`, as the quad damage in its pool on tokays. A player takes it first, and
 * then leaves the water.
 */
static bool G_Ai_IsObjectiveInWater(const GameClient *cl) {

  const GameEntity *objective = G_Ai_State_Objective(cl);

  if (!objective || objective->solid != SOLID_TRIGGER) {
    return false;
  }

  if (Vec3_Distance(objective->s.origin, cl->entity->s.origin) > AI_WATER_ITEM_DISTANCE) {
    return false;
  }

  return (gi.PointContents(objective->s.origin) & CONTENTS_WATER) != 0;
}

/**
 * @return True if the bot, which has a combat target, should retreat from it: if it is unarmed, for
 * health, from an enemy that outguns it, or if it is in water and the enemy is not, as a player in
 * water cannot dodge, and is below an enemy that fires down at them. A bot that retreats from the
 * water does so until it is out of it, so that it does not fight again at its edge. A bot near the
 * item in the water that it collects takes it first (`G_Ai_IsObjectiveInWater`). A bot that
 * retreats fights again only once its health exceeds its retreat health by `AI_RECOVER_SCALE`, so
 * that it does not switch at the threshold, and not within `AI_RETREAT_MILLIS` of the start of its
 * retreat, so that each change of the enemy's weapon does not switch it.
 * @remarks An unarmed bot only defends itself with the Blaster: it retreats to the weapons, armor
 * and health of most utility, and fires back on the way.
 */
static bool G_Ai_WantsRetreat(const GameClient *cl) {

  if (!G_Ai_IsArmed(cl)) {
    return true;
  }

  const GameEntity *ent = cl->entity;
  const bool retreating = cl->ai->state == AI_STATE_RETREAT;

  if (retreating && gameLevel.time - cl->ai->stateTime < AI_RETREAT_MILLIS) {
    return true;
  }

  const bool wet = retreating ? ent->waterLevel > WATER_NONE && (ent->waterType & CONTENTS_WATER)
                              : G_Ai_IsInWater(ent);

  if (wet && !G_Ai_IsInWater(cl->ai->combatTarget.entity.ent) && !G_Ai_IsObjectiveInWater(cl)) {
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
 * utility. After a path that ended in distress, the bot avoids the objective that it failed to reach,
 * and chooses another at once. It waits at an item for its respawn for at most `AI_WAIT_MILLIS`.
 */
static void G_Ai_State_navigate(GameClient *cl) {

  GameAi *ai = cl->ai;

  if (G_Ai_ConsiderAlert(cl)) {
    return;
  }

  if (ai->moveTarget.type == AI_GOAL_NONE && ai->reacquireTime > gameLevel.time) {
    G_Ai_GiveUp(cl, G_Ai_State_Objective(cl));
    ai->reacquireTime = 0;
  }

  if (!G_Ai_State_IsWaiting(cl)) {
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

  const GameEntity *objective = G_Ai_State_Objective(cl);
  float objectiveUtility = 0.f;

  if (objective) {
    const float cost = ai->objectiveNode < G_Ai_Node_Count() ? costs[ai->objectiveNode] : FLT_MAX;

    objectiveUtility = G_Ai_ItemUtility(cl, objective, cost, false, 0.f, NULL);

    if (objectiveUtility <= 0.f) {
      G_Ai_SetObjective(cl, NULL, AI_NODE_INVALID);
      G_Ai_ClearGoal(&ai->moveTarget);
      objective = NULL;
    }
  }

  const GameAiChoice item = G_Ai_ChooseItem(cl, costs, 0.f, false, G_Ai_GateCosts(cl, costs));

  if (item.ent && item.ent != objective && (!objective || item.utility > objectiveUtility * AI_SWITCH_SCALE)) {

    ai->gate = item.gate;

    if (item.gate != -1) {
      G_Ai_Enter(cl, AI_STATE_ACTIVATE);
      ai->gateButton = item.button;
      ai->gateNode = item.buttonNode;
      ai->gateRestTime = 0;
      G_Ai_SetObjective(cl, item.ent, item.node);

      if (G_Ai_PathTo(cl, item.buttonNode, item.utility, NULL)) {
        G_Ai_Probe_GateUse(cl, module.gates[item.gate].name, "choose");
        return;
      }

      G_Ai_Abandon(cl, item.ent);
      return;
    }

    G_Ai_Enter(cl, AI_STATE_COLLECT);
    G_Ai_ClearGoal(&ai->moveTarget);

    if (G_Ai_PathTo(cl, item.node, item.utility, item.ent)) {
      G_Ai_SetObjective(cl, item.ent, item.node);
      return;
    }

    G_Ai_State_Avoid(cl, item.ent);
  }

  if (ai->moveTarget.type == AI_GOAL_NONE) {

    if (objective && G_Ai_State_IsWaiting(cl)) {

      if (gameLevel.time - ai->waitTime < AI_WAIT_MILLIS) {
        return;
      }

      G_Ai_GiveUp(cl, objective);
      objective = NULL;
    }

    if (objective) {
      if (!G_Ai_PathTo(cl, ai->objectiveNode, objectiveUtility, objective)) {
        G_Ai_GiveUp(cl, objective);
      }
      return;
    }

    G_Ai_Enter(cl, AI_STATE_ROAM);

    GameAiNodeId node = ai->objectiveNode;

    if (node == AI_NODE_INVALID || node >= G_Ai_Node_Count() ||
        Vec3_Distance(cl->entity->s.origin, G_Ai_Node_GetPosition(node)) < AI_ROAM_REACHED) {
      node = G_Ai_RoamNode(cl, costs);
    }

    if (G_Ai_PathTo(cl, node, AI_ROAM_PRIORITY, NULL)) {
      G_Ai_SetObjective(cl, NULL, node);
    } else {
      G_Ai_SetObjective(cl, NULL, AI_NODE_INVALID);
    }
    return;
  }

  G_Ai_ChooseDetour(cl, costs);
}

/**
 * @brief Takes the detour item, then resumes the objective. A detour that fails does not count against
 * the objective: the bot avoids the detour item from the start of the detour.
 */
static void G_Ai_State_detour(GameClient *cl) {

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

    const CollisionTrace tr = gi.Trace(cl->entity->s.origin, center, bounds, cl->entity, cl->entity->clipMask);
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
 * @return The node that the bot reaches at the least travel cost, of those that `exclude` does not
 * match, or `AI_NODE_INVALID`.
 */
static GameAiNodeId G_Ai_CheapestNode(const float *costs, bool (*exclude)(const GameAiNodeId node)) {

  GameAiNodeId best = AI_NODE_INVALID;

  for (uint32_t i = 0; i < G_Ai_Node_Count(); i++) {
    if (costs[i] == FLT_MAX || exclude((GameAiNodeId) i)) {
      continue;
    }

    if (best == AI_NODE_INVALID || costs[i] < costs[best]) {
      best = (GameAiNodeId) i;
    }
  }

  return best;
}

/**
 * @return True if the bot, in water to its waist or deeper, takes the path to the nearest node out
 * of liquid, as a retreat with no item to go to. A retreat to an item leaves the water anyway if
 * the item is on dry land, because a path into water costs more. The bot fires on the way, because
 * its aim stays on its combat target.
 */
static bool G_Ai_LeaveWater(GameClient *cl, const float *costs) {

  if (!costs || !G_Ai_IsInWater(cl->entity)) {
    return false;
  }

  const GameAiNodeId best = G_Ai_CheapestNode(costs, G_Ai_Node_IsLiquid);

  return best != AI_NODE_INVALID && G_Ai_PathTo(cl, best, AI_PURSUE_PRIORITY, NULL);
}

/**
 * @brief Sets the bot's move target to its combat target, unless it already moves to that enemy.
 * @return True if the move target is set.
 */
static bool G_Ai_MoveToEnemy(GameClient *cl) {

  GameAi *ai = cl->ai;

  if (ai->moveTarget.type == AI_GOAL_ENTITY && G_Ai_GoalHasEntity(&ai->moveTarget, ai->combatTarget.entity.ent)) {
    return false;
  }

  G_Ai_CopyGoal(&ai->combatTarget, &ai->moveTarget);
  return true;
}

/**
 * @brief Fights the combat target, as only an armed bot does. Within `AI_ENGAGE_SCALE` of the
 * weapon's preferred range, the bot circle strafes around the enemy, and goes only for items close by.
 * Beyond it, the bot collects items while it fires, or, if it has none to collect, paths toward the
 * enemy.
 */
static void G_Ai_State_fight(GameClient *cl) {

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
    const GameAiChoice item = G_Ai_ChooseItem(cl, costs, maxCost, false, NULL);

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

  G_Ai_MoveToEnemy(cl);
}

/**
 * @brief Goes to the health, armor or weapon of most utility that does not lie toward the enemy,
 * while the aim stays on the enemy so that the bot fires back. An unarmed bot with none of those to
 * go to takes any item of value, because its circle strafe around an enemy far away moves toward it. With
 * none to go to, a bot in water leaves it (`G_Ai_LeaveWater`), and a bot out of water strafes
 * around the enemy and fights.
 */
static void G_Ai_State_retreat(GameClient *cl) {

  if (!G_Ai_ShouldReview(cl)) {
    return;
  }

  const float *costs = G_Ai_TravelCosts(cl);

  if (costs) {
    const GameAiChoice item = G_Ai_ChooseItem(cl, costs, 0.f, true, NULL);

    if (G_Ai_GoToItem(cl, costs, &item, 0.f, true)) {
      return;
    }

    if (!G_Ai_IsArmed(cl)) {
      const GameAiChoice any = G_Ai_ChooseItem(cl, costs, 0.f, false, NULL);

      if (G_Ai_GoToItem(cl, costs, &any, 0.f, false)) {
        return;
      }
    }
  }

  if (G_Ai_LeaveWater(cl, costs)) {
    return;
  }

  if (G_Ai_MoveToEnemy(cl)) {
    cl->ai->moveTarget.entity.combatType = AI_COMBAT_FLANK;
  }
}

/**
 * @return True if every activated door of the gate has come to rest.
 */
static bool G_Ai_IsGateAtRest(const GameAiGate *gate) {

  for (int32_t i = 0; i < gate->numDoors; i++) {
    if (gate->doors[i]->moveInfo.state == MOVE_STATE_GOING_UP) {
      return false;
    }
  }

  return true;
}

/**
 * @brief Marks the gate as failed, so that no bot tries it for a time that doubles with each
 * failure, and returns the bot to roam, away from the item.
 */
static void G_Ai_FailGate(GameClient *cl, GameAiGate *gate, const GameEntity *item) {

  gate->failTime = gameLevel.time;
  gate->numFails++;

  G_Ai_Probe_GateUse(cl, gate->name, "fail");

  G_Ai_Abandon(cl, item);
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
 * @return True if a player at `origin` walks straight into the button: its center is within
 * `AI_ACTIVATE_REACH` and in sight, and its bounds overlap the height of the player's box, or lie
 * within a step below it, as a plate in the floor does.
 */
static bool G_Ai_CanTouchButton(const Vec3 origin, const GameEntity *button) {

  const Box3 bounds = button->absBounds;

  if (bounds.maxs.z < origin.z + PM_BOUNDS.mins.z - PM_STEP_HEIGHT || bounds.mins.z > origin.z + PM_BOUNDS.maxs.z) {
    return false;
  }

  return G_Ai_SeesButton(origin, 0.f, button, AI_ACTIVATE_REACH);
}

/**
 * @return True if the bot stands where it presses the button: where it walks straight into a button
 * that it touches, or where it sees, within `AI_SHOOT_REACH`, a button that a shot presses.
 */
static bool G_Ai_IsAtButton(const GameClient *cl, const GameEntity *button) {

  if (G_Ai_IsShotButton(button)) {
    return G_Ai_SeesButton(cl->entity->s.origin, cl->ps.pmState.viewOffset.z, button, AI_SHOOT_REACH);
  }

  return G_Ai_CanTouchButton(cl->entity->s.origin, button);
}

/**
 * @brief Goes to the button of the gate on the way to the objective, and within `AI_ACTIVATE_REACH`
 * walks into it, or, for a button that a shot presses, shoots it once it sees it within
 * `AI_SHOOT_REACH`. Once a door of the gate moves, and a path to the item exists, the bot collects
 * the item. If no path exists `AI_GATE_PATH_MILLIS` after the doors come to rest while the bot is at
 * its button, or the bot does not press the button within `AI_ACTIVATE_MILLIS`, the gate fails. The
 * bot gives up, and avoids the item, if it finds no path back to the button's node.
 */
static void G_Ai_State_activate(GameClient *cl) {

  GameAi *ai = cl->ai;

  if (G_Ai_ConsiderAlert(cl)) {
    return;
  }

  const GameEntity *item = G_Ai_State_Objective(cl);

  if (!item || ai->gate < 0 || ai->gate >= module.numGates || ai->gateButton < 0 ||
      ai->gateButton >= module.gates[ai->gate].numButtons) {
    G_Ai_Abandon(cl, NULL);
    return;
  }

  GameAiGate *gate = &module.gates[ai->gate];

  if (gameLevel.time - ai->stateTime > AI_ACTIVATE_MILLIS) {
    G_Ai_FailGate(cl, gate, item);
    return;
  }

  const GameEntity *target = gate->buttons[ai->gateButton];
  const Vec3 button = Box3_Center(target->absBounds);

  if (G_Ai_IsGateOpen(gate)) {
    ai->shootTarget = NULL;

    if (!G_Ai_ShouldReview(cl)) {
      return;
    }

    const float *costs = G_Ai_TravelCosts(cl);

    if (costs && ai->objectiveNode < G_Ai_Node_Count() && costs[ai->objectiveNode] != FLT_MAX) {
      G_Ai_Probe_GateUse(cl, gate->name, "open");
      G_Ai_Enter(cl, AI_STATE_COLLECT);
      G_Ai_PathTo(cl, ai->objectiveNode, AI_PURSUE_PRIORITY, item);
      return;
    }

    if (!G_Ai_IsGateAtRest(gate) || !G_Ai_IsAtButton(cl, target)) {
      ai->gateRestTime = 0;
    } else if (!ai->gateRestTime) {
      ai->gateRestTime = gameLevel.time;
    } else if (gameLevel.time - ai->gateRestTime > AI_GATE_PATH_MILLIS) {
      G_Ai_FailGate(cl, gate, item);
      return;
    }
  } else if (G_Ai_IsShotButton(target)) {
    ai->shootTarget = G_Ai_SeesButton(cl->entity->s.origin, cl->ps.pmState.viewOffset.z, target, AI_SHOOT_REACH) ? target : NULL;
  } else if (G_Ai_CanTouchButton(cl->entity->s.origin, target)) {
    G_Ai_SetPositionGoal(cl, &ai->moveTarget, AI_PURSUE_PRIORITY, button);
    return;
  }

  if (ai->moveTarget.type == AI_GOAL_NONE && !G_Ai_IsAtButton(cl, target) &&
      !G_Ai_PathTo(cl, ai->gateNode, AI_PURSUE_PRIORITY, NULL)) {
    G_Ai_Abandon(cl, item);
  }
}

/**
 * @return True if the bot is in lava. A bot in slime, which hurts slowly, keeps its state.
 */
static bool G_Ai_IsInLava(const GameClient *cl) {
  return cl->entity->waterLevel > WATER_NONE && (cl->entity->waterType & CONTENTS_LAVA);
}

/**
 * @brief Takes a bot in lava by the cheapest path to the nearest node out of lava and slime, which
 * a path that starts in lava crosses, as up a ladder. A bot in slime, which hurts slowly, does not
 * escape: `G_Ai_IsInLava` enters this state only in lava. The bot plans again whenever its path ends while it is in the
 * lava, at most each `AI_ESCAPE_MILLIS`, and keeps the path out of it until it stands on the ground,
 * so that a bot whose feet leave the lava on a ladder climbs on.
 */
static void G_Ai_State_escape(GameClient *cl) {

  GameAi *ai = cl->ai;

  if (ai->moveTarget.type != AI_GOAL_NONE || ai->reviewTime > gameLevel.time) {
    return;
  }

  ai->reviewTime = gameLevel.time + AI_ESCAPE_MILLIS;

  const float *costs = G_Ai_TravelCosts(cl);

  if (!costs) {
    return;
  }

  const GameAiNodeId best = G_Ai_CheapestNode(costs, G_Ai_Node_IsHazard);

  if (best != AI_NODE_INVALID) {
    G_Ai_PathTo(cl, best, AI_PURSUE_PRIORITY, NULL);
  }
}

/**
 * @brief Goes to the position, once, then resumes the objective. Gives up after `AI_PURSUE_MILLIS`.
 */
static void G_Ai_State_pursue(GameClient *cl, const Vec3 position) {

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
 * @brief Enters `AI_STATE_CLIMB` and keeps the path: the climb owns the bot until it leaves the
 * ladder. A player who climbs is in the open and cannot fight back well, so the bot does not stop
 * to fight, or take a new objective, which would leave it on the ladder with no path.
 */
static void G_Ai_Climb(GameClient *cl) {

  GameAi *ai = cl->ai;

  if (ai->state == AI_STATE_CLIMB) {
    return;
  }

  ai->climbState = ai->state;
  ai->climbStateTime = ai->stateTime;

  G_Ai_SwitchState(cl, AI_STATE_CLIMB);
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

  if (G_Ai_IsAirborne(cl) || ai->ladderLeaveTime > gameLevel.time) {
    return 1;
  }

  if (G_Ai_IsInLava(cl)) {
    G_Ai_Enter(cl, AI_STATE_ESCAPE);
  } else if (G_Ai_IsCommittedToClimb(cl)) {
    G_Ai_Climb(cl);
    return AI_STATE_MILLIS;
  } else {
    if (ai->state == AI_STATE_CLIMB) {
      G_Ai_Debug("%s: climb -> %s\n", cl->persistent.netName, stateNames[ai->climbState]);
      ai->state = ai->climbState;
      ai->stateTime = ai->climbStateTime + (gameLevel.time - ai->stateTime);
    }

    if (ai->state == AI_STATE_ESCAPE) {
      if (cl->entity->ground.ent || ai->moveTarget.type == AI_GOAL_NONE) {
        G_Ai_Resume(cl);
      }
    } else if (ai->combatTarget.type == AI_GOAL_ENTITY) {
      G_Ai_Enter(cl, G_Ai_WantsRetreat(cl) ? AI_STATE_RETREAT : AI_STATE_FIGHT);
    } else if (ai->state == AI_STATE_FIGHT || ai->state == AI_STATE_RETREAT) {
      if (ai->combatTargetLost && G_Ai_WantsPursuit(cl)) {
        G_Ai_Enter(cl, AI_STATE_CHASE);
      } else {
        G_Ai_Resume(cl);
      }
    }
  }

  switch (ai->state) {
    case AI_STATE_COLLECT:
    case AI_STATE_ROAM:
      G_Ai_State_navigate(cl);
      break;
    case AI_STATE_DETOUR:
      G_Ai_State_detour(cl);
      break;
    case AI_STATE_FIGHT:
      G_Ai_State_fight(cl);
      break;
    case AI_STATE_CHASE:
      G_Ai_State_pursue(cl, ai->combatTargetSeenPosition);
      break;
    case AI_STATE_RETREAT:
      G_Ai_State_retreat(cl);
      break;
    case AI_STATE_INVESTIGATE:
      G_Ai_State_pursue(cl, ai->alertPosition);
      break;
    case AI_STATE_ACTIVATE:
      G_Ai_State_activate(cl);
      break;
    case AI_STATE_ESCAPE:
      G_Ai_State_escape(cl);
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
  ai->reviewTime = ai->reviewedTime = 0;
  ai->statePathed = false;
  ai->combatTargetLost = false;
  ai->gate = ai->gateButton = -1;
  ai->gateNode = AI_NODE_INVALID;
  ai->shootTarget = NULL;
  ai->ladderLeaveTime = 0;
  ai->climbState = AI_STATE_ROAM;
  ai->waitTime = 0;
  ai->gateRestTime = 0;
  ai->alertInvestigated = false;

  memset(ai->avoids, 0, sizeof(ai->avoids));

  G_Ai_SetObjective(cl, NULL, AI_NODE_INVALID);
}

/**
 * @return True if the entity is a door that a button MAY move.
 */
static bool G_Ai_IsDoor(const GameEntity *ent) {
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
 * presses it: from which it walks into the button (`G_Ai_CanTouchButton`), or, for a button that a
 * shot presses, with an eye that sees it within `AI_SHOOT_REACH`.
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
    } else if (!G_Ai_CanTouchButton(position, button)) {
      continue;
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
 * @return True if the map marks the door with `"ai_hint" "trap"`.
 */
static bool G_Ai_IsTrapDoor(const GameEntity *ent) {
  return !Str_Compare(gi.EntityValue(ent->def, "ai_hint")->string, "trap");
}

/**
 * @return True if the entity is a button that a bot touches to spring a trap.
 */
static bool G_Ai_IsTrapButton(const GameEntity *ent) {
  return ent->Touch && !Str_Compare(ent->classname, "func_button");
}

/**
 * @return True if the entity is a door that MAY be part of a gate: a door not marked
 * `"ai_hint" "trap"`.
 */
static bool G_Ai_IsGateDoor(const GameEntity *ent) {
  return G_Ai_IsDoor(ent) && !G_Ai_IsTrapDoor(ent);
}

/**
 * @brief Collects, up to `max`, the entities whose string at `field` is `name`, and which `accept`.
 * @return The number of entities collected.
 */
static int32_t G_Ai_FindEntities(const ptrdiff_t field, const char *name, bool (*accept)(const GameEntity *ent),
                                 const GameEntity **ents, const int32_t max) {

  int32_t count = 0;

  G_ForEachEntity(ent, {
    if (count == max) {
      break;
    }

    const char *value = *(const char **) ((const byte *) ent + field);

    if (value && !Str_Compare(value, name) && accept(ent)) {
      ents[count++] = ent;
    }
  });

  return count;
}

/**
 * @return The trap with the name, or NULL.
 */
static GameAiTrap *G_Ai_FindTrap(const char *name) {

  for (int32_t i = 0; i < module.numTraps; i++) {
    if (!Str_Compare(module.traps[i].name, name)) {
      return &module.traps[i];
    }
  }

  return NULL;
}

/**
 * @return The gate with the name, or NULL.
 */
static GameAiGate *G_Ai_FindGate(const char *name) {

  for (int32_t i = 0; i < module.numGates; i++) {
    if (!Str_Compare(module.gates[i].name, name)) {
      return &module.gates[i];
    }
  }

  return NULL;
}

/**
 * @brief Adds the trap of the doors with the `targetname`, with the buttons that target them.
 * @return The trap, with no doors yet.
 */
static GameAiTrap *G_Ai_LoadTraps_create(const char *name) {

  GameAiTrap *trap = &module.traps[module.numTraps++];
  memset(trap, 0, sizeof(*trap));

  trap->name = name;
  trap->numButtons = G_Ai_FindEntities(EOFS(target), name, G_Ai_IsTrapButton, trap->buttons, AI_TRAP_BUTTONS);

  return trap;
}

/**
 * @brief Finds the doors that a map marks with `"ai_hint" "trap"`, grouped by `targetname`, and the
 * buttons that a bot touches to move them.
 */
static void G_Ai_LoadTraps(void) {

  module.numTraps = 0;

  G_ForEachEntity(ent, {
    if (!G_Ai_IsDoor(ent) || !ent->targetName || !G_Ai_IsTrapDoor(ent)) {
      continue;
    }

    GameAiTrap *trap = G_Ai_FindTrap(ent->targetName);

    if (!trap) {
      if (module.numTraps == AI_MAX_TRAPS) {
        G_Warn("More than %d traps have \"ai_hint\" \"trap\"\n", AI_MAX_TRAPS);
        break;
      }

      trap = G_Ai_LoadTraps_create(ent->targetName);
    }

    if (trap->numDoors < AI_TRAP_DOORS) {
      trap->doors[trap->numDoors++] = ent;
    }
  });
}

/**
 * @brief Adds the gate of the doors with the `targetname`, with the shortest time for which one
 * stays activated after a press.
 * @return The gate, with no buttons yet, or NULL if no door has the `targetname`.
 */
static GameAiGate *G_Ai_LoadGates_create(const char *name) {

  GameAiGate *gate = &module.gates[module.numGates];
  memset(gate, 0, sizeof(*gate));

  gate->name = name;
  gate->openMillis = UINT32_MAX;
  gate->numDoors = G_Ai_FindEntities(EOFS(targetName), name, G_Ai_IsGateDoor, gate->doors, AI_GATE_DOORS);

  if (!gate->numDoors) {
    return NULL;
  }

  for (int32_t i = 0; i < gate->numDoors; i++) {
    const uint32_t open = G_Ai_DoorOpenMillis(gate->doors[i]);

    if (open < gate->openMillis) {
      gate->openMillis = open;
    }
  }

  module.numGates++;
  return gate;
}

/**
 * @brief Adds the button to the gate, with the nodes from which a bot presses it.
 */
static void G_Ai_LoadGates_button(GameAiGate *gate, const GameEntity *button) {

  GameAiNodeId nodes[AI_BUTTON_NODES];
  int32_t numNodes;

  G_Ai_ButtonNodes(button, nodes, &numNodes);

  for (int32_t i = 0; i < numNodes; i++) {
    gate->sources[gate->numSources] = nodes[i];
    gate->sourceButtons[gate->numSources] = gate->numButtons;
    gate->numSources++;
  }

  gate->buttons[gate->numButtons++] = button;
}

/**
 * @brief Finds the gates: the doors with the `target` of each `func_button` that a bot touches or
 * shoots, if a navigation link crosses their doorway, or their nodes link to others where a press
 * moves them, and the nodes from which a bot presses the button. A door marked `"ai_hint" "trap"` is
 * not part of a gate.
 */
static void G_Ai_LoadGates(void) {

  module.numGates = 0;

  G_ForEachEntity(ent, {
    if (Str_Compare(ent->classname, "func_button") || !ent->target || !(ent->Touch || G_Ai_IsShotButton(ent))) {
      continue;
    }

    GameAiGate *gate = G_Ai_FindGate(ent->target);

    if (!gate) {
      if (!G_Ai_Node_HasGate(ent->target)) {
        continue;
      }

      if (module.numGates == AI_MAX_GATES) {
        G_Warn("More than %d doors that buttons move gate navigation links\n", AI_MAX_GATES);
        break;
      }

      gate = G_Ai_LoadGates_create(ent->target);

      if (!gate) {
        continue;
      }
    }

    if (gate->numButtons < AI_GATE_BUTTONS) {
      G_Ai_LoadGates_button(gate, ent);
    }
  });

  for (int32_t i = 0; i < module.numGates; i++) {
    const GameAiGate *gate = &module.gates[i];

    G_Ai_Probe_Gate(gate->name, gate->numButtons, gate->numSources, gate->numDoors, gate->openMillis);
  }
}

/**
 * @see g_ai_state.h
 */
void G_Ai_State_Load(void) {

  G_Ai_LoadTraps();
  G_Ai_LoadGates();
}

/**
 * @see g_ai_state.h
 */
void G_Ai_State_Unload(void) {

  module.numGates = 0;
  module.numTraps = 0;
}

/**
 * @see g_ai_state.h
 */
void G_Ai_State_ItemTaken(const GameEntity *item, const GameEntity *taker) {

  if (!taker->client || !taker->client->ai) {
    return;
  }

  GameAi *ai = taker->client->ai;

  if (ai->gate >= 0 && ai->gate < module.numGates && G_Ai_State_Objective(taker->client) == item) {
    module.gates[ai->gate].numFails = 0;
    ai->gate = -1;
  }
}

/**
 * @see g_ai_state.h
 */
void G_Ai_State_Shutdown(void) {

  free(module.costs);
  module.costs = NULL;
  module.count = 0;

  free(module.gateCosts);
  module.gateCosts = NULL;
  module.gateCount = 0;
}
