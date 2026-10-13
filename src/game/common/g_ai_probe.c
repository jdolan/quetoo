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
 * @brief The count of samples of each bot that the probe keeps, and writes out when the bot dies
 * or is pushed: 3.2 seconds, which covers a fall into the void.
 */
#define AI_PROBE_SAMPLES 32

/**
 * @brief The interval in milliseconds between the samples of each bot.
 */
#define AI_PROBE_SAMPLE_MILLIS 100

/**
 * @brief The interval in milliseconds between the `POS` events of each bot.
 */
#define AI_PROBE_POSITION_MILLIS 1000

/**
 * @brief The interval in milliseconds between the `SPEED` events of each bot.
 */
#define AI_PROBE_SPEED_MILLIS 5000

/**
 * @brief The count of `POS` events over which a bot that keeps within `AI_PROBE_STUCK_DISTANCE` is
 * stuck.
 */
#define AI_PROBE_STUCK_POSITIONS 6

/**
 * @brief The distance within which a bot that follows a path is stuck.
 */
#define AI_PROBE_STUCK_DISTANCE 96.f

/**
 * @brief The distance from a `trigger_push` within which a bot that is pushed was pushed by it. The
 * probe runs after the bot moves, and a pad moves it about 25 units in a frame.
 */
#define AI_PROBE_PUSH_DISTANCE 32.f

/**
 * @brief The distance within which the probe finds the node nearest a client.
 */
#define AI_PROBE_VISIT_DISTANCE 128.f

/**
 * @brief The distance by which a node MUST be nearer a client than the node that it visits, for
 * the client to visit the nearer node.
 */
#define AI_PROBE_VISIT_MARGIN 16.f

/**
 * @brief The longest line of the log.
 */
#define AI_PROBE_LINE 4096

/**
 * @brief A sample of what a bot does.
 */
typedef struct {
  Vec3 origin;
  Vec3 velocity;
  int32_t from;
  int32_t to;
  int32_t travelType;
  bool strafeJumping;
  bool ground;
} GameAiProbeSample;

/**
 * @brief What the probe knows of each bot. It belongs to the bot's `GameAi`, and is cleared when
 * another bot takes the client slot.
 */
typedef struct {
  const GameAi *ai;
  const Vector *path;
  int32_t pathStart;
  uint32_t pathCount;
  uint32_t pathIndex;
  GameAiProbeSample samples[AI_PROBE_SAMPLES];
  uint32_t numSamples;
  uint32_t sampleTime;
  uint32_t positionTime;
  uint32_t speedTime;
  float speedSum;
  float speedMax;
  uint32_t frames;
  uint32_t strafeFrames;
  uint32_t airFrames;
  Vec3 positions[AI_PROBE_STUCK_POSITIONS];
  uint32_t numPositions;
  bool stuck;
  bool pushed;
  bool ground;
  bool jumping;
  float jumpHeight;
  int32_t visitNode;
  bool used;
} GameAiProbeClient;

static Cvar *g_aiProbe;

static struct {
  File *file;
  GameAiProbeClient clients[MAX_CLIENTS];
} module;

/**
 * @brief Writes one event to the log: `PROBE <time> <client> <event> <fields>`.
 */
static void G_Ai_Probe_Print(const int32_t client, const char *event, const char *fmt, ...) __attribute__((format(printf, 3, 4)));
static void G_Ai_Probe_Print(const int32_t client, const char *event, const char *fmt, ...) {

  char line[AI_PROBE_LINE];
  int32_t len = snprintf(line, sizeof(line), "PROBE %u %d %s ", gameLevel.time, client, event);

  va_list args;
  va_start(args, fmt);
  len += vsnprintf(line + len, sizeof(line) - len, fmt, args);
  va_end(args);

  len = Mini(len, (int32_t) sizeof(line) - 2);
  line[len++] = '\n';

  gi.WriteFile(module.file, line, 1, len);
}

/**
 * @return The node id, or -1 for `AI_NODE_INVALID`.
 */
static int32_t G_Ai_Probe_Node(const GameAiNodeId id) {
  return id == AI_NODE_INVALID ? -1 : (int32_t) id;
}

