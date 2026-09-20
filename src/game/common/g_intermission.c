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

Cvar *g_voteNextMap;

#define BALLOT_NONE -1

static struct {
  bool active;
  bool voting;
  char maps[MAX_NEXT_MAPS][MAX_QPATH];
  int32_t indices[MAX_NEXT_MAPS];
  int32_t numMaps;
  int32_t ballots[MAX_CLIENTS];
  int32_t published[MAX_NEXT_MAPS];
} module;

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

  for (int32_t i = 0; i < module.numMaps; i++) {
    if (!q_strcmp(module.maps[i], name)) {
      return true;
    }
  }

  return false;
}

/**
 * @brief Offers `name`, if it is a map this server can actually serve and is not already
 * offered. A rotation may name maps that were never installed; `nextMap` does not check,
 * but a vote would let clients elect one.
 */
static void G_Intermission_Offer(const char *name, int32_t index) {

  if (module.numMaps == MAX_NEXT_MAPS) {
    return;
  }

  // by name rather than by position, since a rotation may list either the map we are
  // on or a candidate more than once, and neither is a second thing to vote for
  if (!name || !*name || !q_strcmp(name, gLevel.name) || G_Intermission_Offers(name)) {
    return;
  }

  if (!gi.FileExists(va("maps/%s.bsp", name))) {
    G_Debug("Skipping %s, which is not installed\n", name);
    return;
  }

  module.indices[module.numMaps] = index;
  q_strlcpy(module.maps[module.numMaps++], name, MAX_QPATH);
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

  return gi.EntityValue((const CmEntity *) node->element, "name")->string;
}

/**
 * @brief Fills the candidates from the server's rotation.
 * @details Where we are is the server's index rather than our name, since a rotation
 * may name this map more than once, and the occurrences are different places to
 * resume from.
 */
static void G_Intermission_SelectMaps(void) {

  module.numMaps = 0;

  const int32_t wanted = module.voting ? MAX_NEXT_MAPS : 1;

  List *list = gi.MapList();
  if (list && list->count) {
    const int32_t length = (int32_t) list->count;

    // -1 when this level did not come from the rotation, which starts us at its head
    const int32_t current = gi.MapIndex();

    if (gi.GetCvarInteger("sv_mapListShuffle")) {
      // a shuffled rotation has no next, so offer a sample of it instead; the ordered
      // pass below tops up whatever the draws duplicated
      for (int32_t i = 0; i < length && module.numMaps < wanted; i++) {
        const int32_t index = (int32_t) RandomRangeu(0, (uint32_t) length);
        G_Intermission_Offer(G_Intermission_MapAt(list, index), index);
      }
    }

    // in order from wherever we are, which is what the rotation would have played
    for (int32_t i = 1; i <= length && module.numMaps < wanted; i++) {
      const int32_t index = (current + i) % length;
      G_Intermission_Offer(G_Intermission_MapAt(list, index), index);
    }

    for (const ListNode *node = list->head; node; node = node->next) {
      gi.FreeEntity((CmEntity *) node->element);
    }

    release(list);
  }

  if (module.numMaps == 0) {
    // no rotation, or nothing in it we can serve: the server replays this map, which
    // is what `nextMap` falls back to on its own, so we leave it to do that
    module.indices[0] = -1;
    q_strlcpy(module.maps[0], gLevel.name, MAX_QPATH);
    module.numMaps = 1;
  }

  // one candidate is not a choice
  module.voting &= module.numMaps > 1;
}

/**
 * @brief Counts the ballots cast for each candidate.
 */
static void G_Intermission_Count(int32_t *votes) {

  memset(votes, 0, sizeof(int32_t) * MAX_NEXT_MAPS);

  G_ForEachClient(cl, {
    const int32_t ballot = module.ballots[cl->ps.client];

    if (ballot != BALLOT_NONE && ballot < module.numMaps && G_Vote_Eligible(cl)) {
      votes[ballot]++;
    }
  });
}

/**
 * @brief Publishes the candidates and the tally to the clients, or their absence.
 */
static void G_Intermission_Publish(void) {

  if (!module.active) {
    gi.SetConfigString(CS_NEXT_MAP, "");
    return;
  }

  int32_t votes[MAX_NEXT_MAPS];
  G_Intermission_Count(votes);

  memcpy(module.published, votes, sizeof(votes));

  char string[MAX_STRING_CHARS];
  q_snprintf(string, sizeof(string), "%d", module.voting ? 1 : 0);

  for (int32_t i = 0; i < module.numMaps; i++) {
    q_strlcat(string, va("\\%s\\%d", module.maps[i], votes[i]), sizeof(string));
  }

  G_Debug("%s\n", string);

  gi.SetConfigString(CS_NEXT_MAP, string);
}

/**
 * @brief Publishes the intermission's countdown.
 * @remarks The match clock stops at the intermission, so its config string is free to
 * carry this one instead; the HUD that reads it is hidden by then.
 */
