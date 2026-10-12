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
 * @brief HeroImageList: a thread-safe queue of hero images, fetched in the background.
 */

typedef struct HeroImageList HeroImageList;
typedef struct HeroImageListInterface HeroImageListInterface;

/**
 * @brief HeroImageList synchronizes all access to its backing store internally, so it may be
 * safely shared between the UpdateViewController that drains it and the background thread that
 * fills it. Retaining a HeroImageList for a worker thread, rather than the ViewController, means
 * the ViewController's own teardown never has to wait on or race that worker.
 * @extends Object
 */
struct HeroImageList {

  /**
   * @brief The superclass.
   * @private
   */
  Object object;

  /**
   * @brief The interface type.
   * @private
   */
  HeroImageListInterface *interface[0];

  /**
   * @brief A lock guarding all fields below, held for the duration of every method below.
   * @private
   */
  Lock *lock;

  /**
   * @brief The fetched images, not yet drained.
   * @private
   */
  Array *images;

  /**
   * @brief `true` once the consumer has no further use for this list.
   * @private
   */
  bool cancelled;
};

/**
 * @brief The HeroImageList interface.
 */
struct HeroImageListInterface {

  /**
   * @brief The superclass interface.
   */
  ObjectInterface objectInterface;

  /**
   * @fn HeroImageList *HeroImageList::init(HeroImageList *self)
   * @brief Initializes this HeroImageList.
   * @return The initialized HeroImageList, or `NULL` on error.
   * @memberof HeroImageList
   */
  HeroImageList *(*init)(HeroImageList *self);

  /**
   * @fn void HeroImageList::add(HeroImageList *self, Image *image)
   * @brief Adds `image` to this list.
   * @param self The HeroImageList.
   * @param image The Image, which this list retains.
   * @memberof HeroImageList
   */
  void (*add)(HeroImageList *self, Image *image);

  /**
   * @fn Array *HeroImageList::drain(HeroImageList *self)
   * @brief Removes all images from this list.
   * @param self The HeroImageList.
   * @return The removed images, which the caller must release, or `NULL` if there were none.
   * @memberof HeroImageList
   */
  Array *(*drain)(HeroImageList *self);

  /**
   * @fn void HeroImageList::cancel(HeroImageList *self)
   * @brief Tells the worker filling this list to stop.
   * @param self The HeroImageList.
   * @memberof HeroImageList
   */
  void (*cancel)(HeroImageList *self);

  /**
   * @fn bool HeroImageList::isCancelled(const HeroImageList *self)
   * @param self The HeroImageList.
   * @return `true` if this list has been cancelled, `false` otherwise.
   * @memberof HeroImageList
   */
  bool (*isCancelled)(const HeroImageList *self);
};

/**
 * @fn Class *HeroImageList::_HeroImageList(void)
 * @brief The HeroImageList archetype.
 * @return The HeroImageList Class.
 * @memberof HeroImageList
 */
CGAME_EXPORT Class *_HeroImageList(void);
