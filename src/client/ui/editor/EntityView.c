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

#pragma mark - Delegates

/**
 * @brief TextViewDelegate callback for editing an entity pair.
 */
static void didEndEditing(TextView *textView) {

  EntityView *self = textView->delegate.self;

  assert(self);
  assert(self->delegate.didEditEntity);

  const char *key = self->key->attributedText->string.chars ?: self->key->defaultText;
  const char *value = self->value->attributedText->string.chars ?: self->value->defaultText;

  if (key && value) {
    self->delegate.didEditEntity(self, key, value);
  }
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
 * @fn View *View::init(View *self)
 * @memberof View
 */
static View *init(View *self) {
  return (View *) $((EntityView *) self, initWithEntity, NULL);
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

    // except worldspawn, there's no point in drawing that selection box
    const char *classname = Cm_EntityValue(this->entity, "classname")->string;
    if (g_strcmp0(classname, "worldspawn")) {

      const vec3_t origin = Cm_EntityValue(this->entity, "origin")->vec3;
      const box3_t bounds = Box3_FromCenterRadius(origin, 8.f);

      color_t c = color_white;
      const cm_entity_t *color = Cm_EntityValue(this->entity, "color");
      if (color && color->parsed & ENTITY_COLOR) {
        c = Color3fv(color->vec3);
      }

      R_Draw3DBox(bounds, c, false);
    }
  }
}

#pragma mark - EntityView

/**
 * @fn EntityView *EntityView::initWithEntity(EntityView *self, cm_entity_t *entity)
 * @memberof EntityView
 */
static EntityView *initWithEntity(EntityView *self, cm_entity_t *entity) {

  self = (EntityView *) super(StackView, self, initWithFrame, NULL);
  if (self) {

    $((View *) self, awakeWithResourceName, "ui/editor/EntityView.json");

    $((View *) self, addSubview, (View *) self->key);
    $((View *) self, addSubview, (View *) self->value);

    self->key->delegate.self = self;
    self->key->delegate.didEndEditing = didEndEditing;

    self->value->delegate.self = self;
    self->value->delegate.didEndEditing = didEndEditing;

    $(self, setEntity, entity);
  }

  return self;
}

/**
 * @fn void EntityView::setEntity(EntityView *self, cm_entity_t *entity)
 * @memberof EntityView
 */
static void setEntity(EntityView *self, cm_entity_t *entity) {

  self->entity = entity;

  $(self->key, setAttributedText, NULL);
  $(self->value, setAttributedText, NULL);

  if (entity) {

    $(self->key, setAttributedText, entity->key);

    if (entity->parsed & ENTITY_VEC4) {
      const vec4_t v = entity->vec4;
      $(self->value, setAttributedText, va("%.2f %.2f %.2f %.2f", v.x, v.y, v.z, v.w));
    } else if (entity->parsed & ENTITY_VEC3) {
      const vec3_t v = entity->vec3;
      $(self->value, setAttributedText, va("%.2f %.2f %.2f", v.x, v.y, v.z));
    } else if (entity->parsed & ENTITY_VEC2) {
      const vec2_t v = entity->vec2;
      $(self->value, setAttributedText, va("%.2f %.2f", v.x, v.y));
    } else if (entity->parsed & ENTITY_FLOAT) {
      $(self->value, setAttributedText, va("%.2f", entity->value));
    } else if (entity->parsed & ENTITY_INTEGER) {
      $(self->value, setAttributedText, va("%d", entity->integer));
    } else {
      $(self->value, setAttributedText, entity->nullable_string);
    }
  }
}

#pragma mark - Class lifecycle

/**
 * @see Class::initialize(Class *)
 */
static void initialize(Class *clazz) {

  ((ObjectInterface *) clazz->interface)->dealloc = dealloc;

  ((ViewInterface *) clazz->interface)->awakeWithDictionary = awakeWithDictionary;
  ((ViewInterface *) clazz->interface)->init = init;
  ((ViewInterface *) clazz->interface)->render = render;

  ((EntityViewInterface *) clazz->interface)->initWithEntity = initWithEntity;
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
