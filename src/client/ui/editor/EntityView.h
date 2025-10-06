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

#include "ui_local.h"

#include <ObjectivelyMVC/StackView.h>

typedef struct EntityView EntityView;
typedef struct EntityViewInterface EntityViewInterface;

/**
 * @file
 * @brief A View for editing `cm_entity_t`.
 */

/**
 * @brief The EntityViewDelegate type.
 */
typedef struct EntityViewDelegate {

  /**
   * @brief The Delegate self-reference.
   */
  ident self;

  /**
   * @brief The Delegate callback.
   */
  void (*didEditEntity)(EntityView *view, const char *key, const char *value);

} EntityViewDelegate;

/**
 * @brief The EntityView type.
 * @extends StackView
 */
struct EntityView {

  /**
   * @brief The superclass.
   */
  StackView stackView;

  /**
   * @brief The interface.
   * @protected
   */
  EntityViewInterface *interface;

  /**
   * @brief The EntityViewDelegate.
   */
  EntityViewDelegate delegate;

  /**
   * @brief The entity to edit.
   */
  cm_entity_t *entity;

  /**
   * @brief The user-data, usually to link the BSP entity to a renderer struct.
   */
  ident data;

  /**
   * @brief The entity key.
   */
  TextView *key;

  /**
   * @brief The entity value.
   */
  TextView *value;
};

/**
 * @brief The EntityView interface.
 */
struct EntityViewInterface {

  /**
   * @brief The superclass interface.
   */
  StackViewInterface stackViewInterface;

  /**
   * @fn EntityView *EntityView::init(EntityView *self)
   * @brief Initializes this EntityView.
   * @param self The EntityView.
   * @return The initialized EntityView, or `NULL` on error.
   * @memberof EntityView
   */
  EntityView *(*init)(EntityView *self);

  /**
   * @fn void EntityView::setEntity(EntityView *self, cm_entity_t *entity)
   * @brief Sets the entity to be edited.
   * @param self The EntityView.
   * @param entity The entity.
   * @memberof EntityView
   */
  void (*setEntity)(EntityView *self, cm_entity_t *entity);
};

/**
 * @fn Class *EntityView::_EntityView(void)
 * @brief The EntityView archetype.
 * @return The EntityView Class.
 * @memberof EntityView
 */
OBJECTIVELY_EXPORT Class *_EntityView(void);
