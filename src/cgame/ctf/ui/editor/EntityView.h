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
#include "../../cg_editor.h"

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
   * @brief The delegate self-reference.
   */
  ident self;

  /**
   * @brief Callback invoked when the entity is edited.
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
   * @brief The interface. @protected
   */
  EntityViewInterface *interface;

  /**
   * @brief The EntityViewDelegate.
   */
  EntityViewDelegate delegate;

  /**
   * @brief The editor entity. Pointer into the stable `cg_editor.entities[]` array.
   */
  cg_editor_entity_t *edit;

  /**
   * @brief The specific key-value pair being edited by this view, or `NULL` for a new pair.
   */
  cm_entity_t *pair;

  /**
   * @brief The entity key text field.
   */
  TextView *key;

  /**
   * @brief The entity value text field.
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
   * @fn EntityView *EntityView::initWithEntity(EntityView *self, cg_editor_entity_t *edit, cm_entity_t *pair)
   * @brief Initializes this EntityView.
   * @param self The EntityView.
   * @param edit The editor entity (pointer into `cg_editor.entities[]`).
   * @param pair The key-value pair being edited, or `NULL` for a new pair.
   * @return The initialized EntityView, or `NULL` on error.
   * @memberof EntityView
   */
  EntityView *(*initWithEntity)(EntityView *self, cg_editor_entity_t *edit, cm_entity_t *pair);

  /**
   * @fn void EntityView::setEntity(EntityView *self, cg_editor_entity_t *edit, cm_entity_t *pair)
   * @brief Sets the entity and key-value pair to be edited.
   * @param self The EntityView.
   * @param edit The editor entity (pointer into `cg_editor.entities[]`).
   * @param pair The key-value pair being edited, or `NULL` for a new pair.
   * @memberof EntityView
   */
  void (*setEntity)(EntityView *self, cg_editor_entity_t *edit, cm_entity_t *pair);
};

/**
 * @fn Class *EntityView::_EntityView(void)
 * @brief The EntityView archetype.
 * @return The EntityView Class.
 * @memberof EntityView
 */
CGAME_EXPORT Class *_EntityView(void);
