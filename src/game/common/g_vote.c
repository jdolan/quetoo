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

Cvar *g_vote;
Cvar *g_voteTime;
Cvar *g_voteThreshold;
Cvar *g_voteCooldown;

typedef enum {
  BALLOT_NONE,
  BALLOT_YES,
  BALLOT_NO
} GameBallot;

static struct {
  bool active;
  char type[MAX_QPATH];
  char arg[MAX_QPATH];
  char initiator[MAX_NET_NAME];
  uint32_t deadline;
  GameBallot ballots[MAX_CLIENTS];
  uint32_t cooldown[MAX_CLIENTS];
  int32_t published[3]; // yes, no and eligible as last published
} module;

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
bool G_Vote_Eligible(const GameClient *cl) {
  return cl->inUse && !cl->ai;
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
static GameClient *G_Vote_ClientByName(const char *name) {

  G_ForEachClient(cl, {
    if (G_Vote_Eligible(cl) && !q_strcasecmp(cl->persistent.netName, name)) {
      return cl;
    }
  });

  return NULL;
}

/**
 * @brief The common vote type `name` names, or `NULL`.
 */
static const VoteType *G_Vote_Type(const char *name) {

  for (size_t i = 0; i < lengthof(voteTypesCommon); i++) {
    if (!q_strcmp(voteTypesCommon[i].name, name)) {
      return &voteTypesCommon[i];
    }
  }

  return NULL;
}

/**
 * @brief The tail of the `G_PrepareVote` chain: the common votes.
 */
static bool G_PrepareVote_Common(const GameClient *cl, const char *type, const char *arg, char *canonical, size_t size) {

  const VoteType *vote = G_Vote_Type(type);
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
      const GameClient *target = G_Vote_ClientByName(arg);
      if (!target) {
        return false;
      }
      q_strlcpy(canonical, target->persistent.netName, size);
      return true;
    }

    case VOTE_ARG_INTEGER: {
      if (!q_strcmp(type, "bots") && gLevel.minClientsMap > -1) {
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

  const VoteType *vote = G_Vote_Type(type);
  if (!vote) {
    return false;
  }

  if (!q_strcmp(type, "map")) {
    if (!q_strcmp(arg, "next")) {
      gi.Cbuf("nextMap\n");
    } else {
      gi.Cbuf(va("map %s\n", arg));
    }
    return true;
  }

  if (!q_strcmp(type, "bots")) {
    gi.SetCvarInteger("sv_minClients", (int32_t) strtol(arg, NULL, 10));
    return true;
  }

  if (!q_strcmp(type, "spectate")) {
    GameClient *target = G_Vote_ClientByName(arg);
    if (target && !target->persistent.spectator) {
      G_TossInventory(target);
      target->persistent.spectator = true;
      G_ClientRespawn(target, false);
    }
    return true;
  }

  if (!q_strcmp(type, "mute")) {
    GameClient *target = G_Vote_ClientByName(arg);
    if (target) {
      // mute the client the vote resolved, not one G_ClientByName might match a second time
      G_SetClientMuted(target, !target->persistent.muted);

      gi.BroadcastPrint(PRINT_HIGH, "%s is now %smuted\n", target->persistent.netName,
                        target->persistent.muted ? "" : "un");
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

      switch (module.ballots[cl->ps.client]) {
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

  if (!module.active) {
    gi.SetConfigString(CS_VOTE, "");
    return;
  }

  int32_t yes, no, eligible;
  G_Vote_Count(&yes, &no, &eligible);

  module.published[0] = yes;
  module.published[1] = no;
  module.published[2] = eligible;

  gi.SetConfigString(CS_VOTE, va("%s\\%s\\%d\\%d\\%d\\%u\\%s",
                                 module.type, module.arg, yes, no, eligible,
                                 module.deadline, module.initiator));
}

/**
 * @brief Announces the verdict, applies a passed vote, and clears the state either way.
 */
static void G_Vote_End(bool passed) {

  gi.BroadcastPrint(PRINT_HIGH, "Vote %s%s%s %s\n", module.type,
                    *module.arg ? " " : "", module.arg, passed ? "passed" : "failed");

  module.active = false;
  G_Vote_Publish();

  if (passed) {
    if (!G_ApplyVote(module.type, module.arg)) {
      G_Warn("Nobody applied vote %s %s\n", module.type, module.arg);
    }
  }
}

/**
 * @brief Decides the vote once it can be, and at its deadline.
 */
static void G_Vote_Check(void) {

  if (!module.active) {
    return;
  }

  if (gLevel.intermissionTime) { // the level is ending; a vote does not decide it
    gi.BroadcastPrint(PRINT_HIGH, "Vote %s%s%s cancelled\n", module.type,
                      *module.arg ? " " : "", module.arg);
    module.active = false;
    G_Vote_Publish();
    return;
  }

  int32_t yes, no, eligible;
  G_Vote_Count(&yes, &no, &eligible);

  const int32_t needed = (int32_t) floorf(eligible * Clampf(g_voteThreshold->value, 0.f, 1.f)) + 1;

  if (yes >= needed) {
    G_Vote_End(true);
  } else if (eligible - no < needed || gLevel.time >= module.deadline) {
    G_Vote_End(false);
  } else if (yes != module.published[0] || no != module.published[1] || eligible != module.published[2]) {
    G_Vote_Publish();
  }
}

/**
 * @brief Records a ballot, once per client per vote.
 */
static void G_Vote_Cast(GameClient *cl, GameBallot ballot) {

  if (!module.active) {
    gi.ClientPrint(cl, PRINT_HIGH, "No vote is in progress\n");
    return;
  }

  if (!G_Vote_Eligible(cl)) {
    return;
  }

  module.ballots[cl->ps.client] = ballot;

  G_Vote_Publish();
  G_Vote_Check();
}

/**
 * @brief Opens a vote, if voting is enabled and nothing else is in progress.
 */
static void G_Vote_Call(GameClient *cl, const char *type, const char *arg) {

  if (!g_vote->integer) {
    gi.ClientPrint(cl, PRINT_HIGH, "Voting is disabled\n");
    return;
  }

  if (module.active) {
    gi.ClientPrint(cl, PRINT_HIGH, "A vote is already in progress\n");
    return;
  }

  if (!G_Vote_Eligible(cl)) {
    return;
  }

  const uint32_t cooldown = module.cooldown[cl->ps.client];
  if (cooldown && gLevel.time < cooldown) {
    gi.ClientPrint(cl, PRINT_HIGH, "You may call another vote in %u seconds\n", (cooldown - gLevel.time) / 1000);
    return;
  }

  char canonical[MAX_QPATH];
  if (!G_PrepareVote(cl, type, arg, canonical, sizeof(canonical))) {
    gi.ClientPrint(cl, PRINT_HIGH, "Invalid vote: %s %s\n", type, arg);
    return;
  }

  memset(module.ballots, 0, sizeof(module.ballots));

  module.active = true;
  q_strlcpy(module.type, type, sizeof(module.type));
  q_strlcpy(module.arg, canonical, sizeof(module.arg));
  q_strlcpy(module.initiator, cl->persistent.netName, sizeof(module.initiator));
  module.deadline = gLevel.time + Maxf(1.f, g_voteTime->value) * 1000;
  module.ballots[cl->ps.client] = BALLOT_YES;
  module.cooldown[cl->ps.client] = gLevel.time + Maxf(0.f, g_voteCooldown->value) * 1000;

  gi.BroadcastPrint(PRINT_HIGH, "%s called a vote: %s%s%s\n", cl->persistent.netName, type,
                    *canonical ? " " : "", canonical);

  G_Vote_Publish();
  G_Vote_Check();
}

/**
 * @brief `vote yes`, `vote no`, or `vote <type> [argument]`.
 */
static bool G_HandleClientCommand_Vote(GameClient *cl, const char *cmd) {

  if (q_strcmp(cmd, "vote")) {
    return previous.HandleClientCommand(cl, cmd);
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
  } else if (gLevel.intermissionTime) {
    gi.ClientPrint(cl, PRINT_HIGH, "The level is ending\n");
  } else {
    G_Vote_Call(cl, what, gi.Argc() > 2 ? gi.Argv(2) : "");
  }

  return true;
}

/**
 * @brief Watches the deadline and the tally, and ends the vote that settles.
 */
static void G_FrameDidEnd_Vote(void) {

  G_Vote_Check();

  previous.FrameDidEnd();
}

/**
 * @brief A leaving client's ballot no longer counts, and their cooldown ends
 * with them so that a reconnecting client is not held to it.
 */
static void G_ClientWillDisconnect_Vote(GameClient *cl) {

  module.ballots[cl->ps.client] = BALLOT_NONE;
  module.cooldown[cl->ps.client] = 0;

  previous.ClientWillDisconnect(cl);
}

/**
 * @brief A vote does not outlive its level.
 */
static void G_ConfigureLevel_Vote(void) {

  module.active = false;
  memset(module.cooldown, 0, sizeof(module.cooldown));

  G_Vote_Publish();

  previous.ConfigureLevel();
}

/**
 * @brief Installs voting over the hooks it needs, once per module image.
 */
void G_Vote_Init(void) {

  g_vote = gi.AddCvar("g_vote", "1", CVAR_SERVER_INFO, "Whether clients may call votes.");
  g_voteTime = gi.AddCvar("g_voteTime", "30", 0, "How long a vote runs, in seconds.");
  g_voteThreshold = gi.AddCvar("g_voteThreshold", "0.5", 0, "The fraction of eligible clients whose yes a vote must exceed to pass; 1 lets nothing pass.");
  g_voteCooldown = gi.AddCvar("g_voteCooldown", "60", 0, "How long a client waits between calling votes, in seconds.");

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
