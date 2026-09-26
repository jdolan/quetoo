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

#define _Class _StageView

#pragma mark - Delegates

/**
 * @brief A stage effect that a Checkbox enables.
 */
typedef struct {
  const char *identifier;
  const char *box;
  CmStageFlags flag;
  ptrdiff_t offset;
  float value;
} StageFlag;

/**
 * @brief The stage effects, and the parameter each one defaults when it is enabled at zero.
 */
static const StageFlag stageFlags[] = {
  { "stageBlend", "stageBlendBox", STAGE_BLEND, -1, 0.f },
  { "stageColor", "stageColorBox", STAGE_COLOR, offsetof(CmStage, color.a), 1.f },
  { "stagePulse", "stagePulseBox", STAGE_PULSE, offsetof(CmStage, pulse.hz), 1.f },
  { "stageScroll", "stageScrollBox", STAGE_SCROLL_S | STAGE_SCROLL_T, offsetof(CmStage, scroll.s), .25f },
  { "stageScale", "stageScaleBox", STAGE_SCALE_S | STAGE_SCALE_T, offsetof(CmStage, scale.s), 1.f },
  { "stageRotate", "stageRotateBox", STAGE_ROTATE, offsetof(CmStage, rotate.hz), .25f },
  { "stageStretch", "stageStretchBox", STAGE_STRETCH, offsetof(CmStage, stretch.hz), 1.f },
  { "stageWarp", "stageWarpBox", STAGE_WARP, offsetof(CmStage, warp.hz), 1.f },
  { "stageEmissive", "stageEmissiveBox", STAGE_EMISSIVE, offsetof(CmStage, emissive), 1.f },
  { "stageLighting", "stageLightingBox", STAGE_LIGHTING, offsetof(CmStage, lighting.intensity), 1.f },
  { "stageDirtmap", "stageDirtmapBox", STAGE_DIRTMAP, offsetof(CmStage, dirtmap.intensity), 1.f },
  { "stageLight", "stageLightBox", STAGE_LIGHT, offsetof(CmStage, light.intensity), STAGE_LIGHT_INTENSITY },
  { "stageFlare", NULL, STAGE_FLARE, -1, 0.f },
  { "stageEnvmap", "stageEnvmapBox", STAGE_ENVMAP, offsetof(CmStage, envmap.amount), STAGE_ENVMAP_AMOUNT },
};

/**
 * @brief The sprite that a stage flares with when the flare flag is checked.
 */
#define STAGE_FLARE_SPRITE "flare_1"

/**
 * @brief A stage parameter that a Slider edits.
 */
typedef struct {
  const char *identifier;
  ptrdiff_t offset;
  bool placement;
} StageParam;

/**
 * @brief The stage parameters. A parameter that affects light placement places the lights again.
 */
static const StageParam stageParams[] = {
  { "stageColorR", offsetof(CmStage, color.r), false },
  { "stageColorG", offsetof(CmStage, color.g), false },
  { "stageColorB", offsetof(CmStage, color.b), false },
  { "stageColorA", offsetof(CmStage, color.a), false },
  { "stagePulseHz", offsetof(CmStage, pulse.hz), false },
  { "stageRotateHz", offsetof(CmStage, rotate.hz), false },
  { "stageStretchAmplitude", offsetof(CmStage, stretch.amplitude), false },
  { "stageStretchHz", offsetof(CmStage, stretch.hz), false },
  { "stageWarpHz", offsetof(CmStage, warp.hz), false },
  { "stageWarpAmplitude", offsetof(CmStage, warp.amplitude), false },
  { "stageEnvmapAmount", offsetof(CmStage, envmap.amount), false },
  { "stageEmissiveValue", offsetof(CmStage, emissive), false },
  { "stageLightingIntensity", offsetof(CmStage, lighting.intensity), false },
  { "stageDirtmapIntensity", offsetof(CmStage, dirtmap.intensity), false },
  { "stageLightRadius", offsetof(CmStage, light.radius), true },
  { "stageLightIntensity", offsetof(CmStage, light.intensity), false },
  { "stageLightR", offsetof(CmStage, light.color.x), false },
  { "stageLightG", offsetof(CmStage, light.color.y), false },
  { "stageLightB", offsetof(CmStage, light.color.z), false },
};

