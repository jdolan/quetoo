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

#include "g_local.h"

const box3_t ITEM_BOUNDS = {
  .mins = { { -16.0, -16.0, -16.0 } },
  .maxs = { {  16.0,  16.0,  32.0 } }
};

/**
 * @brief Finds an item by its entity class name.
 */
const g_item_t *G_FindItemByClassName(const char *classname) {

  for (g_item_tag_t t = WEAPON_FIRST; t < ITEM_TOTAL; t++) {
    const g_item_t *it = &g_items[t];

    if (!strcmp(it->def.classname, classname)) {
      return it;
    }
  }

  return NULL;
}

/**
 * @brief Finds an item by its display name.
 */
const g_item_t *G_FindItem(const char *name) {

  if (!name) {
    return NULL;
  }

  const g_item_t *fallback = NULL;
  for (g_item_tag_t t = WEAPON_FIRST; t < ITEM_TOTAL; t++) {
    const g_item_t *it = &g_items[t];

    if (!g_ascii_strcasecmp(it->def.name, name)) {
      if (G_ItemAvailable(it)) {
        return it;
      }
      if (!fallback) {
        fallback = it;
      }
    }
  }

  return fallback;
}

/**
 * @brief Maps Quetoo weapon tags to their Quake equivalent, by gameplay role.
 *  Used to redirect generic bindings (e.g. "use Rocket Launcher") to the
 *  appropriate weapon when `g_level.items == ITEMS_QUAKE`.
 *  `WEAPON_NONE` means no equivalent exists.
 */
static const g_item_tag_t g_quake_weapon_map[WEAPON_LAST] = {
  [WEAPON_BLASTER]          = WEAPON_QUAKE_SHOTGUN,
  [WEAPON_SHOTGUN]          = WEAPON_QUAKE_SHOTGUN,
  [WEAPON_SUPER_SHOTGUN]    = WEAPON_QUAKE_SUPER_SHOTGUN,
  [WEAPON_MACHINEGUN]       = WEAPON_QUAKE_NAILGUN,
  [WEAPON_HAND_GRENADE]     = ITEM_NONE,
  [WEAPON_GRENADE_LAUNCHER] = WEAPON_QUAKE_GRENADE_LAUNCHER,
  [WEAPON_ROCKET_LAUNCHER]  = WEAPON_QUAKE_ROCKET_LAUNCHER,
  [WEAPON_HYPERBLASTER]     = WEAPON_QUAKE_SUPER_NAILGUN,
  [WEAPON_LIGHTNING]        = WEAPON_QUAKE_THUNDERBOLT,
  [WEAPON_RAILGUN]          = WEAPON_QUAKE_THUNDERBOLT,
  [WEAPON_BFG10K]           = ITEM_NONE,
};

/**
 * @brief Returns the Quake equivalent of a Quetoo weapon item when
 *  `g_level.items == ITEMS_QUAKE`, or `NULL` if no mapping exists or the item is
 *  already a Quake weapon.
 */
const g_item_t *G_MappedWeapon(const g_item_t *weapon) {

  if (weapon->def.tag < WEAPON_FIRST || weapon->def.tag >= WEAPON_QUAKE_SHOTGUN) {
    return NULL; // already a Quake weapon, or unmapped
  }

  const g_item_tag_t mapped = g_quake_weapon_map[weapon->def.tag];
  if (!mapped) {
    return NULL;
  }

  return &g_items[mapped];
}

/**
 * @return The strongest armor item held by the specified client, or `NULL`. This
 * will never return the shard armor, because shards are added to the currently
 * held armor type, or to jacket armor if no armor is held.
 */
const g_item_t *G_ClientArmor(const g_client_t *cl) {


  for (g_item_tag_t armor = ARMOR_QUAKE_BODY; armor > ARMOR_SHARD; armor--) {

    if (cl->inventory[armor]) {
      return &g_items[armor];
    }
  }

  return NULL;
}

/**
 * @brief Think function that respawns an item entity after its delay expires.
 */
static void G_ItemRespawn(g_entity_t *ent) {

  if (ent->team) {
    if (ent->team_next) {
      ent = ent->team_next;
    } else {
      ent = ent->team_master;
    }
  }

  ent->sv_flags &= ~SVF_NO_CLIENT;
  ent->solid = SOLID_TRIGGER;

  gi.LinkEntity(ent);

  // send an effect
  ent->s.event = EV_ITEM_RESPAWN;
  ent->s.event_data = ent->item->def.tag;
}

/**
 * @brief Schedules an item entity to respawn after the specified delay in milliseconds.
 */
void G_SetItemRespawn(g_entity_t *ent, uint32_t delay) {

  ent->next_think = g_level.time + delay;
  ent->Think = G_ItemRespawn;

  ent->solid = SOLID_NOT;
  ent->sv_flags |= SVF_NO_CLIENT;

  gi.LinkEntity(ent);
}

/**
 * @brief Handles pickup of the adrenaline powerup, restoring the player to max health.
 */
static bool G_PickupAdrenaline(g_client_t *cl, g_entity_t *ent) {

  if (cl->entity->health < cl->entity->max_health) {
    cl->entity->health = cl->entity->max_health;
  }

  if (!(ent->spawn_flags & SF_ITEM_DROPPED)) {
    G_SetItemRespawn(ent, 40000);
  }

  return true;
}

/**
 * @brief Handles pickup of the quad damage powerup, granting the player quad damage for its duration.
 */
static bool G_PickupQuadDamage(g_client_t *cl, g_entity_t *ent) {

  if (cl->inventory[POWERUP_QUAD]) {
    return false; // already have it
  }

  cl->inventory[POWERUP_QUAD] = 1;

  uint32_t delta = 3000;

  if (ent->spawn_flags & SF_ITEM_DROPPED) { // receive only the time left
    cl->quad_damage_time = ent->next_think;
    cl->quad_countdown_time = ent->next_think - delta;
  } else {
    cl->quad_damage_time = g_level.time + SECONDS_TO_MILLIS(g_balance_quad_damage_time->value);
    cl->quad_countdown_time = cl->quad_damage_time - delta;
    G_SetItemRespawn(ent, SECONDS_TO_MILLIS(g_balance_quad_damage_respawn_time->value));
  }

  cl->entity->s.effects |= EF_QUAD;
  return true;
}

/**
 * @brief Drops the quad damage powerup from the client's inventory as a world entity.
 */
