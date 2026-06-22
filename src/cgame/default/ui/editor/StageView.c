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

#include "StageView.h"

#include <ObjectivelyMVC/Label.h>

#define _Class _StageView

#pragma mark - Effect descriptor tables

/**
 * @brief Describes one editable float parameter of a stage, by its offset into
 * `cm_stage_t` and the slider range used to edit it.
 */
typedef struct {
  const char *label;
  size_t offset;
  double min, max, step;
} stage_param_desc_t;

/**
 * @brief Describes one stage effect: the flag it toggles and its parameters.
 */
typedef struct {
  cm_stage_flags_t flag;
  const char *name;
  const stage_param_desc_t *params;
  size_t num_params;
} stage_effect_desc_t;

#define STAGE_PARAM(field, mn, mx, st) { #field, offsetof(cm_stage_t, field), mn, mx, st }

static const stage_param_desc_t color_params[] = {
  STAGE_PARAM(color.r, 0.0, 1.0, 0.01), STAGE_PARAM(color.g, 0.0, 1.0, 0.01),
  STAGE_PARAM(color.b, 0.0, 1.0, 0.01), STAGE_PARAM(color.a, 0.0, 1.0, 0.01),
};
static const stage_param_desc_t pulse_params[] = {
  STAGE_PARAM(pulse.hz, 0.0, 10.0, 0.1), STAGE_PARAM(pulse.drift, 0.0, 10.0, 0.1),
};
static const stage_param_desc_t stretch_params[] = {
  STAGE_PARAM(stretch.hz, 0.0, 10.0, 0.1), STAGE_PARAM(stretch.amplitude, 0.0, 10.0, 0.1),
};
static const stage_param_desc_t rotate_params[] = {
  STAGE_PARAM(rotate.hz, -10.0, 10.0, 0.1),
};
static const stage_param_desc_t scroll_s_params[] = { STAGE_PARAM(scroll.s, -10.0, 10.0, 0.1) };
static const stage_param_desc_t scroll_t_params[] = { STAGE_PARAM(scroll.t, -10.0, 10.0, 0.1) };
static const stage_param_desc_t scale_s_params[] = { STAGE_PARAM(scale.s, 0.0, 16.0, 0.1) };
static const stage_param_desc_t scale_t_params[] = { STAGE_PARAM(scale.t, 0.0, 16.0, 0.1) };
static const stage_param_desc_t terrain_params[] = {
  STAGE_PARAM(terrain.floor, -2048.0, 2048.0, 8.0), STAGE_PARAM(terrain.ceil, -2048.0, 2048.0, 8.0),
};
static const stage_param_desc_t dirtmap_params[] = { STAGE_PARAM(dirtmap.intensity, 0.0, 1.0, 0.01) };
static const stage_param_desc_t warp_params[] = {
  STAGE_PARAM(warp.hz, 0.0, 10.0, 0.1), STAGE_PARAM(warp.amplitude, 0.0, 10.0, 0.1),
};
static const stage_param_desc_t lighting_params[] = { STAGE_PARAM(lighting.intensity, 0.0, 10.0, 0.1) };
static const stage_param_desc_t emissive_params[] = { STAGE_PARAM(emissive, 0.0, 1.0, 0.01) };
static const stage_param_desc_t shell_params[] = { STAGE_PARAM(shell.radius, 0.0, 100.0, 1.0) };
static const stage_param_desc_t animation_params[] = {
  STAGE_PARAM(animation.fps, 0.0, 60.0, 1.0), STAGE_PARAM(animation.drift, 0.0, 10.0, 0.1),
};

#define STAGE_EFFECT(f, n, p) { f, n, p, lengthof(p) }
#define STAGE_EFFECT_TOGGLE(f, n) { f, n, NULL, 0 }

