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

#include <ObjectivelyMVC/Text.h>

/**
 * @file
 * @brief A Text that shows, or hides, a string each frame.
 */

typedef struct OverlayText OverlayText;
typedef struct OverlayTextInterface OverlayTextInterface;

/**
 * @brief A Text that shows, or hides, a string each frame.
 * @details Subclasses override OverlayText::textForFrame to return what to show for the
 * frame, or `NULL` to hide. The string is copied by Text::setText, so a `va` buffer is fine.
 * @extends Text
 */
struct OverlayText {

  /**
   * @brief The superclass.
   */
  Text text;

  /**
   * @brief The interface type.
   * @protected
   */
  OverlayTextInterface *interface[0];
};

struct OverlayTextInterface {

  /**
   * @brief The superclass interface.
   */
  TextInterface textInterface;

  /**
   * @fn const char *OverlayText::textForFrame(OverlayText *self, const cl_frame_t *frame)
   * @brief Resolves the text to show for the given frame.
   * @param self The OverlayText.
   * @param frame The frame.
   * @return The text, or `NULL` to hide this view.
   * @memberof OverlayText
   */
  const char *(*textForFrame)(OverlayText *self, const cl_frame_t *frame);
};

CGAME_EXPORT Class *_OverlayText(void);
