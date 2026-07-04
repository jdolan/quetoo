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

#include "cg_local.h"

#include "MaterialViewController.h"
#include "StageView.h"

#define _Class _MaterialViewController

#pragma mark - Delegates

/**
 * @brief SliderDelegate callback.
 */
static void didSetValue(Slider *slider, double value) {

  MaterialViewController *this = (MaterialViewController *) slider->delegate.self;

  if (!this->material) {
    return;
  }
  if (slider == this->roughness) {
    this->material->cm->roughness = slider->value;
  } else if (slider == this->hardness) {
    this->material->cm->hardness = slider->value;
  } else if (slider == this->specularity) {
    this->material->cm->specularity = slider->value;
  } else if (slider == this->parallax) {
    this->material->cm->parallax = slider->value;
  } else if (slider == this->shadow) {
    this->material->cm->shadow = slider->value;
  } else if (slider == this->alphaTest) {
    this->material->cm->alpha_test = slider->value;
  } else {
    Cg_Debug("Unknown Slider %p\n", (void *) slider);
    return;
  }

  this->material->cm->dirty = true;
}

/**
 * @brief Rebuilds the stage list, one StageView per collision stage.
 */
static void loadStages(MaterialViewController *this);

/**
 * @brief ButtonDelegate: append a new stage and rebuild the list. Safe to rebuild
 * synchronously here — the Add Stage button is not part of the stage list.
 */
static void didClickAddStage(Button *button) {

  MaterialViewController *this = button->delegate.self;

  if (this->material) {
    cgi.AddMaterialStage(this->material);
    loadStages(this);
  }
}

/**
 * @brief StageViewDelegate: highlight the clicked panel as the selected stage.
 * Removal is per-stage now (each panel has its own X), so selection is purely a
 * visual affordance.
 */
static void didSelectStage(StageView *stageView) {

  MaterialViewController *this = stageView->delegate.self;

  if (this->selectedStage == stageView) {
    return;
  }

  if (this->selectedStage) {
    $(this->selectedStage, setSelected, false);
  }

  $(stageView, setSelected, true);
  this->selectedStage = stageView;
}

static void loadStages(MaterialViewController *this) {

  $((View *) this->stages, removeAllSubviews);
  this->selectedStage = NULL;

  if (this->material) {
    for (cm_stage_t *stage = this->material->cm->stages; stage; stage = stage->next) {

      StageView *stageView = $(alloc(StageView), initWithStage, this->material, stage);

      stageView->delegate.self = this;
      stageView->delegate.didSelectStage = didSelectStage;

      $((View *) this->stages, addSubview, (View *) stageView);

      release(stageView);
    }
  }

  $((View *) this->stages, sizeToFit);
}

#pragma mark - ViewController

/**
 * @see ViewController::loadView(ViewController *)
 */
static void loadView(ViewController *self) {

  super(ViewController, self, loadView);

  MaterialViewController *this = (MaterialViewController *) self;

  // The material tab is built declaratively (KeyValueTableView rows in JSON). The
  // value widgets carry identifiers, so they resolve as outlets even though they
  // are nested inside the inlet-bound KeyValueView rows. The Material Stages box is
  // present but intentionally empty for now (no Add Stage button / stage rows yet).
  Outlet outlets[] = MakeOutlets(
    MakeOutlet("materialContent", &this->content),
    MakeOutlet("materialBox", &this->materialBox),
    MakeOutlet("diffusemap", &this->diffusemap),
    MakeOutlet("normalmap", &this->normalmap),
    MakeOutlet("specularmap", &this->specularmap),
    MakeOutlet("roughness", &this->roughness),
    MakeOutlet("hardness", &this->hardness),
    MakeOutlet("specularity", &this->specularity),
    MakeOutlet("parallax", &this->parallax),
    MakeOutlet("shadow", &this->shadow),
    MakeOutlet("alpha_test", &this->alphaTest),
    MakeOutlet("stages", &this->stages),
    MakeOutlet("addStage", &this->addStage)
  );

  $(self->view, awakeWithResourceName, "ui/editor/MaterialViewController.json");
  $(self->view, resolve, outlets);

  self->view->stylesheet = $$(Stylesheet, stylesheetWithResourceName, "ui/editor/MaterialViewController.css");
  assert(self->view->stylesheet);

  this->roughness->delegate.self = self;
  this->roughness->delegate.didSetValue = didSetValue;

  this->hardness->delegate.self = self;
  this->hardness->delegate.didSetValue = didSetValue;

  this->specularity->delegate.self = self;
  this->specularity->delegate.didSetValue = didSetValue;

  this->parallax->delegate.self = self;
  this->parallax->delegate.didSetValue = didSetValue;

  this->shadow->delegate.self = self;
  this->shadow->delegate.didSetValue = didSetValue;

  this->alphaTest->delegate.self = self;
  this->alphaTest->delegate.didSetValue = didSetValue;

  this->addStage->delegate.self = self;
  this->addStage->delegate.didClick = didClickAddStage;
}

/**
 * @see ViewController::respondToEvent(ViewController *, const SDL_Event *)
 */
