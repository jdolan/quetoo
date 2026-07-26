/*
 * Copyright(c) 2026 Quetoo.
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 2 of the License, or
 * (at your option) any later version.
 */

#include "g_local.h"

/**
 * @brief Checks the common frag limit after mode-specific rules run.
 *
 * Objective modes own their scoring limit through G_ModeCheckRules; this
 * component only handles the ordinary team or individual frag limit.
 */
bool G_CheckMatchLimit(void) {
  if (G_ModeHasCapability(G_MODE_CAP_FLAG_OBJECTIVE) || !g_level.frag_limit) {
    return false;
  }

  if (G_ModeTeamplay()) {
    for (int32_t i = 0; i < g_level.num_teams; i++) {
      if (g_team_list[i].score >= g_level.frag_limit) {
        gi.BroadcastPrint(PRINT_HIGH, "Frag limit hit\n");
        return true;
      }
    }
  } else {
    G_ForEachClient(cl, {
      if (cl->persistent.score >= g_level.frag_limit) {
        gi.BroadcastPrint(PRINT_HIGH, "Frag limit hit\n");
        return true;
      }
    });
  }

  return false;
}
