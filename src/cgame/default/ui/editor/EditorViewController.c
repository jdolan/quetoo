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

#include <ObjectivelyMVC/Panel.h>

#include "EditorViewController.h"
#include "EntityViewController.h"
#include "MaterialViewController.h"
#include "MeshViewController.h"

#include "ControlsViewController.h"
#include "SettingsViewController.h"
#include "DialogViewController.h"

/**
 * @brief Height of the top menu strip, in pixels. The content area is inset by
 * this much at the top so the docked panel / sub-views sit below the strip.
 */
#define EDITOR_MENU_HEIGHT 44

/**
 * @brief Width of the docked editor panel, in pixels. Defined on the panel; its
 * content (tabs/pages) conforms to it.
 */
#define EDITOR_PANEL_WIDTH 420

#pragma mark - Delegates

/**
 * @brief ButtonDelegate for Create Entity.
 */
static void didClickCreateEntity(Button *button) {

  EditorViewController *this = button->delegate.self;

  $(this->entityViewController, createEntity);
}

/**
 * @brief ButtonDelegate for Delete Entity.
 */
static void didClickDeleteEntity(Button *button) {

  EditorViewController *this = button->delegate.self;

  $(this->entityViewController, deleteEntity);
}

/**
 * @brief ButtonDelegate for Save .map, .mat, and mesh configs.
 */
static void didClickSave(Button *button) {

  EditorViewController *this = button->delegate.self;

  cgi.Cbuf("save_editor_map\n");
  cgi.Cbuf("r_save_materials\n");

  $(this->meshViewController, save);
}

/**
 * @brief Presents `clazz` (a Settings/Controls view controller) in the content
 * area below the menu strip, hiding the editor panel. Pass NULL to return to
 * the Editor tab (tear down any presented controller and re-show the panel).
 */
static void selectContent(EditorViewController *this, Class *clazz) {

  if (this->contentViewController) {
    $(this->contentViewController, removeFromParentViewController);
    this->contentViewController = NULL;
  }

  if (clazz) {
    ((View *) this->panel)->hidden = true;

    ViewController *viewController = $((ViewController *) _alloc(clazz), init);
    assert(viewController);

    $((ViewController *) this, addChildViewController, viewController);
    $((View *) this->content, addSubview, viewController->view);

    // Center the panel in the content area (below the strip), at its natural
    // size — mirroring how the main menu centers Settings/Controls via
    // `#contentView > View > * { alignment: middle-center; }`.
    viewController->view->alignment = ViewAlignmentMiddleCenter;
    viewController->view->needsLayout = true;

    release(viewController);
    this->contentViewController = viewController;
  } else {
    ((View *) this->panel)->hidden = false;
  }
}

/**
 * @brief ButtonDelegate for the Editor tab: re-show the editor panel.
 */
static void didClickEditorTab(Button *button) {
  selectContent(button->delegate.self, NULL);
}

/**
 * @brief ButtonDelegate for the Settings tab.
 */
static void didClickSettings(Button *button) {
  selectContent(button->delegate.self, _SettingsViewController());
}

/**
 * @brief ButtonDelegate for the Controls tab.
 */
static void didClickControls(Button *button) {
  selectContent(button->delegate.self, _ControlsViewController());
}

/**
 * @brief ButtonDelegate to leave edit mode and reload the current map fresh
 * (discarding unsaved edits — Save first to keep them). `editor` is a CVAR_LATCH
 * server cvar, so it only takes effect on a map (re)start; `reconnect` keeps the
 * same running server, so the latch would never apply. Restart the current map
 * instead — CS_BSP holds "maps/<name>.bsp".
 */
static void didClickDisableEditor(Button *button) {

  const char *bsp = cgi.ConfigString(CS_BSP);

  if (bsp && *bsp) {
    char name[MAX_QPATH];
    StripExtension(Basename(bsp), name);
    cgi.Cbuf(va("set editor 0; map %s\n", name));
  } else {
    cgi.Cbuf("set editor 0\n");
  }
}