g_entity_t *G_TossQuadDamage(g_client_t *cl) {
  g_entity_t *quad;

  if (!cl->inventory[POWERUP_QUAD]) {
    return NULL;
  }

  quad = G_DropItem(cl, &g_items[POWERUP_QUAD]);

  if (quad) {
    quad->timestamp = cl->quad_damage_time;
  }

  cl->quad_damage_time = 0.0;
  cl->inventory[POWERUP_QUAD] = 0;

  return quad;
}

/**
 * @brief Handles pickup of the Invisibility, granting temporary invisibility.
 */
static bool G_PickupInvisibility(g_client_t *cl, g_entity_t *ent) {

  if (cl->inventory[POWERUP_INVISIBILITY]) {
    return false; // already have it
  }

  cl->inventory[POWERUP_INVISIBILITY] = 1;

  if (ent->spawn_flags & SF_ITEM_DROPPED) {
    cl->invisibility_time = ent->next_think;
  } else {
    cl->invisibility_time = g_level.time + SECONDS_TO_MILLIS(g_balance_invisibility_time->value);
    G_SetItemRespawn(ent, SECONDS_TO_MILLIS(g_balance_invisibility_respawn_time->value));
  }

  cl->entity->s.effects |= EF_INVISIBILITY;

  G_MulticastSound(&(const g_play_sound_t) {
    .index = g_media.sounds.invisibility_pickup,
    .entity = cl->entity,
  }, MULTICAST_PHS);

  return true;
}

/**
 * @brief Drops the Invisibility from the client's inventory as a world entity.
 */
g_entity_t *G_TossInvisibility(g_client_t *cl) {

  if (!cl->inventory[POWERUP_INVISIBILITY]) {
    return NULL;
  }

  g_entity_t *item = G_DropItem(cl, &g_items[POWERUP_INVISIBILITY]);

  if (item) {
    item->timestamp = cl->invisibility_time;
  }

  cl->invisibility_time = 0;
  cl->inventory[POWERUP_INVISIBILITY] = 0;
  cl->entity->s.effects &= ~EF_INVISIBILITY;

  return item;
}

/**
 * @brief Handles pickup of the Invulnerability, granting temporary invulnerability.
 */
static bool G_PickupInvulnerability(g_client_t *cl, g_entity_t *ent) {

  if (cl->inventory[POWERUP_INVULNERABILITY]) {
    return false; // already have it
  }

  cl->inventory[POWERUP_INVULNERABILITY] = 1;

  uint32_t delta = 3000;

  if (ent->spawn_flags & SF_ITEM_DROPPED) {
    cl->invulnerability_time = ent->next_think;
    cl->invulnerability_countdown_time = ent->next_think - delta;
  } else {
    cl->invulnerability_time = g_level.time + SECONDS_TO_MILLIS(g_balance_invulnerability_time->value);
    cl->invulnerability_countdown_time = cl->invulnerability_time - delta;
    G_SetItemRespawn(ent, SECONDS_TO_MILLIS(g_balance_invulnerability_respawn_time->value));
  }

  cl->entity->s.effects |= EF_INVULNERABILITY;

  G_MulticastSound(&(const g_play_sound_t) {
    .index = g_media.sounds.invulnerability_pickup,
    .entity = cl->entity,
  }, MULTICAST_PHS);

  return true;
}

/**
 * @brief Drops the Invulnerability from the client's inventory as a world entity.
 */
g_entity_t *G_TossInvulnerability(g_client_t *cl) {

  if (!cl->inventory[POWERUP_INVULNERABILITY]) {
    return NULL;
  }

  g_entity_t *item = G_DropItem(cl, &g_items[POWERUP_INVULNERABILITY]);

  if (item) {
    item->timestamp = cl->invulnerability_time;
  }

  cl->invulnerability_time = 0;
  cl->invulnerability_countdown_time = 0;
  cl->inventory[POWERUP_INVULNERABILITY] = 0;
  cl->entity->s.effects &= ~EF_INVULNERABILITY;

  return item;
}

/**
 * @brief Adds the given amount of ammo to the client's inventory, clamped to the item's maximum.
 */
bool G_AddAmmo(g_client_t *cl, const g_item_t *item, int16_t count) {
  int16_t max = item->def.max;

  if (!max) {
    return false;
  }

  const g_item_tag_t index = item->def.tag;

  cl->inventory[index] += count;

  if (cl->inventory[index] > max) {
    cl->inventory[index] = max;
  } else if (cl->inventory[index] < 0) {
    cl->inventory[index] = 0;
  }

  return true;
}

/**
 * @brief Sets the client's ammo count for the given item to an absolute value, clamped to its maximum.
 */
bool G_SetAmmo(g_client_t *cl, const g_item_t *item, int16_t count) {
  int16_t max = item->def.max;

  if (!max) {
    return false;
  }

  const g_item_tag_t index = item->def.tag;

  cl->inventory[index] = count;

  if (cl->inventory[index] > max) {
    cl->inventory[index] = max;
  } else if (cl->inventory[index] < 0) {
    cl->inventory[index] = 0;
  }

  return true;
}

/**
 * @brief Handles pickup of an ammo item, adding its quantity to the client's inventory.
 */
static bool G_PickupAmmo(g_client_t *cl, g_entity_t *ent) {
  int32_t count;

  if (ent->count) {
    count = ent->count;
  } else {
    count = ent->item->def.quantity;
  }

  if (!G_AddAmmo(cl, ent->item, count)) {
    return false;
  }

  if (!(ent->spawn_flags & SF_ITEM_DROPPED)) {
    G_SetItemRespawn(ent, g_ammo_respawn_time->value * 1000);
  }

  return true;
}

/**
 * @brief When picking up grenades, give the hand grenades weapon in addition to the ammo.
 */
static bool G_PickupGrenades(g_client_t *cl, g_entity_t *ent) {

  const bool pickup = G_PickupAmmo(cl, ent);
  if (pickup) {
    if (!cl->inventory[WEAPON_HAND_GRENADE]) {
      cl->inventory[WEAPON_HAND_GRENADE]++;
    }

    if (cl->persistent.auto_switch && cl->weapon == &g_items[WEAPON_BLASTER]) {
      G_UseWeapon(cl, &g_items[WEAPON_HAND_GRENADE]);
    }
  }

  return pickup;
}

/**
 * @brief When "using" grenades, route it to the Hand Grenades
 */
