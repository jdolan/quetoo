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

#include "KeyValueView.h"

typedef struct EntityView EntityView;
typedef struct EntityViewInterface EntityViewInterface;

/**
 * @file
 * @brief An editable key->value row for a `cm_entity_t`, with aligned columns.
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
 * @brief An editable entity key->value row.
 * @extends KeyValueView
 * @details The key and value TextViews are the inherited KeyValueView::key and
 * KeyValueView::value (cast to TextView); column widths are owned by the parent
 * KeyValueTableView. This type adds only the entity-editing behavior.
 */
struct EntityView {

  /**
   * @brief The superclass.
   */
  KeyValueView keyValueView;

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
   * @brief When non-empty, the value field is validated live as an asset path:
   * `validatePrefix` + value is resolved in `validateContext`, tinting the field
   * when it does not exist. Empty disables validation (the default). Enable via
   * EntityView::setTextureValidation.
   */
  char validatePrefix[MAX_QPATH];

  /**
   * @brief The asset context used when `validatePrefix` is set.
   */
  cm_asset_context_t validateContext;
};

/**
 * @brief The EntityView interface.
 */
struct EntityViewInterface {

  /**
   * @brief The superclass interface.
   */
  KeyValueViewInterface keyValueViewInterface;

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

  /**
   * @fn void EntityView::setTextureValidation(EntityView *self, const char *prefix, cm_asset_context_t context)
   * @brief Enables live asset-path validation of the value field: `prefix` + value
   * is resolved in `context`, tinting the field maroon when it does not exist.
   * @param self The EntityView.
   * @param prefix Path prefix prepended to the value (e.g. "sky/"), or NULL/empty to disable.
   * @param context The asset context (e.g. `ASSET_CONTEXT_NONE` for a full path).
   * @memberof EntityView
   */
  void (*setTextureValidation)(EntityView *self, const char *prefix, cm_asset_context_t context);
};

/**
 * @fn Class *EntityView::_EntityView(void)
 * @brief The EntityView archetype.
 * @return The EntityView Class.
 * @memberof EntityView
 */
CGAME_EXPORT Class *_EntityView(void);
