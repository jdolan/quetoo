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

#include "MeshViewController.h"

#define _Class _MeshViewController

#pragma mark - Internal helpers

/**
 * @brief Like vtos, but without parens.
 */
static char *vs(const vec3_t v) {
  static char buf[MAX_TOKEN_CHARS];

  g_snprintf(buf, sizeof(buf), "%g %g %g", v.x, v.y, v.z);
  return buf;
}

/**
 * @brief Rebuilds the transform matrix from the raw fields of a config.
 */
static void RebuildMeshConfigTransform(r_mesh_config_t *cfg) {

  cfg->transform = Mat4_Identity();
  cfg->transform = Mat4_ConcatTranslation(cfg->transform, cfg->translate);
  cfg->transform = Mat4_ConcatRotation3(cfg->transform, cfg->rotate);
  cfg->transform = Mat4_ConcatScale(cfg->transform, cfg->scale);
}

#pragma mark - Delegates

/**
 * @brief TextViewDelegate callback — parse text and reapply transform live.
 */
static void didEndEditing(TextView *textView) {

  MeshViewController *this = textView->delegate.self;

  if (!this->model) {
    return;
  }

  const char *text = textView->attributedText->chars ?: "";

  r_mesh_config_t *world = &this->model->mesh->config.world;
  r_mesh_config_t *link  = &this->model->mesh->config.link;
  r_mesh_config_t *view  = &this->model->mesh->config.view;

  if (textView == this->worldTranslate) {
    sscanf(text, "%f %f %f", &world->translate.x, &world->translate.y, &world->translate.z);
    RebuildMeshConfigTransform(world);
  } else if (textView == this->worldRotate) {
    sscanf(text, "%f %f %f", &world->rotate.x, &world->rotate.y, &world->rotate.z);
    RebuildMeshConfigTransform(world);
  } else if (textView == this->worldScale) {
    sscanf(text, "%f", &world->scale);
    RebuildMeshConfigTransform(world);
  } else if (textView == this->linkTranslate) {
    sscanf(text, "%f %f %f", &link->translate.x, &link->translate.y, &link->translate.z);
    RebuildMeshConfigTransform(link);
  } else if (textView == this->linkRotate) {
    sscanf(text, "%f %f %f", &link->rotate.x, &link->rotate.y, &link->rotate.z);
    RebuildMeshConfigTransform(link);
  } else if (textView == this->linkScale) {
    sscanf(text, "%f", &link->scale);
    RebuildMeshConfigTransform(link);
  } else if (textView == this->viewTranslate) {
    sscanf(text, "%f %f %f", &view->translate.x, &view->translate.y, &view->translate.z);
    RebuildMeshConfigTransform(view);
  } else if (textView == this->viewRotate) {
    sscanf(text, "%f %f %f", &view->rotate.x, &view->rotate.y, &view->rotate.z);
    RebuildMeshConfigTransform(view);
  } else if (textView == this->viewScale) {
    sscanf(text, "%f", &view->scale);
    RebuildMeshConfigTransform(view);
  } else if (textView == this->viewMuzzle) {
    sscanf(text, "%f %f %f", &view->muzzle.x, &view->muzzle.y, &view->muzzle.z);
  } else {
    Cg_Debug("Unknown text view %p\n", (void *) textView);
  }
}

#pragma mark - ViewController

/**
 * @see ViewController::loadView(ViewController *)
 */
static void loadView(ViewController *self) {

  super(ViewController, self, loadView);

  MeshViewController *this = (MeshViewController *) self;

  Outlet outlets[] = MakeOutlets(
    MakeOutlet("worldTranslate", &this->worldTranslate),
    MakeOutlet("worldRotate",    &this->worldRotate),
    MakeOutlet("worldScale",     &this->worldScale),
    MakeOutlet("linkTranslate",  &this->linkTranslate),
    MakeOutlet("linkRotate",     &this->linkRotate),
    MakeOutlet("linkScale",      &this->linkScale),
    MakeOutlet("viewTranslate",  &this->viewTranslate),
    MakeOutlet("viewRotate",     &this->viewRotate),
    MakeOutlet("viewScale",      &this->viewScale),
    MakeOutlet("viewMuzzle",     &this->viewMuzzle)
  );

  $(self->view, awakeWithResourceName, "ui/editor/MeshViewController.json");
  $(self->view, resolve, outlets);

  self->view->stylesheet = $$(Stylesheet, stylesheetWithResourceName, "ui/editor/MeshViewController.css");

  TextView *textViews[] = {
    this->worldTranslate, this->worldRotate, this->worldScale,
    this->linkTranslate,  this->linkRotate,  this->linkScale,
    this->viewTranslate,  this->viewRotate,  this->viewScale, this->viewMuzzle,
  };

  for (size_t i = 0; i < lengthof(textViews); i++) {
    textViews[i]->delegate.self = self;
    textViews[i]->delegate.didEndEditing = didEndEditing;
  }
}