/**
 * @brief One axis of a scroll or scale effect, which a TextView edits. The flag of the axis is set
 * only while its value is not zero, because the material parser rejects a zero value.
 */
typedef struct {
  const char *identifier;
  ptrdiff_t offset;
  CmStageFlags flag;
} StageAxis;

/**
 * @brief The scroll and scale axes.
 */
static const StageAxis stageAxes[] = {
  { "stageScrollSValue", offsetof(CmStage, scroll.s), STAGE_SCROLL_S },
  { "stageScrollTValue", offsetof(CmStage, scroll.t), STAGE_SCROLL_T },
  { "stageScaleSValue", offsetof(CmStage, scale.s), STAGE_SCALE_S },
  { "stageScaleTValue", offsetof(CmStage, scale.t), STAGE_SCALE_T },
};

/**
 * @brief The blend factors, in the order the blend selections list them.
 */
static const struct {
  const char *name;
  CmBlend blend;
} stageBlends[] = {
  { "one", BLEND_ONE },
  { "zero", BLEND_ZERO },
  { "src_alpha", BLEND_SRC_ALPHA },
  { "one_minus_src_alpha", BLEND_ONE_MINUS_SRC_ALPHA },
  { "src_color", BLEND_SRC_COLOR },
  { "dst_color", BLEND_DST_COLOR },
  { "one_minus_src_color", BLEND_ONE_MINUS_SRC_COLOR },
};

/**
 * @return The float parameter of the stage at the given offset.
 */
static float *stageFloat(CmStage *stage, ptrdiff_t offset) {
  return (float *) ((byte *) stage + offset);
}

/**
 * @brief Sets the flag of each axis within the mask from its value: a zero axis is not written.
 */
static void resolveStageAxes(CmStage *stage, CmStageFlags mask) {

  for (size_t i = 0; i < lengthof(stageAxes); i++) {
    if (stageAxes[i].flag & mask) {
      if (*stageFloat(stage, stageAxes[i].offset) != 0.f) {
        stage->flags |= stageAxes[i].flag;
      } else {
        stage->flags &= ~stageAxes[i].flag;
      }
    }
  }
}

/**
 * @return The asset name that the stage shows: `portal` or `reflect` for a subview stage, which
 * has no asset, and otherwise its texture or sprite.
 */
static const char *stageAssetName(const CmStage *stage) {

  if (stage->flags & STAGE_PORTAL) {
    return "portal";
  }

  if (stage->flags & STAGE_REFLECT) {
    return "reflect";
  }

  return stage->asset.name;
}

/**
 * @return The summary of the stage for its label: its index, its asset and its effects.
 */
static const char *summary(const StageView *this) {
  static char buf[MAX_STRING_CHARS];

  int32_t index = 1;
  for (const CmStage *s = this->material->cm->stages; s && s != this->stage; s = s->next) {
    index++;
  }

  const char *name = stageAssetName(this->stage);
  if (!*name) {
    name = (this->stage->flags & STAGE_LIGHT) ? "light" : "none";
  }

  q_snprintf(buf, sizeof(buf), "%d: %s", index, name);

  const char *sep = " (";
  for (size_t i = 0; i < lengthof(stageFlags); i++) {
    if (this->stage->flags & stageFlags[i].flag) {
      char flag[MAX_QPATH];
      q_strlcpy(flag, stageFlags[i].identifier + strlen("stage"), sizeof(flag));
      flag[0] = (char) tolower(flag[0]);
      q_strlcat(buf, va("%s%s", sep, flag), sizeof(buf));
      sep = ", ";
    }
  }

  if (strcmp(sep, " (")) {
    q_strlcat(buf, ")", sizeof(buf));
  }

  return buf;
}

/**
 * @brief Shows the values of the stage, and its summary.
 */
