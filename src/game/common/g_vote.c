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

#include <ctype.h>

#include "g_local.h"
#include "bg_vote.h"

cvar_t *g_vote;
cvar_t *g_vote_time;
cvar_t *g_vote_threshold;
cvar_t *g_vote_cooldown;

typedef enum {
  BALLOT_NONE,
  BALLOT_YES,
  BALLOT_NO
} g_ballot_t;

static struct {
  bool active;
  char type[MAX_QPATH];
  char arg[MAX_QPATH];
  char initiator[MAX_NET_NAME];
  uint32_t deadline;
  g_ballot_t ballots[MAX_CLIENTS];
  uint32_t cooldown[MAX_CLIENTS];
  int32_t published[3]; // yes, no and eligible as last published
} g_vote_state;

static struct {
  HandleClientCommand HandleClientCommand;
  FrameDidEnd FrameDidEnd;
  ClientWillDisconnect ClientWillDisconnect;
  ConfigureLevel ConfigureLevel;
} previous;

static bool installed;

/**
 * @brief Connected human players and spectators may vote; bots may not.
 */
static bool G_Vote_Eligible(const g_client_t *cl) {
  return cl->in_use && !cl->ai;
}

/**
 * @brief A map name is a bare file name: letters, digits, underscores and dashes.
 */
static bool G_Vote_ValidMapName(const char *name) {

  if (!*name || q_strlen(name) >= MAX_QPATH) {
    return false;
  }

  for (const char *c = name; *c; c++) {
    if (!isalnum((unsigned char) *c) && *c != '_' && *c != '-') {
      return false;
    }
  }

  return true;
}

/**
 * @brief The eligible client with exactly this name, or `NULL`. A vote that
 * could land on the nearest name would be a vote on somebody else.
 */
static g_client_t *G_Vote_ClientByName(const char *name) {

  G_ForEachClient(cl, {
    if (G_Vote_Eligible(cl) && !q_strcasecmp(cl->persistent.net_name, name)) {
      return cl;
    }
  });

  return NULL;
}

static const vote_type_t *G_Vote_Type(const char *name) {

  for (size_t i = 0; i < lengthof(vote_types_common); i++) {
    if (!q_strcmp(vote_types_common[i].name, name)) {
      return &vote_types_common[i];
    }
  }

  return NULL;
}

/**
 * @brief The tail of the `G_PrepareVote` chain: the common votes.
 */
static bool G_PrepareVote_Common(const g_client_t *cl, const char *type, const char *arg, char *canonical, size_t size) {

  const vote_type_t *vote = G_Vote_Type(type);
  if (!vote) {
    return false;
  }

  switch (vote->arg) {
    case VOTE_ARG_NONE:
      canonical[0] = '\0';
      return true;

    case VOTE_ARG_MAP:
      if (!q_strcmp(arg, "next")) {
        q_strlcpy(canonical, arg, size);
        return true;
      }
      if (!G_Vote_ValidMapName(arg) || !gi.FileExists(va("maps/%s.bsp", arg))) {
        return false;
      }
      q_strlcpy(canonical, arg, size);
      return true;

    case VOTE_ARG_CLIENT: {
      const g_client_t *target = G_Vote_ClientByName(arg);
      if (!target) {
        return false;
      }
      q_strlcpy(canonical, target->persistent.net_name, size);
      return true;
    }

    case VOTE_ARG_INTEGER: {
      if (!q_strcmp(type, "bots") && g_level.min_clients_map > -1) {
        return false;
      }

      char *end;
      const long value = strtol(arg, &end, 10);
      if (end == arg || *end || value < vote->min || value > vote->max) {
        return false;
      }
      q_snprintf(canonical, size, "%ld", value);
      return true;
    }
  }

  return false;
}

PrepareVote G_PrepareVote = G_PrepareVote_Common;

/**
 * @brief The tail of the `G_ApplyVote` chain: the common votes.
 */
static bool G_ApplyVote_Common(const char *type, const char *arg) {

  const vote_type_t *vote = G_Vote_Type(type);
  if (!vote) {
    return false;
  }

  if (!q_strcmp(type, "map")) {
    if (!q_strcmp(arg, "next")) {
      gi.Cbuf("next_map\n");
    } else {
      gi.Cbuf(va("map %s\n", arg));
    }
    return true;
  }

  if (!q_strcmp(type, "bots")) {
    gi.SetCvarInteger("sv_min_clients", (int32_t) strtol(arg, NULL, 10));
    return true;
  }

  if (!q_strcmp(type, "spectate")) {
    g_client_t *target = G_Vote_ClientByName(arg);
    if (target && !target->persistent.spectator) {
      G_TossInventory(target);
      target->persistent.spectator = true;
      G_ClientRespawn(target, false);
    }
    return true;
  }

  if (!q_strcmp(type, "frag_limit") || !q_strcmp(type, "time_limit")) {
    gi.SetCvarInteger(va("g_%s", type), (int32_t) strtol(arg, NULL, 10));
    return true;
  }

  return false;
}

