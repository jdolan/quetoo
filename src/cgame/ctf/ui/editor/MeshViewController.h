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

#include "cgame/cgame.h"

#include <ObjectivelyMVC/ViewController.h>

/**
 * @file
 *
 * @brief The MeshViewController.
 */

typedef struct MeshViewController MeshViewController;
typedef struct MeshViewControllerInterface MeshViewControllerInterface;

/**
 * @brief The MeshViewController type.
 * @extends ViewController
 * @ingroup ViewControllers
 */
struct MeshViewController {

  /**
   * @brief The superclass.
   */
  ViewController viewController;

  /**
   * @brief The interface. @private
   */
  MeshViewControllerInterface *interface;

  /**
   * @brief The model being edited.
   */
  r_model_t *model;

  /**
   * @brief World.cfg translate text view ("x y z").
   */
  TextView *worldTranslate;

  /**
   * @brief World.cfg rotate text view ("x y z").
   */
  TextView *worldRotate;

  /**
   * @brief World.cfg scale text view ("f").
   */
  TextView *worldScale;

  /**
   * @brief Link.cfg translate text view ("x y z").
   */
  TextView *linkTranslate;

  /**
   * @brief Link.cfg rotate text view ("x y z").
   */
  TextView *linkRotate;

  /**
   * @brief Link.cfg scale text view ("f").
   */
  TextView *linkScale;

  /**
   * @brief View.cfg translate text view ("x y z").
   */
  TextView *viewTranslate;

  /**
   * @brief View.cfg rotate text view ("x y z").
   */
  TextView *viewRotate;

  /**
   * @brief View.cfg scale text view ("f").
   */
  TextView *viewScale;

  /**
   * @brief View.cfg muzzle text view ("x y z").
   */
  TextView *viewMuzzle;
};

/**
 * @brief The MeshViewController interface.
 */
struct MeshViewControllerInterface {

  /**
   * @brief The superclass interface.
   */
  ViewControllerInterface viewControllerInterface;

  /**
   * @fn MeshViewController *MeshViewController::init(MeshViewController *self)
   * @brief Initializes this MeshViewController.
   * @param self The MeshViewController.
   * @return The initialized MeshViewController, or `NULL` on error.
   * @memberof MeshViewController
   */
  MeshViewController *(*init)(MeshViewController *self);

  /**
   * @fn void MeshViewController::setModel(MeshViewController *self, r_model_t *model)
   * @brief Sets the model to edit, parsing its config files.
   * @param self The MeshViewController.
   * @param model The model to edit, or `NULL` to clear.
   * @memberof MeshViewController
   */
  void (*setModel)(MeshViewController *self, r_model_t *model);

  /**
   * @fn void MeshViewController::save(MeshViewController *self)
   * @brief Saves modified config files to the write directory.
   * @param self The MeshViewController.
   * @memberof MeshViewController
   */
  void (*save)(MeshViewController *self);
};

/**
 * @fn Class *MeshViewController::_MeshViewController(void)
 * @brief The MeshViewController archetype.
 * @return The MeshViewController Class.
 * @memberof MeshViewController
 */
extern Class *_MeshViewController(void);
