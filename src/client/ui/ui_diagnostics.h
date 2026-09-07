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

#include <ObjectivelyMVC/ViewController.h>

#include "shared/shared.h"

/**
 * @file
 * @brief The diagnostics layer: counters, the net graph, and the renderer and sound stats.
 */

typedef struct DiagnosticsViewController DiagnosticsViewController;
typedef struct DiagnosticsViewControllerInterface DiagnosticsViewControllerInterface;

/**
 * @brief The diagnostics layer, above the HUD and the menus: the counters and net graph in the
 * bottom right, the renderer and sound stats down the left, each behind its cvar.
 * @details Everything here changes every frame, so it is set in the bitmap monospace.
 * @extends ViewController
 */
struct DiagnosticsViewController {

  /**
   * @brief The superclass.
   */
  ViewController viewController;

  /**
   * @brief The interface type.
   * @protected
   */
  DiagnosticsViewControllerInterface *interface[0];

  /**
   * @brief The net graph.
   */
  View *netGraph;

  /**
   * @brief The speed, frame and packet counters.
   */
  Text *counters;

  /**
   * @brief The stats column down the left, below the console when it is open.
   */
  View *stats;

  /**
   * @brief The renderer stats.
   */
  Text *rendererStats;

  /**
   * @brief The sound stats.
   */
  Text *soundStats;
};

struct DiagnosticsViewControllerInterface {

  /**
   * @brief The superclass interface.
   */
  ViewControllerInterface viewControllerInterface;

  /**
   * @fn void DiagnosticsViewController::update(DiagnosticsViewController *self)
   * @brief Updates the diagnostics for the frame about to be drawn.
   * @param self The DiagnosticsViewController.
   * @memberof DiagnosticsViewController
   */
  void (*update)(DiagnosticsViewController *self);
};

Class *_DiagnosticsViewController(void);

/**
 * @brief Accumulates a net graph sample.
 * @param value The sample, `0` to `1`.
 * @param color The sample color.
 */
void Ui_AddNetGraphSample(float value, const color_t color);