/**
 * @brief Quit the game.
 */
static void quit(ident data) {
  cgi.Cbuf("quit\n");
}

/**
 * @brief ButtonDelegate for Quit.
 */
static void didClickQuit(Button *button) {

  EditorViewController *this = button->delegate.self;

  const Dialog dialog = {
    .message = "Are you sure you want to quit?",
    .ok = "Yes",
    .cancel = "No",
    .okFunction = quit
  };

  ViewController *viewController = (ViewController *) $(alloc(DialogViewController), initWithDialog, &dialog);
  $((ViewController *) this, addChildViewController, viewController);
}

/**
 * @brief Appends a button to the top menu strip.
 */
static void menuButton(EditorViewController *this, const char *title, void (*didClick)(Button *)) {

  Button *button = $(alloc(Button), initWithTitle, title);
  assert(button);

  button->control.view.identifier = strdup(title);
  button->delegate.self = this;
  button->delegate.didClick = didClick;

  // Style in C (editor.css's #editorMenu rules don't reliably apply to this
  // controller's view tree), mirroring the main-menu strip buttons.
  View *buttonView = (View *) button;
  buttonView->backgroundColor = (SDL_Color) { 8, 21, 26, 255 };
  buttonView->borderColor = (SDL_Color) { 255, 255, 255, 255 };
  buttonView->borderWidth = 1;

  $((View *) this->menu, addSubview, (View *) button);
  release(button);
}

#define _Class _EditorViewController

#pragma mark - Object

static void dealloc(Object *self) {

  EditorViewController *this = (EditorViewController *) self;

  release(this->tabViewController);
  release(this->entityViewController);
  release(this->materialViewController);
  release(this->meshViewController);

  super(Object, self, dealloc);
}

#pragma mark - ViewController

/**
 * @see ViewController::loadView(ViewController *)
 */
