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

#include "ui/DialogViewController.h"
#include "ui/main/MainViewController.h"
#include "ui/main/LoadingViewController.h"
#include "ui/main/UpdateViewController.h"
#include "ui/hud/HudViewController.h"

static MainViewController *mainViewController;
static UpdateViewController *updateViewController;
static Stylesheet *stylesheet;

Image *Cg_LoadImageScaled(const char *name, float scale) {

  char path[MAX_OS_PATH];
  snprintf(path, sizeof(path), "%s.svg", name);

  void *svg;
  const int64_t length = cgi.LoadFile(path, &svg);
  if (length > 0) {
    const float density = SDL_GetWindowPixelDensity(cgi.context->window) * scale;

    Image *image = $$(Image, imageWithSVG, svg, (size_t) length, density);
    cgi.FreeFile(svg);

    if (image) {
      return image;
    }
  }

  SDL_Surface *surface = cgi.LoadSurface(name);
  if (surface) {
    Image *image = $$(Image, imageWithSurface, surface);
    SDL_DestroySurface(surface);
    return image;
  }

  return NULL;
}

Image *Cg_LoadImage(const char *name) {
  return Cg_LoadImageScaled(name, 1.f);
}

/**
 * @brief `Fs_Enumerator` registering one emoji with the Theme's icon atlas, so that `:name:`
 * in any Text draws it inline.
 */
static void Cg_AddEmoji(const char *path, void *data) {

  char name[MAX_OS_PATH];
  StripExtension(Basename(path), name);

  char resource[MAX_OS_PATH];
  StripExtension(path, resource);

  Image *image = Cg_LoadImage(resource);
  if (image) {
    $((ImageAtlas *) data, addImageWithName, name, image);
    release(image);
  } else {
    Cg_Warn("Failed to load %s\n", path);
  }
}

/**
 * @brief Registers a TTF from the game filesystem with MVC under the given family, so that
 * stylesheets can name it.
 */
static void Cg_CacheFont(const char *path, const char *family) {

  void *ttf;
  const int64_t length = cgi.LoadFile(path, &ttf);
  if (length > 0) {
    Data *data = $(alloc(Data), initWithBytes, ttf, (size_t) length);
    assert(data);

    $$(Font, cacheFont, data, family);

    release(data);
    cgi.FreeFile(ttf);
  } else {
    Cg_Warn("Failed to load %s\n", path);
  }
}

void Cg_InitUi(void) {

  // The HUD's faces: M PLUS U for numbers, tabular digits and an unmarked zero; Barlow
  // Condensed for captions; Rajdhani for variants that want the concept mockups' numerals
  Cg_CacheFont("ui/fonts/MPlusU-Bold.ttf", "M PLUS U");
  Cg_CacheFont("ui/fonts/BarlowCondensed-SemiBold.ttf", "Barlow Condensed");
  Cg_CacheFont("ui/fonts/Rajdhani-Bold.ttf", "Rajdhani");

  stylesheet = $$(Stylesheet, stylesheetWithResourceName, "ui/common.css");
  assert(stylesheet);

  Theme *theme = cgi.Theme();
  assert(theme);

  $(theme, addStylesheet, stylesheet);

  // Registered directly rather than through Theme::addIcon, which would repack the atlas
  // once per emoji; one compile covers them all
  ImageAtlas *icons = $(theme, icons);
  cgi.EnumerateFiles("pics/emoji/*", Cg_AddEmoji, icons);

  if (!$(icons, compile)) {
    Cg_Warn("Failed to compile the icon atlas\n");
  }

  mainViewController = $(alloc(MainViewController), init);
  assert(mainViewController);

  cgi.PushViewController((ViewController *) mainViewController);
}

void Cg_InitHudUi(void) {

  cg_hud_view_controller = (HudViewController *) $((ViewController *) alloc(HudViewController), init);
  assert(cg_hud_view_controller);

  cgi.SetHudViewController((ViewController *) cg_hud_view_controller);
}

/**
 * @brief Shuts down the user interface.
 */
void Cg_ShutdownUi(void) {

  cgi.PopAllViewControllers();
  cgi.PopViewController();

  cgi.SetHudViewController(NULL);
  release(cg_hud_view_controller);

  $(cgi.Theme(), removeStylesheet, stylesheet);

  updateViewController = release(updateViewController);
  mainViewController = release(mainViewController);
  stylesheet = release(stylesheet);
}

/**
 * @brief Pops to the main view controller.
 */
void Cg_ClearUi(void) {

  if (mainViewController) {
    cgi.PopToViewController((ViewController *) mainViewController);
  }
}

/**
 * @brief Updates the loading screen
 */
void Cg_UpdateLoading(const cl_loading_t loading) {
  static LoadingViewController *loadingViewController;

  if (loading.percent == 0) {
    loadingViewController = $(alloc(LoadingViewController), init);
    cgi.PushViewController((ViewController *) loadingViewController);
  } else if (loading.percent == 100) {
    cgi.PopToViewController((ViewController *) mainViewController);
    loadingViewController = release(loadingViewController);
  }

  if (loadingViewController) {
    $(loadingViewController, setProgress, loading);
  }
}

/**
 * @brief Manages the UpdateViewController lifecycle and routes installer progress to it.
 * Pushes UpdateViewController when updating, pops it on completion.
 */
/**
 * @brief Dialog callbacks answering whether to install an available update.
 */
static void Cg_AcceptUpdate(ident data) {
  cgi.ConsentToUpdate(true);
}

static void Cg_DeclineUpdate(ident data) {
  cgi.ConsentToUpdate(false);
}

int32_t Cg_UpdateInstaller(const installer_status_t *in) {

  if (updateViewController == NULL) {
    updateViewController = $(alloc(UpdateViewController), init);
    cgi.PushViewController((ViewController *) updateViewController);
  }

  $(updateViewController, setStatus, in);

  if (in->state == INSTALLER_UPDATE_AVAILABLE) {

    ViewController *this = (ViewController *) updateViewController;

    const Array *children = (Array *) this->childViewControllers;
    for (size_t i = 0; i < children->count; i++) {
      if ($((Object *) children->elements[i], isKindOfClass, _DialogViewController())) {
        return 0;
      }
    }

    ViewController *dialog = (ViewController *) $(alloc(DialogViewController), initWithDialog, &(const Dialog) {
      .message = va("Quetoo %s is available. Install it?", in->current_file),
      .ok = "Install",
      .cancel = "Not now",
      .okFunction = Cg_AcceptUpdate,
      .cancelFunction = Cg_DeclineUpdate
    });

    $(this, addChildViewController, dialog);
    release(dialog);

    return 0;
  }

  if (in->state == INSTALLER_DONE || in->state == INSTALLER_ERROR) {
    static uint64_t done_at = 0;
    if (done_at == 0) {
      done_at = SDL_GetTicks();
    }
    if (SDL_GetTicks() - done_at > 2000) {
      cgi.PopViewController();
      release(updateViewController);
      updateViewController = NULL;
      done_at = 0;
      return 1;
    }
  }

  return 0;
}

/**
 * @brief Inlet binding for `cvar_t *`.
 */
void Cg_BindCvar(const Inlet *inlet, ident obj) {

  const char *name = cast(String, obj)->chars;
  cvar_t *var = cgi.GetCvar(name);

  if (var == NULL) {
    Cg_Debug("%s not found\n", name);
  }

  *(cvar_t **) inlet->dest = var;
}
