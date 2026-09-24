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
  CmStageFlags flag;
  ptrdiff_t offset;
  float value;
} StageFlag;

/**
 * @brief The stage effects, and the parameter each one defaults when it is enabled at zero.
 */
static const StageFlag stageFlags[] = {
  { "stageBlend", STAGE_BLEND, -1, 0.f },
  { "stageColor", STAGE_COLOR, offsetof(CmStage, color.a), 1.f },
  { "stagePulse", STAGE_PULSE, offsetof(CmStage, pulse.hz), 1.f },
  { "stageScrollS", STAGE_SCROLL_S, offsetof(CmStage, scroll.s), .25f },
  { "stageScrollT", STAGE_SCROLL_T, offsetof(CmStage, scroll.t), .25f },
  { "stageScaleS", STAGE_SCALE_S, offsetof(CmStage, scale.s), 1.f },
  { "stageScaleT", STAGE_SCALE_T, offsetof(CmStage, scale.t), 1.f },
  { "stageRotate", STAGE_ROTATE, offsetof(CmStage, rotate.hz), .25f },
  { "stageStretch", STAGE_STRETCH, offsetof(CmStage, stretch.hz), 1.f },
  { "stageWarp", STAGE_WARP, offsetof(CmStage, warp.hz), 1.f },
  { "stageEmissive", STAGE_EMISSIVE, offsetof(CmStage, emissive), 1.f },
  { "stageLighting", STAGE_LIGHTING, offsetof(CmStage, lighting.intensity), 1.f },
  { "stageDirtmap", STAGE_DIRTMAP, offsetof(CmStage, dirtmap.intensity), 1.f },
  { "stageLight", STAGE_LIGHT, offsetof(CmStage, light.intensity), STAGE_LIGHT_INTENSITY },
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
  { "stageScrollSValue", offsetof(CmStage, scroll.s), false },
  { "stageScrollTValue", offsetof(CmStage, scroll.t), false },
  { "stageScaleSValue", offsetof(CmStage, scale.s), false },
  { "stageScaleTValue", offsetof(CmStage, scale.t), false },
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
    if (control) {
      control->state = stage && (stage->flags & stageFlags[i].flag) ? ControlStateSelected : ControlStateDefault;
    }
  }

  for (size_t i = 0; i < lengthof(stageParams); i++) {
    Slider *slider = (Slider *) $(view, descendantWithIdentifier, stageParams[i].identifier);
    if (slider) {
      $(slider, setValue, stage ? (double) *stageFloat(stage, stageParams[i].offset) : 0.0);
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

    if (flag->offset >= 0 && *stageFloat(this->stage, flag->offset) == 0.f) {
      *stageFloat(this->stage, flag->offset) = flag->value;
    }

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

    checkbox->delegate.self = self;
    checkbox->delegate.data = (ident) &stageFlags[i];
    checkbox->delegate.didToggle = didToggleStageFlag;
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
