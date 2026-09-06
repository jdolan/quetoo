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
 * @brief The crosshair, coloured by health and pulsed by pickups.
 */

typedef struct CrosshairView CrosshairView;
typedef struct CrosshairViewInterface CrosshairViewInterface;

/**
 * @brief The crosshair, coloured by health and pulsed by pickups.
 * @details Reads the `cg_draw_crosshair*` cvars as the settings preview does, and adds what
 * only play knows: the `cg_crosshair_health_t` schemes and the pickup pulse. Hidden when
 * dead, spectating, in third person, behind the scoreboard or a center print, or when there
 * is no weapon. In the editor it shows regardless, in white.
 * @extends View
 */
struct CrosshairView {

  /**
   * @brief The superclass.
   */
  View view;

  /**
   * @brief The interface type.
   * @protected
   */
  CrosshairViewInterface *interface[0];

  /**
   * @brief The colour from `cg_draw_crosshair_color`, before health and pulse apply.
   */
  vec4_t color;

  /**
   * @brief The crosshair image.
   */
  ImageView *imageView;
};

struct CrosshairViewInterface {

  /**
   * @brief The superclass interface.
   */
  ViewInterface viewInterface;
};

CGAME_EXPORT Class *_CrosshairView(void);
