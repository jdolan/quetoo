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

/**
 * @brief Value-field validation feedback colors. The "valid" background matches the
 * base `TextView` rule in common.css (#dedede10); "invalid" is a translucent maroon
 * tint, applied when a validated value does not resolve to an asset on disk.
 */
#define ENTITY_FIELD_BG_VALID   ((SDL_Color) { 0xde, 0xde, 0xde, 0x10 })
#define ENTITY_FIELD_BG_INVALID ((SDL_Color) { 0x80, 0x30, 0x30, 0x66 })

/**
 * @return This row's key TextView (the inherited KeyValueView column 0).
 */
static TextView *keyTextView(const EntityView *self) {
  return (TextView *) ((const KeyValueView *) self)->key;
}

/**
 * @return This row's value TextView (the inherited KeyValueView column 1).
 */
static TextView *valueTextView(const EntityView *self) {
  return (TextView *) ((const KeyValueView *) self)->value;
}

/**
 * @brief Tints the value field maroon when validation is enabled and the value does
 * not resolve to an asset (`validatePrefix` + value in `validateContext`). Empty
 * value or disabled validation leaves the field neutral. Purely visual.
 */
static void validateValue(EntityView *self) {

  if (*self->validatePrefix == '\0') {
    return;
  }

  TextView *value = valueTextView(self);
  const char *text = value->attributedText ? value->attributedText->chars : "";

  bool ok = true;
  if (*text) {
    ok = cgi.AssetExists(va("%s%s", self->validatePrefix, text), self->validateContext);
  }

  ((View *) value)->backgroundColor = ok ? ENTITY_FIELD_BG_VALID : ENTITY_FIELD_BG_INVALID;
}

#pragma mark - Delegates

/**
 * @brief TextViewDelegate (didEdit): validate the value live as the user types.
 */
static void didEditValue(TextView *textView) {
  validateValue((EntityView *) textView->delegate.self);
}

/**
 * @brief TextViewDelegate callback for editing an entity pair.
 */
static void didEndEditing(TextView *textView) {

  EntityView *self = textView->delegate.self;

  assert(self);
  assert(self->delegate.didEditEntity);

  cm_entity_t *e = self->pair ?: cgi.AllocEntity();

  const char *key = keyTextView(self)->attributedText->chars;
  const char *value = valueTextView(self)->attributedText->chars;

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

  if (keyTextView(this)) {
    keyTextView(this)->delegate.didEndEditing = NULL;
  }
  if (valueTextView(this)) {
    valueTextView(this)->delegate.didEndEditing = NULL;
  }

  // The key/value TextViews are owned by the superclass as its columns and torn
  // down with the view tree; nothing to release here.

  super(Object, self, dealloc);
}

#pragma mark - View

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

  self = (EntityView *) super(KeyValueView, self, initWithFrame, NULL);
  if (self) {

    // The JSON's "key" and "value" become this KeyValueView's columns; the
    // superclass parents and width-clamps them.
    $((View *) self, awakeWithResourceName, "ui/editor/EntityView.json");

    TextView *key = keyTextView(self);
    TextView *value = valueTextView(self);

    assert(key && value);

    key->delegate.self = self;
    key->delegate.didEndEditing = didEndEditing;

    value->delegate.self = self;
    value->delegate.didEndEditing = didEndEditing;
    value->delegate.didEdit = didEditValue;

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

  TextView *key = keyTextView(self);
  TextView *value = valueTextView(self);

  key->control.state = ControlStateDefault;
  value->control.state = ControlStateDefault;

  if (pair) {
    $(key, setAttributedText, pair->key);
    if (pair->parsed & ENTITY_VEC4) {
      $(value, setAttributedText, va("%g %g %g %g", pair->vec4.x, pair->vec4.y, pair->vec4.z, pair->vec4.w));
    } else if (pair->parsed & ENTITY_VEC3) {
      $(value, setAttributedText, va("%g %g %g", pair->vec3.x, pair->vec3.y, pair->vec3.z));
    } else if (pair->parsed & ENTITY_VEC2) {
      $(value, setAttributedText, va("%g %g", pair->vec2.x, pair->vec2.y));
    } else if (pair->parsed & ENTITY_FLOAT) {
      $(value, setAttributedText, va("%g", pair->value));
    } else {
      $(value, setAttributedText, pair->string);
    }

    if (!q_strcmp(pair->key, "classname")
        && !q_strcmp(pair->string, "worldspawn")) {
      key->control.state |= ControlStateDisabled;
      value->control.state |= ControlStateDisabled;
    }
  } else {
    $(key, setAttributedText, "");
    $(value, setAttributedText, "");
  }

  validateValue(self);
}

/**
 * @fn void EntityView::setTextureValidation(EntityView *self, const char *prefix, cm_asset_context_t context)
 * @memberof EntityView
 */
static void setTextureValidation(EntityView *self, const char *prefix, cm_asset_context_t context) {

  q_strlcpy(self->validatePrefix, prefix ?: "", sizeof(self->validatePrefix));
  self->validateContext = context;

  validateValue(self);
}

#pragma mark - Class lifecycle

/**
 * @see Class::initialize(Class *)
 */
static void initialize(Class *clazz) {

  ((ObjectInterface *) clazz->interface)->dealloc = dealloc;

  ((ViewInterface *) clazz->interface)->init = init;

  ((EntityViewInterface *) clazz->interface)->initWithEntity = initWithEntity;
  ((EntityViewInterface *) clazz->interface)->setEntity = setEntity;
  ((EntityViewInterface *) clazz->interface)->setTextureValidation = setTextureValidation;
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
      .superclass = _KeyValueView(),
      .instanceSize = sizeof(EntityView),
      .interfaceOffset = offsetof(EntityView, interface),
      .interfaceSize = sizeof(EntityViewInterface),
      .initialize = initialize,
    });
  });

  return clazz;
}

#undef _Class