static void G_UseGrenades(g_client_t *cl, const g_item_t *item) {
  G_UseWeapon(cl, G_FindItem("Hand Grenades"));
}

/**
 * @brief When picking up the grenade launcher, give the hand grenades weapon as well.
 */
static bool G_PickupGrenadeLauncher(g_client_t *cl, g_entity_t *ent) {

  const bool pickup = G_PickupWeapon(cl, ent);
  if (pickup) {
    if (!cl->inventory[WEAPON_HAND_GRENADE]) {
      cl->inventory[WEAPON_HAND_GRENADE]++;
    }
  }

  return pickup;
}

/**
 * @brief Handles pickup of a health item, healing the player according to health type rules.
 */
static bool G_PickupHealth(g_client_t *cl, g_entity_t *ent) {
  int32_t h, max;

  const uint16_t tag = ent->item->def.tag;

  const bool always_add = tag == HEALTH_SMALL;
  const bool always_pickup = (tag == HEALTH_SMALL || tag == HEALTH_MEGA || tag == HEALTH_QUAKE_MEGA);

  if (cl->entity->health < cl->entity->max_health || always_add || always_pickup) {

    h = cl->entity->health + ent->item->def.quantity; // target health points
    max = cl->entity->max_health;

    if (always_pickup) { // resolve max
      if (h > max && cl) {
        if (tag == HEALTH_MEGA || tag == HEALTH_QUAKE_MEGA) {
          cl->boost_time = g_level.time + 1000;
        }
        max = cl->max_boost_health;
      }
    } else if (always_add) {
      max = INT16_MAX;
    }
    if (h > max) { // and enforce it
      h = max;
    }

    cl->entity->health = h;

    switch (tag) {
      case HEALTH_SMALL:
        G_SetItemRespawn(ent, g_balance_health_small_respawn->integer * 1000);
        break;
      case HEALTH_MEDIUM:
        G_SetItemRespawn(ent, g_balance_health_medium_respawn->integer * 1000);
        break;
      case HEALTH_LARGE:
        G_SetItemRespawn(ent, g_balance_health_large_respawn->integer * 1000);
        break;
      case HEALTH_MEGA:
        G_SetItemRespawn(ent, g_balance_health_mega_respawn->integer * 1000);
        break;
      case HEALTH_QUAKE_MEDIUM:
        G_SetItemRespawn(ent, g_balance_health_medium_respawn->integer * 1000);
        break;
      case HEALTH_QUAKE_LARGE:
        G_SetItemRespawn(ent, g_balance_health_large_respawn->integer * 1000);
        break;
      case HEALTH_QUAKE_MEGA:
        G_SetItemRespawn(ent, g_balance_health_mega_respawn->integer * 1000);
        break;
    }

    return true;
  }

  return false;
}

/**
 * @return The `g_armor_info_t` for the specified item.
 */
const g_armor_info_t *G_ArmorInfo(const g_item_t *armor) {
  static const g_armor_info_t armor_info[] = {
    { ARMOR_QUAKE_JACKET, 0.3, 0.0 },
    { ARMOR_QUAKE_COMBAT, 0.6, 0.0 },
    { ARMOR_QUAKE_BODY,   0.8, 0.0 },
    { ARMOR_BODY,         0.8, 0.6 },
    { ARMOR_COMBAT,       0.6, 0.3 },
    { ARMOR_JACKET,       0.3, 0.0 }
  };

  if (!armor) {
    return NULL;
  }

  for (size_t i = 0; i < lengthof(armor_info); i++) {
    if (armor->def.tag == armor_info[i].tag) {
      return &armor_info[i];
    }
  }

  return NULL;
}

/**
 * @brief Handles pickup of an armor item, merging with or replacing the client's existing armor.
 */
static bool G_PickupArmor(g_client_t *cl, g_entity_t *ent) {

  const g_item_t *new_armor = ent->item;
  const g_item_t *current_armor = G_ClientArmor(cl);

  const g_armor_info_t *new_info = G_ArmorInfo(new_armor);
  const g_armor_info_t *current_info = G_ArmorInfo(current_armor);

  bool taken = false;

  if (new_armor->def.tag == ARMOR_SHARD) { // always take it, ignoring cap
    if (current_armor) {
      cl->inventory[current_armor->def.tag] =
          Clampf(cl->inventory[current_armor->def.tag] + new_armor->def.quantity,
                0, cl->max_armor);
    } else {
      cl->inventory[ARMOR_JACKET] =
          Clampf((int16_t) new_armor->def.quantity, 0, cl->max_armor);
    }

    taken = true;
  } else if (!current_armor) { // no current armor, take it
    cl->inventory[new_armor->def.tag] =
        Clampf((int16_t) new_armor->def.quantity, 0, cl->max_armor);

    taken = true;
  } else if (new_armor->def.tag >= ARMOR_QUAKE_JACKET) {
    // Quake-family armor uses Q1 effective-score logic: full swap, no salvage.
    // A pickup is only accepted if it strictly improves the player's effective score.
    const float current_score = current_info->normal_protection * cl->inventory[current_armor->def.tag];
    const float new_score = new_info->normal_protection * new_armor->def.quantity;

    if (new_score > current_score) {
      cl->inventory[current_armor->def.tag] = 0;
      cl->inventory[new_armor->def.tag] = Clampf((int16_t) new_armor->def.quantity, 0, cl->max_armor);
      taken = true;
    }
  } else {
    // Q2-family armor uses salvage-conversion logic.
    // we picked up stronger armor than we currently had
    if (new_info->normal_protection > current_info->normal_protection) {

      // get the ratio between the new and old armor to add a portion to
      // new armor pickup. Ganked from q2pro (thanks skuller)
      const float salvage = current_info->normal_protection / new_info->normal_protection;
      const int16_t salvage_count = salvage * cl->inventory[current_armor->def.tag];

      const int16_t new_count = Clampf(salvage_count + new_armor->def.quantity, 0, new_armor->def.max);

      if (new_count < cl->max_armor) {
        cl->inventory[current_armor->def.tag] = 0;

        cl->inventory[new_armor->def.tag] =
            Clampf(new_count, 0, cl->max_armor);
      }

      taken = true;
    } else {
      // we picked up the same, or weaker
      const float salvage = new_info->normal_protection / current_info->normal_protection;
      const int16_t salvage_count = salvage * new_armor->def.quantity;

      int16_t new_count = salvage_count + cl->inventory[current_armor->def.tag];
      new_count = Clampf(new_count, 0, current_armor->def.max);

      // take it
      if (cl->inventory[current_armor->def.tag] < new_count &&
              cl->inventory[current_armor->def.tag] < cl->max_armor) {
        cl->inventory[current_armor->def.tag] =
            Clampf(new_count, 0, cl->max_armor);

        taken = true;
      }
    }
  }

  if (taken && !(ent->spawn_flags & SF_ITEM_DROPPED)) {
    switch (new_armor->def.tag) {
      case ARMOR_SHARD:
        G_SetItemRespawn(ent, g_balance_armor_shard_respawn->integer * 1000);
        break;
      case ARMOR_JACKET:
        G_SetItemRespawn(ent, g_balance_armor_jacket_respawn->integer * 1000);
        break;
      case ARMOR_COMBAT:
        G_SetItemRespawn(ent, g_balance_armor_combat_respawn->integer * 1000);
        break;
      case ARMOR_BODY:
        G_SetItemRespawn(ent, g_balance_armor_body_respawn->integer * 1000);
        break;
      case ARMOR_QUAKE_JACKET:
        G_SetItemRespawn(ent, g_balance_armor_jacket_respawn->integer * 1000);
        break;
      case ARMOR_QUAKE_COMBAT:
        G_SetItemRespawn(ent, g_balance_armor_combat_respawn->integer * 1000);
        break;
      case ARMOR_QUAKE_BODY:
        G_SetItemRespawn(ent, g_balance_armor_body_respawn->integer * 1000);
        break;
      default:
        G_Debug("Invalid armor tag: %d\n", new_armor->def.tag);
        break;
    }
  }

  return taken;
}