ApplyVote G_ApplyVote = G_ApplyVote_Common;

/**
 * @brief Counts the ballots and the clients entitled to cast one.
 */
static void G_Vote_Count(int32_t *yes, int32_t *no, int32_t *eligible) {

  *yes = *no = *eligible = 0;

  G_ForEachClient(cl, {
    if (G_Vote_Eligible(cl)) {
      (*eligible)++;

      switch (g_vote_state.ballots[cl->ps.client]) {
        case BALLOT_YES:
          (*yes)++;
          break;
        case BALLOT_NO:
          (*no)++;
          break;
        default:
          break;
      }
    }
  });
}

/**
 * @brief Publishes the vote to the clients, or its absence.
 */
static void G_Vote_Publish(void) {

  if (!g_vote_state.active) {
    gi.SetConfigString(CS_VOTE, "");
    return;
  }

  int32_t yes, no, eligible;
  G_Vote_Count(&yes, &no, &eligible);

  g_vote_state.published[0] = yes;
  g_vote_state.published[1] = no;
  g_vote_state.published[2] = eligible;

  gi.SetConfigString(CS_VOTE, va("%s\\%s\\%d\\%d\\%d\\%u\\%s",
                                 g_vote_state.type, g_vote_state.arg, yes, no, eligible,
                                 g_vote_state.deadline, g_vote_state.initiator));
}

static void G_Vote_End(bool passed) {

  gi.BroadcastPrint(PRINT_HIGH, "Vote %s%s%s %s\n", g_vote_state.type,
                    *g_vote_state.arg ? " " : "", g_vote_state.arg, passed ? "passed" : "failed");

  g_vote_state.active = false;
  G_Vote_Publish();

  if (passed) {
    if (!G_ApplyVote(g_vote_state.type, g_vote_state.arg)) {
      G_Warn("Nobody applied vote %s %s\n", g_vote_state.type, g_vote_state.arg);
    }
  }
}

/**
 * @brief Decides the vote once it can be, and at its deadline.
 */
static void G_Vote_Check(void) {

  if (!g_vote_state.active) {
    return;
  }

  if (g_level.intermission_time) { // the level is ending; a vote does not decide it
    gi.BroadcastPrint(PRINT_HIGH, "Vote %s%s%s cancelled\n", g_vote_state.type,
                      *g_vote_state.arg ? " " : "", g_vote_state.arg);
    g_vote_state.active = false;
    G_Vote_Publish();
    return;
  }

  int32_t yes, no, eligible;
  G_Vote_Count(&yes, &no, &eligible);

  const int32_t needed = (int32_t) floorf(eligible * Clampf(g_vote_threshold->value, 0.f, 1.f)) + 1;

  if (yes >= needed) {
    G_Vote_End(true);
  } else if (eligible - no < needed || g_level.time >= g_vote_state.deadline) {
    G_Vote_End(false);
  } else if (yes != g_vote_state.published[0] || no != g_vote_state.published[1] || eligible != g_vote_state.published[2]) {
    G_Vote_Publish();
  }
}

static void G_Vote_Cast(g_client_t *cl, g_ballot_t ballot) {

  if (!g_vote_state.active) {
    gi.ClientPrint(cl, PRINT_HIGH, "No vote is in progress\n");
    return;
  }

  if (!G_Vote_Eligible(cl)) {
    return;
  }

  g_vote_state.ballots[cl->ps.client] = ballot;

  G_Vote_Publish();
  G_Vote_Check();
}

