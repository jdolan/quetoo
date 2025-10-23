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

extern cl_client_t cl;

#include "EntityViewController.h"
#include "EntityView.h"

#define _Class _EntityViewController

#pragma mark - Delegates



/**
 * @brief EntityViewDelegate.
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

  Cl_WriteEntityInfoCommand(this->number, this->entity);

  if (view == this->add) {
    EntityView *that = $(alloc(EntityView), initWithEntity, pair);

    that->delegate.self = this;
    that->delegate.didEditEntity = didEditEntity;

    $((View *) this->pairs, addSubviewRelativeTo, (View *) that, (View *) this->add, ViewPositionBefore);

    release(that);

    $(this->add, setEntity, NULL);
  }
}

/**
 * @brief ButtonDelegate for Create.
 */
static void didCreateEntity(Button *button) {

  const vec3_t end = Vec3_Fmaf(cl_view.origin, MAX_WORLD_DIST, cl_view.forward);
  const cm_trace_t tr = Cl_Trace(cl_view.origin, end, Box3_Zero(), 0, CONTENTS_MASK_VISIBLE);

  const int32_t leaf = Cm_PointLeafnum(tr.end, 0);
  if (leaf == -1) {
    Com_Warn("Can't create an entity outside of the world\n");
    return;
  }

  vec3_t org = Vec3_Fmaf(tr.end, 16.f, tr.plane.normal);
  org = Vec3_Scale(Vec3_Roundf(Vec3_Scale(org, 1.f / 16.f)), 16.f);

  cm_entity_t *classname = Cm_AllocEntity();

  g_strlcpy(classname->key, "classname", sizeof(classname->key));
  g_strlcpy(classname->string, "light", sizeof(classname->string));

  Cm_ParseEntity(classname);

  cm_entity_t *origin = Cm_AllocEntity();

  g_strlcpy(origin->key, "origin", sizeof(origin->key));
  g_snprintf(origin->string, sizeof(origin->string), "%g %g %g", org.x, org.y, org.z);

  Cm_ParseEntity(origin);

  classname->next = origin;
  origin->prev = classname;

  Cl_WriteEntityInfoCommand(-1, classname);

  Cm_FreeEntity(classname);
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

  this->create->delegate.self = this;
  this->create->delegate.didClick = didCreateEntity;
}

/**
 * @see ViewController::viewWillAppear(ViewController *)
 */
static void viewWillAppear(ViewController *self) {

  EntityViewController *this = (EntityViewController *) self;

  const vec3_t end = Vec3_Fmaf(cl_view.origin, MAX_WORLD_DIST, cl_view.forward);
  const cm_trace_t tr = Cl_Trace(cl_view.origin, end, Box3_Zero(), 0, CONTENTS_EDITOR);
  if (tr.fraction < 1.f) {
    $(this, setEntity, (int16_t) (intptr_t) tr.ent);
  } else {
    $(this, setEntity, 0);
  }

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
 * @fn void EntityViewController::setEntity(EntityViewController *, int16_t)
 * @memberof EntityViewController
 */
static void setEntity(EntityViewController *self, int16_t number) {

  $((View *) self->pairs, removeAllSubviews);

  self->number = number;
  if (self->entity) {
    Cm_FreeEntity(self->entity);
  }

  self->entity = Cm_EntityFromInfoString(cl.config_strings[CS_ENTITIES + number]);

  for (cm_entity_t *e = self->entity; e; e = e->next) {

    if (g_str_has_prefix(e->key, "_tb_")) {
      continue;
    }

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
