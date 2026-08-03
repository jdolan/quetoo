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
static bool G_CheckCvars_Default(void) {
  return false;
}

CheckCvars G_CheckCvars = G_CheckCvars_Default;

/**
 * @brief The tail of the `G_CheckWinCondition` chain, playing for frags.
 */
static bool G_CheckWinCondition_Default(void) {

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

CheckWinCondition G_CheckWinCondition = G_CheckWinCondition_Default;

/**
 * @brief The tail of the `G_FormatGameName` chain, qualifying the gameplay with
 * team play when the module is playing it.
 */
static void G_FormatGameName_Default(char *name, size_t size) {

  if (g_level.teams) {
    q_strlcpy(name, va("Team %s", name), size);
  }
}

FormatGameName G_FormatGameName = G_FormatGameName_Default;
