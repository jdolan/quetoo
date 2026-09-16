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
#include <ObjectivelyMVC/Slider.h>
#include <ObjectivelyMVC/StackView.h>

/**
 * @file
 * @brief Demo playback transport controls: rewind, resume, fast-forward, a scrubber and a
 * speed slider. Visible only while demo playback is paused - never during active playback, so
 * it never intrudes on a video capture.
 */

typedef struct DemoControlsView DemoControlsView;
typedef struct DemoControlsViewInterface DemoControlsViewInterface;

/**
 * @brief Demo playback transport controls, shown only while paused.
 * @extends StackView
 */
struct DemoControlsView {

  /**
   * @brief The superclass.
   */
  StackView stackView;

  /**
   * @brief The interface type.
   * @protected
   */
  DemoControlsViewInterface *interface[0];

  /**
   * @brief Seeks back 10 seconds.
   */
  Button *rewindButton;

  /**
   * @brief Resumes playback (sends `demo_pause`).
   */
  Button *playButton;

  /**
   * @brief Seeks forward 10 seconds.
   */
  Button *fastForwardButton;

  /**
   * @brief Scrubs to an arbitrary point in the recording.
   */
  Slider *scrubber;

  /**
   * @brief Adjusts `time_scale`.
   */
  Slider *speedSlider;
};

struct DemoControlsViewInterface {

  /**
   * @brief The superclass interface.
   */
  StackViewInterface stackViewInterface;

  /**
   * @fn DemoControlsView *DemoControlsView::initWithFrame(DemoControlsView *self, const SDL_Rect *frame)
   * @brief Initializes this DemoControlsView.
   * @param self The DemoControlsView.
   * @param frame The frame.
   * @return The initialized DemoControlsView, or `NULL` on error.
   * @memberof DemoControlsView
   */
  DemoControlsView *(*initWithFrame)(DemoControlsView *self, const SDL_Rect *frame);

  /**
   * @fn void DemoControlsView::update(DemoControlsView *self, int32_t time, int32_t duration)
   * @brief Refreshes the scrubber and speed slider to reflect the current playback position.
   * @param self The DemoControlsView.
   * @param time The current playback position, in milliseconds.
   * @param duration The total duration of the recording, in milliseconds.
   * @memberof DemoControlsView
   */
  void (*update)(DemoControlsView *self, int32_t time, int32_t duration);
};

CGAME_EXPORT Class *_DemoControlsView(void);