static const stage_effect_desc_t stage_effects[] = {
  STAGE_EFFECT(STAGE_COLOR, "Color", color_params),
  STAGE_EFFECT(STAGE_PULSE, "Pulse", pulse_params),
  STAGE_EFFECT(STAGE_STRETCH, "Stretch", stretch_params),
  STAGE_EFFECT(STAGE_ROTATE, "Rotate", rotate_params),
  STAGE_EFFECT(STAGE_SCROLL_S, "Scroll S", scroll_s_params),
  STAGE_EFFECT(STAGE_SCROLL_T, "Scroll T", scroll_t_params),
  STAGE_EFFECT(STAGE_SCALE_S, "Scale S", scale_s_params),
  STAGE_EFFECT(STAGE_SCALE_T, "Scale T", scale_t_params),
  STAGE_EFFECT(STAGE_ANIMATION, "Animation", animation_params),
  STAGE_EFFECT_TOGGLE(STAGE_ANIM_LERP, "Anim Lerp"),
  STAGE_EFFECT(STAGE_TERRAIN, "Terrain", terrain_params),
  STAGE_EFFECT(STAGE_DIRTMAP, "Dirtmap", dirtmap_params),
  STAGE_EFFECT_TOGGLE(STAGE_ENVMAP, "Envmap"),
  STAGE_EFFECT(STAGE_WARP, "Warp", warp_params),
  STAGE_EFFECT(STAGE_LIGHTING, "Lighting", lighting_params),
  STAGE_EFFECT_TOGGLE(STAGE_LIGHTING_FLAT, "Lighting Flat"),
  STAGE_EFFECT(STAGE_EMISSIVE, "Emissive", emissive_params),
  STAGE_EFFECT_TOGGLE(STAGE_FLARE, "Flare"),
  STAGE_EFFECT(STAGE_SHELL, "Shell", shell_params),
};

#pragma mark - Internal helpers

/**
 * @brief Returns the zero-based index of this view's stage within its material.
 */
static int32_t stageIndex(const StageView *this) {
  int32_t i = 0;
  for (const cm_stage_t *s = this->material->cm->stages; s; s = s->next, i++) {
    if (s == this->stage) {
      return i;
    }
  }
  return -1;
}

/**
 * @brief Allocates a horizontal StackView row with the given CSS class.
 */
static StackView *row(const char *className) {
  StackView *r = $(alloc(StackView), initWithFrame, NULL);
  r->axis = StackViewAxisHorizontal;
  $((View *) r, addClassName, className);
  return r;
}

/**
 * @brief Adds a Label to the view.
 */
static void addLabel(View *view, const char *text) {
  Label *label = $(alloc(Label), initWithText, text, NULL);
  $(view, addSubview, (View *) label);
  release(label);
}

/**
 * @brief Returns the first effect flag not yet active on the stage, or `STAGE_NONE`.
 */
static cm_stage_flags_t firstAvailableEffect(const StageView *this) {
  for (size_t i = 0; i < lengthof(stage_effects); i++) {
    if (!(this->stage->flags & stage_effects[i].flag)) {
      return stage_effects[i].flag;
    }
  }
  return STAGE_NONE;
}

/**
 * @brief Requests a deferred rebuild of this stage's effect rows. Deferred via the
 * event queue because the structural change frees the widget that triggered it.
 */
static void requestRebuild(StageView *this) {
  this->material->cm->dirty = true;
  SDL_PushEvent(&(SDL_Event) {
    .user.type = MVC_NOTIFICATION_EVENT,
    .user.code = NOTIFICATION_MATERIAL_STAGE_EFFECTS_CHANGED,
    .user.data1 = this,
  });
}

#pragma mark - Delegates

/**
 * @brief SliderDelegate: write the edited float straight into the stage. The draw
 * path reads it live, so no rebuild is needed.
 */
static void didSetParam(Slider *slider, double value) {

  StageView *this = (StageView *) slider->delegate.self;

  for (size_t i = 0; i < this->numParams; i++) {
    if (this->params[i].slider == slider) {
      *this->params[i].target = (float) value;
      this->material->cm->dirty = true;
      return;
    }
  }
}

/**
 * @brief SelectDelegate: change which effect an existing row represents.
 */
static void didSelectEffect(Select *select, Option *option) {

  StageView *this = (StageView *) select->delegate.self;

  const cm_stage_flags_t newFlag = (cm_stage_flags_t) (intptr_t) option->value;

  for (size_t i = 0; i < this->numEffects; i++) {
    if (this->effects[i].select == select) {

      const cm_stage_flags_t oldFlag = this->effects[i].flag;
      if (newFlag == oldFlag) {
        return;
      }

      // another row already owns this effect; revert the selection
      if (this->stage->flags & newFlag) {
        $(select, selectOptionWithValue, (ident) (intptr_t) oldFlag);
        return;
      }

      this->stage->flags &= ~oldFlag;
      this->stage->flags |= newFlag;
      cgi.FinalizeMaterialStage(this->material, this->stage);

      requestRebuild(this);
      return;
    }
  }
}

/**
 * @brief ButtonDelegate: remove an effect from the stage.
 */
static void didClickRemoveEffect(Button *button) {

  StageView *this = (StageView *) button->delegate.self;

  for (size_t i = 0; i < this->numEffects; i++) {
    if (this->effects[i].removeButton == button) {
      this->stage->flags &= ~this->effects[i].flag;
      cgi.FinalizeMaterialStage(this->material, this->stage);
      requestRebuild(this);
      return;
    }
  }
}

