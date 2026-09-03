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

#include "LoadingViewController.h"

#define _Class _LoadingViewController

#pragma mark - ViewController

/**
 * @see ViewController::loadView(ViewController *)
 */
static void loadView(ViewController *self) {

  super(ViewController, self, loadView);

  LoadingViewController *this = (LoadingViewController *) self;

  Outlet outlets[] = MakeOutlets(
    MakeOutlet("mapShot", &this->mapShot),
    MakeOutlet("logo", &this->logo),
    MakeOutlet("mapTitle", &this->mapTitle),
    MakeOutlet("serverName", &this->serverName),
    MakeOutlet("progress", &this->progressBar)
  );

  $(self->view, awakeWithResourceName, "ui/main/LoadingViewController.json");
  $(self->view, resolve, outlets);

  self->view->stylesheet = $$(Stylesheet, stylesheetWithResourceName, "ui/main/LoadingViewController.css");
  assert(self->view->stylesheet);

  $(this->logo, setImageWithResourceName, "ui/loading.png");
  $(this->progressBar->foreground, setImageWithResourceName, "ui/progress_bar.png");
}

#pragma mark - LoadingViewController

/**
 * @fn LoadingViewController *LoadingViewController::init(LoadingViewController *self)
 * @memberof LoadingViewController
 */
static LoadingViewController *init(LoadingViewController *self) {
  return (LoadingViewController *) super(ViewController, self, init);
}

/**
 * @brief Loads, blurs and sets the named image as the backdrop.
 */
static void setMapShot(LoadingViewController *self, const char *name) {

  SDL_Surface *surf = cgi.LoadSurface(name);
  if (surf) {
    cgi.BlurSurface(surf, 3);
    $(self->mapShot, setImageWithSurface, surf);
    SDL_DestroySurface(surf);
  } else {
    $(self->mapShot, setImageWithResourceName, name);
  }
}

/**
 * @return The title of the map being loaded.
 */
static const char *resolveMapTitle(void) {

  const char *message = cgi.ConfigString(CS_MESSAGE);
  if (*message) {
    return message;
  }

  char name[MAX_STRING_CHARS];
  StripExtension(Basename(cgi.ConfigString(CS_BSP)), name);

  return va("%s", name);
}

/**
 * @return A human readable name for the server being loaded into.
 */
static const char *resolveServerName(void) {

  if (cgi.client->demo_server) {
    return "Demo playback";
  }

  if (!*cgi.server_name) {
    return "";
  }

  if (!q_strcmp(cgi.server_name, "localhost")) {
    const char *hostname = cgi.GetCvarString("sv_hostname");
    return *hostname ? hostname : "Local server";
  }

  return cgi.server_name;
}

/**
 * @fn void LoadingViewController::setProgress(LoadingViewController *self, const cl_loading_t loading)
 * @memberof LoadingViewController
 */
static void setProgress(LoadingViewController *self, const cl_loading_t loading) {

  $(self->progressBar, setLabelFormat, va("%%0.0lf%%%% (%s)", loading.status));
  $(self->progressBar, setValue, loading.percent);

  if (loading.percent == 0) {

    if (loading.mapshot[0] != '\0') {
      setMapShot(self, loading.mapshot);
    } else {
      setMapShot(self, va("ui/backgrounds/%u.png", RandomRangeu(0, 6)));
    }

    $(self->mapTitle->text, setText, resolveMapTitle());

    const char *server = resolveServerName();
    $(self->serverName->text, setText, server);
    $((View *) self->serverName, setHidden, *server == '\0');
  }
}

#pragma mark - Class lifecycle

/**
 * @see Class::initialize(Class *)
 */
static void initialize(Class *clazz) {

  ((ViewControllerInterface *) clazz->interface)->loadView = loadView;

  ((LoadingViewControllerInterface *) clazz->interface)->init = init;
  ((LoadingViewControllerInterface *) clazz->interface)->setProgress = setProgress;
}

/**
 * @fn Class *LoadingViewController::_LoadingViewController(void)
 * @memberof LoadingViewController
 */
Class *_LoadingViewController(void) {
  static Class *clazz;
  static Once once;

  do_once(&once, {
    clazz = _initialize(&(const ClassDef) {
      .name = "LoadingViewController",
      .superclass = _ViewController(),
      .instanceSize = sizeof(LoadingViewController),
      .interfaceSize = sizeof(LoadingViewControllerInterface),
      .initialize = initialize,
    });
  });

  return clazz;
}

#undef _Class
