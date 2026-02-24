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

  cm_entity_t *e = self->entity.def ?: Cm_AllocEntity();

  const char *key = self->key->attributedText->string.chars;
  const char *value = self->value->attributedText->string.chars;

  if (g_strcmp0(e->key, key) || g_strcmp0(e->string, value)) {

    g_strlcpy(e->key, key, sizeof(e->key));
    g_strlcpy(e->string, value, sizeof(e->string));

    Cm_ParseEntity(e);

    self->delegate.didEditEntity(self, e);

    if (e != self->entity.def) {
      Cm_FreeEntity(e);
    }
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
  return (View *) $((EntityView *) self, initWithEntity, &(EditorEntity) {
    .number = -1,
    .ent = NULL,
    .def = NULL
  });
}

/**
 * @fn void View::render(View *self, Renderer *renderer)
 * @memberof View
 */
static void render(View *self, Renderer *renderer) {

  super(View, self, render, renderer);

  EntityView *this = (EntityView *) self;

  const cl_entity_t *ent = this->entity.ent;
  const cm_entity_t *def = this->entity.def;

  // if we are the classname entity pair, draw the selection box
  if (def && !g_strcmp0(def->key, "classname")) {

    // except worldspawn, there's no point in drawing that selection box
    if (g_strcmp0(def->string, "worldspawn")) {
      vec3_t points[2] = { ent->origin };

      points[1] = Vec3_Fmaf(ent->origin, 64.f, Vec3(1.f, 0.f, 0.f));
      R_Draw3DLines(GL_LINES, points, 2, color_red, true);

      points[1] = Vec3_Fmaf(ent->origin, 64.f, Vec3(0.f, 1.f, 0.f));
      R_Draw3DLines(GL_LINES, points, 2, color_green, true);

      points[1] = Vec3_Fmaf(ent->origin, 64.f, Vec3(0.f, 0.f, 1.f));
      R_Draw3DLines(GL_LINES, points, 2, color_blue, true);

      R_Draw3DBox(Box3_Expand(ent->abs_bounds, 2.f), color_red, false);
    }
  }
}

#pragma mark - EntityView

/**
 * @fn EntityView *EntityView::initWithEntity(EntityView *self, EditorEntity *entity)
 * @memberof EntityView
 */
static EntityView *initWithEntity(EntityView *self, EditorEntity *entity) {

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
static void setEntity(EntityView *self, EditorEntity *entity) {

  assert(entity);

  self->entity = *entity;

  self->key->control.state = ControlStateDefault;
  self->value->control.state = ControlStateDefault;

  const cm_entity_t *def = self->entity.def;
  if (def) {
    $(self->key, setAttributedText, def->key);
    if (def->parsed & ENTITY_VEC4) {
      $(self->value, setAttributedText, va("%g %g %g %g", def->vec4.x, def->vec4.y, def->vec4.z, def->vec4.w));
    } else if (def->parsed & ENTITY_VEC3) {
      $(self->value, setAttributedText, va("%g %g %g", def->vec3.x, def->vec3.y, def->vec3.z));
    } else if (def->parsed & ENTITY_VEC2) {
      $(self->value, setAttributedText, va("%g %g", def->vec2.x, def->vec2.y));
    } else if (def->parsed & ENTITY_FLOAT) {
      $(self->value, setAttributedText, va("%g", def->value));
    } else {
      $(self->value, setAttributedText, def->string);
    }

    if (!g_strcmp0(def->key, "classname")
        && !g_strcmp0(def->string, "worldspawn")) {
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