/**
 * @brief A dropped flag has been idle for 30 seconds, return it.
 */
void G_ResetDroppedFlag(g_entity_t *ent) {
  g_team_t *t;
  g_entity_t *f;

  if (!(t = G_TeamForFlag(ent))) {
    return;
  }

  if (!(f = G_FlagForTeam(t))) {
    return;
  }

  f->sv_flags &= ~SVF_NO_CLIENT;
  f->s.event = EV_ITEM_RESPAWN;
  f->s.event_data = f->item->def.tag;
  f->solid = SOLID_TRIGGER;

  gi.LinkEntity(f);

  G_MulticastSound(&(const g_play_sound_t) {
    .index = g_media.sounds.ctf_return
  }, MULTICAST_PHS_R);

  gi.BroadcastPrint(PRINT_HIGH, "The %s flag has been returned :flag%d_return:\n", t->name, t->id + 1);

  G_FreeEntity(ent);
}

/**
 * @brief Steal the enemy's flag. If our own flag is dropped, return it. Else, if we are
 * carrying the enemy's flag and touch our own flag, that is a capture.
 */
static bool G_PickupFlag(g_client_t *cl, g_entity_t *ent) {
  int32_t index;

  if (!cl->persistent.team) {
    return false;
  }

  g_team_t *team = G_TeamForFlag(ent);
  g_entity_t *team_flag = G_FlagForTeam(team);

  const g_item_t *carried_flag = G_GetFlag(cl);

  if (team == cl->persistent.team) { // our flag

    if (ent->spawn_flags & SF_ITEM_DROPPED) { // return it if necessary

      team_flag->solid = SOLID_TRIGGER;
      team_flag->sv_flags &= ~SVF_NO_CLIENT;

      gi.LinkEntity(team_flag);

      team_flag->s.event = EV_ITEM_RESPAWN;
      team_flag->s.event_data = team_flag->item->def.tag;

      G_MulticastSound(&(const g_play_sound_t) {
        .index = g_media.sounds.ctf_return
      }, MULTICAST_PHS);

      gi.BroadcastPrint(PRINT_HIGH, "%s returned the %s flag :flag%d_return:\n", cl->persistent.net_name, team->name, team->id + 1);

      return true;
    }

    if (carried_flag) {
      const g_team_t *other_team = &g_team_list[carried_flag->def.tag - FLAG_FIRST];
      g_entity_t *other_team_flag = G_FlagForTeam(other_team);

      index = other_team_flag->item->def.tag;
      if (cl->inventory[index]) { // capture

        cl->inventory[index] = 0;
        cl->entity->s.effects &= ~G_EffectForTeam(other_team);
        cl->entity->s.model3 = 0;

        other_team_flag->solid = SOLID_TRIGGER;
        other_team_flag->sv_flags &= ~SVF_NO_CLIENT; // reset the other flag

        gi.LinkEntity(other_team_flag);

        other_team_flag->s.event = EV_ITEM_RESPAWN;
        other_team_flag->s.event_data = other_team_flag->item->def.tag;

        G_MulticastSound(&(const g_play_sound_t) {
          .index = g_media.sounds.ctf_capture
        }, MULTICAST_PHS_R);

        gi.BroadcastPrint(PRINT_HIGH, "%s captured the %s flag :flag%d_capture:\n", cl->persistent.net_name, other_team->name, other_team->id + 1);

        team->captures++;
        cl->persistent.captures++;

        {
          const bool player_ai = cl->ai != NULL;
          g_capture_t capture = {
            .player_ai = player_ai,
            .time = (uint32_t) time(NULL),
          };
          SDL_strlcpy(capture.level,       g_level.name,              sizeof(capture.level));
          SDL_strlcpy(capture.player,      cl->persistent.net_name,   sizeof(capture.player));
          SDL_strlcpy(capture.player_guid, cl->persistent.guid,       sizeof(capture.player_guid));
          SDL_strlcpy(capture.team,        other_team->name,          sizeof(capture.team));

          if (capture.player_guid[0]) {
            g_array_append_val(g_level.captures, capture);
          }
        }

        return false;
      }
    }

    // touching our own flag for no particular reason
    return false;
  }

  // it's enemy's flag, so take it if we can
  if (carried_flag) {
    return false; // we have one already
  }

  team_flag->solid = SOLID_NOT;
  team_flag->sv_flags |= SVF_NO_CLIENT;

  gi.LinkEntity(team_flag);

  index = team_flag->item->def.tag;
  cl->inventory[index] = 1;

  // link the flag model to the player
  cl->entity->s.model3 = team_flag->item->model_index;

  G_MulticastSound(&(const g_play_sound_t) {
    .index = g_media.sounds.ctf_steal,
  }, MULTICAST_PHS_R);

  gi.BroadcastPrint(PRINT_HIGH, "%s stole the %s flag :flag%d_steal:\n", cl->persistent.net_name, team->name, team->id + 1);

  cl->entity->s.effects |= G_EffectForTeam(team);
  return true;
}

