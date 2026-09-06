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

#include <ObjectivelyMVC/ImageAtlas.h>
#include <ObjectivelyMVC/ViewController.h>

#include "cg_types.h"

/**
 * @file
 * @brief The in-game HUD: a View hierarchy loaded from `ui/hud/<cg_hud>.json`, handed each
 * frame through View::updateBindings, and drawn beneath the menus by the client.
 */

typedef struct HudViewController HudViewController;
typedef struct HudViewControllerInterface HudViewControllerInterface;

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
   * @brief The atlas behind every icon, so the HUD draws in few calls.
   */
  ImageAtlas *atlas;

  /**
   * @brief Whether `atlas` has images added since it was last compiled.
   */
  bool atlasDirty;

  /**
   * @brief The View loaded from the variant's JSON, a subview of `view`.
   */
  View *hud;

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
   * @brief Resolves the image by the given resource name from the HUD atlas, loading it on
   * first request.
   * @param self The HudViewController.
   * @param name The image name, e.g. `pics/i_health`.
   * @return The AtlasImage, owned by this controller, or `NULL` if the image was not found.
   * @memberof HudViewController
   */
  AtlasImage *(*image)(HudViewController *self, const char *name);

  /**
   * @fn void HudViewController::reload(HudViewController *self)
   * @brief Loads the variant named by `cg_hud`, falling back to `classic`, and lets each
   * module configure it through Cg_ConfigureHud.
   * @param self The HudViewController.
   * @memberof HudViewController
   */
  void (*reload)(HudViewController *self);

  /**
   * @fn void HudViewController::updateWithFrame(HudViewController *self, const cl_frame_t *frame)
   * @brief Resolves visibility, hands `frame` to the View hierarchy, and compiles the atlas
   * if it grew. Called once per frame, before the client draws.
   * @param self The HudViewController.
   * @param frame The frame.
   * @memberof HudViewController
   */
  void (*updateWithFrame)(HudViewController *self, const cl_frame_t *frame);
};

CGAME_EXPORT Class *_HudViewController(void);

/**
 * @brief The HudViewController, or `NULL` when the HUD is not loaded.
 */
extern HudViewController *cg_hud_view_controller;

/**
 * @return The AtlasImage for the given resource name from the HUD atlas, or `NULL`.
 * @see HudViewController::image
 */
AtlasImage *Cg_HudImage(const char *name);
