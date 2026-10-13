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
#define CG_NAV_DRAW_DISTANCE 1024.f

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
 * @return The color of a link of the given travel type.
 */
static Color Cg_AiLinkColor(const NavTravelType type) {

  switch (type) {
    case NAV_TRAVEL_CROUCH:
      return ColorHSV(COLOR_HUE_YELLOW, 1.f, 1.f);
    case NAV_TRAVEL_WALK_OFF_LEDGE:
      return ColorHSV(COLOR_HUE_RED, 1.f, 1.f);
    case NAV_TRAVEL_BARRIER_JUMP:
      return ColorHSV(COLOR_HUE_ORANGE, 1.f, 1.f);
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

    const Vec3 color = (node->crouch ? ColorHSV(COLOR_HUE_YELLOW, 1.f, 1.f) : ColorHSV(COLOR_HUE_ORANGE, 1.f, 1.f)).vec3;

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
