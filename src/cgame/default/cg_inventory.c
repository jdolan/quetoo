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
#include "game/default/bg_item.h"

cg_item_t cg_items[ITEM_TOTAL];
cg_weapon_t cg_weapons[WEAPON_TOTAL];

static const r_image_t *cg_health_icons[4]; // indexed by health threshold

/**
 * @brief Initializes the inventory cache (weapon icons, ammo tags, models, armor/health icons).
 * Called once per map load from `Cg_LoadHudMedia`.
 */
void Cg_InitInventory(void) {

  memset(cg_items, 0, sizeof(cg_items));
  memset(cg_weapons, 0, sizeof(cg_weapons));

  for (g_item_tag_t t = ITEM_NONE + 1; t < ITEM_TOTAL; t++) {
    if (bg_item_defs[t].icon) {
      cg_items[t].icon = cgi.LoadImage(bg_item_defs[t].icon, IMG_PIC);
    }
    if (bg_item_defs[t].model) {
      cg_items[t].model = cgi.LoadModel(bg_item_defs[t].model);
    }
  }

  for (g_item_tag_t t = WEAPON_FIRST; t < WEAPON_LAST; t++) {
    cg_weapon_t *w = &cg_weapons[t - WEAPON_FIRST];
    w->tag = t;
    w->icon = cg_items[t].icon;
    w->ammo_tag = bg_item_defs[t].ammo;
    w->model = cg_items[t].model;
  }

  // health icons ordered by ascending threshold: <=25, <=75, <=100, >100
  cg_health_icons[0] = cgi.LoadImage("pics/i_health_large", IMG_PIC);
  cg_health_icons[1] = cgi.LoadImage("pics/i_health_medium", IMG_PIC);
  cg_health_icons[2] = cgi.LoadImage("pics/i_health", IMG_PIC);
  cg_health_icons[3] = cgi.LoadImage("pics/i_health_mega", IMG_PIC);
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

  const int16_t switching = (ps->stats[STAT_WEAPON] >> 8) & 0xFF;
  if (switching) {
    return switching - WEAPON_FIRST;
  }

  const int16_t tag = ps->stats[STAT_WEAPON] & 0xFF;
  return tag > 0 ? tag - WEAPON_FIRST : WEAPON_SELECT_OFF;
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

/**
 * @brief Returns the icon for the player's current armor based on inventory.
 * Mirrors `G_ClientArmor`: returns the highest-priority armor in inventory.
 */
const r_image_t *Cg_ArmorIcon(const player_state_t *ps) {

  for (g_item_tag_t t = ARMOR_QUAKE_BODY; t > ARMOR_SHARD; t--) {
    if (ps->inventory[t]) {
      return cg_items[t].icon;
    }
  }
  return NULL;
}

/**
 * @brief Returns the health icon appropriate for the given health value,
 * mirroring the server-side `G_ClientStats` health icon selection.
 */
const r_image_t *Cg_HealthIcon(int16_t health) {

  if (health > 100) {
    return cg_health_icons[3]; // pics/i_health_mega
  } else if (health > 75) {
    return cg_health_icons[2]; // pics/i_health
  } else if (health > 25) {
    return cg_health_icons[1]; // pics/i_health_medium
  } else {
    return cg_health_icons[0]; // pics/i_health_large
  }
}
