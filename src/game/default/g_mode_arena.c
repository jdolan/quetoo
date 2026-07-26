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
 * @brief Grant Arena's standard Quetoo weapon and armor roster.
 */
static void G_ArenaGiveQuetooLoadout(g_client_t *cl) {
  G_Give(cl, "Railgun", 50);
  G_Give(cl, "Lightning Gun", 200);
  G_Give(cl, "Hyperblaster", 200);
  G_Give(cl, "Rocket Launcher", 50);
  G_Give(cl, "Hand Grenades", 1);
  G_Give(cl, "Grenade Launcher", 50);
  G_Give(cl, "Machinegun", 200);
  G_Give(cl, "Super Shotgun", 80);
  G_Give(cl, "Shotgun", 80);
  G_Give(cl, "Blaster", 0);
  G_Give(cl, "Body Armor", -1);
}

/**
 * @brief Grant Arena's Quake weapon and armor roster.
 */
static void G_ArenaGiveQuakeLoadout(g_client_t *cl) {
  G_Give(cl, "Thunderbolt", 200);
  G_Give(cl, "Super Nailgun", 200);
  G_Give(cl, "Nailgun", 200);
  G_Give(cl, "Rocket Launcher", 100);
  G_Give(cl, "Grenade Launcher", 100);
  G_Give(cl, "Super Shotgun", 80);
  G_Give(cl, "Shotgun", 80);
  G_Give(cl, "Red Armor", -1);
}

static bool G_ArenaClientInventory(g_mode_t *mode, g_client_t *cl,
                                   const g_item_t **starting_weapon) {
  const g_mode_context_t *context = G_ModeContext(mode);
  if (!context || !context->level || !context->items) {
    return false;
  }

  if (context->level->items == ITEMS_QUAKE) {
    G_ArenaGiveQuakeLoadout(cl);
    *starting_weapon = &context->items[WEAPON_QUAKE_ROCKET_LAUNCHER];
  } else {
    G_ArenaGiveQuetooLoadout(cl);
    *starting_weapon = &context->items[WEAPON_ROCKET_LAUNCHER];
  }

  return true;
}

static const g_mode_ops_t g_arena_mode_ops = {
  .ClientInventory = G_ArenaClientInventory,
  .ItemReset = G_ModeResetLegacyFlagItem,
};

static const g_mode_def_t g_arena_mode = {
  .name = "arena",
  .kind = G_MODE_MODIFIER,
  .gameplay_selector = true,
  .gameplay = GAME_ARENA,
  .capabilities = G_MODE_CAP_ARENA | G_MODE_CAP_NO_SELF_DAMAGE |
                  G_MODE_CAP_SUPPRESS_ITEMS,
  .ops = &g_arena_mode_ops,
};

const g_mode_def_t *G_ArenaModeDefinition(void) {
  return &g_arena_mode;
}
