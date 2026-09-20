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

#include "DemoList.h"

#define _Class _DemoList

/**
 * @brief PointerArray destroy function for DemoListItemInfo.
 */
static void freeDemoListItemInfo(void *p) {
  DemoListItemInfo *info = p;
  if (info->mapshot) {
    SDL_DestroySurface(info->mapshot);
  }
  free(info);
}

/**
 * @brief Case-insensitive substring test.
 */
static bool containsCaseInsensitive(const char *haystack, const char *needle) {

  const size_t needleLen = strlen(needle);
  if (!needleLen) {
    return true;
  }

  for (const char *h = haystack; *h; h++) {
    if (!q_strncasecmp(h, needle, needleLen)) {
      return true;
    }
  }

  return false;
}

/**
 * @brief Comparator for demo sorting: most recently recorded first.
 */
static Order sortDemos(const ident a, const ident b) {

  const DemoListItemInfo *c = a;
  const DemoListItemInfo *d = b;

  return c->modified > d->modified ? OrderAscending : OrderDescending;
}

/**
 * @brief Rebuilds `filtered` from `demos`, per `filter`. Caller holds `lock`.
 */
static void applyFilter(DemoList *self) {

  $(self->filtered, removeAll);

  for (size_t i = 0; i < self->demos->count; i++) {
    DemoListItemInfo *info = $(self->demos, get, i);

    if (self->filter && *self->filter &&
        !containsCaseInsensitive(DemoListItemName(info), self->filter)) {
      continue;
    }

    $(self->filtered, add, info);
  }
}

#pragma mark - Object

/**
 * @see Object::dealloc(Object *)
 */
static void dealloc(Object *self) {

  DemoList *this = (DemoList *) self;

  release(this->lock);
  release(this->demos);
  release(this->filtered);
  free(this->filter);

  super(Object, self, dealloc);
}

#pragma mark - DemoList

/**
 * @fn DemoList *DemoList::init(DemoList *self)
 * @memberof DemoList
 */
static DemoList *init(DemoList *self) {

  self = (DemoList *) super(Object, self, init);
  if (self) {
    self->lock = $(alloc(Lock), init);
    assert(self->lock);

    self->demos = $(alloc(PointerArray), initWithDestroy, freeDemoListItemInfo);
    assert(self->demos);

    self->filtered = $(alloc(PointerArray), init);
    assert(self->filtered);
  }

  return self;
}

/**
 * @fn size_t DemoList::count(const DemoList *self)
 * @memberof DemoList
 */
static size_t count(const DemoList *self) {

  size_t count;
  synchronized(self->lock, {
    count = self->filtered->count;
  });

  return count;
}

/**
 * @fn DemoListItemInfo *DemoList::get(const DemoList *self, size_t index)
 * @memberof DemoList
 */
static DemoListItemInfo *get(const DemoList *self, size_t index) {

  DemoListItemInfo *info;
  synchronized(self->lock, {
    info = $(self->filtered, get, index);
  });

  return info;
}

/**
 * @fn void DemoList::add(DemoList *self, DemoListItemInfo *info)
 * @memberof DemoList
 */
static void add(DemoList *self, DemoListItemInfo *info) {

  synchronized(self->lock, {

    // the duplicate check must happen under the same lock as the add: reloadDemos can be
    // triggered concurrently (initWithFrame and viewWillAppear both call it), and two racing
    // enumerations could otherwise both observe the path as absent before either adds it
    bool duplicate = false;
    for (size_t i = 0; i < self->demos->count; i++) {
      const DemoListItemInfo *existing = $(self->demos, get, i);
      if (q_strcmp(existing->filename, info->filename) == 0) {
        duplicate = true;
        break;
      }
    }

    if (duplicate) {
      freeDemoListItemInfo(info);
    } else {
      $(self->demos, add, info);
      $(self->demos, sort, sortDemos);
    }

    applyFilter(self);
  });
}

/**
 * @fn void DemoList::remove(DemoList *self, const char *filename)
 * @memberof DemoList
 */
static void _remove(DemoList *self, const char *filename) {

  synchronized(self->lock, {

    for (size_t i = 0; i < self->demos->count; i++) {
      const DemoListItemInfo *info = $(self->demos, get, i);
      if (q_strcmp(info->filename, filename) == 0) {
        $(self->demos, removeAt, i);
        break;
      }
    }

    applyFilter(self);
  });
}

/**
 * @fn void DemoList::setFilter(DemoList *self, const char *filter)
 * @memberof DemoList
 */
static void setFilter(DemoList *self, const char *filter) {

  synchronized(self->lock, {
    free(self->filter);
    self->filter = filter && *filter ? q_strdup(filter) : NULL;

    applyFilter(self);
  });
}

#pragma mark - Class lifecycle

/**
 * @see Class::initialize(Class *)
 */
static void initialize(Class *clazz) {

  ((ObjectInterface *) clazz->interface)->dealloc = dealloc;

  ((DemoListInterface *) clazz->interface)->init = init;
  ((DemoListInterface *) clazz->interface)->count = count;
  ((DemoListInterface *) clazz->interface)->get = get;
  ((DemoListInterface *) clazz->interface)->add = add;
  ((DemoListInterface *) clazz->interface)->remove = _remove;
  ((DemoListInterface *) clazz->interface)->setFilter = setFilter;
}

/**
 * @fn Class *DemoList::_DemoList(void)
 * @memberof DemoList
 */
Class *_DemoList(void) {
  static Class *clazz;
  static Once once;

  do_once(&once, {
    clazz = _initialize(&(const ClassDef) {
      .name = "DemoList",
      .superclass = _Object(),
      .instanceSize = sizeof(DemoList),
      .interfaceSize = sizeof(DemoListInterface),
      .initialize = initialize,
    });
  });

  return clazz;
}

#undef _Class
