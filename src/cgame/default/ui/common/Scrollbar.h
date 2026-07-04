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
#include <ObjectivelyMVC/Control.h>
#include <ObjectivelyMVC/View.h>
#include <ObjectivelyMVC/ScrollView.h>

#include "ScrollThumb.h"

#include "cg_types.h"

/**
 * @file
 * @brief A visible, draggable vertical scrollbar for a ScrollView.
 * @details MVC's ScrollView only pans its content (wheel / drag / offset) and
 * draws no indicator. Scrollbar adds one. It is a container View (the "bar
 * base") that lays its three regions out vertically by hand -- fixed caps top
 * and bottom, the panel filling between them. All interactivity lives in its
 * children: the ScrollThumb drags, the adorners step-scroll on click.
 *
 * Anatomy:
 *
 *   Scrollbar (bar base, vertical View)
 *   |-- topAdorner     step-up cap at the top, height = -adorner-size
 *   |-- scrollPanel    the middle track the thumb travels within
 *   |     `-- thumb    the ScrollThumb grabby-widget
 *   `-- bottomAdorner  step-down cap at the bottom, height = -adorner-size
 *
 * The thumb's size tracks the visible/content ratio and its position tracks the
 * ScrollView's contentOffset; dragging the thumb scrolls the content, clicking
 * an adorner steps it, and the bar re-syncs the thumb whenever the content's
 * offset changes (the base ScrollView fires no didScroll callback). The whole
 * bar may sit on the LEFT or RIGHT of the content (see -orientation).
 *
 * Styling (all set in CSS, read in applyStyle):
 *   width          the bar thickness (standard View attribute)
 *   -adorner-size  the height of the top and bottom caps
 *   -orientation   left | right -- which side of the content the bar occupies
 *   -bgcolor       the bar-base color; the adorners derive a calculated shade
 *                  of this so the caps read as part of the bar
 *   -fgcolor       the thumb (grabby control) color
 */

/**
 * @brief Which side of the content a Scrollbar occupies.
 */
typedef enum {
  ScrollbarOrientationRight,
  ScrollbarOrientationLeft
} ScrollbarOrientation;

/**
 * @brief String <-> ScrollbarOrientation mapping for the `-orientation` attribute.
 */
extern const EnumName ScrollbarOrientationNames[];

typedef struct Scrollbar Scrollbar;
typedef struct ScrollbarInterface ScrollbarInterface;

/**
 * @brief A draggable scrollbar bound to a ScrollView.
 * @extends View
 */
struct Scrollbar {

  /**
   * @brief The superclass.
   * @private
   */
  View view;

  /**
   * @brief The interface.
   * @private
   */
  ScrollbarInterface *interface;

  /**
   * @brief The ScrollView this bar drives. Weak reference (not retained).
   */
  ScrollView *scrollView;

  /**
   * @brief The step-up cap at the top of the bar (.adorner). A click steps up.
   */
  Button *topAdorner;

  /**
   * @brief The middle track within which the thumb travels (.scrollPanel).
   */
  View *scrollPanel;

  /**
   * @brief The step-down cap at the bottom of the bar (.adorner). Click steps down.
   */
  Button *bottomAdorner;

  /**
   * @brief The grabby-widget: the draggable thumb (.thumb).
   */
  ScrollThumb *thumb;

  /**
   * @brief The contentOffset the thumb was last synced to (for change detection).
   * @private
   */
  SDL_Point syncedOffset;

  /**
   * @brief Which side of the content the bar sits on. Attribute `-orientation`.
   */
  ScrollbarOrientation orientation;

  /**
   * @brief The height of the top and bottom caps. Attribute `-adorner-size`.
   */
  int adornerSize;

  /**
   * @brief The bar-base color. Attribute `-bgcolor`. Adorners derive a shade.
   */
  SDL_Color bgColor;

  /**
   * @brief The thumb color. Attribute `-fgcolor`.
   */
  SDL_Color fgColor;
};

/**
 * @brief The Scrollbar interface.
 */
struct ScrollbarInterface {

  /**
   * @brief The superclass interface.
   */
  ViewInterface viewInterface;

  /**
   * @fn Scrollbar *Scrollbar::initWithScrollView(Scrollbar *self, ScrollView *scrollView)
   * @brief Initializes this Scrollbar bound to the given ScrollView.
   * @param self The Scrollbar.
   * @param scrollView The ScrollView to drive, or `NULL` (set later).
   * @return The initialized Scrollbar, or `NULL` on error.
   * @memberof Scrollbar
   */
  Scrollbar *(*initWithScrollView)(Scrollbar *self, ScrollView *scrollView);

  /**
   * @fn void Scrollbar::setScrollView(Scrollbar *self, ScrollView *scrollView)
   * @brief Binds this Scrollbar to the given ScrollView and syncs the thumb.
   * @param self The Scrollbar.
   * @param scrollView The ScrollView to drive.
   * @memberof Scrollbar
   */
  void (*setScrollView)(Scrollbar *self, ScrollView *scrollView);

  /**
   * @fn void Scrollbar::update(Scrollbar *self)
   * @brief Resizes and repositions the thumb from the ScrollView's geometry.
   * @param self The Scrollbar.
   * @memberof Scrollbar
   */
  void (*update)(Scrollbar *self);
};

/**
 * @fn Class *Scrollbar::_Scrollbar(void)
 * @brief The Scrollbar archetype.
 * @return The Scrollbar Class.
 * @memberof Scrollbar
 */
CGAME_EXPORT Class *_Scrollbar(void);