/**
 * @brief Drops the CTF flag currently carried by the client as a world entity.
 */
g_entity_t *G_TossFlag(g_client_t *cl) {

  const g_item_t *flag = G_GetFlag(cl);;

  if (!flag) {
    return NULL;
  }

  const g_team_t *team = &g_team_list[flag->def.tag - FLAG_FIRST];
  const g_item_tag_t index = flag->def.tag;

  if (!cl->inventory[index]) {
    return NULL;
  }

  cl->inventory[index] = 0;

  cl->entity->s.model3 = 0;
  cl->entity->s.effects &= ~EF_CTF_MASK;

  gi.BroadcastPrint(PRINT_HIGH, "%s dropped the %s flag :flag%d_drop:\n", cl->persistent.net_name, team->name, team->id + 1);

  return G_DropItem(cl, flag);
}

/**
 * @brief Drop command callback that tosses the client's carried CTF flag.
 */
static g_entity_t *G_DropFlag(g_client_t *cl, const g_item_t *item) {
  return G_TossFlag(cl);
}

/**
 * @brief Sets the expiration timer and think function for a dropped item entity.
 */
static void G_DropItem_SetExpiration(g_entity_t *ent) {

  if (ent->item->def.type == ITEM_FLAG) { // flags go back to base
    ent->Think = G_ResetDroppedFlag;
  } else if (ent->item->def.type == ITEM_TECH) {
    ent->Think = G_ResetDroppedTech;
  } else { // everything else just gets freed
    ent->Think = G_FreeEntity;
  }

  uint32_t expiration;
  if (ent->item->def.type == ITEM_POWERUP) { // expire from last touch
    expiration = ent->timestamp - g_level.time;
  } else { // general case
    expiration = 30000;
  }

  const int32_t contents = gi.PointContents(ent->s.origin);

  if (contents & CONTENTS_LAVA) { // expire more quickly in lava
    expiration /= 5;
  }
  if (contents & CONTENTS_SLIME) { // and slime
    expiration /= 2;
  }

  ent->next_think = g_level.time + expiration;
}

/**
 * @brief Think function for dropped items that waits until the item lands before setting its expiration.
 */
static void G_DropItem_Think(g_entity_t *ent) {

  // continue to think as we drop to the floor
  if (ent->ground.ent || (gi.PointContents(ent->s.origin) & CONTENTS_MASK_LIQUID)) {
    G_DropItem_SetExpiration(ent);
  } else {
    ent->next_think = g_level.time + QUETOO_TICK_MILLIS;
  }
}

/**
 * @brief Touch callback that handles item pickup when a player contacts an item entity.
 */
void G_TouchItem(g_entity_t *ent, g_entity_t *other, const cm_trace_t *trace) {

  if (G_Ai_InDeveloperMode()) {
    return;
  }

  if (other == ent->owner) {
    if (ent->touch_time > g_level.time) {
      return;
    }
  }

  if (!other->client) {
    return;
  }

  if (other->dead) {
    return; // dead people can't pickup
  }

  if (!ent->item->Pickup) {
    return; // item can't be picked up
  }

  // if we still haven't thunk from being dropped, set the expiration now
  if (ent->Think == G_DropItem_Think) {
    G_DropItem_SetExpiration(ent);
  }

  g_client_t *cl = other->client;

  const bool pickup = ent->item->Pickup(cl, ent);
  if (pickup) {
    // show icon and name on status bar
    uint16_t tag = ent->item->def.tag;

    if ((cl->ps.stats[STAT_PICKUP] & ~STAT_TOGGLE_BIT) == tag) {
      tag |= STAT_TOGGLE_BIT;
    }

    cl->ps.stats[STAT_PICKUP] = tag;
    
    if (ent->item->Use) {
      cl->last_pickup = ent->item;
    }
    cl->pickup_msg_time = g_level.time + 3000;

    if (ent->item->def.pickup_sound) {
      G_MulticastSound(&(const g_play_sound_t) {
        .index = ent->item->pickup_sound_index,
        .origin = &other->s.origin,
      }, MULTICAST_PHS);
    }

    other->s.event = EV_ITEM_PICKUP;
    other->s.event_data = ent->item->def.tag;
  }

  if (!(ent->spawn_flags & SF_ITEM_TARGETS_USED)) {
    G_UseTargets(ent, other);
    ent->spawn_flags |= SF_ITEM_TARGETS_USED;
  }

  if (pickup) {
    if (ent->spawn_flags & SF_ITEM_DROPPED) {
      G_FreeEntity(ent);
    }
  }
}

/**
 * @brief Handles the mechanics of dropping items, but does not adjust the client's
 * inventory. That is left to the caller.
 */
g_entity_t *G_DropItem(g_client_t *cl, const g_item_t *item) {
  vec3_t forward;

  g_entity_t *it = G_AllocEntity(item->def.classname);
  it->owner = cl->entity;

  it->bounds = Box3_Scale(ITEM_BOUNDS, ITEM_SCALE);

  it->solid = SOLID_TRIGGER;

  // resolve forward direction and project origin
  if (cl->entity->dead) {
    Vec3_Vectors(Vec3(.0f, cl->angles.y, .0f), &forward, NULL, NULL);
    it->s.origin = Vec3_Fmaf(cl->entity->s.origin, 24.f, forward);
  } else {
    Vec3_Vectors(cl->entity->s.angles, &forward, NULL, NULL);
    it->s.origin = cl->entity->s.origin;
    it->s.origin.z -= it->bounds.mins.z;
  }

  const cm_trace_t tr = gi.Trace(it->s.origin, it->s.origin, it->bounds, cl->entity, CONTENTS_MASK_SOLID);

  it->item = item;

  // we're in a bad spot, forget it
  if (tr.start_solid) {
    if (item->def.type == ITEM_TECH) {
      G_ResetDroppedTech(it);
    } else if (item->def.type == ITEM_FLAG) {
      G_ResetDroppedFlag(it);
    } else {
      G_FreeEntity(it);
    }

    return NULL;
  }

  it->s.origin = tr.end;

  it->spawn_flags |= SF_ITEM_DROPPED;
  it->move_type = MOVE_TYPE_BOUNCE;
  it->Touch = G_TouchItem;
  it->s.effects = item->def.effects;

  if (item->def.light_radius) {
    it->s.effects |= EF_LIGHT | EF_LIGHT_PULSE;
    it->s.color = Color_Color32(Color3fv(item->def.light_color));
    it->s.termination.x = item->def.light_radius;
  }
  it->touch_time = g_level.time + 1000;

  it->s.model1 = item->model_index;

  if (item->def.type == ITEM_WEAPON) {
    const g_item_t *ammo = item->def.ammo ? &g_items[item->def.ammo] : NULL;
    if (ammo) {
      it->health = ammo->def.quantity;
    }
  }

  it->velocity = Vec3_Scale(forward, 100.0);
  it->velocity.z = 300.0 + (Randomf() * 50.0);

  it->Think = G_DropItem_Think;
  it->next_think = g_level.time + QUETOO_TICK_MILLIS;

  gi.LinkEntity(it);

  return it;
}

