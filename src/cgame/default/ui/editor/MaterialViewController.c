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
#include "Scrollbar.h"

#define _Class _MaterialViewController

/**
 * @brief Width of the scrollbar gutter reserved on the right of the stages list.
 */
#define MATERIAL_SCROLLBAR_WIDTH 14

/**
 * @brief Symmetric inset around the group boxes within the tab content.
 */
#define MATERIAL_CONTENT_PADDING 8

/**
 * @brief Height (window_bounds units) of the fixed section above the stages list
 * -- the Material + PBR boxes and the stages header. The stages scroll viewport
 * gets whatever tab height remains below this. Tuned by eye; a stable constant
 * because the UI renders in window_bounds units and those boxes never change
 * (always 3 map rows + 6 PBR sliders). Mirrors EDITOR_PANEL_CHROME's role.
 */
#define MATERIAL_TOP_SECTION_HEIGHT 500

/**
 * @brief The Add Stage icon button in the "Material Stages [  ]" box-title
 * brackets. Size matches the stage remove "X"; x/y place it in the bracket gap,
 * straddling the box top border like the label. Positioned in C: the stylesheet
 * system applies NO rules to this button (verified: its computed style stays empty
 * even when force-invalidated every frame -- it renders, but the theme pass yields
 * nothing for it), so CSS left/top cannot drive it. Tuned by eye (font-dependent).
 */