static void G_Intermission_PublishTime(void) {

  const uint32_t end = gLevel.intermissionTime + INTERMISSION;

  gi.SetConfigString(CS_TIME, G_FormatTime(end > gLevel.time ? end - gLevel.time : 0));
}

/**
 * @brief Opens the intermission, choosing what it offers.
 */
static void G_Intermission_Begin(void) {

  module.active = true;
  module.voting = g_voteNextMap->integer;

  for (int32_t i = 0; i < MAX_CLIENTS; i++) {
    module.ballots[i] = BALLOT_NONE;
  }

  G_Intermission_SelectMaps();

  // before the maps, since publishing those is what shows the view, and the clock it
  // reads still holds the match time until the next tick
  G_Intermission_PublishTime();

  G_Intermission_Publish();

  if (module.voting) {
    gi.BroadcastPrint(PRINT_HIGH, "Press 1 - %d to vote for the next map\n",
                      module.numMaps);
  }
}

/**
 * @brief Closes the intermission, naming the map the server should serve next.
 * @details The winner is the candidate with the most ballots, ties going to the one the
 * rotation would have played anyway. This runs in the frame that ends the intermission,
 * before the queued `nextMap` is executed.
 */
static void G_Intermission_End(void) {

  int32_t votes[MAX_NEXT_MAPS];
  G_Intermission_Count(votes);

  int32_t winner = 0, cast = votes[0];
  for (int32_t i = 1; i < module.numMaps; i++) {
    cast += votes[i];

    if (votes[i] > votes[winner]) {
      winner = i;
    }
  }

  if (module.voting) {
    gi.BroadcastPrint(PRINT_HIGH, "%s wins the vote with %d of %d\n",
                      module.maps[winner], votes[winner], cast);
  }

  if (module.indices[winner] >= 0) {
    gi.SetNextMap(module.indices[winner]);
  }

  module.active = false;
  module.voting = false;
  module.numMaps = 0;

  G_Intermission_Publish();
}

/**
 * @brief Records a ballot, which a client may change until the intermission ends.
 */
static void G_Intermission_Cast(GameClient *cl, int32_t map) {

  if (!module.active || !module.voting) {
    gi.ClientPrint(cl, PRINT_HIGH, "No map vote is in progress\n");
    return;
  }

  if (map < 0 || map >= module.numMaps) {
    gi.ClientPrint(cl, PRINT_HIGH, "Usage: voteMap 1 - %d\n", module.numMaps);
    return;
  }

  if (!G_Vote_Eligible(cl)) {
    return;
  }

  if (module.ballots[cl->ps.client] == map) {
    return;
  }

  module.ballots[cl->ps.client] = map;

  gi.ClientPrint(cl, PRINT_HIGH, "You voted for %s\n", module.maps[map]);

  G_Intermission_Publish();
}

/**
 * @brief `voteMap <n>`, where `n` is the candidate as the client game numbers them.
 */
static bool G_HandleClientCommand_Intermission(GameClient *cl, const char *cmd) {

  if (q_strcmp(cmd, "voteMap")) {
    return previous.HandleClientCommand(cl, cmd);
  }

  if (gi.Argc() < 2) {
    gi.ClientPrint(cl, PRINT_HIGH, "Usage: voteMap <number>\n");
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

  if (gLevel.intermissionTime) {

    if (!module.active) {
      G_Intermission_Begin();
    }

    if (gLevel.frameNum % QUETOO_TICK_RATE == 0) {
      G_Intermission_PublishTime();
    }

    int32_t votes[MAX_NEXT_MAPS];
    G_Intermission_Count(votes);

    if (memcmp(votes, module.published, sizeof(votes))) {
      G_Intermission_Publish();
    }

  } else if (module.active) {
    G_Intermission_End();
  }

  previous.FrameDidEnd();
}

/**
 * @brief A leaving client's ballot leaves with them, so that it can neither decide the
 * vote nor be inherited by whoever takes their slot.
 */
static void G_ClientWillDisconnect_Intermission(GameClient *cl) {

  if (module.ballots[cl->ps.client] != BALLOT_NONE) {
    module.ballots[cl->ps.client] = BALLOT_NONE;

    G_Intermission_Publish();
  }

  previous.ClientWillDisconnect(cl);
}

/**
 * @brief An intermission does not outlive its level.
 */
static void G_ConfigureLevel_Intermission(void) {

  module.active = false;
  module.voting = false;
  module.numMaps = 0;

  G_Intermission_Publish();

  previous.ConfigureLevel();
}

/**
 * @brief Installs the intermission over the hooks it needs, once per module image.
 */
void G_Intermission_Init(void) {

  g_voteNextMap = gi.AddCvar("g_voteNextMap", "1", CVAR_SERVER_INFO,
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
