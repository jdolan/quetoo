/*
 * Copyright(c) 2026 Quetoo.
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 2 of the License, or
 * (at your option) any later version.
 */

#include "g_local.h"

static const g_mode_ops_t g_deathmatch_mode_ops = {
  .ItemReset = G_ModeResetLegacyFlagItem,
};

static const g_mode_def_t g_deathmatch_mode = {
  .name = "deathmatch",
  .kind = G_MODE_PRIMARY,
  .gameplay = GAME_DEATHMATCH,
  .ops = &g_deathmatch_mode_ops,
};

const g_mode_def_t *G_DeathmatchModeDefinition(void) {
  return &g_deathmatch_mode;
}