/**
 * @brief Use callback that reveals a hidden item and enables it for pickup.
 */
static void G_UseItem(g_entity_t *ent, g_entity_t *other, g_entity_t *activator) {

  ent->sv_flags &= ~SVF_NO_CLIENT;
  ent->Use = NULL;

  if (ent->spawn_flags & SF_ITEM_NO_TOUCH) {
    ent->solid = SOLID_BOX;
    ent->Touch = NULL;
  } else {
    ent->solid = SOLID_TRIGGER;
    ent->Touch = G_TouchItem;
  }

  gi.LinkEntity(ent);
}

/**
 * @brief Returns the distance to the nearest enemy from the given spot.
 */
static float G_TechRangeFromSpawn(const g_entity_t *spawn) {
  float best_dist = FLT_MAX;
  bool any = false;

  for (g_item_tag_t tech = TECH_FIRST; tech < TECH_LAST; tech++) {

    g_entity_t *ent = NULL;
    G_ForEachEntity(e, {
      if (e->item == &g_items[tech]) {
        ent = e;
        break;
      }
    });

    if (!ent) {
      continue;
    }

    const vec3_t v = Vec3_Subtract(spawn->s.origin, ent->s.origin);
    const float dist = Vec3_Length(v);

    if (dist < best_dist) {
      best_dist = dist;
    }

    any = true;
  }

  if (!any) {
    return Randomf() * MAX_WORLD_DIST;
  }

  return best_dist;
}

/**
 * @brief Finds the spawn point farthest from all existing tech items within the given set.
 */
static void G_SelectFarthestTechSpawnPoint(const g_spawn_points_t *spawn_points, g_entity_t **point, float *point_dist) {

  for (size_t i = 0; i < spawn_points->count; i++) {
    g_entity_t *spot = spawn_points->spots[i];
    float dist = G_TechRangeFromSpawn(spot);

    if (dist > *point_dist) {
      *point = spot;
      *point_dist = dist;
    }
  }
}

/**
 * @brief Selects the optimal spawn point for a tech item by maximizing distance from all other techs.
 */
g_entity_t *G_SelectTechSpawnPoint(void) {
  float point_dist = -FLT_MAX;
  g_entity_t *point = NULL;

  if (g_level.teams || g_level.ctf) {
    for (int32_t i = 0; i < g_level.num_teams; i++) {
      G_SelectFarthestTechSpawnPoint(&g_team_list[i].spawn_points, &point, &point_dist);
    }
  } else {
    G_SelectFarthestTechSpawnPoint(&g_level.spawn_points, &point, &point_dist);
  }

  if (!point) {
    G_SelectFarthestTechSpawnPoint(&g_level.spawn_points, &point, &point_dist);
  }

  return point;
}

/**
 * @brief Respawns a tech item at a new spawn point and frees the dropped entity.
 */
void G_ResetDroppedTech(g_entity_t *ent) {

  G_SpawnTech(ent->item);

  G_FreeEntity(ent);
}

/**
 * @brief Check if a player has the specified tech.
 */
bool G_HasTech(const g_client_t *cl, g_item_tag_t tech) {
  return !!cl->inventory[tech];
}

/**
 * @brief Pickup function for techs. Can only hold one tech at a time.
 */
static bool G_PickupTech(g_client_t *cl, g_entity_t *ent) {

  for (g_item_tag_t tech = TECH_FIRST; tech < TECH_LAST; tech++) {

    if (G_HasTech(cl, tech)) {
      return false;
    }
  }

  // add the weapon to inventory
  cl->inventory[ent->item->def.tag]++;

  return true;
}

/**
 * @brief Returns the tech item currently held by the client, or `NULL` if none.
 */
const g_item_t *G_GetTech(const g_client_t *cl) {

  for (g_item_tag_t i = TECH_FIRST; i < TECH_LAST; i++) {

    if (G_HasTech(cl, i)) {
      return &g_items[i];
    }
  }

  return NULL;
}

/**
 * @brief Drops the tech item currently carried by the client as a world entity.
 */
g_entity_t *G_TossTech(g_client_t *cl) {
  const g_item_t *tech = G_GetTech(cl);

  if (!tech) {
    return NULL;
  }

  cl->inventory[tech->def.tag] = 0;

  return G_DropItem(cl, tech);
}

/**
 * @brief Drop the tech.
 */
static g_entity_t *G_DropTech(g_client_t *cl, const g_item_t *item) {
  return G_TossTech(cl);
}

/**
 * @brief Reset the item's interaction state based on the current game state.
 */
void G_ResetItem(g_entity_t *ent) {

  ent->solid = SOLID_TRIGGER;
  ent->sv_flags &= ~SVF_NO_CLIENT;
  ent->Touch = G_TouchItem;

  if (ent->item->def.type == ITEM_FLAG) {
    const g_team_id_t flag_team = ent->item->def.tag - FLAG_FIRST;
    const g_team_id_t last_team = FLAG_FIRST + g_level.num_teams - 1;
    if (g_level.ctf == false || flag_team > last_team) {
      ent->sv_flags |= SVF_NO_CLIENT;
      ent->solid = SOLID_NOT;
    }
  }

  if (ent->spawn_flags & SF_ITEM_TRIGGER) {
    ent->sv_flags |= SVF_NO_CLIENT;
    ent->solid = SOLID_NOT;
    ent->Use = G_UseItem;
  }

  if (ent->spawn_flags & SF_ITEM_NO_TOUCH) {
    ent->solid = SOLID_BOX;
    ent->Touch = NULL;
  }

  const bool inhibited = (g_level.gameplay == GAME_ARENA || g_level.gameplay == GAME_INSTAGIB)
                          && ent->item->def.type != ITEM_FLAG;

  if (inhibited || (ent->flags & FL_TEAM_SLAVE)) {
    ent->sv_flags |= SVF_NO_CLIENT;
    ent->solid = SOLID_NOT;
  }

  // if we were mid-respawn, get us out of it
  if (ent->Think == G_ItemRespawn) {
    ent->next_think = 0;
    ent->Think = NULL;
  }

  gi.LinkEntity(ent);
}