static void loadView(ViewController *self) {

  super(ViewController, self, loadView);

  EditorViewController *this = (EditorViewController *) self;

  View *view = $$(View, viewWithResourceName, "ui/editor/EditorViewController.json", NULL);
  assert(view);

  view->stylesheet = $$(Stylesheet, stylesheetWithResourceName, "ui/editor/editor.css");
  assert(view->stylesheet);

  $(self, setView, view);
  release(view);

  // The root container fills the window. This is set here, not in editor.css:
  // a per-view stylesheet does not style its own root view, so a `#editor` rule
  // would no-op. (Its descendants editorMenu/editorContent ARE styled by CSS.)
  self->view->autoresizingMask = ViewAutoresizingFill;

  Outlet outlets[] = MakeOutlets(
    MakeOutlet("editorMenu", &this->menu),
    MakeOutlet("editorContent", &this->content),
    MakeOutlet("editorPanel", &this->panel),
    MakeOutlet("createEntity", &this->createEntity),
    MakeOutlet("deleteEntity", &this->deleteEntity),
    MakeOutlet("save", &this->save)
  );

  $(self->view, resolve, outlets);

  // Editor chrome layout is driven here, not in editor.css: the per-view
  // stylesheet's #editorMenu / #editorContent ID rules don't reliably reach this
  // controller's view tree, so set the essentials directly.

  // Top menu strip: horizontal, full width, docked at the top of the window.
  this->menu->axis = StackViewAxisHorizontal;
  this->menu->distribution = StackViewDistributionFillEqually;
  this->menu->spacing = 8;
  ((View *) this->menu)->autoresizingMask = ViewAutoresizingWidth;
  ((View *) this->menu)->padding = MakePadding(4, 4, 4, 4);
  ((View *) this->menu)->backgroundColor = (SDL_Color) { 34, 34, 34, 170 };
  // Force a real height: a horizontal StackView's sizeThatFits collapses its
  // height (children measure 0 before layout), leaving the strip's frame empty
  // so hitTest skips it and clicks fall through. minSize keeps it hittable.
  ((View *) this->menu)->minSize.h = EDITOR_MENU_HEIGHT;

  // Content area fills the window; its top inset reserves the strip's height so
  // the panel (and any Settings/Controls sub-view) sit below the strip.
  ((View *) this->content)->autoresizingMask = ViewAutoresizingFill;
  ((View *) this->content)->padding = MakePadding(EDITOR_MENU_HEIGHT, 0, 0, 0);

  // The editor Panel is a docked, fixed child (not a free window): disable drag
  // and resize, dock it top-left below the strip. Width is defined here on the
  // panel (its content conforms via autoresizing); height is filled per-frame by
  // fitContentHeight.
  this->panel->isDraggable = false;
  this->panel->isResizable = false;
  ((View *) this->panel)->alignment = ViewAlignmentTopLeft;
  ((View *) this->panel)->minSize.w = EDITOR_PANEL_WIDTH;

  // Editor tabs (entity/material/mesh) live inside the panel's content view.
  this->tabViewController = $(alloc(TabViewController), init);
  assert(this->tabViewController);

  this->entityViewController = $(alloc(EntityViewController), init);
  assert(this->entityViewController);

  this->materialViewController = $(alloc(MaterialViewController), init);
  assert(this->materialViewController);

  this->meshViewController = $(alloc(MeshViewController), init);
  assert(this->meshViewController);

  ViewController *tabViewController = (ViewController *) this->tabViewController;

  $(tabViewController, addChildViewController, (ViewController *) this->entityViewController);
  $(tabViewController, addChildViewController, (ViewController *) this->materialViewController);
  $(tabViewController, addChildViewController, (ViewController *) this->meshViewController);

  $(self, addChildViewController, tabViewController);
  $((View *) this->panel->contentView, addSubview, tabViewController->view);

  this->createEntity->delegate.self = this;
  this->createEntity->delegate.didClick = didClickCreateEntity;

  this->deleteEntity->delegate.self = this;
  this->deleteEntity->delegate.didClick = didClickDeleteEntity;

  this->save->delegate.self = this;
  this->save->delegate.didClick = didClickSave;

  // Top menu strip (mirrors the main-menu strip).
  menuButton(this, "Editor", didClickEditorTab);
  menuButton(this, "Settings", didClickSettings);
  menuButton(this, "Controls", didClickControls);
  menuButton(this, "Disable editor and reload map", didClickDisableEditor);
  menuButton(this, "Quit", didClickQuit);
}

/**
 * @see ViewController::respondToEvent(ViewController *, const SDL_Event *)
 */
static void respondToEvent(ViewController *self, const SDL_Event *event) {

  EditorViewController *this = (EditorViewController *) self;

  if (event->type == MVC_NOTIFICATION_EVENT) {

    switch (event->user.code) {
      case NOTIFICATION_ENTITY_SELECTED: {
        Control *deleteEntity = (Control *) this->deleteEntity;
        const int16_t number = (int16_t) (intptr_t) event->user.data1;
        if (number <= 0) {
          deleteEntity->state |= ControlStateDisabled;
        } else {
          deleteEntity->state &= ~ControlStateDisabled;
        }
        $(deleteEntity, stateDidChange);

        r_model_t *model = NULL;
        if (number > 0) {
          const cg_editor_entity_t *edit = &cg_editor.entities[number];
          if (edit->model && IS_MESH_MODEL(edit->model)) {
            model = (r_model_t *) edit->model;
          }
        }
        $(this->meshViewController, setModel, model);
      }
        break;
    }
  }

  super(ViewController, self, respondToEvent, event);
}

#pragma mark - EditorViewController

/**
 * @fn void EditorViewController::showEditorTab(EditorViewController *self)
 * @memberof EditorViewController
 */