static void updateStage(StageView *this) {

  CmStage *stage = this->stage;

  $(this->stageTexture, setAttributedText, stageAssetName(stage));

  const View *view = (View *) this;

  for (size_t i = 0; i < lengthof(stageFlags); i++) {
    Control *control = (Control *) $(view, descendantWithIdentifier, stageFlags[i].identifier);
    const bool enabled = stage->flags & stageFlags[i].flag;

    if (control) {
      if (enabled) {
        control->state |= ControlStateSelected;
      } else {
        control->state &= ~ControlStateSelected;
      }
      $(control, stateDidChange);
    }

    View *box = stageFlags[i].box ? $(view, descendantWithIdentifier, stageFlags[i].box) : NULL;
    if (box) {
      if (enabled && !$(box, hasClassName, "enabled")) {
        $(box, addClassName, "enabled");
      } else if (!enabled) {
        $(box, removeClassName, "enabled");
      }
    }
  }

  for (size_t i = 0; i < lengthof(stageParams); i++) {
    Slider *slider = (Slider *) $(view, descendantWithIdentifier, stageParams[i].identifier);
    if (slider) {
      $(slider, setValue, (double) *stageFloat(stage, stageParams[i].offset));
    }
  }

  for (size_t i = 0; i < lengthof(stageAxes); i++) {
    TextView *textView = (TextView *) $(view, descendantWithIdentifier, stageAxes[i].identifier);
    if (textView) {
      $(textView, setAttributedText, va("%g", *stageFloat(stage, stageAxes[i].offset)));
    }
  }

  $(this->stageBlendSrc, selectOptionWithValue, (ident) (intptr_t) stage->blend.src);
  $(this->stageBlendDest, selectOptionWithValue, (ident) (intptr_t) stage->blend.dest);

  $(this->box.label->text, setText, summary(this));
}

/**
 * @brief SelectDelegate callback for the blend factor selections.
 */
static void didSelectBlend(Select *select, Option *option) {

  StageView *this = (StageView *) select->delegate.self;

  if (!this->stage || !option) {
    return;
  }

  const CmBlend blend = (CmBlend) (intptr_t) option->value;
  CmBlend *factor = select == this->stageBlendSrc ? &this->stage->blend.src : &this->stage->blend.dest;

  if (*factor == blend) {
    return;
  }

  *factor = blend;
  this->stage->flags |= STAGE_BLEND;

  cgi.ResolveMaterialStage(this->material->cm, this->stage);
  Cg_ReloadEditorMaterialStages(this->material);
  updateStage(this);
}

/**
 * @brief ButtonDelegate callback for the remove stage button.
 */
static void didClickRemoveStage(Button *button) {

  StageView *this = (StageView *) button->delegate.self;

  if (this->delegate.didRemoveStage) {
    this->delegate.didRemoveStage(this);
  }
}

/**
 * @brief TextViewDelegate callback for the stage asset: `portal` or `reflect` makes the stage a
 * subview, and otherwise the asset is a sprite for a flare stage, and a texture for any other
 * stage. An envmap applies to any of them but a flare.
 */
static void didEndEditingStageTexture(TextView *textView) {

  StageView *this = (StageView *) textView->delegate.self;

  if (!this->stage) {
    return;
  }

  const char *name = textView->attributedText->chars;

  if (!q_strcmp(name, stageAssetName(this->stage))) {
    return;
  }

  if (!q_strcmp(name, "portal") || !q_strcmp(name, "reflect")) {
    *this->stage->asset.name = '\0';
    *this->stage->asset.path = '\0';
    this->stage->flags &= ~(STAGE_TEXTURE | STAGE_DRAW | STAGE_FLARE | STAGE_ANIMATION | STAGE_MASK_SUBVIEW);
    this->stage->flags |= !q_strcmp(name, "portal") ? STAGE_PORTAL : STAGE_REFLECT;
  } else if (*name) {
    q_strlcpy(this->stage->asset.name, name, sizeof(this->stage->asset.name));
    this->stage->flags &= ~STAGE_MASK_SUBVIEW;
    if (!(this->stage->flags & STAGE_FLARE)) {
      this->stage->flags |= STAGE_TEXTURE;
    }
  } else {
    *this->stage->asset.name = '\0';
    *this->stage->asset.path = '\0';
    this->stage->flags &= ~(STAGE_TEXTURE | STAGE_DRAW | STAGE_FLARE | STAGE_MASK_SUBVIEW);
  }

  if (!cgi.ResolveMaterialStage(this->material->cm, this->stage)) {
    Cg_Warn("Failed to resolve stage asset %s\n", name);
  }

  Cg_ReloadEditorMaterialStages(this->material);
  updateStage(this);
}

