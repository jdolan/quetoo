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

#include <Objectively/Null.h>

#include "cg_local.h"

#include "HudViewController.h"
#include "CrosshairView.h"
#include "ScoreboardView.h"

#define _Class _HudViewController

#define HUD_DEFAULT "default"

HudViewController *cg_hud_view_controller;

AtlasImage *Cg_HudImage(const char *name) {

  if (cg_hud_view_controller) {
    return $(cg_hud_view_controller, image, name);
  }

  return NULL;
}

#pragma mark - Object

/**
 * @see Object::dealloc(Object *)
 */
static void dealloc(Object *self) {

  HudViewController *this = (HudViewController *) self;

  if (cg_hud_view_controller == this) {
    cg_hud_view_controller = NULL;
  }

  release(this->hud);
  release(this->scoreboard);
  release(this->navEdit);
  release(this->notify);
  release(this->chat);
  release(this->diagnostics);
  release(this->images);

  super(Object, self, dealloc);
}

#pragma mark - ViewController

/**
 * @see ViewController::init(ViewController *)
 */
static ViewController *init(ViewController *self) {

  self = super(ViewController, self, init);
  if (self) {
    HudViewController *this = (HudViewController *) self;

    this->images = $$(Dictionary, dictionary);
    assert(this->images);
  }

  return self;
}

/**
 * @see ViewController::loadView(ViewController *)
 */
static void loadView(ViewController *self) {

  HudViewController *this = (HudViewController *) self;

  View *view = $(alloc(View), initWithFrame, NULL);
  assert(view);

  view->autoresizingMask = ViewAutoresizingFill;

  $(self, setView, view);
  release(view);

  this->navEdit = (NavEditView *) $((View *) alloc(NavEditView), init);
  assert(this->navEdit);

  $(view, addSubview, (View *) this->navEdit);

  this->notify = (NotifyView *) $((View *) alloc(NotifyView), init);
  assert(this->notify);

  $(view, addSubview, (View *) this->notify);

  this->chat = (ChatView *) $((View *) alloc(ChatView), init);
  assert(this->chat);

  $(view, addSubview, (View *) this->chat);

  this->diagnostics = (DiagnosticsView *) $((View *) alloc(DiagnosticsView), init);
  assert(this->diagnostics);

  $(this, reload);
}

#pragma mark - HudViewController

/**
 * @fn AtlasImage *HudViewController::image(HudViewController *self, const char *name)
 * @memberof HudViewController
 */
static AtlasImage *image(HudViewController *self, const char *name) {

  assert(name);

  Theme *theme = cgi.Theme();
  if (theme == NULL) {
    return NULL;
  }

  AtlasImage *image = $(theme, icon, name);
  if (image) {
    return image;
  }

  Object *cached = $(self->images, objectForKeyPath, name);
  if (cached == NULL) {

    Image *loaded = Cg_LoadImage(name);
    if (loaded) {
      cached = (Object *) $($(theme, icons), addImageWithName, name, loaded);
      release(loaded);

      self->atlasDirty = true;
    } else {
      Cg_Warn("Failed to load %s\n", name);
      cached = (Object *) $$(Null, null);
    }

    $(self->images, setObjectForKeyPath, cached, name);
  }

  return $(cached, isKindOfClass, _AtlasImage()) ? (AtlasImage *) cached : NULL;
}

/**
 * @brief The name of `file` in `hud`, if the hud provides it, else in the default
 * hud. A hud overrides only the files it ships; the rest it inherits.
 */
static const char *hudResource(const char *hud, const char *file) {

  const char *name = va("ui/hud/%s/%s", hud, file);
  if (cgi.StatFile(name, NULL)) {
    return name;
  }

  return va("ui/hud/%s/%s", HUD_DEFAULT, file);
}

/**
 * @brief Loads the hud's View and Stylesheet, or `NULL` if either is missing.
 */
static View *loadHud(const char *hud) {

  const char *json = hudResource(hud, "hud.json");

  View *view = $$(View, viewWithResourceName, json, NULL);
  if (view == NULL) {
    Cg_Warn("Failed to load %s\n", json);
    return NULL;
  }

  const char *css = hudResource(hud, "hud.css");

  view->stylesheet = $$(Stylesheet, stylesheetWithResourceName, css);
  if (view->stylesheet == NULL) {
    Cg_Warn("Failed to load %s\n", css);
    release(view);
    return NULL;
  }

  return view;
}

/**
 * @brief Loads the hud's scoreboard, which a module MAY name a ScoreboardView subclass in.
 */
static ScoreboardView *loadScoreboard(const char *hud) {

  const char *json = hudResource(hud, "scoreboard.json");

  View *scoreboard = $$(View, viewWithResourceName, json, NULL);
  if (scoreboard == NULL || !$((Object *) scoreboard, isKindOfClass, _ScoreboardView())) {
    Cg_Warn("%s did not yield a ScoreboardView\n", json);
    release(scoreboard);
    scoreboard = $((View *) alloc(ScoreboardView), init);
  }

  const char *css = hudResource(hud, "scoreboard.css");

  scoreboard->stylesheet = $$(Stylesheet, stylesheetWithResourceName, css);
  if (scoreboard->stylesheet == NULL) {
    Cg_Warn("Failed to load %s\n", css);
  }

  scoreboard->autoresizingMask = ViewAutoresizingFill;

  return (ScoreboardView *) scoreboard;
}

/**
 * @fn void HudViewController::reload(HudViewController *self)
 * @memberof HudViewController
 */
