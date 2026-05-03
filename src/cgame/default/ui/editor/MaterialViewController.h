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

  /**
   * @brief The superclass.
   */
  ViewController viewController;

  /**
   * @brief The interface. @private
   */
  MaterialViewControllerInterface *interface;

  /**
   * @brief The material being edited.
   */
  r_material_t *material;

  /**
   * @brief The material name text field.
   */
  TextView *name;

  /**
   * @brief The diffusemap texture name text field.
   */
  TextView *diffusemap;

  /**
   * @brief The normalmap texture name text field.
   */
  TextView *normalmap;

  /**
   * @brief The specularmap texture name text field.
   */
  TextView *specularmap;

  /**
   * @brief The roughness slider.
   */
  Slider *roughness;

  /**
   * @brief The hardness slider.
   */
  Slider *hardness;

  /**
   * @brief The specularity slider.
   */
  Slider *specularity;

  /**
   * @brief The parallax amplitude slider.
   */
  Slider *parallax;

  /**
   * @brief The shadow amplitude slider.
   */
  Slider *shadow;

  /**
   * @brief The alpha test threshold slider.
   */
  Slider *alphaTest;
};

/**
 * @brief The MaterialViewController interface.
 */
struct MaterialViewControllerInterface {

  /**
   * @brief The superclass interface.
   */
  ViewControllerInterface viewControllerInterface;

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