/**
 * @brief The completions of the stage asset, and the directory that they are relative to.
 */
typedef struct {
  Array *completions;
  const char *dir;
} StageAssetCompletions;

/**
 * @brief Fs_Enumerator for completionsForStageTexture: adds an image once, without its
 * extension, or a directory with a trailing slash. Normal, specular and tint maps are not stage
 * assets.
 */
static void completionsForStageTexture_enumerate(const char *path, void *data) {

  StageAssetCompletions *assets = data;
  Array *completions = assets->completions;

  const char *name = path + strlen(assets->dir);
  const char *ext = strrchr(name, '.');

  char completion[MAX_QPATH];

  if (ext == NULL) {
    q_snprintf(completion, sizeof(completion), "%s/", name);
  } else {
    if (q_strcasecmp(ext, ".png") && q_strcasecmp(ext, ".jpg") && q_strcasecmp(ext, ".tga")) {
      return;
    }

    StripExtension(name, completion);

    const char *suffixes[] = { "_norm", "_spec", "_tint" };
    for (size_t i = 0; i < lengthof(suffixes); i++) {
      const size_t len = strlen(completion), slen = strlen(suffixes[i]);
      if (len > slen && !q_strcmp(completion + len - slen, suffixes[i])) {
        return;
      }
    }
  }

  for (size_t i = 0; i < completions->count; i++) {
    if (!q_strcmp(((String *) $(completions, objectAtIndex, i))->chars, completion)) {
      return;
    }
  }

  String *string = $$(String, stringWithCharacters, completion);
  $(completions, addObject, string);
  release(string);
}

/**
 * @brief Comparator for completionsForStageTexture.
 */
static Order completionsForStageTexture_compare(const ident a, const ident b) {
  return (Order) Maxi(-1, Mini(1, q_strcmp(((String *) a)->chars, ((String *) b)->chars)));
}

/**
 * @brief TextViewDelegate callback for the stage asset's completions: the sprites of a flare
 * stage, or the textures of a texture material, and their directories.
 */
static Array *completionsForStageTexture(TextView *textView, const char *prefix) {

  StageView *this = (StageView *) textView->delegate.self;

  if (!this->material) {
    return NULL;
  }

  StageAssetCompletions assets = { .dir = "textures/" };

  if (this->stage && (this->stage->flags & STAGE_FLARE)) {
    assets.dir = "sprites/";
  } else if (this->material->cm->context != ASSET_CONTEXT_TEXTURES) {
    return NULL;
  }

  assets.completions = $$(Array, array);

  cgi.EnumerateFiles(va("%s%s*", assets.dir, prefix), completionsForStageTexture_enumerate, &assets);

  $(assets.completions, sort, completionsForStageTexture_compare);

  return assets.completions;
}

/**
 * @brief CheckboxDelegate callback for the stage effects.
 */
