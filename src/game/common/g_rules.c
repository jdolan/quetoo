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

/**
 * @brief The rules a module enforces each frame, and the tails of the chains a
 * feature installs over to have a say in them.
 */

/**
 * @brief The tail of the `G_CheckCvars` chain. A module's own cvars are its own
 * business; this answers for the features it did not build.
 */
static bool G_CheckCvars_Common(void) {
  return false;
}

CheckCvars G_CheckCvars = G_CheckCvars_Common;

/**
 * @brief The tail of the `G_CheckWinner` chain, playing for frags.
 */
static bool G_CheckWinner_Common(void) {

  if (g_level.frag_limit) {

    if (g_level.teams) { // check team scores
      for (int32_t i = 0; i < g_level.num_teams; i++) {
        if (g_team_list[i].score >= g_level.frag_limit) {
          gi.BroadcastPrint(PRINT_HIGH, "Frag limit hit\n");
          return true;
        }
      }
    } else { // or individual scores
      G_ForEachClient(cl, {
        if (cl->persistent.score >= g_level.frag_limit) {
          gi.BroadcastPrint(PRINT_HIGH, "Frag limit hit\n");
          return true;
        }
      });
    }
  }

  return false;
}

CheckWinner G_CheckWinner = G_CheckWinner_Common;

/**
 * @brief The tail of the `G_ClampGameplay` hook: every mode `g_gameplay_t`
 * defines is one this module supports, so there is nothing to coerce.
 */
static g_gameplay_t G_ClampGameplay_Common(g_gameplay_t gameplay) {
  return gameplay;
}

ClampGameplay G_ClampGameplay = G_ClampGameplay_Common;

/**
 * @brief The tail of the `G_FormatGameName` chain. `G_GameplayName` already
 * qualifies the name with team play via the `GAME_TEAMS` bit, so this has
 * nothing to add; a feature can still hook this chain to name its own mode.
 */
static void G_FormatGameName_Common(char *name, size_t size) {
}

FormatGameName G_FormatGameName = G_FormatGameName_Common;