/**
 * @brief Finds the link that the bot travels: the nodes at its ends, and its `NavTravelType`, or
 * -1 for each if the bot follows no path.
 */
static void G_Ai_Probe_Link(const GameClient *cl, int32_t *from, int32_t *to, int32_t *travelType) {

  *from = *to = *travelType = -1;

  const GameAiGoal *goal = &cl->ai->moveTarget;

  if (goal->type != AI_GOAL_PATH || !goal->path.path ||
      goal->path.pathIndex == 0 || goal->path.pathIndex >= goal->path.path->count) {
    return;
  }

  const GameAiNodeId a = VectorValue(goal->path.path, GameAiNodeId, goal->path.pathIndex - 1);
  const GameAiNodeId b = VectorValue(goal->path.path, GameAiNodeId, goal->path.pathIndex);

  const GameAiLink *link = G_Ai_Node_GetLink(a, b);

  *from = G_Ai_Probe_Node(a);
  *to = G_Ai_Probe_Node(b);
  *travelType = link ? link->travelType : -1;
}

/**
 * @return True if the next link of the bot's path, after the one that it travels, is a jump pad
 * link. A vertical pad launches the bot before its path advances onto the pad link.
 */
static bool G_Ai_Probe_NextIsPad(const GameClient *cl) {

  const GameAiGoal *goal = &cl->ai->moveTarget;

  if (goal->type != AI_GOAL_PATH || !goal->path.path || goal->path.pathIndex + 1 >= goal->path.path->count) {
    return false;
  }

  const GameAiLink *link = G_Ai_Node_GetLink(VectorValue(goal->path.path, GameAiNodeId, goal->path.pathIndex),
                                             VectorValue(goal->path.path, GameAiNodeId, goal->path.pathIndex + 1));

  return link && link->travelType == NAV_TRAVEL_PUSH;
}

/**
 * @return True if the bot is within `AI_PROBE_PUSH_DISTANCE` of a `trigger_push`.
 * `PMF_TIME_PUSHED` is also set by the knockback of damage, which the probe does not record as a
 * push.
 */
static bool G_Ai_Probe_TouchesPush(const GameEntity *ent) {

  GameEntity *ents[64];
  const size_t len = gi.BoxEntities(Box3_Expand(ent->absBounds, AI_PROBE_PUSH_DISTANCE), ents, lengthof(ents), BOX_OCCUPY);

  for (size_t i = 0; i < len; i++) {
    if (ents[i]->classname && !Str_Compare(ents[i]->classname, "trigger_push")) {
      return true;
    }
  }

  return false;
}

/**
 * @brief Writes the bot's samples, oldest first, as `HIST` events.
 * @param reason The event that the samples lead to.
 */
static void G_Ai_Probe_History(const GameClient *cl, const char *reason) {

  const GameAiProbeClient *c = &module.clients[cl->ps.client];
  const uint32_t count = Mini(c->numSamples, AI_PROBE_SAMPLES);

  for (uint32_t i = 0; i < count; i++) {
    const GameAiProbeSample *s = &c->samples[(c->numSamples - count + i) % AI_PROBE_SAMPLES];

    G_Ai_Probe_Print(cl->ps.client, "HIST", "reason=%s i=%u pos=%d,%d,%d vel=%d,%d,%d link=%d->%d type=%d strafe=%d ground=%d",
                     reason, i, (int32_t) s->origin.x, (int32_t) s->origin.y, (int32_t) s->origin.z,
                     (int32_t) s->velocity.x, (int32_t) s->velocity.y, (int32_t) s->velocity.z,
                     s->from, s->to, s->travelType, s->strafeJumping, s->ground);
  }
}

/**
 * @return The class name of the bot's goal item, or `none`.
 */
static const char *G_Ai_Probe_Goal(const GameClient *cl) {

  const GameEntity *goal = G_Ai_GoalEntity(cl);

  return goal ? goal->classname : "none";
}