/**
 * @brief Drops the specified item to the floor and sets up interaction
 * properties (Touch, Use, move type, ..).
 */
static void G_ItemDropToFloor(g_entity_t *ent) {
  cm_trace_t tr;
  vec3_t dest;
  bool drop_node = false;

  ent->velocity = Vec3_Zero();
  dest = ent->s.origin;

  if (!(ent->spawn_flags & SF_ITEM_HOVER)) {
    ent->move_type = MOVE_TYPE_BOUNCE;
    drop_node = true;
  } else {
    ent->move_type = MOVE_TYPE_FLY;
  }

  tr = gi.Trace(ent->s.origin, dest, ent->bounds, ent, CONTENTS_MASK_SOLID);
  if (tr.start_solid) {
    // try thinner box
    G_Debug("%s in too small of a spot for large box, correcting..\n", etos(ent));
    ent->bounds.maxs.z /= 2.0;

    tr = gi.Trace(ent->s.origin, dest, ent->bounds, ent, CONTENTS_MASK_SOLID);
    if (tr.start_solid) {

      G_Debug("%s still can't fit, trying Q2 box..\n", etos(ent));

      ent->bounds = Box3_Expand3(ent->bounds, Vec3(-2.f, -2.f, -2.f));

      // try Quake 2 box
      tr = gi.Trace(ent->s.origin, dest, ent->bounds, ent, CONTENTS_MASK_SOLID);
      if (tr.start_solid) {

        G_Debug("%s trying higher, last attempt..\n", etos(ent));

        ent->s.origin.z += 8.0;

        // make an effort to come up out of the floor (broken maps)
        tr = gi.Trace(ent->s.origin, ent->s.origin, ent->bounds, ent, CONTENTS_MASK_SOLID);
        if (tr.start_solid) {
          gi.Warn("%s start_solid\n", etos(ent));
          G_FreeEntity(ent);
          return;
        }
      }
    }
  }

  G_ResetItem(ent);

  if (drop_node) {
    G_Ai_DropItemLikeNode(ent);
  }
}

/**
 * @brief Precaches all data needed for a given item.
 * This will be called for each item spawned in a level,
 * and for each item in each client's inventory.
 */
void G_PrecacheItem(const g_item_t *it) {
  const char *s, *start;
  char data[MAX_QPATH];
  ptrdiff_t len;

  if (!it) {
    return;
  }

  if (it->def.pickup_sound) {
    gi.SoundIndex(it->def.pickup_sound);
  }
  if (it->def.model) {
    gi.ModelIndex(it->def.model);
  }
  if (it->def.icon) {
    gi.ImageIndex(it->def.icon);
  }

  // parse everything for its ammo
  if (it->def.ammo) {
    const g_item_t *ammo = &g_items[it->def.ammo];

    if (ammo != it) {
      G_PrecacheItem(ammo);
    }
  }

  // parse the space-separated precache string for other items
  s = it->def.precaches;
  if (!s || !s[0]) {
    return;
  }

  while (*s) {
    start = s;
    while (*s && *s != ' ') {
      s++;
    }

    len = s - start;
    if (len >= MAX_QPATH || len < 5) {
      gi.Error("%s has bad precache string\n", it->def.classname);
    }
    memcpy(data, start, len);
    data[len] = '\0';
    if (*s) {
      s++;
    }

    // determine type based on extension
    if (g_str_has_suffix(data, ".md3") || g_str_has_suffix(data, ".obj")) {
      gi.ModelIndex(data);
    } else if (g_str_has_suffix(data, ".wav") || g_str_has_suffix(data, ".ogg")) {
      gi.SoundIndex(data);
    } else if (g_str_has_suffix(data, ".png") || g_str_has_suffix(data, ".jpg") || g_str_has_suffix(data, ".tga")) {
      gi.ImageIndex(data);
    } else {
      gi.Error("%s has unknown data type\n", it->def.classname);
    }
  }
}

static void G_InitItem(g_item_t *it, const g_item_def_t *def);

/**
 * @brief Sets the clipping size and plants the object on the floor.
 *
 * Items can't be immediately dropped to floor, because they might
 * be on an entity that hasn't spawned yet.
 */
void G_SpawnItem(g_entity_t *ent, const g_item_t *item) {

  ent->item = item;
  G_PrecacheItem(ent->item);

  ent->bounds = Box3_Scale(ITEM_BOUNDS, ITEM_SCALE);

  if (ent->model) {
    ent->s.model1 = gi.ModelIndex(ent->model);
  } else {
    G_InitItem((g_item_t *) ent->item, &ent->item->def);
    ent->s.model1 = ent->item->model_index;
  }

  ent->s.effects = item->def.effects;

  if (item->def.light_radius) {
    ent->s.effects |= EF_LIGHT | EF_LIGHT_PULSE;
    ent->s.color = Color_Color32(Color3fv(item->def.light_color));
    ent->s.termination.x = item->def.light_radius;
  }

  // weapons override the health field to store their ammo count
  if (ent->item->def.type == ITEM_WEAPON) {
    const g_item_t *ammo = ent->item->def.ammo ? &g_items[ent->item->def.ammo] : NULL;
    if (ammo) {
      ent->health = ammo->def.quantity;
    } else {
      ent->health = 0;
    }
  } else if (ent->item->def.type == ITEM_FLAG) {
    // pass flag tint over (0-based team index)
    ent->s.animation1 = item->def.tag - FLAG_FIRST;
  }

  ent->next_think = g_level.time + QUETOO_TICK_MILLIS * 2;
  ent->Think = G_ItemDropToFloor;
}

