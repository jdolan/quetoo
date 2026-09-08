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

#include "cg_local.h"
#include "bg_item.h"

cg_item_t cg_items[ITEM_TOTAL];
cg_weapon_t cg_weapons[WEAPON_TOTAL];

/**
 * @brief Initializes the inventory cache: item icons and models, weapon and ammo tags.
 * Called once per map load from `Cg_LoadHudMedia`.
 */
void Cg_InitInventory(void) {

  memset(cg_items, 0, sizeof(cg_items));
  memset(cg_weapons, 0, sizeof(cg_weapons));

  for (g_item_tag_t t = ITEM_NONE + 1; t < ITEM_TOTAL; t++) {
    if (bg_item_defs[t].model) {
      cg_items[t].model = cgi.LoadModel(bg_item_defs[t].model);
    }
  }

  for (g_item_tag_t t = WEAPON_FIRST; t < WEAPON_LAST; t++) {
    cg_weapon_t *w = &cg_weapons[t - WEAPON_FIRST];
    w->tag = t;
    w->ammo_tag = bg_item_defs[t].ammo;
    w->model = cg_items[t].model;
  }
}

/**
 * @brief Returns true if the player has at least one weapon in inventory.
 */
bool Cg_HasWeapon(const player_state_t *ps) {

  for (g_item_tag_t i = WEAPON_FIRST; i < WEAPON_LAST; i++) {
    if (ps->inventory[i]) {
      return true;
    }
  }
  return false;
}

/**
 * @brief Returns the active weapon index into `cg_weapons[]`, or `WEAPON_SELECT_OFF`.
 * Prefers the weapon being switched to over the one currently equipped.
 */
int16_t Cg_ActiveWeapon(const player_state_t *ps) {

  const int16_t weapon    = (ps->stats[STAT_WEAPON] >> 0) & 0xff;
  const int16_t switching = (ps->stats[STAT_WEAPON] >> 8) & 0xff;

  if (switching >= WEAPON_FIRST && switching < WEAPON_LAST) {
    return switching - WEAPON_FIRST;
  }

  if (weapon >= WEAPON_FIRST && weapon < WEAPON_LAST) {
    return weapon - WEAPON_FIRST;
  }

  return WEAPON_SELECT_OFF;
}

/**
 * @brief Returns the ammo count for the active weapon, or 0 if none.
 */
int16_t Cg_ActiveAmmo(const player_state_t *ps) {

  const int16_t active = Cg_ActiveWeapon(ps);
  if (active == WEAPON_SELECT_OFF) {
    return 0;
  }

  const g_item_tag_t ammo_tag = cg_weapons[active].ammo_tag;
  if (!ammo_tag) {
    return 0;
  }

  return ps->inventory[ammo_tag];
}
