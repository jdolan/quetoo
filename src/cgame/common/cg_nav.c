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


#include "cg_local.h"

/**
 * @brief The distance from the view within which nodes and links are drawn.
 */
#define CG_NAV_DRAW_DISTANCE 768.f

/**
 * @brief The most points of a jump pad's flight that the client game keeps: one each server frame
 * of `NAV_FLIGHT_MILLIS`.
 */
#define CG_NAV_FLIGHT_POINTS (NAV_FLIGHT_MILLIS / QUETOO_TICK_MILLIS + 1)

/**
 * @brief The points of a jump pad's flight between two segments of its drawn arc: 100 ms.
 */
#define CG_NAV_FLIGHT_STRIDE 4

/**
 * @brief The interval in milliseconds at which the nodes on models that move near the view are
 * linked again, as the game links them every `AI_MOVER_LINK_MILLIS`.
 */
#define CG_NAV_MOVER_LINK_MILLIS 100

/**
 * @brief A link of a node on a model that moves, which `Nav_MoverLink` or `Nav_MoverWalk` found.
 */
typedef struct {
  /**
   * @brief The node that the link leaves.
   */
  int32_t from;

  /**
   * @brief The node that the link leads to.
   */
  int32_t to;

  /**
   * @brief `NAV_TRAVEL_WALK`, drawn once for both ways, or `NAV_TRAVEL_WALK_OFF_LEDGE`.
   */
  NavTravelType travelType;
} CGameNavMoverLink;

/**
 * @brief The flight of a player on one of the level's jump pads, as `Nav_FlyPush` flies it.
 */
typedef struct {
  /**
   * @brief The player origin at each server frame of the flight, from the pad.
   */
  Vec3 points[CG_NAV_FLIGHT_POINTS];
  int32_t numPoints;

  /**
   * @brief True if the flight lands within `NAV_FLIGHT_SECONDS`. A vertical pad's flight falls back
   * onto it, and does not, nor does one that falls into the void.
   */
  bool lands;
} CGameNavFlight;

/**
 * @brief The navigation graph that the client game draws.
 */
static struct {
  /**
   * @brief The graph, if it is built.
   */
  NavGraph graph;

  /**
   * @brief True if the graph is built for this level.
   */
  bool built;

  /**
   * @brief The movement for which `Nav_Build` made the graph.
   */
  PMoveParams movement;

  /**
   * @brief The entity number of each inline model that moves nodes, or 0 if it is not yet found.
   */
  int32_t movers[MAX_MODELS];

  /**
   * @brief The frame in which `Cg_NavMover` last searched for each inline model's entity, so that
   * a model that is not in the frame is searched for once a frame, not once a node.
   */
  int32_t moverFrames[MAX_MODELS];

  /**
   * @brief The flight of each of the graph's pushes.
   */
  CGameNavFlight *flights;

  /**
   * @brief The links of the nodes on models that move near the view, from `Cg_LinkNavMovers`.
   */
  CGameNavMoverLink *moverLinks;

  /**
   * @brief The count of `moverLinks`.
   */
  int32_t numMoverLinks;

  /**
   * @brief The capacity of `moverLinks`.
   */
  int32_t moverLinksCapacity;

  /**
   * @brief The client time at which `Cg_LinkNavMovers` next links the movers.
   */
  uint32_t moverLinkTime;
} module;

/**
 * @see cg_nav.h
 */
void Cg_FreeNav(void) {

  Nav_Free(&module.graph);

  free(module.flights);
  module.flights = NULL;

  free(module.moverLinks);
  module.moverLinks = NULL;
  module.numMoverLinks = module.moverLinksCapacity = 0;
  module.moverLinkTime = 0;

  memset(module.movers, 0, sizeof(module.movers));
  memset(module.moverFrames, 0, sizeof(module.moverFrames));

  module.built = false;
}

/**
 * @return The entity of the inline model in the current frame, or NULL. A model whose entity is
 * not in the frame is searched for at most once a frame.
 */
