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
static vec3_t snapToGrid(const vec3_t v) {
  const float gridSize = cl_editor_grid_size->value;
  return Vec3_Scale(Vec3_Roundf(Vec3_Scale(v, 1.f / gridSize)), gridSize);
}

/**
 * @brief Sets the given entity's origin to where the client is looking.
 */
static void setEntityOriginFromClientView(cm_entity_t *entity) {

  vec3_t origin = Vec3_Fmaf(cl_view.origin, MAX_WORLD_DIST, cl_view.forward);
  const cm_trace_t tr = Cl_Trace(cl_view.origin, origin, Box3_Zero(), 0, CONTENTS_MASK_VISIBLE);

  origin = Vec3_Fmaf(tr.end, cl_editor_grid_size->value, Vec3_Negate(cl_view.forward));
  origin = snapToGrid(origin);

  if (Cm_PointLeafnum(origin, 0) == -1) {
    origin = snapToGrid(Vec3_Fmaf(cl_view.origin, 256.f, cl_view.forward));
  }

  Cm_EntitySetKeyValue(entity, "origin", ENTITY_VEC3, &origin);
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

  Cm_EntitySetKeyValue(this->entity.def, key, ENTITY_STRING, value);

  Cl_WriteEntityInfoCommand(this->entity.number, this->entity.def);

  if (view == this->add) {
    $(this->add, setEntity, NULL);
  }
}

/**
 * @brief ButtonDelegate for Create.
 */
static void didCreateEntity(Button *button) {

  cm_entity_t *entity = Cm_EntitySetKeyValue(NULL, "classname", ENTITY_STRING, "light");

  setEntityOriginFromClientView(entity);

  Cl_WriteEntityInfoCommand(-1, entity);

  Cm_FreeEntity(entity);
}

/**
 * @brief ButtonDelegate for Delete.
 */
static void didDeleteEntity(Button *button) {

  EntityViewController *this = button->delegate.self;

  if (this->entity.number) {

    Cl_WriteEntityInfoCommand(this->entity.number, NULL);

    $(this, setEntity, NULL);
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

  cm_entity_t *e = self->entity.def;

  if (!e) {
    return;
  }

  const SDL_Keycode key = event->key.keysym.sym;
  const int32_t mod = event->key.keysym.mod;

  if (mod & KMOD_CLIPBOARD) {

    char *info;

    switch (key) {
      case SDLK_c:
        info = Cm_EntityToInfoString(e);
        SDL_SetClipboardText(info);
        Mem_Free(info);
        Com_Print("Copied %s\n", Cm_EntityValue(e, "classname")->string);
        break;

      case SDLK_x:
        info = Cm_EntityToInfoString(e);
        SDL_SetClipboardText(info);
        Mem_Free(info);
        didDeleteEntity(self->delete);
        Com_Print("Cut %s\n", Cm_EntityValue(e, "classname")->string);
        break;

      case SDLK_v:
        if (SDL_HasClipboardText()) {
          info = SDL_GetClipboardText();
          cm_entity_t *entity = Cm_EntityFromInfoString(info);
          setEntityOriginFromClientView(entity);
          Cl_WriteEntityInfoCommand(-1, entity);
          Com_Print("Pasted %s\n", Cm_EntityValue(entity, "classname")->string);
          Cm_FreeEntity(entity);
          SDL_free(info);
        }
        break;
    }
  } else if (mod == KMOD_NONE) {

    if (key >= SDLK_1 && key <= SDLK_8) {
      Cvar_SetValue(cl_editor_grid_size->name, (1 << (key - SDLK_1)));
      Com_Print("Editor grid size set to %g\n", cl_editor_grid_size->value);
    }

    vec3_t move = Vec3_Zero();
    float step = cl_editor_grid_size->value;

    switch (key) {
      case SDLK_w:
      case SDLK_KP_8:
      case SDLK_UP:
        move = Vec3_Scale(cl_view.forward, +step);
        break;
      case SDLK_s:
      case SDLK_KP_2:
      case SDLK_DOWN:
        move = Vec3_Scale(cl_view.forward, -step);
        break;

      case SDLK_d:
      case SDLK_KP_6:
      case SDLK_RIGHT:
        move = Vec3_Scale(cl_view.right, +step);
        break;
      case SDLK_a:
      case SDLK_KP_4:
      case SDLK_LEFT:
        move = Vec3_Scale(cl_view.right, -step);
        break;

      case SDLK_q:
      case SDLK_KP_9:
      case SDLK_PAGEUP:
        move = Vec3_Scale(cl_view.up, +step);
        break;
      case SDLK_e:
      case SDLK_KP_3:
      case SDLK_PAGEDOWN:
        move = Vec3_Scale(cl_view.up, -step);
        break;
    }

    if (!Vec3_Equal(move, Vec3_Zero())) {

      vec3_t origin = Cm_EntityValue(e, "origin")->vec3;
      origin = snapToGrid(Vec3_Add(origin, move));

      Cm_EntitySetKeyValue(e, "origin", ENTITY_VEC3, &origin);

      Cl_WriteEntityInfoCommand(self->entity.number, e);
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
    if (number == this->entity.number) {

      $(this, setEntity, &(EditorEntity) {
        .number = number,
        .entity = &cl.entities[number],
        .def = cl.entity_definitions[number]
      });
    }
  }

  super(ViewController, self, respondToEvent, event);
}

/**
 * @see ViewController::viewWillAppear(ViewController *)
 */
static void viewWillAppear(ViewController *self) {

  EntityViewController *this = (EntityViewController *) self;

  int16_t number = 0;

  const vec3_t end = Vec3_Fmaf(cl_view.origin, MAX_WORLD_DIST, cl_view.forward);
  const cm_trace_t tr = Cl_Trace(cl_view.origin, end, Box3_Zero(), 0, CONTENTS_EDITOR);
  if (tr.fraction < 1.f) {
    number = (int16_t) (intptr_t) tr.ent;
  }

  $(this, setEntity, &(EditorEntity) {
    .number = number,
    .entity = &cl.entities[number],
    .def = cl.entity_definitions[number]
  });

  super(ViewController, self, viewWillAppear);
}

#pragma mark - EntityViewController

/**
 * @fn void EntityViewController::setEntity(EntityViewController *, EditorEntity *)
 * @memberof EntityViewController
 */
static void setEntity(EntityViewController *self, EditorEntity *entity) {

  $((View *) self->pairs, removeAllSubviews);

  if (entity) {

    self->entity = *entity;

    for (cm_entity_t *e = self->entity.def; e; e = e->next) {

      if (g_str_has_prefix(e->key, "_tb_")) {
        continue;
      }

      EntityView *view = $(alloc(EntityView), initWithEntity, &(EditorEntity) {
        .number = self->entity.number,
        .entity = self->entity.entity,
        .def = e
      });

      view->delegate.self = self;
      view->delegate.didEditEntity = didEditEntity;

      $((View *) self->pairs, addSubview, (View *) view);

      release(view);
    }
  } else {
    self->entity.number = -1;
    self->entity.entity = NULL;
    self->entity.def = NULL;
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
