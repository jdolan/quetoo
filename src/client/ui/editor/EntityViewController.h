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

#include "EntityView.h"

#include <ObjectivelyMVC/ViewController.h>

/**
 * @file
 *
 * @brief The EntityViewController.
 */

typedef struct EntityViewController EntityViewController;
typedef struct EntityViewControllerInterface EntityViewControllerInterface;

/**
 * @brief The EntityViewController type.
 * @extends ViewController
 * @ingroup ViewControllers
 */
struct EntityViewController {

  /**
   * @brief The superclass.
   * @private
   */
  ViewController viewController;

  /**
   * @brief The interface.
   * @private
   */
  EntityViewControllerInterface *interface;

  /**
   * @brief The StackView containing the pairs.
   */
  StackView *pairs;

  /**
   * @brief The EntityView reserved for adding a new pair to the current entity.
   */
  EntityView *add;

  /**
   * @brief The Button to create a new entity.
   */
  Button *create;

  /**
   * @brief The first pair of the entity being edited.
   */
  cm_entity_t *entity;
};

/**
 * @brief The EntityViewController interface.
 */
struct EntityViewControllerInterface {

  /**
   * @brief The superclass interface.
   */
  ViewControllerInterface viewControllerInterface;

  /**
   * @fn EntityViewController *EntityViewController::init(EntityViewController *self)
   * @brief Initializes this ViewController.
   * @param self The EntityViewController.
   * @return The initialized EntityViewController, or `NULL` on error.
   * @memberof EntityViewController
   */
  EntityViewController *(*init)(EntityViewController *self);

  /**
   * @fn void EntityViewController::setEntity(EntityViewController *self, cm_entity_t *entity)
   * @brief Sets the material to edit.
   * @param self The EntityViewController.
   * @param entity The entity to edit.
   * @memberof EntityViewController
   */
  void (*setEntity)(EntityViewController *self, cm_entity_t *entity);
};

/**
 * @fn Class *EntityViewController::_EntityViewController(void)
 * @brief The EntityViewController archetype.
 * @return The EntityViewController Class.
 * @memberof EntityViewController
 */
extern Class *_EntityViewController(void);
