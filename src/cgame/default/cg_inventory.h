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

#pragma once

#include "cg_types.h"
#include "game/default/bg_item.h"

/**
 * @brief Sentinel value indicating no weapon is selected.
 */
#define WEAPON_SELECT_OFF (-1)

/**
 * @brief Cached per-item data derived from `bg_item_defs` at load time.
 */
typedef struct {

  /**
   * @brief The item icon image, or `NULL` if not found.
   */
  const r_image_t *icon;

  /**
   * @brief The item model, or `NULL` if not found.
   */
  const r_model_t *model;

} cg_item_t;

/**
 * @brief Per-item cache, indexed by `g_item_tag_t`. Populated at load time.
 */
extern cg_item_t cg_items[ITEM_TOTAL];

/**
 * @brief Cached per-weapon data derived from `bg_item_defs` at load time.
 */
typedef struct {

  /**
   * @brief The weapon's item tag.
   */
  g_item_tag_t tag;

  /**
   * @brief The ammo item tag this weapon consumes, or `ITEM_NONE`.
   */
  g_item_tag_t ammo_tag;

  /**
   * @brief The weapon icon image, or `NULL` if not found.
   */
  const r_image_t *icon;

  /**
   * @brief The weapon model, or `NULL` if not found.
   */
  const r_model_t *model;

} cg_weapon_t;

/**
 * @brief Per-weapon cache, indexed by (tag - `WEAPON_FIRST`). Populated at load time.
 */
extern cg_weapon_t cg_weapons[WEAPON_TOTAL];

/**
 * @brief Initializes the inventory cache (weapon icons, ammo tags).
 * Called once per map load from `Cg_LoadHudMedia`.
 */
void Cg_InitInventory(void);

/**
 * @brief Returns true if the player has at least one weapon in inventory.
 */
bool Cg_HasWeapon(const player_state_t *ps);

/**
 * @brief Returns the active weapon index into `cg_weapons[]`, or `WEAPON_SELECT_OFF`.
 * Prefers the weapon being switched to over the one currently equipped.
 */
int16_t Cg_ActiveWeapon(const player_state_t *ps);

/**
 * @brief Returns the active ammo quantity, or 0 if the active weapon has no ammo.
 */
int16_t Cg_ActiveAmmo(const player_state_t *ps);

/**
 * @brief Returns the icon for the player's current armor based on inventory.
 */
const r_image_t *Cg_ArmorIcon(const player_state_t *ps);

/**
 * @brief Returns the health icon appropriate for the given health value.
 */
const r_image_t *Cg_HealthIcon(int16_t health);
