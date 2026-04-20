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

#include "ui/main/MainViewController.h"
#include "ui/main/LoadingViewController.h"
#include "ui/main/UpdateViewController.h"

static MainViewController *mainViewController;
static UpdateViewController *updateViewController;
static Stylesheet *stylesheet;

/**
 * @brief Initializes the user interface.
 */
void Cg_InitUi(void) {

  stylesheet = $$(Stylesheet, stylesheetWithResourceName, "ui/common/common.css");
  assert(stylesheet);

  $(cgi.Theme(), addStylesheet, stylesheet);

  mainViewController = $(alloc(MainViewController), init);
  assert(mainViewController);

  cgi.PushViewController((ViewController *) mainViewController);

  cgi.Update();
}

/**
 * @brief Shuts down the user interface.
 */
void Cg_ShutdownUi(void) {

  cgi.PopAllViewControllers();

  release(updateViewController);
  updateViewController = NULL;

  release(mainViewController);
  mainViewController = NULL;

  $(cgi.Theme(), removeStylesheet, stylesheet);

  release(stylesheet);
  stylesheet = NULL;
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
void Cg_UpdateInstaller(const installer_status_t status) {

  if (mainViewController == NULL) {
    return;
  }

  if (updateViewController == NULL) {
    if (status.state == INSTALLER_IDLE ||
        status.state == INSTALLER_DONE ||
        status.state == INSTALLER_ERROR) {
      return;
    }
    updateViewController = $(alloc(UpdateViewController), init);
    cgi.PushViewController((ViewController *) updateViewController);
  }

  $(updateViewController, setStatus, status);

  if (status.state == INSTALLER_DONE || status.state == INSTALLER_ERROR) {
    static uint64_t done_at = 0;
    if (done_at == 0) {
      done_at = SDL_GetTicks();
    }
    if (SDL_GetTicks() - done_at > 2000) {
      cgi.PopViewController();
      release(updateViewController);
      updateViewController = NULL;
      done_at = 0;
    }
  }
}

/**
 * @brief Inlet binding for `cvar_t *`.
 */
void Cg_BindCvar(const Inlet *inlet, ident obj) {

  const char *name = cast(String, obj)->chars;
  cvar_t *var = cgi.GetCvar(name);

  if (var == NULL) {
    Cg_Warn("%s not found\n", name);
  }

  *(cvar_t **) inlet->dest = var;
}
