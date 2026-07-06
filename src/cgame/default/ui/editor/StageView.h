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

#include <ObjectivelyMVC/StackView.h>
#include <ObjectivelyMVC/Slider.h>
#include <ObjectivelyMVC/Select.h>
#include <ObjectivelyMVC/Checkbox.h>
#include <ObjectivelyMVC/Button.h>

/**
 * @file
 * @brief A View for editing a single material stage (`cm_stage_t`) in the in-game editor.
 */

typedef struct StageView StageView;
typedef struct StageViewInterface StageViewInterface;

/**
 * @brief Upper bounds on the controls a StageView may hold.
 */
#define STAGE_VIEW_MAX_EFFECTS 8
#define STAGE_VIEW_MAX_PARAMS 16

/**
 * @brief Binds a Slider to the stage float field it edits.
 */
typedef struct {
  Slider *slider;
  float *target;
} StageParamControl;

/**
 * @brief One active effect on the stage: the effect it represents and the button
 * that removes it. The effect is chosen when added (via the Add Effect dropdown),
 * so a panel does not carry its own effect Select.
 */
typedef struct {
  Button *removeButton;
  cm_stage_flags_t flag;
} StageEffectControl;

/**
 * @brief The StageViewDelegate type.
 */
typedef struct StageViewDelegate {

  /**
   * @brief The delegate self-reference.
   */
  ident self;

  /**
   * @brief Callback invoked when the stage's Remove button is clicked. The callback
   * must defer destruction of this view (e.g. via the event queue), since it fires
   * from within the button's own event handler.
   */
  void (*didRemoveStage)(StageView *view);

  /**
   * @brief Callback invoked when the user clicks anywhere on this panel, requesting
   * it become the selected stage (which reveals its Remove button).
   */
  void (*didSelectStage)(StageView *view);
} StageViewDelegate;

/**
 * @brief The StageView type.
 * @extends StackView
 */
struct StageView {

  /**
   * @brief The superclass.
   */
  StackView stackView;

  /**
   * @brief The interface. @protected
   */
  StageViewInterface *interface;

  /**
   * @brief The StageViewDelegate.
   */
  StageViewDelegate delegate;

  /**
   * @brief The material owning the stage.
   */
  r_material_t *material;

  /**
   * @brief The stage being edited (a node in `material->cm->stages`).
   */
  cm_stage_t *stage;

  /**
   * @brief The Remove button, shown only while this panel is selected.
   */
  Button *removeButton;

  /**
   * @brief Whether this panel is selected (revealing its Remove button).
   */
  bool selected;

  /**
   * @brief The container holding one row per active effect.
   */
  StackView *effectsContainer;

  /**
   * @brief The Add Effect dropdown: its options are the effects not yet on the
   * stage; picking one adds that effect.
   */
  Select *addEffect;

  /**
   * @brief The active effect rows.
   */
  StageEffectControl effects[STAGE_VIEW_MAX_EFFECTS];
  size_t numEffects;

  /**
   * @brief The parameter Slider bindings (across all active effects).
   */
  StageParamControl params[STAGE_VIEW_MAX_PARAMS];
  size_t numParams;
};

/**
 * @brief The StageView interface.
 */
struct StageViewInterface {

  /**
   * @brief The superclass interface.
   */
  StackViewInterface stackViewInterface;

  /**
   * @fn StageView *StageView::initWithStage(StageView *self, r_material_t *material, cm_stage_t *stage)
   * @brief Initializes this StageView for the given material stage.
   * @return The initialized StageView, or `NULL` on error.
   * @memberof StageView
   */
  StageView *(*initWithStage)(StageView *self, r_material_t *material, cm_stage_t *stage);

  /**
   * @fn void StageView::setSelected(StageView *self, bool selected)
   * @brief Sets this panel's selected state, showing or hiding its Remove button.
   * @memberof StageView
   */
  void (*setSelected)(StageView *self, bool selected);

  /**
   * @fn void StageView::rebuildEffects(StageView *self)
   * @brief Rebuilds the effect rows from the stage's current flags. Called (deferred)
   * after an effect is added, removed, or changed.
   * @memberof StageView
   */
  void (*rebuildEffects)(StageView *self);

  /**
   * @fn void StageView::rebuild(StageView *self)
   * @brief Rebuilds the entire stage panel (fixed controls + effect rows) from the
   * stage's current flags. Used (deferred) after an effect change that alters the
   * fixed controls -- e.g. adding flare/envmap toggles the top Texture row.
   * @memberof StageView
   */
  void (*rebuild)(StageView *self);
};

/**
 * @fn Class *StageView::_StageView(void)
 * @brief The StageView archetype.
 * @return The StageView Class.
 * @memberof StageView
 */
CGAME_EXPORT Class *_StageView(void);
