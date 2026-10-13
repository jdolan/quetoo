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
#define CG_NAV_DRAW_DISTANCE 512.f

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
} module;

/**
 * @see cg_nav.h
 */
void Cg_FreeAiNodes(void) {

  Nav_Free(&module.graph);

  module.built = false;
}

/**
 * @return The name of the travel type, as `aiNodeInfo` prints it.
 */
static const char *Cg_AiTravelName(const NavTravelType type) {

  switch (type) {
    case NAV_TRAVEL_WALK:
      return "walk";
    case NAV_TRAVEL_CROUCH:
      return "crouch";
    case NAV_TRAVEL_WALK_OFF_LEDGE:
      return "walk off ledge";
    case NAV_TRAVEL_BARRIER_JUMP:
      return "barrier jump";
    case NAV_TRAVEL_TELEPORT:
      return "teleport";
    case NAV_TRAVEL_JUMP_PAD:
      return "jump pad";
    case NAV_TRAVEL_ELEVATOR:
      return "elevator";
    case NAV_TRAVEL_SWIM:
      return "swim";
    case NAV_TRAVEL_JUMP:
      return "jump";
    default:
      return "none";
  }
}

/**
 * @brief Prints the view's position, and the node nearest it with its links.
 */
static void Cg_AiNodeInfo_f(void) {

  const Vec3 view = cgi.view->origin;

  cgi.Print("View at %d %d %d\n", (int32_t) view.x, (int32_t) view.y, (int32_t) view.z);

  if (!module.built) {
    cgi.Print("Set cg_drawAiNodes 1 to build the navigation graph\n");
    return;
  }

  const NavGraph *graph = &module.graph;

  int32_t nearest = -1;
  float distance = FLT_MAX;

  for (int32_t i = 0; i < graph->numNodes; i++) {
    const float d = Vec3_Distance(graph->nodes[i].position, view);
    if (d < distance) {
      distance = d;
      nearest = i;
    }
  }

  if (nearest == -1) {
    cgi.Print("No navigation nodes\n");
    return;
  }

  const NavNode *node = &graph->nodes[nearest];

  cgi.Print("Node %d at %d %d %d, %d units away%s%s, %d links:\n", nearest,
            (int32_t) node->position.x, (int32_t) node->position.y, (int32_t) node->position.z,
            (int32_t) distance, node->crouch ? ", crouch" : "",
            (node->contents & CONTENTS_WATER) ? ", water" : "", node->numLinks);

  for (int32_t i = 0; i < node->numLinks; i++) {
    const NavLink *link = &graph->links[node->firstLink + i];
    const Vec3 position = graph->nodes[link->node].position;

    cgi.Print("  %s to node %d at %d %d %d\n", Cg_AiTravelName(link->travelType), link->node,
              (int32_t) position.x, (int32_t) position.y, (int32_t) position.z);
  }
}

/**
 * @see cg_nav.h
 */
void Cg_InitAiNodes(void) {

  cgi.AddCmd("aiNodeInfo", Cg_AiNodeInfo_f, CMD_CGAME, "Prints the navigation node nearest the view, with its links");
}

/**
 * @see cg_nav.h
 */
void Cg_PrepareAiNodes(const ClientFrame *frame) {

  if (!cg_drawAiNodes->integer || module.built) {
    return;
  }

  const int16_t gravity = frame->ps.pmState.params.gravity;

  const NavParams params = {
    .bsp = cgi.Bsp(),
    .gravity = gravity > 0 ? gravity : 800.f,
    .BoxTrace = cgi.BoxTrace,
    .PointInsideBrush = cgi.PointInsideBrush,
    .EntityValue = cgi.EntityValue,
    .Warn = cgi.Warn,
    .Error = cgi.Error
  };

  if (!Nav_Build(&params, &module.graph)) {
    Cg_Warn("This map has no floor to navigate\n");
  } else {
    cgi.Print("Built %d navigation nodes and %d links\n", module.graph.numNodes, module.graph.numLinks);
  }

  module.built = true;
}

/**
 * @return The color of a link of the given travel type, by the kind of movement: green to walk, red
 * to crouch, as crouch nodes are, grey to fall, yellow and orange to jump, and cyan to swim.
 */
static Color Cg_AiLinkColor(const NavTravelType type) {

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
    default:
      return ColorHSV(COLOR_HUE_GREEN, .6f, .8f);
  }
}

/**
 * @return True if `link`, out of node `from`, has a link back of the same type, which is drawn
 * with the other node instead.
 */
static bool Cg_AiLinkIsReturn(const int32_t from, const NavLink *link) {

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
 * @see cg_nav.h
 */
void Cg_DrawAiNodes(const ClientFrame *frame) {

  if (!cg_drawAiNodes->integer || !module.built) {
    return;
  }

  const NavGraph *graph = &module.graph;
  const Vec3 view = cgi.view->origin;

  for (int32_t i = 0; i < graph->numNodes; i++) {
    const NavNode *node = &graph->nodes[i];

    if (Vec3_Distance(node->position, view) > CG_NAV_DRAW_DISTANCE) {
      continue;
    }

    const Vec3 color = (node->crouch ? ColorHSV(COLOR_HUE_RED, 1.f, 1.f) : ColorHSV(0.f, 0.f, 1.f)).vec3;

    Cg_AddSprite(&(CGameSprite) {
      .atlasImage = cgameMedia.sprites.particle,
      .origin = node->position,
      .size = 6.f,
      .color = color,
    });

    for (int32_t j = 0; j < node->numLinks; j++) {
      const NavLink *link = &graph->links[node->firstLink + j];

      if (Cg_AiLinkIsReturn(i, link)) {
        continue;
      }

      Cg_AddSprite(&(CGameSprite) {
        .type = SPRITE_BEAM,
        .image = cgameMedia.beams.line,
        .origin = node->position,
        .termination = graph->nodes[link->node].position,
        .size = 1.5f,
        .color = Cg_AiLinkColor(link->travelType).vec3,
      });
    }
  }
}
