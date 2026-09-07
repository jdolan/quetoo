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
#include <ObjectivelyMVC/View.h>

/**
 * @file
 * @brief The full-screen blends: liquid tint, pickup and damage flashes, powerup glow.
 */

typedef enum {
  BlendViewPickup,
  BlendViewQuad,
  BlendViewInvisibility,
  BlendViewInvulnerability,
  BlendViewDamage,
  BlendViewTotal
} BlendViewFlash;

typedef struct BlendView BlendView;
typedef struct BlendViewInterface BlendViewInterface;

/**
 * @brief The full-screen blends: liquid tint, pickup and damage flashes, powerup glow.
 * @details The liquid tint is this View's background colour, resolved from the material the
 * view origin is in; each flash is a full-screen ImageView whose alpha follows its timer.
 * Governed by the `cg_draw_blend*` cvars. Fills the HUD, so it SHOULD be the first element of
 * a variant.
 * @extends View
 */
struct BlendView {

  /**
   * @brief The superclass.
   */
  View view;

  /**
   * @brief The interface type.
   * @protected
   */
  BlendViewInterface *interface[0];

  /**
   * @brief The flashes, by BlendViewFlash.
   */
  ImageView *flashes[BlendViewTotal];
};

struct BlendViewInterface {

  /**
   * @brief The superclass interface.
   */
  ViewInterface viewInterface;
};

CGAME_EXPORT Class *_BlendView(void);