static const ClientEntity *Cg_NavMover(const int32_t model) {

  if (model <= 0 || model >= MAX_MODELS) {
    return NULL;
  }

  const int32_t frameNum = cgi.client->frame.frameNum;
  const char *name = va("*%d", model);

  const ClientEntity *ent = &cgi.client->entities[module.movers[model]];
  if (module.movers[model] && ent->frameNum == frameNum && ent->current.model1 &&
      !Str_Compare(cgi.ConfigString(CS_MODELS + ent->current.model1), name)) {
    return ent;
  }

  if (module.moverFrames[model] == frameNum) {
    return NULL;
  }

  module.moverFrames[model] = frameNum;
  module.movers[model] = 0;

  for (int32_t i = 1; i < MAX_ENTITIES; i++) {
    ent = &cgi.client->entities[i];

    if (ent->frameNum != frameNum || !ent->current.model1) {
      continue;
    }

    if (!Str_Compare(cgi.ConfigString(CS_MODELS + ent->current.model1), name)) {
      module.movers[model] = i;
      return ent;
    }
  }

  return NULL;
}

/**
 * @return The position of the node: where it stands, or for a node on a model that moves, where the
 * model carries it now.
 */
static Vec3 Cg_NavNodePosition(const NavNode *node) {

  if (node->model) {
    const ClientEntity *ent = Cg_NavMover(node->model);
    if (ent) {
      return Mat4_Transform(Mat4_FromRotationTranslationScale(ent->angles, ent->origin, 1.f),
                            node->local);
    }
  }

  return node->position;
}

/**
 * @see cg_nav.h
 */
void Cg_DescribeNavNode(const Vec3 position, CGameNavRowFunc row, void *data) {

  if (!module.built) {
    row(data, "nav node", "none: set cg_drawNav 1 to build the graph");
    return;
  }

  const NavGraph *graph = &module.graph;

  int32_t nearest = -1;
  float distance = FLT_MAX;

  for (int32_t i = 0; i < graph->numNodes; i++) {
    const float d = Vec3_Distance(Cg_NavNodePosition(&graph->nodes[i]), position);
    if (d < distance) {
      distance = d;
      nearest = i;
    }
  }

  if (nearest == -1) {
    row(data, "nav node", "none: the level has no nodes");
    return;
  }

  const NavNode *node = &graph->nodes[nearest];
  const Vec3 origin = Cg_NavNodePosition(node);

  row(data, "nav node",
      va("^5%d^7 at %d %d %d, %d away%s%s%s%s%s", nearest, (int32_t) origin.x, (int32_t) origin.y,
         (int32_t) origin.z, (int32_t) distance, node->crouch ? ", crouch" : "",
         (node->contents & CONTENTS_WATER) ? ", water" : "", node->swim ? ", swim" : "",
         node->edge ? ", edge" : "", node->model ? va(", on *%d", node->model) : ""));

  for (int32_t i = 0; i < node->numLinks; i++) {
    const NavLink *link = &graph->links[node->firstLink + i];
    const Vec3 to = Cg_NavNodePosition(&graph->nodes[link->node]);

    row(data, va("  %s", Nav_TravelName(link->travelType)),
        va("%d at %d %d %d", link->node, (int32_t) to.x, (int32_t) to.y, (int32_t) to.z));
  }

  for (int32_t i = 0; i < graph->numPushes; i++) {
    const CGameNavFlight *flight = &module.flights[i];

    if (flight->numPoints < 2 || Vec3_Distance(flight->points[0], origin) > NAV_CELL_SIZE * 2.f) {
      continue;
    }

    const Vec3 end = flight->points[flight->numPoints - 1];

    row(data, "  flight",
        flight->lands ? va("lands at %d %d %d", (int32_t) end.x, (int32_t) end.y, (int32_t) end.z) :
        "does not land");
  }
}