/**
 * @brief ButtonDelegate: add the next available effect to the stage.
 */
static void didClickAddEffect(Button *button) {

  StageView *this = (StageView *) button->delegate.self;

  const cm_stage_flags_t flag = firstAvailableEffect(this);
  if (flag == STAGE_NONE) {
    return;
  }

  this->stage->flags |= flag;
  cgi.FinalizeMaterialStage(this->material, this->stage);

  requestRebuild(this);
}

/**
 * @brief ButtonDelegate: request stage removal. Deferred (frees this view).
 */
static void didClickRemove(Button *button) {

  StageView *this = (StageView *) button->delegate.self;

  if (this->delegate.didRemoveStage) {
    this->delegate.didRemoveStage(this);
  }
}

#pragma mark - View

/**
 * @see View::respondToEvent(View *, const SDL_Event *)
 */
static void respondToEvent(View *self, const SDL_Event *event) {

  StageView *this = (StageView *) self;

  if (event->type == SDL_EVENT_MOUSE_BUTTON_DOWN) {
    if ($(self, didReceiveEvent, event)) {
      if (this->delegate.didSelectStage) {
        this->delegate.didSelectStage(this);
      }
    }
  }

  super(View, self, respondToEvent, event);
}

#pragma mark - StageView

/**
 * @brief Builds one effect row (a dropdown + remove button, plus the effect's
 * parameter sliders) for the given active effect.
 */
static void addEffectGroup(StageView *this, const stage_effect_desc_t *effect) {

  StackView *group = $(alloc(StackView), initWithFrame, NULL);
  ((StackView *) group)->axis = StackViewAxisVertical;
  $((View *) group, addClassName, "stageEffect");

  // header: [ effect dropdown ][ - ]
  StackView *header = row("stageEffectHeader");

  Select *select = $(alloc(Select), initWithFrame, NULL);
  for (size_t i = 0; i < lengthof(stage_effects); i++) {
    const cm_stage_flags_t f = stage_effects[i].flag;
    if (f == effect->flag || !(this->stage->flags & f)) {
      $(select, addOption, stage_effects[i].name, (ident) (intptr_t) f);
    }
  }
  $(select, selectOptionWithValue, (ident) (intptr_t) effect->flag);
  select->delegate.self = this;
  select->delegate.didSelectOption = didSelectEffect;
  $((View *) header, addSubview, (View *) select);

  Button *remove = $(alloc(Button), initWithTitle, "-");
  remove->delegate.self = this;
  remove->delegate.didClick = didClickRemoveEffect;
  $((View *) header, addSubview, (View *) remove);

  $((View *) group, addSubview, (View *) header);
  release(header);

  assert(this->numEffects < STAGE_VIEW_MAX_EFFECTS);
  this->effects[this->numEffects].select = select;
  this->effects[this->numEffects].removeButton = remove;
  this->effects[this->numEffects].flag = effect->flag;
  this->numEffects++;

  release(select);
  release(remove);

  // parameter sliders
  for (size_t p = 0; p < effect->num_params; p++) {
    const stage_param_desc_t *param = &effect->params[p];

    StackView *paramRow = row("stageParam");
    addLabel((View *) paramRow, param->label);

    Slider *slider = $(alloc(Slider), initWithFrame, NULL);
    slider->min = param->min;
    slider->max = param->max;
    slider->step = param->step;
    $(slider, setLabelFormat, "%.2f");

    float *target = (float *) ((uint8_t *) this->stage + param->offset);
    $(slider, setValue, (double) *target);

    slider->delegate.self = this;
    slider->delegate.didSetValue = didSetParam;
    $((View *) paramRow, addSubview, (View *) slider);
    release(slider);

    $((View *) group, addSubview, (View *) paramRow);
    release(paramRow);

    assert(this->numParams < STAGE_VIEW_MAX_PARAMS);
    this->params[this->numParams].slider = slider;
    this->params[this->numParams].target = target;
    this->numParams++;
  }

  $((View *) this->effectsContainer, addSubview, (View *) group);
  release(group);
}

/**
 * @fn void StageView::rebuildEffects(StageView *self)
 * @memberof StageView
 */