static void didToggleStageFlag(Checkbox *checkbox) {

  StageView *this = (StageView *) checkbox->delegate.self;
  const StageFlag *flag = checkbox->delegate.data;

  if (!this->stage) {
    return;
  }

  if ($((Control *) checkbox, isSelected)) {
    this->stage->flags |= flag->flag;

    bool zero = true;
    for (size_t i = 0; i < lengthof(stageAxes); i++) {
      if ((stageAxes[i].flag & flag->flag) && *stageFloat(this->stage, stageAxes[i].offset) != 0.f) {
        zero = false;
      }
    }

    if (flag->offset >= 0 && zero && *stageFloat(this->stage, flag->offset) == 0.f) {
      *stageFloat(this->stage, flag->offset) = flag->value;
    }

    resolveStageAxes(this->stage, flag->flag);

    if (flag->flag == STAGE_COLOR && this->stage->color.r + this->stage->color.g + this->stage->color.b == 0.f) {
      this->stage->color = color_white;
    }

    if (flag->flag == STAGE_FLARE) {
      this->stage->flags &= ~(STAGE_TEXTURE | STAGE_DRAW | STAGE_ANIMATION | STAGE_ENVMAP);
      q_strlcpy(this->stage->asset.name, STAGE_FLARE_SPRITE, sizeof(this->stage->asset.name));
    }
  } else {
    this->stage->flags &= ~flag->flag;

    if (flag->flag == STAGE_FLARE) {
      this->stage->flags |= STAGE_TEXTURE;
      q_strlcpy(this->stage->asset.name, this->material->cm->basename, sizeof(this->stage->asset.name));
    }
  }

  cgi.ResolveMaterialStage(this->material->cm, this->stage);
  Cg_ReloadEditorMaterialStages(this->material);
  updateStage(this);
}

/**
 * @brief TextViewDelegate callback for the scroll and scale axes.
 */
static void didEndEditingStageAxis(TextView *textView) {

  StageView *this = (StageView *) textView->delegate.self;

  if (!this->stage) {
    return;
  }

  const StageAxis *axis = textView->delegate.data;
  float *value = stageFloat(this->stage, axis->offset);

  char *end;
  const float parsed = strtof(textView->attributedText->chars, &end);

  if (end == textView->attributedText->chars || parsed == *value) {
    $(textView, setAttributedText, va("%g", *value));
    return;
  }

  *value = parsed;
  resolveStageAxes(this->stage, axis->flag);

  cgi.ResolveMaterialStage(this->material->cm, this->stage);
  Cg_ReloadEditorMaterialStages(this->material);
  updateStage(this);
}

/**
 * @brief SliderDelegate callback for the stage parameters.
 */
static void didSetStageValue(Slider *slider, double value) {

  StageView *this = (StageView *) slider->delegate.self;

  if (!this->stage) {
    return;
  }

  const char *identifier = ((View *) slider)->identifier;

  for (size_t i = 0; i < lengthof(stageParams); i++) {
    if (!q_strcmp(identifier, stageParams[i].identifier)) {

      float *param = stageFloat(this->stage, stageParams[i].offset);
      if (*param == (float) value) {
        return;
      }

      *param = (float) value;
      this->material->cm->dirty = true;

      if (stageParams[i].placement) {
        Cg_UpdateEditorMaterialLights(this->material->cm);
      }

      return;
    }
  }

  Cg_Debug("Unknown Slider %s\n", identifier);
}

#pragma mark - View

/**
 * @see View::containsPoint(const View *, const SDL_Point *)
 * @remarks The label straddles the top border of the Box, so it is included, so that a click on
 * any part of it reaches this StageView.
 */
static bool containsPoint(const View *self, const SDL_Point *point) {
  return super(View, self, containsPoint, point) || $((View *) ((Box *) self)->label, containsPoint, point);
}

/**
 * @see View::respondToEvent(View *, const SDL_Event *)
 * @remarks A click on the label collapses or shows the details of the stage. Its label and asset
 * row always show.
 */
static void respondToEvent(View *self, const SDL_Event *event) {

  if (event->type == SDL_EVENT_MOUSE_BUTTON_UP && event->button.clicks) {
    if ($((View *) ((Box *) self)->label, didReceiveEvent, event)) {
      const bool collapsed = $(self, hasClassName, "collapsed");
      $((StageView *) self, setCollapsed, !collapsed);
    }
  }

  super(View, self, respondToEvent, event);
}

#pragma mark - StageView

/**
 * @fn StageView *StageView::initWithStage(StageView *self, RenderMaterial *material, CmStage *stage)
 * @memberof StageView
 */