/**
 * @brief Prints one row of `Cg_DescribeNavNode` to the console.
 */
static void Cg_NavInfo_row(void *data, const char *name, const char *value) {
  cgi.Print("%s: %s\n", name, value);
}

/**
 * @brief Prints the view's position, and the node nearest it with its links.
 */
static void Cg_NavInfo_f(void) {

  const Vec3 view = cgi.view->origin;

  cgi.Print("View at %d %d %d\n", (int32_t) view.x, (int32_t) view.y, (int32_t) view.z);

  Cg_DescribeNavNode(view, Cg_NavInfo_row, NULL);
}

/**
 * @see cg_nav.h
 */
void Cg_InitNav(void) {

  cgi.AddCmd("navInfo", Cg_NavInfo_f, CMD_CGAME, "Prints the navigation node nearest the view, "
             "with its links");
}

/**
 * @return A bot's trace against the world only, which monster clip blocks, as the game flies
 * jump pads.
 */
static CollisionTrace Cg_NavFlightTrace(const Vec3 start, const Vec3 end, const Box3 bounds) {
  return cgi.BoxTrace(start, end, bounds, 0, CONTENTS_MASK_CLIP_MONSTER);
}

/**
 * @brief Flies each of the graph's pushes, as the game does to link them: from the floor under the
 * center of the pad, with the level's movement and no input.
 */
static void Cg_PrepareNavFlights(const ClientFrame *frame) {

  const NavGraph *graph = &module.graph;

  module.flights = calloc(Maxi(1, graph->numPushes), sizeof(CGameNavFlight));
  if (!module.flights) {
    cgi.Error(__func__, "Failed to allocate %d flights\n", graph->numPushes);
  }

  for (int32_t i = 0; i < graph->numPushes; i++) {
    const NavPush *push = &graph->pushes[i];
    CGameNavFlight *flight = &module.flights[i];

    if (push->startOff || Vec3_Equal(push->velocity, Vec3_Zero())) {
      continue;
    }

    const Vec3 center = Box3_Center(push->bounds);
    const Vec3 top = MakeVec3(center.x, center.y, push->bounds.maxs.z - PM_BOUNDS.mins.z);
    const Vec3 bottom = MakeVec3(center.x, center.y,
                                 push->bounds.mins.z - PM_BOUNDS.mins.z - PM_STEP_HEIGHT * 4.f);

    const CollisionTrace drop = Cg_NavFlightTrace(top, bottom, PM_BOUNDS);
    if (drop.startSolid || drop.fraction == 1.f) {
      continue;
    }

    PMove pm;
    memset(&pm, 0, sizeof(pm));

    pm.s.type = PM_NORMAL;
    pm.s.origin = drop.end;
    pm.s.params = frame->ps.pmState.params;

    pm.PointContents = cgi.PointContents;
    pm.BoxContents = cgi.BoxContents;
    pm.Trace = Cg_NavFlightTrace;

    pm.Debug = cgi.Debug;
    pm.DebugMask = cgi.DebugMask;
    pm.debugMask = DEBUG_PMOVE_CLIENT;

    float seconds;
    flight->lands = Nav_FlyPush(push, &pm, flight->points, CG_NAV_FLIGHT_POINTS, &flight->numPoints,
                                &seconds);
  }
}

/**
 * @see cg_nav.h
 */
void Cg_PrepareNav(const ClientFrame *frame) {

  if (!cg_drawNav->integer) {
    return;
  }

  if (Box3_Equal(frame->ps.pmState.params.bounds, Box3_Zero())) {
    return;
  }

  if (module.built) {
    if (Nav_SameMovement(&module.movement, &frame->ps.pmState.params)) {
      return;
    }
    Cg_FreeNav();
  }

  module.movement = frame->ps.pmState.params;

  const NavParams params = {
    .bsp = cgi.Bsp(),
    .movement = frame->ps.pmState.params,
    .BoxTrace = cgi.BoxTrace,
    .PointInsideBrush = cgi.PointInsideBrush,
    .EntityValue = cgi.EntityValue,
    .Warn = cgi.Warn,
    .Error = cgi.Error
  };

  if (!Nav_Build(&params, &module.graph)) {
    Cg_Warn("This map has no floor to navigate\n");
  } else {
    Cg_PrepareNavFlights(frame);
    cgi.Print("Built %d navigation nodes, %d links and %d jump pad flights\n",
              module.graph.numNodes, module.graph.numLinks, module.graph.numPushes);
  }

  module.built = true;
}

