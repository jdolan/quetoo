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

#include <Objectively/List.h>

#include "g_local.h"
#include "bg_intermission.h"

cvar_t *g_vote_next_map;

#define BALLOT_NONE -1

static struct {
  bool active;
  bool voting;
  char maps[MAX_NEXT_MAPS][MAX_QPATH];
  int32_t num_maps;
  int32_t ballots[MAX_CLIENTS];
  int32_t published[MAX_NEXT_MAPS];
} g_intermission_state;

static struct {
  HandleClientCommand HandleClientCommand;
  FrameDidEnd FrameDidEnd;
  ClientWillDisconnect ClientWillDisconnect;
  ConfigureLevel ConfigureLevel;
} previous;

static bool installed;

/**
 * @brief Whether `name` is already offered, so that a rotation listing a map twice does
 * not offer it twice and split its own vote.
 */
static bool G_Intermission_Offers(const char *name) {

  for (int32_t i = 0; i < g_intermission_state.num_maps; i++) {
    if (!q_strcmp(g_intermission_state.maps[i], name)) {
      return true;
    }
  }

  return false;
}

/**
 * @brief Offers `name`, if it is a map this server can actually serve and is not already
 * offered. A rotation may name maps that were never installed; `next_map` does not check,
 * but a vote would let clients elect one.
 */
static void G_Intermission_Offer(const char *name) {

  if (g_intermission_state.num_maps == MAX_NEXT_MAPS) {
    return;
  }

  if (!name || !*name || G_Intermission_Offers(name)) {
    return;
  }

  if (!gi.FileExists(va("maps/%s.bsp", name))) {
    G_Debug("Skipping %s, which is not installed\n", name);
    return;
  }

  q_strlcpy(g_intermission_state.maps[g_intermission_state.num_maps++], name, MAX_QPATH);
}

/**
 * @brief The `name` of the rotation entry at `index`, or `NULL`.
 */
static const char *G_Intermission_MapAt(const List *list, int32_t index) {

  const ListNode *node = list->head;
  for (int32_t i = 0; i < index && node; i++) {
    node = node->next;
  }

  if (node == NULL) {
    return NULL;
  }

  return gi.EntityValue((const cm_entity_t *) node->element, "name")->string;
}

/**
 * @brief Fills the candidates from the server's rotation.
 * @details The rotation is matched by name, which is how the server resolves the map we
 * hand back to it, so the two agree on which entry a name means.
 */
static void G_Intermission_SelectMaps(void) {

  g_intermission_state.num_maps = 0;

  const int32_t wanted = g_intermission_state.voting ? MAX_NEXT_MAPS : 1;

  List *list = gi.MapList();
  if (list) {
    const int32_t length = (int32_t) list->count;

    int32_t current = -1;
    for (int32_t i = 0; i < length; i++) {
      if (!q_strcmp(G_Intermission_MapAt(list, i) ?: "", g_level.name)) {
        current = i;
        break;
      }
    }

    if (gi.GetCvarInteger("sv_map_list_shuffle")) {
      // a shuffled rotation has no next, so offer a sample of it instead; the ordered
      // pass below tops up whatever the draws duplicated
      for (int32_t i = 0; i < length && g_intermission_state.num_maps < wanted; i++) {
        const int32_t index = (int32_t) RandomRangeu(0, (uint32_t) length);
        if (index != current) {
          G_Intermission_Offer(G_Intermission_MapAt(list, index));
        }
      }
    }

    // in order from wherever we are, which is what the rotation would have played
    for (int32_t i = 1; i <= length && g_intermission_state.num_maps < wanted; i++) {
      const int32_t index = (current + i) % length;
      if (index != current) {
        G_Intermission_Offer(G_Intermission_MapAt(list, index));
      }
    }

    for (const ListNode *node = list->head; node; node = node->next) {
      gi.FreeEntity((cm_entity_t *) node->element);
    }

    release(list);
  }

  if (g_intermission_state.num_maps == 0) {
    // no rotation, or nothing in it we can serve: the server replays this map
    q_strlcpy(g_intermission_state.maps[0], g_level.name, MAX_QPATH);
    g_intermission_state.num_maps = 1;
  }

  // one candidate is not a choice
  g_intermission_state.voting &= g_intermission_state.num_maps > 1;
}

