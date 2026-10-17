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
 * Foundation, Inc., 59 Temple Place - Suite 330, Boston, MA 02111-1307, USA.
 */

#pragma once

#include <ObjectivelyMVC/SlideShowView.h>
#include <ObjectivelyMVC/View.h>

#include "ImageQueue.h"

/**
 * @file
 * @brief A map's mapshots as a thumbnail slideshow.
 */

typedef struct MapshotThumbnailView MapshotThumbnailView;
typedef struct MapshotThumbnailViewInterface MapshotThumbnailViewInterface;

/**
 * @brief Cycles through every mapshot of a map, starting from a random one.
 * @details The mapshots are found and loaded on a worker thread, scaled to this view's size in
 * pixels, once the view is first drawn. A view that is never on screen loads nothing, so a long
 * list costs only what it shows. Each view picks its own slide duration, so that a list of them
 * never changes in step.
 * @extends View
 */
struct MapshotThumbnailView {

  /**
   * @brief The superclass.
   */
  View view;

  /**
   * @brief The interface type.
   * @protected
   */
  MapshotThumbnailViewInterface *interface[0];

  /**
   * @brief The map whose mapshots this view shows.
   */
  char map[MAX_QPATH];

  /**
   * @brief The slideshow, which fills this view.
   * @private
   */
  SlideShowView *slideShow;

  /**
   * @brief The images the worker has loaded, or `NULL` until the view is first drawn.
   * @private
   */
  ImageQueue *queue;
};

/**
 * @brief The MapshotThumbnailView interface.
 */
struct MapshotThumbnailViewInterface {

  /**
   * @brief The superclass interface.
   */
  ViewInterface viewInterface;

  /**
   * @fn MapshotThumbnailView *MapshotThumbnailView::initWithFrame(MapshotThumbnailView *self, const SDL_Rect *frame)
   * @brief Initializes this MapshotThumbnailView with the specified frame.
   * @param self The MapshotThumbnailView.
   * @param frame The frame, or `NULL` for the default frame.
   * @return The initialized MapshotThumbnailView, or `NULL` on error.
   * @memberof MapshotThumbnailView
   */
  MapshotThumbnailView *(*initWithFrame)(MapshotThumbnailView *self, const SDL_Rect *frame);

  /**
   * @fn void MapshotThumbnailView::setMap(MapshotThumbnailView *self, const char *map)
   * @brief Shows the mapshots of `map`, replacing any shown before.
   * @param self The MapshotThumbnailView.
   * @param map The map name, or `NULL` to show nothing.
   * @remarks The mapshots are found on the worker thread, since that enumerates the filesystem.
   * @memberof MapshotThumbnailView
   */
  void (*setMap)(MapshotThumbnailView *self, const char *map);
};

CGAME_EXPORT Class *_MapshotThumbnailView(void);
