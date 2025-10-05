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

#include <assert.h>

#include "ui_local.h"
#include "client.h"

#include "EntityView.h"

#define _Class _EntityView

#pragma mark - Actions and Delegate callbacks

/**
 * @brief TextViewDelegate callback for editing an entity pair.
 */
static void didEndEditing(TextView *textView) {

  EntityView *self = textView->delegate.self;

  assert(self);
  assert(self->delegate.didEditEntity);

  const char *key = self->key->attributedText->string.chars ?: self->key->defaultText;
  const char *value = self->value->attributedText->string.chars ?: self->value->defaultText;

  self->delegate.didEditEntity(self, key, value);
}

#pragma mark - Object

/**
 * @fn void Object::dealloc(Object *self)
 * @memberof Object
 */
static void dealloc(Object *self) {

  EntityView *this = (EntityView *) self;

  release(this->key);
  release(this->value);

  super(Object, self, dealloc);
}

#pragma mark - View

/**
 * @fn void Viem::awakeWithDictionary(View *self, const Dictionary *dictionary)
 * @memberof View
 */
static void awakeWithDictionary(View *self, const Dictionary *dictionary) {

  super(View, self, awakeWithDictionary, dictionary);

  EntityView *this = (EntityView *) self;

  const Inlet inlets[] = MakeInlets(
    MakeInlet("key", InletTypeView, &this->key, NULL),
    MakeInlet("value", InletTypeView, &this->value, NULL)
  );

  $(self, bind, inlets, dictionary);
}

/**
 * @fn void View::render(View *self, Renderer *renderer)
 * @memberof View
 */
static void render(View *self, Renderer *renderer) {

  super(View, self, render, renderer);

  EntityView *this = (EntityView *) self;

  // if we are the first entity pair, draw the selection box
  if (this->entity && this->entity->prev == NULL) {

    const vec3_t origin = Cm_EntityValue(this->entity, "origin")->vec3;
    const box3_t bounds = Box3_FromCenterRadius(origin, 8.f);

    color_t c = color_white;
    const cm_entity_t *color = Cm_EntityValue(this->entity, "_color");
    if (color && color->parsed & ENTITY_VEC3) {
      c = Color3fv(color->vec3);
    }

    R_Draw3DBox(bounds, c, false);
  }
}

#pragma mark - EntityView

/**
 * @fn EntityView *EntityView::init(EntityView *self)
 * @memberof EntityView
 */
static EntityView *initWithFrame(EntityView *self, const SDL_Rect *frame) {

  self = (EntityView *) super(StackView, self, initWithFrame, frame);
  if (self) {

    self->key = $(alloc(TextView), initWithFrame, NULL);
    $((View *) self, addSubview, (View *) self->key);

    self->value = $(alloc(TextView), initWithFrame, NULL);
    $((View *) self, addSubview, (View *) self->value);

    $((View *) self, awakeWithResourceName, "ui/editor/EntityView.json");

    self->key->delegate.self = self;
    self->key->delegate.didEndEditing = didEndEditing;

    self->value->delegate.self = self;
    self->value->delegate.didEndEditing = didEndEditing;
  }

  return self;
}

/**
 * @fn void EntityView::setEntity(EntityView *self, cm_entity_t *entity)
 * @memberof EntityView
 */
static void setEntity(EntityView *self, cm_entity_t *entity) {

  self->entity = entity;

  if (entity) {
    $(self->key, setDefaultText, entity->key);
    $(self->value, setDefaultText, entity->string);
  } else {
    $(self->key, setDefaultText, NULL);
    $(self->value, setDefaultText, NULL);
  }
}

#pragma mark - Class lifecycle

/**
 * @see Class::initialize(Class *)
 */
static void initialize(Class *clazz) {

  ((ObjectInterface *) clazz->interface)->dealloc = dealloc;

  ((ViewInterface *) clazz->interface)->awakeWithDictionary = awakeWithDictionary;
  ((ViewInterface *) clazz->interface)->render = render;

  ((EntityViewInterface *) clazz->interface)->initWithFrame = initWithFrame;
  ((EntityViewInterface *) clazz->interface)->setEntity = setEntity;
}

/**
 * @fn Class *EntityView::_EntityView(void)
 * @memberof EntityView
 */
Class *_EntityView(void) {
  static Class *clazz;
  static Once once;

  do_once(&once, {
    clazz = _initialize(&(const ClassDef) {
      .name = "EntityView",
      .superclass = _StackView(),
      .instanceSize = sizeof(EntityView),
      .interfaceOffset = offsetof(EntityView, interface),
      .interfaceSize = sizeof(EntityViewInterface),
      .initialize = initialize,
    });
  });

  return clazz;
}

#undef _Class