/**
 * @return The color of a link of the given travel type, by the kind of movement: green to walk, red
 * to crouch, as crouch nodes are, grey to fall, yellow and orange to jump, cyan to swim, and violet
 * to climb.
 */
static Color Cg_NavLinkColor(const NavTravelType type) {

  switch (type) {
    case NAV_TRAVEL_CROUCH:
      return ColorHSV(COLOR_HUE_RED, 1.f, 1.f);
    case NAV_TRAVEL_WALK_OFF_LEDGE:
      return ColorHSV(0.f, 0.f, .5f);
    case NAV_TRAVEL_BARRIER_JUMP:
      return ColorHSV(COLOR_HUE_YELLOW, 1.f, 1.f);
    case NAV_TRAVEL_JUMP:
      return ColorHSV(COLOR_HUE_ORANGE, 1.f, 1.f);
    case NAV_TRAVEL_SWIM:
      return ColorHSV(COLOR_HUE_CYAN, 1.f, 1.f);
    case NAV_TRAVEL_LADDER:
      return ColorHSV(COLOR_HUE_VIOLET, 1.f, 1.f);
    default:
      return ColorHSV(COLOR_HUE_GREEN, .6f, .8f);
  }
}

/**
 * @return True if `link`, out of node `from`, has a link back of the same type, which is drawn
 * with the other node instead.
 */
static bool Cg_NavLinkIsReturn(const int32_t from, const NavLink *link) {

  if (link->node > from) {
    return false;
  }

  const NavNode *node = &module.graph.nodes[link->node];

  for (int32_t i = 0; i < node->numLinks; i++) {
    const NavLink *back = &module.graph.links[node->firstLink + i];
    if (back->node == from && back->travelType == link->travelType) {
      return true;
    }
  }

  return false;
}

/**
 * @brief Draws the flights of the jump pads near the view: a blue arc from the pad, with a pulsing
 * blue marker at the pad and at the landing. A vertical pad's flight rises and falls back onto it,
 * so its arc is its column, up to its apex. A flight is drawn if its pad or its landing is near.
 */
static void Cg_AddNavFlights(void) {

  const Vec3 view = cgi.view->origin;
  const Vec3 color = ColorHSV(COLOR_HUE_BLUE, .8f, 1.f).vec3;
  const float pulse = 6.f + 4.f * sinf(cgi.client->unclampedTime * .008f);

  for (int32_t i = 0; i < module.graph.numPushes; i++) {
    const CGameNavFlight *flight = &module.flights[i];

    if (flight->numPoints < 2) {
      continue;
    }

    const Vec3 start = flight->points[0];
    const Vec3 end = flight->points[flight->numPoints - 1];

    if (Vec3_Distance(start, view) > CG_NAV_DRAW_DISTANCE &&
        Vec3_Distance(end, view) > CG_NAV_DRAW_DISTANCE) {
      continue;
    }

    for (int32_t j = 0; j < flight->numPoints - 1; j += CG_NAV_FLIGHT_STRIDE) {
      Cg_AddSprite(&(CGameSprite) {
        .type = SPRITE_BEAM,
        .image = cgameMedia.beams.line,
        .origin = flight->points[j],
        .termination = flight->points[Mini(j + CG_NAV_FLIGHT_STRIDE, flight->numPoints - 1)],
        .size = 2.f,
        .color = color,
      });
    }

    Cg_AddSprite(&(CGameSprite) {
      .atlasImage = cgameMedia.sprites.particle,
      .origin = start,
      .size = pulse,
      .color = color,
    });

    if (flight->lands) {
      Cg_AddSprite(&(CGameSprite) {
        .atlasImage = cgameMedia.sprites.particle,
        .origin = end,
        .size = pulse,
        .color = color,
      });
    }
  }
}

