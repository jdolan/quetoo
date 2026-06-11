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
 * Foundation, Inc., 59 Temple Place - Suite 330, Boston, MA  02111-1307, USA.
 */

#include "g_local.h"
#include "bg_pmove.h"

/**
 * @brief Cached spatial acceleration structures for the navigation graph.
 *
 * The kd-tree accelerates G_Ai_Node_FindClosest queries (O(log n) average vs.
 * the previous O(n) linear scan), and is rebuilt lazily after any mutation
 * of the global node array.
 */
static struct gridkdtree_s *g_ai_nodes_kdtree = NULL;
static vec3_t *g_ai_nodes_kdtree_positions = NULL;
static guint g_ai_nodes_kdtree_count = 0;

/**
 * @brief Invalidates the cached kd-tree. Must be called whenever the
 * underlying g_ai_nodes array is mutated (add/remove/move/reload).
 */
static void G_Ai_Node_InvalidateSpatialIndex(void) {
  if (g_ai_nodes_kdtree) {
    gridkdtree_free(&g_ai_nodes_kdtree);
  }
  if (g_ai_nodes_kdtree_positions) {
    free(g_ai_nodes_kdtree_positions);
    g_ai_nodes_kdtree_positions = NULL;
  }
  g_ai_nodes_kdtree_count = 0;
}

/**
 * @brief State used during player roam recording for AI node development.
 */
static struct {
  vec3_t position;

  vec3_t floor_position;

  ai_node_id_t last_nodes[2];

  bool await_landing, is_jumping, is_water_jump, on_mover;

  button_state_t latched_buttons, old_buttons, buttons;

  GArray *test_path;

  guint file_nodes, file_links;

  bool do_noding, drop_nodes;
} g_ai_player_roam;

/**
 * @brief Returns a test path between the last two recorded nodes for visualization.
 */
GArray *G_Ai_Node_TestPath(void) {

  if (!g_ai_node_dev->integer) {
    return NULL;
  }

  if (g_ai_player_roam.test_path) {
    g_array_unref(g_ai_player_roam.test_path);
    g_ai_player_roam.test_path = NULL;
  }

  if (g_ai_player_roam.last_nodes[0] == AI_NODE_INVALID ||
    g_ai_player_roam.last_nodes[1] == AI_NODE_INVALID) {
    return NULL;
  }

  return g_ai_player_roam.test_path = G_Ai_Node_FindPath(NULL, g_ai_player_roam.last_nodes[1], g_ai_player_roam.last_nodes[0], G_Ai_Node_Heuristic, NULL);
}

/**
 * @brief An AI navigation node with world position, outgoing links, and pathfinding state.
 */
typedef struct {
  // persisted to disk

  vec3_t position;
  GArray *links;

  // only used for nav purposes

  float cost;
  ai_node_id_t came_from;
} ai_node_t;

/**
 * @brief The global array of navigation nodes for the current map.
 */
static GArray *g_ai_nodes;

/**
 * @brief The global array of platforms for the current map.
 */
static GArray *g_ai_platforms;

/**
 * @brief Returns the index of a node pointer within the global node array.
 */
static inline ai_node_id_t G_Ai_Node_Index(const ai_node_t *node) {
  return node - (ai_node_t *) g_ai_nodes->data;
}

static void G_Ai_Node_FreePathPool(void);

/**
 * @brief Returns true if the given node is visible (unobstructed) from the specified position.
 */
static bool G_Ai_Node_Visible(const vec3_t position, const ai_node_id_t node) {

  return gi.Trace(position, G_Ai_Node_GetPosition(node), Box3_Zero(), NULL, CONTENTS_SOLID | CONTENTS_WINDOW).fraction == 1.0f;
}

/**
 * @brief Returns the total number of navigation nodes currently loaded.
 */
guint G_Ai_Node_Count(void) {

  return g_ai_nodes ? g_ai_nodes->len : 0;
}

/**
 * @brief Lazily (re)builds the kd-tree spatial index over g_ai_nodes.
 * Returns true on success. The index is invalidated whenever the node
 * array is mutated (see G_Ai_Node_InvalidateSpatialIndex callers).
 */
static bool G_Ai_Node_EnsureSpatialIndex(void) {

  if (!g_ai_nodes || g_ai_nodes->len == 0) {
    return false;
  }

  if (g_ai_nodes_kdtree && g_ai_nodes_kdtree_count == g_ai_nodes->len) {
    return true;
  }

  G_Ai_Node_InvalidateSpatialIndex();

  const guint n = g_ai_nodes->len;
  g_ai_nodes_kdtree_positions = malloc(sizeof(vec3_t) * n);
  if (!g_ai_nodes_kdtree_positions) {
    return false;
  }

  for (guint i = 0; i < n; i++) {
    g_ai_nodes_kdtree_positions[i] = g_array_index(g_ai_nodes, ai_node_t, i).position;
  }

  g_ai_nodes_kdtree = gridkdtree_create(g_ai_nodes_kdtree_positions, n);
  if (!g_ai_nodes_kdtree) {
    free(g_ai_nodes_kdtree_positions);
    g_ai_nodes_kdtree_positions = NULL;
    return false;
  }

  g_ai_nodes_kdtree_count = n;
  return true;
}

typedef struct {
  vec3_t position;
  bool only_visible;
  bool prefer_level;
} ai_node_query_filter_t;

static bool G_Ai_Node_FindClosestFilter(const size_t nodenum, void *data, float *distance) {
  const ai_node_query_filter_t *filter = data;
  const ai_node_t *node = &g_array_index(g_ai_nodes, ai_node_t, nodenum);

  vec3_t dir = Vec3_Subtract(filter->position, node->position);

  if (filter->prefer_level && !(gi.PointContents(node->position) & CONTENTS_MASK_LIQUID)) {
    dir.z *= 4.0f;
  }

  *distance = Vec3_LengthSquared(dir);

  return !filter->only_visible || G_Ai_Node_Visible(filter->position, nodenum);
}

/**
 * @brief Finds the closest navigation node to the given position within the specified distance.
 *
 * Uses the cached kd-tree spatial index (built lazily) to gather candidate
 * nodes within `max_distance` and selects the best one that passes the
 * `only_visible` / `prefer_level` filters. Falls back to a linear scan if
 * the spatial index is unavailable.
 */
ai_node_id_t G_Ai_Node_FindClosest(const vec3_t position, const float max_distance, const bool only_visible, const bool prefer_level) {

  if (!g_ai_nodes || g_ai_nodes->len == 0) {
    return AI_NODE_INVALID;
  }

  ai_node_id_t closest = AI_NODE_INVALID;
  float closest_dist = 0;
  const float dist_squared = max_distance * max_distance;

  if (G_Ai_Node_EnsureSpatialIndex()) {
    ai_node_query_filter_t filter = {
      .position = position,
      .only_visible = only_visible,
      .prefer_level = prefer_level
    };

    const size_t node = gridkdtree_query_filter(g_ai_nodes_kdtree, position, max_distance, G_Ai_Node_FindClosestFilter, &filter);

    return node == SIZE_MAX ? AI_NODE_INVALID : (ai_node_id_t) node;
  }

  // Fallback: linear scan (kd-tree build failed).
  for (guint i = 0; i < g_ai_nodes->len; i++) {
    const ai_node_t *node = &g_array_index(g_ai_nodes, ai_node_t, i);

    vec3_t dir = Vec3_Subtract(position, node->position);
    if (prefer_level && !(gi.PointContents(node->position) & CONTENTS_MASK_LIQUID)) {
      dir.z *= 4.0f;
    }
    const float dist = Vec3_LengthSquared(dir);

    if (dist < dist_squared && (closest == AI_NODE_INVALID || dist < closest_dist) && (!only_visible || G_Ai_Node_Visible(position, i))) {
      closest = i;
      closest_dist = dist;
    }
  }

  return closest;
}

