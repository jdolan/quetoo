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

#pragma once

#include <ObjectivelyMVC/AtlasImage.h>
#include <ObjectivelyMVC/ViewController.h>

#include "ChatView.h"
#include "DiagnosticsView.h"
#include "NavEditView.h"
#include "NotifyView.h"
#include "ScoreboardView.h"

#include "cg_types.h"

/**
 * @file
 * @brief The in-game HUD: a View hierarchy loaded from `ui/hud/<cg_hud>.json`, plus the
 * scoreboard, handed each frame through View::updateBindings and drawn beneath the menus by
 * the client.
 */

typedef struct HudViewController HudViewController;
typedef struct HudViewControllerInterface HudViewControllerInterface;

/**
 * @brief The HUD: the variant's View tree beneath the scoreboard, the notify lines, the chat
 * and the nav edit card, all fed the frame each Cg_UpdateScreen.
 * @extends ViewController
 */
struct HudViewController {

  /**
   * @brief The superclass.
   */
  ViewController viewController;

  /**
   * @brief The interface type.
   * @protected
   */
  HudViewControllerInterface *interface[0];

  /**
   * @brief Whether the Theme's icon atlas has HUD images added since it was last compiled,
   * which happens only when a variant asks for one HudViewController::warm did not.
   */
  bool atlasDirty;

  /**
   * @brief The View loaded from the variant's JSON, a subview of `view`.
   */
  View *hud;

  /**
   * @brief The scoreboard, a subview of `view` above `hud`, and not part of the variant.
   */
  ScoreboardView *scoreboard;

  /**
   * @brief The navigation edit instructions, shown in place of `hud` while editing.
   */
  NavEditView *navEdit;

  /**
   * @brief The notify lines and the chat, siblings of `hud` so that they outlive it through the
   * intermission and with the HUD off, as the scoreboard does.
   */
  NotifyView *notify;
  ChatView *chat;

  /**
   * @brief The diagnostics table, added to each variant's layout and shown while
   * `cg_draw_diagnostics` is set.
   */
  DiagnosticsView *diagnostics;

  /**
   * @brief AtlasImages by resource name.
   */
  Dictionary *images;
};

struct HudViewControllerInterface {

  /**
   * @brief The superclass interface.
   */
  ViewControllerInterface viewControllerInterface;

  /**
   * @fn AtlasImage *HudViewController::image(HudViewController *self, const char *name)
   * @brief Resolves the image by the given resource name from the Theme's icon atlas, loading
   * and registering it on first request.
   * @details HUD images share the sheet with the `:icon:` escapes, so consecutive draws of
   * either merge. They are registered under their resource names, which a slash keeps from
   * ever matching an escape.
   * @param self The HudViewController.
   * @param name The image name, e.g. `pics/health`.
   * @return The AtlasImage, owned by the Theme's icon atlas, or `NULL` if the image was not
   * found.
   * @memberof HudViewController
   */
  AtlasImage *(*image)(HudViewController *self, const char *name);

  /**
   * @fn void HudViewController::reload(HudViewController *self)
   * @brief Loads the variant named by `cg_hud`, falling back to `classic`. A module arranging
   * its HUD differently ships its own `ui/hud/<variant>.json` in its game directory.
   * @param self The HudViewController.
   * @memberof HudViewController
   */
  void (*reload)(HudViewController *self);

  /**
   * @fn void HudViewController::updateWithFrame(HudViewController *self, const cl_frame_t *frame)
   * @brief Resolves visibility, hands `frame` to the View hierarchy, and compiles the Theme's
   * icon atlas if a variant added to it. Called once per frame, before the client draws.
   * @param self The HudViewController.
   * @param frame The frame.
   * @memberof HudViewController
   */
  void (*updateWithFrame)(HudViewController *self, const cl_frame_t *frame);

  /**
   * @fn void HudViewController::warm(HudViewController *self)
   * @brief Loads every item, health and weapon bar image and compiles the Theme's icon atlas
   * once, so that play does not repack the sheet as items are first seen.
   * @param self The HudViewController.
   * @memberof HudViewController
   */
  void (*warm)(HudViewController *self);
};

CGAME_EXPORT Class *_HudViewController(void);

/**
 * @brief The HudViewController, or `NULL` when the HUD is not loaded.
 */
extern HudViewController *cg_hud_view_controller;

/**
 * @return The AtlasImage for the given resource name from the Theme's icon atlas, or `NULL`.
 * @see HudViewController::image
 */
AtlasImage *Cg_HudImage(const char *name);
