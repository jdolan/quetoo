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

#include "ui_local.h"
#include "client.h"

#include "EntityViewController.h"
#include "EntityView.h"

#define _Class _EntityViewController

#pragma mark - Delegates

/**
 * @brief EntityViewDelegate for lights.
 */
static void didEditLight(cm_entity_t *entity, cm_entity_t *pair) {
  const r_bsp_model_t *bsp = r_models.world->bsp;

  r_bsp_light_t *light = NULL;

  r_bsp_light_t *l = bsp->lights;
  for (int32_t i = 0; i < bsp->num_lights; i++, l++) {
    if (l->entity == entity) {
      light = l;
      break;
    }
  }

  assert(light);

  if (!g_strcmp0(pair->key, "radius")) {
    light->radius = pair->value;
    light->bounds = Box3_FromCenterRadius(light->origin, light->radius);
    light->depth_pass_elements = NULL;
    light->num_depth_pass_elements = bsp->num_elements;
  } else if (!g_strcmp0(pair->key, "color")) {
    light->color = pair->vec3;
  } else if (!g_strcmp0(pair->key, "intensity")) {
    light->intensity = pair->value;
  }
}

/**
 * @brief EntityViewDelegate entry point.
 */
static void didEditEntity(EntityView *view, const char *key, const char *value) {

  EntityViewController *this = view->delegate.self;

  cm_entity_t *pair = view->entity;

  if (view == this->add) {
    pair = Mem_TagMalloc(sizeof(cm_entity_t), MEM_TAG_COLLISION);

    cm_entity_t *e = this->entity;
    while (e->next) {
      e = e->next;
    }
    e->next = pair;
    pair->prev = e;
  }

  g_strlcpy(pair->key, key, sizeof(pair->key));
  g_strlcpy(pair->string, value, sizeof(pair->string));

  Cm_ParseEntity(pair);

  const char *classname = Cm_EntityValue(this->entity, "classname")->nullable_string;
  if (!g_strcmp0(key, "classname")) {
    classname = value;
  }

  if (!g_strcmp0(classname, "light")) {
    didEditLight(this->entity, pair);
  }

  if (view == this->add) {
    EntityView *that = $(alloc(EntityView), initWithEntity, pair);

    that->delegate.self = this;
    that->delegate.didEditEntity = didEditEntity;

    $((View *) this->pairs, addSubviewRelativeTo, (View *) that, (View *) this->add, ViewPositionBefore);

    release(that);

    $(this->add, setEntity, NULL);
  }
}

#pragma mark - ViewController

/**
 * @see ViewController::loadView(ViewController *)
 */
static void loadView(ViewController *self) {

  EntityViewController *this = (EntityViewController *) self;

  Outlet outlets[] = MakeOutlets(
    MakeOutlet("pairs", &this->pairs),
    MakeOutlet("add", &this->add),
    MakeOutlet("create", &this->create)
  );

  self->view = $$(View, viewWithResourceName, "ui/editor/EntityViewController.json", outlets);
  self->view->stylesheet = $$(Stylesheet, stylesheetWithResourceName, "ui/editor/EntityViewController.css");

  this->add->delegate.self = this;
  this->add->delegate.didEditEntity = didEditEntity;
}

/**
 * @see ViewController::viewWillAppear(ViewController *)
 */
static void viewWillAppear(ViewController *self) {

  EntityViewController *this = (EntityViewController *) self;

  const cm_bsp_t *bsp = r_models.world->bsp->cm;
  cm_entity_t *best_entity = bsp->entities[0];

  float best_dot = .8f;

  for (int32_t i = 1; i < bsp->num_entities; i++) {

    cm_entity_t *entity = bsp->entities[i];

    const vec3_t origin = Cm_EntityValue(entity, "origin")->vec3;
    const vec3_t dir = Vec3_Direction(origin, cl_view.origin);

    const float dot = Vec3_Dot(cl_view.forward, dir);
    if (dot > best_dot) {

      const cm_trace_t tr = Cm_BoxTrace(cl_view.origin, origin, Box3_Zero(), 0, CONTENTS_MASK_VISIBLE);
      if (tr.fraction == 1.f) {
        best_dot = dot;
        best_entity = entity;
      }
    }
  }

  $(this, setEntity, best_entity);

  super(ViewController, self, viewWillAppear);
}

#pragma mark - EntityViewController

/**
 * @fn EntityViewController *EntityViewController::init(EntityViewController *)
 * @memberof EntityViewController
 */
static EntityViewController *init(EntityViewController *self) {

  self = (EntityViewController *) super(ViewController, self, init);
  if (self) {

  }

  return self;
}

/**
 * @fn void EntityViewController::setEntity(EntityViewController *, cm_entity *)
 * @memberof EntityViewController
 */
static void setEntity(EntityViewController *self, cm_entity_t *entity) {

  self->entity = entity;

  $((View *) self->pairs, removeAllSubviews);

  for (cm_entity_t *e = self->entity; e; e = e->next) {

    EntityView *view = $(alloc(EntityView), initWithEntity, e);

    view->delegate.self = self;
    view->delegate.didEditEntity = didEditEntity;

    $((View *) self->pairs, addSubview, (View *) view);

    release(view);
  }

  $((View *) self->pairs, sizeToFit);
}

#pragma mark - Class lifecycle

/**
 * @see Class::initialize(Class *)
 */
static void initialize(Class *clazz) {

  ((ViewControllerInterface *) clazz->interface)->loadView = loadView;
  ((ViewControllerInterface *) clazz->interface)->viewWillAppear = viewWillAppear;

  ((EntityViewControllerInterface *) clazz->interface)->init = init;
  ((EntityViewControllerInterface *) clazz->interface)->setEntity = setEntity;
}

/**
 * @fn Class *EntityViewController::_EntityViewController(void)
 * @memberof EntityViewController
 */
Class *_EntityViewController(void) {
  static Class *clazz;
  static Once once;

  do_once(&once, {
    clazz = _initialize(&(const ClassDef) {
      .name = "EntityViewController",
      .superclass = _ViewController(),
      .instanceSize = sizeof(EntityViewController),
      .interfaceOffset = offsetof(EntityViewController, interface),
      .interfaceSize = sizeof(EntityViewControllerInterface),
      .initialize = initialize,
    });
  });

  return clazz;
}

#undef _Class