static void rebuildEffects(StageView *self) {

  $((View *) self->effectsContainer, removeAllSubviews);
  self->numEffects = self->numParams = 0;

  for (size_t e = 0; e < lengthof(stage_effects); e++) {
    if (self->stage->flags & stage_effects[e].flag) {
      addEffectGroup(self, &stage_effects[e]);
    }
  }

  // disable Add Effect when every effect is already present
  Control *add = (Control *) self->addEffect;
  if (firstAvailableEffect(self) == STAGE_NONE) {
    add->state |= ControlStateDisabled;
  } else {
    add->state &= ~ControlStateDisabled;
  }
  $(add, stateDidChange);

  ((View *) self)->needsLayout = true;
}

/**
 * @brief Builds the stage's fixed controls (header, texture, effects container,
 * Add Effect button, Remove button), then the effect rows.
 */
static void buildControls(StageView *this) {

  View *view = (View *) this;

  $(view, removeAllSubviews);
  this->numEffects = this->numParams = 0;

  // header: "Stage N"
  StackView *header = row("stageHeader");
  addLabel((View *) header, va("Stage %d", stageIndex(this) + 1));
  $(view, addSubview, (View *) header);
  release(header);

  // read-only texture asset
  addLabel(view, va("Texture: %s", *this->stage->asset.name ? this->stage->asset.name : "(none)"));

  // container for the active effect rows
  this->effectsContainer = $(alloc(StackView), initWithFrame, NULL);
  ((StackView *) this->effectsContainer)->axis = StackViewAxisVertical;
  $((View *) this->effectsContainer, addClassName, "stageEffects");
  $(view, addSubview, (View *) this->effectsContainer);
  release(this->effectsContainer);

  // Add Effect button
  StackView *addRow = row("stageAddEffect");
  this->addEffect = $(alloc(Button), initWithTitle, "+ Add Effect");
  this->addEffect->delegate.self = this;
  this->addEffect->delegate.didClick = didClickAddEffect;
  $((View *) addRow, addSubview, (View *) this->addEffect);
  release(this->addEffect);
  $(view, addSubview, (View *) addRow);
  release(addRow);

  // Remove, bottom-right, shown only while the panel is selected
  StackView *footer = row("stageFooter");
  ((View *) footer)->alignment = ViewAlignmentBottomRight;

  this->removeButton = $(alloc(Button), initWithTitle, "Remove");
  this->removeButton->delegate.self = this;
  this->removeButton->delegate.didClick = didClickRemove;
  ((View *) this->removeButton)->hidden = !this->selected;
  $((View *) footer, addSubview, (View *) this->removeButton);
  release(this->removeButton);

  $(view, addSubview, (View *) footer);
  release(footer);

  rebuildEffects(this);
}

/**
 * @fn StageView *StageView::initWithStage(StageView *self, r_material_t *material, cm_stage_t *stage)
 * @memberof StageView
 */
static StageView *initWithStage(StageView *self, r_material_t *material, cm_stage_t *stage) {

  self = (StageView *) super(StackView, self, initWithFrame, NULL);
  if (self) {

    self->material = material;
    self->stage = stage;

    ((StackView *) self)->axis = StackViewAxisVertical;
    $((View *) self, addClassName, "stageView");

    buildControls(self);
  }

  return self;
}

/**
 * @fn void StageView::setSelected(StageView *self, bool selected)
 * @memberof StageView
 */
static void setSelected(StageView *self, bool selected) {

  self->selected = selected;

  if (self->removeButton) {
    ((View *) self->removeButton)->hidden = !selected;
    ((View *) self)->needsLayout = true;
  }

  if (selected) {
    $((View *) self, addClassName, "selected");
  } else {
    $((View *) self, removeClassName, "selected");
  }
}

#pragma mark - Class lifecycle

/**
 * @see Class::initialize(Class *)
 */
static void initialize(Class *clazz) {

  ((ViewInterface *) clazz->interface)->respondToEvent = respondToEvent;

  ((StageViewInterface *) clazz->interface)->initWithStage = initWithStage;
  ((StageViewInterface *) clazz->interface)->setSelected = setSelected;
  ((StageViewInterface *) clazz->interface)->rebuildEffects = rebuildEffects;
}

/**
 * @fn Class *StageView::_StageView(void)
 * @memberof StageView
 */
Class *_StageView(void) {
  static Class *clazz;
  static Once once;

  do_once(&once, {
    clazz = _initialize(&(const ClassDef) {
      .name = "StageView",
      .superclass = _StackView(),
      .instanceSize = sizeof(StageView),
      .interfaceOffset = offsetof(StageView, interface),
      .interfaceSize = sizeof(StageViewInterface),
      .initialize = initialize,
    });
  });

  return clazz;
}

#undef _Class
