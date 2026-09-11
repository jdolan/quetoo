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

#include <ObjectivelyMVC/ImageView.h>

#include "cg_local.h"

#include "IntermissionView.h"

#define _Class _IntermissionView

#pragma mark - Object

/**
 * @see Object::dealloc(Object *)
 */
static void dealloc(Object *self) {

  IntermissionView *this = (IntermissionView *) self;

  release(this->maps);
  release(this->countdown);

  super(Object, self, dealloc);
}

#pragma mark - Tiles

/**
 * @brief The map's thumbnail, or a placeholder for a map this client does not have.
 * @remarks Resolving a mapshot enumerates the filesystem, so this runs only when the
 * candidates change, never per frame.
 */
static Image *thumbnail(const char *map) {

  List *mapshots = cgi.Mapshots(map);

  // the first, deterministically: a different one each frame would flicker
  const char *path = mapshots->count ? mapshots->head->element : NULL;

  Image *image = NULL;

  if (path) {
    SDL_Surface *surface = cgi.LoadSurface(path);
    if (surface) {
      image = $$(Image, imageWithSurface, surface);
      SDL_DestroySurface(surface);
    }
  }

  release(mapshots);

  if (image == NULL) {
    image = Cg_LoadImage(va("ui/backgrounds/%u", (uint32_t) (q_strlen(map) % 6)));
  }

  return image;
}

/**
 * @brief Appends the tile for the candidate at `index`.
 */
static void addMap(IntermissionView *self, int32_t index) {

  const cg_next_map_state_t *next_map = &cg_state.next_map;

  StackView *tile = $(alloc(StackView), initWithFrame, NULL);
  assert(tile);

  $((View *) tile, addClassName, "map");

  ImageView *mapshot = $(alloc(ImageView), initWithFrame, NULL);
  assert(mapshot);

  Image *image = thumbnail(next_map->maps[index]);
  $(mapshot, setImage, image);
  release(image);

  $((View *) mapshot, addClassName, "mapshot");
  $((View *) tile, addSubview, (View *) mapshot);
  release(mapshot);

  // the key that picks it, which is the only way to pick one: the HUD layer is drawn
  // beneath the menus and never sees the mouse
  Text *name = $(alloc(Text), initWithText,
                 next_map->voting ? va("%d  %s", index + 1, next_map->maps[index])
                                  : next_map->maps[index], NULL);
  assert(name);

  $((View *) name, addClassName, "name");
  $((View *) tile, addSubview, (View *) name);
  release(name);

  if (next_map->voting) {
    self->votes[index] = $(alloc(Text), initWithText, "", NULL);
    assert(self->votes[index]);

    $((View *) self->votes[index], addClassName, "votes");
    $((View *) tile, addSubview, (View *) self->votes[index]);
    release(self->votes[index]);
  }

  $((View *) self->maps, addSubview, (View *) tile);
  release(tile);
}

#pragma mark - IntermissionView

/**
 * @fn void IntermissionView::rebuild(IntermissionView *self)
 * @memberof IntermissionView
 */
static void rebuild(IntermissionView *self) {

  $((View *) self->maps, removeAllSubviews);

  memset(self->votes, 0, sizeof(self->votes));

  for (int32_t i = 0; i < cg_state.next_map.num_maps; i++) {
    addMap(self, i);
  }
}

#pragma mark - View

/**
 * @see View::init(View *)
 */
static View *init(View *self) {

  self = super(View, self, init);
  if (self) {
    IntermissionView *this = (IntermissionView *) self;

    this->maps = $(alloc(StackView), initWithFrame, NULL);
    assert(this->maps);

    $((View *) this->maps, addClassName, "maps");
    $(self, addSubview, (View *) this->maps);

    this->countdown = $(alloc(Text), initWithText, "", NULL);
    assert(this->countdown);

    $((View *) this->countdown, addClassName, "countdown");
    $(self, addSubview, (View *) this->countdown);
  }

  return self;
}

/**
 * @see View::updateBindings(View *, ident)
 */
static void updateBindings(View *self, ident data) {

  IntermissionView *this = (IntermissionView *) self;

  const cg_next_map_state_t *next_map = &cg_state.next_map;

  if (data) {

    // rebuilt before the recursion, so that new tiles take this frame too
    if (next_map->generation != this->generation) {
      this->generation = next_map->generation;

      $(this, rebuild);
    }

    for (int32_t i = 0; i < next_map->num_maps; i++) {
      if (this->votes[i]) {
        $(this->votes[i], setText, va("%d", next_map->votes[i]));
      }
    }

    // the server publishes the intermission's clock here once the match clock stops
    const char *time = cgi.ConfigString(CS_TIME);
    if (!q_strncmp(time, "^7", 2)) {
      time += 2;
    }

    $(this->countdown, setText, time);
  }

  super(View, self, updateBindings, data);
}

#pragma mark - Class lifecycle

/**
 * @see Class::initialize(Class *)
 */
static void initialize(Class *clazz) {

  ((ObjectInterface *) clazz->interface)->dealloc = dealloc;

  ((ViewInterface *) clazz->interface)->init = init;
  ((ViewInterface *) clazz->interface)->updateBindings = updateBindings;

  ((IntermissionViewInterface *) clazz->interface)->rebuild = rebuild;
}

/**
 * @fn Class *IntermissionView::_IntermissionView(void)
 * @memberof IntermissionView
 */
Class *_IntermissionView(void) {
  static Class *clazz;
  static Once once;

  do_once(&once, {
    clazz = _initialize(&(const ClassDef) {
      .name = "IntermissionView",
      .superclass = _View(),
      .instanceSize = sizeof(IntermissionView),
      .interfaceSize = sizeof(IntermissionViewInterface),
      .initialize = initialize,
    });
  });

  return clazz;
}

#undef _Class
