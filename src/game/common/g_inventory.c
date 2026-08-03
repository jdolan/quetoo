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
 * @brief The tail of the `G_ResetDroppedItem` chain, disposing of an item that
 * has left the world. Features that would rather recycle it install over the
 * top.
 */
static void G_ResetDroppedItem_Default(g_entity_t *ent) {
  G_FreeEntity(ent);
}

ResetDroppedItem G_ResetDroppedItem = G_ResetDroppedItem_Default;

/**
 * @brief The tail of the `G_DropInventoryItem` chain, resolving the name
 * against the item list.
 */
static void G_DropInventoryItem_Default(g_client_t *cl, const char *name) {
  const g_item_t *it;

  // we don't drop in instagib or arena
  if (g_level.gameplay) {
    return;
  }

  if (cl->entity->dead) {
    return;
  }

  it = G_FindItem(name);

  if (!it) {
    gi.ClientPrint(cl, PRINT_HIGH, "Unknown item: %s\n", name);
    return;
  }

  if (!it->Drop) {
    gi.ClientPrint(cl, PRINT_HIGH, "Item can not be dropped\n");
    return;
  }

  const g_item_tag_t index = it->def.tag;

  if (cl->inventory[index] == 0) {
    gi.ClientPrint(cl, PRINT_HIGH, "Out of item: %s\n", name);
    return;
  }

  int32_t drop_quantity;

  if (it->def.type == ITEM_TYPE_AMMO) {
    drop_quantity = it->def.quantity;
  } else {
    drop_quantity = 1;
  }

  if (cl->inventory[index] < drop_quantity) {
    gi.ClientPrint(cl, PRINT_HIGH, "Quantity too low: %s\n", name);
    return;
  }

  cl->inventory[index] -= drop_quantity;
  cl->last_dropped = it;

  it->Drop(cl, it);

  // adjust weapon if we need to
  if (it->def.type == ITEM_TYPE_WEAPON) {
    if (cl->weapon == it && !cl->next_weapon && !cl->inventory[index]) {
      G_UseBestWeapon(cl);
    }
  }
}

DropInventoryItem G_DropInventoryItem = G_DropInventoryItem_Default;
