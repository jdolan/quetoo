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
#include "HudView.h"
#include "CrosshairView.h"

#define _Class _HudViewController

#define HUD_DEFAULT_VARIANT "classic"

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
  release(this->fonts);
  release(this->images);
  release(this->atlas);

  super(Object, self, dealloc);
}

#pragma mark - ViewController

/**
 * @see View::init(View *)
 */
static ViewController *init(ViewController *self) {

  self = super(ViewController, self, init);
  if (self) {
    HudViewController *this = (HudViewController *) self;

    this->atlas = $(alloc(ImageAtlas), init);
    assert(this->atlas);

    this->fonts = $$(Dictionary, dictionary);
    assert(this->fonts);

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

  View *view = $((View *) alloc(HudView), initWithFrame, NULL);
  assert(view);

  view->autoresizingMask = ViewAutoresizingFill;

  $(self, setView, view);
  release(view);

  $(this, reload);
}

#pragma mark - HudViewController

/**
 * @fn BitmapFont *HudViewController::bitmapFont(HudViewController *self, Font *font)
 * @memberof HudViewController
 */
static BitmapFont *bitmapFont(HudViewController *self, Font *font) {

  assert(font);

  const char *name = va("%s-%d-%d", font->family, font->size, font->style);

  Object *cached = $(self->fonts, objectForKeyPath, name);
  if (cached == NULL) {

    BitmapFont *baked = $(alloc(BitmapFont), initWithFont, font, ' ', 95, NULL, self->atlas);
    if (baked) {
      cached = (Object *) baked;
      self->atlasDirty = true;
    } else {
      Cg_Debug("%s is not fixed-width; HUD text using it renders through Font\n", name);
      cached = (Object *) $$(Null, null);
    }

    $(self->fonts, setObjectForKeyPath, cached, name);
    release(baked);
  }

  return $(cached, isKindOfClass, _BitmapFont()) ? (BitmapFont *) cached : NULL;
}

/**
 * @fn AtlasImage *HudViewController::image(HudViewController *self, const char *name)
 * @memberof HudViewController
 */
static AtlasImage *image(HudViewController *self, const char *name) {

  assert(name);

  Object *cached = $(self->images, objectForKeyPath, name);
  if (cached == NULL) {

    SDL_Surface *surface = cgi.LoadSurface(name);
    if (surface) {
      Image *loaded = $$(Image, imageWithSurface, surface);
      SDL_DestroySurface(surface);

      cached = (Object *) $(self->atlas, addImage, loaded);
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
 * @brief Loads the named variant's View and Stylesheet, or `NULL` if either is missing.
 */
static View *loadVariant(const char *variant) {

  View *view = $$(View, viewWithResourceName, va("ui/hud/%s.json", variant), NULL);
  if (view == NULL) {
    Cg_Warn("Failed to load ui/hud/%s.json\n", variant);
    return NULL;
  }

  view->stylesheet = $$(Stylesheet, stylesheetWithResourceName, va("ui/hud/%s.css", variant));
  if (view->stylesheet == NULL) {
    Cg_Warn("Failed to load ui/hud/%s.css\n", variant);
    release(view);
    return NULL;
  }

  return view;
}

/**
 * @fn void HudViewController::reload(HudViewController *self)
 * @memberof HudViewController
 */
static void reload(HudViewController *self) {

  if (self->hud) {
    $(self->hud, removeFromSuperview);
    self->hud = release(self->hud);
  }

  View *hud = loadVariant(cg_hud->string);
  if (hud == NULL && q_strcmp(cg_hud->string, HUD_DEFAULT_VARIANT)) {
    Cg_Warn("Falling back to the %s HUD\n", HUD_DEFAULT_VARIANT);
    hud = loadVariant(HUD_DEFAULT_VARIANT);
  }

  if (hud == NULL) {
    Cg_Warn("No HUD\n");
    return;
  }

  hud->autoresizingMask = ViewAutoresizingFill;

  Cg_ConfigureHud(hud);

  $(self->viewController.view, addSubview, hud);
  self->hud = hud;

  $(self->viewController.view, updateBindings, NULL);
}

/**
 * @fn void HudViewController::resetMedia(HudViewController *self)
 * @memberof HudViewController
 */
static void resetMedia(HudViewController *self) {

  $(self->fonts, removeAllObjects);
  $(self->images, removeAllObjects);

  release(self->atlas);
  self->atlas = $(alloc(ImageAtlas), init);
  assert(self->atlas);

  self->atlasDirty = false;
}

/**
 * @brief ViewEnumerator for updateWithFrame: points every Text at the BitmapFont baked from
 * the Font its style resolved, so that variants choose faces and sizes in CSS.
 */
static void bindBitmapFont(View *view, ident data) {

  if ($((Object *) view, isKindOfClass, _Text())) {
    Text *text = (Text *) view;

    if (text->font && (text->bitmapFont == NULL || text->bitmapFont->font != text->font)) {
      BitmapFont *bitmapFont = $((HudViewController *) data, bitmapFont, text->font);
      $(text, setBitmapFont, bitmapFont);
    }
  }
}

/**
 * @brief ViewEnumerator for updateWithFrame: in the editor, only the crosshair shows. Runs
 * before the hierarchy updates, so an element that hides itself still can.
 */
static void hideForEditor(View *view, ident data) {
  const bool crosshair = $((Object *) view, isKindOfClass, _CrosshairView());
  $(view, setHidden, editor->value && !crosshair);
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

  View *view = self->viewController.view;

  const player_state_t *ps = &frame->ps;

  const bool hidden = !cg_draw_hud->integer || !ps->stats[STAT_TIME] || cg_state.nav_edit;

  $(view, setHidden, hidden);

  if (hidden || self->hud == NULL) {
    return;
  }

  $(self->hud, enumerateSubviews, hideForEditor, NULL);

  $(view, updateBindings, (ident) frame);

  $(view, enumerateDescendants, bindBitmapFont, self);

  if (self->atlasDirty) {
    self->atlasDirty = false;

    if (!$(self->atlas, compile)) {
      Cg_Warn("Failed to compile the HUD atlas\n");
    }
  }
}

#pragma mark - Class lifecycle

static void initialize(Class *clazz) {

  ((ObjectInterface *) clazz->interface)->dealloc = dealloc;

  ((ViewControllerInterface *) clazz->interface)->init = init;
  ((ViewControllerInterface *) clazz->interface)->loadView = loadView;

  ((HudViewControllerInterface *) clazz->interface)->bitmapFont = bitmapFont;
  ((HudViewControllerInterface *) clazz->interface)->image = image;
  ((HudViewControllerInterface *) clazz->interface)->reload = reload;
  ((HudViewControllerInterface *) clazz->interface)->resetMedia = resetMedia;
  ((HudViewControllerInterface *) clazz->interface)->updateWithFrame = updateWithFrame;
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
