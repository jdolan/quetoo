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
#include <ObjectivelyMVC/Control.h>

/**
 * @file
 * @brief The CrosshairPreviewView type.
 */

typedef struct CrosshairPreviewView CrosshairPreviewView;
typedef struct CrosshairPreviewViewInterface CrosshairPreviewViewInterface;

/**
 * @brief The CrosshairPreviewView type.
 * @extends View
 */
struct CrosshairPreviewView {

  /**
   * @brief The superclass.
   * @private
   */
  Control control;

  /**
   * @brief The interface type.
   * @private
   */
  CrosshairPreviewViewInterface *interface[0];

  /**
   * @brief The ImageView.
   */
  ImageView *imageView;
};

/**
 * @brief The CrosshairPreviewView interface.
 */
struct CrosshairPreviewViewInterface {

  /**
   * @brief The superclass interface.
   */
  ControlInterface controlInterface;

  /**
   * @fn CrosshairPreviewView *CrosshairPreviewView::initWithFrame(CrosshairPreviewView *self, const SDL_Rect *frame)
   * @brief Initializes this CrosshairPreviewView with the specified frame.
   * @param frame The frame.
   * @return The initialized CrosshairPreviewView, or `NULL` on error.
   * @memberof CrosshairPreviewView
   */
  CrosshairPreviewView *(*initWithFrame)(CrosshairPreviewView *self, const SDL_Rect *frame);
};

/**
 * @fn Class *CrosshairPreviewView::_CrosshairPreviewView(void)
 * @brief The CrosshairPreviewView archetype.
 * @return The CrosshairPreviewView Class.
 * @memberof CrosshairPreviewView
 */
CGAME_EXPORT Class *_CrosshairPreviewView(void);

