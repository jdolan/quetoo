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

#include "ui_local.h"
#include "client.h"

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
  } else if (slider == this->alphaTest) {
    this->material->cm->alpha_test = slider->value;
  } else {
    Com_Debug(DEBUG_UI, "Unknown Slider %p\n", (void *) slider);
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
    MakeOutlet("name", &this->name),
    MakeOutlet("diffusemap", &this->diffusemap),
    MakeOutlet("normalmap", &this->normalmap),
    MakeOutlet("specularmap", &this->specularmap),
    MakeOutlet("roughness", &this->roughness),
    MakeOutlet("hardness", &this->hardness),
    MakeOutlet("specularity", &this->specularity),
    MakeOutlet("parallax", &this->parallax),
    MakeOutlet("alpha_test", &this->alphaTest)
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

  this->alphaTest->delegate.self = self;
  this->alphaTest->delegate.didSetValue = didSetValue;
}

/**
 * @see ViewController::viewWillAppear(ViewController *)
 */
static void viewWillAppear(ViewController *self) {

  MaterialViewController *this = (MaterialViewController *) self;

  r_material_t *material = NULL;
  const r_model_t *model = NULL;

  float distance = MAX_WORLD_DIST;

  vec3_t start = Vec3_Fmaf(cl_view.origin, 16.f, cl_view.forward);
  vec3_t end = Vec3_Fmaf(start, MAX_WORLD_DIST, cl_view.forward);

  while (material == NULL) {

    const cm_trace_t tr = Cl_Trace(start, end, Box3_Zero(), 0, CONTENTS_MASK_VISIBLE);
    if (!tr.material) {
      break;
    }

    material = R_LoadMaterial(tr.material->name, ASSET_CONTEXT_TEXTURES);
    model = R_WorldModel();

    distance = Vec3_Distance(cl_view.origin, tr.end);
  }

  const r_entity_t *e = cl_view.entities;
  for (int32_t i = 0; i < cl_view.num_entities; i++, e++) {

    if (e->model == NULL) {
      continue;
    }

    if (e->model->type != MODEL_MESH) {
      continue;
    }

    if (e->model->mesh->faces->material == NULL) {
      continue;
    }

    if (e->effects & (EF_SELF | EF_WEAPON)) {
      continue;
    }

    const int32_t head_node = Cm_SetBoxHull(e->abs_model_bounds, CONTENTS_SOLID);

    const cm_trace_t tr = Cm_BoxTrace(cl_view.origin, end, Box3_Zero(), head_node, CONTENTS_SOLID);
    if (tr.fraction < 1.f) {

      const float dist = Vec3_Distance(cl_view.origin, tr.end);
      if (dist < distance) {
        material = e->model->mesh->faces->material;
        model = e->model;

        distance = dist;
      }
    }
  }

  $(this, setModelAndMaterial, model, material);

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
 * @fn void MaterialViewController::setModelAndMaterial(MaterialViewController *self, r_material_t *material)
 * @memberof MaterialViewController
 */
static void setModelAndMaterial(MaterialViewController *self, const r_model_t *model, r_material_t *material) {

  self->model = model;
  self->material = material;

  if (self->model && self->material) {
    $(self->name, setDefaultText, self->material->cm->basename);
    $(self->diffusemap, setDefaultText, self->material->cm->diffusemap.name);
    $(self->normalmap, setDefaultText, self->material->cm->normalmap.name);
    $(self->specularmap, setDefaultText, self->material->cm->specularmap.name);

    $(self->roughness, setValue, (double) self->material->cm->roughness);
    $(self->hardness, setValue, (double) self->material->cm->hardness);
    $(self->specularity, setValue, (double) self->material->cm->specularity);
    $(self->parallax, setValue, (double) self->material->cm->parallax);

    if (material->cm->surface & SURF_ALPHA_TEST) {
      $(self->alphaTest, setValue, (double) self->material->cm->alpha_test);
      self->alphaTest->control.state &= ~ControlStateDisabled;
    } else {
      $(self->alphaTest, setValue, MATERIAL_ALPHA_TEST);
      self->alphaTest->control.state |= ControlStateDisabled;
    }

  } else {
    $(self->name, setDefaultText, NULL);
    $(self->diffusemap, setDefaultText, NULL);
    $(self->normalmap, setDefaultText, NULL);
    $(self->specularmap, setDefaultText, NULL);

    $(self->roughness, setValue, MATERIAL_ROUGHNESS);
    $(self->parallax, setValue, MATERIAL_PARALLAX);
    $(self->hardness, setValue, MATERIAL_HARDNESS);
    $(self->specularity, setValue, MATERIAL_SPECULARITY);
    $(self->alphaTest, setValue, MATERIAL_ALPHA_TEST);
  }
}

#pragma mark - Class lifecycle

/**
 * @see Class::initialize(Class *)
 */
static void initialize(Class *clazz) {

  ((ViewControllerInterface *) clazz->interface)->loadView = loadView;
  ((ViewControllerInterface *) clazz->interface)->viewWillAppear = viewWillAppear;

  ((MaterialViewControllerInterface *) clazz->interface)->init = init;
  ((MaterialViewControllerInterface *) clazz->interface)->setModelAndMaterial = setModelAndMaterial;
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