/**
 * @brief Creates and appends a new navigation node at the specified world position.
 */
ai_node_id_t G_Ai_Node_Create(const vec3_t position) {

  if (gi.PointContents(position) & CONTENTS_MASK_SOLID) {
    G_Ai_Debug("Rejected node at %s (inside solid)\n", vtos(position));
    return AI_NODE_INVALID;
  }

  if (!g_ai_nodes) {
    g_ai_nodes = g_array_new(false, true, sizeof(ai_node_t));
  }

  g_ai_nodes = g_array_append_val(g_ai_nodes, (ai_node_t) {
    .position = position
  });

  G_Ai_Node_InvalidateSpatialIndex();

  G_Ai_Debug("Dropped new node %d\n", g_ai_nodes->len - 1);

  return g_ai_nodes->len - 1;
}

/**
 * @brief Returns true if node a has a directed link to node b.
 */
bool G_Ai_Node_IsLinked(const ai_node_id_t a, const ai_node_id_t b) {
  const ai_node_t *node_a = &g_array_index(g_ai_nodes, ai_node_t, a);

  if (node_a->links) {
    for (guint i = 0; i < node_a->links->len; i++) {
      const ai_link_t *link = &g_array_index(node_a->links, ai_link_t, i);

      if (link->id == b) {
        return true;
      }
    }
  }

  return false;
}

/**
 * @brief Returns the array of outgoing links for the specified node.
 */
const GArray *G_Ai_Node_GetLinks(const ai_node_id_t a) {
  const ai_node_t *node_a = &g_array_index(g_ai_nodes, ai_node_t, a);
  return node_a->links;
}

/**
 * @brief Creates a directed link from node a to node b with the given traversal cost.
 */
void G_Ai_Node_Link(const ai_node_id_t a, const ai_node_id_t b, const float cost) {

  if (!g_ai_nodes || a >= g_ai_nodes->len || b >= g_ai_nodes->len) {
    return;
  }

  if (a == b || G_Ai_Node_IsLinked(a, b)) {
    return;
  }

  ai_node_t *node_a = &g_array_index(g_ai_nodes, ai_node_t, a);

  if (!node_a->links) {
    node_a->links = g_array_new(false, false, sizeof(ai_link_t));
  }

  node_a->links = g_array_append_vals(node_a->links, &(ai_link_t) {
    .id = b,
    .cost = cost
  }, 1);

  G_Ai_Debug("Connected %d -> %d\n", a, b);
}

/**
 * @brief Creates a link between two nodes using their Euclidean distance as the cost.
 */
static inline void G_Ai_Node_LinkDefault(const ai_node_id_t a, const ai_node_id_t b, const bool bidirectional) {

  if (a == b) {
    return;
  }

  G_Ai_Node_Link(a, b, G_Ai_Node_Cost(a, b));

  if (bidirectional) {
    G_Ai_Node_LinkDefault(b, a, false);
  }
}

/**
 * @brief Removes the bidirectional link between two nodes.
 */
static void G_Ai_Node_Unlink(const ai_node_id_t a, const ai_node_id_t b) {
  {
    ai_node_t *node_a = &g_array_index(g_ai_nodes, ai_node_t, a);

    if (node_a->links) {
      for (guint i = 0; i < node_a->links->len; i++) {
        const ai_link_t *link = &g_array_index(node_a->links, ai_link_t, i);

        if (link->id == b) {
          node_a->links = g_array_remove_index_fast(node_a->links, i);

          if (!node_a->links->len) {
            g_array_free(node_a->links, true);
            node_a->links = NULL;
          }
          break;
        }
      }
    }
  }

  {
    ai_node_t *node_b = &g_array_index(g_ai_nodes, ai_node_t, b);
  
    if (node_b->links) {
      for (guint i = 0; i < node_b->links->len; i++) {
        const ai_link_t *link = &g_array_index(node_b->links, ai_link_t, i);

        if (link->id == a) {
          node_b->links = g_array_remove_index_fast(node_b->links, i);

          if (!node_b->links->len) {
            g_array_free(node_b->links, true);
            node_b->links = NULL;
          }
          break;
        }
      }
    }
  }
}

/**
 * @brief Removes all links connected to the specified node.
 */
static void G_Ai_Node_UnlinkAll(const ai_node_id_t id) {
  const ai_node_t *node = &g_array_index(g_ai_nodes, ai_node_t, id);

  if (!node->links) {
    return;
  }

  for (guint i = node->links->len - 1; ; i--) {
    G_Ai_Node_Unlink(id, g_array_index(node->links, ai_link_t, i).id);
    
    if (i == 0 || !node->links) {
      break;
    }
  }
}

/**
 * @brief Node `id` has been removed, so we need to fix up connection IDs
 * so that they don't shift.
*/
static void G_Ai_Node_Adjust(const ai_node_id_t id) {

  for (guint i = 0; i < g_ai_nodes->len; i++) {
    const ai_node_t *node = &g_array_index(g_ai_nodes, ai_node_t, i);

    if (!node->links) {
      continue;
    }

    for (guint l = 0; l < node->links->len; l++) {
      ai_link_t *link = &g_array_index(node->links, ai_link_t, l);

      if (link->id >= id) {
        link->id--;
      }
    }
  }
}

/**
 * @brief Destroys a navigation node, removing all of its links and freeing its slot.
 */
void G_Ai_Node_Destroy(const ai_node_id_t id) {

  if (!g_ai_nodes || id >= g_ai_nodes->len) {
    gi.Warn("Invalid node id %u\n", id);
    return;
  }

  G_Ai_Node_UnlinkAll(id);
  g_ai_nodes = g_array_remove_index(g_ai_nodes, id);

  G_Ai_Node_InvalidateSpatialIndex();

  if (!g_ai_nodes->len) {
    g_array_free(g_ai_nodes, true);
    g_ai_nodes = NULL;
  } else {
    G_Ai_Node_Adjust(id);
  }
}

/**
 * @brief Returns true if the client entity is currently standing on solid ground.
 */
static bool G_Ai_Node_OnGround(const g_client_t *cl) {
  const cm_trace_t tr = gi.Trace(cl->entity->s.origin,
                                 Vec3_Add(cl->entity->s.origin, Vec3(0, 0, -PM_GROUND_DIST)),
                                 cl->entity->s.bounds,
                                 NULL,
                                 CONTENTS_MASK_CLIP_CORPSE);

  return tr.fraction < 1.0f && tr.plane.normal.z > PM_STEP_NORMAL;
}

/**
 * @brief Returns the world position of the specified navigation node.
 */
vec3_t G_Ai_Node_GetPosition(const ai_node_id_t node) {

  return g_array_index(g_ai_nodes, ai_node_t, node).position;
}

/**
 * @brief Recalculates the traversal costs for all links incident to the given node.
 */
