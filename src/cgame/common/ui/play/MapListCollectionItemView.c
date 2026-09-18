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

#include "MapListCollectionItemView.h"

#define _Class _MapListCollectionItemView

/**
 * @brief Badge images by game, shared by every item for the life of the process: the browser
 * reloads all of its items each time the loader discovers another map.
 */
static Dictionary *badges;

/**
 * @brief Returns the badge for the specified game, or `NULL` if no `pics/game_<game>` exists.
 */
static Image *badgeForGame(const char *game) {

  Image *badge = $(badges, objectForKeyPath, game);
  if (badge == NULL) {
    badge = Cg_LoadImage(va("pics/game_%s", game));
    if (badge) {
      $(badges, setObjectForKeyPath, badge, game);
      release(badge);
    }
  }

  return badge;
}

#pragma mark - Object

/**
 * @see Object::dealloc(Object *)
 */
static void dealloc(Object *self) {

  MapListCollectionItemView *this = (MapListCollectionItemView *) self;

  release(this->games);

  super(Object, self, dealloc);
}

#pragma mark - MapListCollectionItemView

/**
 * @fn MapListCollectionItemView *MapListCollectionItemView::initWithFrame(MapListCollectionItemView *self, const SDL_Rect *frame)
 *
 * @memberof MapListCollectionItemView
 */
static MapListCollectionItemView *initWithFrame(MapListCollectionItemView *self, const SDL_Rect *frame) {

  self = (MapListCollectionItemView *) super(CollectionItemView, self, initWithFrame, frame);
  if (self) {
    self->games = $(alloc(StackView), initWithFrame, NULL);
    assert(self->games);

    $((View *) self->games, addClassName, "mapItemGames");
  }

  return self;
}

/**
 * @fn void MapListCollectionItemView::setMapListItemInfo(MapListCollectionItemView *self, const MapListItemInfo *info);
 *
 * @memberof MapListCollectionItemView
 */
static void setMapListItemInfo(MapListCollectionItemView *self, const MapListItemInfo *info) {

  CollectionItemView *item = (CollectionItemView *) self;

  $(item->text, setText, NULL);
  $(item->imageView, setImage, NULL);

  $((View *) self->games, removeAllSubviews);
  $((View *) self->games, removeFromSuperview);

  if (info) {
    $(item->text, setText, info->message);
    $(item->imageView, setImageWithSurface, info->mapshot);

    char games[sizeof(info->games)];
    q_strlcpy(games, info->games, sizeof(games));

    for (char *game = strtok(games, " \t\n"); game; game = strtok(NULL, " \t\n")) {

      if (q_strcmp(game, "dm") == 0) {
        continue;
      }

      Image *badge = badgeForGame(game);
      if (badge) {
        ImageView *imageView = $(alloc(ImageView), initWithImage, badge);
        assert(imageView);

        $((View *) self->games, addSubview, (View *) imageView);
        release(imageView);
      }
    }

    if (((View *) self->games)->subviews->count) {
      $((View *) self, addSubview, (View *) self->games);
    }
  }
}

#pragma mark - Class lifecycle

/**
 * @see Class::initialize(Class *)
 */
static void initialize(Class *clazz) {

  ((ObjectInterface *) clazz->interface)->dealloc = dealloc;

  ((MapListCollectionItemViewInterface *) clazz->interface)->initWithFrame = initWithFrame;
  ((MapListCollectionItemViewInterface *) clazz->interface)->setMapListItemInfo = setMapListItemInfo;
}

/**
 * @fn Class *MapListCollectionItemView::_MapListCollectionItemView(void)
 * @memberof MapListCollectionItemView
 */
Class *_MapListCollectionItemView(void) {
  static Class *clazz;
  static Once once;

  do_once(&once, {
    badges = $(alloc(Dictionary), init);
    assert(badges);

    clazz = _initialize(&(const ClassDef) {
      .name = "MapListCollectionItemView",
      .superclass = _CollectionItemView(),
      .instanceSize = sizeof(MapListCollectionItemView),
      .interfaceSize = sizeof(MapListCollectionItemViewInterface),
      .initialize = initialize,
    });
  });

  return clazz;
}

#undef _Class

