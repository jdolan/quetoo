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
 * @brief Reusable fixed-team component helpers.
 *
 * Modes can replace assignment through g_mode_ops_t::AssignTeam, while the
 * legacy default adapter continues to use these common helpers.
 */
size_t G_TeamSize(const g_team_t *team) {
  size_t count = 0;

  G_ForEachClient(cl, {
    if (cl->persistent.team == team) {
      count++;
    }
  });

  return count;
}

g_team_t *G_SmallestTeam(void) {
  g_team_t *smallest = NULL;
  size_t size = SIZE_MAX;

  g_team_t *team = g_team_list;
  for (int32_t i = 0; i < g_level.num_teams; i++, team++) {
    const size_t s = G_TeamSize(team);
    if (s < size) {
      smallest = team;
      size = s;
    }
  }

  return smallest;
}