static void G_Ai_Node_UpdateCosts(const ai_node_id_t id) {

  for (guint i = 0; i < g_ai_nodes->len; i++) {
    const ai_node_t *node = &g_array_index(g_ai_nodes, ai_node_t, i);

    if (!node->links || !node->links->len) {
      continue;
    }

    for (guint l = 0; l < node->links->len; l++) {
      ai_link_t *link = &g_array_index(node->links, ai_link_t, l);

      if (id == i || link->id == id) {
        link->cost = G_Ai_Node_Cost(i, link->id);
      }
    }
  }
}

/**
 * @brief Returns false if any `func_plat` covering position is not at its bottom (accessible) state.
 * Bots should not attempt to path to nodes on an elevated platform.
 */
static bool G_Ai_PlatformAccessible(const vec3_t position) {

  G_ForEachEntity(ent, {
    if (!ent->classname || strcmp(ent->classname, "func_plat") != 0) {
      continue;
    }

    if (position.x < ent->abs_bounds.mins.x || position.x > ent->abs_bounds.maxs.x ||
        position.y < ent->abs_bounds.mins.y || position.y > ent->abs_bounds.maxs.y) {
      continue;
    }

    // Platform surface (abs_bounds.maxs.z) must be within reach of the node's Z.
    if (ent->abs_bounds.maxs.z > position.z + 32.f) {
      return false;
    }
  });

  return true;
}

/**
 * @brief Check if the node we want to move towards is currently pathable.
 */
bool G_Ai_Node_CanPathTo(const vec3_t position) {

  // if we're heading onto a mover node, only allow us to go forth
  // if the mover is there
  const vec3_t end = Vec3_Subtract(position, Vec3(0, 0, PM_GROUND_DIST * 3.f));

  // check if the destination has ground
  cm_trace_t tr = gi.Trace(position, end, Box3_Expand3(PM_BOUNDS, Vec3(1.f, 1.f, 0.f)), NULL, CONTENTS_MASK_CLIP_CORPSE | CONTENTS_MASK_LIQUID);

  // bad ground
  bool stuck_in_mover = tr.ent
      && (tr.start_solid || tr.all_solid)
      && (((g_entity_t *) tr.ent)->s.number != 0
      && !(tr.contents & CONTENTS_MASK_LIQUID));

  if (tr.fraction == 1.0f) {
    // Legitimate drop links may have no immediate floor under the node position.
    // Treat those as pathable and let normal movement/distress logic handle the descent.
    return true;
  }

  if (stuck_in_mover) {

    // check with a thinner box; it might be a button press or rotating thing
    tr = gi.Trace(position,
               Vec3_Subtract(position, Vec3(0, 0, PM_GROUND_DIST * 3.f)),
               Box3(Vec3(-4.f, -4.f, PM_BOUNDS.mins.z), Vec3(4.f, 4.f, PM_BOUNDS.maxs.z)),
               NULL,
               CONTENTS_MASK_CLIP_CORPSE | CONTENTS_MASK_LIQUID);
    stuck_in_mover = tr.ent
        && (tr.start_solid || tr.all_solid)
        && (((g_entity_t *) tr.ent)->s.number != 0
        && !(tr.contents & CONTENTS_MASK_LIQUID));

    if (!stuck_in_mover) {
      return true;
    }

    return false;
  }

  return !G_Ai_PlatformAccessible(position) ? false : true;
}

/**
 * @brief Check if the node we want to move towards is currently pathable.
 */
bool G_Ai_Path_CanPathTo(const GArray *path, const guint index) {

  // sanity
  if (index >= path->len) {
    return true;
  }

  // if we're heading onto a mover node, only allow us to go forth
  // if the mover is there
  return G_Ai_Node_CanPathTo(g_array_index(g_ai_nodes, ai_node_t, g_array_index(path, ai_node_id_t, index)).position);
}

/**
 * @brief The length of space that nodes will drop while walking.
 */
#define WALKING_DISTANCE  128.f

/**
 * @brief The length of space that couldn't be logically travelled by regular means.
 */
#define TELEPORT_DISTANCE  64.f

/**
 * @brief Handles automatic node placement as the player moves through the map during development.
 */
