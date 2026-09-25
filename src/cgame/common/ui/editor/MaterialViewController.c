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

#include "MaterialViewController.h"

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
    this->material->cm->alphaTest = slider->value;
  } else {
    Cg_Debug("Unknown Slider %p\n", (void *) slider);
    return;
  }

  this->material->cm->dirty = true;
}

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
};

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
static void stagesDidChange(MaterialViewController *this) {

  this->material->cm->dirty = true;

  cgi.ReloadMaterialStages(this->material);

  Cg_FreeFlares();
  Cg_LoadFlares();

  Cg_UpdateEditorMaterialLights(this->material->cm);
}

/**
 * @brief Selects the stage, and shows its values.
 */
static void setStage(MaterialViewController *this, CmStage *stage) {

  this->stage = stage;

  $(this->stages, selectOptionWithValue, stage);

  $(this->stageTexture, setAttributedText, stage && (stage->flags & STAGE_TEXTURE) ? stage->asset.name : "");

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

    View *box = $(view, descendantWithIdentifier, stageFlags[i].box);
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
static void reloadStages(MaterialViewController *this, CmStage *stage) {

  $(this->stages, removeAllOptions);

  if (this->material) {
    int32_t index = 0;
    for (CmStage *s = this->material->cm->stages; s; s = s->next, index++) {
      const char *name = *s->asset.name ? s->asset.name : ((s->flags & STAGE_LIGHT) ? "light" : "none");
      $(this->stages, addOption, va("%d: %s", index + 1, name), s);
    }
  }

  setStage(this, stage);
}

/**
 * @brief SelectDelegate callback for the stage selection.
 */
static void didSelectStage(Select *select, Option *option) {

  MaterialViewController *this = (MaterialViewController *) select->delegate.self;

  setStage(this, option ? option->value : NULL);
}

/**
 * @brief SelectDelegate callback for the blend factor selections.
 */
static void didSelectBlend(Select *select, Option *option) {

  MaterialViewController *this = (MaterialViewController *) select->delegate.self;

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

  MaterialViewController *this = (MaterialViewController *) button->delegate.self;

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

  MaterialViewController *this = (MaterialViewController *) button->delegate.self;

  if (!this->material || !this->stage) {
    return;
  }

  cgi.RemoveMaterialStage(this->material->cm, this->stage);
  this->stage = NULL;

  stagesDidChange(this);
  reloadStages(this, this->material->cm->stages);
}

/**
 * @brief TextViewDelegate callback for the stage texture.
 */
static void didEndEditingStageTexture(TextView *textView) {

  MaterialViewController *this = (MaterialViewController *) textView->delegate.self;

  if (!this->stage) {
    return;
  }

  const char *name = textView->attributedText->chars;

  if (*name) {
    q_strlcpy(this->stage->asset.name, name, sizeof(this->stage->asset.name));
    this->stage->flags |= STAGE_TEXTURE;
  } else {
    *this->stage->asset.name = '\0';
    this->stage->flags &= ~(STAGE_TEXTURE | STAGE_DRAW);
  }

  if (!cgi.ResolveMaterialStage(this->material->cm, this->stage)) {
    Cg_Warn("Failed to resolve stage texture %s\n", name);
  }

  stagesDidChange(this);
  reloadStages(this, this->stage);
}

/**
 * @brief Fs_Enumerator for completeStageTexture: adds a texture once, without its extension, or a
 * directory with a trailing slash. Normal, specular and tint maps are not stage textures.
 */
static void completeStageTexture_enumerate(const char *path, void *data) {

  Array *completions = data;

  const char *name = path + strlen("textures/");
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
 * @brief Comparator for completeStageTexture.
 */
static Order completeStageTexture_compare(const ident a, const ident b) {
  return (Order) Maxi(-1, Mini(1, q_strcmp(((String *) a)->chars, ((String *) b)->chars)));
}

/**
 * @brief TextViewDelegate callback for Tab in the stage texture: completes the text to the longest
 * prefix its matches share, and then cycles through the matches.
 * @return True if the text was completed, so that Tab does not advance.
 */
static bool completeStageTexture(TextView *textView) {

  MaterialViewController *this = (MaterialViewController *) textView->delegate.self;

  if (!this->material || this->material->cm->context != ASSET_CONTEXT_TEXTURES) {
    return false;
  }

  const char *text = textView->attributedText->chars ?: "";

  if (this->completions && this->completions->count > 1) {
    const String *current = $(this->completions, objectAtIndex, this->completion);
    if (!q_strcmp(current->chars, text)) {
      this->completion = (this->completion + 1) % this->completions->count;
      const String *next = $(this->completions, objectAtIndex, this->completion);
      $(textView, setAttributedText, next->chars);
      return true;
    }
  }

  release(this->completions);
  this->completions = $$(Array, array);
  this->completion = 0;

  cgi.EnumerateFiles(va("textures/%s*", text), completeStageTexture_enumerate, this->completions);

  if (this->completions->count == 0) {
    return false;
  }

  $(this->completions, sort, completeStageTexture_compare);

  const char *first = ((String *) $(this->completions, objectAtIndex, 0))->chars;

  size_t len = strlen(first);
  for (size_t i = 1; i < this->completions->count; i++) {
    const char *other = ((String *) $(this->completions, objectAtIndex, i))->chars;
    size_t j = 0;
    while (j < len && first[j] == other[j]) {
      j++;
    }
    len = j;
  }

  if (len > strlen(text)) {
    char prefix[MAX_QPATH];
    q_strlcpy(prefix, first, Mini(len + 1, sizeof(prefix)));
    $(textView, setAttributedText, prefix);
  } else {
    $(textView, setAttributedText, first);
  }

  return true;
}

/**
 * @brief CheckboxDelegate callback for the stage effects.
 */
static void didToggleStageFlag(Checkbox *checkbox) {

  MaterialViewController *this = (MaterialViewController *) checkbox->delegate.self;
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
  } else {
    this->stage->flags &= ~flag->flag;
  }

  cgi.ResolveMaterialStage(this->material->cm, this->stage);
  stagesDidChange(this);
  reloadStages(this, this->stage);
}

/**
 * @brief TextViewDelegate callback for the scroll and scale axes.
 */
static void didEndEditingStageAxis(TextView *textView) {

  MaterialViewController *this = (MaterialViewController *) textView->delegate.self;

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

  MaterialViewController *this = (MaterialViewController *) slider->delegate.self;

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

#pragma mark - Object

/**
 * @see Object::dealloc(Object *)
 */
static void dealloc(Object *self) {

  MaterialViewController *this = (MaterialViewController *) self;

  release(this->completions);

  super(Object, self, dealloc);
}

#pragma mark - ViewController

/**
 * @see ViewController::loadView(ViewController *)
 */
static void loadView(ViewController *self) {

  super(ViewController, self, loadView);

  MaterialViewController *this = (MaterialViewController *) self;

  Outlet outlets[] = MakeOutlets(
    MakeOutlet("name", &this->name),
    MakeOutlet("diffusemap", &this->diffusemap),
    MakeOutlet("normalmap", &this->normalmap),
    MakeOutlet("specularmap", &this->specularmap),
    MakeOutlet("roughness", &this->roughness),
    MakeOutlet("hardness", &this->hardness),
    MakeOutlet("specularity", &this->specularity),
    MakeOutlet("parallax", &this->parallax),
    MakeOutlet("shadow", &this->shadow),
    MakeOutlet("alphaTest", &this->alphaTest),
    MakeOutlet("stage", &this->stages),
    MakeOutlet("addStage", &this->addStage),
    MakeOutlet("removeStage", &this->removeStage),
    MakeOutlet("stageTexture", &this->stageTexture),
    MakeOutlet("stageBlendSrc", &this->stageBlendSrc),
    MakeOutlet("stageBlendDest", &this->stageBlendDest)
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

  this->stages->delegate.self = self;
  this->stages->delegate.didSelectOption = didSelectStage;

  this->addStage->delegate.self = self;
  this->addStage->delegate.didClick = didClickAddStage;

  this->removeStage->delegate.self = self;
  this->removeStage->delegate.didClick = didClickRemoveStage;

  this->stageTexture->delegate.self = self;
  this->stageTexture->delegate.didEndEditing = didEndEditingStageTexture;
  this->stageTexture->delegate.didTab = completeStageTexture;

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
    assert($(self->view, descendantWithIdentifier, stageFlags[i].box));

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

  MaterialViewController *this = (MaterialViewController *) self;

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

#pragma mark - MaterialViewController

/**
 * @fn MaterialViewController *MaterialViewController::init(MaterialViewController *)
 * @memberof MaterialViewController
 */
static MaterialViewController *init(MaterialViewController *self) {
  return (MaterialViewController *) super(ViewController, self, init);
}

/**
 * @fn void MaterialViewController::setMaterial(MaterialViewController *self, RenderMaterial *material)
 * @memberof MaterialViewController
 */
static void setMaterial(MaterialViewController *self, RenderMaterial *material) {

  self->material = material;

  if (self->material) {
    $(self->name, setDefaultText, self->material->cm->basename);
    $(self->diffusemap, setDefaultText, self->material->cm->diffusemap.name);
    $(self->normalmap, setDefaultText, self->material->cm->normalmap.name);
    $(self->specularmap, setDefaultText, self->material->cm->specularmap.name);

    $(self->roughness, setValue, (double) self->material->cm->roughness);
    $(self->hardness, setValue, (double) self->material->cm->hardness);
    $(self->specularity, setValue, (double) self->material->cm->specularity);
    $(self->parallax, setValue, (double) self->material->cm->parallax);
    $(self->shadow, setValue, (double) self->material->cm->shadow);
    $(self->alphaTest, setValue, (double) self->material->cm->alphaTest);

  } else {
    $(self->name, setDefaultText, NULL);
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

  reloadStages(self, self->material ? self->material->cm->stages : NULL);
}

#pragma mark - Class lifecycle

/**
 * @see Class::initialize(Class *)
 */
static void initialize(Class *clazz) {

  ((ObjectInterface *) clazz->interface)->dealloc = dealloc;

  ((ViewControllerInterface *) clazz->interface)->loadView = loadView;
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
      .interfaceSize = sizeof(MaterialViewControllerInterface),
      .initialize = initialize,
    });
  });

  return clazz;
}

#undef _Class
