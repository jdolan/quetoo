/*
 * Copyright(c) 2026 Quetoo.
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 2 of the License, or
 * (at your option) any later version.
 */

#include "g_local.h"

static bool G_InstagibClientInventory(g_mode_t *mode, g_client_t *cl,
                                      const g_item_t **starting_weapon) {
  G_Give(cl, "Railgun", 1);
  G_Give(cl, "Grenades", 1);
  *starting_weapon = &G_ModeContext(mode)->items[WEAPON_RAILGUN];
  return true;
}

static const g_mode_ops_t g_instagib_mode_ops = {
  .ClientInventory = G_InstagibClientInventory,
  .ItemReset = G_ModeResetLegacyFlagItem,
};

static const g_mode_def_t g_instagib_mode = {
  .name = "instagib",
  .kind = G_MODE_MODIFIER,
  .gameplay_selector = true,
  .gameplay = GAME_INSTAGIB,
  .capabilities = G_MODE_CAP_INSTAGIB | G_MODE_CAP_NO_AMMO |
                  G_MODE_CAP_NO_SELF_DAMAGE | G_MODE_CAP_SUPPRESS_ITEMS,
  .ops = &g_instagib_mode_ops,
};

const g_mode_def_t *G_InstagibModeDefinition(void) {
  return &g_instagib_mode;
}
