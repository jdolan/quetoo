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
 * @brief The count of samples of each bot that the profiler keeps, and writes out when the bot dies
 * or is pushed: 3.2 seconds, which covers a fall into the void.
 */
#define AI_PROFILE_SAMPLES 32

/**
 * @brief The interval in milliseconds between the samples of each bot.
 */
#define AI_PROFILE_SAMPLE_MILLIS 100

/**
 * @brief The interval in milliseconds between the `POS` events of each bot.
 */
#define AI_PROFILE_POSITION_MILLIS 1000

/**
 * @brief The interval in milliseconds between the `SPEED` events of each bot.
 */
#define AI_PROFILE_SPEED_MILLIS 5000

/**
 * @brief The count of `POS` events over which a bot that keeps within `AI_PROFILE_STUCK_DISTANCE`
 * is stuck.
 */
#define AI_PROFILE_STUCK_POSITIONS 6

/**
 * @brief The distance within which a bot that follows a path is stuck.
 */
#define AI_PROFILE_STUCK_DISTANCE 96.f

/**
 * @brief The distance from a `trigger_push` within which a bot that is pushed was pushed by it. The
 * profiler runs after the bot moves, and a pad moves it about 25 units in a frame.
 */
#define AI_PROFILE_PUSH_DISTANCE 32.f

/**
 * @brief The distance within which the profiler finds the node nearest a client.
 */
#define AI_PROFILE_VISIT_DISTANCE 128.f

/**
 * @brief The distance by which a node MUST be nearer a client than the node that it visits, for
 * the client to visit the nearer node.
 */
#define AI_PROFILE_VISIT_MARGIN 16.f

/**
 * @brief The longest line of the log.
 */
#define AI_PROFILE_LINE 4096

/**
 * @brief Expands to the x, y and z of the vector, truncated to integers, as the arguments of a
 * `%d,%d,%d` in a format: the log writes each position and velocity so.
 */
#define AI_PROFILE_VEC3(v) (int32_t) (v).x, (int32_t) (v).y, (int32_t) (v).z

/**
 * @brief A sample of what a bot does.
 */
typedef struct {
  /**
   * @brief The bot's origin.
   */
  Vec3 origin;

  /**
   * @brief The bot's velocity.
   */
  Vec3 velocity;

  /**
   * @brief The node at the start of the link that the bot travels, or -1.
   */
  int32_t from;

  /**
   * @brief The node at the end of the link that the bot travels, or -1.
   */
  int32_t to;

  /**
   * @brief The `NavTravelType` of the link that the bot travels, or -1.
   */
  int32_t travelType;

  /**
   * @brief True if the bot strafe jumps.
   */
  bool strafeJumping;

  /**
   * @brief True if the bot is on the ground.
   */
  bool ground;

  /**
   * @brief The name of the bot's state.
   */
  const char *state;

  /**
   * @brief True if the knockback of damage pushes the bot, away from any `trigger_push`.
   */
  bool knocked;

  /**
   * @brief True if the bot is on a ladder.
   */
  bool ladder;

  /**
   * @brief The name of the ladder controller's phase.
   */
  const char *phase;
} GameAiProfileSample;

/**
 * @brief What the profiler knows of each bot. It belongs to the bot's `GameAi`, and is cleared when
 * another bot takes the client slot.
 */
typedef struct {
  /**
   * @brief The bot's `GameAi`, or NULL for a player, by which the profiler tells another bot in the
   * client slot.
   */
  const GameAi *ai;

  /**
   * @brief The path that the bot followed last frame, or NULL.
   */
  const Vector *path;

  /**
   * @brief The first node of that path, or -1.
   */
  int32_t pathStart;

  /**
   * @brief The count of nodes in that path.
   */
  uint32_t pathCount;

  /**
   * @brief The bot's index in that path.
   */
  uint32_t pathIndex;

  /**
   * @brief The bot's last samples, a ring of `AI_PROFILE_SAMPLES`.
   */
  GameAiProfileSample samples[AI_PROFILE_SAMPLES];

  /**
   * @brief The count of samples taken, of which the last `AI_PROFILE_SAMPLES` are kept.
   */
  uint32_t numSamples;

  /**
   * @brief The time of the last sample.
   */
  uint32_t sampleTime;

  /**
   * @brief The time of the last `POS` event.
   */
  uint32_t positionTime;

  /**
   * @brief The time of the last `SPEED` event.
   */
  uint32_t speedTime;

  /**
   * @brief The sum of the bot's horizontal speed over the frames since the last `SPEED` event.
   */
  float speedSum;

  /**
   * @brief The bot's highest horizontal speed since the last `SPEED` event.
   */
  float speedMax;

  /**
   * @brief The count of frames since the last `SPEED` event.
   */
  uint32_t frames;

  /**
   * @brief The count of those frames in which the bot strafe jumped.
   */
  uint32_t strafeFrames;

  /**
   * @brief The count of those frames in which the bot was in the air.
   */
  uint32_t airFrames;

  /**
   * @brief The count of those frames in which the bot followed no path.
   */
  uint32_t offPathFrames;

  /**
   * @brief The count of those frames on a path, by the reason that the bot did or did not strafe
   * jump.
   */
  uint32_t blockFrames[AI_STRAFE_BLOCK_TOTAL];

  /**
   * @brief The bot's positions at its last `POS` events, a ring of `AI_PROFILE_STUCK_POSITIONS`.
   */
  Vec3 positions[AI_PROFILE_STUCK_POSITIONS];

  /**
   * @brief The count of positions kept since the bot last died.
   */
  uint32_t numPositions;

  /**
   * @brief True if the bot was stuck at its last `POS` event.
   */
  bool stuck;

  /**
   * @brief True if the bot was pushed last frame.
   */
  bool pushed;

  /**
   * @brief True if the bot was in lava or slime last frame.
   */
  bool hazard;

  /**
   * @brief True if the bot was on the ground last frame.
   */
  bool ground;

  /**
   * @brief True from the bot's `JUMP` until its `LAND`.
   */
  bool jumping;

  /**
   * @brief The height of the bot's path position when it took off, from which `LAND` measures.
   */
  float jumpHeight;

  /**
   * @brief The node that the client last visited, or -1.
   */
  int32_t visitNode;

  /**
   * @brief True if the bot was on a ladder last frame.
   */
  bool ladder;

  /**
   * @brief True once the profiler has set up the state for the client in the slot.
   */
  bool used;
} GameAiProfileClient;

