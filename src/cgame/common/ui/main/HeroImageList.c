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

#include "HeroImageList.h"

#define _Class _HeroImageList

#pragma mark - Object

/**
 * @see Object::dealloc(Object *)
 */
static void dealloc(Object *self) {

  HeroImageList *this = (HeroImageList *) self;

  release(this->lock);
  release(this->images);

  super(Object, self, dealloc);
}

#pragma mark - HeroImageList

/**
 * @fn HeroImageList *HeroImageList::init(HeroImageList *self)
 * @memberof HeroImageList
 */
static HeroImageList *init(HeroImageList *self) {

  self = (HeroImageList *) super(Object, self, init);
  if (self) {
    self->lock = $(alloc(Lock), init);
    assert(self->lock);

    self->images = $(alloc(Array), init);
    assert(self->images);
  }

  return self;
}

/**
 * @fn void HeroImageList::add(HeroImageList *self, Image *image)
 * @memberof HeroImageList
 */
static void add(HeroImageList *self, Image *image) {

  synchronized(self->lock, {
    $(self->images, addObject, image);
  });
}

/**
 * @fn Array *HeroImageList::drain(HeroImageList *self)
 * @memberof HeroImageList
 */
static Array *drain(HeroImageList *self) {

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
 * @fn void HeroImageList::cancel(HeroImageList *self)
 * @memberof HeroImageList
 */
static void cancel(HeroImageList *self) {

  synchronized(self->lock, {
    self->cancelled = true;
  });
}

/**
 * @fn bool HeroImageList::isCancelled(const HeroImageList *self)
 * @memberof HeroImageList
 */
static bool isCancelled(const HeroImageList *self) {

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

  ((HeroImageListInterface *) clazz->interface)->init = init;
  ((HeroImageListInterface *) clazz->interface)->add = add;
  ((HeroImageListInterface *) clazz->interface)->drain = drain;
  ((HeroImageListInterface *) clazz->interface)->cancel = cancel;
  ((HeroImageListInterface *) clazz->interface)->isCancelled = isCancelled;
}

/**
 * @fn Class *HeroImageList::_HeroImageList(void)
 * @memberof HeroImageList
 */
Class *_HeroImageList(void) {
  static Class *clazz;
  static Once once;

  do_once(&once, {
    clazz = _initialize(&(const ClassDef) {
      .name = "HeroImageList",
      .superclass = _Object(),
      .instanceSize = sizeof(HeroImageList),
      .interfaceSize = sizeof(HeroImageListInterface),
      .initialize = initialize,
    });
  });

  return clazz;
}

#undef _Class
