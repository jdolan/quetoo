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

#include "ui_types.h"

#include <ObjectivelyMVC/ViewController.h>

/**
 * @file
 *
 * @brief The MaterialViewController.
 */

typedef struct MaterialViewController MaterialViewController;
typedef struct MaterialViewControllerInterface MaterialViewControllerInterface;

/**
 * @brief The MaterialViewController type.
 * @extends ViewController
 * @ingroup ViewControllers
 */
struct MaterialViewController {

  ViewController viewController;              ///< The superclass.
  MaterialViewControllerInterface *interface; ///< The interface. @private

  r_material_t *material;                     ///< The material being edited.
  TextView *name;                             ///< The material name text field.
  TextView *diffusemap;                       ///< The diffusemap texture name text field.
  TextView *normalmap;                        ///< The normalmap texture name text field.
  TextView *specularmap;                      ///< The specularmap texture name text field.
  Slider *roughness;                          ///< The roughness slider.
  Slider *hardness;                           ///< The hardness slider.
  Slider *specularity;                        ///< The specularity slider.
  Slider *parallax;                           ///< The parallax amplitude slider.
  Slider *shadow;                             ///< The shadow amplitude slider.
  Slider *alphaTest;                          ///< The alpha test threshold slider.
};

/**
 * @brief The MaterialViewController interface.
 */
struct MaterialViewControllerInterface {

  ViewControllerInterface viewControllerInterface; ///< The superclass interface.

  /**
   * @fn MaterialViewController *MaterialViewController::init(MaterialViewController *self)
   * @brief Initializes this MaterialViewController.
   * @param self The MaterialViewController.
   * @return The initialized MaterialViewController, or `NULL` on error.
   * @memberof MaterialViewController
   */
  MaterialViewController *(*init)(MaterialViewController *self);

  /**
   * @fn void MaterialViewController::setMaterial(MaterialViewController *self, r_material_t *material)
   * @brief Sets the material to edit.
   * @param self The MaterialViewController.
   * @param material The material to edit.
   * @memberof MaterialViewController
   */
  void (*setMaterial)(MaterialViewController *self, r_material_t *material);
};

/**
 * @fn Class *MaterialViewController::_MaterialViewController(void)
 * @brief The MaterialViewController archetype.
 * @return The MaterialViewController Class.
 * @memberof MaterialViewController
 */
extern Class *_MaterialViewController(void);
