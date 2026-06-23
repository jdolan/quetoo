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

#include "EntityViewController.h"
#include "MaterialViewController.h"
#include "MeshViewController.h"

#include <ObjectivelyMVC.h>

/**
 * @file
 * @brief The EditorViewController.
 */

typedef struct EditorViewController EditorViewController;
typedef struct EditorViewControllerInterface EditorViewControllerInterface;

/**
 * @brief The EditorViewController type.
 * @extends ViewController
 * @ingroup
 */
struct EditorViewController {

  /**
   * @brief The superclass.
   */
  ViewController viewController;

  /**
   * @brief The interface. @private
   */
  EditorViewControllerInterface *interface;

  /**
   * @brief The top menu strip (Editor / Settings / Controls / Disable / Quit).
   */
  StackView *menu;

  /**
   * @brief The content area below the menu strip; hosts the editor panel and,
   * when a strip tab other than Editor is active, the Settings/Controls view.
   */
  View *content;

  /**
   * @brief The docked editor Panel (entity/material/mesh tabs).
   */
  Panel *panel;

  /**
   * @brief The currently presented strip sub-controller (Settings/Controls), or
   * NULL when the Editor tab is active and the panel is shown.
   */
  ViewController *contentViewController;

  /**
   * @brief TabViewController containing the editor tabs.
   */
  TabViewController *tabViewController;

  /**
   * @brief The EntityViewController.
   */
  EntityViewController *entityViewController;

  /**
   * @brief The MaterialViewController.
   */
  MaterialViewController *materialViewController;

  /**
   * @brief The MeshViewController.
   */
  MeshViewController *meshViewController;

  /**
   * @brief The Create Entity button.
   */
  Button *createEntity;

  /**
   * @brief The Delete Entity button.
   */
  Button *deleteEntity;

  /**
   * @brief The Save button.
   */
  Button *save;
};

/**
 * @brief The EditorViewController interface.
 */
struct EditorViewControllerInterface {

  /**
   * @brief The superclass interface.
   */
  ViewControllerInterface viewControllerInterface;

  /**
   * @fn void EditorViewController::showEditorTab(EditorViewController *self)
   * @brief Switches the menu strip back to the Editor tab (re-shows the editor
   * panel, tearing down any Settings/Controls sub-view).
   * @memberof EditorViewController
   */
  void (*showEditorTab)(EditorViewController *self);

  /**
   * @fn void EditorViewController::fitContentHeight(EditorViewController *self)
   * @brief Stretches the docked panel to fill the viewport below the menu strip
   * by pinning its content area's minimum height to the available space.
   * @memberof EditorViewController
   */
  void (*fitContentHeight)(EditorViewController *self);
};

/**
 * @fn Class *EditorViewController::_EditorViewController(void)
 * @brief The EditorViewController archetype.
 * @return The EditorViewController Class.
 * @memberof EditorViewController
 */
extern Class *_EditorViewController(void);
