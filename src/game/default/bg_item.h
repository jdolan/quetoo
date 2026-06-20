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

#include "shared/shared.h"

/**
 * @brief Item types.
 */
typedef enum {
  ITEM_TYPE_NONE,
  ITEM_TYPE_AMMO,
  ITEM_TYPE_ARMOR,
  ITEM_TYPE_FLAG,
  ITEM_TYPE_HEALTH,
  ITEM_TYPE_POWERUP,
  ITEM_TYPE_WEAPON,
  ITEM_TYPE_TECH,

  ITEM_TYPE_TOTAL
} g_item_type_t;

/**
 * @brief Global item tags. Each tag is the item's index in `g_items[]`.
 */
typedef enum {
  ITEM_NONE = 0,

  WEAPON_FIRST = 1,
  WEAPON_BLASTER = 1,
  WEAPON_SHOTGUN,
  WEAPON_SUPER_SHOTGUN,
  WEAPON_MACHINEGUN,
  WEAPON_HAND_GRENADE,
  WEAPON_GRENADE_LAUNCHER,
  WEAPON_ROCKET_LAUNCHER,
  WEAPON_HYPERBLASTER,
  WEAPON_LIGHTNING,
  WEAPON_RAILGUN,
  WEAPON_BFG10K,

  WEAPON_QUAKE_SHOTGUN,
  WEAPON_QUAKE_SUPER_SHOTGUN,
  WEAPON_QUAKE_NAILGUN,
  WEAPON_QUAKE_SUPER_NAILGUN,
  WEAPON_QUAKE_GRENADE_LAUNCHER,
  WEAPON_QUAKE_ROCKET_LAUNCHER,
  WEAPON_QUAKE_THUNDERBOLT,

  WEAPON_LAST,
  WEAPON_TOTAL = WEAPON_LAST - WEAPON_FIRST,

  AMMO_FIRST = WEAPON_LAST,
  AMMO_SHELLS = AMMO_FIRST,
  AMMO_BULLETS,
  AMMO_GRENADES,
  AMMO_ROCKETS,
  AMMO_CELLS,
  AMMO_BOLTS,
  AMMO_SLUGS,
  AMMO_NUKES,

  AMMO_QUAKE_SHELLS,
  AMMO_QUAKE_NAILS,
  AMMO_QUAKE_ROCKETS,
  AMMO_QUAKE_BOLTS,

  AMMO_LAST,
  AMMO_TOTAL = AMMO_LAST - AMMO_FIRST,

  ARMOR_FIRST = AMMO_LAST,
  ARMOR_SHARD = ARMOR_FIRST,
  ARMOR_JACKET,
  ARMOR_COMBAT,
  ARMOR_BODY,

  ARMOR_QUAKE_JACKET,
  ARMOR_QUAKE_COMBAT,
  ARMOR_QUAKE_BODY,

  ARMOR_LAST,
  ARMOR_TOTAL = ARMOR_LAST - ARMOR_FIRST,

  HEALTH_FIRST = ARMOR_LAST,
  HEALTH_SMALL = HEALTH_FIRST,
  HEALTH_MEDIUM,
  HEALTH_LARGE,
  HEALTH_MEGA,

  HEALTH_QUAKE_MEDIUM,
  HEALTH_QUAKE_LARGE,
  HEALTH_QUAKE_MEGA,

  HEALTH_LAST,
  HEALTH_TOTAL = HEALTH_LAST - HEALTH_FIRST,

  POWERUP_FIRST = HEALTH_LAST,
  POWERUP_QUAD = POWERUP_FIRST,
  POWERUP_ADRENALINE,
  POWERUP_INVULNERABILITY,
  POWERUP_INVISIBILITY,

  POWERUP_LAST,
  POWERUP_TOTAL = POWERUP_LAST - POWERUP_FIRST,

  TECH_FIRST = POWERUP_LAST,
  TECH_HASTE = TECH_FIRST,
  TECH_REGEN,
  TECH_RESIST,
  TECH_STRENGTH,
  TECH_VAMPIRE,

  TECH_LAST,
  TECH_TOTAL = TECH_LAST - TECH_FIRST,

  FLAG_FIRST = TECH_LAST,
  FLAG_RED = FLAG_FIRST,
  FLAG_BLUE,
  FLAG_YELLOW,
  FLAG_GREEN,

  FLAG_LAST,
  FLAG_TOTAL = FLAG_LAST - FLAG_FIRST,

  ITEM_TOTAL = FLAG_LAST,
} g_item_tag_t;

_Static_assert(ITEM_TOTAL <= MAX_INVENTORY, "ITEM_TOTAL exceeds MAX_INVENTORY; increase MAX_INVENTORY in shared.h");

/**
 * @brief Shared item definition, visible to both game and cgame.
 * This struct holds all static data for an item; `g_item_t` (game only)
 * embeds this as its first member and adds runtime-computed fields.
 */
typedef struct {

  /**
   * @brief Entity classname used for map spawning.
   */
  const char *classname;

  /**
   * @brief Sound to play on pickup.
   */
  const char *pickup_sound;

  /**
   * @brief World model path.
   */
  const char *model;

  /**
   * @brief Entity effect flags on the pickup entity.
   */
  uint32_t effects;

  /**
   * @brief Icon image path, used on HUD and in pickup messages.
   */
  const char *icon;

  /**
   * @brief Display name.
   */
  const char *name;

  /**
   * @brief Amount provided or consumed, depending on type.
   */
  uint16_t quantity;

  /**
   * @brief Maximum amount the player can carry (ammo/armor).
   */
  uint16_t max;

  /**
   * @brief Tag of the ammo item this weapon consumes, or `ITEM_NONE`.
   */
  g_item_tag_t ammo;

  /**
   * @brief Global item tag; equals the item's index in `g_items[]`.
   */
  g_item_tag_t tag;

  /**
   * @brief Type-specific flags (e.g. `g_weapon_flags_t`).
   */
  uint16_t flags;

  /**
   * @brief Item type category.
   */
  g_item_type_t type;

  /**
   * @brief Relative priority used by AI for item selection.
   */
  float priority;

  /**
   * @brief Space-separated list of assets to precache.
   */
  const char *precaches;

  /**
   * @brief RGB color for `EF_LIGHT` emission. Ignored if `light_radius` is 0.
   */
  vec3_t light_color;

  /**
   * @brief Base radius for `EF_LIGHT` emission. 0 means no light.
   */
  float light_radius;

  /**
   * @brief Color used by cgame for item pickup / respawn effects.
   * Zero means "use cgame fallback palette".
   */
  color_t effect_color;

} g_item_def_t;

/**
 * @brief The complete list of item definitions, shared between game and cgame.
 */
extern const g_item_def_t bg_item_defs[];

/**
 * @brief The count of `bg_item_defs`.
 */
extern size_t bg_num_items;