/**
 * @brief Writes a `STUCK` event if the bot follows a path out of combat, and its last
 * `AI_PROBE_STUCK_POSITIONS` positions lie within `AI_PROBE_STUCK_DISTANCE` of each other, and its
 * samples at the first `STUCK` event of each time that it is stuck.
 */
static void G_Ai_Probe_Stuck(const GameClient *cl, GameAiProbeClient *c, const int32_t from, const int32_t to,
                             const int32_t travelType) {

  const bool stuck = c->stuck;
  c->stuck = false;

  if (c->numPositions < AI_PROBE_STUCK_POSITIONS || cl->ai->moveTarget.type != AI_GOAL_PATH ||
      cl->ai->combatTarget.type == AI_GOAL_ENTITY) {
    return;
  }

  float spread = 0.f;
  for (int32_t i = 0; i < AI_PROBE_STUCK_POSITIONS; i++) {
    for (int32_t j = i + 1; j < AI_PROBE_STUCK_POSITIONS; j++) {
      spread = Maxf(spread, Vec3_Distance(c->positions[i], c->positions[j]));
    }
  }

  if (spread >= AI_PROBE_STUCK_DISTANCE) {
    return;
  }

  c->stuck = true;

  const GameEntity *ent = cl->entity;
  const Vec3 next = cl->ai->moveTarget.path.pathPosition;

  G_Ai_Probe_Print(cl->ps.client, "STUCK", "pos=%d,%d,%d state=%s goal=%s link=%d->%d type=%d next=%d,%d,%d ground=%d spread=%d",
                   (int32_t) ent->s.origin.x, (int32_t) ent->s.origin.y, (int32_t) ent->s.origin.z,
                   G_Ai_StateName(cl), G_Ai_Probe_Goal(cl), from, to, travelType,
                   (int32_t) next.x, (int32_t) next.y, (int32_t) next.z, ent->ground.ent != NULL, (int32_t) spread);

  if (!stuck) {
    G_Ai_Probe_History(cl, "stuck");
  }
}

/**
 * @return The probe's state for the client, cleared when another bot, or a player, takes the
 * client slot.
 */
static GameAiProbeClient *G_Ai_Probe_Client(const GameClient *cl) {

  GameAiProbeClient *c = &module.clients[cl->ps.client];

  if (!c->used || c->ai != cl->ai) {
    memset(c, 0, sizeof(*c));
    c->ai = cl->ai;
    c->used = true;
    c->visitNode = -1;
  }

  return c;
}

/**
 * @brief Records a `VISIT` when the node nearest the client changes, so that the log holds each
 * client's route, node by node. The client leaves its node only for one at least
 * `AI_PROBE_VISIT_MARGIN` nearer, so that a client between two nodes does not flip between them.
 * @return The node nearest the client, or -1 if none is within `AI_PROBE_VISIT_DISTANCE`.
 */
static int32_t G_Ai_Probe_Visit(const GameClient *cl, GameAiProbeClient *c) {

  const GameEntity *ent = cl->entity;
  const int32_t node = G_Ai_Probe_Node(G_Ai_Node_FindClosest(ent->s.origin, AI_PROBE_VISIT_DISTANCE, false, true));

  if (node == -1 || node == c->visitNode) {
    return node;
  }

  if (c->visitNode != -1 && (uint32_t) c->visitNode < G_Ai_Node_Count()) {
    const float current = Vec3_Distance(ent->s.origin, G_Ai_Node_GetPosition((GameAiNodeId) c->visitNode));
    const float nearer = Vec3_Distance(ent->s.origin, G_Ai_Node_GetPosition((GameAiNodeId) node));

    if (nearer + AI_PROBE_VISIT_MARGIN > current) {
      return node;
    }
  }

  c->visitNode = node;

  G_Ai_Probe_Print(cl->ps.client, "VISIT", "node=%d pos=%d,%d,%d speed=%d ground=%d water=%d",
                   node, (int32_t) ent->s.origin.x, (int32_t) ent->s.origin.y, (int32_t) ent->s.origin.z,
                   (int32_t) Vec2_Length(Vec3_XY(ent->velocity)), ent->ground.ent != NULL, ent->waterLevel);

  return node;
}