static StageView *initWithStage(StageView *self, RenderMaterial *material, CmStage *stage) {

  self = (StageView *) super(Box, self, initWithFrame, NULL);
  if (self) {

    self->material = material;
    self->stage = stage;

    Outlet outlets[] = MakeOutlets(
      MakeOutlet("removeStage", &self->removeStage),
      MakeOutlet("stageTexture", &self->stageTexture),
      MakeOutlet("stageBlendSrc", &self->stageBlendSrc),
      MakeOutlet("stageBlendDest", &self->stageBlendDest)
    );

    $((View *) self, awakeWithResourceName, "ui/editor/StageView.json");
    $((View *) self, resolve, outlets);

    self->removeStage->delegate.self = self;
    self->removeStage->delegate.didClick = didClickRemoveStage;

    self->stageTexture->delegate.self = self;
    self->stageTexture->delegate.didEndEditing = didEndEditingStageTexture;
    self->stageTexture->delegate.completionsForPrefix = completionsForStageTexture;

    for (size_t i = 0; i < lengthof(stageBlends); i++) {
      $(self->stageBlendSrc, addOption, stageBlends[i].name, (ident) (intptr_t) stageBlends[i].blend);
      $(self->stageBlendDest, addOption, stageBlends[i].name, (ident) (intptr_t) stageBlends[i].blend);
    }

    self->stageBlendSrc->delegate.self = self;
    self->stageBlendSrc->delegate.didSelectOption = didSelectBlend;

    self->stageBlendDest->delegate.self = self;
    self->stageBlendDest->delegate.didSelectOption = didSelectBlend;

    for (size_t i = 0; i < lengthof(stageFlags); i++) {
      Checkbox *checkbox = (Checkbox *) $((View *) self, descendantWithIdentifier, stageFlags[i].identifier);
      assert(checkbox);
      assert(stageFlags[i].box == NULL || $((View *) self, descendantWithIdentifier, stageFlags[i].box));

      checkbox->delegate.self = self;
      checkbox->delegate.data = (ident) &stageFlags[i];
      checkbox->delegate.didToggle = didToggleStageFlag;
    }

    for (size_t i = 0; i < lengthof(stageAxes); i++) {
      TextView *textView = (TextView *) $((View *) self, descendantWithIdentifier, stageAxes[i].identifier);
      assert(textView);

      textView->delegate.self = self;
      textView->delegate.data = (ident) &stageAxes[i];
      textView->delegate.didEndEditing = didEndEditingStageAxis;
    }

    for (size_t i = 0; i < lengthof(stageParams); i++) {
      Slider *slider = (Slider *) $((View *) self, descendantWithIdentifier, stageParams[i].identifier);
      assert(slider);

      slider->delegate.self = self;
      slider->delegate.didSetValue = didSetStageValue;
    }

    updateStage(self);
  }

  return self;
}

/**
 * @fn void StageView::setCollapsed(StageView *self, bool collapsed)
 * @memberof StageView
 */
static void setCollapsed(StageView *self, bool collapsed) {

  View *view = (View *) self;

  if (collapsed && !$(view, hasClassName, "collapsed")) {
    $(view, addClassName, "collapsed");
  } else if (!collapsed) {
    $(view, removeClassName, "collapsed");
  }
}

/**
 * @fn void StageView::update(StageView *self)
 * @memberof StageView
 */
static void update(StageView *self) {
  updateStage(self);
}

#pragma mark - Class lifecycle

/**
 * @see Class::initialize(Class *)
 */
static void initialize(Class *clazz) {

  ((ViewInterface *) clazz->interface)->containsPoint = containsPoint;
  ((ViewInterface *) clazz->interface)->respondToEvent = respondToEvent;

  ((StageViewInterface *) clazz->interface)->initWithStage = initWithStage;
  ((StageViewInterface *) clazz->interface)->setCollapsed = setCollapsed;
  ((StageViewInterface *) clazz->interface)->update = update;
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
      .superclass = _Box(),
      .instanceSize = sizeof(StageView),
      .interfaceSize = sizeof(StageViewInterface),
      .initialize = initialize,
    });
  });

  return clazz;
}

#undef _Class