void G_Ai_Node_PlayerRoam(g_client_t *cl, const pm_cmd_t *cmd) {

  if (!g_ai_node_dev->integer) {
    return;
  }

  g_entity_t *ent = cl->entity;

  g_ai_player_roam.old_buttons = g_ai_player_roam.buttons;
  g_ai_player_roam.buttons = cmd->buttons;
  g_ai_player_roam.latched_buttons |= g_ai_player_roam.buttons & ~g_ai_player_roam.old_buttons;
  
  const bool allow_adjustments = g_ai_node_dev->integer == 1;
  const bool do_noding = allow_adjustments && g_ai_player_roam.drop_nodes && cl->ps.pm_state.type == PM_NORMAL;  

  // we just switched between noclip/not noclip, clear some stuff
  // so we don't accidentally drop nodes
  if (g_ai_player_roam.do_noding != do_noding) {
    g_ai_player_roam.do_noding = do_noding;

    g_ai_player_roam.position = ent->s.origin;
    g_ai_player_roam.last_nodes[0] = g_ai_player_roam.last_nodes[1] = AI_NODE_INVALID;
    g_ai_player_roam.await_landing = true;
  }

  const bool in_water = gi.PointContents(ent->s.origin) & (CONTENTS_WATER | CONTENTS_SLIME | CONTENTS_LAVA);
  const float last_node_distance_compare = g_ai_player_roam.last_nodes[0] == AI_NODE_INVALID ? FLT_MAX : Vec3_Distance(ent->s.origin, G_Ai_Node_GetPosition(g_ai_player_roam.last_nodes[0]));
  const float player_distance_compare = Vec3_Distance(ent->s.origin, g_ai_player_roam.position);

  if (G_Ai_Node_OnGround(cl)) {
    g_ai_player_roam.floor_position = ent->s.origin;
  }

  if (do_noding) {
    // we're waiting to land to drop a node; we jumped, fell, got sent by a jump pad, something like that.
    if (g_ai_player_roam.await_landing) {

      if (G_Ai_Node_OnGround(cl) || in_water) {

        // we landed!
        g_ai_player_roam.await_landing = false;
        g_ai_player_roam.position = ent->s.origin;

        ai_node_id_t landed_near_node = G_Ai_Node_FindClosest(ent->s.origin, WALKING_DISTANCE / 2, true, true);

        if (landed_near_node == AI_NODE_INVALID) {

          landed_near_node = G_Ai_Node_Create(ent->s.origin);
        }

        // one-way node from where we were to here
        if (g_ai_player_roam.last_nodes[0] != AI_NODE_INVALID) {
          bool bidirectional = false;

          if (g_ai_player_roam.is_water_jump) {
            bidirectional = true;
            G_Ai_Debug("Most likely waterjump; connecting both ends\n");
          } else if (g_ai_player_roam.is_jumping || in_water) {
            const float z_diff = fabsf(G_Ai_Node_GetPosition(g_ai_player_roam.last_nodes[0]).z - G_Ai_Node_GetPosition(landed_near_node).z);

            if (z_diff < PM_STEP_HEIGHT || (in_water && z_diff < PM_STEP_HEIGHT * 3.f)) {
              bidirectional = true;
              G_Ai_Debug("Most likely jump or drop-into-water link; connecting both ends\n");
            }
          }

          G_Ai_Node_LinkDefault(g_ai_player_roam.last_nodes[0], landed_near_node, bidirectional);
        }

        g_ai_player_roam.last_nodes[1] = g_ai_player_roam.last_nodes[0];
        g_ai_player_roam.last_nodes[0] = landed_near_node;
        g_ai_player_roam.is_water_jump = false;
      }

      return;
    }

    // we probably teleported; no node, just start dropping here when we land
    if (player_distance_compare > TELEPORT_DISTANCE) {

      g_ai_player_roam.last_nodes[0] = g_ai_player_roam.last_nodes[1] = AI_NODE_INVALID;
      g_ai_player_roam.position = ent->s.origin;
      g_ai_player_roam.await_landing = true;

      G_Ai_Debug("Teleport detected; awaiting landing...\n");
      return;
    }

    // we just left the floor (or water); drop a node here
    if (!G_Ai_Node_OnGround(cl) && !in_water) {
      // for water leavings, we want to drop where we are, not where we went into the water from
      const vec3_t where = g_ai_player_roam.is_water_jump ? ent->s.origin : g_ai_player_roam.floor_position;
      const ai_node_id_t jumped_near_node = G_Ai_Node_FindClosest(where, WALKING_DISTANCE / 2, true, false);
      const bool is_jump = cl->ps.pm_state.velocity.z > 0;

      if (jumped_near_node == AI_NODE_INVALID) {
        const ai_node_id_t id = G_Ai_Node_Create(where);

        if (g_ai_player_roam.last_nodes[0] != AI_NODE_INVALID) {

          G_Ai_Node_LinkDefault(g_ai_player_roam.last_nodes[0], id, true);
        }
      
        g_ai_player_roam.last_nodes[1] = g_ai_player_roam.last_nodes[0];
        g_ai_player_roam.last_nodes[0] = id;
      } else {
        
        if (g_ai_player_roam.last_nodes[0] != AI_NODE_INVALID) {

          G_Ai_Node_LinkDefault(g_ai_player_roam.last_nodes[0], jumped_near_node, true);
        }
      
        g_ai_player_roam.last_nodes[1] = g_ai_player_roam.last_nodes[0];
        g_ai_player_roam.last_nodes[0] = jumped_near_node;
      }

      g_ai_player_roam.await_landing = true;
      g_ai_player_roam.is_jumping = is_jump;

      // we didn't leave from water jump, so turn this off.
      if (!is_jump) {
        g_ai_player_roam.is_water_jump = false;
      }

      G_Ai_Debug("Left ground; jumping? %s\n", is_jump ? "yes" : "nop");
      return;
    }
  }

  // we're walkin'

  const ai_node_id_t closest_node = G_Ai_Node_FindClosest(ent->s.origin, WALKING_DISTANCE / 4, true, false);
  const bool on_mover = ent->ground.ent && ((g_entity_t *) ent->ground.ent)->s.number != 0;

  // attack button enables/disables placement
  if (allow_adjustments && (g_ai_player_roam.latched_buttons & BUTTON_ATTACK)) {
    g_ai_player_roam.drop_nodes = !g_ai_player_roam.drop_nodes;

    g_ai_player_roam.latched_buttons &= ~BUTTON_ATTACK;
  // "use" moves node
  } else if (allow_adjustments && ent->move_node) {
    if (g_ai_player_roam.last_nodes[0] != AI_NODE_INVALID) {
      ai_node_t *node = &g_array_index(g_ai_nodes, ai_node_t, g_ai_player_roam.last_nodes[0]);
      node->position = ent->s.origin;

      if (cmd->up < 0) {
        const cm_trace_t tr = gi.Trace(node->position, Vec3_Subtract(node->position, Vec3(0.f, 0.f, MAX_WORLD_COORD)), PM_BOUNDS, ent, CONTENTS_MASK_SOLID);
        node->position = tr.end;
      }

      // recalculate links
      G_Ai_Node_UpdateCosts(g_ai_player_roam.last_nodes[0]);
    }

    ent->move_node = false;
  // hook destroys node
  } else if (allow_adjustments && (g_ai_player_roam.latched_buttons & BUTTON_HOOK)) {
    if (g_ai_player_roam.last_nodes[0] != AI_NODE_INVALID) {
      G_Ai_Node_Destroy(g_ai_player_roam.last_nodes[0]);
      g_ai_player_roam.position = ent->s.origin;
      g_ai_player_roam.last_nodes[0] = g_ai_player_roam.last_nodes[1] = AI_NODE_INVALID;
      g_ai_player_roam.last_nodes[0] = G_Ai_Node_FindClosest(ent->s.origin, WALKING_DISTANCE * 2.5f, true, false);
    }
    g_ai_player_roam.latched_buttons &= ~BUTTON_HOOK;
  // score adjusts link connections
  } else if (allow_adjustments && (g_ai_player_roam.latched_buttons & BUTTON_SCORE)) {

    if (g_ai_player_roam.last_nodes[1] != AI_NODE_INVALID && g_ai_player_roam.last_nodes[0] != AI_NODE_INVALID) {
      uint8_t bits = 0;

      if (G_Ai_Node_IsLinked(g_ai_player_roam.last_nodes[0], g_ai_player_roam.last_nodes[1])) {
        bits |= 1;
      }
      if (G_Ai_Node_IsLinked(g_ai_player_roam.last_nodes[1], g_ai_player_roam.last_nodes[0])) {
        bits |= 2;
      }

      bits = (bits + 1) % 4;

      G_Ai_Node_Unlink(g_ai_player_roam.last_nodes[0], g_ai_player_roam.last_nodes[1]);
      G_Ai_Node_Unlink(g_ai_player_roam.last_nodes[1], g_ai_player_roam.last_nodes[0]);
      
      if (bits & 1) {
        G_Ai_Node_LinkDefault(g_ai_player_roam.last_nodes[0], g_ai_player_roam.last_nodes[1], false);
      }
      if (bits & 2) {
        G_Ai_Node_LinkDefault(g_ai_player_roam.last_nodes[1], g_ai_player_roam.last_nodes[0], false);
      }
    }
    
    g_ai_player_roam.latched_buttons &= ~BUTTON_SCORE;
  // we're stepping on/off a mover; connect us one-way
  } else if (on_mover != g_ai_player_roam.on_mover) {

    g_ai_player_roam.on_mover = on_mover;

    if (do_noding) {
      ai_node_id_t id = G_Ai_Node_FindClosest(ent->s.origin, WALKING_DISTANCE / 8, true, false);
      
      if (id == AI_NODE_INVALID) {
        id = G_Ai_Node_Create(ent->s.origin);
      }

      if (g_ai_player_roam.last_nodes[0] != AI_NODE_INVALID) {

        G_Ai_Node_LinkDefault(g_ai_player_roam.last_nodes[0], id, false);
      }
    
      g_ai_player_roam.last_nodes[1] = g_ai_player_roam.last_nodes[0];
      g_ai_player_roam.last_nodes[0] = id;
    }

  // if we touched another node and had another node lit up; connect us if we aren't already
  } else if (closest_node != AI_NODE_INVALID && closest_node != g_ai_player_roam.last_nodes[0] && G_Ai_Node_Visible(ent->s.origin, closest_node)) {

    if (do_noding && g_ai_player_roam.last_nodes[0] != AI_NODE_INVALID) {

      G_Ai_Node_LinkDefault(g_ai_player_roam.last_nodes[0], closest_node, !g_ai_player_roam.on_mover);
    }
    
    g_ai_player_roam.last_nodes[1] = g_ai_player_roam.last_nodes[0];
    g_ai_player_roam.last_nodes[0] = closest_node;
  // we got far enough from the last node, so drop a new node
  } else if (last_node_distance_compare > WALKING_DISTANCE) {

    if (do_noding) {
      ai_node_id_t id = G_Ai_Node_FindClosest(ent->s.origin, WALKING_DISTANCE / 2, true, !in_water);
      
      if (id == AI_NODE_INVALID) {
        id = G_Ai_Node_Create(ent->s.origin);
      }

      if (g_ai_player_roam.last_nodes[0] != AI_NODE_INVALID) {

        G_Ai_Node_LinkDefault(g_ai_player_roam.last_nodes[0], id, !g_ai_player_roam.on_mover);
      }
    
      g_ai_player_roam.last_nodes[1] = g_ai_player_roam.last_nodes[0];
      g_ai_player_roam.last_nodes[0] = id;
    }
  }

  // we're currently in water; the only way we can get out
  // is by waterjumping, having something take us out of the water,
  // a ladder or walking straight out of water. mark waterjump as true,
  // we'll figure it out when we actually leave water what the intentions are.
  if (in_water) {
    g_ai_player_roam.is_water_jump = true;
  }

  g_ai_player_roam.position = ent->s.origin;
}

