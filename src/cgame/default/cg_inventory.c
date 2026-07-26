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

cg_item_t cg_items[MAX_INVENTORY];
cg_weapon_t cg_weapons[MAX_INVENTORY];

static bool cg_mode_item[MAX_INVENTORY];
static g_item_type_t cg_item_types[MAX_INVENTORY];
static uint16_t cg_item_quantities[MAX_INVENTORY];
static g_item_tag_t cg_item_ammos[MAX_INVENTORY];
static color_t cg_item_effect_colors[MAX_INVENTORY];
static char cg_item_names[MAX_INVENTORY][MAX_INFO_STRING_VALUE];
static char cg_item_classnames[MAX_INVENTORY][MAX_INFO_STRING_VALUE];
static char cg_item_icons[MAX_INVENTORY][MAX_INFO_STRING_VALUE];
static char cg_item_models[MAX_INVENTORY][MAX_INFO_STRING_VALUE];

static const r_image_t *cg_health_icons[4]; // indexed by health threshold

/**
 * @brief Initializes the inventory cache (weapon icons, ammo tags, models, armor/health icons).
 * Called once per map load from `Cg_LoadHudMedia`.
 */
void Cg_InitInventory(void) {

  memset(cg_items, 0, sizeof(cg_items));
  memset(cg_weapons, 0, sizeof(cg_weapons));

  for (g_item_tag_t t = ITEM_NONE + 1; t < ITEM_TOTAL; t++) {
    cg_items[t].type = bg_item_defs[t].type;
    cg_items[t].effect_color = bg_item_defs[t].effect_color;
    cg_item_types[t] = bg_item_defs[t].type;
    cg_item_quantities[t] = bg_item_defs[t].quantity;
    cg_item_ammos[t] = bg_item_defs[t].ammo;
    cg_item_effect_colors[t] = bg_item_defs[t].effect_color;
    q_strlcpy(cg_item_names[t], bg_item_defs[t].name ?: "", sizeof(cg_item_names[t]));
    if (bg_item_defs[t].icon) {
      cg_items[t].icon = cgi.LoadImage(bg_item_defs[t].icon, IMG_PIC);
    }
    if (bg_item_defs[t].model) {
      cg_items[t].model = cgi.LoadModel(bg_item_defs[t].model);
    }
  }

  for (int32_t tag = ITEM_TOTAL; tag < MAX_INVENTORY; tag++) {
    if (cg_mode_item[tag]) {
      cg_items[tag].type = cg_item_types[tag];
      cg_items[tag].effect_color = cg_item_effect_colors[tag];
      if (cg_item_icons[tag][0]) {
        cg_items[tag].icon = cgi.LoadImage(cg_item_icons[tag], IMG_PIC);
      }
      if (cg_item_models[tag][0]) {
        cg_items[tag].model = cgi.LoadModel(cg_item_models[tag]);
      }
    }
  }

  for (g_item_tag_t t = WEAPON_FIRST; t < WEAPON_LAST; t++) {
    cg_weapon_t *w = &cg_weapons[t];
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

static void Cg_LoadModeItem(const g_item_tag_t tag) {
  if (tag < ITEM_TOTAL || tag >= MAX_INVENTORY || !cg_mode_item[tag]) {
    return;
  }
  cg_items[tag].type = cg_item_types[tag];
  cg_items[tag].effect_color = cg_item_effect_colors[tag];
  cg_items[tag].icon = cg_item_icons[tag][0] ?
      cgi.LoadImage(cg_item_icons[tag], IMG_PIC) : NULL;
  cg_items[tag].model = cg_item_models[tag][0] ?
      cgi.LoadModel(cg_item_models[tag]) : NULL;
}

void Cg_ParseModeItems(const char *catalog) {
  memset(cg_mode_item, 0, sizeof(cg_mode_item));
  memset(cg_item_types, 0, sizeof(cg_item_types));
  memset(cg_item_quantities, 0, sizeof(cg_item_quantities));
  memset(cg_item_ammos, 0, sizeof(cg_item_ammos));
  memset(cg_item_effect_colors, 0, sizeof(cg_item_effect_colors));
  memset(cg_item_names, 0, sizeof(cg_item_names));
  memset(cg_item_classnames, 0, sizeof(cg_item_classnames));
  memset(cg_item_icons, 0, sizeof(cg_item_icons));
  memset(cg_item_models, 0, sizeof(cg_item_models));
  memset(&cg_items[ITEM_TOTAL], 0,
         (MAX_INVENTORY - ITEM_TOTAL) * sizeof(cg_item_t));

  /* Rehydrate the immutable built-in metadata after clearing the mode-owned
   * extension arrays. Configstring updates may happen more than once during a
   * level restart, so built-in quantities/ammo must not depend on the first
   * Cg_InitInventory call. */
  for (g_item_tag_t tag = ITEM_NONE + 1; tag < ITEM_TOTAL; tag++) {
    cg_item_types[tag] = bg_item_defs[tag].type;
    cg_item_quantities[tag] = bg_item_defs[tag].quantity;
    cg_item_ammos[tag] = bg_item_defs[tag].ammo;
    cg_item_effect_colors[tag] = bg_item_defs[tag].effect_color;
  }

  if (!catalog || !*catalog) {
    return;
  }

  char key[MAX_INFO_STRING_KEY];
  char value[MAX_INFO_STRING_VALUE];
  for (const char *cursor = catalog; cursor && *cursor;) {
    const char *next = InfoString_Next(cursor, key, value);
    if (!*key) {
      break;
    }
    if (q_strlen(key) < 2) {
      cursor = next;
      continue;
    }

    const int32_t tag = atoi(key + 1);
    if (tag < ITEM_TOTAL || tag >= MAX_INVENTORY) {
      cursor = next;
      continue;
    }

    cg_mode_item[tag] = true;
    switch (key[0]) {
      case 'n':
        q_strlcpy(cg_item_names[tag], value, sizeof(cg_item_names[tag]));
        break;
      case 'h':
        q_strlcpy(cg_item_classnames[tag], value, sizeof(cg_item_classnames[tag]));
        break;
      case 'i':
        q_strlcpy(cg_item_icons[tag], value, sizeof(cg_item_icons[tag]));
        break;
      case 'm':
        q_strlcpy(cg_item_models[tag], value, sizeof(cg_item_models[tag]));
        break;
      case 'y':
        cg_item_types[tag] = (g_item_type_t) atoi(value);
        break;
      case 'q':
        cg_item_quantities[tag] = (uint16_t) atoi(value);
        break;
      case 'a':
        cg_item_ammos[tag] = (g_item_tag_t) atoi(value);
        break;
      case 'c':
        (void) Color_Parse(value, &cg_item_effect_colors[tag]);
        break;
      default:
        break;
    }

    cursor = next;
  }

  for (int32_t tag = ITEM_TOTAL; tag < MAX_INVENTORY; tag++) {
    Cg_LoadModeItem((g_item_tag_t) tag);
    if (cg_mode_item[tag] && cg_item_types[tag] == ITEM_TYPE_WEAPON) {
      cg_weapons[tag] = (cg_weapon_t) {
        .tag = (g_item_tag_t) tag,
        .ammo_tag = cg_item_ammos[tag],
        .icon = cg_items[tag].icon,
        .model = cg_items[tag].model,
      };
    }
  }
}

const char *Cg_ItemName(const g_item_tag_t tag) {
  if (tag < ITEM_NONE || tag >= MAX_INVENTORY) {
    return "";
  }
  if (tag >= ITEM_TOTAL) {
    return cg_item_names[tag];
  }
  return bg_item_defs[tag].name ?: "";
}

int16_t Cg_ItemQuantity(const g_item_tag_t tag) {
  if (tag < ITEM_NONE || tag >= MAX_INVENTORY) {
    return 0;
  }
  return cg_item_quantities[tag];
}

const r_image_t *Cg_ItemIcon(const g_item_tag_t tag) {
  if (tag < ITEM_NONE || tag >= MAX_INVENTORY) {
    return NULL;
  }
  return cg_items[tag].icon;
}

const char *Cg_ItemClassname(const g_item_tag_t tag) {
  if (tag < ITEM_NONE || tag >= MAX_INVENTORY) {
    return "";
  }
  if (tag >= ITEM_TOTAL) {
    return cg_item_classnames[tag];
  }
  return bg_item_defs[tag].classname ?: "";
}

color_t Cg_ItemEffectColor(const g_item_tag_t tag) {
  if (tag < ITEM_NONE || tag >= MAX_INVENTORY) {
    return color_white;
  }
  return cg_items[tag].effect_color;
}

/**
 * @brief Returns true if the player has at least one weapon in inventory.
 */
bool Cg_HasWeapon(const player_state_t *ps) {

  for (g_item_tag_t i = WEAPON_FIRST; i < MAX_INVENTORY; i++) {
    if (ps->inventory[i] && cg_items[i].type == ITEM_TYPE_WEAPON) {
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

  const g_item_tag_t tag = Cg_ActiveWeaponTag(ps);
  if (tag < WEAPON_FIRST || tag >= WEAPON_LAST) {
    return WEAPON_SELECT_OFF;
  }
  return tag - WEAPON_FIRST;
}

g_item_tag_t Cg_ActiveWeaponTag(const player_state_t *ps) {

  const int16_t weapon    = (ps->stats[STAT_WEAPON] >> 0) & 0xff;
  const int16_t switching = (ps->stats[STAT_WEAPON] >> 8) & 0xff;

  if (switching >= WEAPON_FIRST && switching < MAX_INVENTORY &&
      cg_items[switching].type == ITEM_TYPE_WEAPON) {
    return switching;
  }

  if (weapon >= WEAPON_FIRST && weapon < MAX_INVENTORY &&
      cg_items[weapon].type == ITEM_TYPE_WEAPON) {
    return weapon;
  }

  return ITEM_NONE;
}

/**
 * @brief Returns the ammo count for the active weapon, or 0 if none.
 */
int16_t Cg_ActiveAmmo(const player_state_t *ps) {

  const g_item_tag_t active = Cg_ActiveWeaponTag(ps);
  if (active == ITEM_NONE) {
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