/**
 * @brief Counts the ballots cast for each candidate.
 */
static void G_Intermission_Count(int32_t *votes) {

  memset(votes, 0, sizeof(int32_t) * MAX_NEXT_MAPS);

  G_ForEachClient(cl, {
    const int32_t ballot = g_intermission_state.ballots[cl->ps.client];

    if (ballot != BALLOT_NONE && ballot < g_intermission_state.num_maps && G_Vote_Eligible(cl)) {
      votes[ballot]++;
    }
  });
}

/**
 * @brief Publishes the candidates and the tally to the clients, or their absence.
 */
static void G_Intermission_Publish(void) {

  if (!g_intermission_state.active) {
    gi.SetConfigString(CS_NEXT_MAP, "");
    return;
  }

  int32_t votes[MAX_NEXT_MAPS];
  G_Intermission_Count(votes);

  memcpy(g_intermission_state.published, votes, sizeof(votes));

  char string[MAX_STRING_CHARS];
  q_snprintf(string, sizeof(string), "%d", g_intermission_state.voting ? 1 : 0);

  for (int32_t i = 0; i < g_intermission_state.num_maps; i++) {
    q_strlcat(string, va("\\%s\\%d", g_intermission_state.maps[i], votes[i]), sizeof(string));
  }

  gi.SetConfigString(CS_NEXT_MAP, string);
}

/**
 * @brief Opens the intermission, choosing what it offers.
 */
static void G_Intermission_Begin(void) {

  g_intermission_state.active = true;
  g_intermission_state.voting = g_vote_next_map->integer && g_vote->integer;

  for (int32_t i = 0; i < MAX_CLIENTS; i++) {
    g_intermission_state.ballots[i] = BALLOT_NONE;
  }

  G_Intermission_SelectMaps();
  G_Intermission_Publish();

  if (g_intermission_state.voting) {
    gi.BroadcastPrint(PRINT_HIGH, "Press 1 - %d to vote for the next map\n",
                      g_intermission_state.num_maps);
  }
}

/**
 * @brief Closes the intermission, naming the map the server should serve next.
 * @details The winner is the candidate with the most ballots, ties going to the one the
 * rotation would have played anyway. This runs in the frame that ends the intermission,
 * before the queued `next_map` is executed.
 */
static void G_Intermission_End(void) {

  int32_t votes[MAX_NEXT_MAPS];
  G_Intermission_Count(votes);

  int32_t winner = 0, cast = votes[0];
  for (int32_t i = 1; i < g_intermission_state.num_maps; i++) {
    cast += votes[i];

    if (votes[i] > votes[winner]) {
      winner = i;
    }
  }

  if (g_intermission_state.voting) {
    gi.BroadcastPrint(PRINT_HIGH, "%s wins the vote with %d of %d\n",
                      g_intermission_state.maps[winner], votes[winner], cast);
  }

  gi.SetNextMap(g_intermission_state.maps[winner]);

  g_intermission_state.active = false;
  g_intermission_state.voting = false;
  g_intermission_state.num_maps = 0;

  G_Intermission_Publish();
}

/**
 * @brief Records a ballot, which a client may change until the intermission ends.
 */
