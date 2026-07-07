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
 * @brief Applies the material of the shared selection cursor's current candidate.
 */
static void refreshSelection(MaterialViewController *this);

/**
 * @brief Builds one StageView for `stage` and appends it to the stages list. Shared
 * by the progressive pump (many stages, one per frame) and single-stage add (one
 * stage, applied immediately -- see didClickAddStage) so there is one place that
 * knows how to construct a stage row.
 */
static void appendStageView(MaterialViewController *this, cm_stage_t *stage);

/**
 * @brief ButtonDelegate: append ONE new StageView for the newly added stage.
 * Deliberately does NOT go through loadStages (which tears down and rebuilds the
 * WHOLE list): that full rebuild used to be instant and invisible, but chunking it
 * (for the material-swap perf fix) turned "add one stage" into a visible clear +
 * multi-frame repopulate of every OTHER stage too, and reset the scroll position.
 * Since only one stage actually changed, just add its one row.
 */
static void didClickAddStage(Button *button) {

  MaterialViewController *this = button->delegate.self;

  if (this->material) {
    cm_stage_t *stage = cgi.AddMaterialStage(this->material);
    appendStageView(this, stage);
    $((View *) this->stages, sizeToFit);
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

static void appendStageView(MaterialViewController *this, cm_stage_t *stage) {

  StageView *stageView = $(alloc(StageView), initWithStage, this->material, stage);

  stageView->delegate.self = this;
  stageView->delegate.didSelectStage = didSelectStage;

  $((View *) this->stages, addSubview, (View *) stageView);

  release(stageView);
}

static void loadStages(MaterialViewController *this) {

  $((View *) this->stages, removeAllSubviews);
  this->selectedStage = NULL;

  // Seed the cursor; the pump view (below) builds one StageView per rendered frame
  // from it instead of all of them here synchronously. A heavily-staged material
  // (e.g. 20 stages) rebuilding in one frame is the ~3s / 20fps freeze this avoids --
  // MVC's style pass is O(views x selectors) and a single loadStages call used to
  // apply it to every new stage's whole subtree in the same frame.
  this->pendingStage = this->material ? this->material->cm->stages : NULL;

  if (this->pendingStage == NULL) {
    $((View *) this->stages, sizeToFit);
  }
}

#pragma mark - StagesPumpView

/**
 * @brief A tiny (1x1), effectively invisible View whose sole job is building one
 * pending StageView per rendered frame (see loadStages). It draws nothing visible
 * (no background/border ever set), but CANNOT be zero-size: Renderer::drawView only
 * calls a view's render() when its clippingFrame has non-zero width AND height, so a
 * genuinely zero-size view's render() is never invoked at all -- the pump would be
 * permanently dead (this was tried first and silently never ran). Progress only
 * happens while this view is actually drawn -- i.e. while the Material tab is the
 * active tab, since PageView hides inactive tab pages (and hidden views are not
 * drawn) -- which is exactly the frame cadence we want: MVC's own layout/render pass
 * (Ui_Draw, KEY_UI only) advances the build once per frame, spreading the cost that a
 * single synchronous rebuild used to pay all at once. A private, minimal View
 * subclass is the only place to hook true per-frame-while-visible work in this MVC
 * version: View has no per-instance callback, so behavior can only be added by
 * overriding a virtual method (render) via a Class -- there is no ViewController
 * "tick" hook.
 */
typedef struct StagesPumpView StagesPumpView;
typedef struct StagesPumpViewInterface StagesPumpViewInterface;

struct StagesPumpView {
  View view;
  StagesPumpViewInterface *interface;
  MaterialViewController *controller; // weak
};

struct StagesPumpViewInterface {
  ViewInterface viewInterface;

  StagesPumpView *(*initWithFrame)(StagesPumpView *self, const SDL_Rect *frame);
};

// Every other class's _ClassName(void) is forward-declared via its own public header,
// included before its .c file's first use of super()/alloc(). StagesPumpView has no
// header (private to this file), so it needs its own forward declaration here -- super()
// expands to a call through _StagesPumpView(), used below before its definition.
static Class *_StagesPumpView(void);

// super() expands using the file-scoped _Class macro (#define'd to
// _MaterialViewController above) to resolve "this class's superclass" -- so it must be
// rebound to _StagesPumpView for exactly this class's own methods, and restored
// immediately after, or super() here would resolve MaterialViewController's superclass
// (ViewController) instead of View, reading garbage through a mistyped interface
// pointer (crashed as an access violation on the very first initWithFrame call).
#undef _Class
#define _Class _StagesPumpView

/**
 * @see View::initWithFrame(View *, const SDL_Rect *)
 */
static StagesPumpView *initWithFrame(StagesPumpView *self, const SDL_Rect *frame) {
  return (StagesPumpView *) super(View, self, initWithFrame, frame);
}

/**
 * @see View::render(View *, Renderer *)
 */
static void pumpRender(View *self, Renderer *renderer) {

  super(View, self, render, renderer);

  MaterialViewController *controller = ((StagesPumpView *) self)->controller;

  cm_stage_t *stage = controller->pendingStage;
  if (stage == NULL) {
    return;
  }

  controller->pendingStage = stage->next;

  appendStageView(controller, stage);

  if (controller->pendingStage == NULL) {
    $((View *) controller->stages, sizeToFit);
  }
}

/**
 * @see Class::initialize(Class *)
 */
static void initializeStagesPumpView(Class *clazz) {
  ((ViewInterface *) clazz->interface)->render = pumpRender;
  ((StagesPumpViewInterface *) clazz->interface)->initWithFrame = initWithFrame;
}

/**
 * @brief The StagesPumpView archetype. Private to this file: nothing outside
 * MaterialViewController.c instantiates or references this class.
 */
static Class *_StagesPumpView(void) {
  static Class *clazz;
  static Once once;

  do_once(&once, {
    clazz = _initialize(&(const ClassDef) {
      .name = "StagesPumpView",
      .superclass = _View(),
      .instanceSize = sizeof(StagesPumpView),
      .interfaceOffset = offsetof(StagesPumpView, interface),
      .interfaceSize = sizeof(StagesPumpViewInterface),
      .initialize = initializeStagesPumpView,
    });
  });

  return clazz;
}

// Restore _Class to the outer MaterialViewController for the rest of the file (its
// own super() calls, e.g. in loadView/respondToEvent, must resolve ViewController).
#undef _Class
#define _Class _MaterialViewController

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

  // 1x1, effectively invisible: its only job is ticking the progressive stage build
  // once per rendered frame (see loadStages / StagesPumpView above). Must be non-zero
  // size -- Renderer::drawView skips render() entirely for a zero-size clippingFrame,
  // which is why NULL (a zero-size frame) here previously left the pump permanently
  // dead. A descendant of the stages viewport so it only renders (and thus only makes
  // progress) while the Material tab is the active tab.
  const SDL_Rect pumpFrame = { .w = 1, .h = 1 };
  StagesPumpView *pump = $(alloc(StagesPumpView), initWithFrame, &pumpFrame);
  assert(pump);
  pump->controller = this;
  $(this->stagesViewport, addSubview, (View *) pump);
  release(pump);
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
        // Remove just this one row -- not a full loadStages rebuild (same reasoning
        // as didClickAddStage: only one stage changed, the other rows are unaffected).
        cgi.RemoveMaterialStage(this->material, stageView->stage);
        if (this->selectedStage == stageView) {
          this->selectedStage = NULL;
        }
        $((View *) this->stages, removeSubview, (View *) stageView);
        $((View *) this->stages, sizeToFit);
        break;
      }
    }
  }

  // The mouse wheel cycled the shared selection cursor (issue #840): show the new
  // candidate's material. This lets the user wheel past a foreground brush (e.g. a
  // common/dust surface) to reach the material behind it.
  if (event->type == MVC_NOTIFICATION_EVENT
      && event->user.code == NOTIFICATION_EDITOR_SELECTION_CYCLE) {
    refreshSelection(this);
  }

  super(ViewController, self, respondToEvent, event);
}

