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

#include <Objectively/Array.h>
#include <Objectively/Lock.h>
#include <Objectively/Object.h>

#include <ObjectivelyMVC/Image.h>

#include "cg_types.h"

/**
 * @file
 * @brief ImageQueue: a thread-safe queue of images, loaded in the background.
 */

typedef struct ImageQueue ImageQueue;
typedef struct ImageQueueInterface ImageQueueInterface;

/**
 * @brief ImageQueue synchronizes all access to its backing store internally, so it may be
 * safely shared between the view that drains it on the main thread and the background thread
 * that fills it. Retaining an ImageQueue for a worker thread, rather than the view, means the
 * view's own teardown never has to wait on or race that worker.
 * @extends Object
 */
struct ImageQueue {

  /**
   * @brief The superclass.
   * @private
   */
  Object object;

  /**
   * @brief The interface type.
   * @private
   */
  ImageQueueInterface *interface[0];

  /**
   * @brief A lock guarding all fields below, held for the duration of every method below.
   * @private
   */
  Lock *lock;

  /**
   * @brief The loaded images, not yet drained.
   * @private
   */
  Array *images;

  /**
   * @brief `true` once the consumer has no further use for this queue.
   * @private
   */
  bool cancelled;
};

/**
 * @brief The ImageQueue interface.
 */
struct ImageQueueInterface {

  /**
   * @brief The superclass interface.
   */
  ObjectInterface objectInterface;

  /**
   * @fn ImageQueue *ImageQueue::init(ImageQueue *self)
   * @brief Initializes this ImageQueue.
   * @return The initialized ImageQueue, or `NULL` on error.
   * @memberof ImageQueue
   */
  ImageQueue *(*init)(ImageQueue *self);

  /**
   * @fn void ImageQueue::add(ImageQueue *self, Image *image)
   * @brief Adds `image` to this queue.
   * @param self The ImageQueue.
   * @param image The Image, which this queue retains.
   * @memberof ImageQueue
   */
  void (*add)(ImageQueue *self, Image *image);

  /**
   * @fn Array *ImageQueue::drain(ImageQueue *self)
   * @brief Removes all images from this queue.
   * @param self The ImageQueue.
   * @return The removed images, which the caller must release, or `NULL` if there were none.
   * @memberof ImageQueue
   */
  Array *(*drain)(ImageQueue *self);

  /**
   * @fn void ImageQueue::cancel(ImageQueue *self)
   * @brief Tells the worker filling this queue to stop.
   * @param self The ImageQueue.
   * @memberof ImageQueue
   */
  void (*cancel)(ImageQueue *self);

  /**
   * @fn bool ImageQueue::isCancelled(const ImageQueue *self)
   * @param self The ImageQueue.
   * @return `true` if this queue has been cancelled, `false` otherwise.
   * @memberof ImageQueue
   */
  bool (*isCancelled)(const ImageQueue *self);
};

/**
 * @fn Class *ImageQueue::_ImageQueue(void)
 * @brief The ImageQueue archetype.
 * @return The ImageQueue Class.
 * @memberof ImageQueue
 */
CGAME_EXPORT Class *_ImageQueue(void);
