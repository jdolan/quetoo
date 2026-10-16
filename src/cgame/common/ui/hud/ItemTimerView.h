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

#include "PowerupView.h"

/**
 * @file
 * @brief The seconds until an item respawns, from its `CS_ITEM_TIMER_*` config string.
 */

typedef enum {
  ItemTimerViewMega,
  ItemTimerViewQuad,
  ItemTimerViewInvulnerability,
  ItemTimerViewInvisibility
} ItemTimerViewItem;

typedef struct ItemTimerView ItemTimerView;
typedef struct ItemTimerViewInterface ItemTimerViewInterface;

/**
 * @brief The seconds until an item respawns: its icon beside the countdown.
 * @details Configured in JSON by `item`: `mega`, `quad`, `invulnerability` or
 * `invisibility`. Hides while the item is in play, and while the server's `g_itemTimers` is
 * off, as the server then leaves the config string empty.
 * @extends PowerupView
 */
struct ItemTimerView {

  /**
   * @brief The superclass.
   */
  PowerupView powerupView;

  /**
   * @brief The interface type.
   * @protected
   */
  ItemTimerViewInterface *interface[0];

  /**
   * @brief The item timed.
   */
  ItemTimerViewItem item;
};

struct ItemTimerViewInterface {

  /**
   * @brief The superclass interface.
   */
  PowerupViewInterface powerupViewInterface;
};

CGAME_EXPORT Class *_ItemTimerView(void);