/**
 * @brief Applies the material of the shared selection cursor's current candidate,
 * loaded from the struck surface. A candidate with no material (a point entity or an
 * untextured surface) clears the panel -- wheel past it to the next surface. Only
 * re-applies when the material actually changes, avoiding needless setText + re-layout.
 */
static void refreshSelection(MaterialViewController *this) {

  r_material_t *material = NULL;

  if (cg_editor.num_candidates) {
    const cm_trace_t *tr = &cg_editor.candidates[cg_editor.candidate_index].trace;
    if (tr->material) {
      material = cgi.LoadMaterial(tr->material->name, tr->material->context);
    }
  }

  if (material != this->material) {
    $(this, setMaterial, material);
  }
}

/**
 * @see ViewController::viewWillAppear(ViewController *)
 */
static void viewWillAppear(ViewController *self) {

  MaterialViewController *this = (MaterialViewController *) self;

  const vec3_t start = cgi.view->origin;
  const vec3_t end = Vec3_Fmaf(start, MAX_WORLD_DIST, cgi.view->forward);

  // The material tab reaches the full world distance (walls sit far behind entities),
  // unlike the entity tab's capped ray. Build the shared candidate list and show the
  // nearest surface's material; the wheel steps to surfaces behind it.
  Cg_BuildSelectionCandidates(start, end);

  refreshSelection(this);

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
