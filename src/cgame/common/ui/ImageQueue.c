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

#include "cg_local.h"

#include "ImageQueue.h"

#define _Class _ImageQueue

#pragma mark - Object

/**
 * @see Object::dealloc(Object *)
 */
static void dealloc(Object *self) {

  ImageQueue *this = (ImageQueue *) self;

  release(this->lock);
  release(this->images);

  super(Object, self, dealloc);
}

#pragma mark - ImageQueue

/**
 * @fn ImageQueue *ImageQueue::init(ImageQueue *self)
 * @memberof ImageQueue
 */
static ImageQueue *init(ImageQueue *self) {

  self = (ImageQueue *) super(Object, self, init);
  if (self) {
    self->lock = $(alloc(Lock), init);
    assert(self->lock);

    self->images = $(alloc(Array), init);
    assert(self->images);
  }

  return self;
}

/**
 * @fn void ImageQueue::add(ImageQueue *self, Image *image)
 * @memberof ImageQueue
 */
static void add(ImageQueue *self, Image *image) {

  synchronized(self->lock, {
    $(self->images, addObject, image);
  });
}

/**
 * @fn Array *ImageQueue::drain(ImageQueue *self)
 * @memberof ImageQueue
 */
static Array *drain(ImageQueue *self) {

  Array *images = NULL;
  synchronized(self->lock, {
    if (self->images->count) {
      images = $(alloc(Array), initWithArray, self->images);
      $(self->images, removeAllObjects);
    }
  });

  return images;
}

/**
 * @fn void ImageQueue::cancel(ImageQueue *self)
 * @memberof ImageQueue
 */
static void cancel(ImageQueue *self) {

  synchronized(self->lock, {
    self->cancelled = true;
  });
}

/**
 * @fn bool ImageQueue::isCancelled(const ImageQueue *self)
 * @memberof ImageQueue
 */
static bool isCancelled(const ImageQueue *self) {

  bool cancelled;
  synchronized(self->lock, {
    cancelled = self->cancelled;
  });

  return cancelled;
}

#pragma mark - Class lifecycle

/**
 * @see Class::initialize(Class *)
 */
static void initialize(Class *clazz) {

  ((ObjectInterface *) clazz->interface)->dealloc = dealloc;

  ((ImageQueueInterface *) clazz->interface)->init = init;
  ((ImageQueueInterface *) clazz->interface)->add = add;
  ((ImageQueueInterface *) clazz->interface)->drain = drain;
  ((ImageQueueInterface *) clazz->interface)->cancel = cancel;
  ((ImageQueueInterface *) clazz->interface)->isCancelled = isCancelled;
}

/**
 * @fn Class *ImageQueue::_ImageQueue(void)
 * @memberof ImageQueue
 */
Class *_ImageQueue(void) {
  static Class *clazz;
  static Once once;

  do_once(&once, {
    clazz = _initialize(&(const ClassDef) {
      .name = "ImageQueue",
      .superclass = _Object(),
      .instanceSize = sizeof(ImageQueue),
      .interfaceSize = sizeof(ImageQueueInterface),
      .initialize = initialize,
    });
  });

  return clazz;
}

#undef _Class
