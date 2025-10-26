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

static cvar_t *cl_editor_grid_size;

#include "EntityViewController.h"
#include "EntityView.h"

#define _Class _EntityViewController

/**
 * @brief Snaps the vector to the given grid size.
 */
static vec3_t snapToGrid(const vec3_t v, float gridSize) {
  return Vec3_Scale(Vec3_Roundf(Vec3_Scale(v, 1.f / gridSize)), gridSize);
}

#pragma mark - Delegates

/**
 * @brief EntityViewDelegate.
 */
static void didEditEntity(EntityView *view, const char *key, const char *value) {

  if (!strlen(key) || !strlen(value)) {
    return;
  }

  EntityViewController *this = view->delegate.self;

  Cm_EntitySetKeyValue(this->entity, key, ENTITY_STRING, value);

  Cl_WriteEntityInfoCommand(this->number, this->entity);

  if (view == this->add) {
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

  cm_entity_t *entity = Cm_EntitySetKeyValue(NULL, "classname", ENTITY_STRING, "light");

  const vec3_t origin = snapToGrid(tr.end, cl_editor_grid_size->value);
  Cm_EntitySetKeyValue(entity, "origin", ENTITY_VEC3, &origin);

  Cl_WriteEntityInfoCommand(-1, entity);

  Cm_FreeEntity(entity);
}

/**
 * @brief ButtonDelegate for Delete.
 */
static void didDeleteEntity(Button *button) {

  EntityViewController *this = button->delegate.self;
  if (this->entity && this->number) {
    Cl_WriteEntityInfoCommand(this->number, NULL);
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
    MakeOutlet("create", &this->create),
    MakeOutlet("delete", &this->delete)
  );

  View *view = $$(View, viewWithResourceName, "ui/editor/EntityViewController.json", outlets);
  view->stylesheet = $$(Stylesheet, stylesheetWithResourceName, "ui/editor/EntityViewController.css");

  $(self, setView, view);
  release(view);

  this->add->delegate.self = this;
  this->add->delegate.didEditEntity = didEditEntity;

  this->create->delegate.self = this;
  this->create->delegate.didClick = didCreateEntity;

  this->delete->delegate.self = this;
  this->delete->delegate.didClick = didDeleteEntity;
}

/**
 * @brief
 */
static void respondToKeyEvent(EntityViewController *self, const SDL_Event *event) {

  const SDL_Keycode key = event->key.keysym.sym;

  if (key >= SDLK_1 && key <= SDLK_8) {
    Cvar_SetValue(cl_editor_grid_size->name, (1 << (key - SDLK_1)));
    Com_Print("Editor grid size set to %g\n", cl_editor_grid_size->value);
  }

  if (self->entity) {

    vec3_t move = Vec3_Zero();

    switch (key) {
      case SDLK_w:
      case SDLK_KP_8:
      case SDLK_UP:
        move = Vec3_Scale(cl_view.forward, +cl_editor_grid_size->value);
        break;
      case SDLK_s:
      case SDLK_KP_2:
      case SDLK_DOWN:
        move = Vec3_Scale(cl_view.forward, -cl_editor_grid_size->value);
        break;

      case SDLK_d:
      case SDLK_KP_6:
      case SDLK_RIGHT:
        move = Vec3_Scale(cl_view.right, +cl_editor_grid_size->value);
        break;
      case SDLK_a:
      case SDLK_KP_4:
      case SDLK_LEFT:
        move = Vec3_Scale(cl_view.right, -cl_editor_grid_size->value);
        break;

      case SDLK_e:
      case SDLK_KP_9:
      case SDLK_PAGEUP:
        move = Vec3_Scale(cl_view.up, +cl_editor_grid_size->value);
        break;
      case SDLK_c:
      case SDLK_KP_3:
      case SDLK_PAGEDOWN:
        move = Vec3_Scale(cl_view.up, -cl_editor_grid_size->value);
        break;
    }

    if (!Vec3_Equal(move, Vec3_Zero())) {

      vec3_t origin = Cm_EntityValue(self->entity, "origin")->vec3;
      origin = snapToGrid(Vec3_Add(origin, move), cl_editor_grid_size->value);
      Cm_EntitySetKeyValue(self->entity, "origin", ENTITY_VEC3, &origin);

      Cl_WriteEntityInfoCommand(self->number, self->entity);
    }

    switch (key) {
      case SDLK_DELETE:
      case SDLK_BACKSPACE:
      case SDLK_KP_BACKSPACE:
        didDeleteEntity(self->delete);
        break;
    }
  }
}

/**
 * @see ViewContrxoller::respondToEvent(ViewController *, const SDL_Event *)
 */
static void respondToEvent(ViewController *self, const SDL_Event *event) {

  EntityViewController *this = (EntityViewController *) self;

  if ($(self->view, isVisible)) {
    if (event->type == SDL_KEYDOWN) {
      respondToKeyEvent(this, event);
    }
  }

  if (event->type == MVC_NOTIFICATION_EVENT && event->user.code == NOTIFICATION_ENTITY_PARSED) {
    const int16_t number = (int16_t) (intptr_t) event->user.data1;
    if (number == this->number) {
      $(this, setEntity, number);
    }
  }

  super(ViewController, self, respondToEvent, event);
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

/**
 * @see ViewController::viewWillDisappear(ViewController *)
 */
static void viewWillDisappear(ViewController *self) {

  EntityViewController *this = (EntityViewController *) self;

  Cm_FreeEntity(this->entity);

  this->number = 0;
  this->entity = NULL;
}

#pragma mark - EntityViewController

/**
 * @fn void EntityViewController::setEntity(EntityViewController *, int16_t)
 * @memberof EntityViewController
 */
static void setEntity(EntityViewController *self, int16_t number) {

  $((View *) self->pairs, removeAllSubviews);

  const char *info = cl.config_strings[CS_ENTITIES + number];
  if (strlen(info)) {

    self->number = number;
    self->entity = Cm_EntityFromInfoString(info);

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
  } else {

    Cm_FreeEntity(self->entity);

    self->number = -1;
    self->entity = NULL;
  }

  $((View *) self->pairs, sizeToFit);
}

#pragma mark - Class lifecycle

/**
 * @see Class::initialize(Class *)
 */
static void initialize(Class *clazz) {

  ((ViewControllerInterface *) clazz->interface)->loadView = loadView;
  ((ViewControllerInterface *) clazz->interface)->respondToEvent = respondToEvent;
  ((ViewControllerInterface *) clazz->interface)->viewWillAppear = viewWillAppear;
  ((ViewControllerInterface *) clazz->interface)->viewWillDisappear = viewWillDisappear;

  ((EntityViewControllerInterface *) clazz->interface)->setEntity = setEntity;

  cl_editor_grid_size = Cvar_Add("cl_editor_grid_size", "16.f", CVAR_ARCHIVE, "The editor grid size in world units. Use keys 1-8 to set, like in Radiant.");
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