/**
 * @see g_ai_probe.h
 */
void G_Ai_Probe_Init(void) {

  g_aiProbe = gi.AddCvar("g_aiProbe", "0", CVAR_DEVELOPER, "Records what bots do to ai_probe_<level>.log: 1 for the bots, 2 for the navigation graph, 4 for the players");
}

/**
 * @see g_ai_probe.h
 */
void G_Ai_Probe_Load(void) {

  G_Ai_Probe_Shutdown();

  memset(module.clients, 0, sizeof(module.clients));

  if (!g_aiProbe->integer) {
    return;
  }

  const char *path = va("ai_probe_%s.log", gameLevel.name);

  module.file = gi.OpenFileWrite(path);
  if (!module.file) {
    G_Warn("Failed to open %s for g_aiProbe\n", path);
    return;
  }

  gi.Print("  Recording bots to %s\n", path);
}

/**
 * @see g_ai_probe.h
 */
void G_Ai_Probe_Shutdown(void) {

  if (module.file) {
    gi.CloseFile(module.file);
    module.file = NULL;
  }
}

/**
 * @see g_ai_probe.h
 */
void G_Ai_Probe_Graph(void) {

  if (!module.file) {
    return;
  }

  size_t numLinks = 0;
  for (uint32_t i = 0; i < G_Ai_Node_Count(); i++) {
    const Vector *links = G_Ai_Node_GetLinks((GameAiNodeId) i);
    numLinks += links ? links->count : 0;
  }

  G_Ai_Probe_Print(-1, "GRAPH", "nodes=%u links=%zu", G_Ai_Node_Count(), numLinks);

  if (!(g_aiProbe->integer & AI_PROBE_GRAPH)) {
    return;
  }

  for (uint32_t i = 0; i < G_Ai_Node_Count(); i++) {

    char text[AI_PROBE_LINE - 128] = "";
    size_t len = 0;

    const Vector *links = G_Ai_Node_GetLinks((GameAiNodeId) i);
    for (size_t j = 0; links && j < links->count && len < sizeof(text) - 32; j++) {
      const GameAiLink *link = VectorElement(links, GameAiLink, j);
      len += snprintf(text + len, sizeof(text) - len, "%s%d:%d:%d", j ? "," : "", link->id, link->travelType, (int32_t) link->cost);
    }

    const Vec3 position = G_Ai_Node_GetPosition((GameAiNodeId) i);

    G_Ai_Probe_Print(-1, "NODE", "id=%u pos=%d,%d,%d edge=%d links=%s",
                     i, (int32_t) position.x, (int32_t) position.y, (int32_t) position.z,
                     G_Ai_Node_IsEdge((GameAiNodeId) i), text);
  }

  for (int32_t i = 0; i < sv_maxEntities->integer; i++) {

    const GameEntity *ent = ge.entities[i];
    if (!ent->inUse || !ent->classname) {
      continue;
    }

    if (Str_Compare(ent->classname, "trigger_push") && Str_Compare(ent->classname, "trigger_hurt")) {
      continue;
    }

    G_Ai_Probe_Print(-1, "TRIGGER", "class=%s mins=%d,%d,%d maxs=%d,%d,%d", ent->classname,
                     (int32_t) ent->absBounds.mins.x, (int32_t) ent->absBounds.mins.y, (int32_t) ent->absBounds.mins.z,
                     (int32_t) ent->absBounds.maxs.x, (int32_t) ent->absBounds.maxs.y, (int32_t) ent->absBounds.maxs.z);
  }
}

/**
 * @see g_ai_probe.h
 */
