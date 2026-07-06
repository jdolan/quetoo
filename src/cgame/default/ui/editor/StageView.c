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

#include "KeyValueView.h"
#include "KeyValueTableView.h"

#include <ObjectivelyMVC/Label.h>
#include <ObjectivelyMVC/TextView.h>

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
 * @details `addable` effects may be created from the Add Effect dropdown. All
 * effects are addable; Animation is special-cased on add (its frame count is
 * derived from the texture, and it is skipped if the texture is not a valid
 * numbered sequence) -- see didSelectAddEffect.
 */
typedef struct {
  cm_stage_flags_t flag;
  const char *name;
  const stage_param_desc_t *params;
  size_t num_params;
  bool addable;
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
static const stage_param_desc_t scroll_params[] = {
  STAGE_PARAM(scroll.s, -10.0, 10.0, 0.1), STAGE_PARAM(scroll.t, -10.0, 10.0, 0.1),
};
static const stage_param_desc_t scale_params[] = {
  STAGE_PARAM(scale.s, 0.0, 16.0, 0.1), STAGE_PARAM(scale.t, 0.0, 16.0, 0.1),
};
static const stage_param_desc_t terrain_params[] = {
  STAGE_PARAM(terrain.floor, -2048.0, 2048.0, 8.0), STAGE_PARAM(terrain.ceil, -2048.0, 2048.0, 8.0),
};
static const stage_param_desc_t dirtmap_params[] = { STAGE_PARAM(dirtmap.intensity, 0.0, 1.0, 0.01) };
static const stage_param_desc_t warp_params[] = {
  STAGE_PARAM(warp.hz, 0.0, 10.0, 0.1), STAGE_PARAM(warp.amplitude, 0.0, 10.0, 0.1),
};
static const stage_param_desc_t lighting_params[] = { STAGE_PARAM(lighting.intensity, 0.0, 2.0, 0.1) };
static const stage_param_desc_t emissive_params[] = { STAGE_PARAM(emissive, 0.0, 1.0, 0.01) };
static const stage_param_desc_t shell_params[] = { STAGE_PARAM(shell.radius, 0.0, 100.0, 1.0) };
static const stage_param_desc_t animation_params[] = {
  STAGE_PARAM(animation.fps, 0.0, 60.0, 1.0), STAGE_PARAM(animation.drift, 0.0, 10.0, 0.1),
};

#define STAGE_EFFECT(f, n, p) { f, n, p, lengthof(p), true }
#define STAGE_EFFECT_TOGGLE(f, n) { f, n, NULL, 0, true }

static const stage_effect_desc_t stage_effects[] = {
  STAGE_EFFECT(STAGE_ANIMATION, "Animation", animation_params),
  STAGE_EFFECT_TOGGLE(STAGE_BLEND, "Blend"),
  STAGE_EFFECT(STAGE_COLOR, "Color", color_params),
  STAGE_EFFECT(STAGE_DIRTMAP, "Dirtmap", dirtmap_params),
  STAGE_EFFECT(STAGE_EMISSIVE, "Emissive", emissive_params),
  STAGE_EFFECT_TOGGLE(STAGE_ENVMAP, "Envmap"),
  STAGE_EFFECT_TOGGLE(STAGE_FLARE, "Flare"),
  STAGE_EFFECT(STAGE_LIGHTING, "Lighting", lighting_params),
  STAGE_EFFECT_TOGGLE(STAGE_LIGHTING_FLAT, "Lighting Flat"),
  STAGE_EFFECT(STAGE_PULSE, "Pulse", pulse_params),
  STAGE_EFFECT(STAGE_ROTATE, "Rotate", rotate_params),
  STAGE_EFFECT(STAGE_SCALE_S | STAGE_SCALE_T, "Scale", scale_params),
  STAGE_EFFECT(STAGE_SCROLL_S | STAGE_SCROLL_T, "Scroll", scroll_params),
  STAGE_EFFECT(STAGE_STRETCH, "Stretch", stretch_params),
  STAGE_EFFECT(STAGE_TERRAIN, "Terrain", terrain_params),
  STAGE_EFFECT(STAGE_WARP, "Warp", warp_params),
};

/**
 * @brief One selectable OpenGL blend factor for the Blend effect's src/dest.
 */
typedef struct {
  const char *name;
  uint32_t value;
} stage_blend_factor_t;

/**
 * @brief The blend factors offered by the Blend effect, mirrored from the material
 * parser's table (`cm_blendConstList` in cm_material.c). The values are the fixed
 * OpenGL enum constants, duplicated here so this cgame view needs no GL header.
 */
static const stage_blend_factor_t stage_blend_factors[] = {
  { "GL_ONE",                 0x0001 },
  { "GL_ZERO",                0x0000 },
  { "GL_SRC_ALPHA",           0x0302 },
  { "GL_ONE_MINUS_SRC_ALPHA", 0x0303 },
  { "GL_SRC_COLOR",           0x0300 },
  { "GL_DST_COLOR",           0x0306 },
  { "GL_ONE_MINUS_SRC_COLOR", 0x0301 },
};

/**
 * @brief Side length, in points, of a small square icon button (the effect remove
 * "X"). CSS width/height do not bind to these dynamically-built buttons, so their
 * size is the one sanctioned C fallback: clamping the widget's own minSize/maxSize,
 * which View::resize honors regardless of the stylesheet. Everything else about the
 * effect rows -- columns, spacing, colors -- is CSS-driven (see editor.css).
 */
#define STAGE_ICON_BUTTON_SIZE 17

/**
 * @brief Texture-field validation feedback colors. The "valid" background matches
 * the base `TextView` rule in common.css (#dedede10) so a resolving path looks
 * normal; "invalid" is a translucent maroon tint applied when the typed path does
 * not resolve to an asset on disk.
 */
#define TEXTURE_FIELD_BG_VALID   ((SDL_Color) { 0xde, 0xde, 0xde, 0x10 })
#define TEXTURE_FIELD_BG_INVALID ((SDL_Color) { 0x80, 0x30, 0x30, 0x66 })

/**
 * @brief Value prefilled into a flare/envmap asset field when that effect is added,
 * so the user only completes the name. A flare's context auto-prepends `sprites/`,
 * so its field starts empty; an envmap's context only prepends `textures/`, so it is
 * seeded with the conventional `envmaps/` subfolder.
 */
#define STAGE_FLARE_ASSET_PREFIX  ""
#define STAGE_ENVMAP_ASSET_PREFIX "envmaps/"

#pragma mark - Internal helpers

/**
 * @brief Tints a texture input's background maroon when its text does not resolve
 * to an asset on disk. Empty text is treated as neutral (not invalid). This is a
 * purely visual cue; it does not change the stage.
 */
/**
 * @brief Resolves a stage asset name in the context appropriate to the stage's
 * type: a flare sprite under `sprites/`, an envmap texture under `textures/`, and a
 * plain textured stage in the material's own context.
 */
static bool stageAssetResolves(const StageView *this, const char *name) {

  if (this->stage->flags & STAGE_FLARE) {
    return cgi.AssetExists(name, ASSET_CONTEXT_SPRITES);
  }
  if (this->stage->flags & STAGE_ENVMAP) {
    return cgi.AssetExists(name, ASSET_CONTEXT_TEXTURES);
  }
  return cgi.MaterialAssetExists(this->material, name);
}

static void validateTextureField(StageView *this, TextView *textView) {

  const char *name = (textView->attributedText && textView->attributedText->chars)
    ? textView->attributedText->chars : "";
  const bool ok = (*name == '\0') || stageAssetResolves(this, name);

  ((View *) textView)->backgroundColor = ok ? TEXTURE_FIELD_BG_VALID : TEXTURE_FIELD_BG_INVALID;
}

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
 * @brief Creates a small square "X" remove button (icon fills the button). The
 * fixed size is the one sanctioned C fallback -- CSS width does not bind to these
 * dynamically-built buttons. Caller owns the returned (retained) button.
 */
static Button *makeRemoveButton(StageView *this, void (*didClick)(Button *)) {

  // The icon PNG is authored small (STAGE_ICON_BUTTON_SIZE), so the button sizes to
  // it. Chrome (transparent bg, no border/padding) is styled via editor.css
  // (#material .iconButton) -- CSS scoped by tab id binds where per-tab css does not.
  Button *remove = $(alloc(Button), initWithFrame, NULL);
  $((View *) remove, addClassName, "iconButton");
  $(remove->image, setImageWithResourceName, "ui/editor/icon_delete.png");

  // Size the button to the icon. Left alone, the Button's (empty) title Text
  // inflates it to the font line-height, so it dwarfs the icon. Clamp min == max to
  // the icon size so sizeThatFits (and any later resize) lands exactly there, and
  // let the ImageView fill the button so the icon fully covers it. This C sizing is
  // the one sanctioned fallback -- CSS width does not bind to a dynamically-built
  // button.
  const SDL_Size iconSize = MakeSize(STAGE_ICON_BUTTON_SIZE, STAGE_ICON_BUTTON_SIZE);
  ((View *) remove)->minSize = iconSize;
  ((View *) remove)->maxSize = iconSize;
  ((View *) remove->image)->autoresizingMask |= ViewAutoresizingFill;
  $((View *) remove, sizeToFit);

  remove->delegate.self = this;
  remove->delegate.didClick = didClick;

  return remove;
}

static Button* makeAddButton(StageView* this, void (*didClick)(Button*)) {
    Button* add = $(alloc(Button), initWithFrame, NULL);
    $((View*)add, addClassName, "iconButton");
    $(add->image, setImageWithResourceName, "ui/editor/icon_add.png");

    const SDL_Size iconSize = MakeSize(STAGE_ICON_BUTTON_SIZE, STAGE_ICON_BUTTON_SIZE);
    ((View*)add)->minSize = iconSize;
    ((View*)add)->maxSize = iconSize;
    ((View*)add->image)->autoresizingMask |= ViewAutoresizingFill;
    $((View*)add, sizeToFit);

    add->delegate.self = this;
    add->delegate.didClick = didClick;

    return add;
}

/**
 * @brief Builds a full-width header bar: a left-aligned title label and a
 * right-aligned remove button. Uses a plain View rather than a StackView -- a
 * horizontal StackView lays its children out left-to-right and cannot flush one
 * to the right, whereas the base View::layoutSubviews honors each child's
 * alignment. The Contain mask sizes the bar to its content height; the Width mask
 * spans it across the parent so the background reaches the panel edge and the X
 * sits flush right. Masks are set in C because a CSS-set width mask misapplies on
 * this tree. The (retained) remove button is returned via `remove` for the caller
 * to wire/track; it is already added as a subview of the returned header.
 */
static View *makeHeaderBar(StageView *this, const char *className, const char *title,
                           void (*didClickRemove)(Button *), Button **remove) {

  View *header = $(alloc(View), initWithFrame, NULL);
  $(header, addClassName, className);
  header->autoresizingMask = ViewAutoresizingContain | ViewAutoresizingWidth;

  Label *label = $(alloc(Label), initWithText, title, NULL);
  ((View *) label)->alignment = ViewAlignmentMiddleLeft;
  $(header, addSubview, (View *) label);
  release(label);

  *remove = makeRemoveButton(this, didClickRemove);
  ((View *) *remove)->alignment = ViewAlignmentMiddleRight;
  $(header, addSubview, (View *) *remove);

  return header;
}

/**
 * @brief Appends a `label -> blend-factor dropdown` row to the effect's property
 * table, preselecting `current` and routing changes to `didSelect`.
 */
static void addBlendRow(StageView *this, KeyValueTableView *table, const char *label, uint32_t current,
                        void (*didSelect)(Select *, Option *)) {

  Label *lbl = $(alloc(Label), initWithText, label, NULL);

  Select *select = $(alloc(Select), initWithFrame, NULL);
  for (size_t i = 0; i < lengthof(stage_blend_factors); i++) {
    $(select, addOption, stage_blend_factors[i].name, (ident) (intptr_t) stage_blend_factors[i].value);
  }
  $(select, selectOptionWithValue, (ident) (intptr_t) current);

  select->delegate.self = this;
  select->delegate.didSelectOption = didSelect;

  $(table, addRow, (View *) lbl, (View *) select);
  release(lbl);
  release(select);
}

/**
 * @brief Returns the first effect flag not yet active on the stage, or `STAGE_NONE`.
 */
static cm_stage_flags_t firstAvailableEffect(const StageView *this) {
  for (size_t i = 0; i < lengthof(stage_effects); i++) {
    if (stage_effects[i].addable && !(this->stage->flags & stage_effects[i].flag)) {
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
 * @brief SelectDelegate: set the blend source factor. glBlendFunc reads it live.
 */
static void didSelectBlendSrc(Select *select, Option *option) {

  StageView *this = (StageView *) select->delegate.self;

  this->stage->blend.src = (uint32_t) (intptr_t) option->value;
  this->material->cm->dirty = true;
}

/**
 * @brief SelectDelegate: set the blend destination factor. glBlendFunc reads it live.
 */
static void didSelectBlendDest(Select *select, Option *option) {

  StageView *this = (StageView *) select->delegate.self;

  this->stage->blend.dest = (uint32_t) (intptr_t) option->value;
  this->material->cm->dirty = true;
}

/**
 * @brief CheckboxDelegate: toggle animation frame interpolation. The flags uniform
 * is read live, so no rebuild is needed.
 */
static void didToggleAnimLerp(Checkbox *checkbox) {

  StageView *this = (StageView *) checkbox->delegate.self;

  if ($((Control *) checkbox, isSelected)) {
    this->stage->flags |= STAGE_ANIM_LERP;
  } else {
    this->stage->flags &= ~STAGE_ANIM_LERP;
  }

  this->material->cm->dirty = true;
}

/**
 * @brief ButtonDelegate: remove an effect from the stage.
 */
static void didClickRemoveEffect(Button *button) {

  StageView *this = (StageView *) button->delegate.self;

  for (size_t i = 0; i < this->numEffects; i++) {
    if (this->effects[i].removeButton == button) {
      const cm_stage_flags_t removed = this->effects[i].flag;
      this->stage->flags &= ~removed;

      // Removing a flare/envmap leaves the stage with no asset source; return it to
      // a plain textured stage seeded with the material's diffusemap.
      if (removed & (STAGE_FLARE | STAGE_ENVMAP)) {
        this->stage->flags |= STAGE_TEXTURE;
        cgi.SetMaterialStageTexture(this->material, this->stage, this->material->cm->diffusemap.name);
      }

      cgi.FinalizeMaterialStage(this->material, this->stage);
      requestRebuild(this);
      return;
    }
  }
}

/**
 * @brief SelectDelegate: add the effect chosen from the Add Effect dropdown. The
 * placeholder option (STAGE_NONE) is a no-op. This is the only user action that
 * grows the stage, so it is the only one that requests a (deferred) rebuild.
 */
static void didSelectAddEffect(Select *select, Option *option) {

  StageView *this = (StageView *) select->delegate.self;

  const cm_stage_flags_t flag = (cm_stage_flags_t) (intptr_t) option->value;
  if (flag == STAGE_NONE) {
    return;
  }

  this->stage->flags |= flag;

  // Flare and envmap are stage TYPES that share the one asset slot, so adding one
  // makes the stage that type: drop the base texture and the other type (they are
  // mutually exclusive), and prefill the asset with its directory so the user just
  // completes the name.
  if (flag == STAGE_FLARE) {
    this->stage->flags &= ~(STAGE_TEXTURE | STAGE_ENVMAP);
    cgi.SetMaterialStageTexture(this->material, this->stage, STAGE_FLARE_ASSET_PREFIX);
  } else if (flag == STAGE_ENVMAP) {
    this->stage->flags &= ~(STAGE_TEXTURE | STAGE_FLARE);
    cgi.SetMaterialStageTexture(this->material, this->stage, STAGE_ENVMAP_ASSET_PREFIX);
  }

  // Animation's frame count is derived from its texture (a numbered sequence on
  // disk). Resolve it now; if the current texture is not such a sequence, skip
  // the effect rather than leave a 0-frame animation. The user can point the
  // stage at a valid sequence texture first, then add Animation.
  if (flag == STAGE_ANIMATION) {
    if (cgi.ResolveMaterialStageAnimation(this->material, this->stage) == 0) {
      this->stage->flags &= ~STAGE_ANIMATION;
      Cg_Debug("Stage texture '%s' is not a numbered animation sequence; skipping\n",
               this->stage->asset.name);
    }
  }

  cgi.FinalizeMaterialStage(this->material, this->stage);

  requestRebuild(this);
}

/**
 * @brief ButtonDelegate: remove this whole stage. Deferred to the MaterialView-
 * Controller via a notification, because acting now would free this view (and the
 * button) from inside its own click handler.
 */
static void didClickRemoveStage(Button *button) {

  StageView *this = (StageView *) button->delegate.self;

  SDL_PushEvent(&(SDL_Event) {
    .user.type = MVC_NOTIFICATION_EVENT,
    .user.code = NOTIFICATION_MATERIAL_STAGE_REMOVED,
    .user.data1 = this,
  });
}

/**
 * @brief TextViewDelegate (didEdit): validate the texture path live as the user
 * types, tinting the field when it does not resolve. The stage is not retargeted
 * until editing ends (and only then if the path is valid).
 */
static void didEditTextureLive(TextView *textView) {

  StageView *this = (StageView *) textView->delegate.self;

  validateTextureField(this, textView);
}

/**
 * @brief TextViewDelegate (didEndEditing): retarget the stage's texture to the
 * typed name, but only if it resolves -- an invalid (or empty) path is left tinted
 * and not applied, so the stage keeps its current texture. The import re-resolves
 * the asset and rebuilds the render stages so a valid change loads live.
 */
static void didEditTexture(TextView *textView) {

  StageView *this = (StageView *) textView->delegate.self;

  const char *name = (textView->attributedText && textView->attributedText->chars)
    ? textView->attributedText->chars : "";

  validateTextureField(this, textView);

  if (!*name || !stageAssetResolves(this, name)) {
    if (*name) {
      Cg_Debug("Asset '%s' does not resolve; not applied\n", name);
    }
    return;
  }

  cgi.SetMaterialStageTexture(this->material, this->stage, name);
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
 * @brief Appends a `label -> editable asset name` row to the given table, wired to
 * the stage's single texture slot with live path validation (in the stage's asset
 * context). Used for the stage's Texture row and for the flare/envmap asset rows,
 * which all edit the one `stage->asset`.
 */
static void addAssetRow(StageView *this, KeyValueTableView *table, const char *label) {

  Label *lbl = $(alloc(Label), initWithText, label, NULL);

  TextView *texture = $(alloc(TextView), initWithFrame, NULL);
  texture->isEditable = true;
  $(texture, setAttributedText, this->stage->asset.name);
  texture->delegate.self = this;
  texture->delegate.didEdit = didEditTextureLive;
  texture->delegate.didEndEditing = didEditTexture;
  validateTextureField(this, texture);

  $(table, addRow, (View *) lbl, (View *) texture);
  release(lbl);
  release(texture);
}

/**
 * @brief Appends a `label -> slider` row for one float parameter to the effect's
 * property table, binding the slider to the stage field it edits.
 */
static void addParamRow(StageView *this, KeyValueTableView *table, const stage_param_desc_t *param) {

  // The descriptor label is the full struct field path (e.g. "scroll.s"); show only
  // the leaf property ("s") in the key column to save space -- the effect box header
  // already names the effect.
  const char *dot = strrchr(param->label, '.');
  Label *lbl = $(alloc(Label), initWithText, dot ? dot + 1 : param->label, NULL);

  Slider *slider = $(alloc(Slider), initWithFrame, NULL);
  slider->min = param->min;
  slider->max = param->max;
  slider->step = param->step;
  slider->snapToStep = true; // quantize drag to the step, not just click/arrow keys
  $(slider, setLabelFormat, "%.2f");

  float *target = (float *) ((uint8_t *) this->stage + param->offset);
  $(slider, setValue, (double) *target);

  slider->delegate.self = this;
  slider->delegate.didSetValue = didSetParam;

  $(table, addRow, (View *) lbl, (View *) slider);
  release(lbl);
  release(slider);

  assert(this->numParams < STAGE_VIEW_MAX_PARAMS);
  this->params[this->numParams].slider = slider;
  this->params[this->numParams].target = target;
  this->numParams++;
}

/**
 * @brief Builds one active effect: a header (name + small remove button) and a
 * KeyValueTableView of the effect's property rows. The table owns its columns via
 * CSS (`.stageEffectTable` in editor.css), so every effect's labels and value
 * widgets line up; only the remove button's size is set in C (CSS width does not
 * bind to a dynamically-built button).
 */
static void addEffectGroup(StageView *this, const stage_effect_desc_t *effect) {

  // each effect is a bordered box (styled via #material .stageEffect in editor.css)
  StackView *group = $(alloc(StackView), initWithFrame, NULL);
  ((StackView *) group)->axis = StackViewAxisVertical;
  $((View *) group, addClassName, "stageEffect");

  // Span the effects container width so toggle-only effects (no param table, e.g.
  // Envmap / Flare / Lighting Flat) still fill the row instead of shrinking to
  // their header text. Set in C -- a CSS width mask misapplies on these stacks.
  ((View *) group)->autoresizingMask = ViewAutoresizingContain | ViewAutoresizingWidth;

  // header: [ effect name ......... X ], full width with the X flush right
  Button *remove;
  View *header = makeHeaderBar(this, "stageEffectHeader", effect->name, didClickRemoveEffect, &remove);

  $((View *) group, addSubview, header);
  release(header);

  assert(this->numEffects < STAGE_VIEW_MAX_EFFECTS);
  this->effects[this->numEffects].removeButton = remove;
  this->effects[this->numEffects].flag = effect->flag;
  this->numEffects++;

  release(remove);

  // property rows, in a table whose columns are inherited (and CSS-configured)
  KeyValueTableView *table = $(alloc(KeyValueTableView), initWithFrame, NULL);
  $((View *) table, addClassName, "stageEffectTable");

  for (size_t p = 0; p < effect->num_params; p++) {
    addParamRow(this, table, &effect->params[p]);
  }

  if (effect->flag == STAGE_BLEND) {
    addBlendRow(this, table, "src", this->stage->blend.src, didSelectBlendSrc);
    addBlendRow(this, table, "dest", this->stage->blend.dest, didSelectBlendDest);
  } else if (effect->flag == STAGE_ANIMATION) {

    // frame interpolation is a boolean sub-option of the animation effect
    Label *lbl = $(alloc(Label), initWithText, "lerp", NULL);

    Checkbox *lerp = $(alloc(Checkbox), initWithFrame, NULL);
    if (this->stage->flags & STAGE_ANIM_LERP) {
      ((Control *) lerp)->state |= ControlStateSelected;
    }
    lerp->delegate.self = this;
    lerp->delegate.didToggle = didToggleAnimLerp;

    $(table, addRow, (View *) lbl, (View *) lerp);
    release(lbl);
    release(lerp);
  } else if (effect->flag == STAGE_FLARE) {
    // CAVEAT: flares are NOT live-editable. Cg_LoadFlares (cg_flare.c) bakes the
    // flare list once at map load by scanning BSP faces, and an effect toggle does
    // not recompute the material's stage_flags. A flare edit therefore only takes
    // effect after saving the .mat and reloading the map. Deliberately not made
    // live (would require a full flare-system regen per edit).
    addAssetRow(this, table, "sprite");
  } else if (effect->flag == STAGE_ENVMAP) {
    addAssetRow(this, table, "texture");
  }

  $((View *) group, addSubview, (View *) table);
  release(table);

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
    if (!(self->stage->flags & stage_effects[e].flag)) {
      continue;
    }
    // Envmap mandates flat lighting (Cm_FinalizeStage couples them), so it is not an
    // independent, removable effect on an envmap stage -- don't list it there.
    if (stage_effects[e].flag == STAGE_LIGHTING_FLAT && (self->stage->flags & STAGE_ENVMAP)) {
      continue;
    }
    addEffectGroup(self, &stage_effects[e]);
  }

  // repopulate the Add Effect dropdown with the effects not yet on the stage.
  // The placeholder (STAGE_NONE) keeps the control showing "Add Effect" and is
  // re-selected each rebuild so the same effect can be re-picked later.
  $(self->addEffect, removeAllOptions);
  $(self->addEffect, addOption, "Add Effect", (ident) (intptr_t) STAGE_NONE);
  for (size_t e = 0; e < lengthof(stage_effects); e++) {
    if (!stage_effects[e].addable || (self->stage->flags & stage_effects[e].flag)) {
      continue;
    }
    // flare and envmap are mutually exclusive stage types (one shared asset slot):
    // don't offer one while the other is active.
    if (stage_effects[e].flag == STAGE_FLARE && (self->stage->flags & STAGE_ENVMAP)) {
      continue;
    }
    if (stage_effects[e].flag == STAGE_ENVMAP && (self->stage->flags & STAGE_FLARE)) {
      continue;
    }
    $(self->addEffect, addOption, stage_effects[e].name, (ident) (intptr_t) stage_effects[e].flag);
  }
  $(self->addEffect, selectOptionWithValue, (ident) (intptr_t) STAGE_NONE);

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

  // header bar: "Stage N" (left) + a remove-stage X (flush right), full width
  Button *removeStage;
  View *header = makeHeaderBar(this, "stageHeader", va("Stage %d", stageIndex(this) + 1),
                              didClickRemoveStage, &removeStage);
  release(removeStage);
  $(view, addSubview, header);
  release(header);

  // Editable texture row -- only for a plain textured stage. A flare/envmap stage
  // carries its single asset in its own effect box instead (see addEffectGroup), so
  // it is not shown (and not written as `texture ...`) here.
  if (this->stage->flags & STAGE_TEXTURE) {
    KeyValueTableView *texTable = $(alloc(KeyValueTableView), initWithFrame, NULL);
    $((View *) texTable, addClassName, "stageEffectTable");

    addAssetRow(this, texTable, "Texture");

    $(view, addSubview, (View *) texTable);
    release(texTable);
  }

  // container for the active effect rows
  this->effectsContainer = $(alloc(StackView), initWithFrame, NULL);
  ((StackView *) this->effectsContainer)->axis = StackViewAxisVertical;
  $((View *) this->effectsContainer, addClassName, "stageEffects");
  $(view, addSubview, (View *) this->effectsContainer);
  release(this->effectsContainer);

  // Add Effect dropdown (options are the not-yet-active effects; filled in rebuildEffects)
  StackView *addRow = row("stageAddEffect");
  this->addEffect = $(alloc(Select), initWithFrame, NULL);
  this->addEffect->delegate.self = this;
  this->addEffect->delegate.didSelectOption = didSelectAddEffect;
  $((View *) addRow, addSubview, (View *) this->addEffect);
  release(this->addEffect);
  $(view, addSubview, (View *) addRow);
  release(addRow);

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

    // Fill the stages list width so headers/rows span the panel. Contain|Width is
    // the same combo the header bars use. This is safe (no width ratchet) only
    // because the list carries no right padding: Contain here settles at the list
    // bounds instead of `bounds + padding`. Set in C -- CSS mask inlets do not
    // bind to these dynamically-built views.
    ((View *) self)->autoresizingMask = ViewAutoresizingContain | ViewAutoresizingWidth;

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

  if (selected) {
    $((View *) self, addClassName, "selected");
  } else {
    $((View *) self, removeClassName, "selected");
  }

  ((View *) self)->needsLayout = true;
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
  ((StageViewInterface *) clazz->interface)->rebuild = buildControls;
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
