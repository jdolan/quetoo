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

#include <ObjectivelyMVC/Button.h>
#include <ObjectivelyMVC/StackView.h>

/**
 * @file
 * @brief A single control that cycles first-person, third-person orbit, and free-flight
 * cameras, shown during demo playback and during live in-game spectating (chasing or free
 * spectator flight) - never while actively playing.
 */

typedef struct CameraControlsView CameraControlsView;
typedef struct CameraControlsViewInterface CameraControlsViewInterface;

/**
 * @brief The camera mode control, shown while spectating (live or demo).
 * @extends StackView
 */
struct CameraControlsView {

  /**
   * @brief The superclass.
   */
  StackView stackView;

  /**
   * @brief The interface type.
   * @protected
   */
  CameraControlsViewInterface *interface[0];

  /**
   * @brief Cycles `camera_mode_cycle`, labeled with the current mode.
   */
  Button *cameraModeButton;
};

struct CameraControlsViewInterface {

  /**
   * @brief The superclass interface.
   */
  StackViewInterface stackViewInterface;

  /**
   * @fn CameraControlsView *CameraControlsView::initWithFrame(CameraControlsView *self, const SDL_Rect *frame)
   * @brief Initializes this CameraControlsView.
   * @param self The CameraControlsView.
   * @param frame The frame.
   * @return The initialized CameraControlsView, or `NULL` on error.
   * @memberof CameraControlsView
   */
  CameraControlsView *(*initWithFrame)(CameraControlsView *self, const SDL_Rect *frame);

  /**
   * @fn void CameraControlsView::update(CameraControlsView *self)
   * @brief Refreshes the button's label to reflect the current camera mode.
   * @param self The CameraControlsView.
   * @memberof CameraControlsView
   */
  void (*update)(CameraControlsView *self);
};

CGAME_EXPORT Class *_CameraControlsView(void);
