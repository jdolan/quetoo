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

#include "cg_local.h"

static cvar_t *editor_grid_size;

#include "EntityViewController.h"
#include "EntityView.h"

#define _Class _EntityViewController

#pragma mark - Utilities

/**
 * @brief Sets the given entity's origin to where the client is looking.
 */
static void setEntityOriginFromClientView(cm_entity_t *entity) {

  vec3_t origin = Vec3_Fmaf(cgi.view->origin, MAX_WORLD_DIST, cgi.view->forward);
  const cm_trace_t tr = cgi.Trace(cgi.view->origin, origin, Box3_Zero(), 0, CONTENTS_SOLID);

  origin = Vec3_Fmaf(tr.end, editor_grid_size->value, Vec3_Negate(cgi.view->forward));
  origin = Vec3_Quantize(origin, editor_grid_size->value);

  if (cgi.PointLeafnum(origin, 0) == -1) {
    origin = Vec3_Fmaf(cgi.view->origin, 256.f, cgi.view->forward);
    origin = Vec3_Quantize(origin, editor_grid_size->value);
  }

  cgi.SetEntityKeyValue(entity, "origin", ENTITY_VEC3, &origin);
}

#pragma mark - Delegates

/**
 /**
 * @brief EntityViewDelegate.
 */
static void didEditEntity(EntityView *view, cm_entity_t *def) {

  EntityViewController *this = view->delegate.self;

  if (this->rebuilding) {
    return;
  }

  if (view == this->add) {

    if (!strlen(def->key) || !strlen(def->string)) {
      return;
    }

    cgi.SetEntityKeyValue(this->entity.def, def->key, ENTITY_STRING, def->string);

    $(this->add->key, setAttributedText, NULL);
    $(this->add->value, setAttributedText, NULL);
  }

  cgi.WriteEntityInfoCommand(this->entity.number, this->entity.def);
}

/**
 * @brief EntityViewDelegate.
 */
static void didEditTeamEntity(EntityView *view, cm_entity_t *def) {

  EntityViewController *this = view->delegate.self;

  if (this->rebuilding) {
    return;
  }

  if (view == this->teamAdd) {

    if (!strlen(def->key) || !strlen(def->string)) {
      return;
    }

    cgi.SetEntityKeyValue(this->teamEntity.def, def->key, ENTITY_STRING, def->string);

    $(this->teamAdd->key, setAttributedText, NULL);
    $(this->teamAdd->value, setAttributedText, NULL);
  }

  cgi.WriteEntityInfoCommand(this->teamEntity.number, this->teamEntity.def);
}

#pragma mark - Object

/**
 * @see Object::dealloc(Object *)
 */
static void dealloc(Object *self) {

  EntityViewController *this = (EntityViewController *) self;

  cgi.Free(this->created);

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
    MakeOutlet("add", &this->add),
    MakeOutlet("teamPairs", &this->teamPairs),
    MakeOutlet("teamAdd", &this->teamAdd)
  );

  View *view = $$(View, viewWithResourceName, "ui/editor/EntityViewController.json", outlets);
  view->stylesheet = $$(Stylesheet, stylesheetWithResourceName, "ui/editor/EntityViewController.css");

  $(self, setView, view);
  release(view);

  this->add->delegate.self = this;
  this->add->delegate.didEditEntity = didEditEntity;

  this->teamAdd->delegate.self = this;
  this->teamAdd->delegate.didEditEntity = didEditTeamEntity;
}

/**
 * @brief Handles keyboard events for the EntityViewController, supporting copy/paste of entity definitions.
 */