static void showEditorTab(EditorViewController *self) {
  selectContent(self, NULL);
}

/**
 * @fn void EditorViewController::fitContentHeight(EditorViewController *self)
 * @memberof EditorViewController
 */
static void fitContentHeight(EditorViewController *self) {

  View *view = self->viewController.view;
  View *panel = (View *) self->panel;
  View *panelContent = (View *) self->panel->contentView;
  View *accessory = (View *) self->panel->accessoryView;

  // The render context is MVC's coordinate space (both come from
  // SDL_GetWindowSize), so derive the fill height from it rather than from
  // content->frame.h. That frame is 0 until a KEY_UI layout pass has run, but
  // this code never runs on a KEY_UI frame (see below), so reading it forced the
  // old "open, then press escape twice before the panel fills" behavior.
  const int w = cgi.context->w, h = cgi.context->h;

  // Available height below the strip (the content area's top inset is the strip).
  const int available = h - EDITOR_MENU_HEIGHT;
  if (available <= 0) {
    return;
  }

  // Steady state: already filled for this window size, nothing to do. Cheap
  // early-out so the forced layout below only runs on open and on resize.
  if (view->frame.w == w && view->frame.h == h && panel->frame.h == available) {
    return;
  }

  // Cg_CheckEditor runs from Cg_UpdateScreen, which the client invokes only when
  // key_dest != KEY_UI — never on a frame where WindowController lays the tree
  // out (that is Ui_Draw, KEY_UI only). So size the root to the window and force
  // the layout *here*; deferring via needsLayout (the old approach) parks the
  // reflow until the next KEY_UI frame, i.e. requires pressing escape to apply.
  view->frame.w = w;
  view->frame.h = h;
  view->needsLayout = true;
  $(view, layoutIfNeeded);

  // Non-page chrome: the accessory bar + the panel's own padding + the stack
  // spacing between content and accessory. Stable (independent of content
  // height), so the target is computed in a single shot.
  const int chrome = accessory->frame.h
      + panel->padding.top + panel->padding.bottom
      + self->panel->stackView->spacing;
  const int target = available - chrome;
  if (target <= 0) {
    return;
  }

  // Two things are needed, and neither happens on its own: (1) grow the panel —
  // its parent sizes it from its frame, not its content, so it won't expand to
  // fit; (2) stretch the content area so the accessory bar is pushed to the
  // bottom (the panel sizes its content to natural height otherwise). Apply it
  // immediately with a second forced layout so the panel is correct the first
  // time it is drawn (the next KEY_UI frame), without an escape round-trip.
  panelContent->minSize.h = target;
  const SDL_Size size = MakeSize(panel->frame.w, available);
  $(panel, resize, &size);
  $(view, layoutIfNeeded);
}

#pragma mark - Class lifecycle

/**
 * @see Class::initialize(Class *)
 */
static void initialize(Class *clazz) {

  ((ObjectInterface *) clazz->interface)->dealloc = dealloc;

  ((ViewControllerInterface *) clazz->interface)->loadView = loadView;
  ((ViewControllerInterface *) clazz->interface)->respondToEvent = respondToEvent;

  ((EditorViewControllerInterface *) clazz->interface)->showEditorTab = showEditorTab;
  ((EditorViewControllerInterface *) clazz->interface)->fitContentHeight = fitContentHeight;
}

/**
 * @fn Class *EditorViewController::_EditorViewController(void)
 * @memberof EditorViewController
 */
Class *_EditorViewController(void) {
  static Class *clazz;
  static Once once;

  do_once(&once, {
    clazz = _initialize(&(const ClassDef) {
      .name = "EditorViewController",
      .superclass = _ViewController(),
      .instanceSize = sizeof(EditorViewController),
      .interfaceOffset = offsetof(EditorViewController, interface),
      .interfaceSize = sizeof(EditorViewControllerInterface),
      .initialize = initialize,
    });
  });

  return clazz;
}

#undef _Class
