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

/**
 * @brief The default for `G_ResetDroppedItem`, and the end of any chain a
 * feature installs over it.
 */
static void FreeDroppedItem(g_entity_t *ent) {
  G_FreeEntity(ent);
}

ResetDroppedItem G_ResetDroppedItem = FreeDroppedItem;

/**
 * @brief The default for `G_ModifyDamage`, applying the quad damage powerup.
 */
static void ScalePowerupDamage(g_entity_t *target, g_entity_t *attacker, int32_t *damage, int32_t *knockback) {

  if (attacker->client) {
    if (attacker->client->inventory[POWERUP_QUAD]) {
      *damage *= QUAD_DAMAGE_FACTOR;
      *knockback *= QUAD_KNOCKBACK_FACTOR;
    }
  }
}

ModifyDamage G_ModifyDamage = ScalePowerupDamage;