/**
 * @brief A compact representation of a directed link pair used for render deduplication.
 */
typedef struct {
  union {
    struct {
      ai_node_id_t a, b;
    };
    int32_t v;
  };
} ai_unique_link_t;

/**
 * @brief Renders a single node link line for developer visualization.
 */
static void G_Ai_Node_RenderLinks(gpointer key, gpointer value, gpointer userdata) {

  const ai_unique_link_t ulink = { .v = GPOINTER_TO_INT(key) };
  const int32_t bits = GPOINTER_TO_INT(value);
  
  const ai_node_t *node_a = &g_array_index(g_ai_nodes, ai_node_t, ulink.a);
  const ai_node_t *node_b = &g_array_index(g_ai_nodes, ai_node_t, ulink.b);

  g_client_t *client = NULL;
  G_ForEachClient(cl, {
    if (!cl->ai) {
      client = cl;
      break;
    }
  });

  assert(client);
  g_entity_t *ent = client->entity;

  if (!G_Ai_Node_Visible(Vec3_Add(ent->s.origin, client->ps.pm_state.view_offset), ulink.a)
      && !G_Ai_Node_Visible(Vec3_Add(ent->s.origin,client->ps.pm_state.view_offset), ulink.b)) {
    return;
  }

  gi.WriteByte(SV_CMD_TEMP_ENTITY);
  gi.WriteByte(TE_AI_NODE_LINK);
  gi.WritePosition(node_a->position);
  gi.WritePosition(node_b->position);
  gi.WriteByte(bits);
  gi.Multicast(node_a->position, MULTICAST_PVS);
}

/**
 * @brief Returns true if the specified node ID is present in the given path array.
 */
static bool G_Ai_NodeInPath(GArray *path, ai_node_id_t node) {

  if (!path) {
    return false;
  }

  for (guint i = 0; i < path->len; i++) {
    if (g_array_index(path, ai_node_id_t, i) == node) {
      return true;
    }
  }

  return false;
}

/**
 * @brief Renders all navigation nodes and their links for developer visualization.
 */
void G_Ai_Node_Render(void) {

  if (!g_ai_node_dev->integer) {
    return;
  }

  if (!g_ai_nodes) {
    return;
  }

  g_client_t *client = NULL;
  G_ForEachClient(cl, {
    if (cl->entity && !cl->ai) {
      client = cl;
      break;
    }
  });

  if (!client) { // probably just hasn't spawned yet
    return;
  }

  g_entity_t *ent = client->entity;

  GHashTable *unique_links = g_hash_table_new(g_direct_hash, g_direct_equal);

  for (guint i = 0; i < g_ai_nodes->len; i++) {
    const ai_node_t *node = &g_array_index(g_ai_nodes, ai_node_t, i);
    const bool in_path = G_Ai_NodeInPath(g_ai_player_roam.test_path, i);

    if (G_Ai_Node_Visible(Vec3_Add(ent->s.origin, client->ps.pm_state.view_offset), i)) {

      gi.WriteByte(SV_CMD_TEMP_ENTITY);
      gi.WriteByte(TE_AI_NODE);
      gi.WritePosition(node->position);
      gi.WriteShort(i);

      byte bits = 0;

      if (in_path) {
        bits = 3;
      } else if (g_ai_player_roam.last_nodes[0] == i) {
        bits = 1;
      } else if (g_ai_player_roam.last_nodes[1] == i) {
        bits = 2;
      }

      if (!G_Ai_Node_CanPathTo(node->position)) {
        bits |= 16;
      }

      gi.WriteByte(bits);
      gi.Multicast(node->position, MULTICAST_PVS);
    }

    if (node->links) {

      for (guint l = 0; l < node->links->len; l++) {
        const ai_link_t *link = &g_array_index(node->links, ai_link_t, l);
        ai_unique_link_t ulink;
        int32_t bit;

        if (link->id > i) {
          ulink.a = link->id;
          ulink.b = i;
          bit = 1;
        } else {
          ulink.a = i;
          ulink.b = link->id;
          bit = 2;
        }

        if (!in_path) {
          bit |= 4;
        }

        if (G_Ai_ShouldSlowDrop(i, link->id)) {
          bit |= 16;
        }

        if (!g_hash_table_contains(unique_links, GINT_TO_POINTER(ulink.v))) {
          g_hash_table_insert(unique_links, GINT_TO_POINTER(ulink.v), GINT_TO_POINTER(bit));
        } else {
          int32_t existing_bit = GPOINTER_TO_INT(g_hash_table_lookup(unique_links, GINT_TO_POINTER(ulink.v)));
          g_hash_table_replace(unique_links, GINT_TO_POINTER(ulink.v), GINT_TO_POINTER(existing_bit | bit));
        }
      }
    }
  }

  g_hash_table_foreach(unique_links, G_Ai_Node_RenderLinks, NULL);

  g_hash_table_destroy(unique_links);

  // draw the bots' targets
  G_ForEachClient(cl, {

    if (!cl->ai) {
      continue;
    }


    if (cl->ai->move_target.type != AI_GOAL_PATH) {
      continue;
    }

    gi.WriteByte(SV_CMD_TEMP_ENTITY);
    gi.WriteByte(TE_AI_NODE_LINK);
    gi.WritePosition(cl->entity->s.origin);
    gi.WritePosition(cl->ai->move_target.path.path_position);
    gi.WriteByte(8);
    gi.Multicast(cl->entity->s.origin, MULTICAST_PVS);
  });
}

#define AI_NODE_MAGIC ('Q' | '2' << 8 | 'N' << 16 | 'S' << 24)
#define AI_NODE_VERSION 2

/**
 * @brief Initializes the navigation node system and loads the .nav file for the current map.
 */