/**
 * @return A trace for the links of movers (`NavMoverTrace`), against the world and the entities.
 */
static CollisionTrace Cg_NavMoverTrace(const Vec3 start, const Vec3 end, const Box3 bounds,
                                       const int32_t contents) {
  return cgi.Trace(start, end, bounds, NULL, contents);
}

/**
 * @brief Adds a link to `moverLinks`.
 */
static void Cg_LinkNavMovers_add(const int32_t from, const int32_t to, const NavTravelType type) {

  if (module.numMoverLinks == module.moverLinksCapacity) {
    module.moverLinksCapacity = Maxi(64, module.moverLinksCapacity * 2);
    module.moverLinks = realloc(module.moverLinks,
                                sizeof(CGameNavMoverLink) * module.moverLinksCapacity);
    if (!module.moverLinks) {
      Cg_Error("Failed to allocate %d mover links\n", module.moverLinksCapacity);
    }
  }

  module.moverLinks[module.numMoverLinks++] = (CGameNavMoverLink) {
    .from = from,
    .to = to,
    .travelType = type
  };
}

/**
 * @return True if the point is in lava or slime, where the game links no node that a model carried.
 */
static bool Cg_LinkNavMovers_hazard(const Vec3 point) {
  return (cgi.PointContents(point) & (CONTENTS_LAVA | CONTENTS_SLIME)) != 0;
}

/**
 * @brief Links each node on a model that moves, near the view, as the game links it: to the nodes
 * that do not move in the cells around it (`Nav_MoverLink`), and with a walk to the nodes on other
 * models that move (`Nav_MoverWalk`). A node that its model carried into lava or slime is not
 * linked. The links are found again every `CG_NAV_MOVER_LINK_MILLIS`. The client game does not
 * link the nodes that the game adds, such as items', or the press links of the bots' gates.
 */
static void Cg_LinkNavMovers(void) {

  if (module.moverLinkTime > cgi.client->unclampedTime) {
    return;
  }

  module.moverLinkTime = cgi.client->unclampedTime + CG_NAV_MOVER_LINK_MILLIS;
  module.numMoverLinks = 0;

  const NavGraph *graph = &module.graph;
  const Vec3 view = cgi.view->origin;
  const Box3 bounds = module.movement.bounds;
  const int32_t levels = (int32_t) ceilf(NAV_MOVER_DROP_HEIGHT / NAV_CELL_HEIGHT);

  for (int32_t i = 0; i < graph->numNodes; i++) {
    const NavNode *node = &graph->nodes[i];

    if (!node->model) {
      continue;
    }

    const Vec3 position = Cg_NavNodePosition(node);

    if (Vec3_Distance(position, view) > CG_NAV_DRAW_DISTANCE + NAV_MOVER_LINK_DISTANCE ||
        Cg_LinkNavMovers_hazard(position)) {
      continue;
    }

    const Vec3i xyz = Nav_PointCoordinate(graph, position);

    for (int32_t dz = -levels; dz <= levels; dz++) {
      for (int32_t dy = -1; dy <= 1; dy++) {
        for (int32_t dx = -1; dx <= 1; dx++) {

          const int32_t other = Nav_CellNode(graph, MakeVec3i(xyz.x + dx, xyz.y + dy, xyz.z + dz));
          if (other == -1 || graph->nodes[other].model) {
            continue;
          }

          const Vec3 target = graph->nodes[other].position;

          float cost;
          switch (Nav_MoverLink(Cg_NavMoverTrace, position, target, bounds, &cost)) {
            case NAV_TRAVEL_WALK:
              Cg_LinkNavMovers_add(i, other, NAV_TRAVEL_WALK);
              break;
            case NAV_TRAVEL_WALK_OFF_LEDGE:
              if (target.z > position.z) {
                Cg_LinkNavMovers_add(other, i, NAV_TRAVEL_WALK_OFF_LEDGE);
              } else {
                Cg_LinkNavMovers_add(i, other, NAV_TRAVEL_WALK_OFF_LEDGE);
              }
              break;
            default:
              break;
          }
        }
      }
    }

    for (int32_t j = i + 1; j < graph->numNodes; j++) {
      const NavNode *next = &graph->nodes[j];

      if (!next->model || next->model == node->model) {
        continue;
      }

      const Vec3 target = Cg_NavNodePosition(next);

      float distance;
      if (!Nav_MoverReach(position, target, &distance) || Cg_LinkNavMovers_hazard(target)) {
        continue;
      }

      if (Nav_MoverWalk(Cg_NavMoverTrace, position, target, bounds, &distance)) {
        Cg_LinkNavMovers_add(i, j, NAV_TRAVEL_WALK);
      }
    }
  }
}

