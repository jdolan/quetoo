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
extern cl_static_t cls;

static cvar_t *editor_grid_size;

#include "EntityViewController.h"
#include "EntityView.h"

#define _Class _EntityViewController

#pragma mark - Utilities

/**
 * @brief Sets the given entity's origin to where the client is looking.
 */
static void setEntityOriginFromClientView(cm_entity_t *entity) {

  vec3_t origin = Vec3_Fmaf(cl_view.origin, MAX_WORLD_DIST, cl_view.forward);
  const cm_trace_t tr = Cl_Trace(cl_view.origin, origin, Box3_Zero(), 0, CONTENTS_MASK_VISIBLE);

  origin = Vec3_Fmaf(tr.end, editor_grid_size->value, Vec3_Negate(cl_view.forward));
  origin = Vec3_Quantize(origin, editor_grid_size->value);

  if (Cm_PointLeafnum(origin, 0) == -1) {
    origin = Vec3_Fmaf(cl_view.origin, 256.f, cl_view.forward);
    origin = Vec3_Quantize(origin, editor_grid_size->value);
  }

  Cm_EntitySetKeyValue(entity, "origin", ENTITY_VEC3, &origin);
}

#pragma mark - Delegates

/**
 * @brief EntityViewDelegate.
 */
static void didEditEntity(EntityView *view, cm_entity_t *def) {

  EntityViewController *this = view->delegate.self;

  if (view == this->add) {

    if (!strlen(def->key) || !strlen(def->string)) {
      return;
    }

    Cm_EntitySetKeyValue(this->entity.def, def->key, ENTITY_STRING, def->string);
  }

  Cl_WriteEntityInfoCommand(this->entity.number, this->entity.def);

  if (view == this->add) {
    $(view, setEntity, &view->entity);
  }
}

#pragma mark - Object

/**
 * @see Object::dealloc(Object *)
 */
static void dealloc(Object *self) {

  EntityViewController *this = (EntityViewController *) self;

  Mem_Free(this->created);

  super(Object, self, dealloc);
}

#pragma mark - ViewController

/**
 * @see ViewController::loadView(ViewController *)
 */
static void loadView(ViewController *self) {

  EntityViewController *this = (EntityViewController *) self;

  Outlet outlets[] = MakeOutlets(
    MakeOutlet("pairs", &this->pairs),
    MakeOutlet("add", &this->add)
  );

  View *view = $$(View, viewWithResourceName, "ui/editor/EntityViewController.json", outlets);
  view->stylesheet = $$(Stylesheet, stylesheetWithResourceName, "ui/editor/EntityViewController.css");

  $(self, setView, view);
  release(view);

  this->add->delegate.self = this;
  this->add->delegate.didEditEntity = didEditEntity;
}

/**
 * @brief
 */