void G_Ai_InitNodes(void) {

  G_Ai_ShutdownNodes();

  g_ai_player_roam.position = Vec3(MAX_WORLD_DIST, MAX_WORLD_DIST, MAX_WORLD_DIST);
  g_ai_player_roam.last_nodes[0] = g_ai_player_roam.last_nodes[1] = AI_NODE_INVALID;
  g_ai_player_roam.await_landing = true;

  char filename[MAX_OS_PATH];

  g_snprintf(filename, sizeof(filename), "maps/%s.nav", g_level.name);

  if (!gi.FileExists(filename)) {
    gi.Warn("No navigation file exists for this map; bots will be dumb!\nUse `g_ai_node_dev` to set up nodes.\n");
    return;
  }

  file_t *file = gi.OpenFile(filename);
  int32_t magic, version;
  
  gi.ReadFile(file, &magic, sizeof(magic), 1);

  if (magic != AI_NODE_MAGIC) {
    gi.Warn("Nav file invalid format!\n");
    gi.CloseFile(file);
    return;
  }

  gi.ReadFile(file, &version, sizeof(version), 1);

  if (version != AI_NODE_VERSION) {
    gi.Warn("Nav file out of date!\n");
    gi.CloseFile(file);
    return;
  }

  guint num_nodes;
  gi.ReadFile(file, &num_nodes, sizeof(num_nodes), 1);

  g_ai_nodes = g_array_sized_new(false, true, sizeof(ai_node_t), num_nodes);
  g_array_set_size(g_ai_nodes, num_nodes);

  G_Ai_Node_InvalidateSpatialIndex();

  guint total_links = 0;

  for (guint i = 0; i < g_ai_nodes->len; i++) {
    ai_node_t *node = &g_array_index(g_ai_nodes, ai_node_t, i);

    gi.ReadFile(file, &node->position, sizeof(node->position), 1);

    guint num_links;
  
    gi.ReadFile(file, &num_links, sizeof(num_links), 1);

    if (num_links) {
      node->links = g_array_sized_new(false, false, sizeof(ai_link_t), num_links);
      g_array_set_size(node->links, num_links);
      gi.ReadFile(file, node->links->data, sizeof(ai_link_t), num_links);
      total_links += num_links;

      G_Ai_Node_Unlink(i, i);
    } else {
      node->links = NULL;
    }
  }

  gi.CloseFile(file);
  gi.Print("  Loaded %u nodes with %u total links.\n", num_nodes, total_links);

  g_ai_player_roam.file_nodes = num_nodes;
  g_ai_player_roam.file_links = 0;

  for (guint i = 0; i < g_ai_nodes->len; i++) {
    const ai_node_t *node = &g_array_index(g_ai_nodes, ai_node_t, i);

    if (node->links) {
      g_ai_player_roam.file_links += node->links->len;
    }
  }
}

/**
 * @brief Validates node integrity: warns about item entities with no nearby node and nodes inside solid.
 */
static void G_Ai_CheckNodes(void) {

  if (g_ai_node_dev->integer) {
    G_ForEachEntity(ent, {

      // only warn for item nodes
      if (!ent->item) {
        continue;
      }

      const ai_node_id_t node = G_Ai_Node_FindClosest(ent->s.origin, WALKING_DISTANCE * 2.5f, true, false);

      if (node == AI_NODE_INVALID) {
        gi.Warn("Entity %s @ %s appears to be unreachable by nodes\n", ent->classname, vtos(ent->s.origin));
      }
    });
  }

  for (guint i = 0; i < g_ai_nodes->len; i++) {
    const ai_node_t *node = &g_array_index(g_ai_nodes, ai_node_t, i);
    
    if (gi.PointContents(node->position) & CONTENTS_MASK_SOLID) {
      gi.Warn("Node %i @ %s is inside of solid\n", i, vtos(node->position));
    }
  }
}

/**
 * @brief Called after the first second of gameplay to allow the AI to pick up nodes
 * from late-spawning entities.
 */
void G_Ai_NodesReady(void) {

  if (!g_ai_nodes) {
    return;
  }

  const guint added_nodes = g_ai_nodes->len - g_ai_player_roam.file_nodes;
  guint added_links = 0;

  for (guint i = 0; i < g_ai_nodes->len; i++) {
    const ai_node_t *node = &g_array_index(g_ai_nodes, ai_node_t, i);

    if (node->links) {
      added_links += node->links->len;
    }
  }

  added_links -= g_ai_player_roam.file_links;
  gi.Print("  Game loaded %u additional nodes with %u new links.\n", added_nodes, added_links);

  G_ForEachEntity(ent, {
    if (ent->classname && strcmp(ent->classname, "func_plat") == 0) {
      if (!g_ai_platforms) {
        g_ai_platforms = g_array_new(false, false, sizeof(g_entity_t *));
      }
      g_ai_platforms = g_array_append_vals(g_ai_platforms, &ent, 1);
    }
  });

  /*if (g_ai_node_dev->integer != 1) {
    const guint optimized = Ai_OptimizeNodes();
    gi.Print("  %u nodes optimized\n", optimized);
  }*/

  G_Ai_CheckNodes();
}

/**
 * @brief Serializes all navigation nodes and their links to the current map's .nav file.
 */
void G_Ai_SaveNodes(void) {

  if (g_ai_node_dev->integer != 1) {
    gi.Warn("This command only works with `g_ai_node_dev` set to 1.\n");
    return;
  }

  char filename[MAX_OS_PATH];

  g_snprintf(filename, sizeof(filename), "maps/%s.nav", g_level.name);

  if (!g_ai_nodes) {
    gi.Warn("No nodes to write.\n");
    return;
  }

  file_t *file = gi.OpenFileWrite(filename);
  int32_t magic = AI_NODE_MAGIC;
  int32_t version = AI_NODE_VERSION;
  
  gi.WriteFile(file, &magic, sizeof(magic), 1);
  gi.WriteFile(file, &version, sizeof(version), 1);

  gi.WriteFile(file, &g_ai_nodes->len, sizeof(g_ai_nodes->len), 1);

  for (guint i = 0; i < g_ai_nodes->len; i++) {
    const ai_node_t *node = &g_array_index(g_ai_nodes, ai_node_t, i);

    gi.WriteFile(file, &node->position, sizeof(node->position), 1);

    if (node->links) {
      gi.WriteFile(file, &node->links->len, sizeof(node->links->len), 1);
      gi.WriteFile(file, node->links->data, sizeof(ai_link_t), node->links->len);
    } else {
      guint len = 0;
      gi.WriteFile(file, &len, sizeof(len), 1);
    }
  }

  gi.CloseFile(file);

  gi.Print("Wrote nodes to %s.\n", gi.RealPath(filename));

  G_Ai_CheckNodes();
}

/**
 * @brief Clears all navigation nodes, freeing their link arrays but retaining the backing array.
 */
void G_Ai_DeleteNodes(void) {

  if (g_ai_nodes) {
    for (guint i = 0; i < g_ai_nodes->len; i++) {
      ai_node_t *node = &g_array_index(g_ai_nodes, ai_node_t, i);

      if (node->links) {
        g_array_free(node->links, true);
      }
    }

    g_array_set_size(g_ai_nodes, 0);
  }

  if (g_ai_platforms) {
    g_array_free(g_ai_platforms, true);
    g_ai_platforms = NULL;
  }

  G_Ai_Node_InvalidateSpatialIndex();
  G_Ai_Node_FreePathPool();
}

/**
 * @brief Frees all navigation node data, including the backing node array.
 */