/**
 * @see ViewController::viewWillAppear(ViewController *)
 */
static void viewWillAppear(ViewController *self) {

  MeshViewController *this = (MeshViewController *) self;

  r_model_t *model = NULL;

  if (cg_editor.selected > 0) {
    const cg_editor_entity_t *edit = &cg_editor.entities[cg_editor.selected];
    if (edit->model && IS_MESH_MODEL(edit->model)) {
      model = (r_model_t *) edit->model;
    }
  }

  $(this, setModel, model);

  super(ViewController, self, viewWillAppear);
}

#pragma mark - MeshViewController

/**
 * @fn MeshViewController *MeshViewController::init(MeshViewController *self)
 * @memberof MeshViewController
 */
static MeshViewController *init(MeshViewController *self) {
  return (MeshViewController *) super(ViewController, self, init);
}

/**
 * @fn void MeshViewController::setModel(MeshViewController *self, r_model_t *model)
 * @memberof MeshViewController
 */
static void setModel(MeshViewController *self, r_model_t *model) {

  self->model = model;

  const bool is_weapon = self->model && g_str_has_prefix(self->model->media.name, "models/weapons/");

  const r_mesh_config_t *world = self->model ? &self->model->mesh->config.world : &(r_mesh_config_t) { .scale = 1.f };
  const r_mesh_config_t *link  = self->model ? &self->model->mesh->config.link  : &(r_mesh_config_t) { .scale = 1.f };
  const r_mesh_config_t *view  = self->model ? &self->model->mesh->config.view  : &(r_mesh_config_t) { .scale = 1.f };

  $(self->worldTranslate, setAttributedText, vs(world->translate));
  $(self->worldRotate,    setAttributedText, vs(world->rotate));
  $(self->worldScale,     setAttributedText, va("%g", world->scale));

  $(self->linkTranslate,  setAttributedText, vs(link->translate));
  $(self->linkRotate,     setAttributedText, vs(link->rotate));
  $(self->linkScale,      setAttributedText, va("%g", link->scale));

  $(self->viewTranslate,  setAttributedText, vs(view->translate));
  $(self->viewRotate,     setAttributedText, vs(view->rotate));
  $(self->viewScale,      setAttributedText, va("%g", view->scale));
  $(self->viewMuzzle,     setAttributedText, vs(view->muzzle));

  TextView *link_views[] = { self->linkTranslate, self->linkRotate, self->linkScale };
  TextView *view_views[] = { self->viewTranslate, self->viewRotate, self->viewScale, self->viewMuzzle };

  for (size_t i = 0; i < lengthof(link_views); i++) {
    link_views[i]->control.state &= ~ControlStateDisabled;
    if (!is_weapon) {
      link_views[i]->control.state |= ControlStateDisabled;
    }
  }

  for (size_t i = 0; i < lengthof(view_views); i++) {
    view_views[i]->control.state &= ~ControlStateDisabled;
    if (!is_weapon) {
      view_views[i]->control.state |= ControlStateDisabled;
    }
  }
}

/**
 * @fn void MeshViewController::save(MeshViewController *self)
 * @memberof MeshViewController
 */
static void save(MeshViewController *self) {

  if (!self->model) {
    return;
  }

  cgi.Cbuf(va("r_save_mesh_configs %s", self->model->media.name));
}

#pragma mark - Class lifecycle

/**
 * @see Class::initialize(Class *)
 */
static void initialize(Class *clazz) {

  ((ViewControllerInterface *) clazz->interface)->loadView = loadView;
  ((ViewControllerInterface *) clazz->interface)->viewWillAppear = viewWillAppear;

  ((MeshViewControllerInterface *) clazz->interface)->init = init;
  ((MeshViewControllerInterface *) clazz->interface)->setModel = setModel;
  ((MeshViewControllerInterface *) clazz->interface)->save = save;
}

/**
 * @fn Class *MeshViewController::_MeshViewController(void)
 * @memberof MeshViewController
 */
Class *_MeshViewController(void) {
  static Class *clazz;
  static Once once;

  do_once(&once, {
    clazz = _initialize(&(const ClassDef) {
      .name = "MeshViewController",
      .superclass = _ViewController(),
      .instanceSize = sizeof(MeshViewController),
      .interfaceOffset = offsetof(MeshViewController, interface),
      .interfaceSize = sizeof(MeshViewControllerInterface),
      .initialize = initialize,
    });
  });

  return clazz;
}

#undef _Class