void G_Ai_Probe_Think(const GameClient *cl) {

  if (!module.file || !(g_aiProbe->integer & AI_PROBE_BOTS)) {
    return;
  }

  GameAiProbeClient *c = G_Ai_Probe_Client(cl);

  const GameEntity *ent = cl->entity;
  const int32_t n = cl->ps.client;
  const bool alive = !ent->dead;
  const bool ground = ent->ground.ent != NULL;

  int32_t from, to, travelType;
  G_Ai_Probe_Link(cl, &from, &to, &travelType);

  if (gameLevel.time - c->sampleTime >= AI_PROBE_SAMPLE_MILLIS) {
    c->sampleTime = gameLevel.time;
    c->samples[c->numSamples++ % AI_PROBE_SAMPLES] = (GameAiProbeSample) {
      .origin = ent->s.origin,
      .velocity = ent->velocity,
      .from = from,
      .to = to,
      .travelType = travelType,
      .strafeJumping = cl->ai->strafeJumping,
      .ground = ground
    };
  }

  const Vector *path = cl->ai->moveTarget.type == AI_GOAL_PATH ? cl->ai->moveTarget.path.path : NULL;
  const int32_t pathStart = path && path->count ? G_Ai_Probe_Node(VectorValue(path, GameAiNodeId, 0)) : -1;
  const uint32_t pathCount = path ? (uint32_t) path->count : 0;
  const uint32_t pathIndex = path ? cl->ai->moveTarget.path.pathIndex : 0;

  if (path && pathCount && (path != c->path || pathStart != c->pathStart || pathCount != c->pathCount ||
                            pathIndex < c->pathIndex)) {
    const GameAiNodeId start = VectorValue(path, GameAiNodeId, 0);
    const GameAiNodeId next = VectorValue(path, GameAiNodeId, Mini((int32_t) path->count - 1, 1));
    G_Ai_Probe_Print(n, "PATH", "pos=%d,%d,%d vel=%d,%d,%d ground=%d start=%d next=%d end=%d nodes=%zu goal=%s",
                     (int32_t) ent->s.origin.x, (int32_t) ent->s.origin.y, (int32_t) ent->s.origin.z,
                     (int32_t) ent->velocity.x, (int32_t) ent->velocity.y, (int32_t) ent->velocity.z, ground,
                     G_Ai_Probe_Node(start), G_Ai_Probe_Node(next),
                     G_Ai_Probe_Node(VectorValue(path, GameAiNodeId, path->count - 1)), path->count, G_Ai_Probe_Goal(cl));
  }

  c->path = path;
  c->pathStart = pathStart;
  c->pathCount = pathCount;
  c->pathIndex = pathIndex;

  if (!alive) {
    c->numPositions = 0;
    c->jumping = c->pushed = false;
    return;
  }

  const bool pushed = (cl->ps.pmState.flags & PMF_TIME_PUSHED) != 0;
  if (pushed && !c->pushed && G_Ai_Probe_TouchesPush(ent)) {
    const bool planned = travelType == NAV_TRAVEL_PUSH || G_Ai_Probe_NextIsPad(cl);
    G_Ai_Probe_Print(n, "PUSHED", "pos=%d,%d,%d vel=%d,%d,%d link=%d->%d type=%d strafe=%d ground=%d planned=%d launch=%u goal=%s",
                     (int32_t) ent->s.origin.x, (int32_t) ent->s.origin.y, (int32_t) ent->s.origin.z,
                     (int32_t) ent->velocity.x, (int32_t) ent->velocity.y, (int32_t) ent->velocity.z,
                     from, to, travelType, cl->ai->strafeJumping, ground, planned, cl->ai->pushTime, G_Ai_Probe_Goal(cl));
    if (!planned) {
      G_Ai_Probe_History(cl, "pushed");
    }
  }

  c->pushed = pushed;

  if (c->ground && !ground && travelType == NAV_TRAVEL_JUMP) {
    c->jumping = true;
    c->jumpHeight = cl->ai->moveTarget.path.pathPosition.z;
    G_Ai_Probe_Print(n, "JUMP", "pos=%d,%d,%d vel=%d,%d link=%d->%d",
                     (int32_t) ent->s.origin.x, (int32_t) ent->s.origin.y, (int32_t) ent->s.origin.z,
                     (int32_t) ent->velocity.x, (int32_t) ent->velocity.y, from, to);
  } else if (c->jumping && ground) {
    c->jumping = false;
    G_Ai_Probe_Print(n, "LAND", "pos=%d,%d,%d dz=%d",
                     (int32_t) ent->s.origin.x, (int32_t) ent->s.origin.y, (int32_t) ent->s.origin.z,
                     (int32_t) (ent->s.origin.z - c->jumpHeight));
  }

  c->ground = ground;

  const float speed = Vec2_Length(Vec3_XY(ent->velocity));

  c->speedSum += speed;
  c->speedMax = Maxf(c->speedMax, speed);
  c->frames++;
  c->strafeFrames += cl->ai->strafeJumping;
  c->airFrames += !ground;

  if (gameLevel.time - c->speedTime >= AI_PROBE_SPEED_MILLIS) {
    G_Ai_Probe_Print(n, "SPEED", "name=%s skill=%.2f avg=%d max=%d strafe=%.2f air=%.2f",
                     cl->ai->roster ? cl->ai->roster->name : "none", cl->ai->personality.skill,
                     (int32_t) (c->speedSum / Maxf(1.f, c->frames)), (int32_t) c->speedMax,
                     c->strafeFrames / Maxf(1.f, c->frames), c->airFrames / Maxf(1.f, c->frames));

    c->speedTime = gameLevel.time;
    c->speedSum = c->speedMax = 0.f;
    c->frames = c->strafeFrames = c->airFrames = 0;
  }

  if (gameLevel.time - c->positionTime >= AI_PROBE_POSITION_MILLIS) {
    c->positionTime = gameLevel.time;

    const GameAiGoal *combat = &cl->ai->combatTarget;
    const bool enemy = combat->type == AI_GOAL_ENTITY && combat->entity.ent;

    char fire[MAX_STRING_CHARS] = "";
    for (GameItemTag t = WEAPON_FIRST; t < WEAPON_LAST; t++) {
      const GameItem *it = &gameItems[t];
      if (G_Ai_CanFire(cl, it)) {
        Str_Append(fire, va("%s%s", *fire ? "," : "", it->def.classname), sizeof(fire));
      }
    }

    G_Ai_Probe_Print(n, "POS", "pos=%d,%d,%d speed=%d state=%s goal=%s health=%d link=%d->%d type=%d weapon=%s fire=%s armed=%d enemy=%d combat=%d water=%d ground=%d",
                     (int32_t) ent->s.origin.x, (int32_t) ent->s.origin.y, (int32_t) ent->s.origin.z,
                     (int32_t) Vec2_Length(Vec3_XY(ent->velocity)), G_Ai_StateName(cl), G_Ai_Probe_Goal(cl),
                     ent->health, from, to, travelType, cl->weapon ? cl->weapon->def.classname : "none", *fire ? fire : "none",
                     G_Ai_IsArmed(cl), enemy ? (int32_t) Vec3_Distance(ent->s.origin, combat->entity.ent->s.origin) : -1,
                     enemy ? (int32_t) combat->entity.combatType : -1, ent->waterLevel, ent->ground.ent != NULL);

    c->positions[c->numPositions++ % AI_PROBE_STUCK_POSITIONS] = ent->s.origin;

    G_Ai_Probe_Stuck(cl, c, from, to, travelType);
  }

  G_Ai_Probe_Visit(cl, c);
}

