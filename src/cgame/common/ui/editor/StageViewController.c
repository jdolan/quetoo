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

#include "cg_local.h"

#include "StageView.h"
#include "StageViewController.h"

#define _Class _StageViewController

#pragma mark - Stages

/**
 * @brief Shows the number of stages, and the summary of each StageView, which includes its index.
 */
static void updateStages(StageViewController *this) {

  const Array *stageViews = (Array *) ((View *) this->stageList)->subviews;

  for (size_t i = 0; i < stageViews->count; i++) {
    $((StageView *) $(stageViews, objectAtIndex, i), update);
  }

  $(this->count->text, setText, va("%zu stages", stageViews->count));
}

static void didRemoveStage(StageView *stageView);

/**
 * @brief Appends a StageView for the stage to the list.
 */
static StageView *addStageView(StageViewController *this, CmStage *stage, bool collapsed) {

  $(this->removedStageViews, removeAllObjects);

  StageView *stageView = $(alloc(StageView), initWithStage, this->material, stage);
  assert(stageView);

  stageView->delegate.self = this;
  stageView->delegate.didRemoveStage = didRemoveStage;

  $(stageView, setCollapsed, collapsed);

  $((View *) this->stageList, addSubview, (View *) stageView);
  release(stageView);

  return stageView;
}

#pragma mark - Delegates

/**
 * @brief StageViewDelegate callback: removes the stage and its StageView.
 */
static void didRemoveStage(StageView *stageView) {

  StageViewController *this = stageView->delegate.self;

  cgi.RemoveMaterialStage(this->material->cm, stageView->stage);
  Cg_ReloadEditorMaterialStages(this->material);

  $(this->removedStageViews, removeAllObjects);
  $(this->removedStageViews, addObject, stageView);

  $((View *) this->stageList, removeSubview, (View *) stageView);

  updateStages(this);
}

/**
 * @brief ButtonDelegate callback for the add stage button: appends a stage, and shows it expanded.
 */
static void didClickAddStage(Button *button) {

  StageViewController *this = (StageViewController *) button->delegate.self;

  if (!this->material) {
    return;
  }

  CmStage *stage = cgi.AddMaterialStage(this->material->cm);
  Cg_ReloadEditorMaterialStages(this->material);

  addStageView(this, stage, false);

  updateStages(this);
}

#pragma mark - Object

/**
 * @see Object::dealloc(Object *)
 */
static void dealloc(Object *self) {

  StageViewController *this = (StageViewController *) self;

  release(this->removedStageViews);

  super(Object, self, dealloc);
}

#pragma mark - ViewController

/**
 * @see ViewController::loadView(ViewController *)
 */
static void loadView(ViewController *self) {

  super(ViewController, self, loadView);

  StageViewController *this = (StageViewController *) self;

  Outlet outlets[] = MakeOutlets(
    MakeOutlet("count", &this->count),
    MakeOutlet("addStage", &this->addStage),
    MakeOutlet("stageList", &this->stageList)
  );

  $(self->view, awakeWithResourceName, "ui/editor/StageViewController.json");
  $(self->view, resolve, outlets);

  self->view->stylesheet = $$(Stylesheet, stylesheetWithResourceName, "ui/editor/StageViewController.css");
  assert(self->view->stylesheet);

  this->addStage->delegate.self = self;
  this->addStage->delegate.didClick = didClickAddStage;
}

/**
 * @see ViewController::viewWillAppear(ViewController *)
 */
static void viewWillAppear(ViewController *self) {

  StageViewController *this = (StageViewController *) self;

  const Vec3 start = cgi.view->origin;
  const Vec3 end = Vec3_Fmaf(start, MAX_WORLD_DIST, cgi.view->forward);

  RenderMaterial *material = NULL;

  const CGameEditorTrace tr = Cg_MaterialSelectionTrace(start, end);
  if (tr.trace.fraction < 1.f && tr.trace.material) {
    material = cgi.LoadMaterial(tr.trace.material->name, tr.trace.material->context);
  }

  $(this, setMaterial, material);

  super(ViewController, self, viewWillAppear);
}

#pragma mark - StageViewController

/**
 * @fn StageViewController *StageViewController::init(StageViewController *)
 * @memberof StageViewController
 */
static StageViewController *init(StageViewController *self) {

  self = (StageViewController *) super(ViewController, self, init);
  if (self) {
    self->removedStageViews = $$(Array, array);
  }

  return self;
}

/**
 * @fn void StageViewController::setMaterial(StageViewController *self, RenderMaterial *material)
 * @memberof StageViewController
 * @remarks A material with a single stage shows it expanded, and one with several collapses them.
 */
static void setMaterial(StageViewController *self, RenderMaterial *material) {

  self->material = material;

  $((View *) self->stageList, removeAllSubviews);

  if (self->material) {
    const CmStage *stages = self->material->cm->stages;
    const bool collapsed = stages && stages->next;

    for (CmStage *s = self->material->cm->stages; s; s = s->next) {
      addStageView(self, s, collapsed);
    }
  }

  updateStages(self);

  if (self->material) {
    ((Control *) self->addStage)->state &= ~ControlStateDisabled;
  } else {
    ((Control *) self->addStage)->state |= ControlStateDisabled;
  }
  $((Control *) self->addStage, stateDidChange);
}

#pragma mark - Class lifecycle

/**
 * @see Class::initialize(Class *)
 */
static void initialize(Class *clazz) {

  ((ObjectInterface *) clazz->interface)->dealloc = dealloc;

  ((ViewControllerInterface *) clazz->interface)->loadView = loadView;
  ((ViewControllerInterface *) clazz->interface)->viewWillAppear = viewWillAppear;

  ((StageViewControllerInterface *) clazz->interface)->init = init;
  ((StageViewControllerInterface *) clazz->interface)->setMaterial = setMaterial;
}

/**
 * @fn Class *StageViewController::_StageViewController(void)
 * @memberof StageViewController
 */
Class *_StageViewController(void) {
  static Class *clazz;
  static Once once;

  do_once(&once, {
    clazz = _initialize(&(const ClassDef) {
      .name = "StageViewController",
      .superclass = _ViewController(),
      .instanceSize = sizeof(StageViewController),
      .interfaceSize = sizeof(StageViewControllerInterface),
      .initialize = initialize,
    });
  });

  return clazz;
}

#undef _Class
