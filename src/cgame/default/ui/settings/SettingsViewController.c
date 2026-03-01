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

#include "SettingsViewController.h"

#define _Class _SettingsViewController

#pragma mark - Delegates

/**
 * @brief ButtonDelegate for the Apply button.
 */
static void didClickApply(Button *button) {
  cgi.Cbuf("r_restart; s_restart");
}

/**
 * @brief SelectDelegate callback for quality presets.
 */
static void didSelectQuality(Select *select, Option *option) {

  switch ((intptr_t) option->value) {
    case 0: // Low
      cgi.SetCvarInteger("r_shadows", 0);
      cgi.SetCvarInteger("r_fog_samples", 0);
      cgi.SetCvarInteger("r_lighting_distance", 1024);
      cgi.SetCvarInteger("r_parallax", 0);
      cgi.SetCvarInteger("r_parallax_shadow", 0);
      cgi.SetCvarInteger("r_caustics", 0);
      cgi.SetCvarInteger("cg_add_weather", 0);
      cgi.SetCvarInteger("cg_add_atmospheric", 0);
      cgi.SetCvarInteger("r_sprites_soften", 0);
      cgi.SetCvarInteger("r_sprites_lerp", 0);
      break;
    case 1: // Medium
      cgi.SetCvarInteger("r_shadows", 1);
      cgi.SetCvarInteger("r_shadow_cubemap_array_size", 256);
      cgi.SetCvarInteger("r_shadow_distance", 512);
      cgi.SetCvarInteger("r_fog_samples", 8);
      cgi.SetCvarInteger("r_lighting_distance", 2048);
      cgi.SetCvarInteger("r_parallax", 0);
      cgi.SetCvarInteger("r_parallax_shadow", 0);
      cgi.SetCvarInteger("r_caustics", 0);
      cgi.SetCvarInteger("cg_add_weather", 1);
      cgi.SetCvarInteger("cg_add_atmospheric", 1);
      cgi.SetCvarInteger("r_sprites_soften", 1);
      cgi.SetCvarInteger("r_sprites_lerp", 0);
      break;
    case 2: // High
      cgi.SetCvarInteger("r_shadows", 1);
      cgi.SetCvarInteger("r_shadow_cubemap_array_size", 512);
      cgi.SetCvarInteger("r_shadow_distance", 1024);
      cgi.SetCvarInteger("r_fog_samples", 16);
      cgi.SetCvarInteger("r_lighting_distance", 4096);
      cgi.SetCvarInteger("r_parallax", 1);
      cgi.SetCvarInteger("r_parallax_shadow", 0);
      cgi.SetCvarInteger("r_caustics", 1);
      cgi.SetCvarInteger("cg_add_weather", 1);
      cgi.SetCvarInteger("cg_add_atmospheric", 1);
      cgi.SetCvarInteger("r_sprites_soften", 1);
      cgi.SetCvarInteger("r_sprites_lerp", 1);
      break;
    case 3: // Highest
      cgi.SetCvarInteger("r_shadows", 1);
      cgi.SetCvarInteger("r_shadow_cubemap_array_size", 1024);
      cgi.SetCvarInteger("r_shadow_distance", 2048);
      cgi.SetCvarInteger("r_fog_samples", 32);
      cgi.SetCvarInteger("r_lighting_distance", 8192);
      cgi.SetCvarInteger("r_parallax", 1);
      cgi.SetCvarInteger("r_parallax_shadow", 1);
      cgi.SetCvarInteger("r_caustics", 1);
      cgi.SetCvarInteger("cg_add_weather", 1);
      cgi.SetCvarInteger("cg_add_atmospheric", 1);
      cgi.SetCvarInteger("r_sprites_soften", 1);
      cgi.SetCvarInteger("r_sprites_lerp", 1);
      break;
    default:
      break;
  }

  ViewController *this = select->delegate.self;
  if (this) {
    $(this->view, updateBindings);
  }
}

#pragma mark - ViewController

/**
 * @see ViewController::loadView(ViewController *)
 */
static void loadView(ViewController *self) {

  super(ViewController, self, loadView);

  View *view = $$(View, viewWithResourceName, "ui/settings/SettingsViewController.json", NULL);
  assert(view);

  $(self, setView, view);
  release(view);

  Select *windowMode, *verticalSync, *anisotropy, *quality;
  Button *apply;

  Outlet outlets[] = MakeOutlets(
    MakeOutlet("windowMode", &windowMode),
    MakeOutlet("verticalSync", &verticalSync),
    MakeOutlet("anisotropy", &anisotropy),
    MakeOutlet("quality", &quality),
    MakeOutlet("apply", &apply)
  );

  $(self->view, resolve, outlets);

  $(windowMode, addOption, "Window", (ident) 0);
  $(windowMode, addOption, "Fullscreen", (ident) 1);
  $(windowMode, addOption, "Exclusive Fullscreen", (ident) 2);

  $(verticalSync, addOption, "Disabled", (ident) 0);
  $(verticalSync, addOption, "Enabled", (ident) 1);
  $(verticalSync, addOption, "Adaptive", (ident) -1);

  $(anisotropy, addOption, "Disabled", (ident) 0);
  $(anisotropy, addOption, "2x", (ident) 2);
  $(anisotropy, addOption, "4x", (ident) 4);
  $(anisotropy, addOption, "8x", (ident) 8);
  $(anisotropy, addOption, "16x", (ident) 16);

  $(quality, addOption, "Custom", (ident) -1);
  $(quality, addOption, "Low", (ident) 0);
  $(quality, addOption, "Medium", (ident) 1);
  $(quality, addOption, "High", (ident) 2);
  $(quality, addOption, "Highest", (ident) 3);

  quality->delegate.self = self;
  quality->delegate.didSelectOption = didSelectQuality;

  apply->delegate.didClick = didClickApply;
  apply->delegate.self = self;
}

#pragma mark - Class lifecycle

/**
 * @see Class::initialize(Class *)
 */
static void initialize(Class *clazz) {
  ((ViewControllerInterface *) clazz->interface)->loadView = loadView;
}

/**
 * @fn Class *SettingsViewController::_SettingsViewController(void)
 * @memberof SettingsViewController
 */
Class *_SettingsViewController(void) {
  static Class *clazz;
  static Once once;

  do_once(&once, {
    clazz = _initialize(&(const ClassDef) {
      .name = "SettingsViewController",
      .superclass = _ViewController(),
      .instanceSize = sizeof(SettingsViewController),
      .interfaceOffset = offsetof(SettingsViewController, interface),
      .interfaceSize = sizeof(SettingsViewControllerInterface),
      .initialize = initialize,
    });
  });

  return clazz;
}

#undef _Class
