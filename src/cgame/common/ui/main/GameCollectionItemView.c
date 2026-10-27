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

#include "GameCollectionItemView.h"

#define _Class _GameCollectionItemView

#pragma mark - Object

/**
 * @see Object::dealloc(Object *)
 */
static void dealloc(Object *self) {

  GameCollectionItemView *this = (GameCollectionItemView *) self;

  release(this->description);

  super(Object, self, dealloc);
}

#pragma mark - GameCollectionItemView

/**
 * @fn GameCollectionItemView *GameCollectionItemView::initWithFrame(GameCollectionItemView *self, const SDL_Rect *frame)
 * @memberof GameCollectionItemView
 */
static GameCollectionItemView *initWithFrame(GameCollectionItemView *self, const SDL_Rect *frame) {

  self = (GameCollectionItemView *) super(CollectionItemView, self, initWithFrame, frame);
  if (self) {
    self->description = $(alloc(Text), initWithText, NULL, NULL);
    assert(self->description);

    self->description->lineWrap = true;

    $((View *) self->description, addClassName, "gameItemDescription");
    $((View *) self, addSubview, (View *) self->description);
  }

  return self;
}

/**
 * @fn void GameCollectionItemView::setGameItemInfo(GameCollectionItemView *self, const GameItemInfo *info)
 * @memberof GameCollectionItemView
 */
static void setGameItemInfo(GameCollectionItemView *self, const GameItemInfo *info) {

  CollectionItemView *item = (CollectionItemView *) self;

  $(item->text, setText, NULL);
  $(item->imageView, setImage, NULL);
  $(self->description, setText, NULL);

  if (info) {
    $(item->text, setText, info->info.name);
    $(self->description, setText, info->info.description);

    if (*info->info.image) {
      size_t length;
      void *bytes = SDL_LoadFile(va("%s/%s", info->dir, info->info.image), &length);
      if (bytes) {
        SDL_Surface *surface = cgi.LoadSurfaceFromData(bytes, length);
        if (surface) {
          $(item->imageView, setImageWithSurface, surface);
          SDL_DestroySurface(surface);
        } else {
          Cg_Warn("%s: %s is not a PNG or JPG\n", info->game, info->info.image);
        }
        SDL_free(bytes);
      } else {
        Cg_Warn("%s: no image %s in %s\n", info->game, info->info.image, info->dir);
      }
    }
  }
}

#pragma mark - Class lifecycle

/**
 * @see Class::initialize(Class *)
 */
static void initialize(Class *clazz) {

  ((ObjectInterface *) clazz->interface)->dealloc = dealloc;

  ((GameCollectionItemViewInterface *) clazz->interface)->initWithFrame = initWithFrame;
  ((GameCollectionItemViewInterface *) clazz->interface)->setGameItemInfo = setGameItemInfo;
}

/**
 * @fn Class *GameCollectionItemView::_GameCollectionItemView(void)
 * @memberof GameCollectionItemView
 */
Class *_GameCollectionItemView(void) {
  static Class *clazz;
  static Once once;

  do_once(&once, {
    clazz = _initialize(&(const ClassDef) {
      .name = "GameCollectionItemView",
      .superclass = _CollectionItemView(),
      .instanceSize = sizeof(GameCollectionItemView),
      .interfaceSize = sizeof(GameCollectionItemViewInterface),
      .initialize = initialize,
    });
  });

  return clazz;
}

#undef _Class