static void respondToKeyEvent(EntityViewController *self, const SDL_Event *event) {

  cm_entity_t *e = self->entity.def;

  const SDL_Keycode key = event->key.key;
  const int32_t mod = event->key.mod;

  if (mod & SDL_KMOD_CLIPBOARD) {
    switch (key) {
      case SDLK_C:
        if (cgi.GetKeyDest() == KEY_UI && e) {
          char *info = cgi.EntityToInfoString(e);
          SDL_SetClipboardText(info);
          cgi.Free(info);
          cgi.Print("Copied %s\n", cgi.EntityValue(e, "classname")->string);
        }
        break;

      case SDLK_X:
        if (cgi.GetKeyDest() == KEY_UI && e) {
          char *info = cgi.EntityToInfoString(e);
          SDL_SetClipboardText(info);
          cgi.Free(info);
          $(self, deleteEntity);
          cgi.Print("Cut %s\n", cgi.EntityValue(e, "classname")->string);
        }
        break;

      case SDLK_V:
        if (SDL_HasClipboardText()) {
          char *info = SDL_GetClipboardText();
          cm_entity_t *entity = cgi.EntityFromInfoString(info);
          if (entity) {
            setEntityOriginFromClientView(entity);
            cgi.Free(self->created);
            self->created = cgi.EntityToInfoString(entity);
            cgi.WriteEntityInfoCommand(-1, entity);
            cgi.Print("Pasted %s\n", cgi.EntityValue(entity, "classname")->string);
            cgi.FreeEntity(entity);
          }
          SDL_free(info);
        }
        break;
    }
  } else if (mod == SDL_KMOD_NONE) {

    if (key >= SDLK_1 && key <= SDLK_8) {
      cgi.SetCvarValue(editor_grid_size->name, (1 << (key - SDLK_1)));
      cgi.Print("Editor grid size set to %g\n", editor_grid_size->value);
    }

    if (cgi.GetKeyDest() == KEY_UI && e) {

      vec3_t move = Vec3_Zero();
      vec3_t forward = Vec3_Zero();
      vec3_t right = Vec3_Zero();

      if (fabsf(cgi.view->forward.x) > fabsf(cgi.view->forward.y)) {
        forward.x = SignOf(cgi.view->forward.x);
        right.y = SignOf(cgi.view->right.y);
      } else {
        forward.y = SignOf(cgi.view->forward.y);
        right.x = SignOf(cgi.view->right.x);
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

      const char *model = cgi.EntityValue(e, "model")->string;
      if (!Vec3_Equal(move, Vec3_Zero()) && *model != '*') {

        vec3_t origin = cgi.EntityValue(e, "origin")->vec3;
        origin = Vec3_Quantize(Vec3_Add(origin, move), editor_grid_size->value);

        cgi.SetEntityKeyValue(e, "origin", ENTITY_VEC3, &origin);

        cgi.WriteEntityInfoCommand(self->entity.number, e);
      }
    }
  }
}

/**
 * @see ViewContrxoller::respondToEvent(ViewController *, const SDL_Event *)
 */
static void respondToEvent(ViewController *self, const SDL_Event *event) {

  EntityViewController *this = (EntityViewController *) self;

  if (event->type == SDL_EVENT_KEY_DOWN
      && (cgi.GetKeyDest() == KEY_UI || cgi.GetKeyDest() == KEY_GAME)) {
    respondToKeyEvent(this, event);
  }

  if (event->type == MVC_NOTIFICATION_EVENT) {

    switch (event->user.code) {
      case NOTIFICATION_ENTITY_PARSED: {

        const int16_t number = (int16_t) (intptr_t) event->user.data1;
        const char *info = cgi.client->config_strings[CS_ENTITIES + number];

        const EditorEntity entity = {
          .number = number,
          .ent = &cgi.client->entities[number],
          .def = cg_edit[number].def
        };

        if (number == this->entity.number || number == this->teamEntity.number) {
          $(this, setEntity, &entity);
        } else if (!g_strcmp0(this->created, info)) {
          $(this, setEntity, &entity);
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
    .ent = cgi.client->entities,
    .def = cg_edit[0].def
  };

  cl_entity_t *ent = NULL;
  float best_radius = FLT_MAX;

  vec3_t start = cgi.view->origin;
  const vec3_t end = Vec3_Fmaf(start, MAX_WORLD_DIST, cgi.view->forward);

  while (true) {

    const cm_trace_t tr = cgi.Trace(start, end, Box3_Zero(), ent, CONTENTS_EDITOR);
    if (tr.fraction == 1.f) {
      break;
    }

    start = Vec3_Add(tr.end, cgi.view->forward);
    ent = tr.ent;

    EditorEntity e = {
      .number = ent->current.number,
      .ent = ent,
      .def = cg_edit[ent->current.number].def
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

  cm_entity_t *entity = cgi.SetEntityKeyValue(NULL, "classname", ENTITY_STRING, "light");
  setEntityOriginFromClientView(entity);

  cgi.Free(self->created);
  self->created = cgi.EntityToInfoString(entity);

  cgi.WriteEntityInfoCommand(-1, entity);
  cgi.FreeEntity(entity);
}

/**
 * @fn void EntityViewController::deleteEntity(EntityViewController *)
 * @memberof EntityViewController
 */
static void deleteEntity(EntityViewController *self) {

  if (self->entity.number) {
    cgi.WriteEntityInfoCommand(self->entity.number, NULL);
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
 * @fn void EntityViewController::setEntity(EntityViewController *, const EditorEntity *)
 * @memberof EntityViewController
 */
static void setEntity(EntityViewController *self, const EditorEntity *entity) {

  self->rebuilding = true;

  $((View *) self->pairs, removeAllSubviews);
  $((View *) self->teamPairs, removeAllSubviews);

  if (entity) {

    self->entity = *entity;
    self->teamEntity = *entity;

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

    const char *classname = cgi.EntityValue(self->entity.def, "classname")->string;
    if (!g_strcmp0(classname, "light")) {

      const char *team = cgi.EntityValue(self->entity.def, "team")->nullable_string;
      const int32_t teamMaster = Cg_FindTeamMaster(classname, team);
      if (teamMaster != -1 && teamMaster != self->entity.number) {

        self->teamEntity = (EditorEntity) {
          .number = teamMaster,
          .ent = &cgi.client->entities[teamMaster],
          .def = cg_edit[teamMaster].def
        };

        for (cm_entity_t *e = self->teamEntity.def; e; e = e->next) {

          if (g_str_has_prefix(e->key, "_tb_")
              || !g_strcmp0(e->key, "classname")
              || !g_strcmp0(e->key, "origin")
              || !g_strcmp0(e->key, "team")
              || !g_strcmp0(e->key, "team_master")) {
            continue;
          }

          EntityView *view = $(alloc(EntityView), initWithEntity, &(EditorEntity) {
            .number = self->teamEntity.number,
            .ent = self->teamEntity.ent,
            .def = e
          });

          view->delegate.self = self;
          view->delegate.didEditEntity = didEditTeamEntity;

          $((View *) self->teamPairs, addSubview, (View *) view);

          release(view);
        }
      }
    }

  } else {
    self->entity.number = -1;
    self->entity.ent = NULL;
    self->entity.def = NULL;

    self->teamEntity.number = -1;
    self->teamEntity.ent = NULL;
    self->teamEntity.def = NULL;
  }

  $((View *) self->pairs, sizeToFit);
  $((View *) self->teamPairs, sizeToFit);

  self->rebuilding = false;

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

  editor_grid_size = cgi.AddCvar("editor_grid_size", "16", CVAR_ARCHIVE, "The editor grid size in world units. Use keys 1-8 to set, like in Radiant.");
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
