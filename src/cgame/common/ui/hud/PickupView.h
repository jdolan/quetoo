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
 * Foundation, Inc., 59 Temple Place - Suite 330, Boston, MA 02111-1307, USA.
 */

#pragma once

#include <ObjectivelyMVC/ImageView.h>
#include <ObjectivelyMVC/StackView.h>
#include <ObjectivelyMVC/Text.h>

/**
 * @file
 * @brief The item most recently picked up: its icon beside its name. While the player waits to
 * respawn, the seconds until they may respawn.
 */

typedef struct PickupView PickupView;
typedef struct PickupViewInterface PickupViewInterface;

/**
 * @brief The item most recently picked up: its icon beside its name. While the player waits to
 * respawn after a long delay, the seconds until it may, in place of the item: a dead player picks
 * nothing up. Hidden when neither.
 * @extends StackView
 */
struct PickupView {

  /**
   * @brief The superclass.
   */
  StackView stackView;

  /**
   * @brief The interface type.
   * @protected
   */
  PickupViewInterface *interface[0];

  /**
   * @brief The icon.
   */
  ImageView *icon;

  /**
   * @brief The item shown, or `ITEM_NONE`.
   */
  GameItemTag item;

  /**
   * @brief The item name, or the respawn countdown.
   */
  Text *name;

  /**
   * @brief The seconds shown until the player may respawn, or 0.
   */
  int16_t respawn;
};

struct PickupViewInterface {

  /**
   * @brief The superclass interface.
   */
  StackViewInterface stackViewInterface;
};

CGAME_EXPORT Class *_PickupView(void);
