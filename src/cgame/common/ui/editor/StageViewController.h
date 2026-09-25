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
 * Foundation, Inc., 59 Temple Place - Suite 330, Boston, MA 02111-1307, USA.
 */

#pragma once

#include "cgame/cgame.h"

#include <ObjectivelyMVC/ViewController.h>

/**
 * @file
 *
 * @brief The StageViewController.
 */

typedef struct StageViewController StageViewController;
typedef struct StageViewControllerInterface StageViewControllerInterface;

/**
 * @brief The StageViewController type.
 * @extends ViewController
 * @ingroup ViewControllers
 */
struct StageViewController {

  /**
   * @brief The superclass.
   */
  ViewController viewController;

  /**
   * @brief The interface type. @private
   */
  StageViewControllerInterface *interface[0];

  /**
   * @brief The material whose stages are edited.
   */
  RenderMaterial *material;

  /**
   * @brief The stage being edited, or `NULL`.
   */
  CmStage *stage;

  /**
   * @brief The stage selection.
   */
  Select *stages;

  /**
   * @brief The button that appends a stage.
   */
  Button *addStage;

  /**
   * @brief The button that removes the selected stage.
   */
  Button *removeStage;

  /**
   * @brief The stage texture name text field.
   */
  TextView *stageTexture;

  /**
   * @brief The stage blend source factor selection.
   */
  Select *stageBlendSrc;

  /**
   * @brief The stage blend destination factor selection.
   */
  Select *stageBlendDest;
};

/**
 * @brief The StageViewController interface.
 */
struct StageViewControllerInterface {

  /**
   * @brief The superclass interface.
   */
  ViewControllerInterface viewControllerInterface;

  /**
   * @fn StageViewController *StageViewController::init(StageViewController *self)
   * @brief Initializes this StageViewController.
   * @param self The StageViewController.
   * @return The initialized StageViewController, or `NULL` on error.
   * @memberof StageViewController
   */
  StageViewController *(*init)(StageViewController *self);

  /**
   * @fn void StageViewController::setMaterial(StageViewController *self, RenderMaterial *material)
   * @brief Sets the material whose stages to edit.
   * @param self The StageViewController.
   * @param material The material to edit.
   * @memberof StageViewController
   */
  void (*setMaterial)(StageViewController *self, RenderMaterial *material);
};

/**
 * @fn Class *StageViewController::_StageViewController(void)
 * @brief The StageViewController archetype.
 * @return The StageViewController Class.
 * @memberof StageViewController
 */
extern Class *_StageViewController(void);