static void G_Vote_Call(g_client_t *cl, const char *type, const char *arg) {

  if (!g_vote->integer) {
    gi.ClientPrint(cl, PRINT_HIGH, "Voting is disabled\n");
    return;
  }

  if (g_vote_state.active) {
    gi.ClientPrint(cl, PRINT_HIGH, "A vote is already in progress\n");
    return;
  }

  if (!G_Vote_Eligible(cl)) {
    return;
  }

  const uint32_t cooldown = g_vote_state.cooldown[cl->ps.client];
  if (cooldown && g_level.time < cooldown) {
    gi.ClientPrint(cl, PRINT_HIGH, "You may call another vote in %u seconds\n", (cooldown - g_level.time) / 1000);
    return;
  }

  char canonical[MAX_QPATH];
  if (!G_PrepareVote(cl, type, arg, canonical, sizeof(canonical))) {
    gi.ClientPrint(cl, PRINT_HIGH, "Invalid vote: %s %s\n", type, arg);
    return;
  }

  memset(g_vote_state.ballots, 0, sizeof(g_vote_state.ballots));

  g_vote_state.active = true;
  q_strlcpy(g_vote_state.type, type, sizeof(g_vote_state.type));
  q_strlcpy(g_vote_state.arg, canonical, sizeof(g_vote_state.arg));
  q_strlcpy(g_vote_state.initiator, cl->persistent.net_name, sizeof(g_vote_state.initiator));
  g_vote_state.deadline = g_level.time + Maxf(1.f, g_vote_time->value) * 1000;
  g_vote_state.ballots[cl->ps.client] = BALLOT_YES;
  g_vote_state.cooldown[cl->ps.client] = g_level.time + Maxf(0.f, g_vote_cooldown->value) * 1000;

  gi.BroadcastPrint(PRINT_HIGH, "%s called a vote: %s%s%s\n", cl->persistent.net_name, type,
                    *canonical ? " " : "", canonical);

  G_Vote_Publish();
  G_Vote_Check();
}

/**
 * @brief `vote yes`, `vote no`, or `vote <type> [argument]`.
 */
static bool G_HandleClientCommand_Vote(g_client_t *cl, const char *cmd, bool intermission) {

  if (q_strcmp(cmd, "vote")) {
    return previous.HandleClientCommand(cl, cmd, intermission);
  }

  if (gi.Argc() < 2) {
    gi.ClientPrint(cl, PRINT_HIGH, "Usage: vote yes | no | <type> [argument]\n");
    return true;
  }

  const char *what = gi.Argv(1);

  if (!q_strcasecmp(what, "yes")) {
    G_Vote_Cast(cl, BALLOT_YES);
  } else if (!q_strcasecmp(what, "no")) {
    G_Vote_Cast(cl, BALLOT_NO);
  } else if (intermission) {
    gi.ClientPrint(cl, PRINT_HIGH, "The level is ending\n");
  } else {
    G_Vote_Call(cl, what, gi.Argc() > 2 ? gi.Argv(2) : "");
  }

  return true;
}

static void G_FrameDidEnd_Vote(void) {

  G_Vote_Check();

  previous.FrameDidEnd();
}

/**
 * @brief A leaving client's ballot no longer counts, and their cooldown ends
 * with them so that a reconnecting client is not held to it.
 */
static void G_ClientWillDisconnect_Vote(g_client_t *cl) {

  g_vote_state.ballots[cl->ps.client] = BALLOT_NONE;
  g_vote_state.cooldown[cl->ps.client] = 0;

  previous.ClientWillDisconnect(cl);
}

/**
 * @brief A vote does not outlive its level.
 */
static void G_ConfigureLevel_Vote(void) {

  g_vote_state.active = false;
  memset(g_vote_state.cooldown, 0, sizeof(g_vote_state.cooldown));

  G_Vote_Publish();

  previous.ConfigureLevel();
}

/**
 * @brief Installs voting over the hooks it needs, once per module image.
 */
void G_Vote_Init(void) {

  g_vote = gi.AddCvar("g_vote", "1", CVAR_SERVER_INFO, "Whether clients may call votes.");
  g_vote_time = gi.AddCvar("g_vote_time", "30", 0, "How long a vote runs, in seconds.");
  g_vote_threshold = gi.AddCvar("g_vote_threshold", "0.5", 0, "The fraction of eligible clients whose yes a vote must exceed to pass; 1 lets nothing pass.");
  g_vote_cooldown = gi.AddCvar("g_vote_cooldown", "60", 0, "How long a client waits between calling votes, in seconds.");

  if (!installed) {
    installed = true;

    previous.HandleClientCommand = G_HandleClientCommand;
    G_HandleClientCommand = G_HandleClientCommand_Vote;

    previous.FrameDidEnd = G_FrameDidEnd;
    G_FrameDidEnd = G_FrameDidEnd_Vote;

    previous.ClientWillDisconnect = G_ClientWillDisconnect;
    G_ClientWillDisconnect = G_ClientWillDisconnect_Vote;

    previous.ConfigureLevel = G_ConfigureLevel;
    G_ConfigureLevel = G_ConfigureLevel_Vote;
  }
}