void G_Ai_ShutdownNodes(void) {

  if (g_ai_nodes) {
    for (guint i = 0; i < g_ai_nodes->len; i++) {
      ai_node_t *node = &g_array_index(g_ai_nodes, ai_node_t, i);

      if (node->links) {
        g_array_free(node->links, true);
      }
    }

    g_array_free(g_ai_nodes, true);
    g_ai_nodes = NULL;
  }

  if (g_ai_platforms) {
    g_array_free(g_ai_platforms, true);
    g_ai_platforms = NULL;
  }

  G_Ai_Node_InvalidateSpatialIndex();
  G_Ai_Node_FreePathPool();
}

typedef struct {
  ai_node_id_t id;
  float priority;
} ai_node_priority_t;

static struct gheap_s *g_ai_node_path_queue;
static ai_node_priority_t *g_ai_node_path_entries;
static size_t g_ai_node_path_capacity;
static size_t g_ai_node_path_count;

static void G_Ai_Node_FreePathPool(void) {

  gheap_free(&g_ai_node_path_queue);
  free(g_ai_node_path_entries);
  g_ai_node_path_entries = NULL;
  g_ai_node_path_capacity = 0;
  g_ai_node_path_count = 0;
}

static bool G_Ai_Node_EnsurePathPool(const size_t capacity) {

  if (g_ai_node_path_queue && g_ai_node_path_capacity >= capacity) {
    gheap_reset(g_ai_node_path_queue);
    g_ai_node_path_count = 0;
    return true;
  }

  G_Ai_Node_FreePathPool();

  g_ai_node_path_queue = gheap_create(capacity);
  g_ai_node_path_entries = malloc(sizeof(ai_node_priority_t) * capacity);

  if (!g_ai_node_path_queue || !g_ai_node_path_entries) {
    G_Ai_Node_FreePathPool();
    return false;
  }

  g_ai_node_path_capacity = capacity;
  return true;
}

static ai_node_priority_t *G_Ai_Node_AllocPathEntry(void) {

  if (g_ai_node_path_count == g_ai_node_path_capacity) {
    return NULL;
  }

  return &g_ai_node_path_entries[g_ai_node_path_count++];
}

#define AI_MAX_DROP_HEIGHT 512.f
#define AI_DROP_PENALTY_START 128.f
#define AI_DROP_PENALTY_SCALE 0.5f
#define AI_DROP_HEALTH_MARGIN 8.f
#define AI_DROP_DAMAGE_PENALTY_SCALE 6.f

static inline float G_Ai_LinkCost(const ai_node_id_t a, const ai_node_id_t b) {
  const ai_node_t *node = &g_array_index(g_ai_nodes, ai_node_t, a);

  assert(node->links);

  for (guint i = 0; i < node->links->len; i++) {
    const ai_link_t *link = &g_array_index(node->links, ai_link_t, i);

    if (link->id == b) {
      return link->cost;
    }
  }

  assert(false);
  return -1;
}

/**
 * @brief Returns true if the segment between two nodes intersects lava or slime.
 */
static bool G_Ai_LinkPassesHazard(const vec3_t from, const vec3_t to) {
  const vec3_t delta = Vec3_Subtract(to, from);
  const float length = Vec3_Length(delta);

  if (length <= 0.f) {
    return (gi.PointContents(from) & (CONTENTS_LAVA | CONTENTS_SLIME)) != 0;
  }

  const vec3_t dir = Vec3_Scale(delta, 1.f / length);
  const float step = 24.f;
  const int32_t samples = Maxi(1, (int32_t) ceilf(length / step));

  for (int32_t i = 0; i <= samples; i++) {
    const float dist = Minf(length, i * step);
    const vec3_t point = Vec3_Fmaf(from, dist, dir);

    if (gi.PointContents(point) & (CONTENTS_LAVA | CONTENTS_SLIME)) {
      return true;
    }
  }

  return false;
}

/**
 * @brief Finds the shortest path between two nodes using the A* algorithm.
 */
static float G_Ai_EstimatedFallDamage(const float drop, const int32_t gravity, const int32_t water_level) {

  if (drop <= 0.f || gravity <= 0) {
    return 0.f;
  }

  const float impact_velocity = -sqrtf(2.f * gravity * drop);
  if (impact_velocity > PM_SPEED_FALL) {
    return 0.f;
  }

  float damage = -((impact_velocity - PM_SPEED_FALL) * 0.05f);

  if (water_level > 0) {
    damage /= (float) (1u << Mini(water_level, 3));
  }

  return Maxf(0.f, damage);
}