static void respondToKeyEvent(EntityViewController *self, const SDL_Event *event) {

  cm_entity_t *e = self->entity.def;

  const SDL_Keycode key = event->key.key;
  const int32_t mod = event->key.mod;

  if (mod & SDL_KMOD_CLIPBOARD) {
    switch (key) {
      case SDLK_C:
        if (cls.key_state.dest == KEY_UI && e) {
          char *info = Cm_EntityToInfoString(e);
          SDL_SetClipboardText(info);
          Mem_Free(info);
          Com_Print("Copied %s\n", Cm_EntityValue(e, "classname")->string);
        }
        break;

      case SDLK_X:
        if (cls.key_state.dest == KEY_UI && e) {
          char *info = Cm_EntityToInfoString(e);
          SDL_SetClipboardText(info);
          Mem_Free(info);
          $(self, deleteEntity);
          Com_Print("Cut %s\n", Cm_EntityValue(e, "classname")->string);
        }
        break;

      case SDLK_V:
        if (SDL_HasClipboardText()) {
          char *info = SDL_GetClipboardText();
          cm_entity_t *entity = Cm_EntityFromInfoString(info);
          if (entity) {
            setEntityOriginFromClientView(entity);
            Mem_Free(self->created);
            self->created = Cm_EntityToInfoString(entity);
            Cl_WriteEntityInfoCommand(-1, entity);
            Com_Print("Pasted %s\n", Cm_EntityValue(entity, "classname")->string);
            Cm_FreeEntity(entity);
          }
          SDL_free(info);
        }
        break;
    }
  } else if (mod == SDL_KMOD_NONE) {

    if (key >= SDLK_1 && key <= SDLK_8) {
      Cvar_SetValue(editor_grid_size->name, (1 << (key - SDLK_1)));
      Com_Print("Editor grid size set to %g\n", editor_grid_size->value);
    }

    if (cls.key_state.dest == KEY_UI && e) {

      vec3_t move = Vec3_Zero();
      vec3_t forward = Vec3_Zero();
      vec3_t right = Vec3_Zero();

      if (fabsf(cl_view.forward.x) > fabsf(cl_view.forward.y)) {
        forward.x = SignOf(cl_view.forward.x);
        right.y = SignOf(cl_view.right.y);
      } else {
        forward.y = SignOf(cl_view.forward.y);
        right.x = SignOf(cl_view.right.x);
      }

      const float step = editor_grid_size->value;

      switch (key) {
        case SDLK_W:
        case SDLK_KP_8:
        case SDLK_UP:
          move = Vec3_Scale(forward, +step);
          break;

        case SDLK_S:
        case SDLK_KP_2:
        case SDLK_DOWN:
          move = Vec3_Scale(forward, -step);
          break;

        case SDLK_D:
        case SDLK_KP_6:
        case SDLK_RIGHT:
          move = Vec3_Scale(right, +step);
          break;

        case SDLK_A:
        case SDLK_KP_4:
        case SDLK_LEFT:
          move = Vec3_Scale(right, -step);
          break;

        case SDLK_Q:
        case SDLK_KP_9:
        case SDLK_PAGEUP:
          move = Vec3_Scale(Vec3_Up(), +step);
          break;

        case SDLK_E:
        case SDLK_KP_3:
        case SDLK_PAGEDOWN:
          move = Vec3_Scale(Vec3_Up(), -step);
          break;
      }

      const char *classname = Cm_EntityValue(e, "classname")->string;
      if (!Vec3_Equal(move, Vec3_Zero()) && g_strcmp0(classname, "worldspawn")) {

        vec3_t origin = Cm_EntityValue(e, "origin")->vec3;
        origin = Vec3_Quantize(Vec3_Add(origin, move), editor_grid_size->value);

        Cm_EntitySetKeyValue(e, "origin", ENTITY_VEC3, &origin);

        Cl_WriteEntityInfoCommand(self->entity.number, e);
      }
    }
  }
}

/**
 * @see ViewContrxoller::respondToEvent(ViewController *, const SDL_Event *)
 */
static void respondToEvent(ViewController *self, const SDL_Event *event) {

  EntityViewController *this = (EntityViewController *) self;

  if (event->type == SDL_EVENT_KEY_DOWN) {
    respondToKeyEvent(this, event);
  }

  if (event->type == MVC_NOTIFICATION_EVENT) {

    switch (event->user.code) {
      case NOTIFICATION_ENTITY_PARSED: {

        const int16_t number = (int16_t) (intptr_t) event->user.data1;
        const char *info = cl.config_strings[CS_ENTITIES + number];

        if (number == this->entity.number || !g_strcmp0(this->created, info)) {

          $(this, setEntity, &(EditorEntity) {
            .number = number,
            .ent = &cl.entities[number],
            .def = cl.entity_definitions[number]
          });
        }
      }
        break;
    }
  }

  super(ViewController, self, respondToEvent, event);
}

/**
 * @see ViewController::viewWillAppear(ViewController *)
 */