static void G_Intermission_Cast(g_client_t *cl, int32_t map) {

  if (!g_intermission_state.active || !g_intermission_state.voting) {
    gi.ClientPrint(cl, PRINT_HIGH, "No map vote is in progress\n");
    return;
  }

  if (map < 0 || map >= g_intermission_state.num_maps) {
    gi.ClientPrint(cl, PRINT_HIGH, "Usage: vote_map 1 - %d\n", g_intermission_state.num_maps);
    return;
  }

  if (!G_Vote_Eligible(cl)) {
    return;
  }

  if (g_intermission_state.ballots[cl->ps.client] == map) {
    return;
  }

  g_intermission_state.ballots[cl->ps.client] = map;

  gi.ClientPrint(cl, PRINT_HIGH, "You voted for %s\n", g_intermission_state.maps[map]);

  G_Intermission_Publish();
}

/**
 * @brief `vote_map <n>`, where `n` is the candidate as the client game numbers them.
 */
static bool G_HandleClientCommand_Intermission(g_client_t *cl, const char *cmd) {

  if (q_strcmp(cmd, "vote_map")) {
    return previous.HandleClientCommand(cl, cmd);
  }

  if (gi.Argc() < 2) {
    gi.ClientPrint(cl, PRINT_HIGH, "Usage: vote_map <number>\n");
    return true;
  }

  G_Intermission_Cast(cl, (int32_t) strtol(gi.Argv(1), NULL, 10) - 1);

  return true;
}

/**
 * @brief Opens and closes the intermission around the level's own clock, and keeps the
 * countdown and the tally published while it runs.
 */
static void G_FrameDidEnd_Intermission(void) {

  if (g_level.intermission_time) {

    if (!g_intermission_state.active) {
      G_Intermission_Begin();
    }

    // the match clock stops at the intermission, so the config string is free to carry
    // this one instead; the HUD that reads it is hidden by then
    if (g_level.frame_num % QUETOO_TICK_RATE == 0) {
      const uint32_t end = g_intermission_state.active ? g_level.intermission_time + INTERMISSION : 0;
      gi.SetConfigString(CS_TIME, G_FormatTime(end > g_level.time ? end - g_level.time : 0));
    }

    int32_t votes[MAX_NEXT_MAPS];
    G_Intermission_Count(votes);

    if (memcmp(votes, g_intermission_state.published, sizeof(votes))) {
      G_Intermission_Publish();
    }

  } else if (g_intermission_state.active) {
    G_Intermission_End();
  }

  previous.FrameDidEnd();
}

/**
 * @brief A leaving client's ballot leaves with them, so that it can neither decide the
 * vote nor be inherited by whoever takes their slot.
 */
static void G_ClientWillDisconnect_Intermission(g_client_t *cl) {

  if (g_intermission_state.ballots[cl->ps.client] != BALLOT_NONE) {
    g_intermission_state.ballots[cl->ps.client] = BALLOT_NONE;

    G_Intermission_Publish();
  }

  previous.ClientWillDisconnect(cl);
}

/**
 * @brief An intermission does not outlive its level.
 */
static void G_ConfigureLevel_Intermission(void) {

  g_intermission_state.active = false;
  g_intermission_state.voting = false;
  g_intermission_state.num_maps = 0;

  G_Intermission_Publish();

  previous.ConfigureLevel();
}

/**
 * @brief Installs the intermission over the hooks it needs, once per module image.
 */
void G_Intermission_Init(void) {

  g_vote_next_map = gi.AddCvar("g_vote_next_map", "0", CVAR_SERVER_INFO,
                               "Whether clients vote for the next map during the intermission.");

  if (!installed) {
    installed = true;

    previous.HandleClientCommand = G_HandleClientCommand;
    G_HandleClientCommand = G_HandleClientCommand_Intermission;

    previous.FrameDidEnd = G_FrameDidEnd;
    G_FrameDidEnd = G_FrameDidEnd_Intermission;

    previous.ClientWillDisconnect = G_ClientWillDisconnect;
    G_ClientWillDisconnect = G_ClientWillDisconnect_Intermission;

    previous.ConfigureLevel = G_ConfigureLevel;
    G_ConfigureLevel = G_ConfigureLevel_Intermission;
  }
}