static void respondToEvent(ViewController *self, const SDL_Event *event) {

  MaterialViewController *this = (MaterialViewController *) self;

  if (event->type == MVC_NOTIFICATION_EVENT
      && event->user.code == NOTIFICATION_MATERIAL_STAGE_EFFECTS_CHANGED
      && this->material && this->stages) {

    StageView *stageView = (StageView *) event->user.data1;

    // validate the stage view is still one of ours before rebuilding it
    const Array *subviews = ((View *) this->stages)->subviews;
    for (size_t i = 0; i < subviews->count; i++) {
      if ($(subviews, objectAtIndex, i) == (ident) stageView) {
        $(stageView, rebuildEffects);
        break;
      }
    }
  }

  // deferred per-stage removal: a StageView's own X posts this, since removing it
  // synchronously would free the view from inside its button handler
  if (event->type == MVC_NOTIFICATION_EVENT
      && event->user.code == NOTIFICATION_MATERIAL_STAGE_REMOVED
      && this->material && this->stages) {

    StageView *stageView = (StageView *) event->user.data1;

    const Array *subviews = ((View *) this->stages)->subviews;
    for (size_t i = 0; i < subviews->count; i++) {
      if ($(subviews, objectAtIndex, i) == (ident) stageView) {
        cgi.RemoveMaterialStage(this->material, stageView->stage);
        loadStages(this);
        break;
      }
    }
  }

  super(ViewController, self, respondToEvent, event);
}

/**
 * @see ViewController::viewWillAppear(ViewController *)
 */
static void viewWillAppear(ViewController *self) {

  MaterialViewController *this = (MaterialViewController *) self;

  const vec3_t start = cgi.view->origin;
  const vec3_t end = Vec3_Fmaf(start, MAX_WORLD_DIST, cgi.view->forward);

  r_material_t *material = NULL;

  const cg_editor_trace_t tr = Cg_MaterialSelectionTrace(start, end);
  if (tr.trace.fraction < 1.f && tr.trace.material) {
    material = cgi.LoadMaterial(tr.trace.material->name, tr.trace.material->context);
  }

  // Only refresh when the aimed-at material changes; avoids needless setText +
  // re-layout on every open at the same surface.
  if (material != this->material) {
    $(this, setMaterial, material);
  }

  super(ViewController, self, viewWillAppear);
}

#pragma mark - MaterialViewController

/**
 * @fn MaterialViewController *MaterialViewController::init(MaterialViewController *)
 * @memberof MaterialViewController
 */
static MaterialViewController *init(MaterialViewController *self) {
  return (MaterialViewController *) super(ViewController, self, init);
}

/**
 * @fn void MaterialViewController::setMaterial(MaterialViewController *self, r_material_t *material)
 * @memberof MaterialViewController
 */
static void setMaterial(MaterialViewController *self, r_material_t *material) {

  self->material = material;

  if (self->material) {
    $(self->materialBox->label->text, setText, va("Material ( %s )", self->material->cm->basename));
    $(self->diffusemap, setDefaultText, self->material->cm->diffusemap.name);
    $(self->normalmap, setDefaultText, self->material->cm->normalmap.name);
    $(self->specularmap, setDefaultText, self->material->cm->specularmap.name);

    $(self->roughness, setValue, (double) self->material->cm->roughness);
    $(self->hardness, setValue, (double) self->material->cm->hardness);
    $(self->specularity, setValue, (double) self->material->cm->specularity);
    $(self->parallax, setValue, (double) self->material->cm->parallax);
    $(self->shadow, setValue, (double) self->material->cm->shadow);
    $(self->alphaTest, setValue, (double) self->material->cm->alpha_test);

  } else {
    $(self->materialBox->label->text, setText, "Material");
    $(self->diffusemap, setDefaultText, NULL);
    $(self->normalmap, setDefaultText, NULL);
    $(self->specularmap, setDefaultText, NULL);

    $(self->roughness, setValue, MATERIAL_ROUGHNESS);
    $(self->hardness, setValue, MATERIAL_HARDNESS);
    $(self->specularity, setValue, MATERIAL_SPECULARITY);
    $(self->parallax, setValue, MATERIAL_PARALLAX);
    $(self->shadow, setValue, MATERIAL_SHADOW);
    $(self->alphaTest, setValue, MATERIAL_ALPHA_TEST);
  }

  ((Control *) self->addStage)->state = self->material ? ControlStateDefault : ControlStateDisabled;
  $((Control *) self->addStage, stateDidChange);

  loadStages(self);
}

#pragma mark - Class lifecycle

/**
 * @see Class::initialize(Class *)
 */
static void initialize(Class *clazz) {

  ((ViewControllerInterface *) clazz->interface)->loadView = loadView;
  ((ViewControllerInterface *) clazz->interface)->respondToEvent = respondToEvent;
  ((ViewControllerInterface *) clazz->interface)->viewWillAppear = viewWillAppear;

  ((MaterialViewControllerInterface *) clazz->interface)->init = init;
  ((MaterialViewControllerInterface *) clazz->interface)->setMaterial = setMaterial;
}

/**
 * @fn Class *MaterialViewController::_MaterialViewController(void)
 * @memberof MaterialViewController
 */
Class *_MaterialViewController(void) {
  static Class *clazz;
  static Once once;

  do_once(&once, {
    clazz = _initialize(&(const ClassDef) {
      .name = "MaterialViewController",
      .superclass = _ViewController(),
      .instanceSize = sizeof(MaterialViewController),
      .interfaceOffset = offsetof(MaterialViewController, interface),
      .interfaceSize = sizeof(MaterialViewControllerInterface),
      .initialize = initialize,
    });
  });

  return clazz;
}

#undef _Class
