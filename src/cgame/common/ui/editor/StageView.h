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

#include <ObjectivelyMVC/Box.h>
#include <ObjectivelyMVC/Button.h>
#include <ObjectivelyMVC/Select.h>
#include <ObjectivelyMVC/TextView.h>

/**
 * @file
 *
 * @brief The StageView edits one stage of a material.
 */

typedef struct StageView StageView;
typedef struct StageViewInterface StageViewInterface;

/**
 * @brief The StageView delegate.
 */
typedef struct {

  /**
   * @brief The delegate self-reference.
   */
  ident self;

  /**
   * @brief Called when the remove button of the StageView is clicked.
   * @param stageView The StageView.
   */
  void (*didRemoveStage)(StageView *stageView);
} StageViewDelegate;

/**
 * @brief The StageView type: a Box whose label summarizes the stage, and which collapses when its
 * label is clicked.
 * @extends Box
 */
struct StageView {

  /**
   * @brief The superclass.
   */
  Box box;

  /**
   * @brief The interface type. @private
   */
  StageViewInterface *interface[0];

  /**
   * @brief The delegate.
   */
  StageViewDelegate delegate;

  /**
   * @brief The material that owns the stage.
   */
  RenderMaterial *material;

  /**
   * @brief The stage.
   */
  CmStage *stage;

  /**
   * @brief The button that removes the stage.
   */
  Button *removeStage;

  /**
   * @brief The stage asset text field.
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
 * @brief The StageView interface.
 */
struct StageViewInterface {

  /**
   * @brief The superclass interface.
   */
  BoxInterface boxInterface;

  /**
   * @fn StageView *StageView::initWithStage(StageView *self, RenderMaterial *material, CmStage *stage)
   * @brief Initializes this StageView with the given stage.
   * @param self The StageView.
   * @param material The material that owns the stage.
   * @param stage The stage.
   * @return The initialized StageView, or `NULL` on error.
   * @memberof StageView
   */
  StageView *(*initWithStage)(StageView *self, RenderMaterial *material, CmStage *stage);

  /**
   * @fn void StageView::setCollapsed(StageView *self, bool collapsed)
   * @brief Collapses or shows the contents of this StageView.
   * @param self The StageView.
   * @param collapsed True to collapse the contents.
   * @memberof StageView
   */
  void (*setCollapsed)(StageView *self, bool collapsed);

  /**
   * @fn void StageView::update(StageView *self)
   * @brief Shows the values of the stage again, and its summary, which includes its index.
   * @param self The StageView.
   * @memberof StageView
   */
  void (*update)(StageView *self);
};

/**
 * @fn Class *StageView::_StageView(void)
 * @brief The StageView archetype.
 * @return The StageView Class.
 * @memberof StageView
 */
extern Class *_StageView(void);
