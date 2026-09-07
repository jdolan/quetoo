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

#include "CounterView.h"

/**
 * @file
 * @brief The frame rate, counted by the HUD itself.
 */

typedef struct FpsView FpsView;
typedef struct FpsViewInterface FpsViewInterface;

/**
 * @brief The frame rate, counted by the HUD itself and refreshed once a second.
 * @details A live value, so its text carries the class name `live` rather than `number`.
 * Hidden when `cg_draw_fps` is off.
 * @extends CounterView
 */
struct FpsView {

  /**
   * @brief The superclass.
   */
  CounterView counterView;

  /**
   * @brief The interface type.
   * @protected
   */
  FpsViewInterface *interface[0];

  /**
   * @brief Frames since `time`, and when the count last rolled over.
   */
  int32_t frames, fps;
  uint32_t time;
};

struct FpsViewInterface {

  /**
   * @brief The superclass interface.
   */
  CounterViewInterface counterViewInterface;
};

CGAME_EXPORT Class *_FpsView(void);
