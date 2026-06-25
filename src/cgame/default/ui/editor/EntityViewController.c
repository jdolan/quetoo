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
 * @brief Returns true if this editor entity has BSP brushes.
 */
static bool isBrushEntity(const cg_editor_entity_t *entity) {
  return entity && entity->brushes != NULL;
}

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
 * @brief EntityViewDelegate.
 */
static void didEditEntity(EntityView *view, cm_entity_t *def) {

  EntityViewController *this = view->delegate.self;

  if (view == this->add) {

    if (!q_strlen(def->key) || !q_strlen(def->string)) {
      return;
    }

    if (isBrushEntity(this->entity) && !q_strcmp(def->key, "origin")) {
      Cg_Warn("Skipping origin on %s\n", cgi.EntityValue(this->entity->def, "classname")->string);
    } else {
      cgi.SetEntityKeyValue(this->entity->def, def->key, ENTITY_STRING, def->string);
    }

    $(this->add->key, setAttributedText, NULL);
    $(this->add->value, setAttributedText, NULL);
  }

  cgi.WriteEntityInfoCommand(this->entity->number, this->entity->def);
}

/**
 * @brief EntityViewDelegate.
 */
static void didEditTeamEntity(EntityView *view, cm_entity_t *def) {

  EntityViewController *this = view->delegate.self;

  if (view == this->teamAdd) {

    if (!q_strlen(def->key) || !q_strlen(def->string)) {
      return;
    }

    cgi.SetEntityKeyValue(this->teamEntity->def, def->key, ENTITY_STRING, def->string);

    $(this->teamAdd->key, setAttributedText, NULL);
    $(this->teamAdd->value, setAttributedText, NULL);
  }

  cgi.WriteEntityInfoCommand(this->teamEntity->number, this->teamEntity->def);
}

/**
 * @brief ButtonDelegate for Create Entity (lives on the Entity tab).
 */
static void didClickCreateEntity(Button *button) {
  $((EntityViewController *) button->delegate.self, createEntity);
}

/**
 * @brief ButtonDelegate for Delete Entity (lives on the Entity tab).
 */
static void didClickDeleteEntity(Button *button) {
  $((EntityViewController *) button->delegate.self, deleteEntity);
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
    MakeOutlet("entityBox", &this->entityBox),
    MakeOutlet("createEntity", &this->createEntity),
    MakeOutlet("deleteEntity", &this->deleteEntity),
    MakeOutlet("pairs", &this->pairs),
    MakeOutlet("add", &this->add),
    MakeOutlet("teamEntity", &this->teamMasterBox),
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

  this->createEntity->delegate.self = this;
  this->createEntity->delegate.didClick = didClickCreateEntity;

  this->deleteEntity->delegate.self = this;
  this->deleteEntity->delegate.didClick = didClickDeleteEntity;

  // The Create/Delete bar is pinned to the bottom-left of the tab. For bottom
  // alignment to reach the bottom of the viewport (not just the bottom of the
  // content), the page must fill the tab page; set in C since a per-view
  // stylesheet does not reliably style this controller's own views.
  self->view->autoresizingMask = ViewAutoresizingFill;

  StackView *createDelete = (StackView *) $(self->view, descendantWithIdentifier, "createDelete");
  assert(createDelete);
  createDelete->axis = StackViewAxisHorizontal;
  createDelete->spacing = 4;
  ((View *) createDelete)->alignment = ViewAlignmentBottomLeft;
}

/**
 * @brief Handles keyboard events for the EntityViewController, supporting copy/paste of entity definitions.
 */