/**
 * @brief Draws the links of the nodes on models that move, near the view, from `Cg_LinkNavMovers`.
 */
static void Cg_AddNavMoverLinks(void) {

  Cg_LinkNavMovers();

  const Vec3 view = cgi.view->origin;

  for (int32_t i = 0; i < module.numMoverLinks; i++) {
    const CGameNavMoverLink *link = &module.moverLinks[i];

    const Vec3 from = Cg_NavNodePosition(&module.graph.nodes[link->from]);
    const Vec3 to = Cg_NavNodePosition(&module.graph.nodes[link->to]);

    if (Vec3_Distance(from, view) > CG_NAV_DRAW_DISTANCE &&
        Vec3_Distance(to, view) > CG_NAV_DRAW_DISTANCE) {
      continue;
    }

    Cg_AddSprite(&(CGameSprite) {
      .type = SPRITE_BEAM,
      .image = cgameMedia.beams.line,
      .origin = from,
      .termination = to,
      .size = 1.5f,
      .color = Cg_NavLinkColor(link->travelType).vec3,
    });
  }
}

/**
 * @see cg_nav.h
 */
void Cg_AddNav(const ClientFrame *frame) {

  if (!cg_drawNav->integer || !module.built) {
    return;
  }

  const NavGraph *graph = &module.graph;
  const Vec3 view = cgi.view->origin;

  Cg_AddNavFlights();
  Cg_AddNavMoverLinks();

  for (int32_t i = 0; i < graph->numNodes; i++) {
    const NavNode *node = &graph->nodes[i];

    const Vec3 position = Cg_NavNodePosition(node);

    if (Vec3_Distance(position, view) > CG_NAV_DRAW_DISTANCE) {
      continue;
    }

    const Vec3 color = (node->crouch ? ColorHSV(COLOR_HUE_RED, 1.f, 1.f) :
                        ColorHSV(0.f, 0.f, 1.f)).vec3;

    Cg_AddSprite(&(CGameSprite) {
      .atlasImage = cgameMedia.sprites.particle,
      .origin = position,
      .size = 6.f,
      .color = color,
    });

    for (int32_t j = 0; j < node->numLinks; j++) {
      const NavLink *link = &graph->links[node->firstLink + j];

      if (Cg_NavLinkIsReturn(i, link)) {
        continue;
      }

      Cg_AddSprite(&(CGameSprite) {
        .type = SPRITE_BEAM,
        .image = cgameMedia.beams.line,
        .origin = position,
        .termination = Cg_NavNodePosition(&graph->nodes[link->node]),
        .size = 1.5f,
        .color = Cg_NavLinkColor(link->travelType).vec3,
      });
    }
  }
}