/**
 * @see g_ai_probe.h
 */
void G_Ai_Probe_PathEnd(const GameClient *cl, const char *reason) {

  if (!module.file || !(g_aiProbe->integer & AI_PROBE_BOTS)) {
    return;
  }

  const GameEntity *ent = cl->entity;
  const GameAiGoal *goal = &cl->ai->moveTarget;

  int32_t from, to, travelType;
  G_Ai_Probe_Link(cl, &from, &to, &travelType);

  G_Ai_Probe_Print(cl->ps.client, "PATHEND", "reason=%s pos=%d,%d,%d vel=%d,%d,%d ground=%d type=%d link=%d->%d index=%u/%u goal=%s",
                   reason, (int32_t) ent->s.origin.x, (int32_t) ent->s.origin.y, (int32_t) ent->s.origin.z,
                   (int32_t) ent->velocity.x, (int32_t) ent->velocity.y, (int32_t) ent->velocity.z,
                   ent->ground.ent != NULL, goal->type, from, to,
                   goal->type == AI_GOAL_PATH ? goal->path.pathIndex : 0,
                   goal->type == AI_GOAL_PATH && goal->path.path ? (uint32_t) goal->path.path->count : 0,
                   G_Ai_Probe_Goal(cl));
}

/**
 * @see g_ai_probe.h
 */
