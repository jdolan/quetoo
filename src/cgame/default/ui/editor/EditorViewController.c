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

/**
 * @brief Non-page chrome of the docked panel, in UI (window_bounds) units: the
 * tab selection bar + the action-button bar + the panel's paddings/spacings.
 * Used to size the tab page so the panel spans from below the strip to the
 * window bottom: tabPage.h = window_bounds.h - EDITOR_MENU_HEIGHT - this.
 * Roughly constant across resolutions (UI elements are fixed-unit); tune if the
 * action bar doesn't sit flush at the window bottom.
 */
#define EDITOR_PANEL_CHROME 130

/**
 * @brief Gap, in UI units, between the panel and the menu strip / left edge, so
 * the docked panel floats slightly rather than butting against the chrome.
 */
#define EDITOR_PANEL_MARGIN 2

#pragma mark - Delegates

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

/**
 * @brief TabView delegate: re-fit the docked panel to the newly selected tab.
 * @details The panel is content-sized (Panel `autoresizing-mask: contain`), but a
 * PageView height change on tab switch does not reliably propagate up to the
 * panel in a single pass, so the panel stays sized to the first (entity) tab and
 * taller tabs (material/mesh) overflow -- their content runs past the panel and
 * the action bar overlaps the middle of the form. Marking the chain from the new
 * page up to and including the panel dirty forces the next layout pass (this same
 * KEY_UI frame, depth-first) to re-fit each view to the selected tab.
 */
static void didSelectTab(TabView *tabView, TabViewItem *tab) {

  // Intentionally a no-op. The tab page is a FIXED height (editor.css
  // `.tabPageView { height: 600 }`), so the panel never needs to re-fit when the
  // active tab changes -- the action bar stays put and shorter tabs simply have
  // empty space below their content. Per-tab auto-fit (sizeToContain here) was
  // both unreliable in MVC and crashed on teardown, so it was removed.
  (void) tabView;
  (void) tab;
}

#pragma mark - ViewController

static void setMeshTabEnabled(EditorViewController *this, bool enabled);

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
  // the panel (and any Settings/Controls sub-view) sit below the strip. A small
  // top/left margin makes the panel float clear of the strip and the left edge.
  ((View *) this->content)->autoresizingMask = ViewAutoresizingFill;
  ((View *) this->content)->padding = MakePadding(EDITOR_MENU_HEIGHT + EDITOR_PANEL_MARGIN, 0, 0, EDITOR_PANEL_MARGIN);

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

  // The Mesh tab is always present, but starts disabled (greyed, unclickable) and is
  // enabled only while the selected entity has a mesh model (see setMeshTabEnabled,
  // driven by NOTIFICATION_ENTITY_SELECTED). Keeping all three tabs fixed avoids the
  // add/remove churn (and the tab-bar width recompute) that dynamic tabs caused.
  setMeshTabEnabled(this, false);

  $(self, addChildViewController, tabViewController);

  // Re-fit the panel when the active tab changes (taller tabs otherwise overflow
  // the entity-tab-sized panel; see didSelectTab).
  this->tabViewController->tabView->delegate.self = this;
  this->tabViewController->tabView->delegate.didSelectTab = didSelectTab;

  // Match the working menu panels (Settings/CreateServer): the action buttons
  // live in a `.accessoryView` StackView INSIDE the panel's contentView (not the
  // Panel's built-in accessoryView property, whose contentSize math undersizes
  // the content and floats/pushes the bar). The tab view goes ABOVE that bar.
  View *editorAccessory = $(self->view, descendantWithIdentifier, "editorAccessory");
  assert(editorAccessory);
  $((View *) this->panel->contentView, addSubviewRelativeTo, tabViewController->view,
    editorAccessory, ViewPositionBefore);

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
        const int16_t number = (int16_t) (intptr_t) event->user.data1;

        r_model_t *model = NULL;
        if (number > 0) {
          const cg_editor_entity_t *edit = &cg_editor.entities[number];
          if (edit->model && IS_MESH_MODEL(edit->model)) {
            model = (r_model_t *) edit->model;
          }
        }

        // The entity selection just changed (in the UI): enable the Mesh tab iff the
        // selection has a model (greyed + unclickable otherwise), and push the model in.
        // The tab is always present, so its view/outlets are always valid.
        setMeshTabEnabled(this, model != NULL);
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
 * @brief Enables or disables the (always-present) Mesh tab. It is enabled only while the
 * selected entity has a mesh model -- worldspawn and lights have none, misc_model and
 * pickups do. Disabled = greyed and unclickable (TabView::respondToEvent skips a
 * TabStateDisabled tab); the state also drives the `.disabled` label style. Driven by the
 * NOTIFICATION_ENTITY_SELECTED handler, so it flips the instant the entity selection
 * changes. If the Mesh tab is showing when it gets disabled, we fall back to the first
 * tab (Entity) so the user is not left on a dead tab.
 */
static void setMeshTabEnabled(EditorViewController *this, bool enabled) {

  TabViewItem *tab = $(this->tabViewController, tabForViewController, (ViewController *) this->meshViewController);
  if (tab == NULL) {
    return;
  }

  const bool disabled = (tab->state & TabStateDisabled) != 0;
  if (enabled != disabled) {
    return; // already in the desired state
  }

  if (!enabled && (tab->state & TabStateSelected)) {
    $(this->tabViewController->tabView, selectTab, NULL); // leave the soon-to-be-dead tab
  }

  int state = tab->state;
  if (enabled) {
    state &= ~TabStateDisabled;
  } else {
    state |= TabStateDisabled;
  }
  $(tab, setState, state);
}

/**
 * @fn void EditorViewController::fitContentHeight(EditorViewController *self)
 * @memberof EditorViewController
 */
static void fitContentHeight(EditorViewController *self) {

  View *tabPage = (View *) self->tabViewController->tabView->tabPageView;

  // Size the (fixed, autoresizing:none) tab page so the panel spans from below
  // the strip to the window bottom -- resolution-adaptive, unlike a hardcoded
  // height. window_bounds is the MVC layout coordinate space here (matches the
  // root view), so the chrome/strip constants are in the same units.
  //
  // This only assigns a frame value + needsLayout flags; it does NOT force a
  // layout or touch any ancestor/root frame. That distinction is the whole
  // ballgame: the earlier full-height attempts broke hit-testing / crashed
  // precisely because they forced layout from this (KEY_GAME) tick. MVC applies
  // the new height on its next KEY_UI layout, and `autoresizing-mask: none`
  // keeps it (the parent does not override it).
  const int target = cgi.context->window_bounds.h - EDITOR_MENU_HEIGHT - EDITOR_PANEL_MARGIN - EDITOR_PANEL_CHROME;

  if (target > 0 && tabPage->frame.h != target) {
    tabPage->frame.h = target;
    tabPage->needsLayout = true;
    ((View *) self->panel)->needsLayout = true;
  }

  // Let the material tab size its stages scroll viewport to the same tab height
  // (it fills the space below the fixed maps + PBR boxes). Called every frame; it
  // self-guards and no-ops when the size is stable. Harmless when another tab is
  // showing.
  if (target > 0) {
    $(self->materialViewController, fitStagesHeight, target);
  }
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
