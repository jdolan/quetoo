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

#include "StageViewController.h"

#define _Class _StageViewController

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
 * @brief Rebuilds the render stages, flares and light previews after the stages of the material
 * change, and marks the material dirty.
 */
static void stagesDidChange(StageViewController *this) {

  this->material->cm->dirty = true;

  cgi.ReloadMaterialStages(this->material);

  Cg_FreeFlares();
  Cg_LoadFlares();

  Cg_UpdateEditorMaterialLights(this->material->cm);
}

/**
 * @return The asset name that the stage shows: `portal` or `reflection` for a subview stage, which
 * has no asset, and otherwise its texture, sprite or envmap.
 */
static const char *stageAssetName(const CmStage *stage) {

  if (stage->flags & STAGE_PORTAL) {
    return "portal";
  }

  if (stage->flags & STAGE_REFLECTION) {
    return "reflection";
  }

  return stage->asset.name;
}

/**
 * @brief Selects the stage, and shows its values.
 */
static void setStage(StageViewController *this, CmStage *stage) {

  this->stage = stage;

  $(this->stages, selectOptionWithValue, stage);

  $(this->stageTexture, setAttributedText, stage ? stageAssetName(stage) : "");

  const View *view = this->viewController.view;

  for (size_t i = 0; i < lengthof(stageFlags); i++) {
    Control *control = (Control *) $(view, descendantWithIdentifier, stageFlags[i].identifier);
    const bool enabled = stage && (stage->flags & stageFlags[i].flag);

    if (control) {
      if (enabled) {
        control->state |= ControlStateSelected;
      } else {
        control->state &= ~ControlStateSelected;
      }
      if (stage) {
        control->state &= ~ControlStateDisabled;
      } else {
        control->state |= ControlStateDisabled;
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
      $(slider, setValue, stage ? (double) *stageFloat(stage, stageParams[i].offset) : 0.0);
    }
  }

  for (size_t i = 0; i < lengthof(stageAxes); i++) {
    TextView *textView = (TextView *) $(view, descendantWithIdentifier, stageAxes[i].identifier);
    if (textView) {
      $(textView, setAttributedText, stage ? va("%g", *stageFloat(stage, stageAxes[i].offset)) : "");
    }
  }

  $(this->stageBlendSrc, selectOptionWithValue, (ident) (intptr_t) (stage ? stage->blend.src : BLEND_INVALID));
  $(this->stageBlendDest, selectOptionWithValue, (ident) (intptr_t) (stage ? stage->blend.dest : BLEND_INVALID));
}

/**
 * @brief Lists the stages of the material in the stage selection, and selects the given stage.
 */
static void reloadStages(StageViewController *this, CmStage *stage) {

  $(this->stages, removeAllOptions);

  if (this->material) {
    int32_t index = 0;
    for (CmStage *s = this->material->cm->stages; s; s = s->next, index++) {
      const char *name = *stageAssetName(s) ? stageAssetName(s) : ((s->flags & STAGE_LIGHT) ? "light" : "none");
      if (s->flags & STAGE_FLARE) {
        $(this->stages, addOption, va("%d: flare %s", index + 1, name), s);
      } else {
        $(this->stages, addOption, va("%d: %s", index + 1, name), s);
      }
    }
  }

  setStage(this, stage);
}

/**
 * @brief SelectDelegate callback for the stage selection.
 */
static void didSelectStage(Select *select, Option *option) {

  StageViewController *this = (StageViewController *) select->delegate.self;

  setStage(this, option ? option->value : NULL);
}

/**
 * @brief SelectDelegate callback for the blend factor selections.
 */
static void didSelectBlend(Select *select, Option *option) {

  StageViewController *this = (StageViewController *) select->delegate.self;

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
  stagesDidChange(this);
  setStage(this, this->stage);
}

/**
 * @brief ButtonDelegate callback for the add stage button.
 */
static void didClickAddStage(Button *button) {

  StageViewController *this = (StageViewController *) button->delegate.self;

  if (!this->material) {
    return;
  }

  CmStage *stage = cgi.AddMaterialStage(this->material->cm);

  stagesDidChange(this);
  reloadStages(this, stage);
}

/**
 * @brief ButtonDelegate callback for the remove stage button.
 */
static void didClickRemoveStage(Button *button) {

  StageViewController *this = (StageViewController *) button->delegate.self;

  if (!this->material || !this->stage) {
    return;
  }

  cgi.RemoveMaterialStage(this->material->cm, this->stage);
  this->stage = NULL;

  stagesDidChange(this);
  reloadStages(this, this->material->cm->stages);
}

/**
 * @brief TextViewDelegate callback for the stage asset: `portal` or `reflection` makes the stage a
 * subview, and otherwise the asset is a sprite for a flare stage, an envmap for an envmap stage,
 * and a texture for any other stage.
 */
static void didEndEditingStageTexture(TextView *textView) {

  StageViewController *this = (StageViewController *) textView->delegate.self;

  if (!this->stage) {
    return;
  }

  const char *name = textView->attributedText->chars;

  if (!q_strcmp(name, stageAssetName(this->stage))) {
    return;
  }

  if (!q_strcmp(name, "portal") || !q_strcmp(name, "reflection")) {
    *this->stage->asset.name = '\0';
    *this->stage->asset.path = '\0';
    this->stage->flags &= ~(STAGE_TEXTURE | STAGE_DRAW | STAGE_FLARE | STAGE_ANIMATION | STAGE_ENVMAP | STAGE_MASK_SUBVIEW);
    this->stage->flags |= !q_strcmp(name, "portal") ? STAGE_PORTAL : STAGE_REFLECTION;
  } else if (*name) {
    q_strlcpy(this->stage->asset.name, name, sizeof(this->stage->asset.name));
    this->stage->flags &= ~STAGE_MASK_SUBVIEW;
    if (!(this->stage->flags & (STAGE_FLARE | STAGE_ENVMAP))) {
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

  stagesDidChange(this);
  reloadStages(this, this->stage);
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

  StageViewController *this = (StageViewController *) textView->delegate.self;

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

  StageViewController *this = (StageViewController *) checkbox->delegate.self;
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
  stagesDidChange(this);
  reloadStages(this, this->stage);
}

/**
 * @brief TextViewDelegate callback for the scroll and scale axes.
 */
static void didEndEditingStageAxis(TextView *textView) {

  StageViewController *this = (StageViewController *) textView->delegate.self;

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
  stagesDidChange(this);
  reloadStages(this, this->stage);
}

/**
 * @brief SliderDelegate callback for the stage parameters.
 */
static void didSetStageValue(Slider *slider, double value) {

  StageViewController *this = (StageViewController *) slider->delegate.self;

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

#pragma mark - ViewController

/**
 * @see ViewController::loadView(ViewController *)
 */
static void loadView(ViewController *self) {

  super(ViewController, self, loadView);

  StageViewController *this = (StageViewController *) self;

  Outlet outlets[] = MakeOutlets(
    MakeOutlet("stage", &this->stages),
    MakeOutlet("addStage", &this->addStage),
    MakeOutlet("removeStage", &this->removeStage),
    MakeOutlet("stageTexture", &this->stageTexture),
    MakeOutlet("stageBlendSrc", &this->stageBlendSrc),
    MakeOutlet("stageBlendDest", &this->stageBlendDest)
  );

  $(self->view, awakeWithResourceName, "ui/editor/StageViewController.json");
  $(self->view, resolve, outlets);

  self->view->stylesheet = $$(Stylesheet, stylesheetWithResourceName, "ui/editor/StageViewController.css");
  assert(self->view->stylesheet);

  this->stages->delegate.self = self;
  this->stages->delegate.didSelectOption = didSelectStage;

  this->addStage->delegate.self = self;
  this->addStage->delegate.didClick = didClickAddStage;

  this->removeStage->delegate.self = self;
  this->removeStage->delegate.didClick = didClickRemoveStage;

  this->stageTexture->delegate.self = self;
  this->stageTexture->delegate.didEndEditing = didEndEditingStageTexture;
  this->stageTexture->delegate.completionsForPrefix = completionsForStageTexture;

  for (size_t i = 0; i < lengthof(stageBlends); i++) {
    $(this->stageBlendSrc, addOption, stageBlends[i].name, (ident) (intptr_t) stageBlends[i].blend);
    $(this->stageBlendDest, addOption, stageBlends[i].name, (ident) (intptr_t) stageBlends[i].blend);
  }

  this->stageBlendSrc->delegate.self = self;
  this->stageBlendSrc->delegate.didSelectOption = didSelectBlend;

  this->stageBlendDest->delegate.self = self;
  this->stageBlendDest->delegate.didSelectOption = didSelectBlend;

  for (size_t i = 0; i < lengthof(stageFlags); i++) {
    Checkbox *checkbox = (Checkbox *) $(self->view, descendantWithIdentifier, stageFlags[i].identifier);
    assert(checkbox);
    assert(stageFlags[i].box == NULL || $(self->view, descendantWithIdentifier, stageFlags[i].box));

    checkbox->delegate.self = self;
    checkbox->delegate.data = (ident) &stageFlags[i];
    checkbox->delegate.didToggle = didToggleStageFlag;
  }

  for (size_t i = 0; i < lengthof(stageAxes); i++) {
    TextView *textView = (TextView *) $(self->view, descendantWithIdentifier, stageAxes[i].identifier);
    assert(textView);

    textView->delegate.self = self;
    textView->delegate.data = (ident) &stageAxes[i];
    textView->delegate.didEndEditing = didEndEditingStageAxis;
  }

  for (size_t i = 0; i < lengthof(stageParams); i++) {
    Slider *slider = (Slider *) $(self->view, descendantWithIdentifier, stageParams[i].identifier);
    assert(slider);

    slider->delegate.self = self;
    slider->delegate.didSetValue = didSetStageValue;
  }
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
  return (StageViewController *) super(ViewController, self, init);
}

/**
 * @fn void StageViewController::setMaterial(StageViewController *self, RenderMaterial *material)
 * @memberof StageViewController
 */
static void setMaterial(StageViewController *self, RenderMaterial *material) {

  self->material = material;

  reloadStages(self, self->material ? self->material->cm->stages : NULL);
}

#pragma mark - Class lifecycle

/**
 * @see Class::initialize(Class *)
 */
static void initialize(Class *clazz) {

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
