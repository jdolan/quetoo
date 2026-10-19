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

#include "AiCollectionItemView.h"

#define _Class _AiCollectionItemView

#pragma mark - Object

/**
 * @see Object::dealloc(Object *)
 */
static void dealloc(Object *self) {

  AiCollectionItemView *this = (AiCollectionItemView *) self;

  release(this->skill);
  release(this->aggression);
  release(this->awareness);
  release(this->traits);

  super(Object, self, dealloc);
}

#pragma mark - AiCollectionItemView

/**
 * @return A trait bar from 0 to 1, labeled with the trait's name.
 */
static ProgressBar *trait(const char *name) {

  ProgressBar *bar = $(alloc(ProgressBar), initWithFrame, NULL);
  assert(bar);

  bar->min = 0.0;
  bar->max = 1.0;

  $(bar, setLabelFormat, name);

  return bar;
}

/**
 * @fn AiCollectionItemView *AiCollectionItemView::initWithFrame(AiCollectionItemView *self, const SDL_Rect *frame)
 * @memberof AiCollectionItemView
 */
static AiCollectionItemView *initWithFrame(AiCollectionItemView *self, const SDL_Rect *frame) {

  self = (AiCollectionItemView *) super(CollectionItemView, self, initWithFrame, frame);
  if (self) {
    self->traits = $(alloc(StackView), initWithFrame, NULL);
    assert(self->traits);

    $((View *) self->traits, addClassName, "aiItemTraits");

    self->skill = trait("Skill");
    self->aggression = trait("Aggression");
    self->awareness = trait("Awareness");

    $((View *) self->traits, addSubview, (View *) self->skill);
    $((View *) self->traits, addSubview, (View *) self->aggression);
    $((View *) self->traits, addSubview, (View *) self->awareness);

    $((View *) self, addSubview, (View *) self->traits);
  }

  return self;
}

/**
 * @fn void AiCollectionItemView::setAiItemInfo(AiCollectionItemView *self, const AiItemInfo *info)
 * @memberof AiCollectionItemView
 */
static void setAiItemInfo(AiCollectionItemView *self, const AiItemInfo *info) {

  CollectionItemView *item = (CollectionItemView *) self;

  $(item->text, setText, NULL);
  $(item->imageView, setImage, NULL);

  if (info) {
    $(item->text, setText, info->name);

    char model[MAX_QPATH], skin[MAX_QPATH];
    Str_Copy(model, info->skin, sizeof(model));

    char *slash = strchr(model, '/');
    Str_Copy(skin, slash ? slash + 1 : "default", sizeof(skin));
    if (slash) {
      *slash = '\0';
    }

    SDL_Surface *surface = cgi.LoadSurface(va("players/%s/%s_i", model, skin));
    if (surface) {
      $(item->imageView, setImageWithSurface, surface);
      SDL_DestroySurface(surface);
    } else {
      Cg_Warn("No icon for AI player %s, skin %s\n", info->name, info->skin);
    }

    $(self->skill, setValue, info->skill);
    $(self->aggression, setValue, info->aggression);
    $(self->awareness, setValue, info->awareness);
  }
}

#pragma mark - Class lifecycle

/**
 * @see Class::initialize(Class *)
 */
static void initialize(Class *clazz) {

  ((ObjectInterface *) clazz->interface)->dealloc = dealloc;

  ((AiCollectionItemViewInterface *) clazz->interface)->initWithFrame = initWithFrame;
  ((AiCollectionItemViewInterface *) clazz->interface)->setAiItemInfo = setAiItemInfo;
}

/**
 * @fn Class *AiCollectionItemView::_AiCollectionItemView(void)
 * @memberof AiCollectionItemView
 */
Class *_AiCollectionItemView(void) {
  static Class *clazz;
  static Once once;

  do_once(&once, {
    clazz = _initialize(&(const ClassDef) {
      .name = "AiCollectionItemView",
      .superclass = _CollectionItemView(),
      .instanceSize = sizeof(AiCollectionItemView),
      .interfaceSize = sizeof(AiCollectionItemViewInterface),
      .initialize = initialize,
    });
  });

  return clazz;
}

#undef _Class
