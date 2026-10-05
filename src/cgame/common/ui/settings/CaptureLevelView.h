/*
 * Copyright(c) 2026 Quetoo.
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

#include <ObjectivelyMVC/Text.h>

/**
 * @file
 * @brief The CaptureLevelView type.
 */

typedef struct CaptureLevelView CaptureLevelView;
typedef struct CaptureLevelViewInterface CaptureLevelViewInterface;

/**
 * @brief Live raw and gain-adjusted microphone meters, with an activation threshold marker.
 * @extends View
 */
struct CaptureLevelView {

  /**
   * @brief The superclass.
   * @private
   */
  View view;

  /**
   * @brief The interface type.
   * @private
   */
  CaptureLevelViewInterface *interface[0];

  /**
   * @brief The raw input caption.
   */
  Text *inputCaption;

  /**
   * @brief The gain-adjusted output caption.
   */
  Text *outputCaption;

  /**
   * @brief The raw input display envelope, with immediate attack and a short decay.
   */
  float input;

  /**
   * @brief The gain-adjusted output display envelope.
   */
  float output;

  /**
   * @brief The timestamp of the last meter update.
   */
  uint64_t timestamp;

  /**
   * @brief The meter fill color.
   * @styled
   */
  SDL_Color levelColor;

  /**
   * @brief The track background color.
   * @styled
   */
  SDL_Color trackColor;

  /**
   * @brief The raw activation threshold marker color.
   * @styled
   */
  SDL_Color thresholdColor;

  /**
   * @brief The output clipping color.
   * @styled
   */
  SDL_Color clippingColor;
};

/**
 * @brief The CaptureLevelView interface.
 */
struct CaptureLevelViewInterface {

  /**
   * @brief The superclass interface.
   */
  ViewInterface viewInterface;

  /**
   * @fn CaptureLevelView *CaptureLevelView::initWithFrame(CaptureLevelView *self, const SDL_Rect *frame)
   * @brief Initializes the live microphone meter.
   * @param frame The frame, or `NULL` for stylesheet-driven layout.
   * @return The initialized CaptureLevelView, or `NULL` on error.
   * @memberof CaptureLevelView
   */
  CaptureLevelView *(*initWithFrame)(CaptureLevelView *self, const SDL_Rect *frame);
};

/**
 * @fn Class *CaptureLevelView::_CaptureLevelView(void)
 * @brief The CaptureLevelView archetype.
 * @return The CaptureLevelView Class.
 * @memberof CaptureLevelView
 */
CGAME_EXPORT Class *_CaptureLevelView(void);