static void respondToKeyEvent(EntityViewController *self, const SDL_Event *event) {

  cm_entity_t *e = self->entity ? self->entity->def : NULL;

  const SDL_Keycode key = event->key.key;
  // Mask out lock keys (Num/Caps/Scroll); their sticky modifier bits otherwise
  // suppress all un-modified editor key binds while toggled on (#839).
  const int32_t mod = event->key.mod & ~(SDL_KMOD_NUM | SDL_KMOD_CAPS | SDL_KMOD_SCROLL);

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

    if (key == SDLK_G) {
      self->show_func_groups = !self->show_func_groups;
      cg_editor.show_func_groups = self->show_func_groups;
      cgi.Print("func_group entities %s\n", self->show_func_groups ? "^2shown" : "^1hidden");
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

      if (!Vec3_Equal(move, Vec3_Zero()) && !isBrushEntity(self->entity)) {

        vec3_t origin = cgi.EntityValue(e, "origin")->vec3;
        origin = Vec3_Quantize(Vec3_Add(origin, move), editor_grid_size->value);

        cgi.SetEntityKeyValue(e, "origin", ENTITY_VEC3, &origin);

        cgi.WriteEntityInfoCommand(self->entity->number, e);
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

        cg_editor_entity_t *entity = &cg_editor.entities[number];

        if (this->entity && number == this->entity->number) {
          $(this, setEntity, entity);
        } else if (this->teamEntity && number == this->teamEntity->number) {
          // Preserve the selected light while refreshing team-level fields.
          $(this, setEntity, this->entity);
        } else if (!q_strcmp(this->created, info)) {
          $(this, setEntity, entity);
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

  const vec3_t start = cgi.view->origin;
  const vec3_t end = Vec3_Fmaf(start, MAX_WORLD_DIST, cgi.view->forward);

  const cg_editor_trace_t tr = Cg_EntitySelectionTrace(start, end);

  $(this, setEntity, tr.ent);

  super(ViewController, self, viewWillAppear);
}

/**
 * @see ViewController::viewWillDisappear(ViewController *)
 */
static void viewWillDisappear(ViewController *self) {
  cg_editor.selected = -1;
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

  if (self->entity && self->entity->number) {
    cgi.WriteEntityInfoCommand(self->entity->number, NULL);
    // Fall back to worldspawn (entity 0) rather than clearing the selection.
    $(self, setEntity, &cg_editor.entities[0]);
  }
}

/**
 * @fn EntityViewController *EntityViewController::init(EntityViewController *)
 * @memberof EntityViewController
 */
static EntityViewController *init(EntityViewController *self) {
  self = (EntityViewController *) super(ViewController, self, init);
  if (self) {
    self->show_func_groups = true;
  }
  return self;
}

/**
 * @fn void EntityViewController::setEntity(EntityViewController *, cg_editor_entity_t *)
 * @memberof EntityViewController
 */
static void setEntity(EntityViewController *self, cg_editor_entity_t *entity) {

  $((View *) self->pairs, removeAllSubviews);
  $((View *) self->teamPairs, removeAllSubviews);

  if (entity) {

    self->entity = entity;
    self->teamEntity = entity;

    // The classname is shown as the group-box title (not as an editable pair),
    // and gates the Team Master box (lights only).
    const char *classname = cgi.EntityValue(self->entity->def, "classname")->string;
    $(self->entityBox->label->text, setText, *classname ? classname : "Entity");
    ((View *) self->teamMasterBox)->hidden = q_strcmp(classname, "light") != 0;

    for (cm_entity_t *e = self->entity->def; e; e = e->next) {

      if (!q_strncmp(e->key, "_tb_", 4)) {
        continue;
      }

      if (!q_strcmp(e->key, "classname")) {
        continue;
      }

      if (isBrushEntity(self->entity) && !q_strcmp(e->key, "origin")) {
        continue;
      }

      EntityView *view = $(alloc(EntityView), initWithEntity, self->entity, e);

      view->delegate.self = self;
      view->delegate.didEditEntity = didEditEntity;

      $((View *) self->pairs, addSubview, (View *) view);

      release(view);
    }

    if (!q_strcmp(classname, "light")) {

      const char *team = cgi.EntityValue(self->entity->def, "team")->nullable_string;
      const int32_t teamMaster = Cg_FindTeamMaster(classname, team);
      if (teamMaster != -1 && teamMaster != self->entity->number) {

        self->teamEntity = &cg_editor.entities[teamMaster];

        for (cm_entity_t *e = self->teamEntity->def; e; e = e->next) {

          if (!q_strncmp(e->key, "_tb_", 4)
              || !q_strcmp(e->key, "classname")
              || !q_strcmp(e->key, "origin")
              || !q_strcmp(e->key, "team")
              || !q_strcmp(e->key, "team_master")) {
            continue;
          }

          EntityView *view = $(alloc(EntityView), initWithEntity, self->teamEntity, e);

          view->delegate.self = self;
          view->delegate.didEditEntity = didEditTeamEntity;

          $((View *) self->teamPairs, addSubview, (View *) view);

          release(view);
        }
      }
    }

  } else {
    self->entity = NULL;
    self->teamEntity = NULL;

    $(self->entityBox->label->text, setText, "Entity");
    ((View *) self->teamMasterBox)->hidden = true;
  }

  cg_editor.selected = self->entity ? self->entity->number : -1;

  // Delete is invalid for nothing-selected and for worldspawn (number 0).
  Control *deleteEntity = (Control *) self->deleteEntity;
  if (cg_editor.selected <= 0) {
    deleteEntity->state |= ControlStateDisabled;
  } else {
    deleteEntity->state &= ~ControlStateDisabled;
  }
  $(deleteEntity, stateDidChange);

  $((View *) self->pairs, sizeToFit);
  $((View *) self->teamPairs, sizeToFit);

  // The Team Master box toggles hidden<->visible and the boxes' contents change
  // on each selection; mark both boxes and their container StackView for layout
  // so the container re-arranges in the next (pre-draw) pass. Without this a
  // freshly Created entity renders with the boxes overlapping at the top until
  // the next selection forces a re-layout.
  ((View *) self->entityBox)->needsLayout = true;
  ((View *) self->teamMasterBox)->needsLayout = true;
  ((View *) self->entityBox)->superview->needsLayout = true;

  SDL_PushEvent(&(SDL_Event) {
    .user.type = MVC_NOTIFICATION_EVENT,
    .user.code = NOTIFICATION_ENTITY_SELECTED,
    .user.data1 = (void *) (intptr_t) (self->entity ? self->entity->number : -1)
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
  ((ViewControllerInterface *) clazz->interface)->viewWillDisappear = viewWillDisappear;

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
