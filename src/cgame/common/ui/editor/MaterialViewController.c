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
 * @brief A contents or surface flag that a Checkbox toggles.
 */
typedef struct {
  const char *identifier;
  int32_t flag;
} MaterialFlag;

/**
 * @brief The contents flags.
 */
static const MaterialFlag contentsFlags[] = {
  { "contentsSolid", CONTENTS_SOLID },
  { "contentsWindow", CONTENTS_WINDOW },
  { "contentsDecoration", CONTENTS_DECORATION },
  { "contentsLava", CONTENTS_LAVA },
  { "contentsSlime", CONTENTS_SLIME },
  { "contentsWater", CONTENTS_WATER },
  { "contentsMist", CONTENTS_MIST },
  { "contentsDetail", CONTENTS_DETAIL },
  { "contentsLadder", CONTENTS_LADDER },
};

/**
 * @brief The surface flags.
 */
static const MaterialFlag surfaceFlags[] = {
  { "surfaceSlick", SURF_SLICK },
  { "surfaceSky", SURF_SKY },
  { "surfaceLiquid", SURF_LIQUID },
  { "surfaceBlend33", SURF_BLEND_33 },
  { "surfaceBlend66", SURF_BLEND_66 },
  { "surfaceBlend100", SURF_BLEND_100 },
  { "surfaceNoDraw", SURF_NO_DRAW },
  { "surfaceHint", SURF_HINT },
  { "surfaceSkip", SURF_SKIP },
  { "surfaceAlphaTest", SURF_ALPHA_TEST },
  { "surfacePhong", SURF_PHONG },
  { "surfaceMaterial", SURF_MATERIAL },
  { "surfacePortal", SURF_PORTAL },
  { "surfaceReflect", SURF_REFLECT },
};

/**
 * @brief CheckboxDelegate callback for the contents flags.
 */
static void didToggleContentsFlag(Checkbox *checkbox) {

  MaterialViewController *this = (MaterialViewController *) checkbox->delegate.self;
  const MaterialFlag *flag = checkbox->delegate.data;

  if (!this->material) {
    return;
  }

  if ($((Control *) checkbox, isSelected)) {
    this->material->cm->contents |= flag->flag;
  } else {
    this->material->cm->contents &= ~flag->flag;
  }

  this->material->cm->dirty = true;
}

/**
 * @brief CheckboxDelegate callback for the surface flags.
 */
static void didToggleSurfaceFlag(Checkbox *checkbox) {

  MaterialViewController *this = (MaterialViewController *) checkbox->delegate.self;
  const MaterialFlag *flag = checkbox->delegate.data;

  if (!this->material) {
    return;
  }

  if ($((Control *) checkbox, isSelected)) {
    this->material->cm->surface |= flag->flag;
  } else {
    this->material->cm->surface &= ~flag->flag;
  }

  this->material->cm->dirty = true;
}

/**
 * @brief Checks each flag Checkbox that the mask sets, and disables them all without a material.
 */
static void setFlags(MaterialViewController *this, const MaterialFlag *flags, size_t count, int32_t mask) {

  for (size_t i = 0; i < count; i++) {
    Control *control = (Control *) $(this->viewController.view, descendantWithIdentifier, flags[i].identifier);

    if (mask & flags[i].flag) {
      control->state |= ControlStateSelected;
    } else {
      control->state &= ~ControlStateSelected;
    }

    if (this->material) {
      control->state &= ~ControlStateDisabled;
    } else {
      control->state |= ControlStateDisabled;
    }

    $(control, stateDidChange);
  }
}

#pragma mark - ViewController

/**
 * @see ViewController::loadView(ViewController *)
 */
static void loadView(ViewController *self) {

  super(ViewController, self, loadView);

  MaterialViewController *this = (MaterialViewController *) self;

  Outlet outlets[] = MakeOutlets(
    MakeOutlet("materialBox", &this->materialBox),
    MakeOutlet("diffusemap", &this->diffusemap),
    MakeOutlet("normalmap", &this->normalmap),
    MakeOutlet("specularmap", &this->specularmap),
    MakeOutlet("roughness", &this->roughness),
    MakeOutlet("hardness", &this->hardness),
    MakeOutlet("specularity", &this->specularity),
    MakeOutlet("parallax", &this->parallax),
    MakeOutlet("shadow", &this->shadow),
    MakeOutlet("alphaTest", &this->alphaTest)
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

  for (size_t i = 0; i < lengthof(contentsFlags); i++) {
    Checkbox *checkbox = (Checkbox *) $(self->view, descendantWithIdentifier, contentsFlags[i].identifier);
    assert(checkbox);

    checkbox->delegate.self = self;
    checkbox->delegate.data = (ident) &contentsFlags[i];
    checkbox->delegate.didToggle = didToggleContentsFlag;
  }

  for (size_t i = 0; i < lengthof(surfaceFlags); i++) {
    Checkbox *checkbox = (Checkbox *) $(self->view, descendantWithIdentifier, surfaceFlags[i].identifier);
    assert(checkbox);

    checkbox->delegate.self = self;
    checkbox->delegate.data = (ident) &surfaceFlags[i];
    checkbox->delegate.didToggle = didToggleSurfaceFlag;
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
    $(self->materialBox->label->text, setText, va("Material [%s]", self->material->cm->basename));
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

  setFlags(self, contentsFlags, lengthof(contentsFlags), self->material ? self->material->cm->contents : 0);
  setFlags(self, surfaceFlags, lengthof(surfaceFlags), self->material ? self->material->cm->surface : 0);
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
