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
 * @brief The weapon bar: every carried weapon in a row, the selection marked and named.
 */

typedef struct WeaponBarView WeaponBarView;
typedef struct WeaponBarViewInterface WeaponBarViewInterface;

/**
 * @brief The weapon bar: every carried weapon in a row, the selection marked and named.
 * @details Shown while a weapon change is pending or was just made, fading over
 * `cg_select_weapon_fade`; the selection state itself lives in `cg_hud_state.weapon`, driven
 * by Cg_UpdateSelectWeapon and the `cg_weapon_next` and `cg_weapon_previous` commands.
 * @extends StackView
 */
struct WeaponBarView {

  /**
   * @brief The superclass.
   */
  StackView stackView;

  /**
   * @brief The interface type.
   * @protected
   */
  WeaponBarViewInterface *interface[0];

  /**
   * @brief The weapons shown, so the row is rebuilt only when the inventory changes.
   */
  bool has[WEAPON_TOTAL];

  /**
   * @brief The icon row.
   */
  StackView *icons;

  /**
   * @brief The name of the selected weapon.
   */
  Text *name;

  /**
   * @brief The icon row and the selection marker over it.
   */
  View *row;

  /**
   * @brief The selection marker.
   */
  ImageView *selection;
};

struct WeaponBarViewInterface {

  /**
   * @brief The superclass interface.
   */
  StackViewInterface stackViewInterface;
};

CGAME_EXPORT Class *_WeaponBarView(void);
