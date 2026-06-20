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

#include "cg_local.h"

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

  cm_entity_t *e = self->pair ?: cgi.AllocEntity();

  const char *key = self->key->attributedText->chars;
  const char *value = self->value->attributedText->chars;

  q_strlcpy(e->key, key ?: "", sizeof(e->key));
  q_strlcpy(e->string, value ?: "", sizeof(e->string));

  cgi.ParseEntity(e);

  self->delegate.didEditEntity(self, e);

  if (e != self->pair) {
    cgi.FreeEntity(e);
  }
}

#pragma mark - Object

/**
 * @fn void Object::dealloc(Object *self)
 * @memberof Object
 */
static void dealloc(Object *self) {

  EntityView *this = (EntityView *) self;

  memset(&this->delegate, 0, sizeof(this->delegate));

  this->key->delegate.didEndEditing = NULL;
  this->value->delegate.didEndEditing = NULL;

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
  return (View *) $((EntityView *) self, initWithEntity, NULL, NULL);
}

#pragma mark - EntityView

/**
 * @fn EntityView *EntityView::initWithEntity(EntityView *self, cg_editor_entity_t *edit, cm_entity_t *pair)
 * @memberof EntityView
 */
static EntityView *initWithEntity(EntityView *self, cg_editor_entity_t *edit, cm_entity_t *pair) {

  self = (EntityView *) super(StackView, self, initWithFrame, NULL);
  if (self) {

    $((View *) self, awakeWithResourceName, "ui/editor/EntityView.json");

    $((View *) self, addSubview, (View *) self->key);
    $((View *) self, addSubview, (View *) self->value);

    self->key->delegate.self = self;
    self->key->delegate.didEndEditing = didEndEditing;

    self->value->delegate.self = self;
    self->value->delegate.didEndEditing = didEndEditing;

    $(self, setEntity, edit, pair);
  }

  return self;
}

/**
 * @fn void EntityView::setEntity(EntityView *self, cg_editor_entity_t *edit, cm_entity_t *pair)
 * @memberof EntityView
 */
static void setEntity(EntityView *self, cg_editor_entity_t *edit, cm_entity_t *pair) {

  self->edit = edit;
  self->pair = pair;

  self->key->control.state = ControlStateDefault;
  self->value->control.state = ControlStateDefault;

  if (pair) {
    $(self->key, setAttributedText, pair->key);
    if (pair->parsed & ENTITY_VEC4) {
      $(self->value, setAttributedText, va("%g %g %g %g", pair->vec4.x, pair->vec4.y, pair->vec4.z, pair->vec4.w));
    } else if (pair->parsed & ENTITY_VEC3) {
      $(self->value, setAttributedText, va("%g %g %g", pair->vec3.x, pair->vec3.y, pair->vec3.z));
    } else if (pair->parsed & ENTITY_VEC2) {
      $(self->value, setAttributedText, va("%g %g", pair->vec2.x, pair->vec2.y));
    } else if (pair->parsed & ENTITY_FLOAT) {
      $(self->value, setAttributedText, va("%g", pair->value));
    } else {
      $(self->value, setAttributedText, pair->string);
    }

    if (!q_strcmp(pair->key, "classname")
        && !q_strcmp(pair->string, "worldspawn")) {
      self->key->control.state |= ControlStateDisabled;
      self->value->control.state |= ControlStateDisabled;
    }
  } else {
    $(self->key, setAttributedText, "");
    $(self->value, setAttributedText, "");
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