static void viewWillAppear(ViewController *self) {

  EntityViewController *this = (EntityViewController *) self;

  EditorEntity entity = {
    .number = 0,
    .ent = cl.entities,
    .def = cl.entity_definitions[0]
  };

  int16_t skip = 0;
  float best_radius = FLT_MAX;

  vec3_t start = cl_view.origin;
  const vec3_t end = Vec3_Fmaf(start, MAX_WORLD_DIST, cl_view.forward);

  while (true) {

    const cm_trace_t tr = Cl_Trace(start, end, Box3_Zero(), skip, CONTENTS_EDITOR);
    if (tr.fraction == 1.f) {
      break;
    }

    start = Vec3_Add(tr.end, cl_view.forward);
    skip = (int16_t) (intptr_t) tr.ent;

    EditorEntity e = {
      .number = skip,
      .ent = &cl.entities[skip],
      .def = cl.entity_definitions[skip]
    };

    const float radius = Box3_Radius(e.ent->bounds);
    if (radius < best_radius) {
      best_radius = radius;
      entity = e;
    }
  }

  $(this, setEntity, &entity);

  super(ViewController, self, viewWillAppear);
}

#pragma mark - EntityViewController

/**
 * @fn void EntityViewController::createEntity(EntityViewController *)
 * @memberof EntityViewController
 */
static void createEntity(EntityViewController *self) {

  cm_entity_t *entity = Cm_EntitySetKeyValue(NULL, "classname", ENTITY_STRING, "light");
  setEntityOriginFromClientView(entity);

  Mem_Free(self->created);
  self->created = Cm_EntityToInfoString(entity);

  Cl_WriteEntityInfoCommand(-1, entity);
  Cm_FreeEntity(entity);
}

/**
 * @fn void EntityViewController::deleteEntity(EntityViewController *)
 * @memberof EntityViewController
 */
static void deleteEntity(EntityViewController *self) {

  if (self->entity.number) {
    Cl_WriteEntityInfoCommand(self->entity.number, NULL);
    $(self, setEntity, NULL);
  }
}

/**
 * @fn EntityViewController *EntityViewController::init(EntityViewController *)
 * @memberof EntityViewController
 */
static EntityViewController *init(EntityViewController *self) {
  return (EntityViewController *) super(ViewController, self, init);
}

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
        .ent = self->entity.ent,
        .def = e
      });

      view->delegate.self = self;
      view->delegate.didEditEntity = didEditEntity;

      $((View *) self->pairs, addSubview, (View *) view);

      release(view);
    }

  } else {
    self->entity.number = -1;
    self->entity.ent = NULL;
    self->entity.def = NULL;
  }

  $((View *) self->pairs, sizeToFit);

  SDL_PushEvent(&(SDL_Event) {
    .user.type = MVC_NOTIFICATION_EVENT,
    .user.code = NOTIFICATION_ENTITY_SELECTED,
    .user.data1 = (void *) (intptr_t) self->entity.number
  });
}

#pragma mark - Class lifecycle

/**
 * @see Class::initialize(Class *)
 */
static void initialize(Class *clazz) {

  ((ObjectInterface *) clazz->interface)->dealloc = dealloc;

  ((ViewControllerInterface *) clazz->interface)->loadView = loadView;
  ((ViewControllerInterface *) clazz->interface)->respondToEvent = respondToEvent;
  ((ViewControllerInterface *) clazz->interface)->viewWillAppear = viewWillAppear;

  ((EntityViewControllerInterface *) clazz->interface)->createEntity = createEntity;
  ((EntityViewControllerInterface *) clazz->interface)->deleteEntity = deleteEntity;
  ((EntityViewControllerInterface *) clazz->interface)->init = init;
  ((EntityViewControllerInterface *) clazz->interface)->setEntity = setEntity;

  editor_grid_size = Cvar_Add("editor_grid_size", "16", CVAR_ARCHIVE, "The editor grid size in world units. Use keys 1-8 to set, like in Radiant.");
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
