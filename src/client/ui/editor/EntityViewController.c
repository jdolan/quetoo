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
}

/**
 * @see ViewContrxoller::respondToEvent(ViewController *, const SDL_Event *)
 */
static void respondToEvent(ViewController *self, const SDL_Event *event) {

  EntityViewController *this = (EntityViewController *) self;

  if (event->type == SDL_KEYDOWN) {

    switch (event->key.keysym.sym) {
      case SDLK_1:
        cl_editor_grid_size->value = 1.f;
        break;
      case SDLK_2:
        cl_editor_grid_size->value = 2.f;
        break;
      case SDLK_3:
        cl_editor_grid_size->value = 4.f;
        break;
      case SDLK_4:
        cl_editor_grid_size->value = 8.f;
        break;
      case SDLK_5:
        cl_editor_grid_size->value = 16.f;
        break;
      case SDLK_6:
        cl_editor_grid_size->value = 32.f;
        break;
      case SDLK_7:
        cl_editor_grid_size->value = 64.f;
        break;
      case SDLK_8:
        cl_editor_grid_size->value = 128.f;
        break;
      case SDLK_9:
        cl_editor_grid_size->value = 256.f;
        break;
      default:
        break;
    }

    if (this->entity) {

      vec3_t move = Vec3_Zero();

      switch (event->key.keysym.sym) {
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
          move = Vec3_Scale(cl_view.up, +cl_editor_grid_size->value);
          break;
        case SDLK_c:
        case SDLK_KP_3:
          move = Vec3_Scale(cl_view.up, -cl_editor_grid_size->value);
          break;
      }

      if (!Vec3_Equal(move, Vec3_Zero())) {

        vec3_t origin = Cm_EntityValue(this->entity, "origin")->vec3;
        origin = snapToGrid(Vec3_Add(origin, move), cl_editor_grid_size->value);

        Cm_EntitySetKeyValue(this->entity, "origin", ENTITY_VEC3, &origin);

        Cl_WriteEntityInfoCommand(this->number, this->entity);
      }
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

#pragma mark - EntityViewController

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

  const char *info = cl.config_strings[CS_ENTITIES + number];
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

  ((EntityViewControllerInterface *) clazz->interface)->setEntity = setEntity;

  cl_editor_grid_size = Cvar_Add("cl_editor_grid_size", "16.f", CVAR_ARCHIVE, "The editor grid size in world units. Use keys 1-9 to set, like in Radiant.");
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
