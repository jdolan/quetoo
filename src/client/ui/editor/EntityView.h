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
#include "cl_types.h"

#include <ObjectivelyMVC/StackView.h>

typedef struct EntityView EntityView;
typedef struct EntityViewInterface EntityViewInterface;

/**
 * @file
 * @brief A View for editing `cm_entity_t`.
 */

/**
 * @brief The Editor entity type.
 */
typedef struct {
  /**
   * @brief The entity number.
   */
  int16_t number;

  /**
   * @brief The client entity.
   */
  cl_entity_t *ent;

  /**
   * @brief The entity definition.
   */
  cm_entity_t *def;
} EditorEntity;

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
  void (*didEditEntity)(EntityView *view, cm_entity_t *def);

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
  EditorEntity entity;

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
   * @fn EntityView *EntityView::init(EntityView *self, cm_entity_t *EditorEntity)
   * @brief Initializes this EntityView.
   * @param self The EntityView.
   * @param entity The entity.
   * @return The initialized EntityView, or `NULL` on error.
   * @memberof EntityView
   */
  EntityView *(*initWithEntity)(EntityView *self, EditorEntity *entity);

  /**
   * @fn void EntityView::setEntity(EntityView *self, EditorEntity *entity)
   * @brief Sets the entity to be edited.
   * @param self The EntityView.
   * @param entity The entity.
   * @memberof EntityView
   */
  void (*setEntity)(EntityView *self, EditorEntity *entity);
};

/**
 * @fn Class *EntityView::_EntityView(void)
 * @brief The EntityView archetype.
 * @return The EntityView Class.
 * @memberof EntityView
 */
//OBJECTIVELYMVC_EXPORT Class *_EntityView(void);
__declspec(dllexport) Class* _EntityView(void);