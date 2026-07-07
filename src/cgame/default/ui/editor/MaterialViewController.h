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
#include <ObjectivelyMVC/StackView.h>
#include <ObjectivelyMVC/ScrollView.h>
#include <ObjectivelyMVC/Button.h>
#include <ObjectivelyMVC/Box.h>
#include <ObjectivelyMVC/Label.h>

#include "StageView.h"

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
   * @brief The content container holding the group boxes (maps, PBR, stages).
   */
  StackView *content;

  /**
   * @brief The Material Stages group box; its scrollable list fills the remaining
   * tab height (sized per-frame by fitStagesHeight).
   */
  Box *stagesBox;

  /**
   * @brief The fixed-height viewport (a plain View) inside the stages box that
   * holds the scroll view + scrollbar. fitStagesHeight sizes its height so the
   * stages list fills the rest of the tab.
   */
  View *stagesViewport;

  /**
   * @brief The scroll view panning the stages list (its content view is `stages`).
   * It is inset from the viewport's right by the scrollbar gutter, so its
   * clipsSubviews clips the list before it can reach the bar.
   */
  ScrollView *scrollView;

  /**
   * @brief The vertical scrollbar for the stages list. It lives in the gutter to
   * the right of the (inset) scroll view; fitStagesHeight pins its x to the
   * viewport's right edge so it is not subject to the viewport's right padding.
   */
  View *scrollbar;

  /**
   * @brief The Material group box, whose title shows the material name.
   */
  Box *materialBox;

  /**
   * @brief The diffusemap texture name label (read-only display).
   */
  Label *diffusemap;

  /**
   * @brief The normalmap texture name label (read-only display).
   */
  Label *normalmap;

  /**
   * @brief The specularmap texture name label (read-only display).
   */
  Label *specularmap;

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

  /**
   * @brief The container holding the per-stage StageViews.
   */
  StackView *stages;

  /**
   * @brief The next collision stage to build a StageView for, or `NULL` when the
   * list is fully built. loadStages seeds this (instead of building synchronously)
   * so a material swap does not stall a frame per stage; a private pump view
   * (MaterialViewController.c) builds one StageView per rendered frame from this
   * cursor. Weak: points into `material->cm->stages`, reset on every loadStages call.
   */
  cm_stage_t *pendingStage;

  /**
   * @brief The Add Stage button.
   */
  Button *addStage;

  /**
   * @brief The currently selected (highlighted) stage panel, or `NULL`.
   */
  StageView *selectedStage;
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

  /**
   * @fn void MaterialViewController::fitStagesHeight(MaterialViewController *self, int tabPageHeight)
   * @brief Sizes the stages scroll viewport to fill the tab height below the fixed
   * maps + PBR section, so only the stages list scrolls. Called per-frame from
   * EditorViewController::fitContentHeight with the current tab page height.
   * @param self The MaterialViewController.
   * @param tabPageHeight The tab page's content height (from window_bounds).
   * @memberof MaterialViewController
   */
  void (*fitStagesHeight)(MaterialViewController *self, int tabPageHeight);
};

/**
 * @fn Class *MaterialViewController::_MaterialViewController(void)
 * @brief The MaterialViewController archetype.
 * @return The MaterialViewController Class.
 * @memberof MaterialViewController
 */
extern Class *_MaterialViewController(void);