#define ADD_STAGE_ICON_SIZE 17

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
    MakeOutlet("stagesBox", &this->stagesBox),
    MakeOutlet("diffusemap", &this->diffusemap),
    MakeOutlet("normalmap", &this->normalmap),
    MakeOutlet("specularmap", &this->specularmap),
    MakeOutlet("roughness", &this->roughness),
    MakeOutlet("hardness", &this->hardness),
    MakeOutlet("specularity", &this->specularity),
    MakeOutlet("parallax", &this->parallax),
    MakeOutlet("shadow", &this->shadow),
    MakeOutlet("alpha_test", &this->alphaTest),
    MakeOutlet("stages", &this->stages)
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

  // Add Stage is an icon button (matching the stage remove "X") overlaid on the
  // "Material Stages [  ]" box title, sitting in the bracket gap -- there is no
  // separate header row. It is a subview of the box itself (not the contentView),
  // alignment-none with an explicit frame, so it floats over the title/top border.
  // Built in C because CSS width does not bind to a dynamically-built button (same
  // as the stage buttons).
  this->addStage = $(alloc(Button), initWithFrame, NULL);
  $((View *) this->addStage, addClassName, "iconButton");
  $(this->addStage->image, setImageWithResourceName, "ui/editor/icon_add.png");

  const SDL_Size addIconSize = MakeSize(ADD_STAGE_ICON_SIZE, ADD_STAGE_ICON_SIZE);
  ((View *) this->addStage)->minSize = addIconSize;
  ((View *) this->addStage)->maxSize = addIconSize;
  ((View *) this->addStage->image)->autoresizingMask |= ViewAutoresizingFill;
  $((View *) this->addStage, sizeToFit);

  ((View *) this->addStage)->alignment = ViewAlignmentNone;
  ((View *) this->addStage)->frame.x = 144;
  ((View *) this->addStage)->frame.y = -19;

  this->addStage->delegate.self = self;
  this->addStage->delegate.didClick = didClickAddStage;

  $((View *) this->stagesBox, addSubview, (View *) this->addStage);
  release(this->addStage);

  // Scroll ONLY the Material Stages list (not the whole tab): the maps + PBR boxes
  // stay fixed. The stages box's contentView holds just the scroll structure (the
  // Add Stage button lives in the box title above); that structure is
  //   stagesViewport               [fills remaining tab height, sized per-frame]
  //     |-- scrollView   -> contentView = `stages` (the StageView list)
  //     `-- scrollbar    -> pinned in the right gutter
  //
  // The gutter is created by insetting the SCROLL VIEW, not by padding the list.
  // This is the crux: a Contain-masked list sizes to `content + padding`, so any
  // right padding on it forces the list wider than the scroll view -- it then
  // bleeds under the bar and seeds a width ratchet. Instead the viewport reserves
  // the gutter as the SCROLL VIEW's own right padding, which insets only the
  // scrolled list (the padding shrinks the scroll view's content bounds, so the
  // list lays out to bounds - gutter), while the scroll view itself still fills
  // the viewport. The scrollbar is a sibling filling that full width and simply
  // right-aligns to the viewport's right edge, landing in the gutter beyond the
  // list. This uses only right-alignment + content padding -- no per-frame manual
  // positioning, which did not render reliably.
  View *stagesParent = ((View *) this->stages)->superview;

  this->stagesViewport = $(alloc(View), initWithFrame, NULL);
  assert(this->stagesViewport);
  this->stagesViewport->autoresizingMask = ViewAutoresizingWidth;
  this->stagesViewport->clipsSubviews = true;

  this->scrollView = $(alloc(ScrollView), initWithFrame, NULL);
  assert(this->scrollView);
  ((View *) this->scrollView)->clipsSubviews = true;
  // Reserve the scrollbar gutter inside the scroll view: this insets the content
  // list (`stages`) without moving the scroll view, so nothing extends under the
  // bar. Padding on the scroll view (fill mask) is safe -- unlike padding on the
  // Contain-masked list, which would inflate the list past the viewport.
  ((View *) this->scrollView)->padding.right = MATERIAL_SCROLLBAR_WIDTH + MATERIAL_CONTENT_PADDING;

  retain(this->stages);
  $((View *) this->stages, removeFromSuperview);
  $(this->scrollView, setContentView, (View *) this->stages);
  release(this->stages);

  $(this->stagesViewport, addSubview, (View *) this->scrollView);

  Scrollbar *scrollbar = $(alloc(Scrollbar), initWithScrollView, this->scrollView);
  assert(scrollbar);
  ((View *) scrollbar)->frame.w = MATERIAL_SCROLLBAR_WIDTH;
  ((View *) scrollbar)->autoresizingMask = ViewAutoresizingHeight;
  ((View *) scrollbar)->alignment = ViewAlignmentRight;
  $(this->stagesViewport, addSubview, (View *) scrollbar);
  this->scrollbar = (View *) scrollbar;
  release(scrollbar);

  $(stagesParent, addSubview, this->stagesViewport);
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
        // full rebuild: adding/removing flare/envmap toggles the fixed Texture row,
        // not just the effect rows.
        $(stageView, rebuild);
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
    $(self->diffusemap->text, setText, self->material->cm->diffusemap.name);
    $(self->normalmap->text, setText, self->material->cm->normalmap.name);
    $(self->specularmap->text, setText, self->material->cm->specularmap.name);

    $(self->roughness, setValue, (double) self->material->cm->roughness);
    $(self->hardness, setValue, (double) self->material->cm->hardness);
    $(self->specularity, setValue, (double) self->material->cm->specularity);
    $(self->parallax, setValue, (double) self->material->cm->parallax);
    $(self->shadow, setValue, (double) self->material->cm->shadow);
    $(self->alphaTest, setValue, (double) self->material->cm->alpha_test);

  } else {
    $(self->materialBox->label->text, setText, "Material");
    $(self->diffusemap->text, setText, "");
    $(self->normalmap->text, setText, "");
    $(self->specularmap->text, setText, "");

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

/**
 * @fn void MaterialViewController::fitStagesHeight(MaterialViewController *self, int tabPageHeight)
 * @memberof MaterialViewController
 */
static void fitStagesHeight(MaterialViewController *self, int tabPageHeight) {

  if (self->stagesViewport == NULL) {
    return;
  }

  const int h = tabPageHeight - MATERIAL_TOP_SECTION_HEIGHT;

  // Guarded frame assignment + needsLayout flags only -- no forced layout. MVC
  // applies it on the next KEY_UI pass; this is the same safe pattern as
  // EditorViewController::fitContentHeight (forcing layout from the KEY_GAME tick
  // desyncs frames and breaks hit-testing).
  if (h > 0 && self->stagesViewport->frame.h != h) {
    self->stagesViewport->frame.h = h;
    self->stagesViewport->needsLayout = true;
    if (self->stagesBox) {
      ((View *) self->stagesBox)->needsLayout = true;
    }
    if (self->content) {
      ((View *) self->content)->needsLayout = true;
    }
  }
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
  ((MaterialViewControllerInterface *) clazz->interface)->fitStagesHeight = fitStagesHeight;
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
