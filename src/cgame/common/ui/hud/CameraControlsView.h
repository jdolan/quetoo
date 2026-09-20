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

#include "cg_types.h"

/**
 * @file
 * @brief Announces the camera the viewer is watching through, while spectating a live game or
 * playing a demo back. It shows itself when the camera changes and hides again shortly after,
 * the way the weapon bar does, so that it says what happened without sitting on the screen.
 */

typedef struct CameraControlsView CameraControlsView;
typedef struct CameraControlsViewInterface CameraControlsViewInterface;

/**
 * @brief The camera announcement, shown briefly whenever the camera changes.
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
   * @brief The camera icon.
   */
  ImageView *icon;

  /**
   * @brief The camera name.
   */
  Text *name;

  /**
   * @brief What was last announced, so that the view shows itself only when this changes.
   */
  CGameCameraMode mode;
  bool detached;

  /**
   * @brief When to hide again, in unclamped client time.
   */
  uint32_t time;
};

struct CameraControlsViewInterface {

  /**
   * @brief The superclass interface.
   */
  StackViewInterface stackViewInterface;
};

CGAME_EXPORT Class *_CameraControlsView(void);
