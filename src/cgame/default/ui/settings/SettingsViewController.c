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

#pragma mark - Quality presets

typedef struct {
  int32_t shadows;
  int32_t shadowCubemapArraySize;
  int32_t shadowDistance;
  int32_t fogSamples;
  int32_t lightingDistance;
  int32_t parallax;
  int32_t parallaxShadow;
  int32_t caustics;
  int32_t addWeather;
  int32_t addAtmospheric;
  int32_t spritesSoften;
  int32_t spritesLerp;
} QualityPreset;

static const QualityPreset qualityPresets[] = {
  [0] = { // Low
    .shadows                = 0,
    .shadowCubemapArraySize = 0,
    .shadowDistance         = 0,
    .fogSamples             = 0,
    .lightingDistance       = 1024,
    .parallax               = 0,
    .parallaxShadow         = 0,
    .caustics               = 0,
    .addWeather             = 0,
    .addAtmospheric         = 0,
    .spritesSoften          = 0,
    .spritesLerp            = 0,
  },
  [1] = { // Medium
    .shadows                = 1,
    .shadowCubemapArraySize = 256,
    .shadowDistance         = 512,
    .fogSamples             = 8,
    .lightingDistance       = 2048,
    .parallax               = 0,
    .parallaxShadow         = 0,
    .caustics               = 0,
    .addWeather             = 1,
    .addAtmospheric         = 1,
    .spritesSoften          = 1,
    .spritesLerp            = 0,
  },
  [2] = { // High
    .shadows                = 1,
    .shadowCubemapArraySize = 512,
    .shadowDistance         = 1024,
    .fogSamples             = 16,
    .lightingDistance       = 4096,
    .parallax               = 1,
    .parallaxShadow         = 0,
    .caustics               = 1,
    .addWeather             = 1,
    .addAtmospheric         = 1,
    .spritesSoften          = 1,
    .spritesLerp            = 1,
  },
  [3] = { // Highest
    .shadows                = 1,
    .shadowCubemapArraySize = 1024,
    .shadowDistance         = 2048,
    .fogSamples             = 32,
    .lightingDistance       = 8192,
    .parallax               = 1,
    .parallaxShadow         = 1,
    .caustics               = 1,
    .addWeather             = 1,
    .addAtmospheric         = 1,
    .spritesSoften          = 1,
    .spritesLerp            = 1,
  },
};

/**
 * @brief
 */
static void applyQualityPreset(const QualityPreset *p) {
  cgi.SetCvarInteger("r_shadows",                   p->shadows);
  cgi.SetCvarInteger("r_shadow_cubemap_array_size", p->shadowCubemapArraySize);
  cgi.SetCvarInteger("r_shadow_distance",           p->shadowDistance);
  cgi.SetCvarInteger("r_fog_samples",               p->fogSamples);
  cgi.SetCvarInteger("r_lighting_distance",         p->lightingDistance);
  cgi.SetCvarInteger("r_parallax",                  p->parallax);
  cgi.SetCvarInteger("r_parallax_shadow",           p->parallaxShadow);
  cgi.SetCvarInteger("r_caustics",                  p->caustics);
  cgi.SetCvarInteger("cg_add_weather",              p->addWeather);
  cgi.SetCvarInteger("cg_add_atmospheric",          p->addAtmospheric);
  cgi.SetCvarInteger("r_sprites_soften",            p->spritesSoften);
  cgi.SetCvarInteger("r_sprites_lerp",              p->spritesLerp);
}

/**
 * @return The index of the matching preset, or -1 if no preset matches.
 */
static intptr_t detectQualityPreset(void) {
  const QualityPreset current = {
    .shadows                = cgi.GetCvarInteger("r_shadows"),
    .shadowCubemapArraySize = cgi.GetCvarInteger("r_shadow_cubemap_array_size"),
    .shadowDistance         = cgi.GetCvarInteger("r_shadow_distance"),
    .fogSamples             = cgi.GetCvarInteger("r_fog_samples"),
    .lightingDistance       = cgi.GetCvarInteger("r_lighting_distance"),
    .parallax               = cgi.GetCvarInteger("r_parallax"),
    .parallaxShadow         = cgi.GetCvarInteger("r_parallax_shadow"),
    .caustics               = cgi.GetCvarInteger("r_caustics"),
    .addWeather             = cgi.GetCvarInteger("cg_add_weather"),
    .addAtmospheric         = cgi.GetCvarInteger("cg_add_atmospheric"),
    .spritesSoften          = cgi.GetCvarInteger("r_sprites_soften"),
    .spritesLerp            = cgi.GetCvarInteger("r_sprites_lerp"),
  };

  for (size_t i = 0; i < lengthof(qualityPresets); i++) {
    if (memcmp(&current, &qualityPresets[i], sizeof(QualityPreset)) == 0) {
      return (intptr_t) i;
    }
  }

  return -1;
}

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

  const intptr_t index = (intptr_t) option->value;
  if (index >= 0 && index < (intptr_t) lengthof(qualityPresets)) {
    applyQualityPreset(&qualityPresets[index]);
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

  $(quality, selectOptionWithValue, (ident) detectQualityPreset());

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