void G_Ai_Probe_ItemTaken(const GameClient *cl, const GameEntity *item) {

  if (!module.file || !(g_aiProbe->integer & (cl->ai ? AI_PROBE_BOTS : AI_PROBE_PLAYERS))) {
    return;
  }

  G_Ai_Probe_Print(cl->ps.client, "PICKUP", "item=%s number=%d pos=%d,%d,%d", item->classname, item->s.number,
                   (int32_t) item->s.origin.x, (int32_t) item->s.origin.y, (int32_t) item->s.origin.z);
}

/**
 * @see g_ai_probe.h
 */
void G_Ai_Probe_ClientMove(const GameClient *cl, const PMoveCmd *cmd) {

  if (!module.file || !(g_aiProbe->integer & AI_PROBE_PLAYERS)) {
    return;
  }

  const GameEntity *ent = cl->entity;

  if (cl->ai || cl->persistent.spectator || ent->dead) {
    return;
  }

  GameAiProbeClient *c = G_Ai_Probe_Client(cl);
  const int32_t node = G_Ai_Probe_Visit(cl, c);

  G_Ai_Probe_Print(cl->ps.client, "CMD", "node=%d pos=%d,%d,%d vel=%d,%d,%d angles=%.1f,%.1f move=%d,%d,%d msec=%d "
                   "ground=%d water=%d buttons=%d",
                   node, (int32_t) ent->s.origin.x, (int32_t) ent->s.origin.y, (int32_t) ent->s.origin.z,
                   (int32_t) ent->velocity.x, (int32_t) ent->velocity.y, (int32_t) ent->velocity.z,
                   cl->angles.y, cl->angles.x, cmd->forward, cmd->right, cmd->up, cmd->msec,
                   ent->ground.ent != NULL, ent->waterLevel, cmd->buttons);
}

/**
 * @see g_ai_probe.h
 */
void G_Ai_Probe_Killed(const GameClient *cl, const GameEntity *attacker, const uint32_t mod) {

  if (!module.file || !(g_aiProbe->integer & (AI_PROBE_BOTS | AI_PROBE_PLAYERS))) {
    return;
  }

  const GameEntity *ent = cl->entity;
  const GameClient *killer = attacker && attacker->client ? attacker->client : NULL;

  G_Ai_Probe_Print(cl->ps.client, "KILL", "attacker=%d mod=%u weapon=%s range=%d pos=%d,%d,%d bot=%d attackerBot=%d",
                   killer ? killer->ps.client : -1, mod,
                   killer && killer->weapon ? killer->weapon->def.classname : "none",
                   killer ? (int32_t) Vec3_Distance(ent->s.origin, killer->entity->s.origin) : -1,
                   (int32_t) ent->s.origin.x, (int32_t) ent->s.origin.y, (int32_t) ent->s.origin.z,
                   cl->ai != NULL, killer && killer->ai);

  GameAiProbeClient *c = G_Ai_Probe_Client(cl);
  c->visitNode = -1;

  if (!cl->ai || !(g_aiProbe->integer & AI_PROBE_BOTS)) {
    return;
  }

  int32_t from, to, travelType;
  G_Ai_Probe_Link(cl, &from, &to, &travelType);

  G_Ai_Probe_Print(cl->ps.client, "DEATH", "pos=%d,%d,%d water=%d link=%d->%d type=%d goal=%s",
                   (int32_t) ent->s.origin.x, (int32_t) ent->s.origin.y, (int32_t) ent->s.origin.z,
                   ent->waterLevel, from, to, travelType, G_Ai_Probe_Goal(cl));
  G_Ai_Probe_History(cl, "death");

  c->numPositions = 0;
  c->jumping = c->pushed = false;
}