static Cvar *g_aiProfile;

static struct {
  /**
   * @brief The log, or NULL if the profiler records nothing.
   */
  File *file;

  /**
   * @brief What the profiler knows of each client, by client number.
   */
  GameAiProfileClient clients[MAX_CLIENTS];
} module;

/**
 * @return True if the log is open and `g_aiProfile` has any of the flags.
 */
static bool G_Ai_Profile_Records(const int32_t flags) {
  return module.file && (g_aiProfile->integer & flags);
}

/**
 * @brief Writes one event to the log: `PROFILE <time> <client> <event> <fields>`.
 */
static void G_Ai_Profile_Print(const int32_t client, const char *event, const char *fmt, ...)
  __attribute__((format(printf, 3, 4)));
static void G_Ai_Profile_Print(const int32_t client, const char *event, const char *fmt, ...) {

  char line[AI_PROFILE_LINE];
  int32_t len = snprintf(line, sizeof(line), "PROFILE %u %d %s ", gameLevel.time, client, event);

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
static int32_t G_Ai_Profile_Node(const GameAiNodeId id) {
  return id == AI_NODE_INVALID ? -1 : (int32_t) id;
}

/**
 * @brief Finds the link that the bot travels: the nodes at its ends, and its `NavTravelType`, or
 * -1 for each if the bot follows no path.
 */
static void G_Ai_Profile_Link(const GameClient *cl, int32_t *from, int32_t *to,
                              int32_t *travelType) {

  *from = *to = *travelType = -1;

  const GameAiGoal *goal = &cl->ai->moveTarget;

  if (goal->type != AI_GOAL_PATH || !goal->path.path ||
      goal->path.pathIndex == 0 || goal->path.pathIndex >= goal->path.path->count) {
    return;
  }

  const GameAiNodeId a = VectorValue(goal->path.path, GameAiNodeId, goal->path.pathIndex - 1);
  const GameAiNodeId b = VectorValue(goal->path.path, GameAiNodeId, goal->path.pathIndex);

  const GameAiLink *link = G_Ai_Node_GetLink(a, b);

  *from = G_Ai_Profile_Node(a);
  *to = G_Ai_Profile_Node(b);
  *travelType = link ? link->travelType : -1;
}

/**
 * @return True if the next link of the bot's path, after the one that it travels, is a jump pad
 * link. A vertical pad launches the bot before its path advances onto the pad link.
 */
static bool G_Ai_Profile_NextIsPush(const GameClient *cl) {

  const GameAiGoal *goal = &cl->ai->moveTarget;

  if (goal->type != AI_GOAL_PATH || !goal->path.path ||
      goal->path.pathIndex + 1 >= goal->path.path->count) {
    return false;
  }

  const GameAiLink *link = G_Ai_Node_GetLink(VectorValue(goal->path.path, GameAiNodeId,
                                                         goal->path.pathIndex),
                                             VectorValue(goal->path.path, GameAiNodeId,
                                                         goal->path.pathIndex + 1));

  return link && link->travelType == NAV_TRAVEL_PUSH;
}

/**
 * @return True if the bot is within `AI_PROFILE_PUSH_DISTANCE` of a `trigger_push`.
 * `PMF_TIME_PUSHED` is also set by the knockback of damage, which the profiler does not record as a
 * push.
 */
static bool G_Ai_Profile_TouchesPush(const GameEntity *ent) {

  GameEntity *ents[64];
  const size_t len = gi.BoxEntities(Box3_Expand(ent->absBounds, AI_PROFILE_PUSH_DISTANCE), ents,
                                    lengthof(ents), BOX_OCCUPY);

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
static void G_Ai_Profile_History(const GameClient *cl, const char *reason) {

  const GameAiProfileClient *c = &module.clients[cl->ps.client];
  const uint32_t count = Mini(c->numSamples, AI_PROFILE_SAMPLES);

  for (uint32_t i = 0; i < count; i++) {
    const GameAiProfileSample *s = &c->samples[(c->numSamples - count + i) % AI_PROFILE_SAMPLES];

    G_Ai_Profile_Print(cl->ps.client, "HIST",
                       "reason=%s i=%u pos=%d,%d,%d vel=%d,%d,%d link=%d->%d "
                       "type=%d strafe=%d ground=%d state=%s knocked=%d ladder=%d phase=%s", reason,
                       i, AI_PROFILE_VEC3(s->origin), AI_PROFILE_VEC3(s->velocity), s->from, s->to,
                       s->travelType, s->strafeJumping, s->ground, s->state, s->knocked, s->ladder,
                       s->phase);
  }
}

/**
 * @return `name` with each space replaced by an underscore, in a static buffer, because a value in
 * the log MUST NOT contain a space. A name from the map, such as a gate's target, MAY have one.
 */
static const char *G_Ai_Profile_Token(const char *name) {

  static char token[MAX_QPATH];

  Str_Copy(token, name ?: "none", sizeof(token));

  for (char *c = token; *c; c++) {
    if (isspace((unsigned char) *c)) {
      *c = '_';
    }
  }

  return token;
}

/**
 * @return The class name of the entity of the bot's task while it does one, else of its objective,
 * or `none`.
 */
static const char *G_Ai_Profile_Objective(const GameClient *cl) {

  if (cl->ai->state == AI_STATE_TASK && cl->ai->task.ent && cl->ai->task.ent->inUse) {
    return cl->ai->task.ent->classname;
  }

  const GameEntity *objective = G_Ai_State_Objective(cl);

  return objective ? objective->classname : "none";
}

/**
 * @brief Writes a `STUCK` event if the bot follows a path out of combat, and its last
 * `AI_PROFILE_STUCK_POSITIONS` positions lie within `AI_PROFILE_STUCK_DISTANCE` of each other, and
 * its samples at the first `STUCK` event of each time that it is stuck.
 */
static void G_Ai_Profile_Stuck(const GameClient *cl, GameAiProfileClient *c, const int32_t from,
                               const int32_t to, const int32_t travelType) {

  const bool stuck = c->stuck;
  c->stuck = false;

  if (c->numPositions < AI_PROFILE_STUCK_POSITIONS || cl->ai->moveTarget.type != AI_GOAL_PATH ||
      cl->ai->combatTarget.type == AI_GOAL_ENTITY) {
    return;
  }

  float spread = 0.f;
  for (int32_t i = 0; i < AI_PROFILE_STUCK_POSITIONS; i++) {
    for (int32_t j = i + 1; j < AI_PROFILE_STUCK_POSITIONS; j++) {
      spread = Maxf(spread, Vec3_Distance(c->positions[i], c->positions[j]));
    }
  }

  if (spread >= AI_PROFILE_STUCK_DISTANCE) {
    return;
  }

  c->stuck = true;

  const GameEntity *ent = cl->entity;
  const Vec3 next = cl->ai->moveTarget.path.pathPosition;

  G_Ai_Profile_Print(cl->ps.client, "STUCK", "pos=%d,%d,%d state=%s goal=%s link=%d->%d type=%d "
                     "next=%d,%d,%d ground=%d spread=%d", AI_PROFILE_VEC3(ent->s.origin),
                     G_Ai_State_Name(cl), G_Ai_Profile_Objective(cl), from, to, travelType,
                     AI_PROFILE_VEC3(next), ent->ground.ent != NULL, (int32_t) spread);

  if (!stuck) {
    G_Ai_Profile_History(cl, "stuck");
  }
}

/**
 * @return The profiler's state for the client, cleared when another bot, or a player, takes the
 * client slot.
 */
static GameAiProfileClient *G_Ai_Profile_Client(const GameClient *cl) {

  GameAiProfileClient *c = &module.clients[cl->ps.client];

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
 * `AI_PROFILE_VISIT_MARGIN` nearer, so that a client between two nodes does not flip between them.
 * @return The node nearest the client, or -1 if none is within `AI_PROFILE_VISIT_DISTANCE`.
 */
static int32_t G_Ai_Profile_Visit(const GameClient *cl, GameAiProfileClient *c) {

  const GameEntity *ent = cl->entity;
  const GameAiNodeId closest = G_Ai_Node_FindClosest(ent->s.origin, AI_PROFILE_VISIT_DISTANCE,
                                                      false, true);
  const int32_t node = G_Ai_Profile_Node(closest);

  if (node == -1 || node == c->visitNode) {
    return node;
  }

  if (c->visitNode != -1 && (uint32_t) c->visitNode < G_Ai_Node_Count()) {
    const float current = Vec3_Distance(ent->s.origin,
                                        G_Ai_Node_GetPosition((GameAiNodeId) c->visitNode));
    const float nearer = Vec3_Distance(ent->s.origin, G_Ai_Node_GetPosition((GameAiNodeId) node));

    if (nearer + AI_PROFILE_VISIT_MARGIN > current) {
      return node;
    }
  }

  c->visitNode = node;

  G_Ai_Profile_Print(cl->ps.client, "VISIT", "node=%d pos=%d,%d,%d speed=%d ground=%d water=%d",
                     node, AI_PROFILE_VEC3(ent->s.origin),
                     (int32_t) Vec2_Length(Vec3_XY(ent->velocity)), ent->ground.ent != NULL,
                     ent->waterLevel);

  return node;
}

/**
 * @see g_ai_profile.h
 */
void G_Ai_Profile_Init(void) {

  g_aiProfile = gi.AddCvar("g_aiProfile", "0", CVAR_DEVELOPER, "Records what bots do to "
                           "ai_profile_<level>.log: 1 for the bots, 2 for the navigation graph, "
                           "4 for the players");
}

/**
 * @see g_ai_profile.h
 */
void G_Ai_Profile_Load(void) {

  G_Ai_Profile_Shutdown();

  memset(module.clients, 0, sizeof(module.clients));

  if (!g_aiProfile->integer) {
    return;
  }

  const char *path = va("ai_profile_%s.log", gameLevel.name);

  module.file = gi.OpenFileWrite(path);
  if (!module.file) {
    G_Warn("Failed to open %s for g_aiProfile\n", path);
    return;
  }

  gi.Print("  Recording bots to %s\n", path);
}

/**
 * @see g_ai_profile.h
 */
void G_Ai_Profile_Shutdown(void) {

  if (module.file) {
    gi.CloseFile(module.file);
    module.file = NULL;
  }
}

/**
 * @brief Writes a `NODE` event for the node, with its links, and a `CLIMB` event for each of its
 * ladder links that has a climb.
 */
static void G_Ai_Profile_Graph_node(const uint32_t id) {

  char text[AI_PROFILE_LINE - 128] = "";
  size_t len = 0;

  const Vector *links = G_Ai_Node_GetLinks((GameAiNodeId) id);
  for (size_t j = 0; links && j < links->count && len < sizeof(text) - 32; j++) {
    const GameAiLink *link = VectorElement(links, GameAiLink, j);
    len += snprintf(text + len, sizeof(text) - len, "%s%d:%d:%d", j ? "," : "", link->id,
                    link->travelType, (int32_t) link->cost);
    if (link->gate) {
      len += snprintf(text + len, sizeof(text) - len, ":%d", link->gate);
    }
  }

  const Vec3 position = G_Ai_Node_GetPosition((GameAiNodeId) id);

  G_Ai_Profile_Print(-1, "NODE", "id=%u pos=%d,%d,%d edge=%d links=%s", id,
                     AI_PROFILE_VEC3(position), G_Ai_Node_IsEdge((GameAiNodeId) id), text);

  for (size_t j = 0; links && j < links->count; j++) {
    const GameAiLink *link = VectorElement(links, GameAiLink, j);
    const NavLadderClimb *climb = link->travelType == NAV_TRAVEL_LADDER ?
                                  G_Ai_Node_GetClimb(id, link->id) : NULL;

    if (climb) {
      G_Ai_Profile_Print(-1, "CLIMB", "bottom=%d top=%d column=%d,%d,%d landing=%d,%d,%d time=%d",
                         climb->bottom, climb->top, AI_PROFILE_VEC3(climb->column),
                         AI_PROFILE_VEC3(climb->landing), climb->millis);
    }
  }
}

/**
 * @brief Writes a `TRIGGER` event with the bounds of the entity, if it is a `trigger_push` or a
 * `trigger_hurt`.
 */
static void G_Ai_Profile_Graph_trigger(const GameEntity *ent) {

  if (!ent->inUse || !ent->classname) {
    return;
  }

  if (Str_Compare(ent->classname, "trigger_push") && Str_Compare(ent->classname, "trigger_hurt")) {
    return;
  }

  G_Ai_Profile_Print(-1, "TRIGGER", "class=%s mins=%d,%d,%d maxs=%d,%d,%d", ent->classname,
                     AI_PROFILE_VEC3(ent->absBounds.mins), AI_PROFILE_VEC3(ent->absBounds.maxs));
}

/**
 * @see g_ai_profile.h
 */
void G_Ai_Profile_Graph(void) {

  if (!module.file) {
    return;
  }

  size_t numLinks = 0;
  for (uint32_t i = 0; i < G_Ai_Node_Count(); i++) {
    const Vector *links = G_Ai_Node_GetLinks((GameAiNodeId) i);
    numLinks += links ? links->count : 0;
  }

  G_Ai_Profile_Print(-1, "GRAPH", "nodes=%u links=%zu", G_Ai_Node_Count(), numLinks);

  if (!G_Ai_Profile_Records(AI_PROFILE_GRAPH)) {
    return;
  }

  for (uint32_t i = 0; i < G_Ai_Node_Count(); i++) {
    G_Ai_Profile_Graph_node(i);
  }

  for (int32_t i = 0; i < sv_maxEntities->integer; i++) {
    G_Ai_Profile_Graph_trigger(ge.entities[i]);
  }
}

/**
 * @brief Writes a `LADDER` event when the bot gets on or off a ladder, with a short trace ahead of
 * it.
 */
static void G_Ai_Profile_Think_ladder(const GameClient *cl, GameAiProfileClient *c,
                                      const bool ladder, const int32_t from, const int32_t to,
                                      const int32_t travelType) {

  if (ladder == c->ladder) {
    return;
  }

  c->ladder = ladder;

  const GameEntity *ent = cl->entity;

  Vec3 forward;
  Vec3_Vectors(MakeVec3(0.f, cl->angles.y, 0.f), &forward, NULL, NULL);

  const Box3 bounds = Pm_Bounds(&cl->ps.pmState.params, cl->ps.pmState.flags & PMF_DUCKED);
  const Vec3 ahead = Vec3_Fmaf(ent->s.origin, 4.f, forward);
  const CollisionTrace tr = gi.Trace(ent->s.origin, ahead, bounds, ent, ent->clipMask);

  G_Ai_Profile_Print(cl->ps.client, "LADDER", "on=%d pos=%d,%d,%d vel=%d,%d,%d link=%d->%d type=%d "
                     "phase=%s yaw=%d flags=%x trace=%.2f,%x,%d,%d", ladder,
                     AI_PROFILE_VEC3(ent->s.origin), AI_PROFILE_VEC3(ent->velocity), from, to,
                     travelType, G_Ai_LadderPhaseName(cl), (int32_t) cl->angles.y,
                     (uint32_t) (cl->ps.pmState.flags & PMF_TIME_MASK), tr.fraction,
                     (uint32_t) tr.contents, (int32_t) (tr.plane.normal.x * 100.f),
                     (int32_t) (tr.plane.normal.y * 100.f));
}

/**
 * @brief Keeps a sample of the bot each `AI_PROFILE_SAMPLE_MILLIS`.
 */
static void G_Ai_Profile_Think_sample(const GameClient *cl, GameAiProfileClient *c,
                                      const bool ground, const bool ladder, const int32_t from,
                                      const int32_t to, const int32_t travelType) {

  if (gameLevel.time - c->sampleTime < AI_PROFILE_SAMPLE_MILLIS) {
    return;
  }

  const GameEntity *ent = cl->entity;

  c->sampleTime = gameLevel.time;
  c->samples[c->numSamples++ % AI_PROFILE_SAMPLES] = (GameAiProfileSample) {
    .origin = ent->s.origin,
    .velocity = ent->velocity,
    .from = from,
    .to = to,
    .travelType = travelType,
    .strafeJumping = cl->ai->strafeJumping,
    .ground = ground,
    .state = G_Ai_State_Name(cl),
    .knocked = (cl->ps.pmState.flags & PMF_TIME_PUSHED) && !G_Ai_Profile_TouchesPush(ent),
    .ladder = ladder,
    .phase = G_Ai_LadderPhaseName(cl)
  };
}

/**
 * @brief Writes a `PATH` event when the bot starts a path, or its path restarts, and remembers the
 * path to tell the next one from it.
 */
static void G_Ai_Profile_Think_path(const GameClient *cl, GameAiProfileClient *c,
                                    const bool ground) {

  const GameEntity *ent = cl->entity;

  const GameAiGoal *goal = &cl->ai->moveTarget;
  const Vector *path = goal->type == AI_GOAL_PATH ? goal->path.path : NULL;
  const int32_t pathStart = path && path->count ?
                            G_Ai_Profile_Node(VectorValue(path, GameAiNodeId, 0)) : -1;
  const uint32_t pathCount = path ? (uint32_t) path->count : 0;
  const uint32_t pathIndex = path ? goal->path.pathIndex : 0;

  if (path && pathCount && (path != c->path || pathStart != c->pathStart ||
                            pathCount != c->pathCount || pathIndex < c->pathIndex)) {
    const GameAiNodeId start = VectorValue(path, GameAiNodeId, 0);
    const GameAiNodeId next = VectorValue(path, GameAiNodeId, Mini((int32_t) path->count - 1, 1));
    G_Ai_Profile_Print(cl->ps.client, "PATH",
                       "pos=%d,%d,%d vel=%d,%d,%d ground=%d start=%d next=%d "
                       "end=%d nodes=%zu goal=%s", AI_PROFILE_VEC3(ent->s.origin),
                       AI_PROFILE_VEC3(ent->velocity), ground, G_Ai_Profile_Node(start),
                       G_Ai_Profile_Node(next),
                       G_Ai_Profile_Node(VectorValue(path, GameAiNodeId, path->count - 1)),
                       path->count, G_Ai_Profile_Objective(cl));
  }

  c->path = path;
  c->pathStart = pathStart;
  c->pathCount = pathCount;
  c->pathIndex = pathIndex;
}

/**
 * @brief Writes a `HAZARD` event, and the bot's samples, when the bot enters lava or slime.
 */
static void G_Ai_Profile_Think_hazard(const GameClient *cl, GameAiProfileClient *c,
                                      const int32_t from, const int32_t to,
                                      const int32_t travelType) {

  const GameEntity *ent = cl->entity;

  const bool hazard = ent->waterLevel && (ent->waterType & (CONTENTS_LAVA | CONTENTS_SLIME));
  if (hazard && !c->hazard) {
    G_Ai_Profile_Print(cl->ps.client, "HAZARD", "pos=%d,%d,%d vel=%d,%d,%d link=%d->%d type=%d "
                       "strafe=%d state=%s enemy=%d lava=%d", AI_PROFILE_VEC3(ent->s.origin),
                       AI_PROFILE_VEC3(ent->velocity), from, to, travelType, cl->ai->strafeJumping,
                       G_Ai_State_Name(cl),
                       (cl->ai->combatTarget.type == AI_GOAL_ENTITY &&
                        cl->ai->combatTarget.entity.ent), (ent->waterType & CONTENTS_LAVA) != 0);
    G_Ai_Profile_History(cl, "hazard");
  }

  c->hazard = hazard;
}

/**
 * @brief Writes a `PUSHED` event when a `trigger_push` pushes the bot, and the bot's samples if its
 * path did not plan the push.
 */
static void G_Ai_Profile_Think_pushed(const GameClient *cl, GameAiProfileClient *c,
                                      const bool ground, const int32_t from, const int32_t to,
                                      const int32_t travelType) {

  const GameEntity *ent = cl->entity;

  const bool pushed = (cl->ps.pmState.flags & PMF_TIME_PUSHED) != 0;
  if (pushed && !c->pushed && G_Ai_Profile_TouchesPush(ent)) {
    const bool planned = travelType == NAV_TRAVEL_PUSH || G_Ai_Profile_NextIsPush(cl);
    G_Ai_Profile_Print(cl->ps.client, "PUSHED", "pos=%d,%d,%d vel=%d,%d,%d link=%d->%d type=%d "
                       "strafe=%d ground=%d planned=%d launch=%u goal=%s",
                       AI_PROFILE_VEC3(ent->s.origin), AI_PROFILE_VEC3(ent->velocity), from, to,
                       travelType, cl->ai->strafeJumping, ground, planned, cl->ai->pushTime,
                       G_Ai_Profile_Objective(cl));
    if (!planned) {
      G_Ai_Profile_History(cl, "pushed");
    }
  }

  c->pushed = pushed;
}

/**
 * @brief Writes a `JUMP` event when the bot leaves the ground on a jump link, and a `LAND` event
 * when it lands from that jump.
 */
static void G_Ai_Profile_Think_jump(const GameClient *cl, GameAiProfileClient *c, const bool ground,
                                    const int32_t from, const int32_t to,
                                    const int32_t travelType) {

  const GameEntity *ent = cl->entity;

  if (c->ground && !ground && travelType == NAV_TRAVEL_JUMP) {
    c->jumping = true;
    c->jumpHeight = cl->ai->moveTarget.path.pathPosition.z;
    G_Ai_Profile_Print(cl->ps.client, "JUMP", "pos=%d,%d,%d vel=%d,%d link=%d->%d",
                       AI_PROFILE_VEC3(ent->s.origin), (int32_t) ent->velocity.x,
                       (int32_t) ent->velocity.y, from, to);
  } else if (c->jumping && ground) {
    c->jumping = false;
    G_Ai_Profile_Print(cl->ps.client, "LAND", "pos=%d,%d,%d dz=%d", AI_PROFILE_VEC3(ent->s.origin),
                       (int32_t) (ent->s.origin.z - c->jumpHeight));
  }

  c->ground = ground;
}

/**
 * @brief Adds the frame to the bot's speed and strafe statistics, and writes them as a `SPEED`
 * event each `AI_PROFILE_SPEED_MILLIS`.
 * @param speed The bot's horizontal speed.
 */
static void G_Ai_Profile_Think_speed(const GameClient *cl, GameAiProfileClient *c,
                                     const bool ground, const float speed) {

  c->speedSum += speed;
  c->speedMax = Maxf(c->speedMax, speed);
  c->frames++;
  c->strafeFrames += cl->ai->strafeJumping;
  c->airFrames += !ground;

  if (cl->ai->moveTarget.type == AI_GOAL_PATH) {
    c->blockFrames[cl->ai->strafeBlock]++;
  } else {
    c->offPathFrames++;
  }

  if (gameLevel.time - c->speedTime < AI_PROFILE_SPEED_MILLIS) {
    return;
  }

  const float frames = Maxf(1.f, c->frames);

  G_Ai_Profile_Print(cl->ps.client, "SPEED",
                     "name=%s skill=%.2f avg=%d max=%d strafe=%.2f air=%.2f "
                     "offPath=%.2f strafing=%.2f lowskill=%.2f state=%.2f turn=%.2f short=%.2f "
                     "unsafe=%.2f fall=%.2f", cl->ai->roster ? cl->ai->roster->name : "none",
                     cl->ai->personality.skill, (int32_t) (c->speedSum / frames),
                     (int32_t) c->speedMax, c->strafeFrames / frames, c->airFrames / frames,
                     c->offPathFrames / frames, c->blockFrames[AI_STRAFE_BLOCK_NONE] / frames,
                     c->blockFrames[AI_STRAFE_BLOCK_SKILL] / frames,
                     c->blockFrames[AI_STRAFE_BLOCK_STATE] / frames,
                     c->blockFrames[AI_STRAFE_BLOCK_TURN] / frames,
                     c->blockFrames[AI_STRAFE_BLOCK_SHORT] / frames,
                     c->blockFrames[AI_STRAFE_BLOCK_UNSAFE] / frames,
                     c->blockFrames[AI_STRAFE_BLOCK_FALL] / frames);

  c->speedTime = gameLevel.time;
  c->speedSum = c->speedMax = 0.f;
  c->frames = c->strafeFrames = c->airFrames = c->offPathFrames = 0;
  memset(c->blockFrames, 0, sizeof(c->blockFrames));
}

/**
 * @brief Writes a `POS` event each `AI_PROFILE_POSITION_MILLIS`, keeps the position, and checks
 * whether the bot is stuck.
 * @param speed The bot's horizontal speed.
 */
static void G_Ai_Profile_Think_position(const GameClient *cl, GameAiProfileClient *c,
                                        const bool ground, const float speed, const int32_t from,
                                        const int32_t to, const int32_t travelType) {

  if (gameLevel.time - c->positionTime < AI_PROFILE_POSITION_MILLIS) {
    return;
  }

  c->positionTime = gameLevel.time;

  const GameEntity *ent = cl->entity;
  const GameAiGoal *combat = &cl->ai->combatTarget;
  const bool enemy = combat->type == AI_GOAL_ENTITY && combat->entity.ent;

  char fire[MAX_STRING_CHARS] = "";
  for (GameItemTag t = WEAPON_FIRST; t < WEAPON_LAST; t++) {
    const GameItem *it = &gameItems[t];
    if (G_Ai_CanFire(cl, it)) {
      Str_Append(fire, va("%s%s", *fire ? "," : "", it->def.classname), sizeof(fire));
    }
  }

  G_Ai_Profile_Print(cl->ps.client, "POS", "pos=%d,%d,%d speed=%d state=%s goal=%s health=%d "
                     "link=%d->%d type=%d weapon=%s fire=%s armed=%d enemy=%d combat=%d water=%d "
                     "ground=%d", AI_PROFILE_VEC3(ent->s.origin), (int32_t) speed,
                     G_Ai_State_Name(cl), G_Ai_Profile_Objective(cl), ent->health, from,
                     to, travelType, cl->weapon ? cl->weapon->def.classname : "none",
                     *fire ? fire : "none", G_Ai_IsArmed(cl),
                     enemy ? (int32_t) Vec3_Distance(ent->s.origin,
                                                     combat->entity.ent->s.origin) : -1,
                     enemy ? (int32_t) combat->entity.combatType : -1, ent->waterLevel, ground);

  c->positions[c->numPositions++ % AI_PROFILE_STUCK_POSITIONS] = ent->s.origin;

  G_Ai_Profile_Stuck(cl, c, from, to, travelType);
}

/**
 * @see g_ai_profile.h
 */
void G_Ai_Profile_Think(const GameClient *cl) {

  if (!G_Ai_Profile_Records(AI_PROFILE_BOTS)) {
    return;
  }

  GameAiProfileClient *c = G_Ai_Profile_Client(cl);

  const GameEntity *ent = cl->entity;
  const bool ground = ent->ground.ent != NULL;
  const bool ladder = (cl->ps.pmState.flags & PMF_ON_LADDER) != 0;

  int32_t from, to, travelType;
  G_Ai_Profile_Link(cl, &from, &to, &travelType);

  G_Ai_Profile_Think_ladder(cl, c, ladder, from, to, travelType);
  G_Ai_Profile_Think_sample(cl, c, ground, ladder, from, to, travelType);
  G_Ai_Profile_Think_path(cl, c, ground);

  if (ent->dead) {
    c->numPositions = 0;
    c->jumping = c->pushed = c->hazard = false;
    return;
  }

  const float speed = Vec2_Length(Vec3_XY(ent->velocity));

  G_Ai_Profile_Think_hazard(cl, c, from, to, travelType);
  G_Ai_Profile_Think_pushed(cl, c, ground, from, to, travelType);
  G_Ai_Profile_Think_jump(cl, c, ground, from, to, travelType);
  G_Ai_Profile_Think_speed(cl, c, ground, speed);
  G_Ai_Profile_Think_position(cl, c, ground, speed, from, to, travelType);

  G_Ai_Profile_Visit(cl, c);
}

/**
 * @see g_ai_profile.h
 */
void G_Ai_Profile_PathEnd(const GameClient *cl, const char *reason) {

  if (!G_Ai_Profile_Records(AI_PROFILE_BOTS)) {
    return;
  }

  const GameEntity *ent = cl->entity;
  const GameAiGoal *goal = &cl->ai->moveTarget;

  int32_t from, to, travelType;
  G_Ai_Profile_Link(cl, &from, &to, &travelType);

  G_Ai_Profile_Print(cl->ps.client, "PATHEND", "reason=%s pos=%d,%d,%d vel=%d,%d,%d ground=%d "
                     "goalType=%d link=%d->%d index=%u/%u goal=%s", reason,
                     AI_PROFILE_VEC3(ent->s.origin), AI_PROFILE_VEC3(ent->velocity),
                     ent->ground.ent != NULL, goal->type, from, to,
                     goal->type == AI_GOAL_PATH ? goal->path.pathIndex : 0,
                     goal->type == AI_GOAL_PATH && goal->path.path ?
                     (uint32_t) goal->path.path->count : 0, G_Ai_Profile_Objective(cl));

  if (!Str_Compare(reason, "distress")) {
    G_Ai_Profile_History(cl, "distress");
  }
}

/**
 * @see g_ai_profile.h
 */
void G_Ai_Profile_Gate(const char *name, const int32_t numButtons, const int32_t numSources,
                       const int32_t numDoors, const uint32_t openMillis) {

  if (!G_Ai_Profile_Records(AI_PROFILE_BOTS)) {
    return;
  }

  G_Ai_Profile_Print(-1, "GATE", "name=%s buttons=%d sources=%d doors=%d open=%d",
                     G_Ai_Profile_Token(name), numButtons, numSources, numDoors,
                     openMillis == UINT32_MAX ? -1 : (int32_t) openMillis);
}

/**
 * @see g_ai_profile.h
 */
void G_Ai_Profile_GateUse(const GameClient *cl, const char *name, const char *result) {

  if (!G_Ai_Profile_Records(AI_PROFILE_BOTS)) {
    return;
  }

  G_Ai_Profile_Print(cl->ps.client, "GATEUSE", "name=%s result=%s pos=%d,%d,%d goal=%s",
                     G_Ai_Profile_Token(name), result, (int32_t) cl->entity->s.origin.x,
                     (int32_t) cl->entity->s.origin.y, (int32_t) cl->entity->s.origin.z,
                     G_Ai_Profile_Objective(cl));
}

/**
 * @see g_ai_profile.h
 */
void G_Ai_Profile_WeaponChange(const GameClient *cl, const GameItem *weapon, const char *reason) {

  if (!G_Ai_Profile_Records(AI_PROFILE_BOTS)) {
    return;
  }

  const GameEntity *enemy = cl->ai->combatTarget.type == AI_GOAL_ENTITY ?
                            cl->ai->combatTarget.entity.ent : NULL;

  char from[MAX_QPATH];
  Str_Copy(from, G_Ai_Profile_Token(cl->weapon ? cl->weapon->def.name : NULL), sizeof(from));

  G_Ai_Profile_Print(cl->ps.client, "WEAPON", "from=%s to=%s reason=%s enemy=%d health=%d", from,
                     G_Ai_Profile_Token(weapon->def.name), reason,
                     enemy ? (int32_t) Vec3_Distance(cl->entity->s.origin, enemy->s.origin) : -1,
                     cl->entity->health);
}

/**
 * @see g_ai_profile.h
 */
void G_Ai_Profile_ItemTaken(const GameClient *cl, const GameEntity *item) {

  if (!G_Ai_Profile_Records(cl->ai ? AI_PROFILE_BOTS : AI_PROFILE_PLAYERS)) {
    return;
  }

  G_Ai_Profile_Print(cl->ps.client, "PICKUP", "item=%s number=%d pos=%d,%d,%d", item->classname,
                     item->s.number, AI_PROFILE_VEC3(item->s.origin));
}

/**
 * @see g_ai_profile.h
 */
void G_Ai_Profile_ClientMove(const GameClient *cl, const PMoveCmd *cmd) {

  if (!G_Ai_Profile_Records(AI_PROFILE_PLAYERS)) {
    return;
  }

  const GameEntity *ent = cl->entity;

  if (cl->ai || cl->persistent.spectator || ent->dead) {
    return;
  }

  GameAiProfileClient *c = G_Ai_Profile_Client(cl);
  const int32_t node = G_Ai_Profile_Visit(cl, c);

  G_Ai_Profile_Print(cl->ps.client, "CMD", "node=%d pos=%d,%d,%d vel=%d,%d,%d angles=%.1f,%.1f "
                     "move=%d,%d,%d msec=%d ground=%d water=%d buttons=%d", node,
                     AI_PROFILE_VEC3(ent->s.origin), AI_PROFILE_VEC3(ent->velocity), cl->angles.y,
                     cl->angles.x, cmd->forward, cmd->right, cmd->up, cmd->msec,
                     ent->ground.ent != NULL, ent->waterLevel, cmd->buttons);
}

/**
 * @see g_ai_profile.h
 */
void G_Ai_Profile_Killed(const GameClient *cl, const GameEntity *attacker, const uint32_t mod) {

  if (!G_Ai_Profile_Records(AI_PROFILE_BOTS | AI_PROFILE_PLAYERS)) {
    return;
  }

  const GameEntity *ent = cl->entity;
  const GameClient *killer = attacker && attacker->client ? attacker->client : NULL;

  G_Ai_Profile_Print(cl->ps.client, "KILL", "attacker=%d mod=%u weapon=%s range=%d pos=%d,%d,%d "
                     "bot=%d attackerBot=%d", killer ? killer->ps.client : -1, mod,
                     killer && killer->weapon ? killer->weapon->def.classname : "none",
                     killer ? (int32_t) Vec3_Distance(ent->s.origin, killer->entity->s.origin) : -1,
                     AI_PROFILE_VEC3(ent->s.origin), cl->ai != NULL, killer && killer->ai);

  GameAiProfileClient *c = G_Ai_Profile_Client(cl);
  c->visitNode = -1;

  if (!cl->ai || !G_Ai_Profile_Records(AI_PROFILE_BOTS)) {
    return;
  }

  int32_t from, to, travelType;
  G_Ai_Profile_Link(cl, &from, &to, &travelType);

  G_Ai_Profile_Print(cl->ps.client, "DEATH", "pos=%d,%d,%d water=%d link=%d->%d type=%d goal=%s",
                     AI_PROFILE_VEC3(ent->s.origin), ent->waterLevel, from, to, travelType,
                     G_Ai_Profile_Objective(cl));
  G_Ai_Profile_History(cl, "death");

  c->numPositions = 0;
  c->jumping = c->pushed = false;
}

/**
 * @see g_ai_profile.h
 */
void G_Ai_Profile_Flag(const GameClient *cl, const char *event, const int32_t team) {

  const int32_t flags = cl ? (cl->ai ? AI_PROFILE_BOTS : AI_PROFILE_PLAYERS)
                           : AI_PROFILE_BOTS | AI_PROFILE_PLAYERS;

  if (!G_Ai_Profile_Records(flags)) {
    return;
  }

  const int32_t carrierTeam = cl && cl->persistent.team ? cl->persistent.team->id : -1;

  G_Ai_Profile_Print(cl ? cl->ps.client : -1, "FLAG", "event=%s flag=%d team=%d bot=%d", event,
                     team, carrierTeam, cl && cl->ai != NULL);
}

/**
 * @see g_ai_profile.h
 */
void G_Ai_Profile_Points(const GameClient *cl, const char *event, const int32_t points) {

  if (!G_Ai_Profile_Records(cl->ai ? AI_PROFILE_BOTS : AI_PROFILE_PLAYERS)) {
    return;
  }

  G_Ai_Profile_Print(cl->ps.client, "POINTS", "event=%s points=%d team=%d bot=%d", event, points,
                     cl->persistent.team ? cl->persistent.team->id : -1, cl->ai != NULL);
}

/**
 * @see g_ai_profile.h
 */
void G_Ai_Profile_Task(const GameClient *cl, const GameAiNodeId node, const char *reason) {

  if (!G_Ai_Profile_Records(AI_PROFILE_BOTS)) {
    return;
  }

  const GameAiTask *task = &cl->ai->task;

  G_Ai_Profile_Print(cl->ps.client, "TASK", "reason=%s task=%s ent=%s node=%d pos=%d,%d,%d "
                     "urgent=%d radius=%d", reason, task->name ?: "none",
                     task->ent ? task->ent->classname : "none", (int32_t) node,
                     AI_PROFILE_VEC3(cl->entity->s.origin), task->urgent, (int32_t) task->radius);
}

/**
 * @see g_ai_profile.h
 */
void G_Ai_Profile_PadLink(const GameEntity *push, const char *result, const int32_t from,
                          const int32_t to, const Vec3 landing) {

  if (!G_Ai_Profile_Records(AI_PROFILE_GRAPH)) {
    return;
  }

  G_Ai_Profile_Print(-1, "PADLINK", "result=%s center=%d,%d,%d dir=%.2f,%.2f,%.2f speed=%d "
                     "from=%d to=%d landing=%d,%d,%d", result,
                     AI_PROFILE_VEC3(Box3_Center(push->absBounds)), push->moveDir.x,
                     push->moveDir.y, push->moveDir.z, (int32_t) push->speed, from, to,
                     AI_PROFILE_VEC3(landing));
}