/**
 * @brief The item list; allocated and initialized in `G_InitItems`.
 */
g_item_t *g_items;

/**
 * @brief Returns true if the item belongs to the active item set.
 */
bool G_ItemAvailable(const g_item_t *item) {

  if (item->def.type == ITEM_WEAPON) {
    if (g_level.items == ITEMS_QUAKE) {
      return item->def.tag >= WEAPON_QUAKE_SHOTGUN;
    } else {
      return item->def.tag < WEAPON_QUAKE_SHOTGUN;
    }
  }

  if (item->def.type == ITEM_AMMO) {
    if (g_level.items == ITEMS_QUAKE) {
      return item->def.tag >= AMMO_QUAKE_SHELLS;
    } else {
      return item->def.tag < AMMO_QUAKE_SHELLS;
    }
  }

  if (item->def.type == ITEM_ARMOR) {
    if (g_level.items == ITEMS_QUAKE) {
      return item->def.tag >= ARMOR_QUAKE_JACKET;
    } else {
      return item->def.tag < ARMOR_QUAKE_JACKET;
    }
  }

  if (item->def.type == ITEM_HEALTH) {
    if (g_level.items == ITEMS_QUAKE) {
      return item->def.tag >= HEALTH_QUAKE_MEDIUM;
    } else {
      return item->def.tag < HEALTH_QUAKE_MEDIUM;
    }
  }

  if (item->def.type == ITEM_POWERUP) {
    return true;
  }

  return true;
}

/**
 * @brief Called to set up a specific item in the item list.
 */
static void G_InitItem(g_item_t *it, const g_item_def_t *def) {

  it->def = *def;

  switch (it->def.type) {
    case ITEM_ARMOR:
      it->Pickup = G_PickupArmor;
      break;

    case ITEM_WEAPON:
      it->Pickup = G_PickupWeapon;
      it->Use = G_UseWeapon;
      it->Drop = G_DropWeapon;

      if (!strcmp(it->def.classname, "weapon_blaster") ||
          !strcmp(it->def.classname, "weapon_handgrenades")) {
        it->Drop = NULL;
      } else if (!strcmp(it->def.classname, "weapon_grenadelauncher")) {
        it->Pickup = G_PickupGrenadeLauncher;
      }

      if (!strcmp(it->def.classname, "weapon_blaster")) {
        it->Think = G_FireBlaster;
      } else if (!strcmp(it->def.classname, "weapon_shotgun")) {
        it->Think = G_FireShotgun;
      } else if (!strcmp(it->def.classname, "weapon_supershotgun")) {
        it->Think = G_FireSuperShotgun;
      } else if (!strcmp(it->def.classname, "weapon_machinegun")) {
        it->Think = G_FireMachinegun;
      } else if (!strcmp(it->def.classname, "weapon_handgrenades")) {
        it->Think = G_FireHandGrenade;
      } else if (!strcmp(it->def.classname, "weapon_grenadelauncher")) {
        it->Think = G_FireGrenadeLauncher;
      } else if (!strcmp(it->def.classname, "weapon_rocketlauncher")) {
        it->Think = G_FireRocketLauncher;
      } else if (!strcmp(it->def.classname, "weapon_hyperblaster")) {
        it->Think = G_FireHyperblaster;
      } else if (!strcmp(it->def.classname, "weapon_lightning")) {
        it->Think = G_FireLightning;
      } else if (!strcmp(it->def.classname, "weapon_railgun")) {
        it->Think = G_FireRailgun;
      } else if (!strcmp(it->def.classname, "weapon_bfg")) {
        it->Think = G_FireBfg;
      } else if (!strcmp(it->def.classname, "weapon_quake_shotgun")) {
        it->Think = G_FireQuakeShotgun;
      } else if (!strcmp(it->def.classname, "weapon_quake_supershotgun")) {
        it->Think = G_FireQuakeSuperShotgun;
      } else if (!strcmp(it->def.classname, "weapon_quake_nailgun")) {
        it->Think = G_FireQuakeNailgun;
      } else if (!strcmp(it->def.classname, "weapon_quake_supernailgun")) {
        it->Think = G_FireQuakeSuperNailgun;
      } else if (!strcmp(it->def.classname, "weapon_quake_grenadelauncher")) {
        it->Think = G_FireQuakeGrenadeLauncher;
      } else if (!strcmp(it->def.classname, "weapon_quake_rocketlauncher")) {
        it->Think = G_FireQuakeRocketLauncher;
      } else if (!strcmp(it->def.classname, "weapon_quake_thunderbolt")) {
        it->Think = G_FireQuakeThunderbolt;
      }
      break;

    case ITEM_AMMO:
      it->Pickup = G_PickupAmmo;
      it->Drop = G_DropItem;
      if (!strcmp(it->def.classname, "ammo_grenades")) {
        it->Pickup = G_PickupGrenades;
        it->Use = G_UseGrenades;
      }
      break;

    case ITEM_HEALTH:
      it->Pickup = G_PickupHealth;
      break;

    case ITEM_FLAG:
      it->Pickup = G_PickupFlag;
      it->Drop = G_DropFlag;
      break;

    case ITEM_POWERUP:
      if (!strcmp(it->def.classname, "item_adrenaline")) {
        it->Pickup = G_PickupAdrenaline;
      } else if (!strcmp(it->def.classname, "item_quad")) {
        it->Pickup = G_PickupQuadDamage;
      } else if (!strcmp(it->def.classname, "item_invisibility")) {
        it->Pickup = G_PickupInvisibility;
      } else if (!strcmp(it->def.classname, "item_invulnerability")) {
        it->Pickup = G_PickupInvulnerability;
      }
      break;

    case ITEM_TECH:
      it->Pickup = G_PickupTech;
      it->Drop = G_DropTech;
      break;

    default:
      gi.Error("Item %s has an invalid type\n", it->def.name);
      break;
  }

  it->model_index = gi.ModelIndex(it->def.model);
  it->pickup_sound_index = gi.SoundIndex(it->def.pickup_sound);
}

/**
 * @brief Called to setup special private data for the item list.
 */
void G_InitItems(void) {

  g_items = gi.Malloc(ITEM_TOTAL * sizeof(g_item_t), MEM_TAG_GAME);

  for (size_t i = 0; i < bg_num_items; i++) {
    G_InitItem(&g_items[bg_item_defs[i].tag], &bg_item_defs[i]);
  }
}
