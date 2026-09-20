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

const Box3 ITEM_BOUNDS = {
  .mins = { { -16.0, -16.0, -16.0 } },
  .maxs = { {  16.0,  16.0,  32.0 } }
};

/**
 * @brief Finds an item by its entity class name.
 */
const GameItem *G_FindItemByClassName(const char *classname) {

  for (GameItemTag t = WEAPON_FIRST; t < ITEM_TOTAL; t++) {
    const GameItem *it = &gItems[t];

    if (!q_strcmp(it->def.classname, classname)) {
      return it;
    }
  }

  return NULL;
}

/**
 * @brief Finds an item by its display name.
 */
const GameItem *G_FindItem(const char *name) {

  if (!name) {
    return NULL;
  }

  const GameItem *fallback = NULL;
  for (GameItemTag t = WEAPON_FIRST; t < ITEM_TOTAL; t++) {
    const GameItem *it = &gItems[t];

    if (!q_strcasecmp(it->def.name, name)) {
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
 *  appropriate weapon when `gLevel.items == ITEMS_QUAKE`.
 *  `WEAPON_NONE` means no equivalent exists.
 */
static const GameItemTag gQuakeWeaponMap[WEAPON_LAST] = {
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
 *  `gLevel.items == ITEMS_QUAKE`, or `NULL` if no mapping exists or the item is
 *  already a Quake weapon.
 */
const GameItem *G_MappedWeapon(const GameItem *weapon) {

  if (weapon->def.tag < WEAPON_FIRST || weapon->def.tag >= WEAPON_QUAKE_SHOTGUN) {
    return NULL; // already a Quake weapon, or unmapped
  }

  const GameItemTag mapped = gQuakeWeaponMap[weapon->def.tag];
  if (!mapped) {
    return NULL;
  }

  return &gItems[mapped];
}

/**
 * @return The strongest armor item held by the specified client, or `NULL`. This
 * will never return the shard armor, because shards are added to the currently
 * held armor type, or to jacket armor if no armor is held.
 */
const GameItem *G_ClientArmor(const GameClient *cl) {

  for (GameItemTag armor = ARMOR_QUAKE_BODY; armor > ARMOR_SHARD; armor--) {

    if (cl->inventory[armor]) {
      return &gItems[armor];
    }
  }

  return NULL;
}

/**
 * @brief Returns the item to the origin authored in the map, so that an item carried
 * away by a mover comes back where the mapper placed it. Callers that are visible
 * should expect the item to settle back onto the floor under its own physics.
 * @return True if the item was moved.
 */
static bool G_ItemRestoreOrigin(GameEntity *ent) {

  const CmEntity *origin = gi.EntityValue(ent->def, "origin");
  if (!(origin->parsed & ENTITY_VEC3)) {
    return false;
  }

  if (Vec3_Equal(ent->s.origin, origin->vec3)) {
    return false;
  }

  ent->s.origin = origin->vec3;
  ent->velocity = Vec3_Zero();
  ent->avelocity = Vec3_Zero();
  memset(&ent->ground, 0, sizeof(ent->ground));

  return true;
}

/**
 * @brief Think function that respawns an item entity after its delay expires.
 */
static void G_ItemRespawn(GameEntity *ent) {

  if (ent->team) {
    if (ent->teamNext) {
      ent = ent->teamNext;
    } else {
      ent = ent->teamMaster;
    }
  }

  ent->svFlags &= ~SVF_NO_CLIENT;
  ent->solid = SOLID_TRIGGER;

  gi.LinkEntity(ent);

  // send an effect
  ent->s.event = EV_ITEM_RESPAWN;
  ent->s.eventData = ent->item->def.tag;
}

/**
 * @brief Returns an item flagged `hazard_respawn` to its map origin if it has ended up
 * in lava or slime. Called from `G_CheckWater`, so that only items that are actually
 * moving through the world are considered.
 */
void G_CheckItemHazard(GameEntity *ent) {

  if (!ent->item) {
    return;
  }

  if (!(ent->spawnFlags & SF_ITEM_HAZARD_RESPAWN)) {
    return;
  }

  if (!(ent->waterType & (CONTENTS_LAVA | CONTENTS_SLIME))) {
    return;
  }

  if (!G_ItemRestoreOrigin(ent)) {
    return;
  }

  ent->waterLevel = WATER_NONE;
  ent->waterType = 0;

  gi.LinkEntity(ent);

  ent->s.event = EV_ITEM_RESPAWN;
  ent->s.eventData = ent->item->def.tag;
}

/**
 * @brief Schedules an item entity to respawn after the specified delay in milliseconds.
 */
void G_SetItemRespawn(GameEntity *ent, uint32_t delay) {

  ent->nextThink = gLevel.time + delay;
  ent->Think = G_ItemRespawn;

  ent->solid = SOLID_NOT;
  ent->svFlags |= SVF_NO_CLIENT;

  G_ItemRestoreOrigin(ent);

  gi.LinkEntity(ent);
}

/**
 * @brief Handles pickup of the adrenaline powerup, restoring the player to max health.
 */
static bool G_PickupAdrenaline(GameClient *cl, GameEntity *ent) {

  if (cl->entity->health < cl->entity->maxHealth) {
    cl->entity->health = cl->entity->maxHealth;
  }

  if (!(ent->spawnFlags & SF_ITEM_DROPPED)) {
    G_SetItemRespawn(ent, 40000);
  }

  return true;
}

/**
 * @brief Handles pickup of the quad damage powerup, granting the player quad damage for its duration.
 */
static bool G_PickupQuadDamage(GameClient *cl, GameEntity *ent) {

  if (cl->inventory[POWERUP_QUAD]) {
    return false; // already have it
  }

  cl->inventory[POWERUP_QUAD] = 1;

  uint32_t delta = 3000;

  if (ent->spawnFlags & SF_ITEM_DROPPED) { // receive only the time left
    cl->quadDamageTime = ent->nextThink;
    cl->quadCountdownTime = ent->nextThink - delta;
  } else {
    cl->quadDamageTime = gLevel.time + SECONDS_TO_MILLIS(g_balanceQuadDamageTime->value);
    cl->quadCountdownTime = cl->quadDamageTime - delta;
    G_SetItemRespawn(ent, SECONDS_TO_MILLIS(g_balanceQuadDamageRespawnTime->value));
  }

  cl->entity->s.effects |= EF_QUAD;
  return true;
}

/**
 * @brief Drops the quad damage powerup from the client's inventory as a world entity.
 */
GameEntity *G_TossQuadDamage(GameClient *cl) {
  GameEntity *quad;

  if (!cl->inventory[POWERUP_QUAD]) {
    return NULL;
  }

  quad = G_DropItem(cl, &gItems[POWERUP_QUAD]);

  if (quad) {
    quad->timestamp = cl->quadDamageTime;
  }

  cl->quadDamageTime = 0.0;
  cl->inventory[POWERUP_QUAD] = 0;

  return quad;
}

/**
 * @brief Handles pickup of the Invisibility, granting temporary invisibility.
 */
static bool G_PickupInvisibility(GameClient *cl, GameEntity *ent) {

  if (cl->inventory[POWERUP_INVISIBILITY]) {
    return false; // already have it
  }

  cl->inventory[POWERUP_INVISIBILITY] = 1;

  if (ent->spawnFlags & SF_ITEM_DROPPED) {
    cl->invisibilityTime = ent->nextThink;
  } else {
    cl->invisibilityTime = gLevel.time + SECONDS_TO_MILLIS(g_balanceInvisibilityTime->value);
    G_SetItemRespawn(ent, SECONDS_TO_MILLIS(g_balanceInvisibilityRespawnTime->value));
  }

  cl->entity->s.effects |= EF_INVISIBILITY;

  G_MulticastSound(&(const GamePlaySound) {
    .index = gMedia.sounds.invisibilityPickup,
    .entity = cl->entity,
  }, MULTICAST_PHS);

  return true;
}

/**
 * @brief Drops the Invisibility from the client's inventory as a world entity.
 */
GameEntity *G_TossInvisibility(GameClient *cl) {

  if (!cl->inventory[POWERUP_INVISIBILITY]) {
    return NULL;
  }

  GameEntity *item = G_DropItem(cl, &gItems[POWERUP_INVISIBILITY]);

  if (item) {
    item->timestamp = cl->invisibilityTime;
  }

  cl->invisibilityTime = 0;
  cl->inventory[POWERUP_INVISIBILITY] = 0;
  cl->entity->s.effects &= ~EF_INVISIBILITY;

  return item;
}

/**
 * @brief Handles pickup of the Invulnerability, granting temporary invulnerability.
 */
static bool G_PickupInvulnerability(GameClient *cl, GameEntity *ent) {

  if (cl->inventory[POWERUP_INVULNERABILITY]) {
    return false; // already have it
  }

  cl->inventory[POWERUP_INVULNERABILITY] = 1;

  uint32_t delta = 3000;

  if (ent->spawnFlags & SF_ITEM_DROPPED) {
    cl->invulnerabilityTime = ent->nextThink;
    cl->invulnerabilityCountdownTime = ent->nextThink - delta;
  } else {
    cl->invulnerabilityTime = gLevel.time + SECONDS_TO_MILLIS(g_balanceInvulnerabilityTime->value);
    cl->invulnerabilityCountdownTime = cl->invulnerabilityTime - delta;
    G_SetItemRespawn(ent, SECONDS_TO_MILLIS(g_balanceInvulnerabilityRespawnTime->value));
  }

  cl->entity->s.effects |= EF_INVULNERABILITY;

  G_MulticastSound(&(const GamePlaySound) {
    .index = gMedia.sounds.invulnerabilityPickup,
    .entity = cl->entity,
  }, MULTICAST_PHS);

  return true;
}

/**
 * @brief Drops the Invulnerability from the client's inventory as a world entity.
 */
GameEntity *G_TossInvulnerability(GameClient *cl) {

  if (!cl->inventory[POWERUP_INVULNERABILITY]) {
    return NULL;
  }

  GameEntity *item = G_DropItem(cl, &gItems[POWERUP_INVULNERABILITY]);

  if (item) {
    item->timestamp = cl->invulnerabilityTime;
  }

  cl->invulnerabilityTime = 0;
  cl->invulnerabilityCountdownTime = 0;
  cl->inventory[POWERUP_INVULNERABILITY] = 0;
  cl->entity->s.effects &= ~EF_INVULNERABILITY;

  return item;
}

/**
 * @brief The tail of the `G_TossInventory` chain, tossing the quad damage.
 */
static void G_TossInventory_Common(GameClient *cl) {

  G_TossQuadDamage(cl);
}

TossInventory G_TossInventory = G_TossInventory_Common;

/**
 * @brief Adds the given amount of ammo to the client's inventory, clamped to the item's maximum.
 */
bool G_AddAmmo(GameClient *cl, const GameItem *item, int16_t count) {
  int16_t max = item->def.max;

  if (!max) {
    return false;
  }

  const GameItemTag index = item->def.tag;

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
bool G_SetAmmo(GameClient *cl, const GameItem *item, int16_t count) {
  int16_t max = item->def.max;

  if (!max) {
    return false;
  }

  const GameItemTag index = item->def.tag;

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
static bool G_PickupAmmo(GameClient *cl, GameEntity *ent) {
  int32_t count;

  if (ent->count) {
    count = ent->count;
  } else {
    count = ent->item->def.quantity;
  }

  if (!G_AddAmmo(cl, ent->item, count)) {
    return false;
  }

  if (!(ent->spawnFlags & SF_ITEM_DROPPED)) {
    G_SetItemRespawn(ent, g_ammoRespawnTime->value * 1000);
  }

  return true;
}

/**
 * @brief When picking up grenades, give the hand grenades weapon in addition to the ammo.
 */
static bool G_PickupGrenades(GameClient *cl, GameEntity *ent) {

  const bool pickup = G_PickupAmmo(cl, ent);
  if (pickup) {
    if (!cl->inventory[WEAPON_HAND_GRENADE]) {
      cl->inventory[WEAPON_HAND_GRENADE]++;
    }

    if (cl->persistent.autoSwitch && cl->weapon == &gItems[WEAPON_BLASTER]) {
      G_UseWeapon(cl, &gItems[WEAPON_HAND_GRENADE]);
    }
  }

  return pickup;
}

/**
 * @brief When "using" grenades, route it to the Hand Grenades
 */
static void G_UseGrenades(GameClient *cl, const GameItem *item) {
  G_UseWeapon(cl, G_FindItem("Hand Grenades"));
}

/**
 * @brief When picking up the grenade launcher, give the hand grenades weapon as well.
 */
static bool G_PickupGrenadeLauncher(GameClient *cl, GameEntity *ent) {

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
static bool G_PickupHealth(GameClient *cl, GameEntity *ent) {
  int32_t h, max;

  const uint16_t tag = ent->item->def.tag;

  const bool alwaysAdd = tag == HEALTH_SMALL;
  const bool alwaysPickup = (tag == HEALTH_SMALL || tag == HEALTH_MEGA || tag == HEALTH_QUAKE_MEGA);

  if (cl->entity->health < cl->entity->maxHealth || alwaysAdd || alwaysPickup) {

    h = cl->entity->health + ent->item->def.quantity; // target health points
    max = cl->entity->maxHealth;

    if (alwaysPickup) { // resolve max
      if (h > max && cl) {
        if (tag == HEALTH_MEGA || tag == HEALTH_QUAKE_MEGA) {
          cl->boostTime = gLevel.time + 1000;
        }
        max = cl->maxBoostHealth;
      }
    } else if (alwaysAdd) {
      max = INT16_MAX;
    }
    if (h > max) { // and enforce it
      h = max;
    }

    cl->entity->health = h;

    switch (tag) {
      case HEALTH_SMALL:
        G_SetItemRespawn(ent, g_balanceHealthSmallRespawn->integer * 1000);
        break;
      case HEALTH_MEDIUM:
        G_SetItemRespawn(ent, g_balanceHealthMediumRespawn->integer * 1000);
        break;
      case HEALTH_LARGE:
        G_SetItemRespawn(ent, g_balanceHealthLargeRespawn->integer * 1000);
        break;
      case HEALTH_MEGA:
        G_SetItemRespawn(ent, g_balanceHealthMegaRespawn->integer * 1000);
        break;
      case HEALTH_QUAKE_MEDIUM:
        G_SetItemRespawn(ent, g_balanceHealthMediumRespawn->integer * 1000);
        break;
      case HEALTH_QUAKE_LARGE:
        G_SetItemRespawn(ent, g_balanceHealthLargeRespawn->integer * 1000);
        break;
      case HEALTH_QUAKE_MEGA:
        G_SetItemRespawn(ent, g_balanceHealthMegaRespawn->integer * 1000);
        break;
    }

    return true;
  }

  return false;
}

/**
 * @return The `GameArmorInfo` for the specified item.
 */
const GameArmorInfo *G_ArmorInfo(const GameItem *armor) {
  static const GameArmorInfo armor_info[] = {
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
static bool G_PickupArmor(GameClient *cl, GameEntity *ent) {

  const GameItem *newArmor = ent->item;
  const GameItem *currentArmor = G_ClientArmor(cl);

  const GameArmorInfo *newInfo = G_ArmorInfo(newArmor);
  const GameArmorInfo *currentInfo = G_ArmorInfo(currentArmor);

  bool taken = false;

  if (newArmor->def.tag == ARMOR_SHARD) { // always take it, ignoring cap
    if (currentArmor) {
      cl->inventory[currentArmor->def.tag] =
          Clampf(cl->inventory[currentArmor->def.tag] + newArmor->def.quantity,
                0, cl->maxArmor);
    } else {
      cl->inventory[ARMOR_JACKET] =
          Clampf((int16_t) newArmor->def.quantity, 0, cl->maxArmor);
    }

    taken = true;
  } else if (!currentArmor) { // no current armor, take it
    cl->inventory[newArmor->def.tag] =
        Clampf((int16_t) newArmor->def.quantity, 0, cl->maxArmor);

    taken = true;
  } else if (newArmor->def.tag >= ARMOR_QUAKE_JACKET) {
    // Quake-family armor uses Q1 effective-score logic: full swap, no salvage.
    // A pickup is only accepted if it strictly improves the player's effective score.
    const float currentScore = currentInfo->normalProtection * cl->inventory[currentArmor->def.tag];
    const float newScore = newInfo->normalProtection * newArmor->def.quantity;

    if (newScore > currentScore) {
      cl->inventory[currentArmor->def.tag] = 0;
      cl->inventory[newArmor->def.tag] = Clampf((int16_t) newArmor->def.quantity, 0, cl->maxArmor);
      taken = true;
    }
  } else {
    // Q2-family armor uses salvage-conversion logic.
    // we picked up stronger armor than we currently had
    if (newInfo->normalProtection > currentInfo->normalProtection) {

      // get the ratio between the new and old armor to add a portion to
      // new armor pickup. Ganked from q2pro (thanks skuller)
      const float salvage = currentInfo->normalProtection / newInfo->normalProtection;
      const int16_t salvageCount = salvage * cl->inventory[currentArmor->def.tag];

      const int16_t newCount = Clampf(salvageCount + newArmor->def.quantity, 0, newArmor->def.max);

      if (newCount < cl->maxArmor) {
        cl->inventory[currentArmor->def.tag] = 0;

        cl->inventory[newArmor->def.tag] =
            Clampf(newCount, 0, cl->maxArmor);
      }

      taken = true;
    } else {
      // we picked up the same, or weaker
      const float salvage = newInfo->normalProtection / currentInfo->normalProtection;
      const int16_t salvageCount = salvage * newArmor->def.quantity;

      int16_t newCount = salvageCount + cl->inventory[currentArmor->def.tag];
      newCount = Clampf(newCount, 0, currentArmor->def.max);

      // take it
      if (cl->inventory[currentArmor->def.tag] < newCount &&
              cl->inventory[currentArmor->def.tag] < cl->maxArmor) {
        cl->inventory[currentArmor->def.tag] =
            Clampf(newCount, 0, cl->maxArmor);

        taken = true;
      }
    }
  }

  if (taken && !(ent->spawnFlags & SF_ITEM_DROPPED)) {
    switch (newArmor->def.tag) {
      case ARMOR_SHARD:
        G_SetItemRespawn(ent, g_balanceArmorShardRespawn->integer * 1000);
        break;
      case ARMOR_JACKET:
        G_SetItemRespawn(ent, g_balanceArmorJacketRespawn->integer * 1000);
        break;
      case ARMOR_COMBAT:
        G_SetItemRespawn(ent, g_balanceArmorCombatRespawn->integer * 1000);
        break;
      case ARMOR_BODY:
        G_SetItemRespawn(ent, g_balanceArmorBodyRespawn->integer * 1000);
        break;
      case ARMOR_QUAKE_JACKET:
        G_SetItemRespawn(ent, g_balanceArmorJacketRespawn->integer * 1000);
        break;
      case ARMOR_QUAKE_COMBAT:
        G_SetItemRespawn(ent, g_balanceArmorCombatRespawn->integer * 1000);
        break;
      case ARMOR_QUAKE_BODY:
        G_SetItemRespawn(ent, g_balanceArmorBodyRespawn->integer * 1000);
        break;
      default:
        G_Debug("Invalid armor tag: %d\n", newArmor->def.tag);
        break;
    }
  }

  return taken;
}

/**
 * @brief The tail of the `G_ResetDroppedItem` chain, disposing of an item that
 * has left the world. Features that would rather recycle it install over the
 * top.
 */
static void G_ResetDroppedItem_Common(GameEntity *ent) {
  G_FreeEntity(ent);
}

ResetDroppedItem G_ResetDroppedItem = G_ResetDroppedItem_Common;

/**
 * @brief Sets the expiration timer and think function for a dropped item entity.
 */
static void G_DropItem_SetExpiration(GameEntity *ent) {

  ent->Think = G_ResetDroppedItem;

  uint32_t expiration;
  if (ent->item->def.type == ITEM_TYPE_POWERUP) { // expire from last touch
    expiration = ent->timestamp - gLevel.time;
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

  ent->nextThink = gLevel.time + expiration;
}

/**
 * @brief Think function for dropped items that waits until the item lands before setting its expiration.
 */
static void G_DropItem_Think(GameEntity *ent) {

  // continue to think as we drop to the floor
  if (ent->ground.ent || (gi.PointContents(ent->s.origin) & CONTENTS_MASK_LIQUID)) {
    G_DropItem_SetExpiration(ent);
  } else {
    ent->nextThink = gLevel.time + QUETOO_TICK_MILLIS;
  }
}

/**
 * @brief Touch callback that handles item pickup when a player contacts an item entity.
 */
void G_TouchItem(GameEntity *ent, GameEntity *other, const CmTrace *trace) {

  if (G_Ai_InDeveloperMode()) {
    return;
  }

  // an item that was dropped ignores whoever dropped it for a moment; one that
  // nobody dropped ignores everybody, so an item placed by the game cannot be
  // taken the instant it appears
  if (ent->touchTime > gLevel.time) {
    if (ent->owner == NULL || other == ent->owner) {
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

  GameClient *cl = other->client;

  const bool pickup = ent->item->Pickup(cl, ent);
  if (pickup) {
    // show icon and name on status bar
    uint16_t tag = ent->item->def.tag;

    if ((cl->ps.stats[STAT_PICKUP] & ~STAT_TOGGLE_BIT) == tag) {
      tag |= STAT_TOGGLE_BIT;
    }

    cl->ps.stats[STAT_PICKUP] = tag;
    
    if (ent->item->Use) {
      cl->lastPickup = ent->item;
    }
    cl->pickupMsgTime = gLevel.time + 3000;

    if (ent->item->def.pickupSound) {
      G_MulticastSound(&(const GamePlaySound) {
        .index = ent->item->pickupSoundIndex,
        .origin = &other->s.origin,
      }, MULTICAST_PHS);
    }

    other->s.event = EV_ITEM_PICKUP;
    other->s.eventData = ent->item->def.tag;
  }

  if (!(ent->spawnFlags & SF_ITEM_TARGETS_USED)) {
    G_UseTargets(ent, other);
    ent->spawnFlags |= SF_ITEM_TARGETS_USED;
  }

  if (pickup) {
    if (ent->spawnFlags & SF_ITEM_DROPPED) {
      G_FreeEntity(ent);
    }
  }
}

/**
 * @brief Handles the mechanics of dropping items, but does not adjust the client's
 * inventory. That is left to the caller.
 */
GameEntity *G_DropItem(GameClient *cl, const GameItem *item) {
  Vec3 forward;

  GameEntity *it = G_AllocEntity(item->def.classname);
  it->owner = cl->entity;

  it->bounds = Box3_Scale(ITEM_BOUNDS, ITEM_SCALE);

  it->solid = SOLID_TRIGGER;

  // resolve forward direction and project origin
  if (cl->entity->dead) {
    Vec3_Vectors(MakeVec3(.0f, cl->angles.y, .0f), &forward, NULL, NULL);
    it->s.origin = Vec3_Fmaf(cl->entity->s.origin, 24.f, forward);
  } else {
    Vec3_Vectors(cl->entity->s.angles, &forward, NULL, NULL);
    it->s.origin = cl->entity->s.origin;
    it->s.origin.z -= it->bounds.mins.z;
  }

  const CmTrace tr = gi.Trace(it->s.origin, it->s.origin, it->bounds, cl->entity, CONTENTS_MASK_SOLID);

  it->item = item;

  // we're in a bad spot, forget it
  if (tr.startSolid) {
    G_ResetDroppedItem(it);

    return NULL;
  }

  it->s.origin = tr.end;

  it->spawnFlags |= SF_ITEM_DROPPED;
  it->moveType = MOVE_TYPE_BOUNCE;
  it->Touch = G_TouchItem;
  it->s.effects = item->def.effects | EF_MODULATE;

  if (item->def.lightRadius) {
    it->s.effects |= EF_LIGHT | EF_LIGHT_PULSE;
    it->s.color = Color_Color32(Color3fv(item->def.lightColor));
    it->s.termination.x = item->def.lightRadius;
  }
  it->touchTime = gLevel.time + 1000;

  it->s.model1 = item->modelIndex;

  if (item->def.type == ITEM_TYPE_WEAPON) {
    const GameItem *ammo = item->def.ammo ? &gItems[item->def.ammo] : NULL;
    if (ammo) {
      it->health = ammo->def.quantity;
    }
  }

  it->velocity = Vec3_Scale(forward, 100.0);
  it->velocity.z = 300.0 + (Randomf() * 50.0);

  it->Think = G_DropItem_Think;
  it->nextThink = gLevel.time + QUETOO_TICK_MILLIS;

  gi.LinkEntity(it);

  return it;
}

/**
 * @brief The tail of the `G_ResolveInventoryItem` chain, resolving the name
 * against the item list.
 */
static const GameItem *G_ResolveInventoryItem_Common(GameClient *cl, const char *name) {

  (void) cl;

  return G_FindItem(name);
}

ResolveInventoryItem G_ResolveInventoryItem = G_ResolveInventoryItem_Common;

/**
 * @brief Drops the given item from the client's inventory, reporting to them
 * when they can not.
 */
void G_DropInventoryItem(GameClient *cl, const GameItem *it) {

  const char *name = it->def.name;

  // we don't drop in instagib or arena
  if (gLevel.gameplay & ~GAMEPLAY_TEAMS) {
    return;
  }

  if (cl->entity->dead) {
    return;
  }

  if (!it->Drop) {
    gi.ClientPrint(cl, PRINT_HIGH, "Item can not be dropped\n");
    return;
  }

  const GameItemTag index = it->def.tag;

  if (cl->inventory[index] == 0) {
    gi.ClientPrint(cl, PRINT_HIGH, "Out of item: %s\n", name);
    return;
  }

  int32_t dropQuantity;

  if (it->def.type == ITEM_TYPE_AMMO) {
    dropQuantity = it->def.quantity;
  } else {
    dropQuantity = 1;
  }

  if (cl->inventory[index] < dropQuantity) {
    gi.ClientPrint(cl, PRINT_HIGH, "Quantity too low: %s\n", name);
    return;
  }

  cl->inventory[index] -= dropQuantity;
  cl->lastDropped = it;

  it->Drop(cl, it);

  // adjust weapon if we need to
  if (it->def.type == ITEM_TYPE_WEAPON) {
    if (cl->weapon == it && !cl->nextWeapon && !cl->inventory[index]) {
      G_UseBestWeapon(cl);
    }
  }
}

/**
 * @brief Use callback that reveals a hidden item and enables it for pickup.
 */
static void G_UseItem(GameEntity *ent, GameEntity *other, GameEntity *activator) {

  ent->svFlags &= ~SVF_NO_CLIENT;
  ent->Use = NULL;

  if (ent->spawnFlags & SF_ITEM_NO_TOUCH) {
    ent->solid = SOLID_BOX;
    ent->Touch = NULL;
  } else {
    ent->solid = SOLID_TRIGGER;
    ent->Touch = G_TouchItem;
  }

  gi.LinkEntity(ent);
}

/**
 * @brief Reset the item's interaction state based on the current game state.
 */
static void G_ResetItem_Common(GameEntity *ent) {

  ent->solid = SOLID_TRIGGER;
  ent->svFlags &= ~SVF_NO_CLIENT;
  ent->Touch = G_TouchItem;

  if (ent->spawnFlags & SF_ITEM_TRIGGER) {
    ent->svFlags |= SVF_NO_CLIENT;
    ent->solid = SOLID_NOT;
    ent->Use = G_UseItem;
  }

  if (ent->spawnFlags & SF_ITEM_NO_TOUCH) {
    ent->solid = SOLID_BOX;
    ent->Touch = NULL;
  }

  if (G_InhibitItem(ent) || (ent->flags & FL_TEAM_SLAVE)) {
    ent->svFlags |= SVF_NO_CLIENT;
    ent->solid = SOLID_NOT;
  }

  // if we were mid-respawn, get us out of it
  if (ent->Think == G_ItemRespawn) {
    ent->nextThink = 0;
    ent->Think = NULL;
  }

  gi.LinkEntity(ent);
}

ResetItem G_ResetItem = G_ResetItem_Common;

/**
 * @brief The tail of the `G_InhibitItem` chain: arena and instagib play with
 * whatever the client spawns with.
 */
static bool G_InhibitItem_Common(const GameEntity *ent) {
  const GameplayId gameplay = gLevel.gameplay & ~GAMEPLAY_TEAMS;
  return gameplay == GAMEPLAY_ARENA || gameplay == GAMEPLAY_INSTAGIB;
}

InhibitItem G_InhibitItem = G_InhibitItem_Common;

/**
 * @brief Drops the specified item to the floor and sets up interaction
 * properties (Touch, Use, move type, ..).
 */
static void G_ItemDropToFloor(GameEntity *ent) {
  CmTrace tr;
  Vec3 dest;
  bool dropNode = false;

  ent->velocity = Vec3_Zero();
  dest = ent->s.origin;

  if (!(ent->spawnFlags & SF_ITEM_HOVER)) {
    ent->moveType = MOVE_TYPE_BOUNCE;
    dropNode = true;
  } else {
    ent->moveType = MOVE_TYPE_FLY;
  }

  tr = gi.Trace(ent->s.origin, dest, ent->bounds, ent, CONTENTS_MASK_SOLID);
  if (tr.startSolid) {
    // try thinner box
    G_Debug("%s in too small of a spot for large box, correcting..\n", etos(ent));
    ent->bounds.maxs.z /= 2.0;

    tr = gi.Trace(ent->s.origin, dest, ent->bounds, ent, CONTENTS_MASK_SOLID);
    if (tr.startSolid) {

      G_Debug("%s still can't fit, trying Q2 box..\n", etos(ent));

      ent->bounds = Box3_Expand3(ent->bounds, MakeVec3(-2.f, -2.f, -2.f));

      // try Quake 2 box
      tr = gi.Trace(ent->s.origin, dest, ent->bounds, ent, CONTENTS_MASK_SOLID);
      if (tr.startSolid) {

        G_Debug("%s trying higher, last attempt..\n", etos(ent));

        ent->s.origin.z += 8.0;

        // make an effort to come up out of the floor (broken maps)
        tr = gi.Trace(ent->s.origin, ent->s.origin, ent->bounds, ent, CONTENTS_MASK_SOLID);
        if (tr.startSolid) {
          G_Warn("%s start_solid\n", etos(ent));
          G_FreeEntity(ent);
          return;
        }
      }
    }
  }

  G_ResetItem(ent);

  if (dropNode) {
    G_Ai_DropItemLikeNode(ent);
  }
}

/**
 * @brief Precaches all data needed for a given item.
 * This will be called for each item spawned in a level,
 * and for each item in each client's inventory.
 */
void G_PrecacheItem(const GameItem *it) {
  const char *s, *start;
  char data[MAX_QPATH];
  ptrdiff_t len;

  if (!it) {
    return;
  }

  if (it->def.pickupSound) {
    gi.SoundIndex(it->def.pickupSound);
  }
  if (it->def.model) {
    gi.ModelIndex(it->def.model);
  }
  if (it->def.icon) {
    gi.ImageIndex(it->def.icon);
  }

  // parse everything for its ammo
  if (it->def.ammo) {
    const GameItem *ammo = &gItems[it->def.ammo];

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
      G_Error("%s has bad precache string\n", it->def.classname);
    }
    memcpy(data, start, len);
    data[len] = '\0';
    if (*s) {
      s++;
    }

    // determine type based on extension
    const size_t dlen = q_strlen(data);
    if ((dlen >= 4 && (!q_strcmp(data + dlen - 4, ".md3") || !q_strcmp(data + dlen - 4, ".obj")))) {
      gi.ModelIndex(data);
    } else if (dlen >= 4 && (!q_strcmp(data + dlen - 4, ".wav") || !q_strcmp(data + dlen - 4, ".ogg"))) {
      gi.SoundIndex(data);
    } else if (dlen >= 4 && (!q_strcmp(data + dlen - 4, ".png") || !q_strcmp(data + dlen - 4, ".jpg") || !q_strcmp(data + dlen - 4, ".tga"))) {
      gi.ImageIndex(data);
    } else {
      G_Error("%s has unknown data type\n", it->def.classname);
    }
  }
}

static void G_SetupItem(GameItem *it);

/**
 * @brief Sets the clipping size and plants the object on the floor.
 *
 * Items can't be immediately dropped to floor, because they might
 * be on an entity that hasn't spawned yet.
 */
void G_SpawnItem(GameEntity *ent, const GameItem *item) {

  ent->item = item;
  G_PrecacheItem(ent->item);

  ent->bounds = Box3_Scale(ITEM_BOUNDS, ITEM_SCALE);

  if (ent->model) {
    ent->s.model1 = gi.ModelIndex(ent->model);
  } else {
    G_SetupItem((GameItem *) ent->item);
    ent->s.model1 = ent->item->modelIndex;
  }

  ent->s.effects = item->def.effects | EF_MODULATE;

  if (item->def.lightRadius) {
    ent->s.effects |= EF_LIGHT | EF_LIGHT_PULSE;
    ent->s.color = Color_Color32(Color3fv(item->def.lightColor));
    ent->s.termination.x = item->def.lightRadius;
  }

  // weapons override the health field to store their ammo count
  if (ent->item->def.type == ITEM_TYPE_WEAPON) {
    const GameItem *ammo = ent->item->def.ammo ? &gItems[ent->item->def.ammo] : NULL;
    if (ammo) {
      ent->health = ammo->def.quantity;
    } else {
      ent->health = 0;
    }
  }

#if defined(G_CTF)
  // the flags override animation1 to tint themselves by team, 0-based
  if (ent->item->def.type == ITEM_TYPE_FLAG) {
    ent->s.animation1 = item->def.tag - FLAG_FIRST;
  }
#endif

  ent->nextThink = gLevel.time + QUETOO_TICK_MILLIS * 2;
  ent->Think = G_ItemDropToFloor;
}

/**
 * @brief The item list; allocated and initialized in `G_InitItems`.
 */
GameItem *gItems;

/**
 * @brief Returns true if the item belongs to the active item set.
 */
bool G_ItemAvailable(const GameItem *item) {

  if (item->def.type == ITEM_TYPE_WEAPON) {
    if (gLevel.items == ITEMS_QUAKE) {
      return item->def.tag >= WEAPON_QUAKE_SHOTGUN;
    } else {
      return item->def.tag < WEAPON_QUAKE_SHOTGUN;
    }
  }

  if (item->def.type == ITEM_TYPE_AMMO) {
    if (gLevel.items == ITEMS_QUAKE) {
      return item->def.tag >= AMMO_QUAKE_SHELLS;
    } else {
      return item->def.tag < AMMO_QUAKE_SHELLS;
    }
  }

  if (item->def.type == ITEM_TYPE_ARMOR) {
    if (gLevel.items == ITEMS_QUAKE) {
      return item->def.tag >= ARMOR_QUAKE_JACKET;
    } else {
      return item->def.tag < ARMOR_QUAKE_JACKET;
    }
  }

  if (item->def.type == ITEM_TYPE_HEALTH) {
    if (gLevel.items == ITEMS_QUAKE) {
      return item->def.tag >= HEALTH_QUAKE_MEDIUM;
    } else {
      return item->def.tag < HEALTH_QUAKE_MEDIUM;
    }
  }

  if (item->def.type == ITEM_TYPE_POWERUP) {
    return true;
  }

  return true;
}

/**
 * @brief The tail of the `G_InitItem` chain, answering for the deathmatch item
 * types and erroring on any other.
 */
static void G_InitItem_Common(GameItem *it) {

  switch (it->def.type) {
    case ITEM_TYPE_ARMOR:
      it->Pickup = G_PickupArmor;
      break;

    case ITEM_TYPE_WEAPON:
      it->Pickup = G_PickupWeapon;
      it->Use = G_UseWeapon;
      it->Drop = G_DropWeapon;

      if (!q_strcmp(it->def.classname, "weapon_blaster") ||
          !q_strcmp(it->def.classname, "weapon_handgrenades")) {
        it->Drop = NULL;
      } else if (!q_strcmp(it->def.classname, "weapon_grenadelauncher")) {
        it->Pickup = G_PickupGrenadeLauncher;
      }

      if (!q_strcmp(it->def.classname, "weapon_blaster")) {
        it->Think = G_FireBlaster;
      } else if (!q_strcmp(it->def.classname, "weapon_shotgun")) {
        it->Think = G_FireShotgun;
      } else if (!q_strcmp(it->def.classname, "weapon_supershotgun")) {
        it->Think = G_FireSuperShotgun;
      } else if (!q_strcmp(it->def.classname, "weapon_machinegun")) {
        it->Think = G_FireMachinegun;
      } else if (!q_strcmp(it->def.classname, "weapon_handgrenades")) {
        it->Think = G_FireHandGrenade;
      } else if (!q_strcmp(it->def.classname, "weapon_grenadelauncher")) {
        it->Think = G_FireGrenadeLauncher;
      } else if (!q_strcmp(it->def.classname, "weapon_rocketlauncher")) {
        it->Think = G_FireRocketLauncher;
      } else if (!q_strcmp(it->def.classname, "weapon_hyperblaster")) {
        it->Think = G_FireHyperblaster;
      } else if (!q_strcmp(it->def.classname, "weapon_lightning")) {
        it->Think = G_FireLightning;
      } else if (!q_strcmp(it->def.classname, "weapon_railgun")) {
        it->Think = G_FireRailgun;
      } else if (!q_strcmp(it->def.classname, "weapon_bfg")) {
        it->Think = G_FireBfg;
      } else if (!q_strcmp(it->def.classname, "weapon_quake_shotgun")) {
        it->Think = G_FireQuakeShotgun;
      } else if (!q_strcmp(it->def.classname, "weapon_quake_supershotgun")) {
        it->Think = G_FireQuakeSuperShotgun;
      } else if (!q_strcmp(it->def.classname, "weapon_quake_nailgun")) {
        it->Think = G_FireQuakeNailgun;
      } else if (!q_strcmp(it->def.classname, "weapon_quake_supernailgun")) {
        it->Think = G_FireQuakeSuperNailgun;
      } else if (!q_strcmp(it->def.classname, "weapon_quake_grenadelauncher")) {
        it->Think = G_FireQuakeGrenadeLauncher;
      } else if (!q_strcmp(it->def.classname, "weapon_quake_rocketlauncher")) {
        it->Think = G_FireQuakeRocketLauncher;
      } else if (!q_strcmp(it->def.classname, "weapon_quake_thunderbolt")) {
        it->Think = G_FireQuakeThunderbolt;
      }
      break;

    case ITEM_TYPE_AMMO:
      it->Pickup = G_PickupAmmo;
      it->Drop = G_DropItem;
      if (!q_strcmp(it->def.classname, "ammo_grenades")) {
        it->Pickup = G_PickupGrenades;
        it->Use = G_UseGrenades;
      }
      break;

    case ITEM_TYPE_HEALTH:
      it->Pickup = G_PickupHealth;
      break;

    case ITEM_TYPE_POWERUP:
      if (!q_strcmp(it->def.classname, "item_adrenaline")) {
        it->Pickup = G_PickupAdrenaline;
      } else if (!q_strcmp(it->def.classname, "item_quad")) {
        it->Pickup = G_PickupQuadDamage;
      } else if (!q_strcmp(it->def.classname, "item_invisibility")) {
        it->Pickup = G_PickupInvisibility;
      } else if (!q_strcmp(it->def.classname, "item_invulnerability")) {
        it->Pickup = G_PickupInvulnerability;
      }
      break;

    default:
      G_Error("Item %s (tag %d) has an invalid type\n", it->def.name, it->def.tag);
  }

}

InitItem G_InitItem = G_InitItem_Common;

/**
 * @brief Fills in an item's behaviour and indexes its media. The behaviour is a
 * hook, so a feature answering for its own type never has to remember the media.
 */
static void G_SetupItem(GameItem *it) {

  G_InitItem(it);

  it->modelIndex = gi.ModelIndex(it->def.model);
  it->pickupSoundIndex = gi.SoundIndex(it->def.pickupSound);
}

/**
 * @brief Called to setup special private data for the item list.
 */
void G_InitItems(void) {

  gItems = gi.Malloc(ITEM_TOTAL * sizeof(GameItem), MEM_TAG_GAME);

  for (GameItemTag tag = ITEM_FIRST; tag < ITEM_TOTAL; tag++) {
    gItems[tag].def = bgItemDefs[tag];
    G_SetupItem(&gItems[tag]);
  }
}

