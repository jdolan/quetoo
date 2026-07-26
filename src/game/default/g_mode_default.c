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
 * @brief Legacy flag visibility policy shared by non-objective modes.
 *
 * The item remains in the built-in catalog for compatibility, but its
 * interaction is disabled whenever the objective mode is not active.
 */
bool G_ModeResetLegacyFlagItem(g_mode_t *mode, g_entity_t *ent) {
  const g_mode_context_t *context = G_ModeContext(mode);
  if (!context || !context->level->ctf) {
    ent->sv_flags |= SVF_NO_CLIENT;
    ent->solid = SOLID_NOT;
  }
  return true;
}

static const g_mode_ops_t g_default_mode_ops = {
  .ItemReset = G_ModeResetLegacyFlagItem,
};

static const g_mode_def_t g_default_mode = {
  .name = "default",
  .kind = G_MODE_PRIMARY,
  .gameplay = GAME_DEATHMATCH,
  .ops = &g_default_mode_ops,
};

const g_mode_def_t *G_DefaultModeDefinition(void) {
  return &g_default_mode;
}
