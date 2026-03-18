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

cvar_t *g_ai_min_clients;
cvar_t *g_ai_max_clients;

/**
 * @brief
 */
static void G_Ai_ClientThink(g_entity_t *ent) {
  const int32_t num_runs = 3;
  uint8_t msec_left = QUETOO_TICK_MILLIS;

  for (int32_t i = 0; i < num_runs; i++) {
    pm_cmd_t cmd = { 0 };

    cmd.msec = (i == num_runs - 1) ? msec_left : ceilf(1000.f / QUETOO_TICK_RATE / num_runs);

    G_Ai_Think(ent->client, &cmd);

    msec_left -= cmd.msec;
  }

  ent->next_think = g_level.time + QUETOO_TICK_MILLIS;
}

/**
 * @brief
 */
static void G_Ai_ClientBegin(g_client_t *cl) {

  G_ClientBegin(cl);

  G_Ai_Begin(cl);

  G_Debug("Spawned %s at %s", cl->persistent.net_name, vtos(cl->entity->s.origin));

  cl->entity->Think = G_Ai_ClientThink;
  cl->entity->next_think = g_level.time + QUETOO_TICK_MILLIS;
}

/**
 * @brief
 */
static void G_Ai_Connect(g_client_t *cl) {

  char user_info[MAX_INFO_STRING_STRING];
  Ai_GetUserInfo(cl, user_info);

  cl->ai = gi.Malloc(sizeof(ai_t), MEM_TAG_AI);

  G_ClientConnect(cl, user_info);

  G_Ai_ClientBegin(cl);
}

/**
 * @brief
 */
void G_Ai_Frame(void) {

  if (g_level.intermission_time) {
    return;
  }

  if (g_level.time % 1000 == 0) {

    if (g_level.time == 1000) {
      Ai_NodesReady();
    }

    int32_t count = 0;
    G_ForEachClient(cl, {
      if (cl->ai) {
        count++;
      }
    });

    const int32_t min = Maxi(0, g_ai_min_clients->integer);
    const int32_t max = Maxi(min, g_ai_max_clients->integer);

    if (count < min) {
      G_ForEachFreeClient(cl, {
        G_Ai_Connect(cl);
        break;
      });
    } else if (count > max) {
      G_ForEachClient(cl, {
        if (cl->ai) {
          G_Ai_Disconnect(cl);
          break;
        }
      });
    }
  }

  // run AI think functions
  G_ForEachClient(cl, {
    if (cl->ai && cl->entity) {
      G_RunThink(cl->entity);
    }
  });
}

/**
 * @brief
 */
void G_Ai_Init(void) {

  g_ai_min_clients = gi.AddCvar("g_ai_min_clients", "0", CVAR_SERVER_INFO, "The minimum number of bots to spawn.");
  g_ai_max_clients = gi.AddCvar("g_ai_max_clients", "0", CVAR_SERVER_INFO, "The maximum number of bots to spawn.");

  G_Ai_InitLocals();
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
  const ai_node_id_t src_node = Ai_Node_FindClosest(ent->s.origin, 512.f, true, true);

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
  GArray *src_links = Ai_Node_GetLinks(src_node);

  const ai_node_id_t new_node = Ai_Node_CreateNode(pos);
  const float dist = Vec3_Distance(Ai_Node_GetPosition(src_node), ent->s.origin);

  // bidirectionally connect us to source
  Ai_Node_CreateLink(src_node, new_node, dist);
  Ai_Node_CreateLink(new_node, src_node, dist);

  // if we had source links, link any connected bi-directional nodes
  // to the item as well
  if (src_links) {

    for (guint i = 0; i < src_links->len; i++) {
      ai_node_id_t src_link = g_array_index(src_links, ai_node_id_t, i);

      // not bidirectional
      if (!Ai_Node_IsLinked(src_link, src_node)) {
        continue;
      }

      const vec3_t link_pos = Ai_Node_GetPosition(src_link);

      // can't see 
      if (gi.Trace(ent->s.origin, link_pos, Box3_Zero(), NULL, CONTENTS_MASK_SOLID).fraction < 1.0) {
        continue;
      }

      const float dist = Vec3_Distance(link_pos, ent->s.origin);

      // bidirectionally connect us to source
      Ai_Node_CreateLink(src_link, new_node, dist);
      Ai_Node_CreateLink(new_node, src_link, dist);
    }

    g_array_free(src_links, true);
  }

  ent->node = new_node;
  return true;
}
