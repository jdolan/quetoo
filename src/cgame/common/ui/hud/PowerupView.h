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

#include <ObjectivelyMVC/ImageView.h>
#include <ObjectivelyMVC/StackView.h>
#include <ObjectivelyMVC/Text.h>

/**
 * @file
 * @brief A held powerup: its icon beside the seconds remaining.
 */

typedef enum {
  PowerupViewNone = -1,
  PowerupViewQuad,
  PowerupViewInvulnerability,
  PowerupViewInvisibility
} PowerupViewPowerup;

typedef struct PowerupView PowerupView;
typedef struct PowerupViewInterface PowerupViewInterface;

/**
 * @brief A held powerup: its icon beside the seconds remaining.
 * @details Configured in JSON by `powerup`: `quad`, `invulnerability` or `invisibility`. Hides
 * at zero and turns red under `HUD_POWERUP_LOW`. A module showing something else in the
 * powerup column subclasses this, initialized with `PowerupViewNone`, and feeds
 * PowerupView::update from its own state.
 * @extends StackView
 */
struct PowerupView {

  /**
   * @brief The superclass.
   */
  StackView stackView;

  /**
   * @brief The interface type.
   * @protected
   */
  PowerupViewInterface *interface[0];

  /**
   * @brief The icon.
   */
  ImageView *icon;

  /**
   * @brief The item whose icon is shown, or `ITEM_NONE`.
   */
  g_item_tag_t item;

  /**
   * @brief The powerup shown.
   */
  PowerupViewPowerup powerup;

  /**
   * @brief The seconds remaining.
   */
  Text *value;
};

struct PowerupViewInterface {

  /**
   * @brief The superclass interface.
   */
  StackViewInterface stackViewInterface;

  /**
   * @fn PowerupView *PowerupView::initWithPowerup(PowerupView *self, PowerupViewPowerup powerup)
   * @brief Initializes this PowerupView for the given powerup.
   * @param self The PowerupView.
   * @param powerup The powerup.
   * @return The initialized PowerupView, or `NULL` on error.
   * @memberof PowerupView
   */
  PowerupView *(*initWithPowerup)(PowerupView *self, PowerupViewPowerup powerup);

  /**
   * @fn void PowerupView::update(PowerupView *self, g_item_tag_t item, int16_t value)
   * @brief Shows the given item with the given seconds remaining, or hides this view when
   * `value` is not positive.
   * @param self The PowerupView.
   * @param item The item whose icon to show.
   * @param value The seconds remaining, or `0` to hide; negative shows the icon alone.
   * @memberof PowerupView
   */
  void (*update)(PowerupView *self, g_item_tag_t item, int16_t value);
};

CGAME_EXPORT Class *_PowerupView(void);