GArray *G_Ai_Node_FindPath(const g_client_t *cl, const ai_node_id_t start, const ai_node_id_t end, const G_Ai_NodeCostFunc heuristic, float *length) {
  
  if (length) {
    *length = 0;
  }

  // sanity
  if (start == AI_NODE_INVALID || end == AI_NODE_INVALID) {
    return NULL;
  }
  
  // Pre-collect func_plat entities once so G_Ai_PlatformAccessible doesn't
  // call G_ForEachEntity for every link expansion inside the A* loop.

  // size on 64k nodes is, say, 8kb.
  uint32_t costs_started[g_ai_nodes->len / 32 + 1];
  memset(costs_started, 0, sizeof(costs_started));
  uint32_t visited = 0;
  // Min-heap open set (priority = f-cost). Replaces the previous sorted-array
  // queue (O(n) insertion) with an O(log n) binary heap from grid_ds.c.
  // Capacity is bounded by the node count plus headroom for re-insertions
  // (A* may push a better-cost duplicate before popping the stale one).
  const size_t heap_capacity = (size_t) g_ai_nodes->len * 4 + 16;
  if (!G_Ai_Node_EnsurePathPool(heap_capacity)) {
    return NULL;
  }

  struct gheap_s *queue = g_ai_node_path_queue;
  bool finished = false;

  {
    ai_node_priority_t *e = G_Ai_Node_AllocPathEntry();
    e->id = start;
    e->priority = 0;
    gheap_push(queue, e->priority, e);
  }

  ai_node_t *start_node = &g_array_index(g_ai_nodes, ai_node_t, start);
  start_node->cost = 0;
  costs_started[start / 32] |= (uint32_t)1 << (start % 32);
  visited++;
  // g_hash_table_add(costs_started, start_node);

  for (;;) {
    ai_node_priority_t *current = (ai_node_priority_t *) gheap_pop(queue);
    if (!current) {
      break;
    }

    if (current->id == end) {
      finished = true;
      break;
    }

    ai_node_t *node = &g_array_index(g_ai_nodes, ai_node_t, current->id);

    // Stale entry guard: if this entry's priority is worse than the node's
    // best-known f-cost approximation (cost + 0 heuristic lower bound), the
    // node has already been processed via a better path.
    if (current->priority > node->cost + heuristic(current->id, end) + 0.0001f) {
      continue;
    }

    if (!node->links || !node->links->len) {
      continue;
    }

    // Compute once per expanded node rather than once per link.
    const int32_t node_contents = gi.PointContents(node->position);
    const bool from_hazard = (node_contents & (CONTENTS_LAVA | CONTENTS_SLIME)) != 0;

    for (guint i = 0; i < node->links->len; i++) {
      const ai_link_t *link = &g_array_index(node->links, ai_link_t, i);
      ai_node_t *link_node = &g_array_index(g_ai_nodes, ai_node_t, link->id);
      const float drop = node->position.z - link_node->position.z;
      const int32_t link_contents = gi.PointContents(link_node->position);
      const bool to_hazard = (link_contents & (CONTENTS_LAVA | CONTENTS_SLIME)) != 0;

      // Check platform accessibility using the pre-collected list.
      if (g_ai_platforms) {
        bool blocked = false;
        for (guint p = 0; p < g_ai_platforms->len; p++) {
          const g_entity_t *plat = g_array_index(g_ai_platforms, g_entity_t *, p);
          if (link_node->position.x < plat->abs_bounds.mins.x || link_node->position.x > plat->abs_bounds.maxs.x ||
              link_node->position.y < plat->abs_bounds.mins.y || link_node->position.y > plat->abs_bounds.maxs.y) {
            continue;
          }
          if (plat->abs_bounds.maxs.z > link_node->position.z + 32.f) {
            blocked = true;
            break;
          }
        }
        if (blocked) {
          continue;
        }
      }

      if (from_hazard && to_hazard) {
        continue;
      }

      if (!from_hazard && G_Ai_LinkPassesHazard(node->position, link_node->position)) {
        continue;
      }

      if (drop > AI_MAX_DROP_HEIGHT) {
        continue;
      }

      if (drop > 0.f && (link_contents & (CONTENTS_LAVA | CONTENTS_SLIME))) {
        continue;
      }

      float drop_penalty = 0.f;
      if (drop > AI_DROP_PENALTY_START) {
        drop_penalty = (drop - AI_DROP_PENALTY_START) * AI_DROP_PENALTY_SCALE;
      }

      if (cl && cl->entity) {
        const int32_t water_level = (link_contents & CONTENTS_WATER) ? 1 : 0;
        const float estimated_damage = G_Ai_EstimatedFallDamage(drop, g_level.gravity, water_level);

        if (estimated_damage + AI_DROP_HEALTH_MARGIN >= cl->entity->health) {
          continue;
        }

        drop_penalty += estimated_damage * AI_DROP_DAMAGE_PENALTY_SCALE;
      }

      const float new_cost = node->cost + link->cost + drop_penalty;

      ai_node_id_t link_index = G_Ai_Node_Index(link_node);
      bool found = costs_started[link_index / 32] & ((uint32_t)1 << (link_index % 32));
      if (!found || new_cost < link_node->cost) {
        costs_started[link_index / 32] |= (uint32_t)1 << (link_index % 32);
        visited++;

        link_node->cost = new_cost;
        const float priority = new_cost + heuristic(link->id, end);

        ai_node_priority_t *e = G_Ai_Node_AllocPathEntry();
        if (!e) {
          gi.Warn("A* open-set entry pool exhausted (capacity %zu)\n", heap_capacity);
          break;
        }

        e->id = link->id;
        e->priority = priority;
        // If the heap is somehow full (large maps with many revisits),
        // fall back to allocating a larger one is overkill; we simply
        // log and stop expanding from this node. The capacity heuristic
        // above (4 * N + 16) is generous for typical Quetoo maps.
        if (!gheap_push(queue, priority, e)) {
          gi.Warn("A* open-set heap exhausted (capacity %zu)\n", heap_capacity);
          break;
        }

        link_node->came_from = G_Ai_Node_Index(node);
      }
    }
  }

  GArray *return_path = NULL;

  if (finished) {
    G_Ai_Debug("Found path from %u -> %u with %u nodes visited\n", start, end, visited);

    return_path = g_array_sized_new(false, false, sizeof(ai_node_id_t), visited);

    return_path = g_array_prepend_val(return_path, end);

    if (start != end) {
      ai_node_id_t from = end;

      for (;;) {
        const ai_node_t *from_node = &g_array_index(g_ai_nodes, ai_node_t, from);
        from = from_node->came_from;
        return_path = g_array_prepend_val(return_path, from);

        if (from == start) {
          break;
        }
      }

      if (length) {
        for (guint i = 0; i < return_path->len - 1; i++) {
          const ai_node_id_t a = g_array_index(return_path, ai_node_id_t, i);
          const ai_node_id_t b = g_array_index(return_path, ai_node_id_t, i + 1);

          *length += G_Ai_LinkCost(a, b);
        }
      }
    }
  } else {
    G_Ai_Debug("Couldn't find path from %u -> %u\n", start, end);
  }

  return return_path;
}

void G_Ai_OffsetNodes_f(void) {

  vec3_t translate;

  if (gi.Argc() <= 1) {
    if (g_ai_player_roam.last_nodes[0] == AI_NODE_INVALID) {
      return;
    }

    const vec3_t node = G_Ai_Node_GetPosition(g_ai_player_roam.last_nodes[0]);
    const vec3_t player_position = g_ai_player_roam.position;
    translate = Vec3_Subtract(player_position, node);
  } else {  
    const char *offset = gi.Argv(1);

    if (Parse_QuickPrimitive(offset, PARSER_DEFAULT, PARSE_DEFAULT, PARSE_FLOAT, &translate, 3) != 3) {
      return;
    }
  }

  for (guint i = 0; i < g_ai_nodes->len; i++) {
    ai_node_t *node = &g_array_index(g_ai_nodes, ai_node_t, i);
    node->position = Vec3_Add(node->position, translate);
  }

  G_Ai_Node_InvalidateSpatialIndex();
}

/**
 * @brief Drops a node on top of this object and connects it to any nearby
 * nodes.
 */
bool G_Ai_DropItemLikeNode(g_entity_t *ent) {

  if (G_Ai_InDeveloperMode()) {
    return false;
  }

  // find node closest to us
  const ai_node_id_t src_node = G_Ai_Node_FindClosest(ent->s.origin, 512.f, true, true);

  if (src_node == AI_NODE_INVALID) {
    return false;
  }

  // make a new node on the item
  cm_trace_t down = gi.Trace(ent->s.origin, Vec3_Subtract(ent->s.origin, Vec3(0, 0, MAX_WORLD_COORD)), Box3_Zero(), NULL, CONTENTS_MASK_SOLID);
  vec3_t pos;

  if (down.fraction == 1.0) {
    pos = ent->s.origin;
  } else {
    pos = Vec3_Subtract(down.end, Vec3(0.f, 0.f, PM_BOUNDS.mins.z));
  }

  // grab all the links of the node that brought us here
  const GArray *src_links = G_Ai_Node_GetLinks(src_node);

  const ai_node_id_t new_node = G_Ai_Node_Create(pos);
  const float dist = Vec3_Distance(G_Ai_Node_GetPosition(src_node), ent->s.origin);

  // bidirectionally connect us to source
  G_Ai_Node_Link(src_node, new_node, dist);
  G_Ai_Node_Link(new_node, src_node, dist);

  // if we had source links, link any connected bi-directional nodes
  // to the item as well
  if (src_links) {

    for (guint i = 0; i < src_links->len; i++) {
      const ai_link_t *link = &g_array_index(src_links, ai_link_t, i);

      // not bidirectional
      if (!G_Ai_Node_IsLinked(link->id, src_node)) {
        continue;
      }

      const vec3_t link_pos = G_Ai_Node_GetPosition(link->id);

      // can't see 
      if (gi.Trace(ent->s.origin, link_pos, Box3_Zero(), NULL, CONTENTS_MASK_SOLID).fraction < 1.0) {
        continue;
      }

      const float dist = Vec3_Distance(link_pos, ent->s.origin);

      // bidirectionally connect us to source
      G_Ai_Node_Link(link->id, new_node, dist);
      G_Ai_Node_Link(new_node, link->id, dist);
    }
  }

  ent->node = new_node;
  return true;
}
