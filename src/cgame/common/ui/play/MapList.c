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

#include "cg_local.h"

#include "MapList.h"

#define _Class _MapList

/**
 * @brief PointerArray destroy function for MapListItemInfo.
 */
static void freeMapListItemInfo(void *p) {
  MapListItemInfo *info = p;
  if (info->mapshot) {
    SDL_DestroySurface(info->mapshot);
  }
  free(info);
}

/**
 * @brief Comparator for map sorting.
 */
static Order sortMaps(const ident a, const ident b) {

  const MapListItemInfo *c = a;
  const MapListItemInfo *d = b;

  const char *e = !q_strncmp(c->message, "The ", 4) ? c->message + 4 : c->message;
  const char *f = !q_strncmp(d->message, "The ", 4) ? d->message + 4 : d->message;

  return q_strcasecmp(e, f) < 0 ? OrderAscending : OrderDescending;
}

#pragma mark - Object

/**
 * @see Object::dealloc(Object *)
 */
static void dealloc(Object *self) {

  MapList *this = (MapList *) self;

  release(this->lock);
  release(this->maps);

  super(Object, self, dealloc);
}

#pragma mark - MapList

/**
 * @fn MapList *MapList::init(MapList *self)
 * @memberof MapList
 */
static MapList *init(MapList *self) {

  self = (MapList *) super(Object, self, init);
  if (self) {
    self->lock = $(alloc(Lock), init);
    assert(self->lock);

    self->maps = $(alloc(PointerArray), initWithDestroy, freeMapListItemInfo);
    assert(self->maps);
  }

  return self;
}

/**
 * @fn size_t MapList::count(const MapList *self)
 * @memberof MapList
 */
static size_t count(const MapList *self) {

  size_t count;
  synchronized(self->lock, {
    count = self->maps->count;
  });

  return count;
}

/**
 * @fn MapListItemInfo *MapList::get(const MapList *self, size_t index)
 * @memberof MapList
 */
static MapListItemInfo *get(const MapList *self, size_t index) {

  MapListItemInfo *info;
  synchronized(self->lock, {
    info = $(self->maps, get, index);
  });

  return info;
}

/**
 * @fn void MapList::add(MapList *self, MapListItemInfo *info)
 * @memberof MapList
 */
static void add(MapList *self, MapListItemInfo *info) {

  synchronized(self->lock, {

    // the duplicate check must happen under the same lock as the add: a MapList is only ever
    // populated by one loader today, but nothing here should rely on that remaining true
    bool duplicate = false;
    for (size_t i = 0; i < self->maps->count; i++) {
      const MapListItemInfo *existing = $(self->maps, get, i);
      if (q_strcmp(existing->mapname, info->mapname) == 0) {
        duplicate = true;
        break;
      }
    }

    if (duplicate) {
      freeMapListItemInfo(info);
    } else {
      $(self->maps, add, info);
      $(self->maps, sort, sortMaps);
    }
  });
}

#pragma mark - Class lifecycle

/**
 * @see Class::initialize(Class *)
 */
static void initialize(Class *clazz) {

  ((ObjectInterface *) clazz->interface)->dealloc = dealloc;

  ((MapListInterface *) clazz->interface)->init = init;
  ((MapListInterface *) clazz->interface)->count = count;
  ((MapListInterface *) clazz->interface)->get = get;
  ((MapListInterface *) clazz->interface)->add = add;
}

/**
 * @fn Class *MapList::_MapList(void)
 * @memberof MapList
 */
Class *_MapList(void) {
  static Class *clazz;
  static Once once;

  do_once(&once, {
    clazz = _initialize(&(const ClassDef) {
      .name = "MapList",
      .superclass = _Object(),
      .instanceSize = sizeof(MapList),
      .interfaceSize = sizeof(MapListInterface),
      .initialize = initialize,
    });
  });

  return clazz;
}

#undef _Class
