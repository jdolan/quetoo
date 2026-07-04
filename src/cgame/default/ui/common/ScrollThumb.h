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

#include <ObjectivelyMVC/Control.h>

#include "cg_types.h"

/**
 * @file
 * @brief The draggable "grabby-widget" of a Scrollbar.
 * @details ScrollThumb is the only interactive part of a Scrollbar: a Control
 * that, while grabbed, reports each increment of vertical mouse motion to its
 * delegate. Its owner (Scrollbar) maps that motion onto the ScrollView's
 * content offset. Kept generic (no ScrollView knowledge) and deliberately not
 * named like Slider's handle to avoid any control-name collision.
 */

typedef struct ScrollThumb ScrollThumb;
typedef struct ScrollThumbInterface ScrollThumbInterface;

/**
 * @brief The ScrollThumb delegate protocol.
 */
typedef struct {

  /**
   * @brief The delegate self pointer (the Scrollbar).
   */
  ident self;

  /**
   * @brief Called for each mouse-motion increment while the thumb is grabbed.
   * @param thumb The ScrollThumb.
   * @param delta The vertical motion this event, in window pixels.
   */
  void (*didDrag)(ScrollThumb *thumb, int delta);

} ScrollThumbDelegate;

/**
 * @brief A draggable scrollbar thumb.
 * @extends Control
 */
struct ScrollThumb {

  /**
   * @brief The superclass.
   * @private
   */
  Control control;

  /**
   * @brief The interface.
   * @private
   */
  ScrollThumbInterface *interface;

  /**
   * @brief The delegate.
   */
  ScrollThumbDelegate delegate;
};

/**
 * @brief The ScrollThumb interface.
 */
struct ScrollThumbInterface {

  /**
   * @brief The superclass interface.
   */
  ControlInterface controlInterface;

  /**
   * @fn ScrollThumb *ScrollThumb::initWithFrame(ScrollThumb *self, const SDL_Rect *frame)
   * @brief Initializes this ScrollThumb with the given frame.
   * @param self The ScrollThumb.
   * @param frame The frame, or `NULL`.
   * @return The initialized ScrollThumb, or `NULL` on error.
   * @memberof ScrollThumb
   */
  ScrollThumb *(*initWithFrame)(ScrollThumb *self, const SDL_Rect *frame);
};

/**
 * @fn Class *ScrollThumb::_ScrollThumb(void)
 * @brief The ScrollThumb archetype.
 * @return The ScrollThumb Class.
 * @memberof ScrollThumb
 */
CGAME_EXPORT Class *_ScrollThumb(void);