static void reload(HudViewController *self) {

  if (self->hud) {
    $((View *) self->hud, removeFromSuperview);
    self->hud = release(self->hud);
  }

  // The scoreboard belongs to the hud, but shows through the intermission and with the
  // HUD off, so it is swapped before the hud and survives a hud that fails to load
  if (self->scoreboard) {
    $((View *) self->scoreboard, removeFromSuperview);
    self->scoreboard = release(self->scoreboard);
  }

  self->scoreboard = loadScoreboard(cg_hud->string);
  $(self->viewController.view, addSubview, (View *) self->scoreboard);

  View *hud = loadHud(cg_hud->string);
  if (hud == NULL && q_strcmp(cg_hud->string, HUD_DEFAULT)) {
    Cg_Warn("Falling back to the %s HUD\n", HUD_DEFAULT);
    hud = loadHud(HUD_DEFAULT);
  }

  if (hud == NULL) {
    Cg_Warn("No HUD\n");
    return;
  }

  hud->autoresizingMask = ViewAutoresizingFill;

  // beneath the notify lines, the chat, the scoreboard and the nav edit
  $(self->viewController.view, addSubview, hud);
  $(self->viewController.view, bringSubviewToFront, (View *) self->notify);
  $(self->viewController.view, bringSubviewToFront, (View *) self->chat);
  $(self->viewController.view, bringSubviewToFront, (View *) self->scoreboard);
  $(self->viewController.view, bringSubviewToFront, (View *) self->navEdit);
  self->hud = hud;

  // The diagnostics join the hud's layout so that its stylesheet and inset apply to them
  View *layout = $(hud, descendantWithIdentifier, "layout") ?: hud;
  $(layout, addSubview, (View *) self->diagnostics);

  $(self, warm);

  $(self->viewController.view, updateBindings, NULL);
}

/**
 * @brief ViewEnumerator for updateWithFrame: in the editor, only the crosshair shows. Runs
 * before the hierarchy updates, so an element that hides itself still can. The layout wrapper
 * is looked through, not hidden, since the crosshair lives in it.
 */
static void hideForEditor(View *view, ident data) {

  if (view->identifier && strcmp(view->identifier, "layout") == 0) {
    $(view, enumerateSubviews, hideForEditor, data);
    return;
  }

  const bool crosshair = $((Object *) view, isKindOfClass, _CrosshairView());
  $(view, setHidden, editor->value && !crosshair);
}

/**
 * @fn void HudViewController::warm(HudViewController *self)
 * @memberof HudViewController
 */
static void warm(HudViewController *self) {

  for (g_item_tag_t t = ITEM_NONE + 1; t < ITEM_TOTAL; t++) {
    if (bg_item_defs[t].icon) {
      $(self, image, bg_item_defs[t].icon);
    }
  }

  const char *pics[] = {
    "pics/health_large", "pics/health_medium", "pics/health", "pics/health_mega"
  };

  for (size_t i = 0; i < lengthof(pics); i++) {
    $(self, image, pics[i]);
  }

  Theme *theme = cgi.Theme();
  if (theme && self->atlasDirty) {
    self->atlasDirty = false;

    if (!$($(theme, icons), compile)) {
      Cg_Warn("Failed to compile the icon atlas\n");
    }
  }
}

/**
 * @fn void HudViewController::updateWithFrame(HudViewController *self, const cl_frame_t *frame)
 * @memberof HudViewController
 */
static void updateWithFrame(HudViewController *self, const cl_frame_t *frame) {

  assert(frame);

  if (cg_hud->modified) {
    cg_hud->modified = false;
    $(self, reload);
  }

  const player_state_t *ps = &frame->ps;

  $((View *) self->navEdit, updateBindings, (ident) frame);
  $((View *) self->notify, updateBindings, (ident) frame);
  $((View *) self->chat, updateBindings, (ident) frame);

  // The scoreboard outlives the HUD: it shows through the intermission, and with the HUD off.
  // Only what shows takes the frame, since some elements trace the world to fill themselves in.
  const bool scores = ps->stats[STAT_SCORES] && !cg_state.nav_edit;

  $((View *) self->scoreboard, setHidden, !scores);

  if (scores) {
    $((View *) self->scoreboard, updateBindings, (ident) frame);
  }

  const bool hidden = !cg_draw_hud->integer || !ps->stats[STAT_TIME] || cg_state.nav_edit;

  if (self->hud) {
    $(self->hud, setHidden, hidden);

    if (!hidden) {
      $(self->hud, enumerateSubviews, hideForEditor, NULL);
      $(self->hud, updateBindings, (ident) frame);
    }
  }

  if (self->atlasDirty) {
    $(self, warm);
  }
}

#pragma mark - Class lifecycle

static void initialize(Class *clazz) {

  ((ObjectInterface *) clazz->interface)->dealloc = dealloc;

  ((ViewControllerInterface *) clazz->interface)->init = init;
  ((ViewControllerInterface *) clazz->interface)->loadView = loadView;

  ((HudViewControllerInterface *) clazz->interface)->image = image;
  ((HudViewControllerInterface *) clazz->interface)->reload = reload;
  ((HudViewControllerInterface *) clazz->interface)->updateWithFrame = updateWithFrame;
  ((HudViewControllerInterface *) clazz->interface)->warm = warm;
}

/**
 * @fn Class *HudViewController::_HudViewController(void)
 * @memberof HudViewController
 */
Class *_HudViewController(void) {
  static Class *clazz;
  static Once once;

  do_once(&once, {
    clazz = _initialize(&(const ClassDef) {
      .name = "HudViewController",
      .superclass = _ViewController(),
      .instanceSize = sizeof(HudViewController),
      .interfaceSize = sizeof(HudViewControllerInterface),
      .initialize = initialize,
    });
  });

  return clazz;
}

#undef _Class
